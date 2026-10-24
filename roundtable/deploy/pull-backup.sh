#!/usr/bin/env bash
#
# pull-backup.sh: copy the Roundtable server's newest nightly snapshot to
# this machine, and keep the newest 7 here (design 6.2, C12).
#
# Run it on JJ's own machine, not on the server: a pull, so the server holds
# no credentials for anything else. The server writes a snapshot at 03:30
# UTC (nereus-roundtable-snapshot.timer) and keeps 2; this keeps 7, so
# anything removed on the server (text past a year, a removed message, a
# deleted archive) is gone from every copy within 7 days.
#
# Usage:
#   roundtable/deploy/pull-backup.sh             pull the newest snapshot
#   roundtable/deploy/pull-backup.sh --dry-run   say what would be pulled and
#                                                removed; change nothing here
#
# Settings, as environment variables:
#   NEREUS_RT_BACKUP_SOURCE      the SSH destination: an   nereus-web-root
#                                alias in ~/.ssh/config for
#                                root on the website server
#                                (roundtable/README.md), as
#                                the snapshots live in the
#                                server's private state
#                                directory, which only root
#                                reads
#   NEREUS_RT_BACKUP_REMOTE_DIR  where they are there      /var/lib/nereus-roundtable/backup
#   NEREUS_RT_BACKUP_DIR         where they go here        ~/NereusSDR-backups/roundtable
#   NEREUS_RT_SSH                the ssh program           ssh
#
# Every night, for example from crontab -e on the Mac (after the server's
# 03:30 UTC snapshot):
#   15 5 * * * /path/to/NereusSDR/roundtable/deploy/pull-backup.sh >> ~/NereusSDR-backups/pull.log 2>&1
#
# A pulled copy is checked with SQLite's integrity_check before it takes the
# place of anything, so a broken transfer never counts as one of the 7.

set -euo pipefail
umask 077

readonly KEEP=7
readonly NAME_RE='^roundtable-[0-9]{4}-[0-9]{2}-[0-9]{2}\.db$'

source_host="${NEREUS_RT_BACKUP_SOURCE:-nereus-web-root}"
remote_dir="${NEREUS_RT_BACKUP_REMOTE_DIR:-/var/lib/nereus-roundtable/backup}"
local_dir="${NEREUS_RT_BACKUP_DIR:-${HOME}/NereusSDR-backups/roundtable}"
ssh_cmd="${NEREUS_RT_SSH:-ssh}"

die() { printf 'pull-backup.sh: error: %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<'EOF'
Usage: roundtable/deploy/pull-backup.sh [--dry-run]

Copies the newest Roundtable snapshot from $NEREUS_RT_BACKUP_SOURCE
(default nereus-web-root) to $NEREUS_RT_BACKUP_DIR (default
~/NereusSDR-backups/roundtable) and keeps the newest 7 there.

  --dry-run   say what would be pulled and removed; change nothing here
EOF
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

[[ "$remote_dir" =~ ^/[A-Za-z0-9._/-]+$ ]] || die "NEREUS_RT_BACKUP_REMOTE_DIR must be an absolute path of plain characters"
command -v python3 >/dev/null 2>&1 || die "python3 not found (it checks each copy)"

# The newest snapshot there, by its date in the name.
listing="$("$ssh_cmd" "$source_host" "ls -1 ${remote_dir}")" || die "could not list ${remote_dir} on ${source_host}"
newest="$(printf '%s\n' "$listing" | grep -E "$NAME_RE" | sort | tail -n 1 || true)"
[[ -n "$newest" ]] || die "no snapshot in ${remote_dir} on ${source_host}"
echo "Newest there: ${newest}"

if (( dry_run )); then
    if [[ -f "${local_dir}/${newest}" ]]; then
        echo "Already here: ${local_dir}/${newest}"
    else
        echo "Would pull it to ${local_dir}/${newest}"
    fi
else
    mkdir -p "$local_dir"
    if [[ -f "${local_dir}/${newest}" ]]; then
        echo "Already here: ${local_dir}/${newest}"
    else
        part="${local_dir}/.${newest}.part"
        trap 'rm -f -- "$part"' EXIT
        "$ssh_cmd" "$source_host" "cat ${remote_dir}/${newest}" > "$part" || die "the copy of ${newest} failed"
        check="$(python3 - "$part" <<'PY'
import sqlite3, sys
db = sqlite3.connect("file:" + sys.argv[1] + "?mode=ro", uri=True)
print(db.execute("PRAGMA integrity_check").fetchone()[0])
PY
)" || die "the copy of ${newest} does not open as a database"
        [[ "$check" == ok ]] || die "the copy of ${newest} failed its integrity check: ${check}"
        mv -f -- "$part" "${local_dir}/${newest}"
        echo "Pulled: ${local_dir}/${newest} ($(wc -c < "${local_dir}/${newest}" | tr -d ' ') bytes)"
    fi
fi

# Keep the newest 7 here (with the one pulled in a dry run counted).
names="$( { if [[ -d "$local_dir" ]]; then ls -1 "$local_dir"; fi; if (( dry_run )); then printf '%s\n' "$newest"; fi; } \
    | grep -E "$NAME_RE" | sort -u || true)"
count="$(printf '%s\n' "$names" | grep -c . || true)"
if (( count > KEEP )); then
    printf '%s\n' "$names" | head -n $(( count - KEEP )) | while IFS= read -r old; do
        if (( dry_run )); then
            echo "Would remove: ${local_dir}/${old}"
        else
            rm -f -- "${local_dir}/${old}"
            echo "Removed: ${local_dir}/${old}"
        fi
    done
fi
echo "Kept here: $(( count > KEEP ? KEEP : count )) of at most ${KEEP}"
