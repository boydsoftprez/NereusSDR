#!/usr/bin/env bash
#
# setup-server.sh: prepare the NereusSDR web server.
#
# Run it as root on the server itself. It is idempotent: every step looks at
# the current state first and skips work that is already done, so it is safe
# to run again, for example after changing the Caddyfile.
#
# Usage:
#   bash setup-server.sh <Caddyfile> '<SSH public key line>'
#
#   <Caddyfile>            path to the Caddyfile to install
#                          (a copy of website/deploy/Caddyfile)
#   <SSH public key line>  the whole public key line for the deploy account,
#                          for example 'ssh-ed25519 AAAA... comment'
#
# Typical run, from the repository root on the maintainer's Mac (see
# website/README.md):
#   scp website/deploy/Caddyfile website/deploy/setup-server.sh root@<droplet>:/root/
#   ssh root@<droplet> "bash /root/setup-server.sh /root/Caddyfile '$(cat ~/.ssh/nereus_vps_ed25519.pub)'"
#
# Steps:
#   1. Refuse to run unless this is root on Ubuntu.
#   2. Install Caddy from the official Caddy apt repository, and rsync.
#   3. Create the deploy account nereusweb. Its authorized_keys file ends up
#      holding exactly the given key line.
#   4. Create the web root /var/www/nereussdr, owned by nereusweb, with a
#      placeholder index.html when it has none.
#   5. Validate the Caddyfile with caddy validate, and only when that passes
#      install it as /etc/caddy/Caddyfile, keeping a timestamped backup of
#      the previous one. A Caddyfile that fails validation is never
#      installed and the script exits non-zero.
#   6. Enable Caddy, reload it (or start it), show its TCP listeners on 80
#      and 443, and fail if Caddy holds any UDP port. HTTP/3 must stay off
#      because UDP 443 on this host is used by another service.
#   7. Print a summary.
#
# Inside the deploy account's own directories (its home, ~/.ssh and the web
# root) the work is done as that account, never as root; see as_deploy_user.
#
# It never touches the firewall, the SSH daemon configuration, or any other
# service or account on the host.

set -euo pipefail
umask 022

readonly DEPLOY_USER="nereusweb"
readonly DEPLOY_HOME="/home/nereusweb"
readonly WEB_ROOT="/var/www/nereussdr"
readonly CADDYFILE_DEST="/etc/caddy/Caddyfile"

# The official Caddy apt repository, as given in the Caddy install docs
# (https://caddyserver.com/docs/install, "Debian, Ubuntu, Raspbian").
readonly CADDY_PREREQS=(debian-keyring debian-archive-keyring apt-transport-https curl)
readonly CADDY_KEY_URL="https://dl.cloudsmith.io/public/caddy/stable/gpg.key"
readonly CADDY_LIST_URL="https://dl.cloudsmith.io/public/caddy/stable/debian.deb.txt"
readonly CADDY_KEYRING="/usr/share/keyrings/caddy-stable-archive-keyring.gpg"
readonly CADDY_SOURCES="/etc/apt/sources.list.d/caddy-stable.list"

export DEBIAN_FRONTEND=noninteractive
# needrestart (part of Ubuntu server) can restart running services after
# apt installs packages. List mode only reports, so no other service on
# this host is restarted by this script.
export NEEDRESTART_MODE=l

log()  { printf '\n==> %s\n' "$*"; }
note() { printf '    %s\n' "$*"; }
die()  { printf 'setup-server.sh: error: %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<'EOF'
Usage: bash setup-server.sh <Caddyfile> '<SSH public key line>'

  <Caddyfile>            path to the Caddyfile to install
  <SSH public key line>  the whole deploy key line, e.g. 'ssh-ed25519 AAAA... comment'

Run as root on the web server. Safe to run again.
EOF
}

# apt-get that waits up to 5 minutes for another apt or dpkg run (such as
# unattended-upgrades on a freshly booted server) to release its lock.
apt_get() {
    apt-get -o DPkg::Lock::Timeout=300 "$@"
}

