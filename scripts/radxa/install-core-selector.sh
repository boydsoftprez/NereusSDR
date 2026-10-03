#!/usr/bin/env bash
set -euo pipefail
# Serialize installation and retention outside the directories they prune.
exec 9>/run/lock/nereus-core-deploy.lock
flock -x 9
retention=/var/lib/nereus-build/prune-core-artifacts.py
checkpoint=${1:?signed checkpoint is required}
if [[ ! "$checkpoint" =~ ^[0-9a-f]{8,40}$ ]]; then exit 2; fi
stage=/home/yonder/nereus-core/$checkpoint/stage
previous=${2:?verified previous checkpoint required}
if [[ ! "$previous" =~ ^[0-9a-f]{8,40}$ ]]; then exit 2; fi
backup=/var/lib/nereus-build/rollback-$previous-before-$checkpoint
state=/var/lib/private/nereusd/.config
test -x "$stage/usr/local/bin/nereusd"
test -s "$stage/usr/local/bin/nereusd"
test -s "$stage/usr/local/lib/libNereusCore.so"
test -s "$stage/usr/local/lib/librade.so.0.1"
test -f "$stage/usr/local/lib/libNereusCore.so"
test -f "$stage/usr/local/share/doc/nereussdr/LICENSE"
test -d "$stage/usr/local/share/doc/nereussdr/licenses"
test -s "$stage/usr/lib/systemd/system/nereusd.service"
test -s "$stage/usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz"
for model in Default_large.bin Default_small.bin; do
    test -s "$stage/usr/local/share/NereusSDR/models/rnnoise/$model"
done
for f in LICENSE LICENSE-APACHE LICENSE-MIT COMMIT; do
    test -s "$stage/usr/local/share/doc/nereussdr/deepfilter/$f"
done
systemd-analyze verify "$stage/usr/lib/systemd/system/nereusd.service"
# Every new daemon stage ships maintenance with its native bounded logger.
# Older stages can still use the already installed maintenance helper.
shipped_retention="$stage/usr/local/libexec/nereusd/prune-core-artifacts.py"
if test -f "$shipped_retention"; then
    install -D -m 700 "$shipped_retention" /usr/local/libexec/nereusd/prune-core-artifacts.py
    install -m 700 "$shipped_retention" "$retention.incomplete"
    mv "$retention.incomplete" "$retention"
fi
test -f "$retention"
test ! -e "$backup"
mkdir -m 700 "$backup"
tar -C / -cpf "$backup/files.tar" usr/local/bin/nereusd usr/local/lib/librade.so usr/local/lib/librade.so.0.1 usr/lib/systemd/system/nereusd.service etc/nereusd.conf
if test -e /usr/local/lib/libNereusCore.so; then
    cp -p /usr/local/lib/libNereusCore.so "$backup/libNereusCore.so"
fi
# Model assets + licences: older installations may not have them.
asset_present=()
for p in usr/local/share/NereusSDR/models/dfnet3 usr/local/share/NereusSDR/models/rnnoise usr/local/share/doc/nereussdr/deepfilter; do
    if test -e "/$p" || test -L "/$p"; then asset_present+=("$p"); fi
done
printf '%s\n' "${asset_present[@]}" > "$backup/assets-present.txt"
if test "${#asset_present[@]}" -gt 0; then
    tar -C / -cpf "$backup/assets.tar" "${asset_present[@]}"
fi
state_snapshot_ready=0
snapshot_daemon_state() {
    # The previous Core can save settings while stopping. Publish a complete
    # post-stop copy before any new binary can change them.
    cp -a "$state" "$backup/daemon-config.incomplete" || return 1
    mv "$backup/daemon-config.incomplete" "$backup/daemon-config" || return 1
    sync -f "$backup/daemon-config" || return 1
    state_snapshot_ready=1
}
restore_daemon_state() {
    [[ "$state_snapshot_ready" == 1 ]] || return 0
    local restore="$state.restore-$checkpoint"
    local failed="$state.failed-$checkpoint"
    [[ ! -e "$restore" && ! -L "$restore" && ! -e "$failed" && ! -L "$failed" ]] || return 1
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
    if test "$result" -ne 0; then
        set +e
        echo "installation failed ($result); restoring $previous"
        if ! systemctl stop nereusd; then
            echo "rollback stopped: could not stop Core; backup retained at $backup"
            exit "$result"
        fi
        local restored=1
        tar -C / -xpf "$backup/files.tar" || restored=0
        if test -e "$backup/libNereusCore.so"; then
            cp -p "$backup/libNereusCore.so" /usr/local/lib/libNereusCore.so || restored=0
        else
            rm -f /usr/local/lib/libNereusCore.so || restored=0
        fi
        rm -rf /usr/local/share/NereusSDR/models/dfnet3 /usr/local/share/NereusSDR/models/rnnoise /usr/local/share/doc/nereussdr/deepfilter || restored=0
        if test -f "$backup/assets.tar"; then tar -C / -xpf "$backup/assets.tar" || restored=0; fi
        restore_daemon_state || restored=0
        systemctl daemon-reload || restored=0
        if [[ "$restored" == 1 ]]; then
            systemctl start nereusd
        else
            echo "rollback incomplete; Core remains stopped; backup retained at $backup"
        fi
    fi
    exit "$result"
}
sync -f "$backup/files.tar"
if test -e "$backup/libNereusCore.so"; then sync -f "$backup/libNereusCore.so"; fi
if test -f "$backup/assets.tar"; then sync -f "$backup/assets.tar"; fi
trap rollback EXIT
systemctl stop nereusd
test "$(systemctl show nereusd -p ExecMainStatus --value)" = 0
snapshot_daemon_state
install -m 755 "$stage/usr/local/bin/nereusd" /usr/local/bin/nereusd
install -m 644 "$stage/usr/local/lib/libNereusCore.so" /usr/local/lib/libNereusCore.so
install -m 644 "$stage/usr/local/lib/librade.so.0.1" /usr/local/lib/librade.so.0.1
ln -sfn librade.so.0.1 /usr/local/lib/librade.so
install -m 644 "$stage/usr/lib/systemd/system/nereusd.service" /usr/lib/systemd/system/nereusd.service
install -d /usr/local/share/nereusd
install -m 644 "$stage/usr/local/share/nereusd/nereusd.conf.sample" /usr/local/share/nereusd/nereusd.conf.sample
if ldd /usr/local/bin/nereusd | rg -q 'not found'; then exit 1; fi
/usr/local/bin/nereusd --help > "$backup/installed-help.txt"
install -d /usr/local/share/doc/nereussdr
# Includes the DFNR licences (doc/nereussdr/deepfilter/).
cp -a "$stage/usr/local/share/doc/nereussdr/." /usr/local/share/doc/nereussdr/
install -D -m 644 "$stage/usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz" /usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz
for model in Default_large.bin Default_small.bin; do
    install -D -m 644 "$stage/usr/local/share/NereusSDR/models/rnnoise/$model" "/usr/local/share/NereusSDR/models/rnnoise/$model"
