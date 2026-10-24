#!/usr/bin/env bash
#
# setup-server.sh: prepare the NereusSDR website server (nereus-web) to run
# the Roundtable server beside the website, behind the website's Caddy.
#
# Run it as root on the server itself, from a copy of roundtable/ (it reads
# its sibling files and ../server/roundtable.conf.sample). It is idempotent:
# every step looks at the current state first and skips work that is
# already done, so it is safe to run again, for example after changing the
# unit or the Caddy site file.
#
# Usage:
#   bash setup-server.sh [--dry-run] [--no-start]
#
#   --dry-run    check everything and print what would change; change
#                nothing. Off the server (not root, not Ubuntu) it still
#                prints every step and checks what it can.
#   --no-start   install and configure, but start, stop, reload or enable
#                nothing; the reloads and restarts its changes call for are
#                kept in /etc/nereus-roundtable/pending-actions, and the
#                next run that starts services carries them out
#
# Settings, as environment variables (defaults are nereussdr.com's):
#   RT_HOST         the server's host name              roundtable.nereussdr.com
#   RT_DEPLOY_USER  the account roundtable/deploy.sh    nereusweb (the
#                   publishes the code as               website's)
#   RT_MEMORY_MB    the server's memory, MiB, that      found on the host
#                   Caddy's GOMEMLIMIT is worked out
#                   from
#   RT_ROOT         a folder every path is taken        empty (the real /)
#                   under, for trying the script off
#                   the server
#
# Typical run, from the repository root on the maintainer's Mac (see
# roundtable/README.md; the rm keeps a second copy from landing inside the
# first):
#   ssh root@<server> rm -rf /root/roundtable
#   scp -r roundtable root@<server>:/root/roundtable
#   ssh root@<server> bash /root/roundtable/deploy/setup-server.sh --dry-run
#   ssh root@<server> bash /root/roundtable/deploy/setup-server.sh
#
# Steps:
#   1. Check the host, the inputs and the settings.
#   2. Install python3, python3-websockets, python3-cryptography and rsync
#      from Ubuntu. Caddy must be there already: the website's own
#      website/deploy/setup-server.sh installs it.
#   3. Make /opt/nereus-roundtable for roundtable/deploy.sh, owned by the
#      deploy account (the website's, which must exist).
#   4. Write /etc/nereus-roundtable/roundtable.conf from the sample when
#      there is none, with RT_HOST as public_host. An existing one is never
#      changed: it holds the server's own settings (helpers, limits).
#   5. Install the units: the server, the daily job and the snapshot with
#      their timers, and Caddy's drop-in with GOMEMLIMIT from the server's
#      memory.
#   6. Write /etc/caddy/sites/roundtable.caddy, after caddy validate has
#      passed on the website's Caddyfile with it in place. The Caddyfile's
#      line "import /etc/caddy/sites/*.caddy" pulls it in.
#   7. Enable the server and the two timers, start what is stopped, restart
#      what had a unit change, reload Caddy after a site change (restart it
#      after a drop-in change, which a reload does not apply), in this run
#      or in an earlier one that did not get this far; check the server
#      holds its two loopback ports.
#   8. Print a summary.
#
# It never changes the firewall, the SSH daemon, the website's Caddyfile or
# its files, and touches no account.

set -euo pipefail
umask 022

rt_root="${RT_ROOT:-}"
rt_root="${rt_root%/}"
readonly rt_root