# True when the Debian package named $1 is installed.
is_installed() {
    local status
    # shellcheck disable=SC2016  # ${Status} is a dpkg-query field, not a shell variable
    status="$(dpkg-query -W -f='${Status}' "$1" 2>/dev/null || true)"
    [[ "$status" == "install ok installed" ]]
}

# Rule: root does no file operation (create, write, compare, rename, remove,
# chown, chmod) on a path inside a directory the deploy account can write:
# its home, ~/.ssh and the web root. The account could swap such a path for
# a symlink between root's check and root's write, and so point root at any
# file on the host. That work runs as the account through as_deploy_user,
# where a symlink reaches only files the account could change anyway. Root
# keeps the rest: apt, adduser, the web root entry itself (its parent
# /var/www is root's), the Caddyfile and the service.
#
# as_deploy_user runs a command as the deploy account. runuser keeps the
# caller's working directory, which the account may not be able to enter
# (/root, for example), so the command starts in / instead.
as_deploy_user() {
    (cd / && runuser -u "$DEPLOY_USER" -- "$@")
}

# Root's own temporary files and directories, removed on exit. A path that
# has since been moved into place no longer exists, so removing it is a
# no-op. None of them may lie inside the deploy account's directories (see
# the rule above).
tmp_paths=()
cleanup() {
    local p
    for p in ${tmp_paths[@]+"${tmp_paths[@]}"}; do
        rm -rf -- "$p"
    done
}
trap cleanup EXIT

if [[ $# -ne 2 ]]; then
    usage >&2
    exit 2
fi
caddyfile_src="$1"
pubkey="$2"

# ---------------------------------------------------------------------------
log "1/7 Checking the host"

if [[ "$(id -u)" -ne 0 ]]; then
    die "run this as root"
fi
# shellcheck source=/dev/null  # /etc/os-release exists only on the server
os_id="$(. /etc/os-release 2>/dev/null && printf '%s' "${ID:-}")" || true
if [[ "$os_id" != "ubuntu" ]]; then
    die "this script supports Ubuntu only (found: ${os_id:-unknown})"
fi
command -v runuser >/dev/null 2>&1 || die "runuser (util-linux) is not installed"
# shellcheck source=/dev/null
note "root on $(. /etc/os-release && printf '%s' "${PRETTY_NAME:-Ubuntu}")"

# Check both inputs before changing anything.
if [[ ! -f "$caddyfile_src" || ! -r "$caddyfile_src" ]]; then
    die "Caddyfile not found or not readable: ${caddyfile_src}"
fi
case "$pubkey" in
    *$'\n'* | *$'\r'*)
        die "the SSH public key must be a single line"
        ;;
    ssh-* | ecdsa-* | sk-*)
        ;;
    *)
        die "the second argument must be an SSH public key line, such as 'ssh-ed25519 AAAA... comment'"
        ;;
esac
work_dir="$(mktemp -d)"
tmp_paths+=("$work_dir")
printf '%s\n' "$pubkey" > "${work_dir}/deploy_key.pub"
if ! key_info="$(ssh-keygen -l -f "${work_dir}/deploy_key.pub" 2>/dev/null)"; then
    die "ssh-keygen does not accept the second argument as an SSH public key"
fi
note "deploy key: ${key_info}"

# ---------------------------------------------------------------------------
log "2/7 Caddy (official Caddy apt repository) and rsync"

if [[ -s "$CADDY_KEYRING" && -s "$CADDY_SOURCES" ]]; then
    note "Caddy apt repository already configured"
else
    note "adding the Caddy apt repository"
    apt_get update
    # The packages the Caddy docs install first, plus gnupg for gpg --dearmor
    # (already present on stock Ubuntu). Only missing ones are installed.
    missing=()
    for pkg in "${CADDY_PREREQS[@]}" gnupg; do
        is_installed "$pkg" || missing+=("$pkg")
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        apt_get install -y "${missing[@]}"
    fi
    # Same URLs, curl flags, paths and o+r permissions as the Caddy docs.
    # The downloads land in a temp dir first and are then moved into place,
    # so an interrupted run never leaves a half-written key or list behind.
    curl -1sLf "$CADDY_KEY_URL" | gpg --dearmor -o "${work_dir}/caddy-stable-archive-keyring.gpg"
    curl -1sLf "$CADDY_LIST_URL" -o "${work_dir}/caddy-stable.list"
    install -m 0644 "${work_dir}/caddy-stable-archive-keyring.gpg" "$CADDY_KEYRING"
    install -m 0644 "${work_dir}/caddy-stable.list" "$CADDY_SOURCES"
