#!/usr/bin/env bash
# upgrade-core-pi4.sh <new-checkpoint> <installed-checkpoint>
#
# Runs ON the Pi as root. Replaces an installed nereusd with a stage built by
# build-pi4-in-docker.sh and copied to /home/nereus/nereus-stage/<new>/.
# Mirrors the Rock's install-core-selector.sh: the installed binary, library,
# RADE library, unit, /etc/nereusd.conf and the daemon's settings directory are
# copied to a root-only rollback directory first, and any failure after the
# service stops puts them back and starts the previous Core again.
# /etc/nereusd.conf is kept as it is (upgrades never rewrite operator config).
# The DFNR model/licences and both RNNoise models are installed with Core.
# Existing asset directories are saved and restored on rollback; a first
# installation of those directories is removed on rollback.
set -euo pipefail

new=${1:?new checkpoint required}
previous=${2:?installed checkpoint required}
[[ "$new" =~ ^[0-9a-f]{8}$ && "$previous" =~ ^[0-9a-f]{8}$ ]] || { echo 'bad checkpoint'; exit 2; }
src=/home/nereus/nereus-stage/$new
keep=/var/lib/nereus-build/pi4/$new
backup=/var/lib/nereus-build/pi4/rollback-$previous-before-$new
tar=stage-$new-pi4.tar
state=/var/lib/private/nereusd/.config

echo '==== preflight'
[[ $(id -u) == 0 ]] || { echo 'run as root'; exit 1; }
for tool in flock python3; do
    command -v "$tool" >/dev/null || { echo "missing required dependency: $tool"; exit 1; }
done
exec 9>/run/lock/nereus-core-deploy.lock
flock -x 9
retention=${NEREUS_RETENTION_HELPER:-/usr/local/libexec/nereusd/prune-core-artifacts.py}
if [[ ! -f "$retention" && -f "$(dirname "$0")/prune-core-artifacts.py" ]]; then
    retention=$(dirname "$0")/prune-core-artifacts.py
fi
[[ -e /usr/local/bin/nereusd ]] || { echo 'nothing installed; use install-core-pi4.sh'; exit 1; }
[[ ! -e "$keep" && ! -e "$backup" ]] || { echo "$keep or $backup already exists"; exit 1; }
for f in "$tar" "$tar.sha256" "stage-$new-pi4.sha256" runtime-packages.txt; do
    [[ -s "$src/$f" ]] || { echo "missing $src/$f"; exit 1; }
done

echo '==== verify the stage'
install -d -m 700 "$keep"
touch "$keep/.created-by-deploy"
cp "$src/$tar" "$src/$tar.sha256" "$src/stage-$new-pi4.sha256" "$src/runtime-packages.txt" "$keep/"
(cd "$keep" && sha256sum -c "$tar.sha256")
unpack=$(mktemp -d "$keep/unpack.XXXXXX")
tar -xpf "$keep/$tar" -C "$unpack" --no-same-owner
(cd "$unpack" && sha256sum --strict --quiet -c "$keep/stage-$new-pi4.sha256")
test -s "$unpack/usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz"
for model in Default_large.bin Default_small.bin; do
    test -s "$unpack/usr/local/share/NereusSDR/models/rnnoise/$model"
done
for f in LICENSE LICENSE-APACHE LICENSE-MIT COMMIT; do
    test -s "$unpack/usr/local/share/doc/nereussdr/deepfilter/$f"
done
export DEBIAN_FRONTEND=noninteractive
missing=$(while read -r p; do
    dpkg-query -W -f='${Status}\n' "$p" 2>/dev/null | grep -q 'install ok installed' || echo "$p"
done < "$keep/runtime-packages.txt")
if [[ -n "$missing" ]]; then
    apt-get update -qq
    apt-get install -y --no-install-recommends -q $missing