readonly ETC_DIR="${rt_root}/etc/nereus-roundtable"
readonly SERVICE_CONF="${ETC_DIR}/roundtable.conf"
readonly PENDING_FILE="${ETC_DIR}/pending-actions"
readonly CODE_DIR="${rt_root}/opt/nereus-roundtable"
readonly UNIT_DIR="${rt_root}/etc/systemd/system"
readonly UNIT_NAME="nereus-roundtable.service"
readonly JOB_UNITS=(nereus-roundtable-daily.service nereus-roundtable-daily.timer nereus-roundtable-snapshot.service nereus-roundtable-snapshot.timer)
readonly TIMERS=(nereus-roundtable-daily.timer nereus-roundtable-snapshot.timer)
readonly CADDY_DROPIN_DEST="${UNIT_DIR}/caddy.service.d/nereus.conf"
readonly CADDYFILE="${rt_root}/etc/caddy/Caddyfile"
readonly SITES_DIR="${rt_root}/etc/caddy/sites"
readonly SITE_DEST="${SITES_DIR}/roundtable.caddy"
# The line in website/deploy/Caddyfile that pulls the site files in.
readonly IMPORT_LINE="import /etc/caddy/sites/*.caddy"
readonly PACKAGES=(python3 python3-websockets python3-cryptography rsync)
readonly PORTS=(8720 8721)
# The site address in the repository's site file, replaced by RT_HOST.
readonly SITE_HOST="roundtable.nereussdr.com"
# One name a line in PENDING_FILE: what changed on disk and has not yet
# been acted on in step 7.
readonly PENDING_FLAGS=(units_changed service_changed caddy_changed site_changed)

# Caddy's GOMEMLIMIT, worked out as for the rendezvous server
# (rendezvous/deploy/setup-server.sh): the system keeps a fixed reserve and
# Caddy's Go runtime aims to stay under half of the rest. At 1 GB (961 MiB
# seen by the kernel): 352 MiB. The Roundtable server's own ceiling is the
# unit's MemoryMax=256M, so at 1 GB Caddy, Roundtable and the reserve fit.
readonly MEMORY_RESERVE_MB=256
readonly CADDY_SHARE_PERCENT=50
readonly MEMORY_MIN_MB=768

export DEBIAN_FRONTEND=noninteractive
# needrestart (part of Ubuntu server) can restart running services after
# apt installs packages. List mode only reports, so this script restarts
# nothing it does not name.
export NEEDRESTART_MODE=l

log()  { printf '\n==> %s\n' "$*"; }
note() { printf '    %s\n' "$*"; }
die()  { printf 'setup-server.sh: error: %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<'EOF'
Usage: bash setup-server.sh [--dry-run] [--no-start]

Run as root on the website server, from a copy of roundtable/. Safe to run
again. Settings come from RT_* environment variables; see the script's
header and roundtable/README.md.
EOF
}

apt_get() {
    apt-get -o DPkg::Lock::Timeout=300 "$@"
}

is_installed() {
    local status
    # shellcheck disable=SC2016  # ${Status} is a dpkg-query field, not a shell variable
    status="$(dpkg-query -W -f='${Status}' "$1" 2>/dev/null || true)"
    [[ "$status" == "install ok installed" ]]
}

dry_run=0
no_start=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run) dry_run=1 ;;
        --no-start) no_start=1 ;;
        -h | --help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
    shift
done

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly script_dir
readonly repo_unit="${script_dir}/${UNIT_NAME}"
readonly repo_site="${script_dir}/roundtable.caddy"
readonly repo_caddy_dropin="${script_dir}/caddy-override.conf"
readonly repo_sample="${script_dir}/../server/roundtable.conf.sample"

rt_host="${RT_HOST:-$SITE_HOST}"
deploy_user="${RT_DEPLOY_USER:-nereusweb}"

would=""
if (( dry_run )); then
    would=" (dry run)"
fi

tmp_paths=()
cleanup() {
    local p
    for p in ${tmp_paths[@]+"${tmp_paths[@]}"}; do
        rm -rf -- "$p"
    done
}
trap cleanup EXIT
trap 'exit 130' INT TERM HUP

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/nereus-rt-setup.XXXXXX")"
tmp_paths+=("$work_dir")

# Pending actions survive a --no-start run or one that stopped part way.
# They are held as " flag flag " (bash 3 on macOS has no associative
# arrays, and a dry run there must work).
pending=" "
if [[ -f "$PENDING_FILE" ]]; then
    while IFS= read -r flag; do
        if [[ -n "$flag" ]]; then
            pending+="${flag} "
        fi
    done < "$PENDING_FILE"