fi

missing=()
for pkg in caddy rsync; do
    is_installed "$pkg" || missing+=("$pkg")
done
if [[ ${#missing[@]} -eq 0 ]]; then
    note "caddy and rsync already installed"
else
    note "installing: ${missing[*]}"
    apt_get update
    # confold: an existing /etc/caddy/Caddyfile is never swapped for the
    # package's sample by dpkg; step 5 manages that file.
    apt_get install -y \
        -o Dpkg::Options::=--force-confdef \
        -o Dpkg::Options::=--force-confold \
        "${missing[@]}"
fi
command -v caddy >/dev/null 2>&1 || die "caddy is not on PATH after installation"

# ---------------------------------------------------------------------------
log "3/7 Deploy account ${DEPLOY_USER}"

if id -u "$DEPLOY_USER" >/dev/null 2>&1; then
    note "account ${DEPLOY_USER} already exists"
else
    adduser --disabled-password --gecos "" --home "$DEPLOY_HOME" --shell /bin/bash "$DEPLOY_USER"
    note "created account ${DEPLOY_USER}"
fi
actual_home="$(getent passwd "$DEPLOY_USER" | cut -d: -f6)"
if [[ "$actual_home" != "$DEPLOY_HOME" ]]; then
    die "account ${DEPLOY_USER} has home ${actual_home}, expected ${DEPLOY_HOME}; fix it by hand"
fi

ssh_dir="${DEPLOY_HOME}/.ssh"
auth_keys="${ssh_dir}/authorized_keys"
# Runs as the deploy account (see the rule above as_deploy_user): create
# ~/.ssh (700), write the key line, which arrives on stdin, to a temp file
# there (600), compare it with authorized_keys, and rename it into place, so
# the mode always applies. Prints "same" or "set".
install_key_script="$(cat <<'EOF'
set -euo pipefail
umask 077
ssh_dir="$1"
auth_keys="$2"
mkdir -p "$ssh_dir"
chmod 700 "$ssh_dir"
new_keys="$(mktemp "${ssh_dir}/.authorized_keys.XXXXXX")"
trap 'rm -f -- "$new_keys"' EXIT
cat > "$new_keys"
chmod 600 "$new_keys"
if [[ -f "$auth_keys" ]] && cmp -s "$new_keys" "$auth_keys"; then
    echo same
else
    echo set
fi
mv -f -- "$new_keys" "$auth_keys"
EOF
)"
if ! key_result="$(printf '%s\n' "$pubkey" \
        | as_deploy_user bash -c "$install_key_script" "as-${DEPLOY_USER}" "$ssh_dir" "$auth_keys")"; then
    die "could not write ${auth_keys} as ${DEPLOY_USER} (see above); ${DEPLOY_HOME} and ${ssh_dir} must belong to ${DEPLOY_USER}"
fi
case "$key_result" in
    same) note "authorized_keys already holds exactly this key" ;;
    set) note "authorized_keys set to exactly this key" ;;
    *) die "unexpected answer from the authorized_keys step as ${DEPLOY_USER}" ;;
esac

# ---------------------------------------------------------------------------
log "4/7 Web root ${WEB_ROOT}"

# The web root entry itself sits in /var/www, which the deploy account cannot
# change, so root creates it and sets its owner and mode. The symlink check
# and chown -h keep root from following a link to any other path.
if [[ -L "$WEB_ROOT" ]]; then
    die "${WEB_ROOT} is a symlink; refusing to follow it as root"
fi
mkdir -p "$WEB_ROOT"
chown -h "${DEPLOY_USER}:${DEPLOY_USER}" "$WEB_ROOT"
chmod 755 "$WEB_ROOT"
# Runs as the deploy account (see the rule above as_deploy_user): when the web
# root has no index.html, write a placeholder to a temp file there and rename
# it into place. Prints "present" or "written".
placeholder_script="$(cat <<'EOF'
set -euo pipefail
umask 022
index="$1/index.html"
if [[ -e "$index" || -L "$index" ]]; then
    echo present
    exit 0
