#!/usr/bin/env bash
#
# deploy.sh: publish the Roundtable server's code to the website server
# with rsync.
#
# Usage:
#   roundtable/deploy.sh             publish the code
#   roundtable/deploy.sh --dry-run   show what would change, change nothing
#
# Works from any directory: paths are resolved from this script's location.
#
# The destination is $NEREUS_RT_TARGET, by default
# nereus-web:/opt/nereus-roundtable/ (nereus-web is the SSH alias in
# ~/.ssh/config for the website's deploy account, see website/README.md, so
# no address is in the repository; /opt/nereus-roundtable is made for that
# account by roundtable/deploy/setup-server.sh, and the server's unit runs
# the code from there). Point NEREUS_RT_TARGET at a local directory to try
# it without the server.
#
# It publishes roundtable/server/nereus_roundtable/, the web pages' static
# files (server/static/) and the sample configuration, nothing else: no
# tests, no configuration of the server's own (setup-server.sh writes that)
# and no database. rsync runs with --delete, so the destination ends up with
# exactly those files. It never runs setup-server.sh and never restarts
# anything: the deploy account has no root. After a publish, restart the
# server as root (the script prints the command).
#
# As in website/deploy.sh, the rsync options are limited to ones that both
# GNU rsync 3.x and macOS's openrsync support, and the files are sent from a
# world-readable staging copy, so the server's account can read them.

set -euo pipefail

readonly default_target="nereus-web:/opt/nereus-roundtable/"
readonly required=(__init__.py __main__.py clock.py config.py grid.py http.py identity.py limits.py messages.py moderation.py protocol.py retention.py rooms.py service.py store.py transport.py turns.py web.py)

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly script_dir
readonly src_dir="${script_dir}/server"
readonly target="${NEREUS_RT_TARGET:-$default_target}"

die() { printf 'deploy.sh: error: %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<'EOF2'
Usage: roundtable/deploy.sh [--dry-run]

Publishes roundtable/server/ (the server's code, its static files and the
sample configuration) with rsync to $NEREUS_RT_TARGET (default:
nereus-web:/opt/nereus-roundtable/).

  --dry-run   show what would change on the target; change nothing
EOF2
}

dry_run=0
if [[ $# -gt 0 ]]; then
    case "$1" in
        --dry-run) dry_run=1; shift ;;
        -h | --help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
fi
if [[ $# -gt 0 ]]; then
    usage >&2
    exit 2
fi

command -v rsync >/dev/null 2>&1 || die "rsync not found"
command -v python3 >/dev/null 2>&1 || die "python3 not found"

# Safety checks: rsync runs with --delete, so a half-built source would
# leave the server with a program that cannot start.
[[ -d "${src_dir}/nereus_roundtable" ]] || die "source directory not found: ${src_dir}/nereus_roundtable"
for name in "${required[@]}"; do
    [[ -f "${src_dir}/nereus_roundtable/${name}" ]] || die "missing ${src_dir}/nereus_roundtable/${name}; refusing to deploy"
done
[[ -d "${src_dir}/static" ]] || die "missing ${src_dir}/static; refusing to deploy"
[[ -f "${src_dir}/roundtable.conf.sample" ]] || die "missing ${src_dir}/roundtable.conf.sample; refusing to deploy"
# Every module must at least parse (no bytecode is written).
python3 - "${src_dir}/nereus_roundtable" <<'PY' || die "a module does not parse; refusing to deploy"
import ast, pathlib, sys
for directory in sys.argv[1:]:
    for path in sorted(pathlib.Path(directory).glob("*.py")):
        ast.parse(path.read_text(encoding="utf-8"), str(path))
PY

staging="$(mktemp -d "${TMPDIR:-/tmp}/nereus-rt-deploy.XXXXXX")"
trap 'rm -rf -- "$staging"' EXIT
for part in nereus_roundtable static; do
    mkdir -p "${staging}/${part}"
    rsync -r -t --exclude='__pycache__' --exclude='*.pyc' --exclude='.DS_Store' \
        "${src_dir}/${part}/" "${staging}/${part}/"
done
cp -p "${src_dir}/roundtable.conf.sample" "${staging}/"
chmod -R a+rX "$staging"
file_count="$(find "$staging" -type f | wc -l)"
file_count="${file_count//[[:space:]]/}"

rsync_args=(-r -l -t -z -v --delete)
if (( dry_run )); then
    rsync_args+=(--dry-run)
fi

echo "Source: ${src_dir}/ (${file_count} files)"
echo "Target: ${target}"
if (( dry_run )); then
    echo "Mode:   dry run, nothing is changed"
fi
echo

if ! rsync "${rsync_args[@]}" "${staging}/" "$target"; then
    die "rsync failed (for the default target, check the nereus-web entry in ~/.ssh/config, see website/README.md)"
fi

if (( dry_run )); then
    echo
    echo "Dry run only. Run roundtable/deploy.sh without --dry-run to publish."
    exit 0
fi

echo
echo "Published. The server runs the new code once restarted, as root on the server:"
echo "  systemctl restart nereus-roundtable"
echo "(Stations and live viewers reconnect on their own.)"
