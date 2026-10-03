#!/usr/bin/env bash
#
# caddy-check.sh: the rendezvous server's own Caddyfile, in Docker.
#
# In ubuntu:24.04 with Caddy from Caddy's official apt repository (the
# same source rendezvous/deploy/setup-server.sh installs from), with the
# service behind Caddy as on the rendezvous server:
#
#   1. caddy validate passes on the real rendezvous/deploy/Caddyfile, with
#      no warning (it is as caddy fmt writes it, and has no header_up Caddy
#      calls unnecessary), and on the test copy with local certificates.
#   2. A WebSocket to rv.nereussdr.com upgrades and reaches the service by
#      host name. Written byte for byte, both the usual request (Upgrade:
#      websocket, Host without a port) and Apple's Network.framework's
#      (Upgrade: WebSocket, Host with :443) upgrade.
#   3. The service sees each client's own address, whatever X-Forwarded-For
#      the client sends: with two connections allowed per address, a third
#      from one client is refused while another client, over IPv4 or IPv6,
#      is still let in.
#   4. A plain request to rv gets the service's short plain answer (426)
#      and the headers, "Upgrade: websocket" and "Connection: upgrade"
#      included over HTTP/1.1.
#   5. What an HTTP/2 client gets: a plain GET, and whether Caddy offers
#      WebSockets over HTTP/2 (RFC 8441) and what one gets.
#   6. Caddy holds no UDP port (HTTP/3 stays off: UDP 443 is coturn's).
#   7. wss://rv.nereussdr.com/v1/relay reaches the WebSocket relay
#      (nereus-relay on its Unix socket, which only Caddy's group may
#      open; rendezvous document section 12): a
#      Core leg and a device leg of one grant join, datagrams up to the
#      1500-byte cap cross both ways unchanged, the relay counts legs by
#      the client's own address, and a plain request there gets 426.
#   8. A Core leg reading slowly through Caddy: where its datagrams wait,
#      with net.ipv4.tcp_notsent_lowat as deploy/sysctl.conf sets it
#      (NOTSENT_LOWAT=off measures without it).
#   9. What reaches each upstream in X-Forwarded-For: with the service and
#      the relay swapped for a program that echoes the header, exactly one
#      entry, the client's own address, over IPv4 and IPv6, at / and at
#      /v1/relay, whatever X-Forwarded-For the client sent.
#
# Certificates come from Caddy's own local authority, switched on for the
# test only (local_certs in a copy of the Caddyfile); the real file is
# validated unchanged. Nothing leaves the containers but the apt downloads
# when the image is first built.
#
# Usage: rendezvous/tests/caddy-check.sh      (needs Docker)

set -euo pipefail

repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
readonly repo
readonly image="nereus-rv-caddy:24.04"
readonly tag="$$"
readonly net="nereus-rv-caddy-${tag}"
readonly server="nereus-rv-caddy-server-${tag}"
readonly client_a="nereus-rv-caddy-a-${tag}"
readonly client_b="nereus-rv-caddy-b-${tag}"
readonly client_c="nereus-rv-caddy-c-${tag}"
readonly v4_net="198.51.100.0/24" v6_net="2001:db8:c0::/64"
readonly server_v4="198.51.100.10" server_v6="2001:db8:c0::10"

passed=0
pass() { passed=$((passed + 1)); printf 'ok %d - %s\n' "$passed" "$*"; }
fail() { printf 'not ok - %s\n' "$*" >&2; exit 1; }

work="$(mktemp -d "${TMPDIR:-/tmp}/nereus-caddy-check.XXXXXX")"
cleanup() {
    docker rm -f "$server" "$client_a" "$client_b" "$client_c" >/dev/null 2>&1 || true
    docker network rm "$net" >/dev/null 2>&1 || true
    rm -rf -- "$work"
}
trap cleanup EXIT

command -v docker >/dev/null 2>&1 || fail "docker is not installed"

