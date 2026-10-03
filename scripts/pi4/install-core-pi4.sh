#!/usr/bin/env bash
# install-core-pi4.sh <checkpoint>
#
# Runs ON the Pi as root (sudo -n bash ~/nereus-stage/<cp>/install-core-pi4.sh <cp>).
# First install of nereusd from a stage built by build-pi4-in-docker.sh and
# copied to /home/nereus/nereus-stage/<cp>/ together with runtime-packages.txt
# and nereusd.conf. Refuses to run over an existing install.
#
# Keeps a root-only copy of what it installed under /var/lib/nereus-build/pi4/<cp>/
# (the verified stage tar, manifests, installed hashes, startup journal).
# Also installs the DFNR (DeepFilterNet3) model where nereusd looks for it
# (/usr/local/share/NereusSDR/models/dfnet3/) and its licences
# (/usr/local/share/doc/nereussdr/deepfilter/). The library is linked
# statically into libNereusCore.so; there is no separate file for it.
set -euo pipefail

cp=${1:?checkpoint required}
[[ "$cp" =~ ^[0-9a-f]{8}$ ]] || { echo "bad checkpoint: $cp"; exit 2; }
src=/home/nereus/nereus-stage/$cp
keep=/var/lib/nereus-build/pi4/$cp
tar=stage-$cp-pi4.tar

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
[[ "$(uname -m)" == aarch64 ]] || { echo 'not aarch64'; exit 1; }
grep -m1 -qw crc32 /proc/cpuinfo || { echo 'CPU lacks crc32'; exit 1; }
. /etc/os-release
[[ "${VERSION_CODENAME:-}" == trixie ]] || { echo "needs Debian 13 trixie, found ${VERSION_CODENAME:-?}"; exit 1; }
for f in /usr/local/bin/nereusd /usr/local/lib/libNereusCore.so /usr/lib/systemd/system/nereusd.service; do
    [[ ! -e "$f" ]] || { echo "already installed ($f); this script is first install only"; exit 1; }
done
[[ ! -e "$keep" ]] || { echo "$keep already exists"; exit 1; }
for f in "$tar" "$tar.sha256" "stage-$cp-pi4.sha256" runtime-packages.txt nereusd.conf; do
    [[ -s "$src/$f" ]] || { echo "missing $src/$f"; exit 1; }
done

echo '==== verify the stage (as root, from a root-only copy)'
install -d -m 700 /var/lib/nereus-build /var/lib/nereus-build/pi4 "$keep"
touch "$keep/.created-by-deploy"
cp "$src/$tar" "$src/$tar.sha256" "$src/stage-$cp-pi4.sha256" "$src/runtime-packages.txt" \
   "$src/nereusd.conf" "$keep/"
(cd "$keep" && sha256sum -c "$tar.sha256")
unpack=$(mktemp -d "$keep/unpack.XXXXXX")
tar -xpf "$keep/$tar" -C "$unpack" --no-same-owner
(cd "$unpack" && sha256sum --strict --quiet -c "$keep/stage-$cp-pi4.sha256")
echo "all $(wc -l < "$keep/stage-$cp-pi4.sha256") staged files verified"
test -s "$unpack/usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz"
for model in Default_large.bin Default_small.bin; do
    test -s "$unpack/usr/local/share/NereusSDR/models/rnnoise/$model"
done
for f in LICENSE LICENSE-APACHE LICENSE-MIT COMMIT; do
    test -s "$unpack/usr/local/share/doc/nereussdr/deepfilter/$f"
done
echo 'DFNR model and licences present in the stage'

echo '==== runtime packages'
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
xargs -a "$keep/runtime-packages.txt" apt-get install -y --no-install-recommends -q
dpkg-query -W -f='${Package} ${Version}\n' libc6 libqt6core6t64 libqt6gui6 libqt6network6 \
    libqt6multimedia6 libqt6websockets6 libfftw3-single3 libssl3t64 libstdc++6 \
    | tee "$keep/runtime-versions.txt"

echo '==== stage checks against this Pi'"'"'s libraries'
ldd_out=$(env LD_LIBRARY_PATH="$unpack/usr/local/lib" ldd "$unpack/usr/local/bin/nereusd" 2>&1)
if printf '%s\n' "$ldd_out" | grep -q 'not found'; then printf '%s\n' "$ldd_out"; exit 1; fi
env LD_LIBRARY_PATH="$unpack/usr/local/lib" "$unpack/usr/local/bin/nereusd" --help > /dev/null
echo 'libraries resolve and nereusd starts on this CPU'

echo '==== install'
install -m 755 "$unpack/usr/local/bin/nereusd" /usr/local/bin/nereusd
install -d /usr/local/lib
install -m 644 "$unpack/usr/local/lib/libNereusCore.so" /usr/local/lib/libNereusCore.so
install -m 644 "$unpack/usr/local/lib/librade.so.0.1" /usr/local/lib/librade.so.0.1
ln -sfn librade.so.0.1 /usr/local/lib/librade.so
install -m 644 "$unpack/usr/lib/systemd/system/nereusd.service" /usr/lib/systemd/system/nereusd.service
install -d /usr/local/share/nereusd
install -m 644 "$unpack/usr/local/share/nereusd/nereusd.conf.sample" /usr/local/share/nereusd/nereusd.conf.sample
install -d /usr/local/share/doc/nereussdr
# Includes the DFNR licences (doc/nereussdr/deepfilter/).
cp -a "$unpack/usr/local/share/doc/nereussdr/." /usr/local/share/doc/nereussdr/
install -D -m 644 "$unpack/usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz" /usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz
for model in Default_large.bin Default_small.bin; do
    install -D -m 644 "$unpack/usr/local/share/NereusSDR/models/rnnoise/$model" "/usr/local/share/NereusSDR/models/rnnoise/$model"
done
if [[ ! -e /etc/nereusd.conf ]]; then
    install -m 644 "$keep/nereusd.conf" /etc/nereusd.conf
else
    echo '/etc/nereusd.conf already exists; left unchanged'
fi
if ldd /usr/local/bin/nereusd | grep -q 'not found'; then echo 'installed nereusd has unresolved libraries'; exit 1; fi
/usr/local/bin/nereusd --help > "$keep/installed-help.txt"
systemd-analyze verify /usr/lib/systemd/system/nereusd.service
sha256sum /usr/local/bin/nereusd /usr/local/lib/libNereusCore.so /usr/local/lib/librade.so.0.1 \
    /usr/lib/systemd/system/nereusd.service /etc/nereusd.conf \
    /usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz | tee "$keep/installed.sha256"
sync

echo '==== start'
systemctl daemon-reload
systemctl enable nereusd
systemctl start nereusd
sleep 12
systemctl show nereusd -p ActiveState -p SubState -p MainPID -p NRestarts -p ExecMainStatus
journalctl -u nereusd -b --no-pager -o short-iso > "$keep/startup.journal"
chmod 600 "$keep/startup.journal"
systemctl is-active --quiet nereusd
test "$(systemctl show nereusd -p NRestarts --value)" = 0
new=$cp
previous=$cp
backup=
state_snapshot_ready=0
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
echo "$cp installed and running; startup journal kept in $keep/startup.journal (root only: it holds the pairing token)"
