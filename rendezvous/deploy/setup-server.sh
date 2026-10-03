#!/usr/bin/env bash
#
# setup-server.sh: prepare a dedicated server for the NereusSDR rendezvous
# service, its relays (coturn, and the WebSocket relay nereus-relay) and the
# Caddy in front of the service and the WebSocket relay.
#
# Run it as root on the server itself, from a copy of rendezvous/deploy/
# (it reads its sibling files). It is idempotent: every step looks at the
# current state first and skips work that is already done, so it is safe to
# run again, for example after changing turnserver.conf.
#
# Usage:
#   bash setup-server.sh [--dry-run] [--no-start] [--rotate-secret]
#
#   --dry-run        check everything and print what would change; change
#                    nothing
#   --no-start       install and configure, but start, stop or enable no
#                    service (the default on a host not booted with systemd,
#                    such as the containers of the checks in rendezvous/tests/);
#                    the reloads and restarts its changes call for are kept
#                    in /etc/nereus-rendezvous/pending-actions, and the next
#                    run that starts services carries them out
#   --rotate-secret  make a new TURN secret and a new relay secret (see
#                    rendezvous/README.md, "Rotating the secret")
#
# Settings, as environment variables (defaults are nereussdr.com's). Every
# value that belongs to one server is here, never in the files beside this
# script, so the same files serve any server:
#   RV_HOST                  the service's host name       rv.nereussdr.com
#   RV_RELAY_HOST4           the IPv4-only relay name      rv4.nereussdr.com
#   RV_RELAY_HOST6           the IPv6-only relay name      rv6.nereussdr.com
#   RV_PUBLIC_IPV4           the server's public IPv4      found on the host
#   RV_PUBLIC_IPV6           the server's public IPv6      found on the host
#   RV_RELAY_SLOTS           relays at once (total-quota)  128
#   RV_WS_RELAY_SLOTS        WebSocket relay sessions at   16
#                            once (nereus-relay's slots)
#   RV_TRANSFER_GB_PER_MONTH the data-use report's         1000
#                            threshold, GB a month (it
#                            caps nothing)
#   RV_DATA_USE_INTERFACE    the interface whose outbound  the default
#                            bytes the report counts       route's
#   RV_MEMORY_MB             the server's memory, MiB,     found on the host
#                            that the memory limits are
#                            worked out from
#   RV_DEPLOY_USER           the account deploy.sh uses    nereusrv
#   RV_DEPLOY_KEY            that account's SSH public     (needed when the
#                            key line ('ssh-ed25519 AAAA   account does not
#                            ... comment'); its            exist yet)
#                            authorized_keys ends up
#                            holding exactly this line
#   RV_MANAGE_CADDY          yes: install Caddy and own    yes
#                            /etc/caddy/Caddyfile (a
#                            dedicated server); no: leave
#                            Caddy alone (a server shared
#                            with a website; see the
#                            README)
#
# Typical run, from the repository root on the maintainer's Mac (see
# rendezvous/README.md; the rm keeps a second copy from landing inside the
# first):
#   ssh root@<server> rm -rf /root/rendezvous
#   scp -r rendezvous root@<server>:/root/rendezvous
#   ssh root@<server> "RV_DEPLOY_KEY='$(cat ~/.ssh/<key>.pub)' bash /root/rendezvous/deploy/setup-server.sh --dry-run"
#   ssh root@<server> "RV_DEPLOY_KEY='$(cat ~/.ssh/<key>.pub)' bash /root/rendezvous/deploy/setup-server.sh"
#
# Steps:
#   1. Check the host, the inputs and the settings; refuse to go on while
#      any other process holds UDP 3478 or UDP 443 (the relay's ports).
#   2. Install Caddy from Caddy's own apt repository, and coturn,
#      python3-websockets, python3-cryptography and rsync from Ubuntu,
#      without letting a package start a service, and leave coturn disabled
#      until step 9 (also when the install fails).
#   3. Make the deploy account, with exactly the given SSH key.
#   4. Make the TURN secret and the relay secret (root only) once, and
#      coturn's DTLS key pair.
#   5. Write /etc/turnserver.conf: turnserver.conf from this directory plus
#      the lines only this server knows (secret, addresses, sizing).
#   6. Write /etc/nereus-rendezvous/rendezvous.conf and relay.conf.
#   7. Write /etc/caddy/Caddyfile from Caddyfile here, after caddy validate.
#   8. Install the units and drop-ins (the service and the WebSocket relay
#      with their memory ceilings, coturn's, Caddy's, the data-use report
#      and its timer), the kernel setting of sysctl.conf, and make
#      /opt/nereus-rendezvous for deploy.sh.
#   9. Enable Caddy, coturn, the service, the WebSocket relay and the timer;
#      start what is stopped, reload Caddy after a Caddyfile change, and
#      restart only what had a unit or configuration change (a coturn
#      restart drops every live relay, a WebSocket relay restart makes every
#      leg join again), in this run or in an earlier one that did not get
#      this far (--no-start, or a run that stopped with an error); check
#      exactly which ports each one holds.
#  10. Print a summary.
#
# It never changes the firewall (the README lists the rules) or the SSH
# daemon, and touches no account but the deploy account.

set -euo pipefail
umask 022

readonly ETC_DIR="/etc/nereus-rendezvous"
readonly SECRET_FILE="${ETC_DIR}/turn-secret"
readonly RELAY_SECRET_FILE="${ETC_DIR}/relay-secret"
readonly COTURN_DIR="${ETC_DIR}/coturn"
readonly DTLS_CERT="${COTURN_DIR}/cert.pem"
readonly DTLS_KEY="${COTURN_DIR}/key.pem"
readonly SERVICE_CONF="${ETC_DIR}/rendezvous.conf"
readonly RELAY_CONF="${ETC_DIR}/relay.conf"
readonly TURNSERVER_CONF="/etc/turnserver.conf"
readonly CADDYFILE_DEST="/etc/caddy/Caddyfile"
readonly CODE_DIR="/opt/nereus-rendezvous"
readonly UNIT_NAME="nereus-rendezvous.service"
readonly UNIT_DEST="/etc/systemd/system/${UNIT_NAME}"
readonly MEMORY_DROPIN_DEST="/etc/systemd/system/${UNIT_NAME}.d/memory.conf"
readonly RELAY_UNIT_NAME="nereus-relay.service"
readonly RELAY_UNIT_DEST="/etc/systemd/system/${RELAY_UNIT_NAME}"
readonly RELAY_MEMORY_DROPIN_DEST="/etc/systemd/system/${RELAY_UNIT_NAME}.d/memory.conf"
readonly DROPIN_DEST="/etc/systemd/system/coturn.service.d/nereus.conf"
readonly CADDY_DROPIN_DEST="/etc/systemd/system/caddy.service.d/nereus.conf"
readonly USE_SERVICE_DEST="/etc/systemd/system/nereus-data-use.service"
readonly USE_TIMER_DEST="/etc/systemd/system/nereus-data-use.timer"
readonly USE_SCRIPT_DEST="/usr/local/libexec/nereus-rendezvous/data-use"
readonly USE_CONF="${ETC_DIR}/data-use.conf"
readonly SYSCTL_DEST="/etc/sysctl.d/60-nereus-rendezvous.conf"
# What changed on disk and has not yet been acted on in step 9: one name of
# PENDING_FLAGS a line, root only. Every change is written here the moment
# it is made, and step 9 takes each one off once it has done what it calls
# for, so a run with --no-start (or one that stopped with an error) leaves
# the reloads and restarts to the next run instead of losing them: that run
# finds its files "same".
readonly PENDING_FILE="${ETC_DIR}/pending-actions"
readonly PENDING_FLAGS=(units_changed caddy_changed caddyfile_changed coturn_changed service_changed relay_changed)
readonly CREDENTIAL_PATH="/run/credentials/${UNIT_NAME}/turn-secret"
readonly RELAY_CREDENTIAL_PATH="/run/credentials/${UNIT_NAME}/relay-secret"
readonly RELAY_OWN_CREDENTIAL_PATH="/run/credentials/${RELAY_UNIT_NAME}/relay-secret"
readonly SERVICE_PORT=8710
# The WebSocket relay's Unix socket (in the RuntimeDirectory its unit
# makes), mode and group: Caddy (group caddy) may connect, nobody else.
readonly WS_RELAY_SOCKET="/run/nereus-relay/relay.sock"
readonly WS_RELAY_SOCKET_MODE="0660"
readonly WS_RELAY_SOCKET_GROUP="caddy"
readonly RELAY_PORTS=(3478 443)
readonly PACKAGES=(coturn python3 python3-websockets python3-cryptography rsync)
# The site address in the repository's Caddyfile, replaced by RV_HOST.
readonly CADDYFILE_SITE="rv.nereussdr.com"

