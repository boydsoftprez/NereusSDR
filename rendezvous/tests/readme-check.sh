#!/usr/bin/env bash
#
# readme-check.sh: a fresh ubuntu:24.04 container set up by following
# rendezvous/README.md alone, then used in every role: the service (a
# WebSocket through Caddy, by host name), the TURN relay (STUN and TURN by
# the relay names, on both ports, in both families) and the WebSocket relay
# (the service's relay grants joined at wss://<host>/v1/relay through
# Caddy).
#
# The README marks each shell block it expects to be run:
#   <!-- check: server -->    run as written, as root, on the server
#   <!-- check: settings -->  the names and sizing; the check uses its own
#                             test names instead (the block must set the same
#                             variables), exported for every server block
#   <!-- check: copy -->      scp to the server: done with docker cp
#   <!-- check: deploy -->    rendezvous/deploy.sh from your computer: run as
#                             written on this machine, with NEREUS_RV_TARGET
#                             pointed at a local directory, then copied in
#   <!-- check: service -->   systemctl: the container has no systemd, so the
#                             check starts the same programs the units start,
#                             as the same accounts with the same arguments
# Unmarked blocks (DNS, the firewall, the checks against a live server) are
# not run. The server's public addresses are stand-ins on a Docker network
# (11.99.0.0/24 and the benchmarking prefix 2001:2:0:99::/64; the relay
# refuses documentation ranges as peers), passed to setup-server.sh as
# RV_PUBLIC_IPV4/IPV6, and
# Caddy uses its own local certificate authority (local_certs, added for the
# test only) instead of Let's Encrypt.
#
# Usage: rendezvous/tests/readme-check.sh      (needs Docker and network
#        access for apt; a few minutes)

set -euo pipefail

repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
readonly repo
readonly readme="${repo}/rendezvous/README.md"
readonly tag="$$"
readonly net="nereus-rv-readme-${tag}"
readonly server="nereus-rv-readme-server-${tag}" client="nereus-rv-readme-client-${tag}"
readonly s4="11.99.0.10" s6="2001:2:0:99::10"
readonly c4="11.99.0.20" c6="2001:2:0:99::20"
readonly test_env="export RV_PUBLIC_IPV4=${s4} RV_PUBLIC_IPV6=${s6} RV_ALLOW_DOCUMENTATION_ADDRESSES=1 DEBIAN_FRONTEND=noninteractive"

passed=0
pass() { passed=$((passed + 1)); printf 'ok %d - %s\n' "$passed" "$*"; }
fail() { printf 'not ok - %s\n' "$*" >&2; exit 1; }

work="$(mktemp -d "${TMPDIR:-/tmp}/nereus-readme-check.XXXXXX")"
# A deploy key made for this run only.
ssh-keygen -q -t ed25519 -N "" -C readme-check -f "${work}/deploy"
test_settings="export RV_HOST=rv.test RV_RELAY_HOST4=rv4.test RV_RELAY_HOST6=rv6.test RV_RELAY_SLOTS=128 RV_TRANSFER_GB_PER_MONTH=1000 RV_DEPLOY_KEY='$(cat "${work}/deploy.pub")'"
readonly test_settings
cleanup() {
    docker rm -f "$server" "$client" >/dev/null 2>&1 || true
    docker network rm "$net" >/dev/null 2>&1 || true
    rm -rf -- "$work"
}
trap cleanup EXIT

command -v docker >/dev/null 2>&1 || fail "docker is not installed"

# The marked blocks, in order, one file each, named <number>-<kind>.sh.
python3 - "$readme" "$work/blocks" <<'PY'
import re, sys, pathlib
text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
out = pathlib.Path(sys.argv[2])
out.mkdir()
pattern = re.compile(r"<!-- check: (\w+) -->\n```sh\n(.*?)```", re.S)
for n, m in enumerate(pattern.finditer(text)):
    (out / ("%02d-%s.sh" % (n, m.group(1)))).write_text(m.group(2), encoding="utf-8")
