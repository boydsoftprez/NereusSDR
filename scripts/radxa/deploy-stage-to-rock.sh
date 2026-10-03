#!/usr/bin/env bash
# deploy-stage-to-rock.sh [--dry-run <scratch-name>] <checkpoint>
#
# Copy a stage built by build-rock-in-docker.sh to the Rock, check it there,
# and print the install command. It NEVER runs the installer and never stops,
# starts or restarts anything on the Rock.
#
# What it does:
#   1. locally: re-hash out/<checkpoint>/stage-<checkpoint>.tar
#   2. on the Rock, read-only: CPU features, live package versions against the
#      image's, disk space, which checkpoint is installed now; refuses if
#      /home/yonder/nereus-core/<checkpoint>/stage already exists
#   3. copies the tar + both manifests into a root-only temp dir on the Rock,
#      re-hashes the tar, unpacks it, checks every file against the manifest
#      (content, exact file set, types and modes), gives it to yonder, then runs
#      the stage checks: installer preconditions, systemd-analyze verify, ldd
#      (stdout + stderr) and nereusd --help as yonder
#   4. only if all of that passed, renames it to
#      /home/yonder/nereus-core/<checkpoint>/stage and prints
#        bash /var/lib/nereus-build/install-core-selector.sh <checkpoint> <previous>
#
# --dry-run <scratch-name>: the same steps into /home/yonder/nereus-core/<scratch-name>
# instead. The name must be <checkpoint>-<suffix> (so it can never be a real
# checkpoint directory) and must not exist yet; the directory is removed again
# at the end, pass or fail. The install command is printed for reference only.
#
# Environment:
#   ROCK=root@192.168.109.106   ssh destination (key auth, BatchMode)
#   ROCK_SSH_OPTS="-o Port=2222 ..."  extra -o options for both ssh and scp
#   ALLOW_VERSION_DRIFT=1       continue although a required package (glibc, Qt,
#                               FFTW, OpenSSL) differs upstream between the image
#                               and the live Rock
#
# Written for the macOS system bash (3.2): no heredocs inside $(...).
set -euo pipefail

usage() { printf 'usage: %s [--dry-run <checkpoint>-<suffix>] <checkpoint>\n' "$0" >&2; exit 2; }
die() { printf 'deploy-stage-to-rock: %s\n' "$*" >&2; exit 1; }