# The official Caddy apt repository, as website/deploy/setup-server.sh and
# the Caddy install docs have it.
readonly CADDY_PREREQS=(debian-keyring debian-archive-keyring apt-transport-https curl gnupg)
readonly CADDY_KEY_URL="https://dl.cloudsmith.io/public/caddy/stable/gpg.key"
readonly CADDY_LIST_URL="https://dl.cloudsmith.io/public/caddy/stable/debian.deb.txt"
readonly CADDY_KEYRING="/usr/share/keyrings/caddy-stable-archive-keyring.gpg"
readonly CADDY_SOURCES="/etc/apt/sources.list.d/caddy-stable.list"

# How many relays (allocations) coturn serves at once: its total-quota.
# A session runs two ICE connections (control and media), each taking up
# to 2 relays at an end that relays, so 4 at one end and 8 at both: 128 is
# about 32 sessions relayed at one end or 16 at both (rendezvous document
# section 8). The one number to change for the relay's size.
readonly DEFAULT_RELAY_SLOTS=128
# How many WebSocket relay sessions (one Core leg and one device leg each)
# nereus-relay serves at once: its slots. 16 matches coturn's 128 slots,
# about 16 sessions relayed at both ends, and what one vCPU carries in
# Python (rendezvous document section 12.5).
readonly DEFAULT_WS_RELAY_SLOTS=16
# The data-use report's threshold, in GB (10^9 bytes) a month: a warning
# line in the journal once the server has sent more than this in the
# calendar month. It caps nothing.
readonly DEFAULT_TRANSFER_GB_PER_MONTH=1000

# The memory limits, worked out from the server's memory in one place
# (rendezvous document section 9.1). The system and coturn keep a fixed
# reserve; of the rest, Caddy's Go runtime aims to stay under half
# (GOMEMLIMIT, a soft limit that makes it collect garbage harder) and the
# service may use 35% before the kernel ends it (MemoryMax), leaving 15% as
# headroom. At 1 GB (961 MiB seen by the kernel): Caddy 352 MiB, the service
# 246 MiB. At 2 GB (1967 MiB): 855 and 598.
readonly MEMORY_RESERVE_MB=256
readonly CADDY_SHARE_PERCENT=50
readonly SERVICE_SHARE_PERCENT=35
readonly MEMORY_MIN_MB=768
# The WebSocket relay's ceiling (MemoryMax) comes from its slots, not the
# server's memory: 32 MiB for Python and the idle relay, and 2 MiB a slot,
# about four times what a session with both legs stalled was seen to hold
# (rendezvous document section 12.5). 64 MiB at the default 16 slots, taken
# from the 15% headroom above.
readonly WS_RELAY_MEMORY_BASE_MB=32
readonly WS_RELAY_MEMORY_PER_SLOT_MB=2

# Keys only this script writes into /etc/turnserver.conf. turnserver.conf
# in the repository must not set any of them.
readonly MANAGED_KEYS=(static-auth-secret listening-ip relay-ip external-ip cert pkey bps-capacity total-quota realm)

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
Usage: bash setup-server.sh [--dry-run] [--no-start] [--rotate-secret]

Run as root on the server, from a copy of rendezvous/deploy/. Safe to run
again. Settings come from RV_* environment variables; see the script's
header and rendezvous/README.md.
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
rotate=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run) dry_run=1 ;;
        --no-start) no_start=1 ;;
        --rotate-secret) rotate=1 ;;
        -h | --help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
    shift
done

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly script_dir
readonly repo_turnserver_conf="${script_dir}/turnserver.conf"
readonly repo_unit="${script_dir}/${UNIT_NAME}"
readonly repo_relay_unit="${script_dir}/${RELAY_UNIT_NAME}"
readonly repo_dropin="${script_dir}/coturn-override.conf"
readonly repo_caddy_dropin="${script_dir}/caddy-override.conf"
readonly repo_caddyfile="${script_dir}/Caddyfile"
readonly repo_use_service="${script_dir}/nereus-data-use.service"
readonly repo_use_timer="${script_dir}/nereus-data-use.timer"
readonly repo_use_script="${script_dir}/data-use.py"
readonly repo_sysctl="${script_dir}/sysctl.conf"

rv_host="${RV_HOST:-rv.nereussdr.com}"
relay_host4="${RV_RELAY_HOST4:-rv4.nereussdr.com}"
relay_host6="${RV_RELAY_HOST6:-rv6.nereussdr.com}"
relay_slots="${RV_RELAY_SLOTS:-$DEFAULT_RELAY_SLOTS}"
ws_relay_slots="${RV_WS_RELAY_SLOTS:-$DEFAULT_WS_RELAY_SLOTS}"
transfer_gb="${RV_TRANSFER_GB_PER_MONTH:-$DEFAULT_TRANSFER_GB_PER_MONTH}"
deploy_user="${RV_DEPLOY_USER:-nereusrv}"
deploy_key="${RV_DEPLOY_KEY:-}"
manage_caddy="${RV_MANAGE_CADDY:-yes}"

# Root's temporary files, removed on exit.
tmp_paths=()
cleanup() {
    local p
    for p in ${tmp_paths[@]+"${tmp_paths[@]}"}; do
        rm -rf -- "$p"
    done
}
trap cleanup EXIT
# A signal ends the script through exit, so the EXIT trap still runs (it
# removes a policy-rc.d this script put in place, among others).
trap 'exit 130' INT TERM HUP

# Lists every socket bound to one of the given ports (every port when none
# is given), one line each:
# "<proto> <port> <local address> <pid> <program>", for UDP and TCP in both
# families, from /proc (the same on a server and in a container, without
# ss). Run as root it sees every process.
socket_holders() {
    python3 - "$@" <<'PY'
import os, socket, struct, sys
ports = {int(p) for p in sys.argv[1:]}
every = not ports
def addr(hexaddr):
    host, port = hexaddr.split(":")
    raw = bytes.fromhex(host)
    if len(raw) == 4:
        text = socket.inet_ntop(socket.AF_INET, raw[::-1])
    else:
        text = socket.inet_ntop(socket.AF_INET6, b"".join(raw[i:i + 4][::-1] for i in range(0, 16, 4)))
    return text, int(port, 16)
owners = {}
for pid in filter(str.isdigit, os.listdir("/proc")):
    try:
        comm = open("/proc/%s/comm" % pid).read().strip()
        for fd in os.listdir("/proc/%s/fd" % pid):
            try:
                link = os.readlink("/proc/%s/fd/%s" % (pid, fd))
            except OSError:
                continue
            if link.startswith("socket:["):
                owners.setdefault(link[8:-1], (pid, comm))
    except OSError:
        continue
for proto in ("udp", "udp6", "tcp", "tcp6"):
    try:
        lines = open("/proc/net/%s" % proto).read().splitlines()[1:]
    except OSError:
        continue
    for line in lines:
        fields = line.split()
        host, port = addr(fields[1])
        state, inode = fields[3], fields[9]
        # TCP sockets count only when listening (state 0A).
        if proto.startswith("tcp") and state != "0A":
            continue
        if every or port in ports:
            pid, comm = owners.get(inode, ("?", "?"))
            print(proto.rstrip("6"), port, host, pid, comm)
PY
}

# The source address this host would use to reach the internet in one
# family, found without sending anything (a UDP connect sends no packet).
detect_address() {
    python3 - "$1" <<'PY'
import socket, sys
family, target = (socket.AF_INET, "192.0.2.1") if sys.argv[1] == "4" else (socket.AF_INET6, "2001:db8::1")
try:
    s = socket.socket(family, socket.SOCK_DGRAM)
    s.connect((target, 9))
    print(s.getsockname()[0])
except OSError:
    pass
PY
}

