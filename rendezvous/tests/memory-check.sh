#!/usr/bin/env bash
#
# memory-check.sh: what the service and Caddy hold in memory at the sizing
# of the rendezvous document section 9.1, measured, in Docker.
#
# A server container (ubuntu:24.04, Caddy from Caddy's apt repository,
# Ubuntu's python3-websockets) runs the service on loopback behind Caddy
# with rendezvous/deploy/Caddyfile (local certificates for the test), as
# on the rendezvous server, and GOMEMLIMIT as setup-server.sh sets it for
# MEMORY_MB of memory. A client container runs memory_probe.py through
# wss://rv.nereussdr.com/ and, after each step, the server container's
# own cgroup is read: the service's and Caddy's resident memory, the
# container's memory, the host's TCP socket memory (/proc/net/sockstat;
# Docker Desktop's kernel does not charge sockets to a container's
# cgroup) and the bytes waiting in the service's own sockets.
#
# Steps, each on top of the one before:
#   baseline   nothing connected
#   stations   STATIONS registered stations, idle
#   clients    CLIENTS clients past hello, idle (the default pool, 256)
#   partial    PARTIAL more clients, each holding all but one byte of a
#              131072-byte message (the most one connection can make the
#              service hold on the way in)
#   stalled    STALLED of the stations stop reading while introductions
#              carrying a 60000-byte offer are sent to them (3 clients each)
#
# It prints one line a step and the differences per connection. It checks
# nothing; the numbers go into the rendezvous document section 9.1.
#
# Usage: rendezvous/tests/memory-check.sh
#   environment: STATIONS (2000) CLIENTS (256) PARTIAL (500) STALLED (100)
#                MEMORY_MB (1024: Caddy's GOMEMLIMIT as setup-server.sh
#                works it out) SOCKET_BUFFER_BYTES (the service's default)

set -euo pipefail

repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
readonly repo
readonly image="nereus-rv-caddy:24.04"
readonly tag="$$"
readonly net="nereus-rv-mem-${tag}"
readonly server="nereus-rv-mem-server-${tag}"
readonly client="nereus-rv-mem-client-${tag}"
stations="${STATIONS:-2000}"
clients="${CLIENTS:-256}"
memory_mb="${MEMORY_MB:-1024}"
# setup-server.sh: (memory - 256) x 50%.
gomemlimit="$(( (memory_mb - 256) * 50 / 100 ))MiB"
partial="${PARTIAL:-500}"
stalled="${STALLED:-100}"
buffer_line=""
if [[ -n "${SOCKET_BUFFER_BYTES:-}" ]]; then
    buffer_line="socket_buffer_bytes = ${SOCKET_BUFFER_BYTES}"
fi

work="$(mktemp -d "${TMPDIR:-/tmp}/nereus-memory-check.XXXXXX")"
chmod 777 "$work"
cleanup() {
    docker rm -f "$server" "$client" >/dev/null 2>&1 || true
    docker network rm "$net" >/dev/null 2>&1 || true
    rm -rf -- "$work"
}
trap cleanup EXIT

command -v docker >/dev/null 2>&1 || { echo "docker is not installed" >&2; exit 1; }

# The same image caddy-check.sh builds.
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

docker network create "$net" >/dev/null
docker run -d --name "$server" --network "$net" --ulimit nofile=8192:8192 \
    -v "${repo}:/repo:ro" -v "${work}:/sync" "$image" sleep infinity >/dev/null