echo "# building ${image} (ubuntu:24.04, Caddy from Caddy's apt repository)"
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
      caddy python3 python3-websockets python3-cryptography \
 && rm -rf /var/lib/apt/lists/*
EOF
echo "# $(docker run --rm "$image" caddy version)"

docker network create --ipv6 --subnet "$v4_net" --subnet "$v6_net" "$net" >/dev/null
# The server's sysctl as setup-server.sh installs it (deploy/sysctl.conf),
# or NOTSENT_LOWAT=off to measure without it.
sysctl_args=()
if [[ "${NOTSENT_LOWAT:-}" != off ]]; then
    lowat_setting="$(sed -n 's/^net.ipv4.tcp_notsent_lowat = \([0-9]*\)$/\1/p' "${repo}/rendezvous/deploy/sysctl.conf")"
    [[ -n "$lowat_setting" ]] || fail "rendezvous/deploy/sysctl.conf sets no net.ipv4.tcp_notsent_lowat"
    sysctl_args=(--sysctl "net.ipv4.tcp_notsent_lowat=${lowat_setting}")
fi
docker run -d --name "$server" --network "$net" --ip "$server_v4" --ip6 "$server_v6" ${sysctl_args[@]+"${sysctl_args[@]}"} \
    -v "${repo}:/repo:ro" "$image" sleep infinity >/dev/null

# 1. The real file, unchanged, and the test copy with local certificates
# switched on in the global block (after its opening line, the first line
# that is only "{").
out="$(docker exec "$server" caddy validate --config /repo/rendezvous/deploy/Caddyfile --adapter caddyfile 2>&1)" \
    || { printf '%s\n' "$out" >&2; fail "caddy validate refused rendezvous/deploy/Caddyfile"; }
grep -q 'Valid configuration' <<<"$out" || { printf '%s\n' "$out" >&2; fail "no 'Valid configuration' from caddy validate"; }
if grep -q '"level":"warn"' <<<"$out"; then
    printf '%s\n' "$out" >&2
    fail "caddy validate warns about rendezvous/deploy/Caddyfile (above)"
fi
docker exec -i "$server" bash -s <<'EOS' || fail "the test copy with local certificates does not validate"
set -euo pipefail
awk 'BEGIN { done = 0 } { print } !done && $0 == "{" { print "\tlocal_certs"; print "\tskip_install_trust"; done = 1 }' \
    /repo/rendezvous/deploy/Caddyfile > /tmp/rv.Caddyfile
caddy validate --config /tmp/rv.Caddyfile --adapter caddyfile >/dev/null 2>&1
EOS
pass "caddy validate rendezvous/deploy/Caddyfile: Valid configuration, no warning (and the test copy with local certificates)"

# The service, as the unprivileged nobody, on loopback as in production,
# with two connections allowed per address for step 4.
docker exec -i "$server" bash -s <<'EOS'
set -euo pipefail
mkdir -p /run/rv && chmod 755 /run/rv
python3 -c 'import secrets; print(secrets.token_hex(32))' > /run/rv/secret
chown nobody /run/rv/secret && chmod 600 /run/rv/secret
cat > /run/rv/rv.conf <<'CONF'
[rendezvous]
listen = 127.0.0.1:8710 [::1]:8710
turn_secret_file = /run/rv/secret
[limits]
connections_per_address = 2
CONF
cd / && PYTHONPATH=/repo/rendezvous/server PYTHONDONTWRITEBYTECODE=1 \
    setsid runuser -u nobody -- python3 -m nereus_rendezvous --config /run/rv/rv.conf \
    > /run/rv/log 2>&1 < /dev/null &
for _ in $(seq 50); do
    grep -q "listening on 2 addresses" /run/rv/log 2>/dev/null && exit 0
    sleep 0.2
done
cat /run/rv/log >&2
exit 1
EOS
pass "the service listens on 127.0.0.1:8710 and [::1]:8710"

# The WebSocket relay, as nobody with the caddy group, on its Unix socket
# as relay.conf on the server has it, with two connections allowed per
# address for step 7.
docker exec -i "$server" bash -s <<'EOS'
set -euo pipefail
python3 -c 'import secrets; print(secrets.token_hex(32))' > /run/rv/relay-secret
chown nobody /run/rv/relay-secret && chmod 600 /run/rv/relay-secret
cp /run/rv/relay-secret /run/rv/relay-secret.probe && chmod 644 /run/rv/relay-secret.probe
cat > /run/rv/relay.conf <<'CONF'
[relay]
socket = /run/nereus-relay/relay.sock
socket_mode = 0660
socket_group = caddy
relay_secret_file = /run/rv/relay-secret
[limits]
connections_per_address = 2
CONF
install -d -o nobody -m 0755 /run/nereus-relay
cd / && PYTHONPATH=/repo/rendezvous/server PYTHONDONTWRITEBYTECODE=1 \
    setsid setpriv --reuid=nobody --regid=nogroup --groups=caddy python3 -m nereus_relay --config /run/rv/relay.conf \
    > /run/rv/relay.log 2>&1 < /dev/null &
for _ in $(seq 50); do
    grep -q "listening on its Unix socket" /run/rv/relay.log 2>/dev/null && exit 0
    sleep 0.2
done
cat /run/rv/relay.log >&2
exit 1
EOS
docker exec -i "$server" bash -s <<'EOS' || fail "the relay's socket is not as relay.conf has it, or someone outside Caddy's group can open it"
set -euo pipefail
[[ "$(stat -c '%F %a %U %G' /run/nereus-relay/relay.sock)" == "socket 660 nobody caddy" ]]
open_as() {
    runuser -u "$1" -- python3 -c 'import socket, sys
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
try:
    s.connect("/run/nereus-relay/relay.sock")
except PermissionError:
    sys.exit(3)'
}
open_as caddy
status=0; open_as daemon || status=$?
[[ "$status" -eq 3 ]]
EOS
pass "the WebSocket relay listens on its Unix socket /run/nereus-relay/relay.sock (socket 660, group caddy): the caddy account may open it, another account may not"

caddy_run() {
    # $1: start or reload; $2: the config.
    docker exec "$server" bash -c "caddy $1 --config $2 --adapter caddyfile >/tmp/caddy-$1.log 2>&1" \
        || { docker exec "$server" cat "/tmp/caddy-$1.log" >&2; fail "caddy $1 $2"; }
    for _ in $(seq 50); do
        if docker exec "$server" bash -c "exec 3<>/dev/tcp/127.0.0.1/443" 2>/dev/null; then
            return
        fi
        sleep 0.2
    done
    fail "caddy does not listen on 443"
}

hosts=(--add-host "rv.nereussdr.com:${server_v4}")
docker run -d --name "$client_a" --network "$net" --ip 198.51.100.30 "${hosts[@]}" \
    -v "${repo}:/repo:ro" "$image" sleep infinity >/dev/null
docker run -d --name "$client_b" --network "$net" --ip 198.51.100.31 "${hosts[@]}" \
    -v "${repo}:/repo:ro" "$image" sleep infinity >/dev/null
docker run -d --name "$client_c" --network "$net" --ip6 2001:db8:c0::32 \
    --add-host "rv.nereussdr.com:${server_v6}" \
    -v "${repo}:/repo:ro" "$image" sleep infinity >/dev/null

caddy_run start /tmp/rv.Caddyfile
docker exec "$server" cat /root/.local/share/caddy/pki/authorities/local/root.crt > "${work}/ca.crt"
for c in "$client_a" "$client_b" "$client_c"; do
    docker cp "${work}/ca.crt" "${c}:/tmp/ca.crt"
done

# 2 and 3. WebSockets by host name, through Caddy, from three clients.
docker exec "$client_a" python3 /repo/rendezvous/tests/caddy_probe.py open wss://rv.nereussdr.com/ \
    --cacert /tmp/ca.crt --count 3 --hold 6 > "${work}/a.txt" &
probe_a=$!
sleep 3
docker exec "$client_b" python3 /repo/rendezvous/tests/caddy_probe.py open wss://rv.nereussdr.com/ \
    --cacert /tmp/ca.crt --count 1 > "${work}/b.txt"
docker exec "$client_c" python3 /repo/rendezvous/tests/caddy_probe.py open wss://rv.nereussdr.com/ \
    --cacert /tmp/ca.crt --count 1 > "${work}/c.txt"
wait "$probe_a"
cat "${work}/a.txt" "${work}/b.txt" "${work}/c.txt" | sed 's/^/# /'
python3 - "${work}/a.txt" "${work}/b.txt" "${work}/c.txt" <<'PY' || fail "the WebSocket results above are not the expected ones"
import json, sys
a, b, c = ([json.loads(l) for l in open(p)] for p in sys.argv[1:4])
assert [x["first"] for x in a[:2]] == ["hello", "hello"], a
assert a[2]["upgraded"] and a[2]["first"] == "error" and a[2]["code"] == "tooManyConnections", a
assert b[0]["upgraded"] and b[0]["first"] == "hello", b
assert c[0]["upgraded"] and c[0]["first"] == "hello", c
PY
pass "wss://rv.nereussdr.com/ upgrades and reaches the service (hello)"
pass "the service counts each client by its own address: a third connection from one client refused (tooManyConnections) whatever X-Forwarded-For it sent, while another IPv4 client and an IPv6 client were let in"

# 2, byte for byte: the usual request, and Apple's.
raws="$(for pair in "websocket rv.nereussdr.com" "WebSocket rv.nereussdr.com:443"; do
    read -r upgrade host_header <<<"$pair"
    docker exec "$client_a" python3 /repo/rendezvous/tests/caddy_probe.py raw rv.nereussdr.com \
        --cacert /tmp/ca.crt --upgrade "$upgrade" --host-header "$host_header"
done)"
printf '%s\n' "$raws" | sed 's/^/# /'
python3 - <<PY || fail "a WebSocket request written byte for byte did not upgrade"
import json
lines = [json.loads(l) for l in """${raws}""".splitlines()]
assert len(lines) == 2, lines
for l in lines:
    assert l["status"].startswith("HTTP/1.1 101") and l["hello"], l
PY
pass "byte for byte over HTTP/1.1, 'Upgrade: websocket' with 'Host: rv.nereussdr.com' and Apple's 'Upgrade: WebSocket' with 'Host: rv.nereussdr.com:443' both upgrade and get hello"

# 4. A plain request to rv, over HTTP/1.1 and over HTTP/2.
for version in http1.1 http2; do
    docker exec "$client_a" curl -sS "--${version}" --cacert /tmp/ca.crt -o /tmp/rv-body -D /tmp/rv-head https://rv.nereussdr.com/
    docker exec "$client_a" cat /tmp/rv-head /tmp/rv-body | tr -d '\r' > "${work}/rv-${version}.txt"
    sed "s/^/# ${version}: /" "${work}/rv-${version}.txt"
    grep -Eq '^HTTP/(1.1|2) 426' "${work}/rv-${version}.txt" || fail "a plain ${version} request to rv did not get 426"
    for h in 'strict-transport-security: max-age=31536000' 'x-content-type-options: nosniff' \
             'referrer-policy: no-referrer' 'x-frame-options: DENY' 'cache-control: no-store' \
             "content-security-policy: default-src 'none'; frame-ancestors 'none'" \
             'content-type: text/plain; charset=utf-8'; do
        grep -qixF "$h" "${work}/rv-${version}.txt" || fail "rv's ${version} answer lacks: ${h}"
    done
    if grep -Eqi '^(server|via):' "${work}/rv-${version}.txt"; then
        fail "rv's ${version} answer names the server or the proxy"
    fi
    grep -q 'NereusSDR connection service' "${work}/rv-${version}.txt" || fail "rv's ${version} answer lacks its text"
done
grep -qixF 'upgrade: websocket' "${work}/rv-http1.1.txt" || fail "rv's HTTP/1.1 426 lacks Upgrade: websocket"
grep -qixF 'connection: upgrade' "${work}/rv-http1.1.txt" || fail "rv's HTTP/1.1 426 lacks Connection: upgrade"
h2_upgrade="absent"
if grep -qi '^upgrade:' "${work}/rv-http2.txt"; then
    h2_upgrade="present"
fi
pass "a plain request to rv gets the service's 426, its short text and the security headers, no Server or Via header; over HTTP/1.1 with Upgrade: websocket and Connection: upgrade (over HTTP/2 the Upgrade header is ${h2_upgrade}: HTTP/2 has no Upgrade)"

# 5. What an HTTP/2 client gets.
h2="$(docker exec "$client_a" python3 /repo/rendezvous/tests/caddy_probe.py h2 rv.nereussdr.com --cacert /tmp/ca.crt)"
printf '%s\n' "$h2" | sed 's/^/# h2: /'
h2_summary="$(python3 - "$h2" <<'PY'
import json, sys
r = json.loads(sys.argv[1])
assert r["alpn"] == "h2", r
assert r["get"]["status"] == 426, r
assert r["serverSettings"], r  # the SETTINGS of the server were read
if not r["enableConnectProtocol"]:
    print("Caddy does not offer WebSockets over HTTP/2 (no SETTINGS_ENABLE_CONNECT_PROTOCOL), so an RFC 8441 client opens its WebSocket over HTTP/1.1")
elif r["connect"]["hello"]:
    print("Caddy offers WebSockets over HTTP/2 and an extended CONNECT reaches the service (hello)")
else:
    print("Caddy offers WebSockets over HTTP/2 but an extended CONNECT gets status %s, reset %s, and no hello" % (
        r["connect"]["status"], r["connect"]["reset"]))
PY
)" || fail "the HTTP/2 probe: ${h2}"
pass "HTTP/2: a plain GET gets 426; ${h2_summary}"

# 7. The WebSocket relay through Caddy.
docker cp "${server}:/run/rv/relay-secret.probe" "${work}/relay-secret"
docker cp "${work}/relay-secret" "${client_a}:/tmp/relay-secret"
relay_out="$(docker exec "$client_a" python3 /repo/rendezvous/tests/caddy_probe.py relay wss://rv.nereussdr.com/v1/relay \
    --cacert /tmp/ca.crt --secret-file /tmp/relay-secret --extra 1)"
printf '%s\n' "$relay_out" | sed 's/^/# relay: /'
python3 - "$relay_out" <<'PY' || fail "the relay through Caddy did not behave as expected"
import json, sys
r = json.loads(sys.argv[1])
assert r["coreReady"] == "810100" and r["deviceReady"] == "810101" and r["corePeer"] == "8201", r
assert r["crossed"] == 16, r
# Both legs came from this client, whatever X-Forwarded-For each claimed:
# a third connection from it is over the cap of two.
assert r["extra"] == ["tooManyConnections"], r
PY
docker exec "$client_a" curl -sS --http1.1 --cacert /tmp/ca.crt -o /tmp/relay-body -D /tmp/relay-head \
    https://rv.nereussdr.com/v1/relay
docker exec "$client_a" cat /tmp/relay-head | tr -d '\r' | grep -Eq '^HTTP/1.1 426' || fail "a plain request to /v1/relay did not get 426"
if docker exec "$server" grep -Eq "$(cat "${work}/relay-secret")|198\.51\.100\.30|203\.0\.113" /run/rv/relay.log; then
    fail "the relay's log holds its secret or an address"
fi
pass "wss://rv.nereussdr.com/v1/relay reaches the WebSocket relay through Caddy: a Core leg and a device leg of one grant join (ready, peer present), datagrams of 1 to 1500 bytes with both stream tags cross both ways unchanged, a third connection from the same client is refused whatever X-Forwarded-For it claims (tooManyConnections), a plain request gets 426, and the relay's log holds no secret or address"

# 8. Where delay builds up behind the relay's queue: a Core leg reading
# slowly through Caddy, measured (rendezvous document section 12.5).
slow_out="$(docker exec "$client_a" python3 /repo/rendezvous/tests/caddy_probe.py slowreader \
    wss://rv.nereussdr.com/v1/relay --cacert /tmp/ca.crt --secret-file /tmp/relay-secret)"
lowat="$(docker exec "$server" cat /proc/sys/net/ipv4/tcp_notsent_lowat)"
printf '%s\n' "$slow_out" | sed "s/^/# slow reader (tcp_notsent_lowat ${lowat}): /"
python3 - "$slow_out" <<'PY' || fail "the slow reader through Caddy"
import json, sys
r = json.loads(sys.argv[1])
assert r["inOrder"], r
# The relay dropped datagrams for the slow reader (its queue drops its
# oldest), so the delay stops growing instead of growing without bound.
assert r["droppedAmongWhatWasRead"] > 0, r
PY
pass "a Core leg reading a quarter of what is sent through Caddy (tcp_notsent_lowat ${lowat}): the relay drops its oldest datagrams, and how old they are when read levels off (the ages above, for section 12.5)"

# 6. HTTP/3 stays off.
udp="$(docker exec -i "$server" python3 - <<'PY'
import os
inodes = set()
for pid in filter(str.isdigit, os.listdir("/proc")):
    try:
        if open("/proc/%s/comm" % pid).read().strip() != "caddy":
            continue
        for fd in os.listdir("/proc/%s/fd" % pid):
            link = os.readlink("/proc/%s/fd/%s" % (pid, fd))
            if link.startswith("socket:["):
                inodes.add(link[8:-1])
    except OSError:
        continue
for name in ("udp", "udp6"):
    for line in open("/proc/net/" + name).read().splitlines()[1:]:
        if line.split()[9] in inodes:
            print(name, line.split()[1])
PY
)"
[[ -z "$udp" ]] || fail "caddy holds a UDP socket: ${udp}"
pass "caddy holds no UDP socket, so HTTP/3 is off"

# 9. X-Forwarded-For as each upstream receives it. The service and the
# relay trust only its last entry; Caddy must send exactly one, the
# client's own address, whatever the client sent. A program that echoes
# every X-Forwarded-For header it gets takes the service's loopback port
# and the relay's Unix socket.
docker exec -i "$server" bash -s <<'EOS' || fail "the X-Forwarded-For echo did not start"
set -euo pipefail
pkill -f 'python3 -m nereus_rendezvous' || true
pkill -f 'python3 -m nereus_relay' || true
for _ in $(seq 50); do
    pgrep -f 'python3 -m nereus_re' >/dev/null || break
    sleep 0.2
done
rm -f /run/nereus-relay/relay.sock
cat > /run/rv/xff-echo.py <<'ECHO'
import asyncio, json, os
SOCK = "/run/nereus-relay/relay.sock"
async def answer(reader, writer):
    head = (await reader.readuntil(b"\r\n\r\n")).decode("latin-1").split("\r\n")[1:]
    xff = [l.split(":", 1)[1].strip() for l in head if l.lower().startswith("x-forwarded-for:")]
    body = json.dumps({"xff": xff}).encode()
    writer.write(b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n"
                 b"Connection: close\r\n\r\n%s" % (len(body), body))
    await writer.drain()
    writer.close()
async def main():
    tcp = await asyncio.start_server(answer, "127.0.0.1", 8710)
    unix = await asyncio.start_unix_server(answer, SOCK)
    os.chmod(SOCK, 0o666)
    print("ready", flush=True)
    async with tcp, unix:
        await asyncio.Event().wait()
asyncio.run(main())
ECHO
cd / && setsid python3 /run/rv/xff-echo.py > /run/rv/xff-echo.log 2>&1 < /dev/null &
for _ in $(seq 50); do
    grep -q ready /run/rv/xff-echo.log 2>/dev/null && exit 0
    sleep 0.2
done
cat /run/rv/xff-echo.log >&2
exit 1
EOS
for pair in "${client_a} 198.51.100.30" "${client_c} 2001:db8:c0::32"; do
    read -r client want <<<"$pair"
    for path in / /v1/relay; do
        got="$(docker exec "$client" curl -sS --http1.1 --cacert /tmp/ca.crt \
            -H 'X-Forwarded-For: 203.0.113.7, 203.0.113.8' "https://rv.nereussdr.com${path}")"
        printf '# X-Forwarded-For from %s at %s: %s\n' "$want" "$path" "$got"
        python3 - "$got" "$want" <<'PY' || fail "X-Forwarded-For at ${path} from ${want} is not exactly the client's address: ${got}"
import json, sys
got, want = json.loads(sys.argv[1]), sys.argv[2]
assert got["xff"] == [want], got
PY
    done
done
pass "X-Forwarded-For reaches the service's port and the relay's socket as exactly one entry, the client's own address (IPv4 and IPv6), although each client sent two entries of its own"

echo "caddy-check: all ${passed} checks passed"