# The interface of the default route (IPv4, else IPv6), for the data-use
# report.
default_interface() {
    python3 - <<'PY'
for line in open("/proc/net/route").read().splitlines()[1:]:
    f = line.split()
    if f[1] == "00000000" and int(f[3], 16) & 2:
        print(f[0])
        raise SystemExit
for line in open("/proc/net/ipv6_route").read().splitlines():
    f = line.split()
    if f[0] == "0" * 32 and f[1] == "00" and f[9] != "lo":
        print(f[9])
        raise SystemExit
PY
}

# True when $1 is a public unicast address of family $2 (4 or 6).
is_public_address() {
    python3 - "$1" "$2" <<'PY'
import ipaddress, sys
try:
    a = ipaddress.ip_address(sys.argv[1])
except ValueError:
    sys.exit(1)
ok = a.version == int(sys.argv[2]) and not (a.is_private or a.is_loopback or a.is_link_local or a.is_multicast or a.is_reserved or a.is_unspecified)
# The Docker checks use documentation and benchmarking ranges as stand-ins
# for public ones.
if not ok and "RV_ALLOW_DOCUMENTATION_ADDRESSES" in __import__("os").environ:
    docs = ("192.0.2.0/24", "198.51.100.0/24", "203.0.113.0/24", "2001:db8::/32", "2001:2::/48")
    ok = a.version == int(sys.argv[2]) and any(a in ipaddress.ip_network(n) for n in docs)
sys.exit(0 if ok else 1)
PY
}

# Installs $1 as $2 with mode $3 and owner $4 when it differs, keeping a
# timestamped copy of what was there. Prints "same" or "changed". In a dry
# run it only says what it would do. With $5 = secret the file holds the
# TURN secret: its copy is root's alone (600), and once the new file is in
# place every earlier copy is removed, so a rotated secret lingers in one
# copy at most, until the next change.
install_file() {
    local src="$1" dest="$2" mode="$3" owner="$4" secret="${5:-}"
    if [[ -L "$dest" ]]; then
        die "${dest} is a symlink; refusing to follow it as root"
    fi
    if [[ -f "$dest" ]] && cmp -s "$src" "$dest"; then
        if ! (( dry_run )); then
            chmod "$mode" "$dest"
            chown "$owner" "$dest"
        fi
        echo same
        return
    fi
    if (( dry_run )); then
        echo changed
        return
    fi
    local backup=""
    if [[ -e "$dest" ]]; then
        backup="${dest}.bak-$(date +%Y%m%d-%H%M%S)"
        if [[ "$secret" == secret ]]; then
            (umask 077 && cp -- "$dest" "$backup")
            chown root:root "$backup"
            chmod 600 "$backup"
        else
            cp -p -- "$dest" "$backup"
        fi
    fi
    local staged
    staged="$(mktemp "$(dirname "$dest")/.$(basename "$dest").XXXXXX")"
    cp -- "$src" "$staged"
    chmod "$mode" "$staged"
    chown "$owner" "$staged"
    mv -f -- "$staged" "$dest"
    if [[ "$secret" == secret ]]; then
        local old
        for old in "${dest}".bak-*; do
            if [[ -e "$old" && "$old" != "$backup" ]]; then
                rm -f -- "$old"
            fi
        done
    fi
    echo changed
}

# What each pending flag calls for, for the notes.
pending_action() {
    case "$1" in
        units_changed) echo "systemctl daemon-reload" ;;
        caddy_changed) echo "restart caddy" ;;
        caddyfile_changed) echo "reload caddy" ;;
        coturn_changed) echo "restart coturn" ;;
        service_changed) echo "restart ${UNIT_NAME}" ;;
        relay_changed) echo "restart ${RELAY_UNIT_NAME}" ;;
    esac
}

# The actions of every pending flag that is set, joined by "; ".
pending_list() {
    local flag text=""
    for flag in "${PENDING_FLAGS[@]}"; do
        if (( ${!flag} )); then
            text+="${text:+; }$(pending_action "$flag")"
        fi
    done
    printf '%s\n' "$text"
}

# Writes the pending flags that are set to PENDING_FILE (root 600, renamed
# into place), or removes it when none is. Nothing in a dry run.
save_pending() {
    (( dry_run )) && return 0
    [[ -L "$PENDING_FILE" ]] && die "${PENDING_FILE} is a symlink; refusing to follow it as root"
    local flag lines=""
    for flag in "${PENDING_FLAGS[@]}"; do
        if (( ${!flag} )); then
            lines+="${flag}"$'\n'
        fi
    done
    if [[ -z "$lines" ]]; then
        rm -f -- "$PENDING_FILE"
        return 0
    fi
    local staged
    staged="$(mktemp "${ETC_DIR}/.pending-actions.XXXXXX")"
    tmp_paths+=("$staged")
    chmod 600 "$staged"
    printf '%s' "$lines" > "$staged"
    mv -f -- "$staged" "$PENDING_FILE"
}

# Sets each named flag and records it at once.
mark_changed() {
    local flag
    for flag in "$@"; do
        printf -v "$flag" 1
    done
    save_pending
}

# Takes each named flag off once its action is done.
mark_done() {
    local flag
    for flag in "$@"; do
        printf -v "$flag" 0
    done
    save_pending
}

# Rule (as in website/deploy/setup-server.sh): root does no file operation
# inside a directory the deploy account can write (its home and ~/.ssh);
# that work runs as the account, where a symlink it planted reaches only
# files it could change anyway. runuser keeps the caller's working
# directory, which the account may not enter, so the command starts in /.
as_deploy_user() {
    (cd / && runuser -u "$deploy_user" -- "$@")
}

# True when $1 is one SSH public key line whose key blob names the same
# type as its first field (what ssh-keygen -l checks, without needing it).
is_ssh_key_line() {
    python3 - "$1" <<'PY'
import base64, struct, sys
line = sys.argv[1]
if "\n" in line or "\r" in line:
    sys.exit(1)
fields = line.split()
if len(fields) < 2 or not fields[0].startswith(("ssh-", "ecdsa-", "sk-")):
    sys.exit(1)
try:
    blob = base64.b64decode(fields[1], validate=True)
    (n,) = struct.unpack(">I", blob[:4])
    ok = blob[4:4 + n].decode("ascii") == fields[0]
except Exception:
    ok = False
sys.exit(0 if ok else 1)
PY
}

# Every unit the script enables, and what it tells a failure by.
enable_units() {
    local out
    if ! out="$(systemctl enable "$@" 2>&1)"; then
        printf '%s\n' "$out" >&2
        die "systemctl enable $* failed (above)"
    fi
}

# ---------------------------------------------------------------------------
log "1/10 Checking the host, the inputs and the settings"

if [[ "$(id -u)" -ne 0 ]]; then
    die "run this as root"
fi
# shellcheck source=/dev/null  # /etc/os-release exists only on the server
os_id="$(. /etc/os-release 2>/dev/null && printf '%s' "${ID:-}")" || true
if [[ "$os_id" != "ubuntu" ]]; then
    die "this script supports Ubuntu only (found: ${os_id:-unknown})"
fi
command -v python3 >/dev/null 2>&1 || die "python3 is not installed (it is on every Ubuntu server image)"
command -v runuser >/dev/null 2>&1 || die "runuser (util-linux) is not installed"
for f in "$repo_turnserver_conf" "$repo_unit" "$repo_relay_unit" "$repo_dropin" "$repo_caddy_dropin" "$repo_caddyfile" \
        "$repo_use_service" "$repo_use_timer" "$repo_use_script" "$repo_sysctl"; do
    [[ -f "$f" && -r "$f" ]] || die "missing ${f}; run this from a whole copy of rendezvous/deploy/"
done
(( dry_run )) && note "dry run: nothing is changed"
have_systemd=0
if [[ -d /run/systemd/system ]]; then
    have_systemd=1
fi
if ! (( no_start )) && ! (( have_systemd )); then
    # Not booted with systemd (a container): nothing can be started here.
    note "systemd is not running on this host: services are installed but not started (as with --no-start)"
    no_start=1
fi

for key in "${MANAGED_KEYS[@]}"; do
    if grep -Eq "^[[:space:]]*-{0,2}${key}([[:space:]]*=|[[:space:]]*$)" "$repo_turnserver_conf"; then
        die "${repo_turnserver_conf} sets ${key}, which only this script may write"
    fi