fi
ldd_out=$(env LD_LIBRARY_PATH="$unpack/usr/local/lib" ldd "$unpack/usr/local/bin/nereusd" 2>&1)
if printf '%s\n' "$ldd_out" | grep -q 'not found'; then printf '%s\n' "$ldd_out"; exit 1; fi
env LD_LIBRARY_PATH="$unpack/usr/local/lib" "$unpack/usr/local/bin/nereusd" --help > /dev/null
echo 'stage verified; libraries resolve and it starts on this CPU'

echo '==== rollback copy'
mkdir -m 700 "$backup"
touch "$backup/.created-by-deploy"
tar -C / -cpf "$backup/files.tar" usr/local/bin/nereusd usr/local/lib/libNereusCore.so \
    usr/local/lib/librade.so usr/local/lib/librade.so.0.1 usr/lib/systemd/system/nereusd.service \
    etc/nereusd.conf
sync -f "$backup/files.tar"
# Model assets + licences: only what exists (an older Core may have none).
asset_paths=(usr/local/share/NereusSDR/models/dfnet3 usr/local/share/NereusSDR/models/rnnoise usr/local/share/doc/nereussdr/deepfilter)
asset_present=()
for p in "${asset_paths[@]}"; do
    if [[ -e "/$p" || -L "/$p" ]]; then asset_present+=("$p"); fi