dry_name=
if [[ "${1:-}" == --dry-run ]]; then
    [[ $# -eq 3 ]] || usage
    dry_name=$2
    shift 2
fi
[[ $# -eq 1 ]] || usage
checkpoint=$1
# Refuse a stage that has not passed the startup check (added 2026-09-23 after
# the 9e8ae810 Core crashed on the Rock at start; `--help` alone missed it).
smoke_log="$(cd "$(dirname "$0")" && pwd)/out/$checkpoint/smoke-start.log"
if ! { test -s "$smoke_log" && grep -q "nereusd started" "$smoke_log" && grep -qx "exit=124" "$smoke_log"; }; then
    printf 'Refusing: run ./smoke-start-in-docker.sh %s first and see it pass.\n' "$checkpoint" >&2
    exit 1
fi
[[ "$checkpoint" =~ ^[0-9a-f]{8}$ ]] || usage
if [[ -n "$dry_name" ]]; then
    [[ "$dry_name" =~ ^[0-9a-f]{8}-[a-z0-9][a-z0-9-]*$ && "${dry_name%%-*}" == "$checkpoint" ]] \
        || die "dry-run name must look like $checkpoint-<suffix> (lowercase letters, digits, dashes)"
    name=$dry_name
else
    name=$checkpoint
fi

here=$(cd "$(dirname "$0")" && pwd)
rock=${ROCK:-root@192.168.109.106}
out=$here/out/$checkpoint
tar_name=stage-$checkpoint.tar
dir=/home/yonder/nereus-core/$name
installer=/var/lib/nereus-build/install-core-selector.sh
ssh_opts=(-o BatchMode=yes -o ConnectTimeout=15)
extra_opts=()
if [[ -n "${ROCK_SSH_OPTS:-}" ]]; then read -r -a extra_opts <<< "$ROCK_SSH_OPTS"; fi
ssh_opts+=(${extra_opts[@]+"${extra_opts[@]}"})

# remote_bash <script> [args...]: run <script> with bash on the Rock. The
# arguments are %q-quoted because ssh hands the remote shell one string.
remote_bash() {
    local script=$1 args=
    shift
    if [[ $# -gt 0 ]]; then args=$(printf '%q ' "$@"); fi
    printf '%s\n' "$script" | ssh "${ssh_opts[@]}" "$rock" "bash -s -- $args"
}

if [[ -n "$dry_name" ]]; then mode="DRY RUN into $dir"; else mode="deploy into $dir"; fi
printf '==== %s: %s (%s)\n' "$checkpoint" "$mode" "$rock"

# ---------------------------------------------------------------- 1. local
for f in "$tar_name" "stage-$checkpoint.sha256" "stage-$checkpoint.tar.sha256" image-versions.txt; do
    [[ -s "$out/$f" ]] || die "missing $out/$f (run build-rock-in-docker.sh $checkpoint first)"
done
(cd "$out" && shasum -a 256 -c "stage-$checkpoint.tar.sha256") || die 'local tar does not match its hash'
tar_sha=$(cut -d' ' -f1 "$out/stage-$checkpoint.tar.sha256")
tar_bytes=$(wc -c < "$out/$tar_name" | tr -d ' ')

# ---------------------------------------------------------------- 2. preflight (read-only)
read -r -d '' PREFLIGHT <<'REMOTE' || true
set -euo pipefail
dir=$1 checkpoint=$2 installer=$3
shift 3
echo "arch=$(uname -m)"
echo "features=$(grep -m1 '^Features' /proc/cpuinfo | cut -d: -f2 | xargs)"
echo "features_uniform=$(grep '^Features' /proc/cpuinfo | sort -u | wc -l)"
if id yonder >/dev/null 2>&1; then echo "yonder=ok"; fi
if [[ -e $dir ]]; then echo "dir_exists=yes"; else echo "dir_exists=no"; fi
if [[ -e $dir/stage ]]; then echo "stage_exists=yes"; else echo "stage_exists=no"; fi
if [[ -e $dir/stage-$checkpoint.tar ]]; then echo "tar_exists=yes"; else echo "tar_exists=no"; fi
real=/home/yonder/nereus-core/$checkpoint
if [[ -e $real/stage || -e $real/stage-$checkpoint.tar ]]; then echo "real_blocked=yes"; else echo "real_blocked=no"; fi
if [[ -f $installer ]]; then echo "installer=yes"; else echo "installer=no"; fi
echo "free_kb=$(df -Pk /home/yonder | awk 'NR==2{print $4}')"
# Which checkpoint is installed now: the stage whose three binaries hash-match
# the installed ones. Only real checkpoint directories (hex names) count.
trio() { sha256sum "$1/bin/nereusd" "$1/lib/libNereusCore.so" "$1/lib/librade.so.0.1" 2>/dev/null | cut -d' ' -f1 | tr '\n' ' ' || true; }
inst=$(trio /usr/local)
matches=
for s in /home/yonder/nereus-core/*/stage; do
    c=$(basename "$(dirname "$s")")
    if [[ "$c" =~ ^[0-9a-f]{8,40}$ && -n "$inst" && "$(trio "$s/usr/local")" == "$inst" ]]; then
        matches="$matches $c"
    fi
done
matches=$(echo $matches)
echo "installed=$matches"
first=${matches%% *}
if [[ -n "$first" && -e /var/lib/nereus-build/rollback-$first-before-$checkpoint ]]; then
    echo "rollback_exists=yes"
else
    echo "rollback_exists=no"
fi
echo "service=$(systemctl is-active nereusd < /dev/null 2> /dev/null || true)"
for p in "$@"; do
    v=$(dpkg-query -W -f='${Version}' "$p" 2>/dev/null || true)
    echo "pkg $p ${v:--}"
done
REMOTE

pkgs=()
while read -r _ _ pkg _; do pkgs+=("$pkg"); done \
    < <(grep -E '^(MATCH|DRIFT-REVISION|DRIFT-UPSTREAM|MISSING) ' "$out/image-versions.txt")
printf '\n==== preflight on the Rock (read-only)\n'
pre=$(remote_bash "$PREFLIGHT" "$dir" "$checkpoint" "$installer" ${pkgs[@]+"${pkgs[@]}"}) || die 'preflight ssh failed'
printf '%s\n' "$pre" | grep -v '^pkg '
kv() { printf '%s\n' "$pre" | sed -n "s/^$1=//p" | head -n1; }

[[ "$(kv arch)" == aarch64 ]] || die 'the target is not aarch64'
[[ "$(kv yonder)" == ok ]] || die 'user yonder missing on the target'
[[ "$(kv features_uniform)" == 1 ]] || die 'CPU cores report different feature sets'
# Everything the build relies on: -march=armv8.2-a needs atomics/asimdrdm/crc32/
# dcpop, isa-scan.py allowed the rest. An ARMv8.0 board (Pi 4, RK3399) stops here.
for feat in fp asimd aes pmull sha1 sha2 crc32 atomics fphp asimdhp asimdrdm lrcpc dcpop asimddp; do
    [[ " $(kv features) " == *" $feat "* ]] || die "target CPU lacks '$feat'; this build is for the RK3588S"
done
echo 'CPU features: ARMv8.2-A and every optional feature the scan allowed are present'
if [[ -n "$dry_name" ]]; then
    [[ "$(kv dir_exists)" == no ]] || die "refusing: scratch directory $dir already exists"
    if [[ "$(kv real_blocked)" == yes ]]; then
        echo "note: a real deploy of $checkpoint would refuse here: /home/yonder/nereus-core/$checkpoint/stage (or its tar) already exists"
    fi
else
    [[ "$(kv stage_exists)" == no ]] || die "refusing: $dir/stage already exists"
    [[ "$(kv tar_exists)" == no ]] || die "refusing: $dir/stage-$checkpoint.tar already exists"
fi
[[ "$(kv installer)" == yes ]] || printf 'WARNING: %s not found on the Rock\n' "$installer"
(( $(kv free_kb) > 3 * tar_bytes / 1024 )) || die 'not enough free space under /home/yonder'

printf '\n==== image vs live Rock package versions\n'
drift_block=
while read -r _ class pkg img_field _; do
    img=${img_field#image=}
    live=$(printf '%s\n' "$pre" | awk -v p="$pkg" '$1=="pkg" && $2==p {print $3}')
    up_img=${img#*:}; up_img=${up_img%-*}
    up_live=${live#*:}; up_live=${up_live%-*}
    if [[ "$img" == "$live" ]]; then st=MATCH
    elif [[ "$live" == - || "$img" == - ]]; then st=MISSING
    elif [[ "$up_img" == "$up_live" ]]; then st=DRIFT-REVISION
    else st=DRIFT-UPSTREAM; fi
    printf '%-15s %-8s %-24s image=%s rock=%s\n' "$st" "$class" "$pkg" "$img" "$live"
    if [[ "$class" == required ]] && [[ "$st" == DRIFT-UPSTREAM || "$st" == MISSING ]]; then
        drift_block="$drift_block $pkg"
    fi
done < <(grep -E '^(MATCH|DRIFT-REVISION|DRIFT-UPSTREAM|MISSING) ' "$out/image-versions.txt")
if [[ -n "$drift_block" ]]; then
    [[ "${ALLOW_VERSION_DRIFT:-0}" == 1 ]] \
        || die "upstream version differs on the Rock for:$drift_block (ALLOW_VERSION_DRIFT=1 to override after review)"
    printf 'WARNING: ALLOW_VERSION_DRIFT=1, continuing despite:%s\n' "$drift_block"
fi

# ---------------------------------------------------------------- 3. copy, verify, unpack, check
read -r -d '' PREPARE <<'REMOTE' || true
set -euo pipefail
exec 9>/run/lock/nereus-core-deploy.lock
flock -x 9
dir=$1 dry=$2
# The marker records that this run created $dir, so cleanup may remove it.
if [[ -n "$dry" ]]; then
    [[ ! -e "$dir" ]]
    install -d -o yonder -g yonder "$dir"
    touch "$dir/.created-by-deploy"
elif [[ ! -e "$dir" ]]; then
    install -d -o yonder -g yonder "$dir"
    touch "$dir/.created-by-deploy"
fi
mktemp -d "$dir/.deploy.XXXXXX"
REMOTE

read -r -d '' CLEANUP <<'REMOTE' || true
set -euo pipefail
exec 9>/run/lock/nereus-core-deploy.lock
flock -x 9
tmp=$1 dry=$2 dir=$3 rc=$4
rm -rf -- "$tmp"
created=no
if [[ -f "$dir/.created-by-deploy" ]]; then created=yes; rm -f -- "$dir/.created-by-deploy"; fi
# A dry run removes its scratch dir, but only one this run created and never
# anything named like a real checkpoint directory. A failed real deploy only
# removes the checkpoint dir if this run created it and it is empty again.
if [[ -n "$dry" && $created == yes && ! "$(basename "$dir")" =~ ^[0-9a-f]{8,40}$ ]]; then
    rm -rf -- "$dir"
    echo "dry run: removed $dir"
elif [[ $rc != 0 && $created == yes ]] && rmdir -- "$dir" 2> /dev/null; then
    echo "removed the empty $dir this run had created"
fi
REMOTE

read -r -d '' UNPACK <<'REMOTE' || true
set -euo pipefail
exec 9>/run/lock/nereus-core-deploy.lock
flock -x 9
tmp=$1 dir=$2 checkpoint=$3 tar_sha=$4
tarf=stage-$checkpoint.tar
# The live Core keeps running: stay out of its way (CPU and, where the I/O
# scheduler honours it, disk).
renice -n 10 -p $$ > /dev/null
ionice -c 3 -p $$ 2> /dev/null || true
cd "$tmp"
# The hash computed on the Mac just now, then the one the build wrote.
echo "$tar_sha  $tarf" | sha256sum --strict -c
sha256sum --strict -c "$tarf.sha256"
mkdir stage.partial
tar -xpf "$tarf" -C stage.partial --no-same-owner
(cd stage.partial && sha256sum --strict --quiet -c "../stage-$checkpoint.sha256")
echo "every file matches stage-$checkpoint.sha256 ($(wc -l < "stage-$checkpoint.sha256") entries)"
diff <(cut -c67- "stage-$checkpoint.sha256") \
     <(cd stage.partial && find . \( -type f -o -type l \) | LC_ALL=C sort)
echo 'no extra or missing files'
diff <(tar -tvf "$tarf" | awk '{p=$6; if (p == "./") p = "."; else sub(/\/$/, "", p); print $1, p}' | LC_ALL=C sort -k2) \
     <(cd stage.partial && find . -printf '%M %p\n' | LC_ALL=C sort -k2)
echo 'types and modes match the tar'
chown -R yonder:yonder stage.partial
chmod 755 "$tmp"
s=$tmp/stage.partial
# install-core-selector.sh's own preconditions on the stage (read-only tests).
test -x "$s/usr/local/bin/nereusd"
test -s "$s/usr/local/bin/nereusd"
test -s "$s/usr/local/lib/libNereusCore.so"
test -s "$s/usr/local/lib/librade.so.0.1"
test -f "$s/usr/local/lib/libNereusCore.so"
test -f "$s/usr/local/share/doc/nereussdr/LICENSE"
test -d "$s/usr/local/share/doc/nereussdr/licenses"
test -s "$s/usr/lib/systemd/system/nereusd.service"
test -s "$s/usr/local/share/nereusd/nereusd.conf.sample"
test -s "$s/usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz"
for f in LICENSE LICENSE-APACHE LICENSE-MIT COMMIT; do
    test -s "$s/usr/local/share/doc/nereussdr/deepfilter/$f"
done
echo 'installer preconditions hold (DFNR model and licences included)'
systemd-analyze verify "$s/usr/lib/systemd/system/nereusd.service" < /dev/null
echo 'systemd-analyze verify: passed'
# stderr too: "version `GLIBC_2.xx' not found" goes there.
ldd_out=$(runuser -u yonder -- env LD_LIBRARY_PATH="$s/usr/local/lib" ldd "$s/usr/local/bin/nereusd" 2>&1 < /dev/null)
if printf '%s\n' "$ldd_out" | grep -q 'not found'; then printf '%s\n' "$ldd_out"; echo 'ldd: something not found'; exit 1; fi
printf '%s\n' "$ldd_out" | grep -E 'libNereusCore|librade'
echo "ldd: $(printf '%s\n' "$ldd_out" | grep -c '=>') libraries resolved, none missing"
printf '%s\n' "$ldd_out" | grep -q "libNereusCore.so => $s/usr/local/lib/libNereusCore.so"
printf '%s\n' "$ldd_out" | grep -q "librade.so.0.1 => $s/usr/local/lib/librade.so.0.1"
echo 'ldd: everything resolves; the private libraries come from the stage'
runuser -u yonder -- env LANG=C.UTF-8 LD_LIBRARY_PATH="$s/usr/local/lib" "$s/usr/local/bin/nereusd" --help > "$tmp/help.txt" < /dev/null
echo "nereusd --help ran on the Rock as yonder ($(wc -l < "$tmp/help.txt") lines)"
for f in "$tarf" "stage-$checkpoint.sha256" "stage-$checkpoint.tar.sha256"; do
    [[ ! -e "$dir/$f" ]]
    mv "$tmp/$f" "$dir/$f"
    chown yonder:yonder "$dir/$f"
done
# Last and atomic: stage/ appears only after every check above passed.
[[ ! -e "$dir/stage" ]]
mv -T "$s" "$dir/stage"
(cd "$dir/stage" && sha256sum --strict --quiet -c "../stage-$checkpoint.sha256")
echo "stage in place: $dir/stage (owner $(stat -c %U:%G "$dir/stage")), re-verified at its final path"
# A checked new candidate replaces older unused candidates. The current Core,
# its previous stage, and its complete rollback remain protected. Dry-run
# directories never trigger retention. A missing/stale receipt fails closed.
if [[ "${dir##*/}" == "$checkpoint" ]]; then
    if ! python3 - "$checkpoint" <<'PY'
import hashlib, json, pathlib, subprocess, sys
root = pathlib.Path("/var/lib/nereus-build")
proof = json.loads((root / "current-install.json").read_text())
backup = root / proof["backup"]
assert proof == json.loads((backup / "successful-install.json").read_text())
assert proof["post_stop_config_complete"] and (backup / "daemon-config").is_dir()
for name, expected in proof["installed_sha256"].items():
    assert hashlib.sha256((pathlib.Path("/usr/local") / name).read_bytes()).hexdigest() == expected
for name, expected in proof["recovery_sha256"].items():
    digest = hashlib.sha256()
    with (backup / name).open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    assert digest.hexdigest() == expected
subprocess.run([sys.executable, str(root / "prune-core-artifacts.py"),
                "--stage-root", "/home/yonder/nereus-core", "--backup-root", str(root),
                "--current", proof["checkpoint"], "--previous", proof["previous"],
                "--keep-stage", sys.argv[1], "--backup", proof["backup"],
                "--apply"], check=True)
PY
    then
        echo 'WARNING: stage verified; older artifacts retained because retention could not verify the live recovery.' >&2
    fi
fi
find "$dir/stage" \( -type f -o -type l \) -printf '%M %u:%g %10s %P\n' | LC_ALL=C sort -k4
REMOTE

printf '\n==== copy to the Rock\n'
tmp=$(remote_bash "$PREPARE" "$dir" "$dry_name") || die 'could not create the remote temp dir'
[[ "$tmp" == "$dir"/.deploy.?????? ]] || die "unexpected remote temp dir: $tmp"
echo "remote temp dir: $tmp (root, 0700)"
cleanup_remote() {
    local rc=$?
    remote_bash "$CLEANUP" "$tmp" "$dry_name" "$dir" "$rc" || true
    exit "$rc"
}
trap cleanup_remote EXIT
scp -q "${ssh_opts[@]}" "$out/$tar_name" "$out/stage-$checkpoint.sha256" \
    "$out/stage-$checkpoint.tar.sha256" "$rock:$tmp/"
echo "copied $tar_name ($tar_bytes bytes) and both manifests"

printf '\n==== verify, unpack and check on the Rock\n'
remote_bash "$UNPACK" "$tmp" "$dir" "$checkpoint" "$tar_sha"

# ---------------------------------------------------------------- 4. install command
installed=$(kv installed)
if [[ "$installed" =~ ^[0-9a-f]{8,40}$ ]]; then
    previous=$installed
    prev_note="The Rock runs $previous now (installed binaries match /home/yonder/nereus-core/$previous/stage)"
    if [[ "$previous" == "$checkpoint" ]]; then
        prev_note="$prev_note; that is this very checkpoint, so there is nothing to install"
    fi
    if [[ "$(kv rollback_exists)" == yes ]]; then
        prev_note="$prev_note; WARNING: /var/lib/nereus-build/rollback-$previous-before-$checkpoint already exists, the installer will refuse"
    fi
else
    previous='<previous>'
    prev_note="Could not tell which checkpoint is installed (matches: '${installed:-none}'); fill in <previous> by hand"
fi
cmd="ssh $rock 'bash $installer $checkpoint $previous'"
printf '\n'
if [[ -n "$dry_name" ]]; then
    printf 'DRY RUN passed; %s is removed next. Nothing real was touched.\n' "$dir"
    printf 'For a real deploy of %s this would print:\n  %s.\n  %s\n' "$checkpoint" "$prev_note" "$cmd"
else
    printf 'Stage ready: %s/stage. Nothing was installed and nereusd was not touched.\n' "$dir"
    printf '%s. To install, the controller runs:\n  %s\n' "$prev_note" "$cmd"
fi