done
for name in "$rv_host" "$relay_host4" "$relay_host6"; do
    [[ "$name" =~ ^[A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?$ ]] || die "not a host name: ${name}"
done
[[ "$relay_slots" =~ ^[1-9][0-9]*$ ]] || die "RV_RELAY_SLOTS must be a whole number, at least 1 (got ${relay_slots})"
[[ "$ws_relay_slots" =~ ^[1-9][0-9]*$ ]] || die "RV_WS_RELAY_SLOTS must be a whole number, at least 1 (got ${ws_relay_slots})"
[[ "$transfer_gb" =~ ^[1-9][0-9]*$ ]] || die "RV_TRANSFER_GB_PER_MONTH must be a whole number of GB (got ${transfer_gb})"
[[ "$manage_caddy" == yes || "$manage_caddy" == no ]] || die "RV_MANAGE_CADDY must be yes or no (got ${manage_caddy})"
[[ "$deploy_user" =~ ^[a-z_][a-z0-9_-]*$ ]] || die "not an account name: ${deploy_user}"
grep -q "^${CADDYFILE_SITE} {\$" "$repo_caddyfile" || die "${repo_caddyfile} has no '${CADDYFILE_SITE} {' site line"
if [[ -n "$deploy_key" ]]; then
    is_ssh_key_line "$deploy_key" || die "RV_DEPLOY_KEY is not one SSH public key line, such as 'ssh-ed25519 AAAA... comment'"
    note "deploy key: ${deploy_key%% *} ... (for ${deploy_user})"
elif id -u "$deploy_user" >/dev/null 2>&1; then
    note "deploy account ${deploy_user} exists; no RV_DEPLOY_KEY given, so its keys are left as they are"
else
    die "the deploy account ${deploy_user} does not exist yet: set RV_DEPLOY_KEY to its SSH public key line"
fi

public4="${RV_PUBLIC_IPV4:-$(detect_address 4)}"
public6="${RV_PUBLIC_IPV6:-$(detect_address 6)}"
[[ -n "$public4" ]] || die "no public IPv4 address found; set RV_PUBLIC_IPV4"
is_public_address "$public4" 4 || die "${public4} is not a public IPv4 address; set RV_PUBLIC_IPV4"
[[ -n "$public6" ]] || die "no public IPv6 address found; set RV_PUBLIC_IPV6 (the relay needs both families)"
is_public_address "$public6" 6 || die "${public6} is not a public IPv6 address; set RV_PUBLIC_IPV6"
note "public addresses: IPv4 ${public4}, IPv6 ${public6} (they stay on this server)"

# Code in /opt from before the WebSocket relay: this script would install
# the relay's unit and a rendezvous.conf that code cannot read, and restart
# the service onto it. The code goes first (rendezvous/README.md,
# "Updating"); refused in a dry run too.
if [[ -f "${CODE_DIR}/nereus_rendezvous/__main__.py" ]] \
        && ! [[ -f "${CODE_DIR}/nereus_rendezvous/relaygrant.py" && -f "${CODE_DIR}/nereus_relay/__main__.py" ]]; then
    die "the code in ${CODE_DIR} is from before the WebSocket relay: run rendezvous/deploy.sh first, then this script again. Nothing was changed."
fi

# Sizing (rendezvous/README.md, "Limits"): the relay is sized by slots.
# total-quota is the number of allocations at once. coturn reserves max-bps
# of bps-capacity for every live allocation and refuses one (486) when
# nothing is left, so bps-capacity is slots x max-bps: never the limit
# below the slot count.
max_bps="$(sed -n 's/^max-bps=\([0-9][0-9]*\)$/\1/p' "$repo_turnserver_conf")"
[[ -n "$max_bps" ]] || die "${repo_turnserver_conf} has no max-bps line"
user_quota="$(sed -n 's/^user-quota=\([0-9][0-9]*\)$/\1/p' "$repo_turnserver_conf")"
[[ -n "$user_quota" ]] || die "${repo_turnserver_conf} has no user-quota line"
total_quota="$relay_slots"
bps_capacity=$(( relay_slots * max_bps ))
peak_kbit=$(( bps_capacity * 8 / 1000 ))
note "relay: ${total_quota} slots (total-quota), ${user_quota} per station id (user-quota), ${max_bps} bytes/s each way per slot (max-bps): bps-capacity ${bps_capacity} bytes/s, at most ${peak_kbit} kbit/s out when every slot is full"

# Memory (the formula above).
memory_mb="${RV_MEMORY_MB:-$(awk '/^MemTotal:/ { print int($2 / 1024) }' /proc/meminfo)}"
[[ "$memory_mb" =~ ^[1-9][0-9]*$ ]] || die "RV_MEMORY_MB must be a whole number of MiB (got ${memory_mb})"
(( memory_mb >= MEMORY_MIN_MB )) || die "${memory_mb} MiB of memory is too little; the rendezvous needs at least ${MEMORY_MIN_MB} (rendezvous document section 9.1)"
caddy_limit_mb=$(( (memory_mb - MEMORY_RESERVE_MB) * CADDY_SHARE_PERCENT / 100 ))
service_limit_mb=$(( (memory_mb - MEMORY_RESERVE_MB) * SERVICE_SHARE_PERCENT / 100 ))
ws_relay_limit_mb=$(( WS_RELAY_MEMORY_BASE_MB + ws_relay_slots * WS_RELAY_MEMORY_PER_SLOT_MB ))
note "memory: ${memory_mb} MiB; ${MEMORY_RESERVE_MB} kept for the system and coturn; Caddy GOMEMLIMIT ${caddy_limit_mb} MiB; the service MemoryMax ${service_limit_mb} MiB"
note "WebSocket relay: ${ws_relay_slots} sessions at once, 80000 bytes/s each way per session, MemoryMax ${ws_relay_limit_mb} MiB"

data_use_iface="${RV_DATA_USE_INTERFACE:-$(default_interface)}"
[[ -n "$data_use_iface" ]] || die "no default route found for the data-use report; set RV_DATA_USE_INTERFACE"
[[ "$data_use_iface" =~ ^[A-Za-z0-9_.:-]+$ && -r "/sys/class/net/${data_use_iface}/statistics/tx_bytes" ]] \
    || die "no network interface ${data_use_iface}; set RV_DATA_USE_INTERFACE"
note "data-use report: outbound bytes on ${data_use_iface}, a warning past ${transfer_gb} GB in a calendar month (caps nothing)"

# The relay's UDP ports must be free, or already coturn's. TCP on the same
# numbers is another matter: Caddy holds TCP 443.
holders="$(socket_holders "${RELAY_PORTS[@]}" | awk '$1 == "udp" && $5 != "turnserver"' || true)"
if [[ -n "$holders" ]]; then
    printf '%s\n' "$holders" >&2
    if (( no_start )); then
        note "another process holds a relay port (above); coturn is not started (--no-start)"
    else
        die "another process holds UDP 3478 or 443 (above); stop it first. Nothing was changed."
    fi
else
    note "UDP 3478 and 443 are free or held by coturn"
fi

# ---------------------------------------------------------------------------
log "2/10 Packages"

packages=("${PACKAGES[@]}")
if [[ "$manage_caddy" == yes ]]; then
    packages+=(caddy)
fi
missing=()
for pkg in "${packages[@]}"; do
    is_installed "$pkg" || missing+=("$pkg")
done
caddy_repo=0
if [[ " ${missing[*]} " == *" caddy "* ]] && ! [[ -s "$CADDY_KEYRING" && -s "$CADDY_SOURCES" ]]; then
    caddy_repo=1
fi
if [[ ${#missing[@]} -eq 0 ]]; then
    note "already installed: ${packages[*]}"
elif (( dry_run )); then
    note "would install: ${missing[*]}"
    if (( caddy_repo )); then
        note "would add Caddy's apt repository first"
    fi
else
    note "installing: ${missing[*]}"
    # coturn's package starts coturn with its stock configuration, which
    # allows anyone to relay, and Caddy's starts Caddy with its sample. A
    # policy-rc.d that answers 101 keeps any service from being started
    # during the install; it is removed after, and one that was already
    # there is left alone. It goes on the list of files removed on exit the
    # moment it exists, so no failure below leaves it blocking every
    # service on the host.
    policy="/usr/sbin/policy-rc.d"
    added_policy=0
    if [[ ! -e "$policy" ]]; then
        tmp_paths+=("$policy")
        printf '#!/bin/sh\nexit 101\n' > "$policy"
        chmod 755 "$policy"
        added_policy=1
    fi
    install_status=0
    apt_get update || install_status=$?
    if (( install_status == 0 && caddy_repo )); then
        note "adding Caddy's apt repository"
        repo_missing=()
        for pkg in "${CADDY_PREREQS[@]}"; do
            is_installed "$pkg" || repo_missing+=("$pkg")
        done
        if [[ ${#repo_missing[@]} -gt 0 ]]; then
            apt_get install -y "${repo_missing[@]}" || install_status=$?
        fi
        if (( install_status == 0 )); then
            repo_dir="$(mktemp -d)"
            tmp_paths+=("$repo_dir")
            { curl -1sLf "$CADDY_KEY_URL" | gpg --dearmor -o "${repo_dir}/caddy.gpg" \
                && curl -1sLf "$CADDY_LIST_URL" -o "${repo_dir}/caddy.list" \
                && install -m 0644 "${repo_dir}/caddy.gpg" "$CADDY_KEYRING" \
                && install -m 0644 "${repo_dir}/caddy.list" "$CADDY_SOURCES" \
                && apt_get update; } || install_status=$?
        fi
    fi
    if (( install_status == 0 )); then
        apt_get install -y --no-install-recommends \
            -o Dpkg::Options::=--force-confdef \
            -o Dpkg::Options::=--force-confold \
            "${missing[@]}" || install_status=$?
    fi
    if (( have_systemd )) && [[ " ${missing[*]} " == *" coturn "* ]]; then
        # The package enables coturn at boot on its stock, open
        # configuration, and it may have been unpacked even when the
        # install as a whole failed. Keep it disabled (and stopped, should
        # anything have started it) until step 9, whatever happened above.
        systemctl disable --now coturn >/dev/null 2>&1 || true
        note "coturn is disabled until its configuration is in place"
    fi
    if (( added_policy )); then
        rm -f -- "$policy"
    fi
    (( install_status == 0 )) || die "installing the packages failed (apt-get, above)"
fi
if ! (( dry_run )); then
    command -v turnserver >/dev/null 2>&1 || die "turnserver is not on PATH after installation"
    id -u turnserver >/dev/null 2>&1 || die "the turnserver account is missing after installation"
    if [[ "$manage_caddy" == yes ]]; then
        command -v caddy >/dev/null 2>&1 || die "caddy is not on PATH after installation"
    fi
fi

# ---------------------------------------------------------------------------
log "3/10 Deploy account ${deploy_user}"

if (( dry_run )); then
    if id -u "$deploy_user" >/dev/null 2>&1; then note "account present"; else note "would create ${deploy_user}"; fi
    [[ -n "$deploy_key" ]] && note "would set its authorized_keys to exactly the given key"
else
    if ! id -u "$deploy_user" >/dev/null 2>&1; then
        # No password: the account signs in with its key only.
        useradd --create-home --shell /bin/bash "$deploy_user"
        note "created ${deploy_user}"
    fi
    if [[ -n "$deploy_key" ]]; then
        deploy_home="$(getent passwd "$deploy_user" | cut -d: -f6)"
        # As the deploy account: ~/.ssh (700), the key line from stdin to a
        # temp file there (600), renamed over authorized_keys.
        key_script="$(cat <<'EOF'
set -euo pipefail
umask 077
mkdir -p "$1"
chmod 700 "$1"
new_keys="$(mktemp "$1/.authorized_keys.XXXXXX")"
trap 'rm -f -- "$new_keys"' EXIT
cat > "$new_keys"
chmod 600 "$new_keys"
if [[ -f "$2" ]] && cmp -s "$new_keys" "$2"; then echo same; else echo set; fi
mv -f -- "$new_keys" "$2"
EOF
)"
        key_result="$(printf '%s\n' "$deploy_key" \
            | as_deploy_user bash -c "$key_script" "as-${deploy_user}" "${deploy_home}/.ssh" "${deploy_home}/.ssh/authorized_keys")" \
            || die "could not write ${deploy_home}/.ssh/authorized_keys as ${deploy_user} (above)"
        note "authorized_keys: ${key_result}"
    fi
fi

# ---------------------------------------------------------------------------
log "4/10 TURN secret, relay secret and coturn's DTLS key pair"

# What changes, for step 9: coturn and the service are restarted only when
# one of their own files changed, in this run or in an earlier one that
# left it pending.
coturn_changed=0
service_changed=0
relay_changed=0
units_changed=0
caddy_changed=0
caddyfile_changed=0
if [[ -L "$PENDING_FILE" ]]; then
    die "${PENDING_FILE} is a symlink; refusing to follow it as root"
fi
if [[ -f "$PENDING_FILE" ]]; then
    while IFS= read -r line; do
        known=0
        for flag in "${PENDING_FLAGS[@]}"; do
            if [[ "$line" == "$flag" ]]; then
                printf -v "$flag" 1
                known=1
            fi
        done
        if ! (( known )) && [[ -n "$line" ]]; then
            note "${PENDING_FILE}: ignoring an unknown line"
        fi
    done < "$PENDING_FILE"
    pending="$(pending_list)"
    if [[ -n "$pending" ]]; then
        note "pending from an earlier run that started no service (--no-start) or stopped early: ${pending}"
    fi
fi
if (( dry_run )); then
    if [[ -s "$SECRET_FILE" && "$rotate" -eq 0 ]]; then
        note "secret present"
    elif [[ -s "$SECRET_FILE" ]]; then
        note "would make a new secret in ${SECRET_FILE} (--rotate-secret)"
        coturn_changed=1 service_changed=1
    else
        note "would make ${SECRET_FILE}"
        coturn_changed=1 service_changed=1
    fi
    if [[ -s "$RELAY_SECRET_FILE" && "$rotate" -eq 0 ]]; then
        note "relay secret present"
    elif [[ -s "$RELAY_SECRET_FILE" ]]; then
        note "would make a new relay secret in ${RELAY_SECRET_FILE} (--rotate-secret)"
        service_changed=1 relay_changed=1
    else
        note "would make ${RELAY_SECRET_FILE}"
        service_changed=1 relay_changed=1
    fi
    if [[ -s "$DTLS_CERT" && -s "$DTLS_KEY" ]]; then
        note "key pair present"
    else
        note "would make ${DTLS_CERT} and ${DTLS_KEY}"
        coturn_changed=1
    fi
else
    install -d -m 0755 -o root -g root "$ETC_DIR"
    install -d -m 0750 -o root -g turnserver "$COTURN_DIR"
    if [[ -s "$SECRET_FILE" && "$rotate" -eq 0 ]]; then
        note "secret present; left as it is"
    else
        # 32 random bytes as 64 hex characters: printable, so coturn's
        # configuration and the service read the same bytes (rendezvous
        # document section 8).
        staged="$(mktemp "${ETC_DIR}/.turn-secret.XXXXXX")"
        tmp_paths+=("$staged")
        chmod 600 "$staged"
        python3 -c 'import secrets; print(secrets.token_hex(32))' > "$staged"
        mv -f -- "$staged" "$SECRET_FILE"
        note "made a new secret in ${SECRET_FILE} (root only)"
        mark_changed coturn_changed service_changed
    fi
    chown root:root "$SECRET_FILE"
    chmod 600 "$SECRET_FILE"
    if [[ -s "$RELAY_SECRET_FILE" && "$rotate" -eq 0 ]]; then
        note "relay secret present; left as it is"
    else
        # The same form as the TURN secret, and a different value: the
        # service signs relay grants with it and nereus-relay checks them
        # (rendezvous document section 12.2). Nothing else reads it.
        staged="$(mktemp "${ETC_DIR}/.relay-secret.XXXXXX")"
        tmp_paths+=("$staged")
        chmod 600 "$staged"
        python3 -c 'import secrets; print(secrets.token_hex(32))' > "$staged"
        mv -f -- "$staged" "$RELAY_SECRET_FILE"
        note "made a new relay secret in ${RELAY_SECRET_FILE} (root only)"
        mark_changed service_changed relay_changed
    fi
    chown root:root "$RELAY_SECRET_FILE"
    chmod 600 "$RELAY_SECRET_FILE"
    if [[ -s "$DTLS_CERT" && -s "$DTLS_KEY" ]]; then
        note "key pair present; left as it is"
    else
        # Self-signed, for coturn's second UDP port only (turnserver.conf,
        # "Ports"): its cipher list is empty, so no DTLS session is ever made
        # and nothing ever checks the certificate.
        python3 - "$DTLS_CERT" "$DTLS_KEY" "$rv_host" <<'PY'
import datetime, os, sys
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
cert_path, key_path, host = sys.argv[1:4]
key = ec.generate_private_key(ec.SECP256R1())
name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, host)])
now = datetime.datetime.now(datetime.timezone.utc)
cert = (x509.CertificateBuilder().subject_name(name).issuer_name(name)
        .public_key(key.public_key()).serial_number(x509.random_serial_number())
        .not_valid_before(now - datetime.timedelta(days=1))
        .not_valid_after(now + datetime.timedelta(days=3650))
        .sign(key, hashes.SHA256()))
old = os.umask(0o027)
with open(key_path + ".new", "wb") as f:
    f.write(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
with open(cert_path + ".new", "wb") as f:
    f.write(cert.public_bytes(serialization.Encoding.PEM))
os.umask(old)
os.replace(key_path + ".new", key_path)
os.replace(cert_path + ".new", cert_path)
PY
        note "made ${DTLS_CERT} and ${DTLS_KEY}"
        mark_changed coturn_changed
    fi
    chown root:turnserver "$DTLS_CERT" "$DTLS_KEY"
    chmod 640 "$DTLS_CERT" "$DTLS_KEY"
fi

# ---------------------------------------------------------------------------
log "5/10 ${TURNSERVER_CONF}"

work_dir="$(mktemp -d)"
tmp_paths+=("$work_dir")
chmod 700 "$work_dir"
secret_line="static-auth-secret=(made in step 4)"
if [[ -s "$SECRET_FILE" ]]; then
    secret_line="static-auth-secret=$(cat "$SECRET_FILE")"
fi
{
    cat "$repo_turnserver_conf"
    printf '\n# ---------------------------------------------------------------- This server\n'
    printf '# Written by rendezvous/deploy/setup-server.sh; never copy these lines\n'
    printf '# into the repository.\n'
    printf 'realm=%s\n' "$rv_host"
    printf 'listening-ip=%s\n' "$public4" "$public6"
    printf 'relay-ip=%s\n' "$public4" "$public6"
    printf 'cert=%s\n' "$DTLS_CERT"
    printf 'pkey=%s\n' "$DTLS_KEY"
    printf '# %s slots of max-bps %s bytes/s (RV_RELAY_SLOTS).\n' "$relay_slots" "$max_bps"
    printf 'bps-capacity=%s\n' "$bps_capacity"
    printf 'total-quota=%s\n' "$total_quota"
    printf '%s\n' "$secret_line"
} > "${work_dir}/turnserver.conf"
would=""
if (( dry_run )); then
    would=" would be"
fi
result="$(install_file "${work_dir}/turnserver.conf" "$TURNSERVER_CONF" 640 root:turnserver secret)"
if [[ "$result" == changed ]]; then
    mark_changed coturn_changed
fi
note "${TURNSERVER_CONF}${would}: ${result}"

# ---------------------------------------------------------------------------
log "6/10 ${SERVICE_CONF} and ${RELAY_CONF}"

{
    printf '# The NereusSDR rendezvous service on this server. Written by\n'
    printf '# rendezvous/deploy/setup-server.sh; every key not here has the\n'
    printf "# default in rendezvous/server/rendezvous.conf.sample.\n\n"
    printf '[rendezvous]\n'
    printf 'listen = 127.0.0.1:%s [::1]:%s\n' "$SERVICE_PORT" "$SERVICE_PORT"
    printf 'trusted_proxies = 127.0.0.1 ::1\n'
    # IPv4 first in both lists, as the service's own defaults: the pinned
    # libjuice uses only the first STUN server (rendezvous document
    # section 8).
    printf 'stun_urls = stun:%s:3478 stun:%s:3478\n' "$relay_host4" "$relay_host6"
    printf 'turn_urls ='
    for name in "$relay_host4" "$relay_host6"; do
        for port in "${RELAY_PORTS[@]}"; do
            printf ' turn:%s:%s?transport=udp' "$name" "$port"
        done
    done
    printf '\n'
    printf '# systemd hands the secrets over here (LoadCredential=).\n'
    printf 'turn_secret_file = %s\n' "$CREDENTIAL_PATH"
    printf '# The WebSocket relay behind the same name (Caddy, /v1/relay).\n'
    printf 'relay_url = wss://%s/v1/relay\n' "$rv_host"
    printf 'relay_secret_file = %s\n' "$RELAY_CREDENTIAL_PATH"
} > "${work_dir}/rendezvous.conf"
# The configurations are checked with the programs' own readers, the
# secrets taken from where they are now rather than where systemd will put
# them. A secret not made yet (a dry run before the first real one) is
# stood in for by a random one in the work directory, so the dry run
# catches what the real run would.
check_secret() {
    # $1: the real secret file; $2: a name for the stand-in. Prints the file to read.
    if [[ -s "$1" ]]; then
        printf '%s\n' "$1"
    else
        python3 -c 'import secrets; print(secrets.token_hex(32))' > "${work_dir}/$2"
        printf '%s\n' "${work_dir}/$2"
    fi
}
if [[ -f "${CODE_DIR}/nereus_rendezvous/config.py" ]]; then
    turn_check="$(check_secret "$SECRET_FILE" turn-secret.standin)"
    relay_check="$(check_secret "$RELAY_SECRET_FILE" relay-secret.standin)"
    sed -e "s|^turn_secret_file = .*|turn_secret_file = ${turn_check}|" \
        -e "s|^relay_secret_file = .*|relay_secret_file = ${relay_check}|" \
        "${work_dir}/rendezvous.conf" > "${work_dir}/check.conf"
    if ! PYTHONPATH="$CODE_DIR" PYTHONDONTWRITEBYTECODE=1 python3 -c \
            'import sys; from nereus_rendezvous import config; config.load(sys.argv[1])' "${work_dir}/check.conf"; then
        die "the service refuses the configuration above; ${SERVICE_CONF} was left unchanged"
    fi
    standin=""
    [[ -s "$SECRET_FILE" && -s "$RELAY_SECRET_FILE" ]] || standin=" (a stand-in for a secret not made yet)"
    note "${SERVICE_CONF}: accepted by the service's own reader${standin}"
fi
result="$(install_file "${work_dir}/rendezvous.conf" "$SERVICE_CONF" 644 root:root)"
if [[ "$result" == changed ]]; then
    mark_changed service_changed
fi
note "${SERVICE_CONF}${would}: ${result}"
{
    printf '# The NereusSDR WebSocket relay on this server. Written by\n'
    printf '# rendezvous/deploy/setup-server.sh; every key not here has the\n'
    printf "# default in rendezvous/server/relay.conf.sample.\n\n"
    printf '[relay]\n'
    printf '# Caddy reaches it here (reverse_proxy unix//...); only its group may.\n'
    printf 'socket = %s\n' "$WS_RELAY_SOCKET"
    printf 'socket_mode = %s\n' "$WS_RELAY_SOCKET_MODE"
    printf 'socket_group = %s\n' "$WS_RELAY_SOCKET_GROUP"
    printf '# systemd hands the secret over here (LoadCredential=).\n'
    printf 'relay_secret_file = %s\n' "$RELAY_OWN_CREDENTIAL_PATH"
    printf '\n[limits]\n'
    printf '# RV_WS_RELAY_SLOTS.\n'
    printf 'slots = %s\n' "$ws_relay_slots"
} > "${work_dir}/relay.conf"
if [[ -f "${CODE_DIR}/nereus_relay/config.py" ]]; then
    relay_check="$(check_secret "$RELAY_SECRET_FILE" relay-secret.standin)"
    sed "s|^relay_secret_file = .*|relay_secret_file = ${relay_check}|" "${work_dir}/relay.conf" > "${work_dir}/relay-check.conf"
    if ! PYTHONPATH="$CODE_DIR" PYTHONDONTWRITEBYTECODE=1 python3 -c \
            'import sys; from nereus_relay import config; config.load(sys.argv[1])' "${work_dir}/relay-check.conf"; then
        die "the WebSocket relay refuses the configuration above; ${RELAY_CONF} was left unchanged"
    fi
    standin=""
    [[ -s "$RELAY_SECRET_FILE" ]] || standin=" (a stand-in for a secret not made yet)"
    note "${RELAY_CONF}: accepted by the relay's own reader${standin}"
fi
result="$(install_file "${work_dir}/relay.conf" "$RELAY_CONF" 644 root:root)"
if [[ "$result" == changed ]]; then
    mark_changed relay_changed
fi
note "${RELAY_CONF}${would}: ${result}"

# ---------------------------------------------------------------------------
log "7/10 ${CADDYFILE_DEST}"

if [[ "$manage_caddy" == no ]]; then
    note "RV_MANAGE_CADDY=no: Caddy and its configuration are left alone (add the ${rv_host} site yourself; see the README)"
else
    sed "s|^${CADDYFILE_SITE} {\$|${rv_host} {|" "$repo_caddyfile" > "${work_dir}/Caddyfile"
    if command -v caddy >/dev/null 2>&1; then
        if ! out="$(caddy validate --config "${work_dir}/Caddyfile" --adapter caddyfile 2>&1)"; then
            printf '%s\n' "$out" >&2
            die "the Caddyfile failed caddy validate (above); ${CADDYFILE_DEST} was left unchanged"
        fi
        note "caddy validate: Valid configuration"
    else
        note "caddy is not installed yet: the Caddyfile will be validated once it is"
    fi
    if ! (( dry_run )); then
        install -d -m 0755 -o root -g root "$(dirname "$CADDYFILE_DEST")"
    fi
    result="$(install_file "${work_dir}/Caddyfile" "$CADDYFILE_DEST" 644 root:root)"
    if [[ "$result" == changed ]]; then
        mark_changed caddyfile_changed
    fi
    note "${CADDYFILE_DEST}${would}: ${result}"
fi

# ---------------------------------------------------------------------------
log "8/10 Units, drop-ins and ${CODE_DIR}"

printf 'RV_DATA_USE_INTERFACE=%s\nRV_TRANSFER_GB_PER_MONTH=%s\n' "$data_use_iface" "$transfer_gb" > "${work_dir}/data-use.conf"
{
    printf '# Written by rendezvous/deploy/setup-server.sh from the server'"'"'s memory\n'
    printf '# (%s MiB; the formula is in the script and the rendezvous document,\n' "$memory_mb"
    printf '# section 9.1).\n[Service]\nMemoryMax=%sM\n' "$service_limit_mb"
} > "${work_dir}/memory.conf"
{
    printf '# Written by rendezvous/deploy/setup-server.sh from the WebSocket relay'"'"'s\n'
    printf '# slots (%s; %s MiB and %s MiB a slot, as the script and the rendezvous\n' \
        "$ws_relay_slots" "$WS_RELAY_MEMORY_BASE_MB" "$WS_RELAY_MEMORY_PER_SLOT_MB"
    printf '# document, section 12.5, explain).\n[Service]\nMemoryMax=%sM\n' "$ws_relay_limit_mb"
} > "${work_dir}/relay-memory.conf"
sed "s|@GOMEMLIMIT@|${caddy_limit_mb}MiB|" "$repo_caddy_dropin" > "${work_dir}/caddy-override.conf"
if ! (( dry_run )); then
    install -d -m 0755 -o root -g root "$(dirname "$DROPIN_DEST")" "$(dirname "$MEMORY_DROPIN_DEST")" \
        "$(dirname "$RELAY_MEMORY_DROPIN_DEST")" "$(dirname "$USE_SCRIPT_DEST")"
    if [[ "$manage_caddy" == yes ]]; then
        install -d -m 0755 -o root -g root "$(dirname "$CADDY_DROPIN_DEST")"
    fi
fi
# $1 source, $2 destination, $3 mode, $4..: flags to set when it changed.
place() {
    local src="$1" dest="$2" mode="$3" result
    shift 3
    result="$(install_file "$src" "$dest" "$mode" root:root)"
    note "${dest}${would}: ${result}"
    if [[ "$result" == changed && $# -gt 0 ]]; then
        mark_changed "$@"
    fi
}
place "$repo_unit" "$UNIT_DEST" 644 service_changed units_changed
place "${work_dir}/memory.conf" "$MEMORY_DROPIN_DEST" 644 service_changed units_changed
place "$repo_relay_unit" "$RELAY_UNIT_DEST" 644 relay_changed units_changed
place "${work_dir}/relay-memory.conf" "$RELAY_MEMORY_DROPIN_DEST" 644 relay_changed units_changed
place "$repo_dropin" "$DROPIN_DEST" 644 coturn_changed units_changed
if [[ "$manage_caddy" == yes ]]; then
    place "${work_dir}/caddy-override.conf" "$CADDY_DROPIN_DEST" 644 caddy_changed units_changed
fi
place "$repo_use_service" "$USE_SERVICE_DEST" 644 units_changed
place "$repo_use_timer" "$USE_TIMER_DEST" 644 units_changed
place "$repo_use_script" "$USE_SCRIPT_DEST" 755
place "${work_dir}/data-use.conf" "$USE_CONF" 644
# The kernel setting of deploy/sysctl.conf (tcp_notsent_lowat, rendezvous
# document section 12.5), applied now as well as at every boot. To undo it,
# the lines at the end of the file.
sysctl_changed=0
place "$repo_sysctl" "$SYSCTL_DEST" 644 sysctl_changed
if (( sysctl_changed )) && ! (( dry_run )); then
    if (( have_systemd )) && sysctl -q --load "$SYSCTL_DEST" 2>/dev/null; then
        note "${SYSCTL_DEST}: applied"
    else
        note "${SYSCTL_DEST}: could not be applied now (no systemd, or /proc/sys is read-only here); it applies at the next boot"
    fi
elif (( sysctl_changed )); then
    note "${SYSCTL_DEST} would be applied at once (sysctl --load)"
fi
if (( dry_run )); then
    if [[ -d "$CODE_DIR" ]]; then note "${CODE_DIR} present"; else note "would make ${CODE_DIR} for ${deploy_user}"; fi
else
    if [[ -L "$CODE_DIR" ]]; then
        die "${CODE_DIR} is a symlink; refusing to follow it as root"
    fi
    install -d -m 0755 "$CODE_DIR"
    chown -h "${deploy_user}:${deploy_user}" "$CODE_DIR"
    note "${CODE_DIR} belongs to ${deploy_user} (deploy.sh writes it)"
fi

# ---------------------------------------------------------------------------
log "9/10 Services"

# $1: unit; $2: 1 when one of its files changed. Prints what happens (or
# would happen): start a stopped unit, restart a running one only after a
# change, else leave it running.
plan_for() {
    if (( have_systemd )) && systemctl is-active --quiet "$1"; then
        if (( $2 )); then echo restart; else echo keep; fi
    else
        echo start
    fi
}
# $1: unit; $2: start, restart, reload or keep.
apply_plan() {
    case "$2" in
        keep) note "$1 is running and none of its files changed: left as it is" ;;
        start | restart | reload)
            systemctl "$2" "$1" || die "$1 did not ${2}; see: journalctl -u $1 -n 50 --no-pager"
            sleep 1
            systemctl is-active --quiet "$1" || die "$1 is not active; see: journalctl -u $1 -n 50 --no-pager"
            note "$1: ${2} done, active" ;;
    esac
}
coturn_plan="$(plan_for coturn "$coturn_changed")"
service_plan="$(plan_for "$UNIT_NAME" "$service_changed")"
relay_plan="$(plan_for "$RELAY_UNIT_NAME" "$relay_changed")"
caddy_plan=keep
if [[ "$manage_caddy" == yes ]]; then
    caddy_plan="$(plan_for caddy "$caddy_changed")"
    # A Caddyfile change alone is a reload, which keeps open WebSockets
    # (stream_close_delay); only a change to its unit restarts Caddy.
    if [[ "$caddy_plan" == keep ]] && (( caddyfile_changed )); then
        caddy_plan=reload
    fi
fi
code_present=0
if [[ -f "${CODE_DIR}/nereus_rendezvous/__main__.py" ]]; then
    code_present=1
fi
relay_code_present=0
if [[ -f "${CODE_DIR}/nereus_relay/__main__.py" ]]; then
    relay_code_present=1
fi
if (( dry_run )); then
    if [[ "$manage_caddy" == yes ]]; then
        case "$caddy_plan" in
            restart) note "caddy would be restarted (its unit changed); every WebSocket is dropped and the Cores connect again" ;;
            reload) note "caddy would reload its configuration (open WebSockets are kept)" ;;
            start) note "caddy would be started" ;;
            keep) note "caddy would be left running (none of its files changed)" ;;
        esac
    fi
    case "$coturn_plan" in
        restart) note "coturn would be restarted (its files changed); a restart drops every live relay" ;;
        start) note "coturn would be started" ;;
        keep) note "coturn would be left running (none of its files changed)" ;;
    esac
    if (( code_present )); then
        case "$service_plan" in
            restart) note "${UNIT_NAME} would be restarted (its files changed); every Core connects again" ;;
            start) note "${UNIT_NAME} would be started" ;;
            keep) note "${UNIT_NAME} would be left running (none of its files changed)" ;;
        esac
    fi
    if (( relay_code_present )); then
        case "$relay_plan" in
            restart) note "${RELAY_UNIT_NAME} would be restarted (its files changed); every relay leg joins again" ;;
            start) note "${RELAY_UNIT_NAME} would be started" ;;
            keep) note "${RELAY_UNIT_NAME} would be left running (none of its files changed)" ;;
        esac
    fi
    note "dry run: no service is started, stopped or enabled"