done
sync -f /usr/local/bin/nereusd
sync -f /usr/local/lib/libNereusCore.so
sync -f /usr/local/lib/librade.so.0.1
sync -f /usr/lib/systemd/system/nereusd.service
sync -f /usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz
for model in Default_large.bin Default_small.bin; do
    sync -f "/usr/local/share/NereusSDR/models/rnnoise/$model"
done
systemctl daemon-reload
systemctl start nereusd
sleep 8
systemctl is-active --quiet nereusd
journalctl -u nereusd -n 120 --no-pager > "$backup/startup.log"
systemctl is-active --quiet nereusd
printf '%s\n' "$checkpoint installed; service active. Live client validation follows separately."
systemctl show nereusd -p Result -p ExecMainStatus -p NRestarts
sha256sum /usr/local/bin/nereusd /usr/local/lib/libNereusCore.so /usr/local/lib/librade.so.0.1 \
    /usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz \
    /usr/local/share/NereusSDR/models/rnnoise/Default_large.bin \
    /usr/local/share/NereusSDR/models/rnnoise/Default_small.bin
trap - EXIT
# Never let maintenance failure trigger rollback of a healthy new Core.
# Publish success only after the complete post-stop recovery and startup checks.
if python3 - "$backup" "$checkpoint" "$previous" <<'PY'
import datetime, hashlib, json, os, pathlib, sys
backup = pathlib.Path(sys.argv[1])
proof = {
    "checkpoint": sys.argv[2], "previous": sys.argv[3],
    "backup": backup.name,
    "installed_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    "post_stop_config_complete": (backup / "daemon-config").is_dir(),
    "recovery_sha256": {}, "installed_sha256": {},
}
assert proof["post_stop_config_complete"]
for name in ("files.tar", "libNereusCore.so", "assets.tar"):
    path = backup / name
    assert path.is_file() and path.stat().st_size > 0
    with path.open("rb") as source:
        digest = hashlib.sha256()
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    proof["recovery_sha256"][name] = digest.hexdigest()
for name in ("bin/nereusd", "lib/libNereusCore.so", "lib/librade.so.0.1"):
    installed = pathlib.Path("/usr/local") / name
    stage = pathlib.Path("/home/yonder/nereus-core") / sys.argv[2] / "stage/usr/local" / name
    digest = hashlib.sha256(installed.read_bytes()).hexdigest()
    assert digest == hashlib.sha256(stage.read_bytes()).hexdigest(), name
    proof["installed_sha256"][name] = digest
path = backup / "successful-install.json"
tmp = path.with_suffix(".json.incomplete")
with tmp.open("w") as output:
    json.dump(proof, output, indent=2)
    output.write("\n")
    output.flush()
    os.fsync(output.fileno())
os.replace(tmp, path)
fd = os.open(backup, os.O_RDONLY | os.O_DIRECTORY)
try:
    os.fsync(fd)
finally:
    os.close(fd)
latest = backup.parent / "current-install.json"
tmp = latest.with_suffix(".json.incomplete")
with tmp.open("w") as output:
    json.dump(proof, output, indent=2)
    output.write("\n")
    output.flush()
    os.fsync(output.fileno())
os.replace(tmp, latest)
fd = os.open(backup.parent, os.O_RDONLY | os.O_DIRECTORY)
try:
    os.fsync(fd)
finally:
    os.close(fd)
PY
then
    if ! python3 "$retention" --stage-root /home/yonder/nereus-core \
        --backup-root /var/lib/nereus-build --current "$checkpoint" --previous "$previous" \
        --backup "${backup##*/}" --apply; then
        echo "WARNING: installed Core is healthy; artifact cleanup failed. Recovery retained at $backup" >&2
    fi
else
    echo "WARNING: installed Core is healthy; success manifest failed. Older recovery artifacts retained." >&2
fi