done
printf '%s\n' "${asset_present[@]}" > "$backup/assets-present.txt"
if (( ${#asset_present[@]} )); then
    tar -C / -cpf "$backup/assets.tar" "${asset_present[@]}"
    sync -f "$backup/assets.tar"
fi

state_snapshot_ready=0
snapshot_daemon_state() {
    # Core saves settings while stopping. Publish only the complete copy
    # made after that stop, before any new binary can alter the state.
    cp -a "$state" "$backup/daemon-config.incomplete" || return 1
    mv "$backup/daemon-config.incomplete" "$backup/daemon-config" || return 1
    sync -f "$backup/daemon-config" || return 1
    state_snapshot_ready=1
}

restore_daemon_state() {
    [[ "$state_snapshot_ready" == 1 ]] || return 0
    local restore="$state.restore-$new"
    local failed="$state.failed-$new"
    [[ ! -e "$restore" && ! -L "$restore" && ! -e "$failed" && ! -L "$failed" ]] || return 1
    # Prepare the saved copy before moving the failed state. Both renames
    # use the same parent directory; no new state is deleted or overlaid.
    cp -a "$backup/daemon-config" "$restore" || return 1
    local moved=0
    if [[ -e "$state" || -L "$state" ]]; then
        mv "$state" "$failed" || return 1
        moved=1
    fi
    if ! mv "$restore" "$state"; then
        if [[ "$moved" == 1 ]]; then mv "$failed" "$state" || true; fi
        return 1
    fi
    if [[ "$moved" == 1 ]]; then
        if ! mv "$failed" "$backup/daemon-config-failed"; then
            echo "failed upgrade settings retained at $failed"
        fi
    fi
}

rollback() {
    local result=$?
    if [[ $result -ne 0 ]]; then
        set +e
        echo "upgrade failed ($result); restoring $previous"
        if ! systemctl stop nereusd; then
            echo "rollback stopped: could not stop Core; backup retained at $backup"
            exit "$result"
        fi
        local restored=1
        tar -C / -xpf "$backup/files.tar" || restored=0
        rm -rf /usr/local/share/NereusSDR/models/dfnet3 /usr/local/share/NereusSDR/models/rnnoise /usr/local/share/doc/nereussdr/deepfilter || restored=0
        if [[ -f "$backup/assets.tar" ]]; then tar -C / -xpf "$backup/assets.tar" || restored=0; fi
        restore_daemon_state || restored=0
        systemctl daemon-reload || restored=0
        if [[ "$restored" == 1 ]]; then
            systemctl start nereusd
            systemctl show nereusd -p ActiveState -p MainPID
        else
            echo "rollback incomplete; Core remains stopped; backup retained at $backup"
        fi
    fi
    rm -rf "$unpack"
    exit "$result"
}
trap rollback EXIT

echo '==== install'
stop_started=$(date '+%Y-%m-%d %H:%M:%S')
systemctl stop nereusd
stop_status=$(systemctl show nereusd -p ExecMainStatus --value)
if [[ "$stop_status" != 0 ]]; then
    # The previous Core crashed while stopping (seen 2026-09-23 on ac4c63aa).
    # It is stopped either way, so the upgrade continues; the crash is kept
    # as evidence rather than treated as a reason to keep the old Core.
    echo "WARNING: $previous exited with status $stop_status while stopping"
    coredumpctl list nereusd --since "$stop_started" --no-pager 2>/dev/null | tail -2 \
        | tee "$backup/stop-crash.txt" || true
fi
snapshot_daemon_state
systemctl reset-failed nereusd 2>/dev/null || true
install -m 755 "$unpack/usr/local/bin/nereusd" /usr/local/bin/nereusd
install -m 644 "$unpack/usr/local/lib/libNereusCore.so" /usr/local/lib/libNereusCore.so
install -m 644 "$unpack/usr/local/lib/librade.so.0.1" /usr/local/lib/librade.so.0.1
ln -sfn librade.so.0.1 /usr/local/lib/librade.so
install -m 644 "$unpack/usr/lib/systemd/system/nereusd.service" /usr/lib/systemd/system/nereusd.service
install -m 644 "$unpack/usr/local/share/nereusd/nereusd.conf.sample" /usr/local/share/nereusd/nereusd.conf.sample
# Includes the DFNR licences (doc/nereussdr/deepfilter/).
cp -a "$unpack/usr/local/share/doc/nereussdr/." /usr/local/share/doc/nereussdr/
install -D -m 644 "$unpack/usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz" /usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz
for model in Default_large.bin Default_small.bin; do
    install -D -m 644 "$unpack/usr/local/share/NereusSDR/models/rnnoise/$model" "/usr/local/share/NereusSDR/models/rnnoise/$model"
done
sync -f /usr/local/bin/nereusd
sync -f /usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz
for model in Default_large.bin Default_small.bin; do
    sync -f "/usr/local/share/NereusSDR/models/rnnoise/$model"
done
ldd /usr/local/bin/nereusd | grep -q 'not found' && { echo 'unresolved libraries'; exit 1; }
systemd-analyze verify /usr/lib/systemd/system/nereusd.service
systemctl daemon-reload
systemctl start nereusd
sleep 10
systemctl is-active --quiet nereusd
test "$(systemctl show nereusd -p NRestarts --value)" = 0
# BEGIN healthy retention hook
# Health has passed. Maintenance errors must never restore the previous Core.
trap - EXIT
if [[ -f "$unpack/usr/local/libexec/nereusd/prune-core-artifacts.py" ]]; then
    if awk '$2 ~ /^\*?(\.\/)?usr\/local\/libexec\/nereusd\/prune-core-artifacts\.py$/' "$keep/stage-$new-pi4.sha256" \
        | (cd "$unpack" && sha256sum --strict --quiet -c -) \
        && install -d /usr/local/libexec/nereusd \
        && install -m 755 "$unpack/usr/local/libexec/nereusd/prune-core-artifacts.py" /usr/local/libexec/nereusd/prune-core-artifacts.py; then
        if [[ -z "${NEREUS_RETENTION_HELPER:-}" ]]; then
            retention=/usr/local/libexec/nereusd/prune-core-artifacts.py
        fi
    else
        echo 'WARNING: could not verify/install the packaged retention helper; healthy Core retained' >&2
    fi
fi
if ! rm -rf "$unpack"; then
    echo 'WARNING: could not remove unpacked stage; healthy Core retained, retention skipped' >&2
else
    post_healthy_retention() {
        # Receipts are durable before planning. Core/settings are never removed
        # here; the generic helper only admits hex stages and rollback names.
        python3 - "$new" "$previous" "$backup" "$keep" "$state_snapshot_ready" <<'RECEIPT'
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import sys
import tempfile

checkpoint, previous, backup_name, keep_name, complete = sys.argv[1:]
keep = Path(keep_name)
backup = Path(backup_name) if backup_name else None

def digest(path):
    if not stat.S_ISREG(path.lstat().st_mode):
        raise ValueError("proof must be a regular nonsymlink file: " + str(path))
    value = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()

manifest = {}
for line in (keep / ('stage-' + checkpoint + '-pi4.sha256')).read_text().splitlines():
    match = re.fullmatch(r'([0-9a-f]{64}) [ *](.+)', line)
    if not match:
        raise ValueError('invalid verified stage manifest entry')
    name = match[2][2:] if match[2].startswith('./') else match[2]
    if name in manifest:
        raise ValueError('duplicate verified stage manifest entry')
    manifest[name] = match[1]
installed = {}
for relative in ('bin/nereusd', 'lib/libNereusCore.so', 'lib/librade.so.0.1'):
    expected = manifest.get('usr/local/' + relative)
    actual = digest(Path('/usr/local') / relative)
    if expected is None or actual != expected:
        raise ValueError('installed Core does not match verified stage: ' + relative)
    installed[relative] = actual
recovery = {}
if backup is not None:
    if complete != '1' or not stat.S_ISDIR((backup / 'daemon-config').lstat().st_mode):
        raise ValueError('complete post-stop daemon settings snapshot is required')
    recovery['files.tar'] = digest(backup / 'files.tar')
    if (backup / 'assets.tar').exists():
        recovery['assets.tar'] = digest(backup / 'assets.tar')
    elif (backup / 'assets-present.txt').read_text().strip():
        raise ValueError('saved asset recovery archive is missing')
receipt = dict(checkpoint=checkpoint, previous=previous, backup=backup.name if backup else None,
               installed_sha256=installed, recovery_sha256=recovery,
               post_stop_snapshot_complete=backup is not None)

def publish(path, value):
    descriptor, temporary = tempfile.mkstemp(prefix='.receipt-', dir=path.parent)
    try:
        with os.fdopen(descriptor, 'w') as output:
            if isinstance(value, dict):
                json.dump(value, output, sort_keys=True)
                output.write('\n')
            else:
                output.write(value)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
        directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)

publish(keep / 'installed.sha256', ''.join(value + '  /usr/local/' + relative + '\n'
                                        for relative, value in installed.items()))
publish(keep / 'successful-install.json', receipt)
publish(keep.parent / 'current-install.json', receipt)
RECEIPT
        local result=$?
        [[ "$result" == 0 ]] || return "$result"
        rm -f "$keep/.created-by-deploy" || return 1
        if [[ -n "$backup" ]]; then rm -f "$backup/.created-by-deploy" || return 1; fi
        local recovery_args=()
        if [[ -n "$backup" ]]; then recovery_args=(--backup "${backup##*/}"); fi
        python3 "$retention" --stage-root /var/lib/nereus-build/pi4 \
            --backup-root /var/lib/nereus-build/pi4 --current "$new" --previous "$previous" \
            ${recovery_args[@]+"${recovery_args[@]}"} --apply || return 1
        python3 "$retention" --stage-root /home/nereus/nereus-stage \
            --backup-root /var/lib/nereus-build/pi4 --current "$new" --previous "$previous" \
            ${recovery_args[@]+"${recovery_args[@]}"} --apply || return 1
    }
    if ! post_healthy_retention; then
        echo 'WARNING: success receipt or artifact cleanup failed; healthy Core and recovery retained' >&2
    fi
fi
# END healthy retention hook
systemctl show nereusd -p ActiveState -p MainPID -p NRestarts || true
echo "$new installed; rollback to $previous kept in $backup"