elif (( no_start )); then
    note "--no-start: no service is started, stopped or enabled"
    pending="$(pending_list)"
    if [[ -n "$pending" ]]; then
        note "left in ${PENDING_FILE} for the next run without --no-start: ${pending}"
    fi
else
    if (( units_changed )); then
        systemctl daemon-reload
        mark_done units_changed
    fi
    if [[ "$manage_caddy" == yes ]]; then
        enable_units caddy
    fi
    # The relay's unit takes the caddy group (SupplementaryGroups=), which
    # Caddy's package makes; without it systemd cannot start the relay.
    getent group "$WS_RELAY_SOCKET_GROUP" >/dev/null \
        || die "there is no ${WS_RELAY_SOCKET_GROUP} group (Caddy's package makes it); the WebSocket relay needs it for its socket"
    enable_units coturn "$UNIT_NAME" "$RELAY_UNIT_NAME" nereus-data-use.timer
    # Each pending flag comes off once its unit is done; one that stops
    # the script stays for the next run. Without code the service (or the
    # relay) cannot be running, so it has nothing to restart.
    if [[ "$manage_caddy" == yes ]]; then
        apply_plan caddy "$caddy_plan"
    fi
    mark_done caddy_changed caddyfile_changed
    apply_plan coturn "$coturn_plan"
    mark_done coturn_changed
    if (( code_present )); then
        apply_plan "$UNIT_NAME" "$service_plan"
    else
        note "${CODE_DIR} holds no code yet: run rendezvous/deploy.sh, then: systemctl restart ${UNIT_NAME}"
    fi
    mark_done service_changed
    if (( relay_code_present )); then
        apply_plan "$RELAY_UNIT_NAME" "$relay_plan"
    else
        note "${CODE_DIR} holds no relay code yet: run rendezvous/deploy.sh, then: systemctl restart ${RELAY_UNIT_NAME}"
    fi
    mark_done relay_changed
    systemctl start nereus-data-use.timer || die "nereus-data-use.timer did not start"
    note "nereus-data-use.timer is active (a journal line a day: journalctl -u nereus-data-use)"
