#!/usr/bin/env bash
#
# coturn-check.sh: the relay's configuration, in Docker, on Ubuntu 24.04's
# own coturn.
#
# A server container (ubuntu:24.04 with Caddy from Caddy's apt repository
# and Ubuntu's coturn and Python packages, as a dedicated server ends up) is
# set up by rendezvous/deploy/setup-server.sh (with
# --no-start, as the container has no systemd) and coturn is started the
# way the unit and its drop-in start it: as the turnserver account with
# CAP_NET_BIND_SERVICE only. A peer container on the same networks plays
# the client and the relayed peer. Three networks: a public one standing in
# for the internet (11.77.0.0/24 and the benchmarking prefix
# 2001:2:0:77::/64, neither of them denied), a private one (10.99.0.0/24
# and fd99::/64) and a documentation one (192.0.2.0/24 and
# 2001:db8:77::/64), which the relay must never reach, although it could.
#
# It checks:
#   - setup-server.sh when apt-get fails (policy-rc.d never left behind,
#     coturn disabled even when the install failed after unpacking);
#   - setup-server.sh (dry run, then real, then again: nothing changes),
#     the files it writes and their modes, and the default relay sizing;
#   - setup-server.sh where systemd runs (a stand-in systemctl that starts
#     the units' programs): a TCP 443 holder is allowed, a UDP 443 or 3478
#     holder refused; coturn is disabled right after it is installed;
#     step 9 starts Caddy, coturn, the service and the WebSocket relay,
#     then leaves them alone,
#     reloads Caddy after a Caddyfile change and restarts only after a unit
#     or configuration change, also when a --no-start run made the change
#     (the next real run does what it left pending, and a run after that
#     nothing), and checks the ports each one holds; a
#     failing systemctl enable and a failing file install stop it with a
#     message; the TURN secret's backups; the deploy account and its key;
#     the Caddyfile, Caddy's drop-in and the memory limits; the relay
#     secret, relay.conf, the relay's unit and its memory ceiling;
#   - coturn holds UDP 3478 and 443 on both addresses and no TCP port;
#   - STUN answers on both ports in both families, without credentials;
#   - turnutils_uclient: a valid allocation relays on both ports in both
#     families, an expired one is refused, a blocked peer never receives;
#   - every denied range, reachable ones included, in each written form
#     (turn_probe.py, for the error codes);
#   - no anonymous, wrongly signed or TCP relay allocation, no TCP listener;
#   - the per-user and total quotas (the relay's slots at a small test
#     value, 10);
#   - credentials minted by the running service verify with coturn, and the
#     relay grants it mints admit both legs on the running WebSocket relay
#     (each run from setup's configuration, secrets via the credential
#     paths);
#   - what coturn does when an allocation is refreshed after its credential
#     expired (the rendezvous document, section 8), with the real nonce
#     lifetime and with a short one;
#   - what coturn logs, and that it writes no log file.
#
# Usage: rendezvous/tests/coturn-check.sh      (needs Docker; about 2 minutes)

# "check && check || fail": fail exits, and runs exactly when a check fails.
# shellcheck disable=SC2015
set -euo pipefail

repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
readonly repo
readonly image="nereus-rv-server:24.04"
readonly tag="$$"
readonly pub="nereus-rv-pub-${tag}" priv="nereus-rv-priv-${tag}"
readonly server="nereus-rv-turn-${tag}" peer="nereus-rv-peer-${tag}"
readonly doc="nereus-rv-doc-${tag}"
readonly s4="11.77.0.10" s6="2001:2:0:77::10"
readonly p4="11.77.0.20" p6="2001:2:0:77::20"
readonly q4="10.99.0.20" q6="fd99::20"
readonly d4="192.0.2.20" d6="2001:db8:77::20"

passed=0
pass() { passed=$((passed + 1)); printf 'ok %d - %s\n' "$passed" "$*"; }
fail() { printf 'not ok - %s\n' "$*" >&2; exit 1; }

cleanup() {
    docker rm -f "$server" "$peer" >/dev/null 2>&1 || true
    docker network rm "$pub" "$priv" "$doc" >/dev/null 2>&1 || true
}
trap cleanup EXIT

command -v docker >/dev/null 2>&1 || fail "docker is not installed"