fi
placeholder="$(mktemp "$1/.index.html.XXXXXX")"
trap 'rm -f -- "$placeholder"' EXIT
printf '%s\n' '<!doctype html><html lang="en"><head><meta charset="utf-8"><title>NereusSDR</title></head><body><p>NereusSDR website coming soon.</p></body></html>' > "$placeholder"
chmod 644 "$placeholder"
mv -f -- "$placeholder" "$index"
echo written
EOF
)"
if ! index_result="$(as_deploy_user bash -c "$placeholder_script" "as-${DEPLOY_USER}" "$WEB_ROOT")"; then
    die "could not check or write ${WEB_ROOT}/index.html as ${DEPLOY_USER} (see above)"
fi
case "$index_result" in
    present) note "index.html present; left as it is" ;;
    written) note "wrote a placeholder index.html" ;;
    *) die "unexpected answer from the index.html step as ${DEPLOY_USER}" ;;
esac

# ---------------------------------------------------------------------------
log "5/7 Caddyfile"

mkdir -p "$(dirname "$CADDYFILE_DEST")"
staged="$(mktemp "$(dirname "$CADDYFILE_DEST")/.Caddyfile.new.XXXXXX")"
tmp_paths+=("$staged")
cp -- "$caddyfile_src" "$staged"
chmod 644 "$staged"
note "validating ${caddyfile_src}"
if ! caddy validate --config "$staged" --adapter caddyfile; then
    die "the Caddyfile failed validation; ${CADDYFILE_DEST} was left unchanged"
fi
backup=""
if [[ -f "$CADDYFILE_DEST" ]] && cmp -s "$staged" "$CADDYFILE_DEST"; then
    note "${CADDYFILE_DEST} is already this Caddyfile"
else
    if [[ -e "$CADDYFILE_DEST" ]]; then
        backup="${CADDYFILE_DEST}.bak-$(date +%Y%m%d-%H%M%S)"
        cp -p -- "$CADDYFILE_DEST" "$backup"
        note "previous Caddyfile kept as ${backup}"
    fi
    mv -f -- "$staged" "$CADDYFILE_DEST"
    note "installed ${CADDYFILE_DEST}"
fi

# ---------------------------------------------------------------------------
log "6/7 Caddy service"

journal_hint="see: journalctl -u caddy -n 50 --no-pager"
systemctl enable caddy
if systemctl is-active --quiet caddy; then
    note "caddy is running; reloading its configuration"
    systemctl reload caddy \
        || die "systemctl reload caddy failed; Caddy keeps serving its previous configuration (previous file: ${backup:-unchanged}); ${journal_hint}"
else
    note "starting caddy"
    systemctl start caddy || die "systemctl start caddy failed; ${journal_hint}"
fi
state="$(systemctl is-active caddy || true)"
note "systemctl is-active caddy: ${state}"
if [[ "$state" != "active" ]]; then
    die "caddy is not active; ${journal_hint}"
fi

note "TCP listeners on ports 80 and 443:"
ss -lntp '( sport = :80 or sport = :443 )'

udp_sockets="$(ss -lnup)"
if grep -q '"caddy"' <<<"$udp_sockets"; then
    grep '"caddy"' <<<"$udp_sockets" >&2 || true
    die "caddy is listening on UDP (above). HTTP/3 must stay off on this host, because UDP 443 is used by another service; check the servers block in ${CADDYFILE_DEST}"
fi
note "caddy holds no UDP sockets, so HTTP/3 is off"

# ---------------------------------------------------------------------------
log "7/7 Summary"

note "Caddy version: $(caddy version)"
note "Web root:      ${WEB_ROOT} (owner ${DEPLOY_USER}, mode 755)"
note "Deploy user:   ${DEPLOY_USER} (key in ${auth_keys})"
note "Caddyfile:     ${CADDYFILE_DEST}"
note ""
note "HTTPS certificates are only issued once DNS for both nereussdr.com and"
note "www.nereussdr.com points at this server. Until then Caddy keeps retrying"
note "on its own, and https:// requests fail."