fi

if ! (( dry_run || no_start )); then
    # coturn: UDP 3478 and 443 on the two public addresses, and no TCP port.
    # The service: TCP 8710 on loopback only. The WebSocket relay: its Unix
    # socket, mode 0660, group caddy, and no IP socket. Caddy: no UDP
    # (HTTP/3 off).
    held="$(socket_holders 3478 443 "$SERVICE_PORT")"
    note "sockets on UDP 3478, UDP 443 and TCP ${SERVICE_PORT}:"
    printf '%s\n' "$held" | sed 's/^/      /'
    every="$(socket_holders)"
    if [[ -n "$(awk '$1 == "tcp" && $5 == "turnserver"' <<<"$every")" ]]; then
        die "coturn holds a TCP port; it must use UDP only"
    fi
    if [[ -n "$(awk '$1 == "udp" && $5 == "caddy"' <<<"$every")" ]]; then
        die "caddy holds a UDP port; HTTP/3 must stay off (the servers block in ${CADDYFILE_DEST})"
    fi
    for port in "${RELAY_PORTS[@]}"; do
        for addr in "$public4" "$public6"; do
            [[ -n "$(awk -v p="$port" -v a="$addr" '$1 == "udp" && $2 == p && $3 == a && $5 == "turnserver"' <<<"$held")" ]] \
                || die "coturn does not hold UDP ${port} on ${addr}"
        done
    done
    if [[ -n "$(awk -v p="$SERVICE_PORT" '$1 == "tcp" && $2 == p && $3 != "127.0.0.1" && $3 != "::1"' <<<"$held")" ]]; then
        die "something listens on TCP ${SERVICE_PORT} beyond loopback"
    fi
    if (( relay_code_present )); then
        relay_socket="$(stat -c '%F %a %G' "$WS_RELAY_SOCKET" 2>/dev/null || true)"
        [[ "$relay_socket" == "socket ${WS_RELAY_SOCKET_MODE#0} ${WS_RELAY_SOCKET_GROUP}" ]] \
            || die "the WebSocket relay's socket ${WS_RELAY_SOCKET} is not a socket of mode ${WS_RELAY_SOCKET_MODE} and group ${WS_RELAY_SOCKET_GROUP} (found: ${relay_socket:-nothing})"
        if [[ -n "$(awk '$1 == "tcp" || $1 == "udp"' <<<"$(socket_holders)" | awk '$5 == "python3" && $2 != '"$SERVICE_PORT"'')" ]]; then
            die "a Python program holds an IP port other than the service's"
        fi
        note "the WebSocket relay listens on ${WS_RELAY_SOCKET} (socket, mode ${WS_RELAY_SOCKET_MODE}, group ${WS_RELAY_SOCKET_GROUP}) and on no IP port"
    fi
    note "coturn holds UDP 3478 and 443 on both addresses and no TCP port; Caddy holds no UDP port"