server_ip="$(docker inspect -f "{{(index .NetworkSettings.Networks \"${net}\").IPAddress}}" "$server")"
docker run -d --name "$client" --network "$net" --ulimit nofile=65536:65536 \
    --add-host "rv.nereussdr.com:${server_ip}" \
    -v "${repo}:/repo:ro" -v "${work}:/sync" "$image" sleep infinity >/dev/null

# The service, as nobody with the unit's LimitNOFILE (runuser would reset
# the open-file limit to 1024), with the defaults except the limits this load would meet
# first (every client comes from one address, and they wait far longer
# than a real one); the rv site from the real Caddyfile.
docker exec -i "$server" bash -s -- "$buffer_line" "$gomemlimit" <<'EOS'
set -euo pipefail
mkdir -p /run/rv && chmod 755 /run/rv
python3 -c 'import secrets; print(secrets.token_hex(32))' > /run/rv/secret
chown nobody /run/rv/secret && chmod 600 /run/rv/secret
cat > /run/rv/rv.conf <<CONF
[rendezvous]
listen = 127.0.0.1:8710 [::1]:8710
turn_secret_file = /run/rv/secret
[limits]
connections_per_address = 100000
stations_per_address = 100000
max_connections = 100000
max_stations = 100000
introductions_per_address_per_minute = 100000
introductions_per_station_per_minute = 100000
handshake_timeout_ms = 600000
idle_timeout_ms = 600000
introduction_lifetime_ms = 600000
send_stall_ms = 600000
$1
CONF
cd / && PYTHONPATH=/repo/rendezvous/server PYTHONDONTWRITEBYTECODE=1 \
    setsid setpriv --reuid=nobody --regid=nogroup --clear-groups python3 -m nereus_rendezvous --config /run/rv/rv.conf \
    > /run/rv/log 2>&1 < /dev/null &
for _ in $(seq 50); do grep -q "listening on 2 addresses" /run/rv/log 2>/dev/null && break; sleep 0.2; done
awk 'BEGIN { done = 0 } { print } !done && $0 == "{" { print "\tlocal_certs"; print "\tskip_install_trust"; done = 1 }' \
    /repo/rendezvous/deploy/Caddyfile > /tmp/rv.Caddyfile
GOMEMLIMIT="$2" caddy start --config /tmp/rv.Caddyfile --adapter caddyfile >/tmp/caddy.log 2>&1
for _ in $(seq 50); do (exec 3<>/dev/tcp/127.0.0.1/443) 2>/dev/null && break; sleep 0.2; done
sleep 1
cp /root/.local/share/caddy/pki/authorities/local/root.crt /sync/ca.crt
chmod 644 /sync/ca.crt
EOS

# The server side: measures on each step file the client writes.
docker exec -d "$server" bash -c '
measure() {
    python3 - "$1" <<"PY"
import json, os, sys
def rss(comm, arg=""):
    for pid in filter(str.isdigit, os.listdir("/proc")):
        try:
            name = open("/proc/%s/comm" % pid).read().strip()
            cmd = open("/proc/%s/cmdline" % pid, "rb").read()
        except OSError:
            continue
        if name == comm and arg.encode() in cmd:
            for line in open("/proc/%s/status" % pid):
                if line.startswith("VmRSS:"):
                    return int(line.split()[1])
    return 0
stat = dict(l.split() for l in open("/sys/fs/cgroup/memory.stat"))
# The kernel memory of every TCP socket on the host (the Docker VM, where
# little else runs), in pages, and the bytes waiting in the own sockets of
# the service (local port 8710), both ways.
tcp_pages = int(open("/proc/net/sockstat").read().split("TCP:")[1].split("mem")[1].split()[0])
queued = 0
for name in ("tcp", "tcp6"):
    for line in open("/proc/net/" + name).read().splitlines()[1:]:
        f = line.split()
        if int(f[1].rsplit(":", 1)[1], 16) == 8710:
            tx, rx = f[4].split(":")
            queued += int(tx, 16) + int(rx, 16)
print(json.dumps({
    "step": sys.argv[1],
    "service_rss_kib": rss("python3", "nereus_rendezvous"),
    "caddy_rss_kib": rss("caddy"),
    "tcp_mem_kib": tcp_pages * 4,
    "service_queued_kib": queued // 1024,
    "anon_kib": int(stat["anon"]) // 1024,
    "cgroup_kib": int(open("/sys/fs/cgroup/memory.current").read()) // 1024,
}))
PY
}
for step in baseline stations clients partial stalled; do
    while [[ ! -e /sync/$step ]]; do sleep 0.2; done
    sleep 3
    measure "$step" >> /sync/measured.txt
    touch /sync/$step.done
done'

echo "# ${stations} stations, ${clients} clients, ${partial} unfinished messages, ${stalled} stalled stations; ${buffer_line:-socket_buffer_bytes default}; Caddy GOMEMLIMIT ${gomemlimit} (${memory_mb} MiB)"
docker exec "$client" python3 /repo/rendezvous/tests/memory_probe.py wss://rv.nereussdr.com/ \
    --sync /sync --cacert /sync/ca.crt --stations "$stations" --clients "$clients" \
    --partial "$partial" --stalled "$stalled" | sed 's/^/# probe: /'
for _ in $(seq 50); do [[ -e "${work}/stalled.done" ]] && break; sleep 0.2; done
cat "${work}/measured.txt"
python3 - "${work}/measured.txt" "$stations" "$clients" "$partial" "$stalled" <<'PY'
import json, sys
m = {j["step"]: j for j in map(json.loads, open(sys.argv[1]))}
stations, clients, partial, stalled = map(int, sys.argv[2:6])
def per(a, b, n, key):
    return (m[b][key] - m[a][key]) / n
for a, b, n, what in (("baseline", "stations", stations, "idle station"),
                      ("stations", "clients", clients, "idle client"),
                      ("clients", "partial", partial, "unfinished message"),
                      ("partial", "stalled", stalled, "stalled station (and its 3 clients)")):
    print("per %s: service %.1f KiB, Caddy %.1f KiB, host TCP memory %.1f KiB, queued in the service's sockets %.1f KiB" % (
        what, per(a, b, n, "service_rss_kib"), per(a, b, n, "caddy_rss_kib"), per(a, b, n, "tcp_mem_kib"),
        per(a, b, n, "service_queued_kib")))
print("baseline: service %d KiB, Caddy %d KiB" % (m["baseline"]["service_rss_kib"], m["baseline"]["caddy_rss_kib"]))
for step in ("clients", "partial", "stalled"):
    print("after %s: service %d KiB, Caddy %d KiB, host TCP memory %d KiB, server container %d KiB" % (
        step, m[step]["service_rss_kib"], m[step]["caddy_rss_kib"], m[step]["tcp_mem_kib"], m[step]["cgroup_kib"]))
PY
echo "memory-check: done"