PY
blocks=("$work"/blocks/*.sh)
echo "# ${#blocks[@]} marked blocks in rendezvous/README.md: $(for b in "${blocks[@]}"; do basename "$b" .sh; done | tr '\n' ' ')"
[[ ${#blocks[@]} -ge 5 ]] || fail "too few marked blocks in the README"

docker network create --ipv6 --subnet 11.99.0.0/24 --subnet 2001:2:0:99::/64 "$net" >/dev/null
# A plain ubuntu:24.04: everything on it comes from the README's blocks.
docker run -d --name "$server" --network "$net" --ip "$s4" --ip6 "$s6" ubuntu:24.04 sleep infinity >/dev/null

run_server_block() {
    # $1: the block file. As root, with the settings and the test addresses.
    { printf 'set -euo pipefail\ncd /root\n%s\n%s\n' "$test_env" "$test_settings"; cat "$1"; } \
        | docker exec -i "$server" bash -s
}

start_services() {
    # What systemd would run, read from the installed units: coturn as its
    # packaged unit and drop-in run it, the service and the WebSocket relay
    # as their units run them (LoadCredential= done by hand, nobody standing
    # in for DynamicUser=).
    docker exec -i "$server" bash -s <<'EOS'
set -euo pipefail
cd /
running() { for p in /proc/[0-9]*; do [[ "$(cat "$p/comm" 2>/dev/null)" == "$1" ]] && return 0; done; return 1; }
running_module() {
    local p
    for p in /proc/[0-9]*; do
        tr '\0' ' ' < "$p/cmdline" 2>/dev/null | grep -q -- "-m $1 " && return 0
    done
    return 1
}
if ! running turnserver; then
    grep -qx 'AmbientCapabilities=CAP_NET_BIND_SERVICE' /etc/systemd/system/coturn.service.d/nereus.conf
    exec_start="$(sed -n 's/^ExecStart=//p' /usr/lib/systemd/system/coturn.service)"
    # shellcheck disable=SC2086
    setsid setpriv --reuid=turnserver --regid=turnserver --init-groups \
        --inh-caps=-all,+net_bind_service --ambient-caps=-all,+net_bind_service \
        --bounding-set=-all,+net_bind_service $exec_start </dev/null >/dev/null 2>&1 &
    sleep 2
    running turnserver
fi
# $1: the unit's name; $2: its module; $3: the line its log shows once it
# listens.
start_unit() {
    local unit="/etc/systemd/system/$1.service" cred src line exec_start
    if [[ ! -f "/opt/nereus-rendezvous/$2/__main__.py" ]] || running_module "$2"; then
        return 0
    fi
    install -d -m 0755 "/run/credentials/$1.service"
    while IFS=: read -r cred src; do
        install -m 0400 -o nobody "$src" "/run/credentials/$1.service/${cred}"
    done < <(sed -n 's/^LoadCredential=//p' "$unit")
    envs=()
    while IFS= read -r line; do envs+=("$line"); done < <(sed -n 's/^Environment=//p' "$unit")
    exec_start="$(sed -n 's/^ExecStart=//p' "$unit")"
    # RuntimeDirectory= and SupplementaryGroups=, as systemd would.
    local runtime extra groups_arg=--clear-groups
    runtime="$(sed -n 's/^RuntimeDirectory=//p' "$unit")"
    if [[ -n "$runtime" ]]; then
        install -d -o nobody -m "$(sed -n 's/^RuntimeDirectoryMode=//p' "$unit")" "/run/${runtime}"
    fi
    extra="$(sed -n 's/^SupplementaryGroups=//p' "$unit")"
    if [[ -n "$extra" ]]; then
        groups_arg="--groups=${extra// /,}"
    fi
    # shellcheck disable=SC2086
    setsid env "${envs[@]}" setpriv --reuid=nobody --regid=nogroup "$groups_arg" $exec_start </dev/null >"/run/$1.log" 2>&1 &
    for _ in $(seq 50); do grep -q "$3" "/run/$1.log" && break; sleep 0.2; done
    grep -q "$3" "/run/$1.log" || { cat "/run/$1.log" >&2; exit 1; }
}
start_unit nereus-rendezvous nereus_rendezvous 'listening on 2 addresses, relay on, relay grants on'
start_unit nereus-relay nereus_relay 'listening on its Unix socket, 16 slots'

EOS
}

start_caddy() {
    # Test only: Caddy's local authority instead of Let's Encrypt.
    docker exec -i "$server" bash -s <<'EOS'
set -euo pipefail
awk 'BEGIN { done = 0 } { print } !done && $0 == "{" { print "\tlocal_certs"; print "\tskip_install_trust"; done = 1 }' \
    /etc/caddy/Caddyfile > /run/Caddyfile.test
cd /
if caddy reload --config /run/Caddyfile.test --adapter caddyfile >/dev/null 2>&1; then
    exit 0
fi
caddy start --config /run/Caddyfile.test --adapter caddyfile >/run/caddy.log 2>&1
EOS
}

for block in "${blocks[@]}"; do
    name="$(basename "$block" .sh)"
    kind="${name#*-}"
    case "$kind" in
        settings)
            for var in RV_HOST RV_RELAY_HOST4 RV_RELAY_HOST6 RV_RELAY_SLOTS RV_TRANSFER_GB_PER_MONTH RV_DEPLOY_KEY; do
                grep -q "^export ${var}=" "$block" || fail "the settings block does not set ${var}"
            done
            pass "${name}: sets RV_HOST, RV_RELAY_HOST4/6, RV_RELAY_SLOTS, RV_TRANSFER_GB_PER_MONTH and RV_DEPLOY_KEY (test values used instead)"
            ;;
        copy)
            grep -q 'rendezvous root@.*:/root/rendezvous$' "$block" || fail "the copy block does not copy rendezvous to /root/rendezvous"
            docker cp "${repo}/rendezvous" "${server}:/root/rendezvous"
            pass "${name}: rendezvous/ copied to /root/rendezvous (docker cp for scp)"
            ;;
        server)
            out="$(run_server_block "$block" 2>&1)" || { printf '%s\n' "$out" | tail -30 >&2; fail "${name} failed"; }
            if grep -q 'setup-server.sh$' "$block"; then
                printf '%s\n' "$out" | grep -E 'relay: |data-use report|systemd is not running' | sed 's/^/# /'
            fi
            pass "${name}: run as written ($(grep -c . "$block") lines)"
            ;;
        deploy)
            mkdir -p "${work}/code"
            (cd "$repo" && sed "s|NEREUS_RV_TARGET=[^ ]*|NEREUS_RV_TARGET=${work}/code/|" "$block" | bash -euo pipefail) >"${work}/deploy.log" 2>&1 \
                || { cat "${work}/deploy.log" >&2; fail "${name} failed"; }
            docker cp "${work}/code/." "${server}:/opt/nereus-rendezvous/"
            docker exec "$server" chown -R nereusrv:nereusrv /opt/nereus-rendezvous
            pass "${name}: deploy.sh (dry run, then real) published the code, copied to /opt/nereus-rendezvous"
            ;;
        service)
            start_caddy || fail "${name}: Caddy did not start"
            grep -q 'systemctl restart nereus-rendezvous nereus-relay' "$block" \
                || fail "${name}: the block does not restart both the service and the relay"
            start_services || fail "${name}: coturn, the service or the relay did not start"
            pass "${name}: Caddy (with the Caddyfile setup-server.sh installed, local certificates for the test), coturn, the service and the WebSocket relay started as their units start them"
            ;;
        *) fail "unknown block kind ${kind}" ;;
    esac
done

docker exec "$server" cat /home/nereusrv/.ssh/authorized_keys | cmp -s - "${work}/deploy.pub" \
    || fail "the deploy account's authorized_keys is not exactly the given key"
docker exec "$server" grep -qx 'rv.test {' /etc/caddy/Caddyfile || fail "the installed Caddyfile does not name rv.test"
pass "setup-server.sh made the deploy account nereusrv with exactly the given key, and installed the Caddyfile for rv.test"

# --------------------------------------------------------------- use it
# A client on the same network, with the names in its hosts file as DNS
# would give them: rv both families, rv4 IPv4 only, rv6 IPv6 only. It runs
# coturn's test tools and the Python client code from the repository.
docker build -q -t nereus-rv-coturn:24.04 - >/dev/null <<'EOF'
FROM ubuntu:24.04
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      coturn python3 python3-websockets python3-cryptography python3-pytest \
 && rm -rf /var/lib/apt/lists/*
EOF
docker run -d --name "$client" --network "$net" --ip "$c4" --ip6 "$c6" \
    --add-host "rv.test:${s4}" --add-host "rv4.test:${s4}" --add-host "rv6.test:${s6}" \
    -v "${repo}:/repo:ro" nereus-rv-coturn:24.04 sleep infinity >/dev/null
docker exec "$server" cat /root/.local/share/caddy/pki/authorities/local/root.crt > "${work}/ca.crt"
docker cp "${work}/ca.crt" "${client}:/tmp/ca.crt"
docker exec -d "$client" turnutils_peer -L "$c4" -L "$c6" -p 3480
sleep 1

minted="$(docker exec -i -e SSL_CERT_FILE=/tmp/ca.crt "$client" bash -c \
    'cd /repo/rendezvous/tests && PYTHONPATH=/repo/rendezvous/server:/repo/rendezvous/tests PYTHONDONTWRITEBYTECODE=1 python3 -' <<'PY'
import asyncio, json
from nereus_rendezvous import protocol
from helpers import register, recv_json, introduce_message
from runner import ws_connect
from relay_helpers import connect, recv
async def meet(uri):
    st, _, sid = await register(uri)
    cl = await ws_connect(uri, "192.0.2.99")
    hello = await recv_json(cl)
    assert hello["stun"] == ["stun:rv4.test:3478", "stun:rv6.test:3478"], hello["stun"]
    await cl.send(protocol.encode(introduce_message(sid, hello["nonce"])))
    intro = await recv_json(st)
    await st.send(protocol.encode({"type": "answer", "to": intro["from"], "answer": "v=0\r\n", "turn": True}))
    answer = await recv_json(cl)
    device_grant = await recv_json(cl)
    assert answer["turn"] == (await recv_json(st))["turn"]
    core_grant = await recv_json(st)
    # The WebSocket relay: each end joins the URL its grant names, through
    # Caddy, and a datagram crosses.
    assert core_grant["url"] == device_grant["url"] == "wss://rv.test/v1/relay", core_grant
    core = await connect(core_grant["url"], "192.0.2.98")
    await core.send(b"\x80" + core_grant["token"].encode())
    assert await recv(core) == b"\x81\x01\x00"
    device = await connect(device_grant["url"], "192.0.2.99")
    await device.send(b"\x80" + device_grant["token"].encode())
    assert await recv(device) == b"\x81\x01\x01"
    assert await recv(core) == b"\x82\x01"
    await device.send(b"\x02" + bytes(1500))
    assert await recv(core) == b"\x02" + bytes(1500)
    await core.close()
    await device.close()
    print(json.dumps(answer["turn"]))
async def go():
    # Two Cores, the four relay URLs tried two per Core: the relays of each
    # Core stay allocated until they time out, well within user-quota 8.
    for _ in range(2):
        await meet("wss://rv.test/")
asyncio.run(go())
PY
)" || fail "a Core and a client could not meet through wss://rv.test/"
pass "wss://rv.test/ through Caddy: a Core registers, a client is introduced, the answer carries relay credentials, and the relay grants to both ends join one session at wss://rv.test/v1/relay, where a datagram crosses"

first="$(sed -n 1p <<<"$minted")"
second="$(sed -n 2p <<<"$minted")"
# (A loop, not mapfile: macOS still ships bash 3.2.)
urls=()
while IFS= read -r url; do
    urls+=("$url")
done < <(python3 -c 'import json,sys; print("\n".join(json.loads(sys.argv[1])["urls"]))' "$first")
[[ "${urls[*]}" == "turn:rv4.test:3478?transport=udp turn:rv4.test:443?transport=udp turn:rv6.test:3478?transport=udp turn:rv6.test:443?transport=udp" ]] \
    || fail "unexpected relay URLs: ${urls[*]}"
for url in "${urls[@]}"; do
    hostport="${url#turn:}"
    hostport="${hostport%%\?*}"
    host="${hostport%:*}" port="${hostport##*:}"
    out="$(docker exec "$client" python3 /repo/rendezvous/tests/turn_probe.py binding "$host" "$port")" \
        || fail "no STUN answer from ${host} UDP ${port}"
    if [[ "$host" == rv4.* ]]; then
        peer="${c4}:3480" creds="$second"
    else
        peer="[${c6}]:3480" creds="$first"
    fi
    user="$(python3 -c 'import json,sys; print(json.loads(sys.argv[1])["username"])' "$creds")"
    password="$(python3 -c 'import json,sys; print(json.loads(sys.argv[1])["password"])' "$creds")"
    out="$(docker exec "$client" python3 /repo/rendezvous/tests/turn_probe.py allocate "$host" "$port" \
        --username "$user" --password "$password" --peer "$peer" 2>&1 || true)"
    grep -q '"relayed": true' <<<"$out" || fail "no relay through ${host} UDP ${port}: ${out}"
done
pass "every relay URL the service hands out answers STUN and relays with its credentials: rv4 and rv6, UDP 3478 and 443"

echo "readme-check: all ${passed} checks passed"