fi
is_pending() {
    [[ "$pending" == *" $1 "* ]]
}
write_pending() {
    if (( dry_run )); then
        return 0
    fi
    local flag out=""
    for flag in "${PENDING_FLAGS[@]}"; do
        if is_pending "$flag"; then
            out+="${flag}"$'\n'
        fi
    done
    install -d -m 0755 "$ETC_DIR"
    if [[ -n "$out" ]]; then
        printf '%s' "$out" > "${PENDING_FILE}.new"
        chmod 600 "${PENDING_FILE}.new"
        mv -f -- "${PENDING_FILE}.new" "$PENDING_FILE"
    else
        rm -f -- "$PENDING_FILE"
    fi
}
mark_changed() {
    local flag
    for flag in "$@"; do
        if ! is_pending "$flag"; then
            pending+="${flag} "
        fi
    done
    write_pending
}

# $1 source, $2 destination, $3 mode. Prints new, same or changed; writes
# only when not a dry run, through a temp file renamed into place.
install_file() {
    local src="$1" dest="$2" mode="$3" result
    if [[ -L "$dest" ]]; then
        die "${dest} is a symlink; refusing to follow it as root"
    fi
    if [[ ! -e "$dest" ]]; then
        result=new
    elif cmp -s "$src" "$dest"; then
        result=same
    else
        result=changed
    fi
    if [[ "$result" != same ]] && ! (( dry_run )); then
        install -d -m 0755 "$(dirname "$dest")"
        local staged
        staged="$(mktemp "$(dirname "$dest")/.$(basename "$dest").XXXXXX")"
        cp -- "$src" "$staged"
        chmod "$mode" "$staged"
        mv -f -- "$staged" "$dest"
    fi
    printf '%s' "$result"
}
# $1 source, $2 destination, $3 mode, $4..: flags to set when it changed.
place() {
    local src="$1" dest="$2" mode="$3" result
    shift 3
    result="$(install_file "$src" "$dest" "$mode")"
    note "${dest}${would}: ${result}"
    if [[ "$result" != same && $# -gt 0 ]]; then
        # A dry run keeps the flags in memory only, for step 7 to list.
        mark_changed "$@"
    fi
}

# ---------------------------------------------------------------------------
log "1/8 Checking the host and the inputs"

on_server=1
if [[ "$(id -u)" -ne 0 ]]; then
    if (( dry_run )); then
        note "not root: a dry run checks only what it can read"
        on_server=0
    else
        die "run this as root"
    fi
fi
os_id=""
if [[ -r "${rt_root}/etc/os-release" ]]; then
    # shellcheck source=/dev/null  # /etc/os-release exists only on the server
    os_id="$(. "${rt_root}/etc/os-release" 2>/dev/null && printf '%s' "${ID:-}")" || true
fi
if [[ "$os_id" != "ubuntu" ]]; then
    if (( dry_run )); then
        note "not Ubuntu (found: ${os_id:-unknown}): the package and service steps only say what they would do"
        on_server=0
    else
        die "this script supports Ubuntu only (found: ${os_id:-unknown})"
    fi
fi
have_systemd=0
if (( on_server )) && [[ -d /run/systemd/system ]] && command -v systemctl >/dev/null 2>&1; then
    have_systemd=1
fi
if [[ -n "$rt_root" ]] && ! (( no_start )); then
    # The files go under RT_ROOT, but systemctl would act on this host.
    note "RT_ROOT is set (${rt_root}): nothing is started (as with --no-start)"
    no_start=1
fi
if ! (( have_systemd )) && ! (( no_start )); then
    note "no running systemd here: nothing is started (as with --no-start)"
    no_start=1
fi
for f in "$repo_unit" "$repo_site" "$repo_caddy_dropin" "$repo_sample"; do
    [[ -f "$f" && -r "$f" ]] || die "missing ${f}; run this from a whole copy of roundtable/"
done
for f in "${JOB_UNITS[@]}"; do
    [[ -f "${script_dir}/${f}" ]] || die "missing ${script_dir}/${f}"
done
[[ "$rt_host" =~ ^[a-z0-9]([a-z0-9.-]*[a-z0-9])?$ ]] || die "RT_HOST must be a lower-case host name with no port (got: ${rt_host})"
[[ "$deploy_user" =~ ^[a-z_][a-z0-9_-]*$ ]] || die "RT_DEPLOY_USER is not an account name: ${deploy_user}"
grep -q "^${SITE_HOST} {\$" "$repo_site" || die "${repo_site} has no ${SITE_HOST} site block"

memory_mb="${RT_MEMORY_MB:-}"
if [[ -z "$memory_mb" ]]; then
    if [[ -r /proc/meminfo ]]; then
        memory_mb="$(awk '/^MemTotal:/ { print int($2 / 1024) }' /proc/meminfo)"
    else
        memory_mb=961
        note "no /proc/meminfo here: working out the memory limit for 961 MiB (a 1 GB server)"
    fi
fi
[[ "$memory_mb" =~ ^[0-9]+$ ]] || die "RT_MEMORY_MB must be a whole number of MiB"
if (( memory_mb < MEMORY_MIN_MB )); then
    die "the server has ${memory_mb} MiB; at least ${MEMORY_MIN_MB} MiB is needed for Caddy, Roundtable and the system"
fi
caddy_limit_mb=$(( (memory_mb - MEMORY_RESERVE_MB) * CADDY_SHARE_PERCENT / 100 ))
note "host name: ${rt_host}"
note "memory: ${memory_mb} MiB; Caddy GOMEMLIMIT ${caddy_limit_mb} MiB; Roundtable MemoryMax 256 MiB (its unit)"

# ---------------------------------------------------------------------------
log "2/8 Packages: ${PACKAGES[*]}"

missing=()
if (( on_server )); then
    for pkg in "${PACKAGES[@]}"; do
        is_installed "$pkg" || missing+=("$pkg")
    done
else
    missing=("${PACKAGES[@]}")
fi
if [[ ${#missing[@]} -eq 0 ]]; then
    note "all installed"
elif (( dry_run )); then
    if (( on_server )); then
        note "would install: ${missing[*]}"
    else
        note "would install any of these that are missing: ${missing[*]}"
    fi
else
    note "installing: ${missing[*]}"
    apt_get update
    apt_get install -y "${missing[@]}"
fi
have_caddy=0
if command -v caddy >/dev/null 2>&1; then
    have_caddy=1
    note "caddy: $(caddy version 2>/dev/null | head -n 1)"
elif (( dry_run )); then
    note "caddy is not installed here: the website's setup-server.sh installs it; validation is skipped in this dry run"
else
    die "caddy is not installed; run website/deploy/setup-server.sh first"
fi

# ---------------------------------------------------------------------------
log "3/8 Code directory ${CODE_DIR}"

if (( on_server )) && ! id -u "$deploy_user" >/dev/null 2>&1; then
    if (( dry_run )); then
        note "the deploy account ${deploy_user} does not exist; a real run stops here"
    else
        die "the deploy account ${deploy_user} does not exist; run website/deploy/setup-server.sh first, or set RT_DEPLOY_USER"
    fi
fi
if [[ -L "$CODE_DIR" ]]; then
    die "${CODE_DIR} is a symlink; refusing to follow it as root"
fi
if (( dry_run )); then
    if [[ -d "$CODE_DIR" ]]; then note "${CODE_DIR} present"; else note "would make ${CODE_DIR} for ${deploy_user}"; fi
else
    install -d -m 0755 "$CODE_DIR"
    chown -h "${deploy_user}:${deploy_user}" "$CODE_DIR"
    note "${CODE_DIR}: owner ${deploy_user}, mode 755"
fi

# ---------------------------------------------------------------------------
log "4/8 Configuration ${SERVICE_CONF}"

if [[ -L "$SERVICE_CONF" ]]; then
    die "${SERVICE_CONF} is a symlink; refusing to follow it as root"
fi
if [[ -e "$SERVICE_CONF" ]]; then
    note "${SERVICE_CONF} present; left as it is (it holds this server's own settings)"
    if ! grep -q "^public_host = ${rt_host}\$" "$SERVICE_CONF"; then
        note "note: its public_host is not ${rt_host}; change it by hand if the name moved"
    fi
else
    sed "s|^public_host = .*\$|public_host = ${rt_host}|" "$repo_sample" > "${work_dir}/roundtable.conf"
    grep -q "^public_host = ${rt_host}\$" "${work_dir}/roundtable.conf" || die "the sample has no public_host line"
    place "${work_dir}/roundtable.conf" "$SERVICE_CONF" 644 service_changed
fi

# ---------------------------------------------------------------------------
log "5/8 Units and Caddy's drop-in"

place "$repo_unit" "${UNIT_DIR}/${UNIT_NAME}" 644 service_changed units_changed
for f in "${JOB_UNITS[@]}"; do
    place "${script_dir}/${f}" "${UNIT_DIR}/${f}" 644 units_changed
done
sed "s|@GOMEMLIMIT@|${caddy_limit_mb}MiB|" "$repo_caddy_dropin" > "${work_dir}/caddy-override.conf"
place "${work_dir}/caddy-override.conf" "$CADDY_DROPIN_DEST" 644 caddy_changed units_changed

# ---------------------------------------------------------------------------
log "6/8 Caddy site ${SITE_DEST}"

sed "s|^${SITE_HOST} {\$|${rt_host} {|" "$repo_site" > "${work_dir}/roundtable.caddy"
if [[ ! -f "$CADDYFILE" ]]; then
    if (( dry_run )); then
        note "${CADDYFILE} not found: the website's setup-server.sh installs it"
    else
        die "${CADDYFILE} not found; run website/deploy/setup-server.sh first"
    fi
elif ! grep -qxF "$IMPORT_LINE" "$CADDYFILE"; then
    if (( dry_run )); then
        note "${CADDYFILE} lacks the line '${IMPORT_LINE}': install the website's current Caddyfile first"
    else
        die "${CADDYFILE} lacks the line '${IMPORT_LINE}'; run website/deploy/setup-server.sh with the current website/deploy/Caddyfile first"
    fi
fi
# Validate before anything is installed: a copy of the Caddyfile whose
# import names a staging folder holding the other site files and the new
# roundtable.caddy.
if (( have_caddy )) && [[ -f "$CADDYFILE" ]]; then
    stage="${work_dir}/caddy"
    mkdir -p "${stage}/sites"
    if [[ -d "$SITES_DIR" ]]; then
        for f in "$SITES_DIR"/*.caddy; do
            [[ -e "$f" ]] || continue
            [[ "$(basename "$f")" == roundtable.caddy ]] && continue
            cp -- "$f" "${stage}/sites/"
        done
    fi
    cp -- "${work_dir}/roundtable.caddy" "${stage}/sites/roundtable.caddy"
    awk -v line="$IMPORT_LINE" -v repl="import ${stage}/sites/*.caddy" '$0 == line { print repl; next } { print }' \
        "$CADDYFILE" > "${stage}/Caddyfile"
    note "validating ${CADDYFILE} with the new site file"
    if ! caddy validate --config "${stage}/Caddyfile" --adapter caddyfile; then
        die "the Caddy configuration with the new site file failed validation; ${SITE_DEST} was left unchanged"
    fi
fi
if (( dry_run )); then
    if [[ -d "$SITES_DIR" ]]; then note "${SITES_DIR} present"; else note "would make ${SITES_DIR}"; fi
else
    install -d -m 0755 "$SITES_DIR"
fi
place "${work_dir}/roundtable.caddy" "$SITE_DEST" 644 site_changed

# ---------------------------------------------------------------------------
log "7/8 Services"

pending_list() {
    local flag out=""
    for flag in "${PENDING_FLAGS[@]}"; do
        if is_pending "$flag"; then
            out+=" ${flag}"
        fi
    done
    printf '%s' "${out:- nothing}" | sed 's/^ //'
}
clear_flag() {
    pending="${pending/ $1 / }"
    write_pending
}
if (( dry_run )); then
    note "would enable ${UNIT_NAME} ${TIMERS[*]}, start what is stopped, and act on: $(pending_list)"
elif (( no_start )); then
    note "starting nothing; kept for the next run: $(pending_list)"
else
    journal_hint="see: journalctl -u nereus-roundtable -n 50 --no-pager"
    if is_pending units_changed; then
        systemctl daemon-reload
        clear_flag units_changed
    fi
    systemctl enable "$UNIT_NAME" "${TIMERS[@]}"
    if is_pending service_changed && systemctl is-active --quiet "$UNIT_NAME"; then
        note "restarting ${UNIT_NAME} (its unit or configuration changed)"
        systemctl restart "$UNIT_NAME" || die "systemctl restart ${UNIT_NAME} failed; ${journal_hint}"
    elif ! systemctl is-active --quiet "$UNIT_NAME"; then
        if [[ -f "${CODE_DIR}/nereus_roundtable/__main__.py" ]]; then
            note "starting ${UNIT_NAME}"
            systemctl start "$UNIT_NAME" || die "systemctl start ${UNIT_NAME} failed; ${journal_hint}"
        else
            note "no code in ${CODE_DIR} yet: run roundtable/deploy.sh, then systemctl start ${UNIT_NAME}"
        fi
    fi
    clear_flag service_changed
    for timer in "${TIMERS[@]}"; do
        systemctl start "$timer"
    done
    caddy_hint="see: journalctl -u caddy -n 50 --no-pager"
    if is_pending caddy_changed; then
        note "restarting caddy (its drop-in changed; a reload does not apply one)"
        systemctl restart caddy || die "systemctl restart caddy failed; ${caddy_hint}"
        clear_flag caddy_changed
        clear_flag site_changed
    elif is_pending site_changed; then
        caddy validate --config "$CADDYFILE" --adapter caddyfile \
            || die "the installed Caddy configuration failed validation; Caddy keeps serving its previous one; fix ${SITE_DEST}"
        note "reloading caddy"
        systemctl reload caddy || die "systemctl reload caddy failed; Caddy keeps serving its previous configuration; ${caddy_hint}"
        clear_flag site_changed
    fi
    if systemctl is-active --quiet "$UNIT_NAME"; then
        note "${UNIT_NAME}: active"
        sleep 2
        for port in "${PORTS[@]}"; do
            listeners="$(ss -Hltn "( sport = :${port} )" | awk '{ print $4 }' | sort -u | tr '\n' ' ')"
            note "port ${port}: ${listeners:-none}"
            if [[ -z "$listeners" ]]; then
                die "nothing listens on port ${port}; ${journal_hint}"
            fi
            for address in $listeners; do
                case "$address" in
                    127.0.0.1:"$port" | "[::1]:$port") ;;
                    *) die "port ${port} is held on ${address}, not loopback only; check ${SERVICE_CONF}" ;;
                esac
            done
        done
    fi
fi

# ---------------------------------------------------------------------------
log "8/8 Summary"

note "host name:     ${rt_host} (DNS A and AAAA records point it at this server)"
note "code:          ${CODE_DIR} (written by ${deploy_user} through roundtable/deploy.sh)"
note "configuration: ${SERVICE_CONF}"
note "state:         ${rt_root}/var/lib/nereus-roundtable (database, backup/ with 2 snapshots)"
note "timers:        daily 04:00 UTC, snapshot 03:30 UTC"
note "Caddy:         ${SITE_DEST}; drop-in ${CADDY_DROPIN_DEST} (GOMEMLIMIT ${caddy_limit_mb}MiB)"
if (( dry_run )); then
    note ""
    note "Dry run only: nothing was changed."
fi
