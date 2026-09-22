#!/usr/bin/env bash
#
# deploy.sh: publish website/public/ to the NereusSDR web server with rsync.
#
# Usage:
#   website/deploy.sh             publish the site
#   website/deploy.sh --dry-run   show what would change, change nothing
#
# Works from any directory: paths are resolved from this script's location.
#
# The destination is $NEREUS_WEB_TARGET, by default
# nereus-web:/var/www/nereussdr/ (nereus-web is an SSH alias in
# ~/.ssh/config, see website/README.md). Point NEREUS_WEB_TARGET at a local
# directory to try the script without the server. rsync runs with --delete:
# anything in the destination that is not in website/public/ is removed,
# .DS_Store, *.swp and .git* files included. Those names are left out when
# the staging copy described below is made, so they are never sent, and the
# rsync to the destination has no excludes: an exclude there would also keep
# --delete from removing such files already in the destination.
#
# The rsync options are limited to ones that both GNU rsync 3.x and the
# openrsync that macOS ships as /usr/bin/rsync support. openrsync accepts
# --chmod but only applies it together with --perms, which is outside that
# set. So the site is first copied to a temporary staging directory, made
# world-readable there (chmod -R a+rX), and sent from the staging copy: new
# files arrive readable by Caddy, which runs as its own user.

set -euo pipefail

readonly default_target="nereus-web:/var/www/nereussdr/"
readonly min_files=3

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly script_dir
readonly src_dir="${script_dir}/public"
readonly target="${NEREUS_WEB_TARGET:-$default_target}"

die() { printf 'deploy.sh: error: %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<'EOF'
Usage: website/deploy.sh [--dry-run]

Publishes website/public/ with rsync to $NEREUS_WEB_TARGET
(default: nereus-web:/var/www/nereussdr/).

  --dry-run   show what would change on the target; change nothing
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

command -v rsync >/dev/null 2>&1 || die "rsync not found"

# Safety checks. rsync runs with --delete, so deploying an empty or
# half-built source directory would wipe the live site.
# The excludes apply to the staging copy only (see the header).
excludes=(--exclude=.DS_Store --exclude='*.swp' --exclude='.git*')
[[ -d "$src_dir" ]] || die "source directory not found: ${src_dir}"
[[ -f "${src_dir}/index.html" ]] || die "missing ${src_dir}/index.html; refusing to deploy"
# Count only the files that would be sent (the excluded names do not count).
file_count="$(find "$src_dir" \( -name .DS_Store -o -name '*.swp' -o -name '.git*' \) -prune \
    -o -type f -print | wc -l)"
file_count="${file_count//[[:space:]]/}"
if (( file_count < min_files )); then
    die "${src_dir} holds ${file_count} file(s), fewer than ${min_files}; refusing to deploy"
fi

# Stage a world-readable copy (see the header). -t keeps modification times,
# so rsync still sends only what changed.
staging="$(mktemp -d "${TMPDIR:-/tmp}/nereus-deploy.XXXXXX")"
trap 'rm -rf -- "$staging"' EXIT
rsync -r -l -t "${excludes[@]}" "${src_dir}/" "${staging}/"
chmod -R a+rX "$staging"

# No excludes here: with none, --delete also removes any .DS_Store, *.swp or
# .git* file that is on the target (the staging copy holds none).
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
    echo "Dry run only. Run website/deploy.sh without --dry-run to publish."
    exit 0
fi

if [[ "$target" == "$default_target" ]]; then
    echo
    echo "Published to:"
    echo "  https://nereussdr.com/"
    echo "  https://www.nereussdr.com/"
    # Informational only: DNS or the certificate may not be live yet.
    status="$(curl -sS -o /dev/null -w '%{http_code}' --max-time 10 https://nereussdr.com/)" || true
    echo "HTTPS check, https://nereussdr.com/ answered: ${status:-nothing}"
    if [[ "${status:-000}" == "000" ]]; then
        echo "(000 means no answer; expected until DNS and the certificate are live)"
    fi
fi