fi

# ---------------------------------------------------------------------------
log "10/10 Summary"

if ! (( dry_run )); then
    note "coturn:        $(dpkg-query -W -f='${Version}' coturn 2>/dev/null || echo unknown)"
    note "websockets:    $(dpkg-query -W -f='${Version}' python3-websockets 2>/dev/null || echo unknown)"
    if [[ "$manage_caddy" == yes ]]; then
        note "caddy:         $(caddy version 2>/dev/null | cut -d' ' -f1 || echo unknown)"
    fi
fi
note "service name:  ${rv_host} (Caddy forwards it to 127.0.0.1:${SERVICE_PORT} and [::1]:${SERVICE_PORT})"
note "ws relay:      wss://${rv_host}/v1/relay (Caddy forwards it to the Unix socket ${WS_RELAY_SOCKET}); ${ws_relay_slots} sessions, MemoryMax ${ws_relay_limit_mb} MiB"
note "relay names:   ${relay_host4} (A ${public4}), ${relay_host6} (AAAA ${public6})"
note "relay ports:   UDP 3478 and 443; relays on UDP 61000 to 65535"
note "relay size:    ${total_quota} slots, ${user_quota} per station id, bps-capacity ${bps_capacity} bytes/s"
note "memory:        Caddy GOMEMLIMIT ${caddy_limit_mb} MiB, the service MemoryMax ${service_limit_mb} MiB (of ${memory_mb})"
note "data use:      ${data_use_iface}, report threshold ${transfer_gb} GB a month"
note "deploy:        ${deploy_user} writes ${CODE_DIR}"
note "secrets:       ${SECRET_FILE} and ${RELAY_SECRET_FILE} (root only; never printed)"
note ""
note "Caddy obtains the certificate for ${rv_host} from Let's Encrypt once its A and"
note "AAAA records point at this server and TCP 80 and 443 are open."