echo "# building ${image} (ubuntu:24.04 with Caddy from Caddy's apt repository, Ubuntu's coturn, Python packages and rsync)"
docker build -q -t "$image" - >/dev/null <<'EOF'
FROM ubuntu:24.04
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      debian-keyring debian-archive-keyring apt-transport-https curl gnupg ca-certificates \
 && curl -1sLf https://dl.cloudsmith.io/public/caddy/stable/gpg.key \
      | gpg --dearmor -o /usr/share/keyrings/caddy-stable-archive-keyring.gpg \
 && curl -1sLf https://dl.cloudsmith.io/public/caddy/stable/debian.deb.txt \
      -o /etc/apt/sources.list.d/caddy-stable.list \
 && apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      caddy coturn python3 python3-websockets python3-cryptography python3-pytest rsync \
 && rm -rf /var/lib/apt/lists/*
EOF
echo "# coturn $(docker run --rm "$image" dpkg-query -W -f='${Version}' coturn), $(docker run --rm "$image" turnserver --version 2>/dev/null | head -1)"

docker network create --ipv6 --subnet 11.77.0.0/24 --subnet 2001:2:0:77::/64 "$pub" >/dev/null
docker network create --ipv6 --subnet 10.99.0.0/24 --subnet fd99::/64 "$priv" >/dev/null
docker network create --ipv6 --subnet 192.0.2.0/24 --subnet 2001:db8:77::/64 "$doc" >/dev/null
# --init reaps the programs the check stops. SYS_PTRACE: root on a real
# server reads every process's sockets (setup's port checks name their
# owners); root in a container needs it granted.
docker run -d --init --name "$server" --network "$pub" --ip "$s4" --ip6 "$s6" --cap-add SYS_PTRACE \
    -v "${repo}:/repo:ro" "$image" sleep infinity >/dev/null
docker network connect --ip 10.99.0.10 --ip6 fd99::10 "$priv" "$server"
docker network connect --ip 192.0.2.10 --ip6 2001:db8:77::10 "$doc" "$server"
docker run -d --name "$peer" --network "$pub" --ip "$p4" --ip6 "$p6" \
    -v "${repo}:/repo:ro" "$image" sleep infinity >/dev/null
docker network connect --ip "$q4" --ip6 "$q6" "$priv" "$peer"
docker network connect --ip "$d4" --ip6 "$d6" "$doc" "$peer"

sx() { docker exec -i "$server" bash -s -- "$@"; }
px() { docker exec -i "$peer" bash -s -- "$@"; }
probe() { docker exec "$peer" python3 /repo/rendezvous/tests/turn_probe.py "$@"; }
# Test-only settings for setup-server.sh: the test addresses stand in for
# public ones, and the relay has 10 slots so the total quota can be reached.
# RV_MEMORY_MB fixes the memory the limits are worked out from.
readonly setup_env="RV_PUBLIC_IPV4=${s4} RV_PUBLIC_IPV6=${s6} RV_ALLOW_DOCUMENTATION_ADDRESSES=1 RV_RELAY_SLOTS=10 RV_MEMORY_MB=1024"

# --------------------------------------------------------------- setup
# A deploy key made for this run only; setup-server.sh makes the account.
work="$(mktemp -d "${TMPDIR:-/tmp}/nereus-coturn-check.XXXXXX")"
ssh-keygen -q -t ed25519 -N "" -C coturn-check -f "${work}/deploy"
docker cp "${work}/deploy.pub" "${server}:/run/deploy.pub"
rm -rf -- "$work"
sx <<'EOS'
set -euo pipefail
# setup-server.sh with RV_DEPLOY_KEY, which holds spaces, so it cannot ride
# in the settings string the calls below pass through env.
cat > /usr/local/bin/rv-setup <<'SH'
#!/bin/bash
RV_DEPLOY_KEY="$(cat /run/deploy.pub)"
export RV_DEPLOY_KEY
exec bash /repo/rendezvous/deploy/setup-server.sh "$@"
SH
chmod 755 /usr/local/bin/rv-setup
# The packaged Caddyfile, to show a dry run leaves it alone.
sha256sum /etc/caddy/Caddyfile > /run/caddyfile.sum
# Docker's ubuntu image ships a policy-rc.d of its own; a server has none.
rm -f /usr/sbin/policy-rc.d
# A syslog socket, as journald provides on the server, so coturn's log
# lines can be read.
cat > /usr/local/bin/syslog-capture <<'PY'
import os, socket, sys
try:
    os.unlink("/dev/log")
except FileNotFoundError:
    pass
s = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
s.bind("/dev/log")
os.chmod("/dev/log", 0o666)
with open("/run/syslog.txt", "ab", buffering=0) as out:
    while True:
        out.write(s.recv(65536).rstrip(b"\n") + b"\n")
PY
setsid python3 /usr/local/bin/syslog-capture </dev/null >/dev/null 2>&1 &
EOS

# ------------------------------------------------------ apt-get failures
# Stand-ins: every package reads as missing, and apt-get fails at update
# (then at install). Each records whether policy-rc.d was in place while it
# ran, so the check knows the failure really happened inside the window.
for failing in update install; do
    out="$(sx "$failing" "$setup_env" <<'EOS' 2>&1
set -uo pipefail
failing="$1"
mkdir -p /run/apt-standin
printf '#!/bin/sh
exit 1
' > /run/apt-standin/dpkg-query
cat > /run/apt-standin/apt-get <<'SH'
#!/bin/sh
test -e /usr/sbin/policy-rc.d && echo present >> /run/apt-standin/policy-seen
for arg in "$@"; do
    [ "$arg" = "$(cat /run/apt-standin/failing)" ] && exit 100
done
exit 0
SH
echo "$failing" > /run/apt-standin/failing
chmod 755 /run/apt-standin/dpkg-query /run/apt-standin/apt-get
# shellcheck disable=SC2086
PATH=/run/apt-standin:$PATH env $2 rv-setup --no-start
echo "exit $?"
test -e /usr/sbin/policy-rc.d && echo "policy-rc.d left behind"
cat /run/apt-standin/policy-seen 2>/dev/null
rm -rf /run/apt-standin
EOS
)"
    if ! grep -q "installing the packages failed" <<<"$out" || ! grep -qx 'exit 1' <<<"$out" \
            || ! grep -qx present <<<"$out" || grep -q 'left behind' <<<"$out"; then
        printf '%s
' "$out" >&2
        fail "setup-server.sh with a failing apt-get ${failing}"
    fi
done
sx <<'EOS' || fail "a failing apt-get left files behind"
test ! -e /usr/sbin/policy-rc.d && test ! -e /etc/nereus-rendezvous
EOS
pass "a failing apt-get update, and a failing apt-get install, stop setup-server.sh with no policy-rc.d left behind (it was in place while apt-get ran)"

out="$(sx <<EOS 2>&1
set -euo pipefail
env ${setup_env} rv-setup --dry-run --no-start
EOS
)" || { printf '%s\n' "$out" >&2; fail "setup-server.sh --dry-run failed"; }
sx <<'EOS' || fail "the dry run changed something"
test ! -e /etc/nereus-rendezvous && test ! -e /etc/systemd/system/nereus-rendezvous.service
! id -u nereusrv >/dev/null 2>&1
sha256sum --quiet -c /run/caddyfile.sum
EOS
grep -q 'relay: 10 slots (total-quota), 8 per station id (user-quota), 80000 bytes/s each way per slot (max-bps): bps-capacity 800000 bytes/s' <<<"$out" \
    || { printf '%s\n' "$out" >&2; fail "the dry run's sizing line (10 slots)"; }
out="$(sx <<EOS 2>&1
set -euo pipefail
env RV_PUBLIC_IPV4=${s4} RV_PUBLIC_IPV6=${s6} RV_ALLOW_DOCUMENTATION_ADDRESSES=1 rv-setup --dry-run --no-start
EOS
)" || { printf '%s\n' "$out" >&2; fail "setup-server.sh --dry-run with the default sizing failed"; }
grep -q 'relay: 128 slots (total-quota), 8 per station id (user-quota), 80000 bytes/s each way per slot (max-bps): bps-capacity 10240000 bytes/s, at most 81920 kbit/s out' <<<"$out" \
    || { printf '%s\n' "$out" >&2; fail "the default sizing line"; }
grep -Eq 'data-use report: outbound bytes on eth[0-9]+, a warning past 1000 GB' <<<"$out" \
    || { printf '%s\n' "$out" >&2; fail "the data-use line"; }
pass "setup-server.sh --dry-run passes and changes nothing; default relay 128 slots, bps-capacity 10240000 bytes/s (128 x 80000), user-quota 8; data use on the default route's interface, threshold 1000 GB"

out="$(sx <<EOS 2>&1
set -euo pipefail
env ${setup_env} rv-setup --no-start
EOS
)" || { printf '%s\n' "$out" >&2; fail "setup-server.sh failed"; }
printf '%s\n' "$out" | grep -E 'bps-capacity|made|installed|changed' | sed 's/^/# /'
pass "setup-server.sh --no-start"

out="$(sx <<EOS 2>&1
set -euo pipefail
env ${setup_env} rv-setup --no-start
EOS
)" || { printf '%s\n' "$out" >&2; fail "setup-server.sh (second run) failed"; }
if grep -q 'changed$' <<<"$out"; then
    printf '%s\n' "$out" >&2
    fail "a second run of setup-server.sh changed a file"
fi
pass "a second run changes nothing (idempotent)"

sx <<'EOS' || fail "files, owners or modes are not as expected"
set -euo pipefail
check() { [[ "$(stat -c '%U:%G %a' "$1")" == "$2" ]] || { echo "$1: $(stat -c '%U:%G %a' "$1"), want $2" >&2; exit 1; }; }
check /etc/nereus-rendezvous/turn-secret "root:root 600"
check /etc/nereus-rendezvous/relay-secret "root:root 600"
check /etc/sysctl.d/60-nereus-rendezvous.conf "root:root 644"
cmp -s /etc/sysctl.d/60-nereus-rendezvous.conf /repo/rendezvous/deploy/sysctl.conf
check /etc/nereus-rendezvous/relay.conf "root:root 644"
check /etc/systemd/system/nereus-relay.service "root:root 644"
check /etc/systemd/system/nereus-relay.service.d/memory.conf "root:root 644"
check /etc/turnserver.conf "root:turnserver 640"
check /etc/nereus-rendezvous/coturn/key.pem "root:turnserver 640"
check /etc/nereus-rendezvous/rendezvous.conf "root:root 644"
check /etc/systemd/system/nereus-rendezvous.service "root:root 644"
check /etc/systemd/system/coturn.service.d/nereus.conf "root:root 644"
check /etc/systemd/system/nereus-data-use.service "root:root 644"
check /etc/systemd/system/nereus-data-use.timer "root:root 644"
check /usr/local/libexec/nereus-rendezvous/data-use "root:root 755"
check /etc/nereus-rendezvous/data-use.conf "root:root 644"
check /opt/nereus-rendezvous "nereusrv:nereusrv 755"
check /etc/caddy/Caddyfile "root:root 644"
check /etc/systemd/system/caddy.service.d/nereus.conf "root:root 644"
check /etc/systemd/system/nereus-rendezvous.service.d/memory.conf "root:root 644"
check /home/nereusrv/.ssh/authorized_keys "nereusrv:nereusrv 600"
cmp -s /home/nereusrv/.ssh/authorized_keys /run/deploy.pub
grep -qx 'rv.nereussdr.com {' /etc/caddy/Caddyfile
caddy validate --config /etc/caddy/Caddyfile --adapter caddyfile >/dev/null 2>&1
# RV_MEMORY_MB=1024: (1024 - 256) x 50% and x 35%.
grep -qx 'Environment=GOMEMLIMIT=384MiB' /etc/systemd/system/caddy.service.d/nereus.conf
grep -qx 'MemoryMax=268M' /etc/systemd/system/nereus-rendezvous.service.d/memory.conf
# The WebSocket relay: 32 MiB and 2 MiB for each of its 16 default slots.
grep -qx 'MemoryMax=64M' /etc/systemd/system/nereus-relay.service.d/memory.conf
[[ "$(wc -c < /etc/nereus-rendezvous/relay-secret)" -eq 65 ]]
! cmp -s /etc/nereus-rendezvous/relay-secret /etc/nereus-rendezvous/turn-secret
! grep -q "$(cat /etc/nereus-rendezvous/relay-secret)" /etc/turnserver.conf
grep -qx 'relay_secret_file = /run/credentials/nereus-rendezvous.service/relay-secret' /etc/nereus-rendezvous/rendezvous.conf
grep -qx 'relay_url = wss://rv.nereussdr.com/v1/relay' /etc/nereus-rendezvous/rendezvous.conf
grep -qx 'relay_secret_file = /run/credentials/nereus-relay.service/relay-secret' /etc/nereus-rendezvous/relay.conf
grep -qx 'socket = /run/nereus-relay/relay.sock' /etc/nereus-rendezvous/relay.conf
grep -qx 'socket_mode = 0660' /etc/nereus-rendezvous/relay.conf
grep -qx 'socket_group = caddy' /etc/nereus-rendezvous/relay.conf
grep -qx 'SupplementaryGroups=caddy' /etc/systemd/system/nereus-relay.service
grep -qx 'RuntimeDirectory=nereus-relay' /etc/systemd/system/nereus-relay.service
grep -qx 'slots = 16' /etc/nereus-rendezvous/relay.conf
grep -Eqx 'RV_DATA_USE_INTERFACE=eth[0-9]+' /etc/nereus-rendezvous/data-use.conf
grep -qx 'RV_TRANSFER_GB_PER_MONTH=1000' /etc/nereus-rendezvous/data-use.conf
grep -qx 'total-quota=10' /etc/turnserver.conf
grep -qx 'bps-capacity=800000' /etc/turnserver.conf
grep -qx 'user-quota=8' /etc/turnserver.conf
[[ "$(wc -c < /etc/nereus-rendezvous/turn-secret)" -eq 65 ]]
grep -qx "static-auth-secret=$(cat /etc/nereus-rendezvous/turn-secret)" /etc/turnserver.conf
grep -qx 'turn_secret_file = /run/credentials/nereus-rendezvous.service/turn-secret' /etc/nereus-rendezvous/rendezvous.conf
# The URLs setup writes are the service's built-in defaults and the
# sample's, in the same order (IPv4 first; rendezvous document section 8).
sed -e 's|^turn_secret_file = .*|turn_secret_file =|' -e 's|^relay_secret_file = .*|relay_secret_file =|' \
    /etc/nereus-rendezvous/rendezvous.conf > /tmp/urls.conf
PYTHONPATH=/repo/rendezvous/server PYTHONDONTWRITEBYTECODE=1 python3 - <<'PY'
from nereus_rendezvous import config
written = config.load("/tmp/urls.conf")
sample = config.load("/repo/rendezvous/server/rendezvous.conf.sample")
defaults = config.Config()
for name in ("stun_urls", "turn_urls"):
    assert getattr(written, name) == getattr(defaults, name) == getattr(sample, name), name
assert written.stun_urls[0].startswith("stun:rv4.") and written.turn_urls[0].startswith("turn:rv4."), written
assert written.relay_url == defaults.relay_url == sample.relay_url, written.relay_url
PY
# The relay's configuration as setup wrote it loads with the relay's own
# reader, the secret taken from where it is now.
sed 's|^relay_secret_file = .*|relay_secret_file = /etc/nereus-rendezvous/relay-secret|' \
    /etc/nereus-rendezvous/relay.conf > /tmp/relay.conf
PYTHONPATH=/repo/rendezvous/server PYTHONDONTWRITEBYTECODE=1 python3 -c \
    'from nereus_relay import config; c = config.load("/tmp/relay.conf"); assert c.slots == 16 and (c.socket, c.socket_mode, c.socket_group) == ("/run/nereus-relay/relay.sock", 0o660, "caddy"), c'
EOS
pass "relay secret (root 600, 64 hex characters, not the TURN secret and nowhere in coturn's configuration); the sysctl file (tcp_notsent_lowat, root 644); relay.conf (the Unix socket /run/nereus-relay/relay.sock, mode 0660, group caddy, 16 slots, the credential path); the relay's unit and its MemoryMax 64M; the service's relay_url and relay_secret_file"
pass "secret (root 600, 64 hex characters), coturn configuration (root:turnserver 640, total-quota 10, bps-capacity 800000, user-quota 8), service configuration (STUN and TURN URLs the same, in the same order, as the service's defaults and the sample: rv4 first), units, data-use report and code directory; the deploy account nereusrv with exactly the given key; the rv Caddyfile, validated; Caddy's drop-in with GOMEMLIMIT 384MiB and the service's MemoryMax 268M from 1024 MiB"

# The data-use report as installed, run once the way its unit runs it.
out="$(sx <<'EOS' 2>&1
set -euo pipefail
set -a; . /etc/nereus-rendezvous/data-use.conf; set +a
/usr/local/libexec/nereus-rendezvous/data-use --interface "$RV_DATA_USE_INTERFACE" \
    --threshold-gb "$RV_TRANSFER_GB_PER_MONTH" --state /tmp/data-use-state.json
EOS
)" || { printf '%s\n' "$out" >&2; fail "the installed data-use report did not run"; }
printf '%s\n' "$out" | sed 's/^/# /'
grep -Eq '^<6>data use: 0.00 GB so far this month \([0-9-]+\) on eth[0-9]+.*report threshold 1000 GB$' <<<"$out" || fail "the data-use report's line"
pass "the installed data-use report reads the interface's counter and writes its line"

# ------------------------------------------------ where systemd runs
# setup-server.sh believes systemd runs when /run/systemd/system exists.
# A stand-in systemctl logs every call and starts the programs the units
# name, as their accounts, so step 7 runs whole: enabling, starting,
# restarting only after a change, and checking the sockets.
sx <<'EOS'
set -euo pipefail
mkdir -p /run/systemd/system
cat > /usr/local/sbin/systemctl <<'SH'
#!/bin/bash
# A stand-in for systemctl, for coturn-check.sh.
echo "$*" >> /run/systemctl.log
running() {
    local p
    for p in /proc/[0-9]*; do
        if [[ "$(cat "$p/comm" 2>/dev/null)" == "$1" ]] && tr '\0' ' ' < "$p/cmdline" 2>/dev/null | grep -q -- "$2"; then
            echo "${p#/proc/}"
        fi
    done
}
unit_of() {
    case "$1" in
        coturn | coturn.service) echo coturn ;;
        caddy | caddy.service) echo caddy ;;
        nereus-rendezvous | nereus-rendezvous.service) echo service ;;
        nereus-relay | nereus-relay.service) echo relay ;;
        *) echo other ;;
    esac
}
# Caddy runs from a copy of the Caddyfile with its own local authority
# (local_certs), so the check never asks Let's Encrypt for anything.
caddy_test_config() {
    awk 'BEGIN { done = 0 } { print } !done && $0 == "{" { print "\tlocal_certs"; print "\tskip_install_trust"; done = 1 }' \
        /etc/caddy/Caddyfile > /run/Caddyfile.test
}
active() {
    case "$(unit_of "$1")" in
        coturn) [[ -n "$(running turnserver turnserver)" ]] ;;
        caddy) [[ -n "$(running caddy caddy)" ]] ;;
        service) [[ -n "$(running python3 nereus_rendezvous)" ]] ;;
        relay) [[ -n "$(running python3 nereus_relay)" ]] ;;
        *) [[ -e "/run/standin-active-$1" ]] ;;
    esac
}
stop() {
    local pid
    case "$(unit_of "$1")" in
        coturn) for pid in $(running turnserver turnserver); do kill "$pid"; done ;;
        caddy) for pid in $(running caddy caddy); do kill "$pid"; done ;;
        service) for pid in $(running python3 nereus_rendezvous); do kill "$pid"; done ;;
        relay) for pid in $(running python3 nereus_relay); do kill "$pid"; done ;;
        *) rm -f "/run/standin-active-$1" ;;
    esac
    sleep 1
}
start() {
    cd /
    case "$(unit_of "$1")" in
        coturn)
            exec_start="$(sed -n 's/^ExecStart=//p' /usr/lib/systemd/system/coturn.service)"
            # shellcheck disable=SC2086
            setsid setpriv --reuid=turnserver --regid=turnserver --init-groups \
                --inh-caps=-all,+net_bind_service --ambient-caps=-all,+net_bind_service \
                --bounding-set=-all,+net_bind_service $exec_start </dev/null >/dev/null 2>&1 &
            sleep 2 ;;
        caddy)
            caddy_test_config
            chown caddy:caddy /run/Caddyfile.test
            setsid setpriv --reuid=caddy --regid=caddy --init-groups \
                --inh-caps=-all,+net_bind_service --ambient-caps=-all,+net_bind_service \
                --bounding-set=-all,+net_bind_service env HOME=/var/lib/caddy \
                caddy run --config /run/Caddyfile.test --adapter caddyfile </dev/null >/run/caddy.log 2>&1 &
            for _ in $(seq 50); do (exec 3<>/dev/tcp/127.0.0.1/443) 2>/dev/null && break; sleep 0.2; done ;;
        service | relay)
            name="nereus-rendezvous"
            [[ "$(unit_of "$1")" == relay ]] && name="nereus-relay"
            unit="/etc/systemd/system/${name}.service"
            # Every LoadCredential= line, as systemd would hand it over.
            install -d -m 0755 "/run/credentials/${name}.service"
            while IFS=: read -r cred src; do
                install -m 0400 -o nobody "$src" "/run/credentials/${name}.service/${cred}"
            done < <(sed -n 's/^LoadCredential=//p' "$unit")
            envs=()
            while IFS= read -r line; do envs+=("$line"); done < <(sed -n 's/^Environment=//p' "$unit")
            exec_start="$(sed -n 's/^ExecStart=//p' "$unit")"
            # RuntimeDirectory= and SupplementaryGroups=, as systemd would.
            groups_arg=--clear-groups
            runtime="$(sed -n 's/^RuntimeDirectory=//p' "$unit")"
            if [[ -n "$runtime" ]]; then
                install -d -o nobody -m "$(sed -n 's/^RuntimeDirectoryMode=//p' "$unit")" "/run/${runtime}"
            fi
            extra="$(sed -n 's/^SupplementaryGroups=//p' "$unit")"
            if [[ -n "$extra" ]]; then
                groups_arg="--groups=${extra// /,}"
            fi
            # shellcheck disable=SC2086
            setsid env "${envs[@]}" setpriv --reuid=nobody --regid=nogroup "$groups_arg" $exec_start \
                </dev/null >>"/run/${name}.log" 2>&1 &
            for _ in $(seq 50); do active "$name" && break; sleep 0.2; done
            sleep 1 ;;
        *) touch "/run/standin-active-$1" ;;
    esac
}
cmd="$1"; shift
[[ "$cmd" == --quiet ]] && { cmd="$1"; shift; }
[[ "${1:-}" == --quiet ]] && shift
case "$cmd" in
    daemon-reload) exit 0 ;;
    enable) [[ ! -e /run/standin-fail-enable ]] || { echo "Failed to enable unit: stand-in refuses" >&2; exit 1; } ;;
    reload)
        for u in "$@"; do
            [[ "$(unit_of "$u")" == caddy ]] || continue
            caddy_test_config
            chown caddy:caddy /run/Caddyfile.test
            runuser -u caddy -- env HOME=/var/lib/caddy caddy reload --config /run/Caddyfile.test --adapter caddyfile >/dev/null 2>&1
        done ;;
    disable) if [[ "${1:-}" == --now ]]; then shift; for u in "$@"; do stop "$u"; done; fi; exit 0 ;;
    is-active) active "$1" ;;
    start) for u in "$@"; do active "$u" || start "$u"; done ;;
    stop) for u in "$@"; do stop "$u"; done ;;
    restart) for u in "$@"; do stop "$u"; start "$u"; done ;;
    *) echo "systemctl stand-in: $cmd not handled" >&2; exit 1 ;;
esac
SH
chmod 755 /usr/local/sbin/systemctl
# The service's and the relay's code, where deploy.sh would put it.
cp -R /repo/rendezvous/server/nereus_rendezvous /repo/rendezvous/server/nereus_relay /opt/nereus-rendezvous/
chown -R nereusrv:nereusrv /opt/nereus-rendezvous
EOS

# Holders of the relay's port numbers, then a dry run each time.
holder_case() {
    # $1: tcp or udp; $2: port. Prints the dry run's output and exit status.
    sx "$1" "$2" "$setup_env" <<'EOS' 2>&1
set -uo pipefail
python3 -c 'import socket, sys, time
kind = socket.SOCK_STREAM if sys.argv[1] == "tcp" else socket.SOCK_DGRAM
s = socket.socket(socket.AF_INET, kind); s.bind(("0.0.0.0", int(sys.argv[2])))
if kind == socket.SOCK_STREAM: s.listen()
time.sleep(30)' "$1" "$2" &
holder=$!
sleep 1
# shellcheck disable=SC2086
env $3 rv-setup --dry-run
echo "exit $?"
kill "$holder"
EOS
}
out="$(holder_case tcp 443)"
grep -qx 'exit 0' <<<"$out" && grep -q 'UDP 3478 and 443 are free or held by coturn' <<<"$out" \
    || { printf '%s\n' "$out" >&2; fail "a TCP 443 holder (Caddy's port) was refused"; }
for port in 443 3478; do
    out="$(holder_case udp "$port")"
    grep -qx 'exit 1' <<<"$out" && grep -q 'another process holds UDP 3478 or 443' <<<"$out" \
        && grep -Eq "^udp ${port} 0\.0\.0\.0 [0-9]+ python3$" <<<"$out" \
        || { printf '%s\n' "$out" >&2; fail "a UDP ${port} holder was not refused"; }
done
pass "where systemd runs: a TCP 443 holder (Caddy's) is allowed, a UDP 443 holder and a UDP 3478 holder are refused, each named"

# The whole script where systemd runs, with coturn reading as missing so
# step 2 runs with apt-get standing in (it installs nothing; coturn is
# already on the image).
out="$(sx "$setup_env" <<'EOS' 2>&1
set -euo pipefail
mkdir -p /run/apt-standin
printf '#!/bin/sh
exit 1
' > /run/apt-standin/dpkg-query
printf '#!/bin/sh
exit 0
' > /run/apt-standin/apt-get
chmod 755 /run/apt-standin/*
rm -f /run/systemctl.log
# shellcheck disable=SC2086
PATH=/run/apt-standin:$PATH env $1 rv-setup
rm -rf /run/apt-standin
echo "--- systemctl calls"
cat /run/systemctl.log
EOS
)" || { printf '%s
' "$out" >&2; fail "setup-server.sh where systemd runs failed"; }
printf '%s\n' "$out" | sed -n '/sockets on UDP/,/no TCP port/p; /--- systemctl calls/,$p' | sed 's/^/# /'
python3 - <<PY || { printf '%s\n' "$out" >&2; fail "step 2 or step 7 where systemd runs"; }
out = """${out}"""
calls = out.split("--- systemctl calls\n", 1)[1].splitlines()
assert "disable --now coturn" in calls, calls
first_disable = calls.index("disable --now coturn")
enable = calls.index("enable coturn nereus-rendezvous.service nereus-relay.service nereus-data-use.timer")
assert first_disable < enable, calls
assert "enable caddy" in calls, calls
for unit in ("caddy", "coturn", "nereus-rendezvous.service", "nereus-relay.service", "nereus-data-use.timer"):
    assert "start " + unit in calls, (unit, calls)
assert not any(c.startswith(("restart", "reload")) for c in calls), calls
assert "coturn is disabled until its configuration is in place" in out
assert "coturn holds UDP 3478 and 443 on both addresses and no TCP port; Caddy holds no UDP port" in out
assert "the WebSocket relay listens on /run/nereus-relay/relay.sock (socket, mode 0660, group caddy) and on no IP port" in out
PY
pass "where systemd runs: coturn is disabled right after its install, then step 9 enables and starts Caddy, coturn, the service, the WebSocket relay and the timer, and finds coturn on exactly UDP 3478 and 443 on both addresses and no TCP port, Caddy on no UDP port, and the relay on its Unix socket (mode 0660, group caddy) and no IP port"

rerun() {
    # $@: extra arguments. Prints the output and the systemctl calls.
    sx "$setup_env" "$@" <<'EOS' 2>&1
set -euo pipefail
settings="$1"
shift
rm -f /run/systemctl.log
# shellcheck disable=SC2086
env $settings rv-setup "$@"
echo "--- systemctl calls"
cat /run/systemctl.log 2>/dev/null || true
EOS
}
out="$(rerun)" || { printf '%s\n' "$out" >&2; fail "a second run where systemd runs failed"; }
grep -q 'caddy is running and none of its files changed: left as it is' <<<"$out" \
    && grep -q 'coturn is running and none of its files changed: left as it is' <<<"$out" \
    && grep -q 'nereus-rendezvous.service is running and none of its files changed: left as it is' <<<"$out" \
    && grep -q 'nereus-relay.service is running and none of its files changed: left as it is' <<<"$out" \
    && ! sed -n '/--- systemctl calls/,$p' <<<"$out" | grep -Eq '^(restart|reload)' \
    && grep -q 'coturn holds UDP 3478 and 443 on both addresses' <<<"$out" \
    || { printf '%s\n' "$out" >&2; fail "a second run restarted something"; }
out="$(rerun --dry-run --rotate-secret)" || { printf '%s\n' "$out" >&2; fail "--dry-run --rotate-secret failed"; }
grep -q 'would make a new secret in /etc/nereus-rendezvous/turn-secret (--rotate-secret)' <<<"$out" \
    && grep -q 'coturn would be restarted (its files changed); a restart drops every live relay' <<<"$out" \
    && grep -q 'nereus-rendezvous.service would be restarted' <<<"$out" \
    && grep -q 'would make a new relay secret in /etc/nereus-rendezvous/relay-secret (--rotate-secret)' <<<"$out" \
    && grep -q 'nereus-relay.service would be restarted (its files changed); every relay leg joins again' <<<"$out" \
    || { printf '%s\n' "$out" >&2; fail "--dry-run --rotate-secret did not say what it would do"; }
out="$(rerun --dry-run)" || { printf '%s\n' "$out" >&2; fail "--dry-run failed"; }
grep -q 'coturn would be left running (none of its files changed)' <<<"$out" \
    || { printf '%s\n' "$out" >&2; fail "a plain dry run did not say coturn would be left running"; }
before="$(sx <<<'cat /etc/nereus-rendezvous/turn-secret')"
relay_before="$(sx <<<'cat /etc/nereus-rendezvous/relay-secret')"
out="$(rerun --rotate-secret)" || { printf '%s\n' "$out" >&2; fail "--rotate-secret failed"; }
sed -n '/--- systemctl calls/,$p' <<<"$out" | grep -qx 'restart coturn' \
    && sed -n '/--- systemctl calls/,$p' <<<"$out" | grep -qx 'restart nereus-rendezvous.service' \
    && sed -n '/--- systemctl calls/,$p' <<<"$out" | grep -qx 'restart nereus-relay.service' \
    || { printf '%s\n' "$out" >&2; fail "--rotate-secret did not restart coturn, the service and the relay"; }
[[ "$(sx <<<'cat /etc/nereus-rendezvous/relay-secret')" != "$relay_before" ]] || fail "--rotate-secret kept the relay secret"
sx "$before" <<'EOS' || fail "the secret's backups are not as expected"
set -euo pipefail
[[ "$(cat /etc/nereus-rendezvous/turn-secret)" != "$1" ]]
backups=(/etc/turnserver.conf.bak-*)
[[ ${#backups[@]} -eq 1 ]] || { echo "backups: ${backups[*]}" >&2; exit 1; }
[[ "$(stat -c '%U:%G %a' "${backups[0]}")" == "root:root 600" ]] || { stat -c '%U:%G %a' "${backups[0]}" >&2; exit 1; }
grep -qx "static-auth-secret=$1" "${backups[0]}"
EOS
# A changed Caddyfile is a reload, not a restart.
sx <<<'echo "# changed by hand" >> /etc/caddy/Caddyfile'
out="$(rerun --dry-run)" || { printf '%s\n' "$out" >&2; fail "--dry-run after a Caddyfile change failed"; }
grep -q 'caddy would reload its configuration (open WebSockets are kept)' <<<"$out" \
    || { printf '%s\n' "$out" >&2; fail "the dry run did not say Caddy would reload"; }
out="$(rerun)" || { printf '%s\n' "$out" >&2; fail "a run after a Caddyfile change failed"; }
calls="$(sed -n '/--- systemctl calls/,$p' <<<"$out")"
grep -qx 'reload caddy' <<<"$calls" && ! grep -Eq '^restart' <<<"$calls" \
    || { printf '%s\n' "$out" >&2; fail "a Caddyfile change did not reload Caddy alone"; }
# A --no-start run between a change and the real run (the live steps:
# verify with --no-start, then start): the real run still does what the
# change calls for, and a run after it does nothing.
sx <<'EOS'
set -euo pipefail
echo "# changed by hand" >> /etc/caddy/Caddyfile
echo "# changed by hand" >> /etc/systemd/system/nereus-rendezvous.service
EOS
out="$(rerun --no-start)" || { printf '%s\n' "$out" >&2; fail "a --no-start run after a Caddyfile and unit change failed"; }
calls="$(sed -n '/--- systemctl calls/,$p' <<<"$out")"
! grep -Eq '^(start|restart|reload|enable|daemon-reload)' <<<"$calls" \
    && grep -q 'left in /etc/nereus-rendezvous/pending-actions for the next run without --no-start: systemctl daemon-reload; reload caddy; restart nereus-rendezvous.service$' <<<"$out" \
    || { printf '%s\n' "$out" >&2; fail "--no-start started something, or did not say what it left pending"; }
sx <<'EOS' || fail "the pending-actions file after --no-start is not as expected"
set -euo pipefail
[[ "$(stat -c '%U:%G %a' /etc/nereus-rendezvous/pending-actions)" == "root:root 600" ]]
[[ "$(cat /etc/nereus-rendezvous/pending-actions)" == $'units_changed\ncaddyfile_changed\nservice_changed' ]]
sha256sum /etc/nereus-rendezvous/pending-actions > /run/pending.sum
EOS
out="$(rerun --dry-run)" || { printf '%s\n' "$out" >&2; fail "a dry run with actions pending failed"; }
grep -q 'pending from an earlier run that started no service (--no-start) or stopped early: systemctl daemon-reload; reload caddy; restart nereus-rendezvous.service$' <<<"$out" \
    && grep -q 'caddy would reload its configuration (open WebSockets are kept)' <<<"$out" \
    && grep -q 'nereus-rendezvous.service would be restarted' <<<"$out" \
    && grep -q 'coturn would be left running (none of its files changed)' <<<"$out" \
    && grep -q 'nereus-relay.service would be left running (none of its files changed)' <<<"$out" \
    || { printf '%s\n' "$out" >&2; fail "the dry run did not report the pending reload and restart"; }
sx <<<'sha256sum --quiet -c /run/pending.sum' || fail "the dry run changed the pending-actions file"
out="$(rerun)" || { printf '%s\n' "$out" >&2; fail "the real run after --no-start failed"; }
calls="$(sed -n '/--- systemctl calls/,$p' <<<"$out")"
grep -qx 'daemon-reload' <<<"$calls" && grep -qx 'reload caddy' <<<"$calls" \
    && grep -qx 'restart nereus-rendezvous.service' <<<"$calls" \
    && [[ "$(grep -Ec '^(restart|reload)' <<<"$calls")" -eq 2 ]] \
    || { printf '%s\n' "$out" >&2; fail "the real run after --no-start did not reload Caddy and restart the service alone"; }
sx <<<'test ! -e /etc/nereus-rendezvous/pending-actions' || fail "the real run left the pending-actions file"
out="$(rerun)" || { printf '%s\n' "$out" >&2; fail "a second real run after --no-start failed"; }
calls="$(sed -n '/--- systemctl calls/,$p' <<<"$out")"
! grep -Eq '^(restart|reload|daemon-reload)' <<<"$calls" \
    || { printf '%s\n' "$out" >&2; fail "a second real run after --no-start reloaded or restarted something"; }
pass "where systemd runs: a --no-start run that changes the Caddyfile and the service's unit starts nothing and leaves the reload and restart pending (root 600; the dry run reports them and changes nothing); the next real run runs daemon-reload, reloads Caddy and restarts the service and nothing else; a run after that does neither"
# A failing systemctl enable says so.
sx <<<'touch /run/standin-fail-enable'
out="$(rerun)" && { printf '%s\n' "$out" >&2; fail "a failing systemctl enable did not stop setup-server.sh"; }
sx <<<'rm -f /run/standin-fail-enable'
grep -q 'Failed to enable unit: stand-in refuses' <<<"$out" && grep -q 'systemctl enable caddy failed (above)' <<<"$out" \
    || { printf '%s\n' "$out" >&2; fail "a failing systemctl enable gave no message"; }
# A file that cannot be installed stops the script (install_file's die is
# not lost inside a command substitution).
sx <<<'rm -f /etc/nereus-rendezvous/data-use.conf && ln -s /etc/hostname /etc/nereus-rendezvous/data-use.conf'
out="$(rerun)" && { printf '%s\n' "$out" >&2; fail "a symlinked data-use.conf did not stop setup-server.sh"; }
sx <<<'rm -f /etc/nereus-rendezvous/data-use.conf'
grep -q 'data-use.conf is a symlink; refusing to follow it as root' <<<"$out" && ! grep -q '10/10 Summary' <<<"$out" \
    || { printf '%s\n' "$out" >&2; fail "a failing file install did not stop setup-server.sh with its message"; }
out="$(rerun)" || { printf '%s\n' "$out" >&2; fail "a run to restore data-use.conf failed"; }
# coturn stays disabled when the install fails after its package was
# unpacked.
out="$(sx "$setup_env" <<'EOS' 2>&1
set -uo pipefail
mkdir -p /run/apt-standin
printf '#!/bin/sh\nexit 1\n' > /run/apt-standin/dpkg-query
printf '#!/bin/sh\nfor a in "$@"; do [ "$a" = install ] && exit 100; done\nexit 0\n' > /run/apt-standin/apt-get
chmod 755 /run/apt-standin/*
rm -f /run/systemctl.log
# shellcheck disable=SC2086
PATH=/run/apt-standin:$PATH env $1 rv-setup
echo "exit $?"
test -e /usr/sbin/policy-rc.d && echo "policy-rc.d left behind"
rm -rf /run/apt-standin
echo "--- systemctl calls"
cat /run/systemctl.log
EOS
)"
grep -qx 'exit 1' <<<"$out" && grep -qx 'disable --now coturn' <<<"$out" && ! grep -q 'left behind' <<<"$out" \
    || { printf '%s\n' "$out" >&2; fail "a failed install left coturn enabled"; }
out="$(rerun)" || { printf '%s\n' "$out" >&2; fail "a run after the failed install failed"; }
pass "where systemd runs: a Caddyfile change reloads Caddy (the dry run says so) and restarts nothing; a failing systemctl enable and a symlinked data-use.conf each stop the script with a message; a failed install after coturn was unpacked still disables coturn"

pass "where systemd runs: a second run leaves Caddy, coturn, the service and the WebSocket relay running; --dry-run --rotate-secret says it would make new TURN and relay secrets and restart coturn, the service and the relay (dropping live relays); --rotate-secret restarts all three with a new relay secret; one backup of /etc/turnserver.conf remains, root 600"

# Code in /opt from before the WebSocket relay: setup refuses, in a dry run
# too, and changes nothing, until deploy.sh has put the new code there.
sx <<'EOS'
set -euo pipefail
mv /opt/nereus-rendezvous/nereus_relay /run/nereus_relay.new
mv /opt/nereus-rendezvous/nereus_rendezvous/relaygrant.py /run/relaygrant.py.new
sha256sum /etc/nereus-rendezvous/rendezvous.conf /etc/systemd/system/nereus-rendezvous.service > /run/stale.sum
EOS
for args in "--dry-run" ""; do
    # shellcheck disable=SC2086
    out="$(rerun $args)" && { printf '%s\n' "$out" >&2; fail "setup-server.sh ${args} ran with code from before the relay"; }
    grep -q "is from before the WebSocket relay: run rendezvous/deploy.sh first" <<<"$out" \
        && ! sed -n '/--- systemctl calls/,$p' <<<"$out" | grep -Eq '^(start|restart|reload|enable)' \
        || { printf '%s\n' "$out" >&2; fail "setup-server.sh ${args} with old code did not stop with its message"; }
done
sx <<'EOS' || fail "setup-server.sh changed files while refusing old code"
set -euo pipefail
sha256sum --quiet -c /run/stale.sum
mv /run/nereus_relay.new /opt/nereus-rendezvous/nereus_relay
mv /run/relaygrant.py.new /opt/nereus-rendezvous/nereus_rendezvous/relaygrant.py
EOS
out="$(rerun --dry-run)" || { printf '%s\n' "$out" >&2; fail "a dry run with the new code back failed"; }
# Before the relay secret exists (a server set up before the relay), the dry
# run still checks both configurations, with a stand-in secret.
sx <<<'mv /etc/nereus-rendezvous/relay-secret /run/relay-secret.aside'
out="$(rerun --dry-run)" || { printf '%s\n' "$out" >&2; fail "a dry run without the relay secret failed"; }
sx <<<'mv /run/relay-secret.aside /etc/nereus-rendezvous/relay-secret'
grep -q 'would make /etc/nereus-rendezvous/relay-secret' <<<"$out" \
    && grep -q 'rendezvous.conf: accepted by the service.s own reader (a stand-in for a secret not made yet)' <<<"$out" \
    && grep -q 'relay.conf: accepted by the relay.s own reader (a stand-in for a secret not made yet)' <<<"$out" \
    || { printf '%s\n' "$out" >&2; fail "the dry run without the relay secret did not check the configurations"; }
pass "where systemd runs: code in /opt from before the WebSocket relay stops setup-server.sh (the dry run too) with 'run rendezvous/deploy.sh first', starting and changing nothing; without the relay secret yet, the dry run still checks both configurations with the programs' own readers and a stand-in secret"

# Back to no systemd for the rest: the check starts coturn itself.
sx <<'EOS'
set -euo pipefail
systemctl stop caddy coturn nereus-rendezvous.service nereus-relay.service
rm -rf /run/systemd/system /usr/local/sbin/systemctl /run/credentials /opt/nereus-rendezvous/nereus_rendezvous \
    /opt/nereus-rendezvous/nereus_relay
EOS

# --------------------------------------------------------------- coturn
start_coturn() {
    # $1: configuration file. As the unit and drop-in start it: the
    # turnserver account, CAP_NET_BIND_SERVICE and nothing else.
    sx "$1" <<'EOS'
set -euo pipefail
cd /
setsid setpriv --reuid=turnserver --regid=turnserver --init-groups \
    --inh-caps=-all,+net_bind_service --ambient-caps=-all,+net_bind_service \
    --bounding-set=-all,+net_bind_service \
    turnserver -c "$1" --pidfile= </dev/null >/run/turnserver.stdout 2>&1 &
for _ in $(seq 50); do
    grep -q 'Total General servers' /run/syslog.txt 2>/dev/null && break
    sleep 0.1
done
sleep 0.5
EOS
}
stop_coturn() {
    sx <<'EOS'
for pid in $(ls /proc | grep -E '^[0-9]+$'); do
    [[ "$(cat /proc/$pid/comm 2>/dev/null)" == turnserver ]] && kill "$pid" 2>/dev/null
done
sleep 1
EOS
}

start_coturn /etc/turnserver.conf
sockets="$(sx <<'EOS'
python3 - <<'PY'
import os, socket
def addr(h):
    host, port = h.split(":")
    raw = bytes.fromhex(host)
    if len(raw) == 4:
        return socket.inet_ntop(socket.AF_INET, raw[::-1]), int(port, 16)
    return socket.inet_ntop(socket.AF_INET6, b"".join(raw[i:i + 4][::-1] for i in range(0, 16, 4))), int(port, 16)
# Docker's root may not read another account's file descriptors, so the
# sockets are found by owner: every socket of the turnserver account.
import pwd
uid = str(pwd.getpwnam("turnserver").pw_uid)
for pid in filter(str.isdigit, os.listdir("/proc")):
    try:
        if open("/proc/%s/comm" % pid).read().strip() != "turnserver":
            continue
        caps = [l for l in open("/proc/%s/status" % pid) if l.startswith(("Uid:", "CapEff:"))]
        print("process", " ".join(l.split()[1] for l in caps))
    except OSError:
        pass
seen = set()
for name in ("udp", "udp6", "tcp", "tcp6"):
    for line in open("/proc/net/" + name).read().splitlines()[1:]:
        f = line.split()
        if f[7] == uid and (name.startswith("udp") or f[3] == "0A"):
            host, port = addr(f[1])
            seen.add("%s %s %d" % (name.rstrip("6"), host, port))
for s in sorted(seen):
    print(s)
PY
EOS
)"
printf '%s\n' "$sockets" | sed 's/^/# /'
expected="$(printf 'udp %s 3478\nudp %s 443\nudp %s 3478\nudp %s 443\n' "$s6" "$s6" "$s4" "$s4" | sort)"
[[ "$(printf '%s\n' "$sockets" | grep -v '^process' | sort)" == "$expected" ]] \
    || { sx <<<'tail -20 /run/syslog.txt; cat /run/turnserver.stdout' >&2; fail "coturn's sockets are not exactly UDP 3478 and 443 on both addresses"; }
grep -q '^process 100 0000000000000400$' <<<"$sockets" \
    || fail "coturn does not run as turnserver with CAP_NET_BIND_SERVICE only"
pass "coturn (as turnserver, CAP_NET_BIND_SERVICE only) holds exactly UDP 3478 and 443 on ${s4} and ${s6}, and no TCP port"
# shellcheck disable=SC2016  # expanded in the container
echo "# coturn resident memory at start: $(sx <<<'for p in /proc/[0-9]*; do [[ "$(cat $p/comm 2>/dev/null)" == turnserver ]] && grep VmRSS $p/status; done' | awk '{print $2, $3}')"

for host in "$s4" "$s6"; do
    for port in 3478 443; do
        line="$(probe binding "$host" "$port")" || fail "no STUN answer on ${host} UDP ${port}"
        grep -q '"ok": true' <<<"$line" || fail "STUN on ${host} UDP ${port}: ${line}"
    done
done
pass "STUN binding without credentials answers on UDP 3478 and 443, IPv4 and IPv6"

# Relay peers: an echo on the public network, and echoes the relay must
# never reach (private network, and loopback on the server itself).
docker exec -d "$peer" turnutils_peer -L "$p4" -L "$p6" -L "$q4" -L "$q6" -L "$d4" -L "$d6" -p 3480
docker exec -d "$server" turnutils_peer -L 127.0.0.1 -L ::1 -p 3480
sleep 1

# Credentials in coturn's time-limited form, computed here, not by the
# service: "<expiry>:<id>" and base64(HMAC-SHA1(secret, username)).
docker cp "${server}:/etc/nereus-rendezvous/turn-secret" - | docker cp - "${peer}:/tmp/"
creds() {
    # $1: seconds until expiry (negative: already expired); $2: id.
    px "$1" "$2" <<'EOS'
python3 - "$1" "$2" <<'PY'
import base64, hashlib, hmac, sys, time
secret = open("/tmp/turn-secret", "rb").read().rstrip(b"\r\n")
user = "%d:%s" % (int(time.time()) + int(sys.argv[1]), sys.argv[2])
print(user, base64.b64encode(hmac.new(secret, user.encode(), hashlib.sha1).digest()).decode())
PY
EOS
}

# uclient: no RTCP pair (-c), 20 messages to the peer; its whole output.
uclient() {
    # $1 user $2 password $3 server $4 port $5 peer; prints uclient's summary.
    # uclient keeps its allocations until they expire, so each run uses an
    # id of its own (user-quota) and coturn is restarted before it
    # (total-quota).
    docker exec "$peer" turnutils_uclient -c -n 20 -u "$1" -w "$2" -e "$5" -r 3480 -p "$4" "$3" 2>&1 || true
}
for combo in "$s4 3478 $p4 uclientaaaaaaaaaaaaaaaaaaa" "$s4 443 $p4 uclientbbbbbbbbbbbbbbbbbbb" \
        "$s6 3478 $p6 uclientccccccccccccccccccc" "$s6 443 $p6 uclientddddddddddddddddddd"; do
    read -r host port target id <<<"$combo"
    stop_coturn
    start_coturn /etc/turnserver.conf
    read -r user password < <(creds 600 "$id")
    out="$(uclient "$user" "$password" "$host" "$port" "$target")"
    grep -q 'Total lost packets 0 (0.000000%)' <<<"$out" \
        || { printf '%s\n' "$out" >&2; fail "uclient via ${host} UDP ${port} to ${target} lost packets"; }
    grep -q 'Total transmit time' <<<"$out" || fail "uclient via ${host} UDP ${port} did not run"
done
pass "turnutils_uclient: valid allocations relay to the peer with no loss, UDP 3478 and 443, IPv4 and IPv6"

stop_coturn
start_coturn /etc/turnserver.conf
read -r user password < <(creds -60 uclientexpiredexpiredexpir)
out="$(uclient "$user" "$password" "$s4" 3478 "$p4")"
if ! grep -q 'Cannot complete Allocation' <<<"$out" || grep -q 'Total lost packets' <<<"$out"; then
    printf '%s\n' "$out" >&2
    fail "uclient allocated with an expired credential"
fi
# uclient does not print the error code; the probe does.
out="$(probe allocate "$s4" 3478 --username "$user" --password "$password" || true)"
grep -q '"error": 401' <<<"$out" || fail "an expired credential was not refused 401: ${out}"
pass "turnutils_uclient: an expired credential gets no allocation (Cannot complete Allocation; the probe sees 401)"

for target in "$q4" "$q6"; do
    stop_coturn
    start_coturn /etc/turnserver.conf
    read -r user password < <(creds 600 uclienteeeeeeeeeeeeeeeeeee)
    out="$(uclient "$user" "$password" "$s4" 3478 "$target")"
    if grep -q 'Total lost packets 0 (0.000000%)' <<<"$out"; then
        printf '%s\n' "$out" >&2
        fail "uclient reached ${target} through the relay"
    fi
done
pass "turnutils_uclient: nothing reaches the reachable private peers ${q4} and ${q6}"

for target in "$d4" "$d6"; do
    stop_coturn
    start_coturn /etc/turnserver.conf
    read -r user password < <(creds 600 uclientffffffffffffffffffff)
    out="$(uclient "$user" "$password" "$s4" 3478 "$target")"
    if grep -q 'Total lost packets 0 (0.000000%)' <<<"$out"; then
        printf '%s\n' "$out" >&2
        fail "uclient reached ${target} through the relay"
    fi
done
pass "turnutils_uclient: nothing reaches the reachable documentation-range peers ${d4} and ${d6}"

stop_coturn
start_coturn /etc/turnserver.conf

# Every denied range, in each written form, with coturn's answer.
v4peers=(--peer "${p4}:3480" --peer "${q4}:3480" --peer "${d4}:3480" --peer 127.0.0.1:3480 --peer 0.0.0.0:3480
    --peer 172.16.0.1:3480 --peer 192.168.1.1:3480 --peer 100.64.0.1:3480 --peer 169.254.1.1:3480
    --peer 169.254.169.254:3480 --peer 224.0.0.1:3480 --peer 255.255.255.255:3480
    --peer 192.0.0.1:3480 --peer 198.18.0.1:3480 --peer 198.19.255.1:3480 --peer 198.51.100.1:3480
    --peer 203.0.113.1:3480)
v6peers=(--peer "[${p6}]:3480" --peer "[${q6}]:3480" --peer "[${d6}]:3480" --peer "[::1]:3480" --peer "[::]:3480"
    --peer "[fe80::1]:3480" --peer "[ff02::1]:3480" --peer "[::ffff:127.0.0.1]:3480"
    --peer "[::ffff:${q4}]:3480" --peer "[::ffff:${p4}]:3480" --peer "[64:ff9b::a63:14]:3480"
    --peer "[64:ff9b:1::a63:14]:3480" --peer "[100::1]:3480" --peer "[2001::1]:3480"
    --peer "[2001:db8::1]:3480" --peer "[2002::1]:3480")
peers4="$(probe allocate "$s4" 3478 --secret-file /tmp/turn-secret --id peersipv4peersipv4peersipv "${v4peers[@]}")"
peers6="$(probe allocate "$s6" 443 --secret-file /tmp/turn-secret --id peersipv6peersipv6peersipv "${v6peers[@]}")"
printf '%s\n%s\n' "$peers4" "$peers6" | sed 's/^/# /'
python3 - "$p4" "$p6" <<PY || fail "a denied peer was allowed, or the public peer was not"
import json, sys
lines = [json.loads(l) for l in """${peers4}
${peers6}""".splitlines()]
public = {"%s:3480" % sys.argv[1], "[%s]:3480" % sys.argv[2]}
peers = [l for l in lines if l["step"] == "peer"]
assert len(peers) == 33, len(peers)
for l in peers:
    if l["peer"] in public:
        assert l["permission"]["ok"] and l["relayed"], l
    else:
        assert not l["permission"]["ok"] and l["permission"]["error"] in (403, 443), l
PY
pass "denied peers: loopback, 0/8, RFC 1918, 100.64/10, link-local (the cloud metadata address too), multicast, broadcast, ::, unique-local, IPv4-mapped and NAT64 forms, and coturn's recommended ranges (192.0.0/24, documentation, benchmarking, 100::/64, Teredo, 6to4; the documentation peers reachable) all refused (403); the public peer relays in both families"

out="$(probe allocate "$s4" 3478 --anonymous || true)"
grep -q '"error": 401' <<<"$out" || fail "an anonymous allocation was not refused: ${out}"
out="$(probe allocate "$s4" 3478 --username "$(date +%s)0:someone" --password wrong || true)"
grep -q '"error": 401' <<<"$out" || fail "a wrongly signed allocation was not refused: ${out}"
read -r user password < <(creds 600 tcprelaytcprelaytcprelaytc)
out="$(probe allocate "$s4" 3478 --username "$user" --password "$password" --relay tcp || true)"
grep -q '"error": 442' <<<"$out" || fail "a TCP relay allocation was not refused: ${out}"
out="$(probe allocate "$s4" 3478 --username "$user" --password "$password" --transport tcp 2>&1 || true)"
grep -q 'ConnectionRefusedError' <<<"$out" || fail "TCP 3478 answered: ${out}"
pass "refused: anonymous (401), wrong password (401), TCP relay (442); TCP 3478 is closed"

# UDP 443 is coturn's DTLS-role port (turnserver.conf, "Ports"), with an
# empty cipher list: a DTLS client gets nowhere, plain TURN works (checked
# above).
read -r user password < <(creds 600 dtlsdtlsdtlsdtlsdtlsdtlsdt)
out="$(docker exec "$peer" timeout 20 turnutils_uclient -S -c -n 5 -u "$user" -w "$password" -e "$p4" -r 3480 -p 443 "$s4" 2>&1 || true)"
if grep -q 'Total lost packets' <<<"$out"; then
    printf '%s\n' "$out" >&2
    fail "a DTLS session was set up with coturn on UDP 443"
fi
pass "no DTLS session with coturn on UDP 443 (empty cipher list), while plain TURN there works"

stop_coturn
start_coturn /etc/turnserver.conf
quota="$(px "$s4" <<'EOS'
set -euo pipefail
cd /tmp
cat > hold.py <<'PY'
import sys, time
sys.path.insert(0, "/repo/rendezvous/tests")
import turn_probe as t
from argparse import Namespace
host, ids = sys.argv[1], sys.argv[2:]
clients = []
for sid in ids:
    c = t.Client(host, 3478)
    user, password, _ = t.credentials("/tmp/turn-secret", sid, 600)
    c.login(user, password)
    r = t.outcome(t.allocate(c, Namespace(relay="udp", family=None)))
    print(sid[:6], "ok" if r["ok"] else r["error"])
    clients.append(c)
PY
a=useraaaaaaaaaaaaaaaaaaaaaa b=userbbbbbbbbbbbbbbbbbbbbbb
python3 hold.py "$1" "$a" "$a" "$a" "$a" "$a" "$a" "$a" "$a" "$a" "$b" "$b" usercccccccccccccccccccccc
EOS
)"
printf '%s\n' "$quota" | sed 's/^/# /'
[[ "$(printf '%s\n' "$quota" | tr '\n' ' ')" == "useraa ok useraa ok useraa ok useraa ok useraa ok useraa ok useraa ok useraa ok useraa 486 userbb ok userbb ok usercc 486 " ]] \
    || fail "quotas: expected eight ok, 486, ok ok, 486"
pass "quotas: an eighth allocation by one station id is accepted and a ninth refused (486, user-quota 8); with 10 slots the eleventh on the server is refused (486, total-quota 10), and bps-capacity 800000 (10 x 80000) never refuses first"

# --------------------------------------------------- the service's credentials
stop_coturn
start_coturn /etc/turnserver.conf
sx <<'EOS'
set -euo pipefail
# LoadCredential=, by hand: systemd would copy the secret here for the
# service alone.
install -d -m 0755 /run/credentials/nereus-rendezvous.service /run/credentials/nereus-relay.service
install -m 0400 -o nobody /etc/nereus-rendezvous/turn-secret /run/credentials/nereus-rendezvous.service/turn-secret
install -m 0400 -o nobody /etc/nereus-rendezvous/relay-secret /run/credentials/nereus-rendezvous.service/relay-secret
install -m 0400 -o nobody /etc/nereus-rendezvous/relay-secret /run/credentials/nereus-relay.service/relay-secret
cd / && PYTHONPATH=/repo/rendezvous/server PYTHONDONTWRITEBYTECODE=1 \
    setsid runuser -u nobody -- python3 -m nereus_rendezvous --config /etc/nereus-rendezvous/rendezvous.conf \
    </dev/null >/run/rv.log 2>&1 &
install -d -o nobody -m 0755 /run/nereus-relay
cd / && PYTHONPATH=/repo/rendezvous/server PYTHONDONTWRITEBYTECODE=1 \
    setsid setpriv --reuid=nobody --regid=nogroup --groups=caddy \
    python3 -m nereus_relay --config /etc/nereus-rendezvous/relay.conf \
    </dev/null >/run/relay.log 2>&1 &
for _ in $(seq 50); do
    grep -q 'listening on 2 addresses, relay on, relay grants on' /run/rv.log \
        && grep -q 'listening on its Unix socket, 16 slots' /run/relay.log && exit 0
    sleep 0.2
done
cat /run/rv.log /run/relay.log >&2
exit 1
EOS
minted="$(sx <<'EOS'
cd /repo/rendezvous/tests && PYTHONPATH=/repo/rendezvous/server:/repo/rendezvous/tests PYTHONDONTWRITEBYTECODE=1 python3 - <<'PY'
import asyncio, json
from nereus_rendezvous import protocol
from helpers import register, recv_json, introduce_message
from runner import ws_connect
async def go():
    uri = "ws://127.0.0.1:8710/"
    st, _, sid = await register(uri, address="198.51.100.7")
    cl = await ws_connect(uri, "198.51.100.8")
    hello = await recv_json(cl)
    await cl.send(protocol.encode(introduce_message(sid, hello["nonce"])))
    intro = await recv_json(st)
    await st.send(protocol.encode({"type": "answer", "to": intro["from"], "answer": "v=0\r\n", "turn": True}))
    answer = await recv_json(cl)
    device_grant = await recv_json(cl)
    credentials = await recv_json(st)
    core_grant = await recv_json(st)
    assert answer["turn"] == credentials["turn"]
    # The relay grants: the token of each end joined on the running relay.
    from relay_helpers import connect, recv
    assert device_grant["url"] == core_grant["url"] == "wss://rv.nereussdr.com/v1/relay"
    relay_uri = "unix:/run/nereus-relay/relay.sock"
    core = await connect(relay_uri, "198.51.100.7")
    await core.send(b"\x80" + core_grant["token"].encode())
    assert await recv(core) == b"\x81\x01\x00"
    device = await connect(relay_uri, "198.51.100.8")
    await device.send(b"\x80" + device_grant["token"].encode())
    assert await recv(device) == b"\x81\x01\x01"
    assert await recv(core) == b"\x82\x01"
    await device.send(b"\x02datagram")
    assert await recv(core) == b"\x02datagram"
    print(json.dumps(answer["turn"]))
asyncio.run(go())
PY
EOS
)"
user="$(printf '%s' "$minted" | python3 -c 'import json,sys; print(json.load(sys.stdin)["username"])')"
password="$(printf '%s' "$minted" | python3 -c 'import json,sys; print(json.load(sys.stdin)["password"])')"
urls="$(printf '%s' "$minted" | python3 -c 'import json,sys; print(" ".join(json.load(sys.stdin)["urls"]))')"
echo "# minted for a station: urls ${urls}"
out="$(probe allocate "$s4" 443 --username "$user" --password "$password" --peer "${p4}:3480")"
grep -q '"relayed": true' <<<"$out" || fail "coturn did not accept the service's credentials: ${out}"
out="$(probe allocate "$s6" 3478 --username "$user" --password "$password" --peer "[${p6}]:3480")"
grep -q '"relayed": true' <<<"$out" || fail "coturn did not accept the service's credentials over IPv6: ${out}"
pass "credentials minted by the running service (setup's configuration, secret via the credentials path) allocate and relay on coturn; the relay grants it minted join both legs on the WebSocket relay run from setup's relay.conf, which forwards between them"

# --------------------------------------------------- refresh after expiry
expiry="$(probe expiry "$s4" 3478 --secret-file /tmp/turn-secret --id expiryexpiryexpiryexpiryex \
    --ttl 5 --wait 3 --wait 12 --peer "${p4}:3480")"
printf '%s\n' "$expiry" | sed 's/^/# real configuration (stale-nonce 600): /'
sx <<'EOS'
set -euo pipefail
# A test-only copy with a 15 s nonce lifetime and its own ports, to see what
# happens once the nonce the allocation was made with has gone stale.
sed -e 's/^stale-nonce=600$/stale-nonce=15/' -e 's/^listening-port=3478$/listening-port=5000/' \
    -e 's/^tls-listening-port=443$/tls-listening-port=5001/' -e 's/^min-port=61000$/min-port=60000/' \
    -e 's/^max-port=65535$/max-port=60500/' /etc/turnserver.conf > /run/stale.conf
chown root:turnserver /run/stale.conf && chmod 640 /run/stale.conf
EOS
start_coturn /run/stale.conf
stale="$(probe expiry "$s4" 5000 --secret-file /tmp/turn-secret --id staleexpirystaleexpirystal \
    --ttl 5 --wait 10 --wait 20 --wait 40 --peer "${p4}:3480")"
printf '%s\n' "$stale" | sed 's/^/# stale-nonce 15 (test only): /'
python3 - <<PY || fail "coturn's refresh-after-expiry behaviour is not the one recorded in the rendezvous document section 8"
import json
real = [json.loads(l) for l in """${expiry}""".splitlines()]
stale = [json.loads(l) for l in """${stale}""".splitlines()]
for lines in (real, stale):
    refreshes = [l for l in lines if l["step"] == "refresh"]
    assert all(l["ok"] for l in refreshes), refreshes
    assert any(l["credentialExpired"] for l in refreshes), refreshes
    assert all(l["permission"]["ok"] and l["relayed"] for l in lines if l["step"] == "peer-after-refresh"), lines
    late = [l for l in lines if l["step"] == "new-allocation-with-expired-credential"]
    assert late and late[0]["error"] == 401, late
assert any(l.get("firstAnswer") == 438 for l in stale if l["step"] == "refresh"), stale
PY
pass "refresh after expiry: coturn keeps refreshing an allocation (and granting permissions that relay) after its credential expired, also through a stale-nonce challenge (438, then accepted); only a new allocation with the expired credential is refused (401)"

# --------------------------------------------------------------- logs
logs="$(sx <<'EOS'
set -euo pipefail
test ! -e /var/log/turnserver
! ls /var/log/turn*.log /var/tmp/turn*.log /tmp/turn*.log /var/log/turn_*.log >/dev/null 2>&1
[[ ! -s /run/turnserver.stdout ]]
echo "lines $(grep -c . /run/syslog.txt)"
sed -E 's/^<[0-9]+>[A-Z][a-z]{2} [ 0-9]{2} [0-9:]{8} turnserver: [0-9]+: : //' /run/syslog.txt \
    | grep -E 'ERROR|WARNING|session|user|peer|realm' | sed -E 's/[0-9]{9,10}:[a-z0-9]{6}[a-z0-9]*/<expiry>:<station id>/g; s/[0-9a-f]{1,4}(:[0-9a-f]{0,4}){2,7}|([0-9]{1,3}\.){3}[0-9]{1,3}/<address>/g' \
    | sort | uniq -c | sort -rn
EOS
)" || fail "coturn wrote a log file or to standard output"
printf '%s\n' "$logs" | sed 's/^/# log: /'
pass "coturn logs to syslog only (no log file, nothing on standard output); the kinds of line it logs are listed above"

stop_coturn
echo "coturn-check: all ${passed} checks passed"
