#!/usr/bin/env bash
# no-port-check: NereusSDR-original.
# =================================================================
# tests/scripts/traversal-harness.sh  (NereusSDR)
# =================================================================
#
# iPhone app plan Task 27 (R-IOS-16): reaching a Core through the remote
# access service across real NATs, on one Linux machine, with nothing
# leaving it. Linux network namespaces and nftables stand up a small
# internet:
#
#   rvsrv   the service's host: the rendezvous (rendezvous/server) behind a
#           TLS terminator, coturn for STUN and TURN, and DNS (a plain
#           resolver, and DNS64 for the IPv6-only client)
#   inet    the router between everyone's public addresses
#   nats    the Core's NAT router          sta   the Core (station role)
#   natc    the client's NAT router        cli   the client
#   plat    a NAT64 router (IPv6 inside)   cli6  an IPv6-only client with a
#                                                CLAT (464XLAT)
#
# and runs tests/tools/nereus_rendezvous_peer in `sta` and in `cli`/`cli6`
# for each scenario, checking how the connection was made:
#
#   eim-nat            both behind endpoint-independent NAT, the relay
#                      denied: direct (hole punching, nothing else)
#   random-nat-both    port-randomising NAT at both ends: relayed
#   udp-direct-blocked UDP between the two ends' public addresses dropped:
#                      relayed
#   udp-blocked        all UDP from the client dropped but DNS: no
#                      connection, reported plainly and in time (the TCP
#                      floor is plan Task 29)
#   mtu-1100           datagrams over 1100 bytes dropped: connected, a
#                      60000-byte message echoed in 1000-byte datagrams
#   ipv6-only-nat64    an IPv6-only client behind NAT64 and DNS64 with a
#                      CLAT, the Core on IPv4: the service's name answered
#                      by DNS64, connected
#   netem-loss         2 % loss and 40 ms delay on both uplinks: connected
#                      (the display channel the echo rides never
#                      retransmits, so a 60000-byte echo is not required)
#   ipv6-both          routed IPv6 at both ends (behind their routers'
#                      stateful firewalls) as well as IPv4 NAT, the relay
#                      denied: direct, on an IPv6 pair
#
# and, plan Task 28, a whole session over the control connection the
# service introduces (a Core as nereusd runs it, `core`, and a desktop,
# `session`, StationClient::connectThroughService):
#
#   session-direct     both behind endpoint-independent NAT, the relay
#                      denied: the session runs, direct
#   session-relayed    UDP between the two ends' public addresses dropped:
#                      the session runs through the relay, and both ends
#                      give their allocations back when it ends
#   session-loss       2 % loss and 40 ms delay on both uplinks: the
#                      session's reliable channel still carries the whole
#                      connect sequence
#
# and, plan Task 29 Step 2a, the path race and moving a session (the Core
# also listens on its own address, forwarded at its router when the
# scenario says so), and real media:
#
#   race-direct        the Core's port forwarded: the session ends on the
#                      direct connection
#   race-service       no forward: the service's path without the relay
#   upgrade            starts through the relay; the forward opens while it
#                      runs and the session moves to the direct connection,
#                      signed in once
#   upgrade-media      the upgrade with the desktop's own media playing:
#                      audio and display follow onto a new media
#                      connection, no silent run over 40 ms
#   session-media      audio and display decoded through the service
#   session-media-relayed
#                      the same through the relay; all four allocations
#                      (control and media at each end) given back
#
# and, plan Task 29 step 2b, the web relay (the rendezvous document,
# section 12), run as on the server: its own process on a Unix socket
# behind a TLS front that routes /v1/relay to it (tests/tools/
# harness_front.py, which also counts the relay's datagrams and any
# holding the session's plaintext):
#
#   web-relay          the client's network passes only DNS and TCP 443: a
#                      session, audio and display over the web relay, both
#                      connections on one leg per end, no plaintext seen
#   web-relay-rejoin   the front restarts mid-session: both legs join again
#                      and the session carries on
#   web-relay-standby  every path open: the legs join, ICE takes another
#                      pair
#   direct-wss-media   direct TCP 443 works but UDP does not: media uses
#                      the negotiated UUID tunnel
#   web-to-direct-wss  media starts through the web relay, then the session
#                      moves to direct wss and replacement media follows
#   web-relay-deadline TUNE keyed over the web relay at 2 and 3 % loss and
#                      150 ms round trip: keepalive gaps and trips, logged
#   direct-wss-deadline the same TX deadline measurement over the direct
#                      media tunnel, with the Core's wss forwarded on 443
#
# Every secret (the TURN secret, the TLS key, both ends' keys) is made at
# run time in a temporary directory and removed with it. It never touches
# the live server: every name is under harness.test and every address is a
# documentation or private one, inside namespaces.
#
# Needs root, and: ip (iproute2), nft, tc, python3 with websockets and
# cryptography, turnserver (coturn), socat, openssl, unbound, tayga.
# Registered as the ctest `traversal_harness` (label `traversal`) when
# configured with -DNEREUS_TRAVERSAL_TESTS=ON; the CI job of that name runs
# it when the workflow is started by hand (workflow_dispatch).
#
# Usage: traversal-harness.sh --peer PATH --source DIR [--only SCENARIO]
#                             [--probe PATH] [--python PATH]
#
# Plan Task 29 Step 1: the relay floor measurement
# (tests/scripts/floor-measurement.sh) runs the scenario floor-measure,
# only when named with --only, with tests/tools/nereus_floor_probe as
# --probe. It adds a second coturn with a TLS listener on TCP 443 at a
# second address of the service's host (rvtls.harness.test, 198.51.100.66),
# as option (C) would need on the real server, and starts the service
# through tests/tools/floor_relay_prototype.py, which adds option (E)'s
# WebSocket relay at /v1/relay. See the script for its settings.
#
# =================================================================
# Modification history (NereusSDR):
#   2026-09-26: original implementation for NereusSDR by J.J. Boyd
#               (KG4VCF), with AI-assisted implementation via Anthropic
#               Claude Code.
#   2026-09-26: Task 27 fix wave: ipv6-both, DNS64 through an auth-zone,
#               eim-nat with the relay denied. J.J. Boyd (KG4VCF),
#               AI-assisted via Anthropic Claude Code.
#   2026-09-26: iPhone app plan Task 28 (R-IOS-16): the local run's fixes
#               (tayga 0.9.2 has no wkpf-strict, so NAT64 uses a
#               network-specific prefix; every program started in the
#               background runs under ip netns exec directly, so
#               stop_station and cleanup stop it; coturn's per-user quota
#               for a full run; the NAT routers drop unsolicited packets to
#               their own WAN address, as a home router does); netem-loss
#               asks for a connection, not an echo; the session-* scenarios.
#               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#   2026-09-27: iPhone app plan Task 29 Step 2a (R-IOS-16): race-direct,
#               race-service, upgrade, session-media, session-media-relayed.
#               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#   2026-09-27: Task 29 fix wave (review Important 1): upgrade-media.
#               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#   2026-09-28: observe the selected IPv6 pair after first echo, within the
#               existing preference window. J.J. Boyd (KG4VCF), AI-assisted
#               via OpenAI Codex.
# =================================================================

set -euo pipefail

PEER=""
PROBE=""
SOURCE=""
ONLY=""
# The interpreter with websockets and cryptography (CMake's
# NEREUS_TEST_PYTHON). Under sudo -E with the runner's PATH, a bare python3
# is install-qt-action's toolcache Python, which has neither (INFRA-C1).
PYTHON="python3"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --peer) PEER="$2"; shift 2 ;;
        --probe) PROBE="$2"; shift 2 ;;
        --source) SOURCE="$2"; shift 2 ;;
        --only) ONLY="$2"; shift 2 ;;
        --python) PYTHON="$2"; shift 2 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

if [[ -z "$PEER" || -z "$SOURCE" ]]; then
    echo "usage: $0 --peer PATH --source DIR [--only SCENARIO]" >&2
    exit 2
fi
if [[ "$(uname -s)" != "Linux" ]]; then
    echo "the traversal harness runs on Linux only" >&2
    exit 2
fi
if [[ "$(id -u)" != "0" ]]; then
    echo "the traversal harness needs root (network namespaces)" >&2
    exit 2
fi
for tool in ip nft tc "$PYTHON" turnserver socat openssl unbound tayga; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "missing: $tool" >&2
        exit 2
    fi
done
"$PYTHON" -c "import websockets, cryptography" 2>/dev/null || {
    echo "missing: websockets and cryptography for $PYTHON" >&2
    exit 2
}

WORK="$(mktemp -d /tmp/nereus-traversal.XXXXXX)"
chmod 700 "$WORK"
NAMESPACES=(rvsrv inet nats natc sta cli plat cli6)
PIDS=()

cleanup() {
    set +e
    if [[ -s "$WORK/front.pid" ]]; then
        kill "$(cat "$WORK/front.pid")" 2>/dev/null
    fi
    for pid in "${PIDS[@]:-}"; do
        [[ -n "$pid" ]] && kill "$pid" 2>/dev/null
    done
    sleep 0.5
    for pid in "${PIDS[@]:-}"; do
        [[ -n "$pid" ]] && kill -9 "$pid" 2>/dev/null
    done
    for ns in "${NAMESPACES[@]}"; do
        ip netns del "h-$ns" 2>/dev/null
        rm -rf "/etc/netns/h-$ns"
    done
    rm -rf "$WORK"
}
trap cleanup EXIT

say() { echo "[traversal] $*"; }
in_ns() { local ns="$1"; shift; ip netns exec "h-$ns" "$@"; }
# A program started in the background runs under `ip netns exec` directly,
# never through in_ns: `in_ns ... &` backgrounds a subshell, so $! would
# name the subshell and cleanup would orphan the program itself.

# ── The small internet ────────────────────────────────────────────────

for ns in "${NAMESPACES[@]}"; do
    ip netns add "h-$ns"
    in_ns "$ns" ip link set lo up
    in_ns "$ns" sysctl -qw net.ipv6.conf.all.disable_ipv6=0
    in_ns "$ns" sysctl -qw net.ipv6.conf.all.accept_dad=0
    in_ns "$ns" sysctl -qw net.ipv6.conf.default.accept_dad=0
done

# link A B IFA IFB: a veth pair between two namespaces.
link() {
    ip link add "$3" netns "h-$1" type veth peer name "$4" netns "h-$2"
    in_ns "$1" ip link set "$3" up
    in_ns "$2" ip link set "$4" up
}

# Public side, IPv4 198.51.100.0/24 in /30s and IPv6 2001:db8::/32.
link rvsrv inet srv0 inet-srv
in_ns rvsrv ip addr add 198.51.100.2/30 dev srv0
in_ns rvsrv ip -6 addr add 2001:db8:1::2/64 dev srv0 nodad
in_ns rvsrv ip -6 addr add 2001:db8:1::53/64 dev srv0 nodad
in_ns inet ip addr add 198.51.100.1/30 dev inet-srv
in_ns inet ip -6 addr add 2001:db8:1::1/64 dev inet-srv nodad
in_ns rvsrv ip route add default via 198.51.100.1
in_ns rvsrv ip -6 route add default via 2001:db8:1::1

link natc inet wan inet-natc
in_ns natc ip addr add 198.51.100.6/30 dev wan
in_ns inet ip addr add 198.51.100.5/30 dev inet-natc
in_ns natc ip route add default via 198.51.100.5

link nats inet wan inet-nats
in_ns nats ip addr add 198.51.100.10/30 dev wan
in_ns inet ip addr add 198.51.100.9/30 dev inet-nats
in_ns nats ip route add default via 198.51.100.9

link plat inet wan inet-plat
in_ns plat ip addr add 198.51.100.14/30 dev wan
in_ns inet ip addr add 198.51.100.13/30 dev inet-plat
in_ns plat ip route add default via 198.51.100.13
in_ns plat ip -6 addr add 2001:db8:4::2/64 dev wan nodad
in_ns inet ip -6 addr add 2001:db8:4::1/64 dev inet-plat nodad
in_ns plat ip -6 route add 2001:db8:1::/64 via 2001:db8:4::1

# Private sides.
link cli natc eth0 lan
in_ns cli ip addr add 10.1.0.2/24 dev eth0
in_ns natc ip addr add 10.1.0.1/24 dev lan
in_ns cli ip route add default via 10.1.0.1

link sta nats eth0 lan
in_ns sta ip addr add 10.2.0.2/24 dev eth0
in_ns nats ip addr add 10.2.0.1/24 dev lan
in_ns sta ip route add default via 10.2.0.1

# The IPv6-only client: IPv6 on its link, IPv4 only through its CLAT.
link cli6 plat eth0 lan
in_ns cli6 ip -6 addr add 2001:db8:5::2/64 dev eth0 nodad
in_ns plat ip -6 addr add 2001:db8:5::1/64 dev lan nodad
in_ns cli6 ip -6 route add default via 2001:db8:5::1
in_ns inet ip -6 route add 2001:db8:5::/64 via 2001:db8:4::2
in_ns rvsrv ip -6 route add 2001:db8:5::/64 via 2001:db8:1::1

for ns in inet natc nats plat; do
    in_ns "$ns" sysctl -qw net.ipv4.ip_forward=1
    in_ns "$ns" sysctl -qw net.ipv6.conf.all.forwarding=1
done

# The NAT routers' own firewall: replies only, as a home router.
nat_router() {
    local ns="$1" mode="$2"
    in_ns "$ns" nft -f - <<EOF
flush ruleset
table ip nat {
    chain post {
        type nat hook postrouting priority 100;
        oifname "wan" masquerade $mode
    }
}
table inet filter {
    chain forward {
        type filter hook forward priority 0; policy drop;
        ct state established,related accept
        iifname "lan" accept
    }
    chain input {
        type filter hook input priority 0; policy accept;
        iifname "wan" ct state new drop
    }
}
EOF
}

# ── DNS: names under harness.test ────────────────────────────────────

# rv (the service), rv4 (IPv4 only) and rv6 (IPv6 only), as on the NereusSDR
# server; the service's own name is IPv4 only here so the IPv6-only client
# reaches it through NAT64.
for ns in sta cli; do
    mkdir -p "/etc/netns/h-$ns"
    cat > "/etc/netns/h-$ns/hosts" <<EOF
127.0.0.1 localhost
198.51.100.2 rv.harness.test
198.51.100.2 rv4.harness.test
2001:db8:1::2 rv6.harness.test
198.51.100.66 rvtls.harness.test
EOF
    echo "nameserver 198.51.100.2" > "/etc/netns/h-$ns/resolv.conf"
done
mkdir -p /etc/netns/h-cli6
printf '::1 localhost\n' > /etc/netns/h-cli6/hosts
echo "nameserver 2001:db8:1::53" > /etc/netns/h-cli6/resolv.conf

cat > "$WORK/unbound.conf" <<EOF
server:
    interface: 198.51.100.2
    interface: 2001:db8:1::53
    access-control: 0.0.0.0/0 allow
    access-control: ::/0 allow
    do-daemonize: no
    username: ""
    chroot: ""
    directory: "$WORK"
    pidfile: ""
    use-syslog: no
    logfile: ""
    module-config: "dns64 iterator"
    dns64-prefix: 2001:db8:64::/96
    # harness.test is an auth-zone the iterator reads (for-upstream), not a
    # local-zone: a local-zone answers before the dns64 module runs, so the
    # IPv6-only client would get no synthesized AAAA for rv.harness.test and
    # would reach the service through its CLAT, proving nothing about
    # DNS64. Through the iterator, an A-only name gets its AAAA from dns64.
    # unbound serves test. itself by default (RFC 6761), which would answer
    # before the auth-zone: that default is turned off.
    local-zone: "test." nodefault
auth-zone:
    name: "harness.test."
    zonefile: "$WORK/harness.test.zone"
    for-downstream: no
    for-upstream: yes
    fallback-enabled: no
EOF
cat > "$WORK/harness.test.zone" <<'EOF'
$ORIGIN harness.test.
$TTL 60
@    IN SOA ns.harness.test. hostmaster.harness.test. 1 3600 600 86400 60
@    IN NS  ns.harness.test.
ns   IN A    198.51.100.2
rv   IN A    198.51.100.2
rv4  IN A    198.51.100.2
rv6  IN AAAA 2001:db8:1::2
rvtls IN A   198.51.100.66
EOF
ip netns exec h-rvsrv unbound -d -c "$WORK/unbound.conf" >"$WORK/unbound.log" 2>&1 &
PIDS+=($!)

# ── NAT64 (the carrier's PLAT) and the client's CLAT ─────────────────

# plat: tayga maps 2001:db8:64::/96 to IPv4 through its own pool, which
# nftables then masquerades onto its public address.
mkdir -p "$WORK/tayga-plat"
cat > "$WORK/tayga-plat.conf" <<EOF
tun-device nat64
ipv4-addr 192.168.255.1
ipv6-addr 2001:db8:4::64
prefix 2001:db8:64::/96
dynamic-pool 192.168.255.0/24
data-dir $WORK/tayga-plat
EOF
in_ns plat tayga -c "$WORK/tayga-plat.conf" --mktun
in_ns plat ip link set nat64 up
in_ns plat ip route add 192.168.255.0/24 dev nat64
in_ns plat ip -6 route add 2001:db8:64::/96 dev nat64
ip netns exec h-plat tayga -c "$WORK/tayga-plat.conf" -d >"$WORK/tayga-plat.log" 2>&1 &
PIDS+=($!)
in_ns plat nft -f - <<EOF
flush ruleset
table ip nat {
    chain post {
        type nat hook postrouting priority 100;
        oifname "wan" masquerade
    }
}
EOF

# cli6: a CLAT (464XLAT), tayga mapping the host's 192.0.0.2 (RFC 7335) to
# an IPv6 address of its own prefix, routed to it through plat, so IPv4
# sockets work through NAT64 as on a phone.
cat > "$WORK/tayga-clat.conf" <<EOF
tun-device clat
ipv4-addr 192.0.0.1
ipv6-addr 2001:db8:6::1
prefix 2001:db8:64::/96
map 192.0.0.2 2001:db8:6::464
EOF
in_ns cli6 sysctl -qw net.ipv6.conf.all.forwarding=1
in_ns cli6 tayga -c "$WORK/tayga-clat.conf" --mktun
in_ns cli6 ip link set clat up
in_ns cli6 ip addr add 192.0.0.2/32 dev clat
in_ns cli6 ip route add default dev clat mtu 1260
in_ns cli6 ip -6 route add 2001:db8:6::/64 dev clat
in_ns plat ip -6 route add 2001:db8:6::/64 via 2001:db8:5::2
ip netns exec h-cli6 tayga -c "$WORK/tayga-clat.conf" -d >"$WORK/tayga-clat.log" 2>&1 &
PIDS+=($!)

# ── The service: coturn, the rendezvous behind TLS ──────────────────

"$PYTHON" -c 'import secrets; print(secrets.token_hex(24))' > "$WORK/turn-secret"
chmod 600 "$WORK/turn-secret"
# Plan Task 29 step 2b: the web relay's own secret (never the TURN one,
# rendezvous section 12.2), which the service mints grants with.
"$PYTHON" -c 'import secrets; print(secrets.token_hex(32))' > "$WORK/relay-secret"
chmod 600 "$WORK/relay-secret"
ip netns exec h-rvsrv turnserver -n -v --no-cli --no-tls --no-dtls --fingerprint \
    --listening-ip=198.51.100.2 --listening-ip=2001:db8:1::2 --listening-port=3478 \
    --relay-ip=198.51.100.2 --relay-ip=2001:db8:1::2 \
    --realm=harness.test --use-auth-secret \
    --static-auth-secret="$(cat "$WORK/turn-secret")" \
    --user-quota=64 --total-quota=64 \
    --allowed-peer-ip=198.51.100.0-198.51.100.255 \
    --allowed-peer-ip=2001:db8::-2001:db8:ffff:ffff:ffff:ffff:ffff:ffff \
    --log-file=stdout >"$WORK/coturn.log" 2>&1 &
COTURN_PID=$!
PIDS+=("$COTURN_PID")

cat > "$WORK/rendezvous.conf" <<EOF
[rendezvous]
listen = 127.0.0.1:8710
stun_urls = stun:rv4.harness.test:3478 stun:rv6.harness.test:3478
turn_urls = turn:rv4.harness.test:3478?transport=udp turn:rv6.harness.test:3478?transport=udp
turn_secret_file = $WORK/turn-secret
log_level = info
EOF
FLOOR=0
if [[ "$ONLY" == floor-* ]]; then
    FLOOR=1
fi
if (( FLOOR )); then
    # Plan Task 29 Step 1: the service with option (E)'s relay prototype.
    ip netns exec h-rvsrv env PYTHONPATH="$SOURCE/rendezvous/server" \
        "$PYTHON" "$SOURCE/tests/tools/floor_relay_prototype.py" --config "$WORK/rendezvous.conf" \
        >"$WORK/rendezvous.log" 2>&1 &
else
    # Plan Task 29 step 2b: the service mints web relay grants
    # (rendezvous section 12.1), and the relay runs as on the real server,
    # on a Unix socket behind the front (below).
    cat >> "$WORK/rendezvous.conf" <<EOF
relay_url = wss://rv.harness.test/v1/relay
relay_secret_file = $WORK/relay-secret
EOF
    # This isolated fixture opts in only for the independent-watch acceptance
    # rows. Production's relay_watch_version default remains disabled.
    if [[ -z "$ONLY" || "$ONLY" == web-relay-deadline
          || "$ONLY" == direct-wss-deadline
          || "$ONLY" == web-relay-second-rx-deadline ]]; then
        printf 'relay_watch_version = 1\n' >> "$WORK/rendezvous.conf"
    fi
    ip netns exec h-rvsrv env PYTHONPATH="$SOURCE/rendezvous/server" \
        "$PYTHON" -m nereus_rendezvous --config "$WORK/rendezvous.conf" >"$WORK/rendezvous.log" 2>&1 &
fi
RENDEZVOUS_PID=$!
PIDS+=("$RENDEZVOUS_PID")
if (( ! FLOOR )); then
    cat > "$WORK/relay.conf" <<EOF
[relay]
socket = $WORK/relay.sock
socket_mode = 0660
socket_group = root
relay_secret_file = $WORK/relay-secret
log_level = info
EOF
    ip netns exec h-rvsrv env PYTHONPATH="$SOURCE/rendezvous/server" \
        "$PYTHON" -m nereus_relay --config "$WORK/relay.conf" >"$WORK/relay.log" 2>&1 &
    RELAY_PID=$!
    PIDS+=("$RELAY_PID")
fi

# A test certificate authority and a certificate for rv.harness.test, made
# now, trusted only by the two peers (--ca).
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -days 1 \
    -subj "/CN=NereusSDR traversal harness CA" \
    -keyout "$WORK/ca.key" -out "$WORK/ca.pem" >/dev/null 2>&1
openssl req -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes \
    -subj "/CN=rv.harness.test" -addext "subjectAltName=DNS:rv.harness.test" \
    -keyout "$WORK/rv.key" -out "$WORK/rv.csr" >/dev/null 2>&1
printf 'subjectAltName=DNS:rv.harness.test\n' > "$WORK/rv.ext"
openssl x509 -req -in "$WORK/rv.csr" -CA "$WORK/ca.pem" -CAkey "$WORK/ca.key" \
    -CAcreateserial -days 1 -extfile "$WORK/rv.ext" -out "$WORK/rv.pem" >/dev/null 2>&1
cat "$WORK/rv.pem" "$WORK/rv.key" > "$WORK/rv-bundle.pem"
chmod 600 "$WORK"/*.key "$WORK/rv-bundle.pem"
# Plan Task 29 Step 1: for the floor measurement the front sends each
# write at once (TCP_NODELAY), as Caddy (Go's default) does on the real
# server; option (E)'s frames cross it.
NODELAY=""
if (( FLOOR )); then
    NODELAY=",nodelay"
fi
# Plan Task 29 step 2b: the front stands in for Caddy (rendezvous section
# 12.7): /v1/relay to the relay's Unix socket, the rest to the service, and
# counts of the relay's datagrams and of any holding the session's
# plaintext markers (the control channel's JSON, the marker the session
# peer sends on its media "tx" channel).
MARKER_JSON="$(printf '"type"' | od -An -tx1 | tr -d ' \n')"
MARKER_TX="$(printf 'NEREUS-PLAINTEXT-MARKER' | od -An -tx1 | tr -d ' \n')"
start_front() {
    ip netns exec h-rvsrv "$PYTHON" "$SOURCE/tests/tools/harness_front.py" \
        --listen 198.51.100.2 --listen 2001:db8:1::2 --cert "$WORK/rv-bundle.pem" \
        --relay-socket "$WORK/relay.sock" --marker "$MARKER_JSON" --marker "$MARKER_TX" \
        --report "$WORK/front.json" >>"$WORK/front.log" 2>&1 &
    FRONT_PID=$!
    printf '%s\n' "$FRONT_PID" > "$WORK/front.pid"
    PIDS+=("$FRONT_PID")
}
stop_front() {
    local pid
    pid="$(cat "$WORK/front.pid")"
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
}
if (( FLOOR )); then
    for listen in "OPENSSL-LISTEN:443,bind=198.51.100.2,reuseaddr,fork" \
                  "OPENSSL-LISTEN:443,bind=[2001:db8:1::2],pf=ip6,reuseaddr,fork"; do
        ip netns exec h-rvsrv socat "$listen,cert=$WORK/rv-bundle.pem,verify=0$NODELAY" \
            "TCP:127.0.0.1:8710$NODELAY" >>"$WORK/socat.log" 2>&1 &
        PIDS+=($!)
    done
else
    start_front
fi
sleep 2

SERVER="wss://rv.harness.test/"

# ── Plan Task 29 Step 1: the relay floor's server side ──────────────

# Option (C) (and (B)) reach coturn's TLS listener on TCP 443, which the
# service's TLS front already holds on the service's address, so coturn's
# TLS listener gets a second address of its own (D38: on the real server,
# a second address or a splitter by TLS name in front of Caddy). Option
# (E) needs nothing more: its relay is the service's /v1/relay, behind the
# same TLS front.
if (( FLOOR )); then
    in_ns rvsrv ip addr add 198.51.100.66/32 dev lo
    in_ns inet ip route add 198.51.100.66/32 via 198.51.100.2
    openssl req -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes \
        -subj "/CN=rvtls.harness.test" -addext "subjectAltName=DNS:rvtls.harness.test" \
        -keyout "$WORK/rvtls.key" -out "$WORK/rvtls.csr" >/dev/null 2>&1
    printf 'subjectAltName=DNS:rvtls.harness.test\n' > "$WORK/rvtls.ext"
    openssl x509 -req -in "$WORK/rvtls.csr" -CA "$WORK/ca.pem" -CAkey "$WORK/ca.key" \
        -CAcreateserial -days 1 -extfile "$WORK/rvtls.ext" -out "$WORK/rvtls.pem" >/dev/null 2>&1
    chmod 600 "$WORK/rvtls.key"
    ip netns exec h-rvsrv turnserver -n --no-cli --no-udp --no-tcp --no-dtls --fingerprint \
        --listening-ip=198.51.100.66 --tls-listening-port=443 --relay-ip=198.51.100.66 \
        --cert="$WORK/rvtls.pem" --pkey="$WORK/rvtls.key" \
        --realm=harness.test --use-auth-secret \
        --static-auth-secret="$(cat "$WORK/turn-secret")" \
        --user-quota=64 --total-quota=64 \
        --allowed-peer-ip=198.51.100.0-198.51.100.255 \
        --log-file=stdout >"$WORK/coturn-tls.log" 2>&1 &
    COTURN_TLS_PID=$!
    PIDS+=("$COTURN_TLS_PID")
    sleep 1
fi

# ── The two ends ─────────────────────────────────────────────────────

CLIENT_KEY="$("$PEER" key --dir "$WORK/client-key")"
# Plan Task 28: the desktop's device key, for the session-* scenarios.
DESKTOP_KEY="$("$PEER" device-key --dir "$WORK/desktop-key")"

start_station() {
    local relay="$1"
    rm -f "$WORK/station-id"
    ip netns exec h-sta "$PEER" station --dir "$WORK/station-key" --server "$SERVER" \
        --paired "$CLIENT_KEY" --relay "$relay" --id-file "$WORK/station-id" \
        --ca "$WORK/ca.pem" >"$WORK/station.log" 2>&1 &
    STATION_PID=$!
    PIDS+=("$STATION_PID")
    for _ in $(seq 1 100); do
        [[ -s "$WORK/station-id" ]] && return 0
        sleep 0.2
    done
    say "the Core did not register"
    cat "$WORK/station.log" "$WORK/rendezvous.log" >&2 || true
    return 1
}

stop_station() {
    kill "$STATION_PID" 2>/dev/null || true
    wait "$STATION_PID" 2>/dev/null || true
}

# run_client NS: prints the client's JSON result line.
run_client() {
    local ns="$1"
    shift
    in_ns "$ns" "$PEER" client --dir "$WORK/client-key" --server "$SERVER" \
        --station-id "$(cat "$WORK/station-id")" --timeout-ms 90000 \
        --ca "$WORK/ca.pem" "$@" 2>>"$WORK/client.log" | tail -n 1 || true
}

field() { "$PYTHON" -c "import json,sys; print(json.loads(sys.argv[1]).get(sys.argv[2]))" "$1" "$2"; }

# Plan Task 28: a Core as nereusd runs it, answering introductions with a
# control connection, the desktop paired.
# start_core RELAY [ARGS...]: Task 29 passes --listen PORT (the Core's own
# address, for the race) and --media (real audio and display).
start_core() {
    local relay="$1"
    shift
    rm -f "$WORK/core-id"
    ip netns exec h-sta "$PEER" core --dir "$WORK/core" --server "$SERVER" \
        --paired "$DESKTOP_KEY" --relay "$relay" --id-file "$WORK/core-id" \
        --ca "$WORK/ca.pem" "$@" >"$WORK/core.log" 2>&1 &
    CORE_PID=$!
    PIDS+=("$CORE_PID")
    for _ in $(seq 1 100); do
        [[ -s "$WORK/core-id" ]] && return 0
        sleep 0.2
    done
    say "the Core did not register"
    cat "$WORK/core.log" "$WORK/rendezvous.log" >&2 || true
    return 1
}

stop_core() {
    kill "$CORE_PID" 2>/dev/null || true
    wait "$CORE_PID" 2>/dev/null || true
}

# run_session NS [ARGS...]: prints the desktop's JSON result line. Task 29
# passes --direct URL (race the Core's address against the service),
# --upgrade-schedule-ms, --wait-upgrade-ms and --media-ms.
run_session() {
    local ns="$1"
    shift
    in_ns "$ns" "$PEER" session --dir "$WORK/desktop-key" --server "$SERVER" \
        --core "$WORK/core-id" --timeout-ms 120000 \
        --ca "$WORK/ca.pem" "$@" 2>>"$WORK/session.log" | tee "$WORK/session-events.log" | tail -n 1 || true
}

FAILED=0
check() {
    local name="$1" result="$2" want_echo="$3" want_relay="$4"
    local echoed relayed
    echoed="$(field "$result" echoed 2>/dev/null || echo None)"
    relayed="$(field "$result" relayed 2>/dev/null || echo None)"
    if [[ "$echoed" != "$want_echo" ]]; then
        say "FAIL $name: echoed=$echoed, expected $want_echo: $result"
        FAILED=1
        return
    fi
    if [[ "$want_relay" != "any" && "$relayed" != "$want_relay" ]]; then
        say "FAIL $name: relayed=$relayed, expected $want_relay: $result"
        FAILED=1
        return
    fi
    say "PASS $name: $result"
}

# check_session NAME RESULT WANT_RELAY: it connected (for a session: the
# whole connect sequence ran), on the path wanted.
check_session() {
    local name="$1" result="$2" want_relay="$3"
    local connected relayed
    connected="$(field "$result" connected 2>/dev/null || echo None)"
    relayed="$(field "$result" relayed 2>/dev/null || echo None)"
    if [[ "$connected" != "True" ]]; then
        say "FAIL $name: connected=$connected: $result"
        FAILED=1
        return
    fi
    if [[ "$want_relay" != "any" && "$relayed" != "$want_relay" ]]; then
        say "FAIL $name: relayed=$relayed, expected $want_relay: $result"
        FAILED=1
        return
    fi
    say "PASS $name: $result"
}

# check_path NAME RESULT WANT_RANK WANT_MOVED: Task 29, the session ran and
# ended on the path of rank WANT_RANK (0 this network, 1 direct, 2 the
# service without the relay, 3 the relay); WANT_MOVED True, False or any.
check_path() {
    local name="$1" result="$2" want_rank="$3" want_moved="$4"
    local connected rank moved
    connected="$(field "$result" connected 2>/dev/null || echo None)"
    rank="$(field "$result" rankAfter 2>/dev/null || echo None)"
    moved="$(field "$result" moved 2>/dev/null || echo None)"
    if [[ "$connected" != "True" || "$rank" != "$want_rank" ]]; then
        say "FAIL $name: connected=$connected rankAfter=$rank, expected rank $want_rank: $result"
        FAILED=1
        return
    fi
    if [[ "$want_moved" != "any" && "$moved" != "$want_moved" ]]; then
        say "FAIL $name: moved=$moved, expected $want_moved: $result"
        FAILED=1
        return
    fi
    say "PASS $name: $result"
}

# check_media NAME RESULT: Task 29, real audio and display decoded.
check_media() {
    local name="$1" result="$2"
    if "$PYTHON" -c "
import json, sys
r = json.loads(sys.argv[1])
ok = r.get('connected') and r.get('audioDecoded', 0) > 0 and r.get('displayDecoded', 0) > 0
sys.exit(0 if ok else 1)" "$result" 2>/dev/null; then
        say "PASS $name: $result"
    else
        say "FAIL $name: no audio or no display decoded: $result"
        FAILED=1
    fi
}

# The Core's own listener, for the race: TCP 47910 at the Core's router
# forwarded to the Core, as a person forwards a port at home.
CORE_PORT=47910
CORE_URL="wss://198.51.100.10:$CORE_PORT"
forward_core() {
    in_ns nats nft add chain ip nat pre "{ type nat hook prerouting priority -100; }"
    in_ns nats nft add rule ip nat pre iifname "wan" tcp dport "$CORE_PORT" \
        dnat to "10.2.0.2:$CORE_PORT"
    in_ns nats nft add rule inet filter forward ct status dnat accept
}

# No UDP between the two routers' public addresses: the service's path is
# the relay.
block_direct_udp() {
    in_ns inet nft -f - <<EOF
table inet filter {
    chain forward {
        type filter hook forward priority 0;
        ip saddr 198.51.100.6 ip daddr 198.51.100.10 meta l4proto udp drop
        ip saddr 198.51.100.10 ip daddr 198.51.100.6 meta l4proto udp drop
    }
}
EOF
}

reset_rules() {
    nat_router natc ""
    nat_router nats ""
    in_ns inet nft flush ruleset
    for ns in natc nats; do
        in_ns "$ns" tc qdisc del dev wan root 2>/dev/null || true
    done
}

scenario() {
    [[ -z "$ONLY" || "$ONLY" == "$1" ]]
}

# eim-nat: Linux masquerade keeps each flow's source port where it can, so
# the mapping is endpoint independent: hole punching works. The relay is
# denied, so a relayed pair nominated first cannot pass it by.
if scenario eim-nat; then
    reset_rules
    start_station deny
    check eim-nat "$(run_client cli)" True False
    stop_station
fi

# random-nat-both: every mapping gets a random port at both ends, so the
# server-reflexive ports mean nothing to the far end: the relay carries it.
if scenario random-nat-both; then
    reset_rules
    nat_router natc "fully-random"
    nat_router nats "fully-random"
    start_station allow
    check random-nat-both "$(run_client cli)" True True
    stop_station
fi

# udp-direct-blocked: no UDP between the two NATs' public addresses; UDP to
# the service's host still flows.
if scenario udp-direct-blocked; then
    reset_rules
    in_ns inet nft -f - <<EOF
table inet filter {
    chain forward {
        type filter hook forward priority 0;
        ip saddr 198.51.100.6 ip daddr 198.51.100.10 meta l4proto udp drop
        ip saddr 198.51.100.10 ip daddr 198.51.100.6 meta l4proto udp drop
    }
}
EOF
    start_station allow
    check udp-direct-blocked "$(run_client cli)" True True
    stop_station
fi

# udp-blocked: the client's network passes no UDP but DNS. TURN over UDP
# cannot work; the TCP floor is plan Task 29. The client must say so in
# time, not hang.
if scenario udp-blocked; then
    reset_rules
    in_ns natc nft insert rule inet filter forward iifname "lan" meta l4proto udp udp dport != 53 drop
    start_station allow
    started=$(date +%s)
    result="$(run_client cli)"
    elapsed=$(( $(date +%s) - started ))
    check udp-blocked "$result" False any
    if (( elapsed > 120 )); then
        say "FAIL udp-blocked: the client took ${elapsed}s to give up"
        FAILED=1
    fi
    stop_station
fi

# mtu-1100: datagrams over 1100 bytes dropped on the way (the carrier
# behaviour the pairing design section 9.3 describes); the 60000-byte
# message crosses in 1000-byte datagrams.
if scenario mtu-1100; then
    reset_rules
    in_ns inet nft -f - <<EOF
table inet filter {
    chain forward {
        type filter hook forward priority 0;
        meta l4proto udp meta length > 1100 drop
    }
}
EOF
    start_station allow
    check mtu-1100 "$(run_client cli)" True any
    stop_station
fi

# ipv6-only-nat64: the client has only IPv6 (with DNS64, NAT64 and a CLAT),
# the Core only IPv4 behind NAT. The service's IPv4-only name must come back
# from DNS64 as an address in 2001:db8:64::/96, so the scenario proves DNS64 and
# not only the CLAT.
if scenario ipv6-only-nat64; then
    reset_rules
    synthesized="$(in_ns cli6 getent ahostsv6 rv.harness.test | awk '{print $1}' | sort -u | tr '\n' ' ' || true)"
    if [[ "$synthesized" != *"2001:db8:64::"* ]]; then
        say "FAIL ipv6-only-nat64: DNS64 did not answer rv.harness.test (got: ${synthesized:-nothing})"
        FAILED=1
    else
        say "DNS64 answered rv.harness.test with $synthesized"
    fi
    start_station allow
    check ipv6-only-nat64 "$(run_client cli6)" True any
    stop_station
fi

# netem-loss: 2 % loss and 40 ms each way on both uplinks. The echo rides
# the display channel, which never retransmits (latest value wins), so one
# 60000-byte message sent once is lost with most of its 60 datagrams each
# way: the scenario asks for the connection. A whole session over the
# reliable control channel under the same loss is session-loss.
if scenario netem-loss; then
    reset_rules
    for ns in natc nats; do
        in_ns "$ns" tc qdisc add dev wan root netem loss 2% delay 40ms
    done
    start_station allow
    check_session netem-loss "$(run_client cli)" any
    stop_station
fi

# ipv6-both: routed IPv6 at both ends, as most ISPs give it, behind each
# router's stateful firewall (nat_router's table inet filter: replies
# only), beside the IPv4 NAT. Both ends then gather IPv6 host candidates as
# well as IPv4 ones, and ICE must choose an IPv6 pair (the IPv6-preference
# acceptance item of plan Task 27; its unit test skips on a computer with
# no global IPv6). The relay is denied so only direct pairs compete. The
# addresses are added for this scenario only and removed after it, so the
# IPv4 scenarios stay IPv4.
ipv6_both() {
    if [[ "$1" == "up" ]]; then
        in_ns natc ip -6 addr add 2001:db8:2::2/64 dev wan nodad
        in_ns inet ip -6 addr add 2001:db8:2::1/64 dev inet-natc nodad
        in_ns natc ip -6 route add default via 2001:db8:2::1
        in_ns natc ip -6 addr add 2001:db8:11::1/64 dev lan nodad
        in_ns cli ip -6 addr add 2001:db8:11::2/64 dev eth0 nodad
        in_ns cli ip -6 route add default via 2001:db8:11::1
        in_ns inet ip -6 route add 2001:db8:11::/64 via 2001:db8:2::2

        in_ns nats ip -6 addr add 2001:db8:3::2/64 dev wan nodad
        in_ns inet ip -6 addr add 2001:db8:3::1/64 dev inet-nats nodad
        in_ns nats ip -6 route add default via 2001:db8:3::1
        in_ns nats ip -6 addr add 2001:db8:12::1/64 dev lan nodad
        in_ns sta ip -6 addr add 2001:db8:12::2/64 dev eth0 nodad
        in_ns sta ip -6 route add default via 2001:db8:12::1
        in_ns inet ip -6 route add 2001:db8:12::/64 via 2001:db8:3::2
        return
    fi
    set +e
    in_ns inet ip -6 route del 2001:db8:11::/64
    in_ns inet ip -6 route del 2001:db8:12::/64
    in_ns cli ip -6 route del default
    in_ns sta ip -6 route del default
    in_ns cli ip -6 addr del 2001:db8:11::2/64 dev eth0
    in_ns sta ip -6 addr del 2001:db8:12::2/64 dev eth0
    in_ns natc ip -6 route del default
    in_ns nats ip -6 route del default
    in_ns natc ip -6 addr del 2001:db8:11::1/64 dev lan
    in_ns nats ip -6 addr del 2001:db8:12::1/64 dev lan
    in_ns natc ip -6 addr del 2001:db8:2::2/64 dev wan
    in_ns nats ip -6 addr del 2001:db8:3::2/64 dev wan
    in_ns inet ip -6 addr del 2001:db8:2::1/64 dev inet-natc
    in_ns inet ip -6 addr del 2001:db8:3::1/64 dev inet-nats
    set -e
}

if scenario ipv6-both; then
    reset_rules
    ipv6_both up
    start_station deny
    result="$(run_client cli --require-ipv6)"
    check ipv6-both "$result" True False
    # run_client preserves the final JSON even when the helper fails. A
    # path observed at failure must not satisfy the preference assertion.
    connected="$(field "$result" connected 2>/dev/null || echo None)"
    reason="$(field "$result" reason 2>/dev/null || echo Invalid)"
    if [[ "$connected" != True || "$reason" != None ]]; then
        say "FAIL ipv6-both: preference observation did not succeed: $result"
        FAILED=1
    fi
    local_address="$(field "$result" localAddress 2>/dev/null || echo None)"
    remote_address="$(field "$result" remoteAddress 2>/dev/null || echo None)"
    if [[ "$local_address" == *:* && "$remote_address" == *:* ]]; then
        say "PASS ipv6-both: the selected pair is IPv6 ($local_address to $remote_address)"
    else
        say "FAIL ipv6-both: the selected pair is not IPv6 ($local_address to $remote_address)"
        FAILED=1
    fi
    stop_station
    ipv6_both down
fi

# ── Plan Task 28: a whole session over an introduced connection ─────

# session-direct: endpoint-independent NAT at both ends, the relay denied.
if scenario session-direct; then
    reset_rules
    start_core deny
    check_session session-direct "$(run_session cli)" False
    stop_core
fi

# session-relayed: no UDP between the two NATs' public addresses; the
# session runs through the relay, and when it ends each end gives its
# allocation back (a Refresh of lifetime 0), which coturn logs with -v as
# "refreshed, ..., lifetime=0".
if scenario session-relayed; then
    reset_rules
    in_ns inet nft -f - <<EOF
table inet filter {
    chain forward {
        type filter hook forward priority 0;
        ip saddr 198.51.100.6 ip daddr 198.51.100.10 meta l4proto udp drop
        ip saddr 198.51.100.10 ip daddr 198.51.100.6 meta l4proto udp drop
    }
}
EOF
    start_core allow
    released_before="$(grep -c "lifetime=0" "$WORK/coturn.log" 2>/dev/null || true)"
    check_session session-relayed "$(run_session cli)" True
    sleep 2
    released_after="$(grep -c "lifetime=0" "$WORK/coturn.log" 2>/dev/null || true)"
    released=$(( ${released_after:-0} - ${released_before:-0} ))
    if (( released >= 2 )); then
        say "PASS session-relayed: both allocations given back ($released)"
    else
        say "FAIL session-relayed: allocations given back: $released, expected 2"
        FAILED=1
    fi
    stop_core
fi

# session-loss: 2 % loss and 40 ms each way on both uplinks.
if scenario session-loss; then
    reset_rules
    for ns in natc nats; do
        in_ns "$ns" tc qdisc add dev wan root netem loss 2% delay 40ms
    done
    start_core allow
    check_session session-loss "$(run_session cli)" any
    stop_core
fi

# ── Plan Task 29 Step 2a: the race, moving, media ───────────────────

# race-direct: the Core's port forwarded and the service reachable; the
# desktop races both and ends on the direct connection (it may start on
# the service and move once the session is up).
if scenario race-direct; then
    reset_rules
    forward_core
    start_core deny --listen "$CORE_PORT"
    check_path race-direct "$(run_session cli --direct "$CORE_URL" --wait-upgrade-ms 8000)" 1 any
    stop_core
fi

# race-service: the Core's address does not answer (no forward); the
# service's path without the relay wins, and nothing better turns up.
if scenario race-service; then
    reset_rules
    start_core deny --listen "$CORE_PORT"
    check_path race-service "$(run_session cli --direct "$CORE_URL" \
        --upgrade-schedule-ms 2000 --wait-upgrade-ms 6000)" 2 False
    stop_core
fi

# upgrade: the session starts through the relay (no forward, no UDP between
# the routers); the forward is opened while it runs, and the next look
# moves the session to the direct connection with nothing signed in again.
if scenario upgrade; then
    reset_rules
    block_direct_udp
    start_core allow --listen "$CORE_PORT"
    ( sleep 12; forward_core ) &
    OPEN_PID=$!
    result="$(run_session cli --direct "$CORE_URL" \
        --upgrade-schedule-ms 15000,5000,5000 --wait-upgrade-ms 40000)"
    wait "$OPEN_PID" 2>/dev/null || true
    check_path upgrade "$result" 1 True
    handshakes="$(field "$result" handshakes 2>/dev/null || echo None)"
    if [[ "$handshakes" != "1" ]]; then
        say "FAIL upgrade: signed in $handshakes times, expected once"
        FAILED=1
    fi
    stop_core
fi

# upgrade-media: the upgrade scenario with the desktop's own media
# listening (Task 29 fix wave, review Important 1): the session starts
# through the relay, moves to the direct connection once the forward
# opens and UDP passes again, and its audio and display follow onto a new
# media connection with no silent run over 40 ms.
if scenario upgrade-media; then
    reset_rules
    block_direct_udp
    start_core allow --listen "$CORE_PORT" --media
    # The better network arrives: the forward opens and UDP between the
    # routers passes again, so the new media connection has a path of its
    # own (with UDP still blocked it stays on the relay, correctly).
    ( sleep 12; forward_core; in_ns inet nft flush ruleset ) &
    OPEN_PID=$!
    result="$(run_session cli --direct "$CORE_URL" \
        --upgrade-schedule-ms 15000,5000,5000 --wait-upgrade-ms 40000 --follow-media-ms 6000)"
    wait "$OPEN_PID" 2>/dev/null || true
    check_path upgrade-media "$result" 1 True
    if "$PYTHON" -c "
import json, sys
r = json.loads(sys.argv[1])
ok = (r.get('mediaConnections', 0) >= 2 and r.get('heardMs', 0) > 3000
      and r.get('longestSilentMs', 1000) <= 40
      and r.get('audioAfterMoveMs', -1) >= 0
      and r.get('displayAfterReplacement', 0) > 0
      and r.get('displayAfterMoveMs', -1) >= 0
      and not r.get('replacePending', True))
sys.exit(0 if ok else 1)" "$result" 2>/dev/null; then
        say "PASS upgrade-media: media followed the move"
    else
        say "FAIL upgrade-media: media did not follow the move cleanly: $result"
        FAILED=1
    fi
    stop_core
fi

# session-media: audio and display through the service, the relay denied.
if scenario session-media; then
    reset_rules
    start_core deny --media
    check_media session-media "$(run_session cli --media-ms 5000)"
    stop_core
fi

# session-media-relayed: audio and display through the relay; each end
# holds two allocations (control and media) and gives all four back.
if scenario session-media-relayed; then
    reset_rules
    block_direct_udp
    start_core allow --media
    released_before="$(grep -c "lifetime=0" "$WORK/coturn.log" 2>/dev/null || true)"
    result="$(run_session cli --media-ms 5000)"
    check_media session-media-relayed "$result"
    sleep 3
    released_after="$(grep -c "lifetime=0" "$WORK/coturn.log" 2>/dev/null || true)"
    released=$(( ${released_after:-0} - ${released_before:-0} ))
    if (( released >= 4 )); then
        say "PASS session-media-relayed: allocations given back ($released)"
    else
        say "FAIL session-media-relayed: allocations given back: $released, expected 4"
        FAILED=1
    fi
    stop_core
fi

# ── Plan Task 29 step 2b: the web relay ─────────────────────────────

# web_only NS_ROUTER: the network behind NS_ROUTER passes DNS and TCP 443
# and nothing else (the options survey's N1).
web_only() {
    in_ns "$1" nft insert rule inet filter forward iifname "lan" meta l4proto udp udp dport != 53 drop
    in_ns "$1" nft insert rule inet filter forward iifname "lan" meta l4proto tcp tcp dport != 443 drop
}

# front_field KEY TAG: a count from the front's report.
front_field() {
    "$PYTHON" -c "
import json, sys
r = json.load(open(sys.argv[1]))
print(r.get(sys.argv[2], {}).get(sys.argv[3], 0))" "$WORK/front.json" "$1" "$2" 2>/dev/null || echo 0
}

# check_web_relay NAME RESULT: the session ran over the web relay (rank 4)
# with audio and display, both lanes carried datagrams, and none held the
# session's plaintext markers.
check_web_relay() {
    local name="$1" result="$2"
    sleep 1.5
    local control media hits
    control="$(front_field datagrams 1)"
    media="$(front_field datagrams 2)"
    hits="$(( $(front_field markerHits 1) + $(front_field markerHits 2) ))"
    if "$PYTHON" -c "
import json, sys
r = json.loads(sys.argv[1])
ok = (r.get('connected') and r.get('rankAfter') == 4 and r.get('audioDecoded', 0) > 0
      and r.get('displayDecoded', 0) > 0)
sys.exit(0 if ok else 1)" "$result" 2>/dev/null \
        && (( control > 0 && media > 0 && hits == 0 )); then
        say "PASS $name: control $control, media $media datagrams through the relay, plaintext seen 0: $result"
    else
        say "FAIL $name: control $control, media $media, plaintext seen $hits: $result"
        FAILED=1
    fi
}

# web-relay: the client's network passes only DNS and TCP 443 (UDP to the
# service's STUN and TURN too is dropped): the session, audio and display
# run over the web relay, one leg per end carrying both connections, and
# the relay sees no plaintext.
if scenario web-relay; then
    reset_rules
    web_only natc
    start_core allow --media
    check_web_relay web-relay "$(run_session cli --media-ms 6000)"
    stop_core
fi

# A forwarded Core wss port shares the allowed HTTPS port. ICE has no UDP
# path between the devices, so audio and display must use the direct tunnel.
if scenario direct-wss-media; then
    reset_rules
    web_only natc
    CORE_PORT=443
    CORE_URL="wss://198.51.100.10:$CORE_PORT"
    forward_core
    start_core allow --listen "$CORE_PORT" --media
    result="$(run_session cli --direct "$CORE_URL" --media-ms 6000)"
    check_path direct-wss-media "$result" 1 any
    if "$PYTHON" -c "
import json,sys
r=json.loads(sys.argv[1]); sys.exit(0 if r.get('audioDecoded',0)>0 and
 r.get('displayDecoded',0)>0 and r.get('mediaViaShim') else 1)" "$result"; then
        say "PASS direct-wss-media: media carried by the UUID tunnel: $result"
    else
        say "FAIL direct-wss-media: $result"
        FAILED=1
    fi
    stop_core
    CORE_PORT=47910
    CORE_URL="wss://198.51.100.10:$CORE_PORT"
fi

# Start on the service's web relay, then open direct TCP 443 while UDP stays
# blocked. The start declaration remains valid across the path move and the
# replacement uses the direct tunnel without tearing down live media first.
if scenario web-to-direct-wss; then
    reset_rules
    web_only natc
    CORE_PORT=443
    CORE_URL="wss://198.51.100.10:$CORE_PORT"
    start_core allow --listen "$CORE_PORT" --media
    ( sleep 12; forward_core ) &
    OPEN_PID=$!
    result="$(run_session cli --direct "$CORE_URL" \
        --upgrade-schedule-ms 15000,5000,5000 --wait-upgrade-ms 40000 \
        --follow-media-ms 6000)"
    wait "$OPEN_PID" 2>/dev/null || true
    check_path web-to-direct-wss "$result" 1 True
    if "$PYTHON" -c "
import json,sys
r=json.loads(sys.argv[1]); sys.exit(0 if r.get('mediaConnections',0)>=2 and
 r.get('heardMs',0)>3000 and r.get('audioAfterMoveMs',-1)>=0 and
 r.get('displayAfterReplacement',0)>0 and r.get('displayAfterMoveMs',-1)>=0 and
 not r.get('replacePending',True) else 1)" "$result"; then
        say "PASS web-to-direct-wss: replacement audio and display followed onto the tunnel: $result"
    else
        say "FAIL web-to-direct-wss: $result"
        FAILED=1
    fi
    stop_core
    CORE_PORT=47910
    CORE_URL="wss://198.51.100.10:$CORE_PORT"
fi

# web-relay-rejoin: as web-relay, and the front restarts mid-session, so
# both legs' connections drop without an END: each joins again with its
# token, the loopback sockets kept, and the session carries on (one
# sign-in, audio still coming).
if scenario web-relay-rejoin; then
    reset_rules
    web_only natc
    start_core allow --media
    ( sleep 14; stop_front; sleep 1; start_front ) &
    BOUNCE_PID=$!
    result="$(run_session cli --media-ms 20000)"
    wait "$BOUNCE_PID" 2>/dev/null || true
    sleep 2
    joins="$(grep -c "joined again\|joins again\|leg joined" "$WORK/relay.log" 2>/dev/null || true)"
    if (( joins >= 2 )) && "$PYTHON" -c "
import json, sys
r = json.loads(sys.argv[1])
ok = (r.get('connected') and r.get('rankAfter') == 4 and r.get('handshakes') == 1
      and r.get('audioDecoded', 0) > 350)
sys.exit(0 if ok else 1)" "$result" 2>/dev/null; then
        say "PASS web-relay-rejoin: the session carried on across the legs' drop ($joins relay join lines): $result"
    else
        say "FAIL web-relay-rejoin: $result"
        tail -n 30 "$WORK/relay.log" >&2 || true
        FAILED=1
    fi
    stop_core
fi

# web-relay-standby: every path open and the relay allowed: the legs join
# at the introduction, and ICE still takes a direct pair (rank 2) or TURN
# (rank 3), never the web relay.
if scenario web-relay-standby; then
    reset_rules
    start_core allow --media
    result="$(run_session cli --media-ms 3000)"
    rank="$(field "$result" rankAfter 2>/dev/null || echo None)"
    if [[ "$rank" == "2" || "$rank" == "3" ]]; then
        say "PASS web-relay-standby: rank $rank with the web relay offered: $result"
    else
        say "FAIL web-relay-standby: rank $rank: $result"
        FAILED=1
    fi
    stop_core
fi

# web-relay-deadline: the transmit deadline over the web relay (brief
# requirement 7): the client's network passes only DNS and TCP 443, with
# DEADLINE_LOSS % loss and DEADLINE_DELAY ms each way on its link (150 ms
# round trip); TUNE requested for 60 s from the device. The Core's watchdog
# reports each keepalive's gap and any trip. A watchdog stop while TUNE is
# requested fails the scenario; the observed tails remain for operator review.
deadline_trace() {
    "$PYTHON" - "$WORK/core.log" "$WORK/session-events.log" <<'PY'
import json, sys
def events(path):
    with open(path) as lines:
        for line in lines:
            try:
                event = json.loads(line)
            except ValueError:
                continue
            if isinstance(event, dict):
                yield event
core = list(events(sys.argv[1]))
session = list(events(sys.argv[2]))
for trip in (e for e in core if e.get('event') == 'tripped' and not e.get('linkClosed')):
    at = trip['atEpochMs']
    nearby = sorted((e for e in core + session if e.get('event') in
                     ('keepaliveSent', 'keepaliveReceived', 'tripped') and
                     at - 800 <= e.get('atEpochMs', -1) <= at + 200),
                    key=lambda e: e['atEpochMs'])
    print(json.dumps(nearby, separators=(',', ':')))
PY
}
deadline_netem_add() {
    local loss="$1" delay="$2"
    local outbound=() inbound=()
    if [[ -n "${DEADLINE_SEED:-}" ]]; then
        [[ "$DEADLINE_SEED" =~ ^[0-9]+$ ]] || { say "invalid DEADLINE_SEED"; exit 2; }
        outbound=(seed "$DEADLINE_SEED")
        inbound=(seed "$((DEADLINE_SEED + 1))")
        say "NETEM seed client-out=$DEADLINE_SEED client-in=$((DEADLINE_SEED + 1)) loss=${loss}% delay=${delay}ms each way"
    fi
    in_ns natc tc qdisc add dev wan root netem loss "${loss}%" delay "${delay}ms" "${outbound[@]}"
    in_ns inet tc qdisc del dev inet-natc root 2>/dev/null || true
    in_ns inet tc qdisc add dev inet-natc root netem loss "${loss}%" delay "${delay}ms" "${inbound[@]}"
}
deadline_netem_stats() {
    local prefix="$deadline_output/netem"
    in_ns natc tc -s -j qdisc show dev wan > "$prefix-out.json"
    in_ns inet tc -s -j qdisc show dev inet-natc > "$prefix-in.json"
    in_ns cli nstat -az > "$prefix-cli-nstat.txt"
    in_ns rvsrv nstat -az > "$prefix-service-nstat.txt"
    "$PYTHON" - "$prefix-out.json" "$prefix-in.json" <<'PY'
import json, sys
for direction, path in zip(('client-out', 'client-in'), sys.argv[1:]):
    data = json.load(open(path))[0]
    packets, dropped = data.get('packets', 0), data.get('drops', 0)
    total = packets + dropped
    print(f'{direction}: packets={packets} drops={dropped} '
          f'dropFraction={(dropped / total if total else 0):.5f} file={path}')
PY
    say "TCP retrans counters: $(grep TcpRetransSegs "$prefix-cli-nstat.txt") | $(grep TcpRetransSegs "$prefix-service-nstat.txt")"
}
deadline_check() {
    "$PYTHON" - "$1" "$2" "$WORK/core.log" "$WORK/session-events.log" \
              "$WORK/front.json" "$deadline_output/check.json" <<'PY'
import json
import sys

name, result_text, core_path, session_path, front_path, output_path = sys.argv[1:]
try:
    result = json.loads(result_text)
except ValueError:
    result = {}

def events(path):
    with open(path) as lines:
        for line in lines:
            try:
                item = json.loads(line)
            except ValueError:
                continue
            if isinstance(item, dict):
                yield item

core = list(events(core_path))
session = list(events(session_path))
key = [e for e in core if e.get('event') == 'coreKeyed']
tune = [e for e in session if e.get('event') == 'tune']
heard = [e for e in core if e.get('event') == 'keepaliveReceived']
keyed_ms = (key[1]['atEpochMs'] - key[0]['atEpochMs']
            if len(key) == 2 and key[0].get('on') is True
            and key[1].get('on') is False else -1)
false_stops = [e for e in core if e.get('event') == 'tripped'
               and e.get('linkClosed') is False]
try:
    with open(front_path) as handle:
        front = json.load(handle)
except (OSError, ValueError):
    front = {}

checks = {
    'connected': result.get('connected') is True and not result.get('reason'),
    'selectedPath': result.get('rankAfter') == (4 if name.startswith('web-relay') else 1),
    'watchReadyBeforeKey': result.get('watchReadyBeforeTune') is True,
    'watchReadyAtOff': result.get('watchReadyAtOff') is True,
    'requestedSixtySeconds': result.get('tuneKeyedMs', -1) >= 59900,
    'coreSixtySeconds': keyed_ms >= 59900,
    'explicitOnOff': len(tune) == 2 and tune[0].get('on') is True
                     and tune[1].get('on') is False,
    'noFalseWatchdogStop': len(false_stops) == 0,
    'coreAcceptedHeartbeats': len(heard) >= 100,
    'auxiliarySentHeartbeats': result.get('tuneKeyedAuxiliaryKeepalives', 0) >= 500,
    'primarySentHeartbeats': result.get('tuneKeyedChannelKeepalives', 0) > 0
                              or result.get('tuneKeyedSessionKeepalives', 0) > 0,
    'mediaBeforeKey': result.get('tunePreAudioDecoded', 0) > 0
                      and result.get('tunePreDisplay', 0) > 0,
    # This one fake RX slice is half-duplex. Core intentionally has no RX
    # audio source while TUNE is on; require real decoded audio on both sides
    # of the key and display frames throughout the keyed interval.
    'displayDuringKey': result.get('tuneKeyedDisplay', 0) > 0,
    'postOffRecovery': result.get('tunePostWindowMs', -1) >= 4900
                       and result.get('tunePostAudioDecoded', 0) > 0
                       and result.get('tunePostDisplay', 0) > 0
                       and result.get('tunePostAudio') is True
                       and result.get('tunePostDisplayReady') is True
                       and result.get('tunePostHeard') is True,
}
if name.startswith('web-relay'):
    sockets = front.get('relaySockets', {})
    client = [sid for sid, entry in sockets.items()
              if entry.get('source') == '198.51.100.6'
              and entry.get('toRelay', {}).get('3', 0) > 0]
    station = [sid for sid, entry in sockets.items()
               if entry.get('source') == '198.51.100.10'
               and entry.get('fromRelay', {}).get('3', 0) > 0]
    checks['separateWatchTrafficReachedCoreSocket'] = bool(
        set(client).isdisjoint(station) and client and station)
    client_media = {sid for sid, entry in sockets.items()
                    if entry.get('source') == '198.51.100.6'
                    and entry.get('fromRelay', {}).get('2', 0) > 0}
    station_media = {sid for sid, entry in sockets.items()
                     if entry.get('source') == '198.51.100.10'
                     and entry.get('toRelay', {}).get('2', 0) > 0}
    checks['watchSocketsSeparateFromMedia'] = bool(
        client_media and station_media and client and station
        and set(client).isdisjoint(client_media)
        and set(station).isdisjoint(station_media))
    checks['realMediaRelayTraffic'] = front.get('datagrams', {}).get('2', 0) > 0
if name == 'web-relay-second-rx-deadline':
    receivers = [e for e in core if e.get('event') == 'secondRxReady']
    observed = [e for e in session if e.get('event') == 'tuneCoreObservedOn']
    checks['distinctListeningReceiver'] = len(receivers) == 1 and (
        receivers[0].get('txSlice') != receivers[0].get('listeningSlice')
        and receivers[0].get('txStream') != receivers[0].get('listeningStream'))
    checks['decodedAudioDuringKey'] = (
        result.get('tuneKeyedAudioDecoded', 0) >= 500
        and result.get('tuneAudioCounterReset') is False)
    checks['heldAfterObservedCoreOn'] = (
        len(observed) == 1 and len(tune) == 2
        and tune[0].get('atEpochMs', 0) <= observed[0].get('atEpochMs', -1)
        < tune[1].get('atEpochMs', 0)
        and result.get('tuneHeldAfterCoreObservedMs', -1) >= 59900)
    checks['audibleAudioThroughoutKey'] = (
        result.get('tuneKeyedAudioFullSeconds', 0) >= 59
        and result.get('tuneKeyedAudioAudibleSeconds')
            == result.get('tuneKeyedAudioFullSeconds')
        and result.get('tuneKeyedAudioLongestSilentMs', 100000) <= 1000)

summary = {
    'scenario': name, 'seed': int(__import__('os').environ.get('DEADLINE_SEED', '0')),
    'requestedTuneMs': 60000, 'clientKeyedMs': result.get('tuneKeyedMs', -1),
    'coreKeyedMs': keyed_ms, 'coreHeartbeats': len(heard),
    'auxiliarySent': result.get('tuneKeyedAuxiliaryKeepalives', 0),
    'falseWatchdogStops': len(false_stops), 'checks': checks,
    'result': result,
}
with open(output_path, 'w') as output:
    json.dump(summary, output, indent=2, sort_keys=True)
print(json.dumps({key: value for key, value in summary.items() if key != 'result'},
                 sort_keys=True))
sys.exit(0 if all(checks.values()) else 1)
PY
}
deadline_save_artifacts() {
    local file
    for file in core.log session.log session-events.log front.json front.log \
                relay.log rendezvous.log; do
        if [[ -f "$WORK/$file" ]]; then
            cp "$WORK/$file" "$deadline_output/$file"
        fi
    done
}
if scenario web-relay-deadline; then
    deadline_name=web-relay-deadline
    deadline_args=(--follow-media-ms 65000 --tune-ms 60000 --require-watch
                   --ready-timeout-ms 20000)
    for condition in ${DEADLINE_CONDITIONS:-"2:75" "3:75"}; do
        loss="${condition%%:*}"
        delay="${condition##*:}"
        deadline_output="${DEADLINE_OUT:-$SOURCE/build-r5-linux/watch-acceptance}/$deadline_name-$loss-$delay-${DEADLINE_SEED:-unseeded}"
        mkdir -p "$deadline_output"
        reset_rules
        web_only natc
        deadline_netem_add "$loss" "$delay"
        start_core allow --media --keyable
        result="$(run_session cli "${deadline_args[@]}")"
        sleep 2
        stats="$(grep '"event":"keepalives"' "$WORK/core.log" | tail -n 1 || true)"
        trips="$(grep -c '"event":"tripped"' "$WORK/core.log" || true)"
        trip_events="$(grep '"event":"tripped"' "$WORK/core.log" || true)"
        tune_events="$(grep '"event":"tune"' "$WORK/session-events.log" || true)"
        say "MEASURE $deadline_name loss=${loss}% delay=${delay}ms: $stats trips=$trips tripEvents=$trip_events tuneEvents=$tune_events session=$result"
        if ! deadline_check "$deadline_name" "$result"; then
            say "FAIL $deadline_name: media or keepalive route did not match the scenario"
            FAILED=1
        fi
        if grep -q '"event":"tripped".*"linkClosed":false' "$WORK/core.log"; then
            say "FAIL $deadline_name: the Core stopped TUNE before its requested release"
            say "TRACE $deadline_name: $(deadline_trace)"
            FAILED=1
        fi
        deadline_netem_stats
        deadline_save_artifacts
        in_ns inet tc qdisc del dev inet-natc root 2>/dev/null || true
        stop_core
    done
fi

if scenario direct-wss-deadline; then
    deadline_name=direct-wss-deadline
    deadline_args=(--follow-media-ms 65000 --tune-ms 60000 --require-watch
                   --ready-timeout-ms 20000)
    for condition in ${DEADLINE_CONDITIONS:-"2:75" "3:75"}; do
        loss="${condition%%:*}"
        delay="${condition##*:}"
        deadline_output="${DEADLINE_OUT:-$SOURCE/build-r5-linux/watch-acceptance}/$deadline_name-$loss-$delay-${DEADLINE_SEED:-unseeded}"
        mkdir -p "$deadline_output"
        reset_rules
        web_only natc
        CORE_PORT=443
        CORE_URL="wss://198.51.100.10:$CORE_PORT"
        forward_core
        deadline_netem_add "$loss" "$delay"
        start_core allow --listen "$CORE_PORT" --media --keyable
        result="$(run_session cli --direct "$CORE_URL" "${deadline_args[@]}")"
        sleep 2
        stats="$(grep '"event":"keepalives"' "$WORK/core.log" | tail -n 1 || true)"
        trips="$(grep -c '"event":"tripped"' "$WORK/core.log" || true)"
        trip_events="$(grep '"event":"tripped"' "$WORK/core.log" || true)"
        tune_events="$(grep '"event":"tune"' "$WORK/session-events.log" || true)"
        say "MEASURE $deadline_name loss=${loss}% delay=${delay}ms: $stats trips=$trips tripEvents=$trip_events tuneEvents=$tune_events session=$result"
        if ! deadline_check "$deadline_name" "$result"; then
            say "FAIL $deadline_name: media or keepalive route did not match the scenario"
            FAILED=1
        fi
        if grep -q '"event":"tripped".*"linkClosed":false' "$WORK/core.log"; then
            say "FAIL $deadline_name: the Core stopped TUNE before its requested release"
            say "TRACE $deadline_name: $(deadline_trace)"
            FAILED=1
        fi
        deadline_netem_stats
        deadline_save_artifacts
        in_ns inet tc qdisc del dev inet-natc root 2>/dev/null || true
        stop_core
        CORE_PORT=47910
        CORE_URL="wss://198.51.100.10:$CORE_PORT"
    done
fi

# A separate fixed-seed stress row for a production-supported second RX
# receiver. The TX-bound slice withdraws on MOX; the other receiver keeps
# remote Opus audio audible while watch and display traffic share the relay.
# The four original deadline rows above are intentionally unchanged.
if [[ "$ONLY" == web-relay-second-rx-deadline ]]; then
    deadline_name=web-relay-second-rx-deadline
    DEADLINE_SEED="${DEADLINE_SEED:-20261015}"
    deadline_output="${DEADLINE_OUT:-$SOURCE/build-r5-linux/watch-acceptance}/$deadline_name-3-75-$DEADLINE_SEED"
    mkdir -p "$deadline_output"
    reset_rules
    web_only natc
    deadline_netem_add 3 75
    start_core allow --media --second-rx --keyable
    result="$(run_session cli --follow-media-ms 65000 --tune-ms 60000 \
                           --require-watch --require-keyed-audio --ready-timeout-ms 20000)"
    sleep 2
    stats="$(grep '"event":"keepalives"' "$WORK/core.log" | tail -n 1 || true)"
    say "MEASURE $deadline_name: $stats session=$result"
    if ! deadline_check "$deadline_name" "$result"; then
        say "FAIL $deadline_name: a strict Core-key, media or watch assertion failed"
        FAILED=1
    fi
    if grep -q '"event":"tripped".*"linkClosed":false' "$WORK/core.log"; then
        say "FAIL $deadline_name: Core stopped TUNE before requested release"
        say "TRACE $deadline_name: $(deadline_trace)"
        FAILED=1
    fi
    deadline_netem_stats
    deadline_save_artifacts
    in_ns inet tc qdisc del dev inet-natc root 2>/dev/null || true
    stop_core
fi

# ── Plan Task 29 Step 1: the relay floor measurement ─────────────────

# floor-measure: only when named (--only floor-measure), with --probe; run
# by tests/scripts/floor-measurement.sh, which reads the settings below
# from the environment and summarises FLOOR_OUT/results.jsonl.
#   FLOOR_OUT            where results.jsonl and the logs go (required)
#   FLOOR_OPTIONS        "none c e": none is today's TURN over UDP, for
#                        reference (UDP open, direct blocked); c and e run
#                        with the client's network passing only DNS and
#                        TCP 443
#   FLOOR_CONDITIONS     "LOSS:DELAY ..." (percent, ms), netem on each
#                        direction of the client's link, default
#                        "0.5:30 0.5:80 1:30 1:80"
#   FLOOR_RATES          the payload rates for the first condition,
#                        default "145 520"; the others run at the first
#   FLOOR_DURATION_S     seconds of traffic per run, default 60
#   FLOOR_RESET_AT_S     c and e: one more run at the first condition and
#                        rate, its floor connection reset this many seconds
#                        in (0: none), default 15
floor_rules() {
    local option="$1" loss="$2" delay="$3"
    reset_rules
    in_ns inet tc qdisc del dev inet-natc root 2>/dev/null || true
    if [[ "$option" == "none" ]]; then
        in_ns inet nft -f - <<EOF
table inet filter {
    chain forward {
        type filter hook forward priority 0;
        ip saddr 198.51.100.6 ip daddr 198.51.100.10 meta l4proto udp drop
        ip saddr 198.51.100.10 ip daddr 198.51.100.6 meta l4proto udp drop
    }
}
EOF
    else
        in_ns natc nft insert rule inet filter forward iifname "lan" meta l4proto udp udp dport != 53 drop
        in_ns natc nft insert rule inet filter forward iifname "lan" meta l4proto tcp tcp dport != 443 drop
    fi
    if [[ "$loss" != "0" || "$delay" != "0" ]]; then
        in_ns natc tc qdisc add dev wan root netem loss "${loss}%" delay "${delay}ms"
        in_ns inet tc qdisc add dev inet-natc root netem loss "${loss}%" delay "${delay}ms"
    fi
}

cpu_ticks() {
    awk '{print $14 + $15}' "/proc/$1/stat" 2>/dev/null || echo 0
}

floor_run() {
    local option="$1" loss="$2" delay="$3" rate="$4" reset="$5"
    local tag="$option-$loss-$delay-$rate-$reset"
    floor_rules "$option" "$loss" "$delay"
    rm -f "$WORK/probe-core-id"
    ip netns exec h-sta "$PROBE" core --dir "$WORK/probe-core" --server "$SERVER" \
        --paired "$PROBE_KEY" --floor "$option" --relay-url "wss://rv.harness.test/v1/relay" \
        --report-after-s "$(( FLOOR_DURATION_S + 2 ))" --id-file "$WORK/probe-core-id" --ca "$WORK/ca.pem" >"$FLOOR_OUT/core-$tag.log" 2>&1 &
    local core_pid=$!
    PIDS+=("$core_pid")
    for _ in $(seq 1 100); do
        [[ -s "$WORK/probe-core-id" ]] && break
        sleep 0.2
    done
    local turn_before tls_before service_before
    turn_before="$(cpu_ticks "$COTURN_PID")"
    tls_before="$(cpu_ticks "${COTURN_TLS_PID:-0}")"
    service_before="$(cpu_ticks "$RENDEZVOUS_PID")"
    local reset_args=()
    if (( reset > 0 )); then
        reset_args=(--reset-at-s "$reset")
    fi
    local device
    device="$(in_ns cli "$PROBE" device --dir "$WORK/probe-device" --server "$SERVER" \
        --station-id "$(cat "$WORK/probe-core-id" 2>/dev/null)" --floor "$option" \
        --turn-tls rvtls.harness.test:443 --relay-url "wss://rv.harness.test/v1/relay" \
        --rate "$rate" --duration-s "$FLOOR_DURATION_S" "${reset_args[@]}" \
        --ca "$WORK/ca.pem" 2>>"$FLOOR_OUT/device-$tag.log" | tail -n 1 || true)"
    sleep 3
    local core
    core="$(grep '"event":"report"' "$FLOOR_OUT/core-$tag.log" | tail -n 1 || true)"
    local ticks turn_cpu tls_cpu service_cpu
    ticks="$(getconf CLK_TCK)"
    turn_cpu=$(( $(cpu_ticks "$COTURN_PID") - turn_before ))
    tls_cpu=$(( $(cpu_ticks "${COTURN_TLS_PID:-0}") - tls_before ))
    service_cpu=$(( $(cpu_ticks "$RENDEZVOUS_PID") - service_before ))
    kill "$core_pid" 2>/dev/null || true
    wait "$core_pid" 2>/dev/null || true
    "$PYTHON" - "$option" "$loss" "$delay" "$rate" "$reset" "${device:-null}" "${core:-null}" \
        "$turn_cpu" "$tls_cpu" "$service_cpu" "$ticks" >>"$FLOOR_OUT/results.jsonl" <<'PY'
import json, sys
a = sys.argv
def load(text):
    try:
        return json.loads(text)
    except ValueError:
        return None
ticks = float(a[11])
print(json.dumps({
    "option": a[1], "lossPercent": float(a[2]), "delayMs": float(a[3]),
    "rateKbps": int(a[4]), "resetAtS": int(a[5]),
    "device": load(a[6]), "core": load(a[7]),
    "serverCpuSeconds": {"coturnUdp": int(a[8]) / ticks, "coturnTls": int(a[9]) / ticks,
                         "service": int(a[10]) / ticks},
}))
PY
    local connected
    connected="$(field "${device:-null}" connected 2>/dev/null || echo None)"
    if [[ "$connected" == "True" && ( -n "$core" || "$reset" -gt 0 ) ]]; then
        say "PASS floor $tag: connected, measured"
    else
        say "FAIL floor $tag: device ${device:-none}, core ${core:-none}"
        FAILED=1
    fi
}

if [[ "$ONLY" == "floor-measure" ]]; then
    if [[ -z "$PROBE" || -z "${FLOOR_OUT:-}" ]]; then
        say "floor-measure needs --probe and FLOOR_OUT"
        exit 2
    fi
    mkdir -p "$FLOOR_OUT"
    : >"$FLOOR_OUT/results.jsonl"
    PROBE_KEY="$("$PROBE" key --dir "$WORK/probe-device")"
    read -r -a options <<<"${FLOOR_OPTIONS:-none c e}"
    read -r -a conditions <<<"${FLOOR_CONDITIONS:-0.5:30 0.5:80 1:30 1:80}"
    read -r -a rates <<<"${FLOOR_RATES:-145 520}"
    FLOOR_DURATION_S="${FLOOR_DURATION_S:-60}"
    reset_at="${FLOOR_RESET_AT_S:-15}"
    first="${conditions[0]}"
    for option in "${options[@]}"; do
        for rate in "${rates[@]}"; do
            floor_run "$option" "${first%%:*}" "${first##*:}" "$rate" 0
        done
        for condition in "${conditions[@]:1}"; do
            floor_run "$option" "${condition%%:*}" "${condition##*:}" "${rates[0]}" 0
        done
        if [[ "$option" != "none" ]] && (( reset_at > 0 )); then
            floor_run "$option" "${first%%:*}" "${first##*:}" "${rates[0]}" "$reset_at"
        fi
    done
    # Option (E)'s relay saw only ciphertext: no frame it carried held the
    # text every display frame carries.
    plaintext="$(grep -o 'plaintext=[0-9]*' "$WORK/rendezvous.log" | tail -n 1 | cut -d= -f2 || true)"
    if [[ " ${options[*]} " == *" e "* ]]; then
        if [[ "${plaintext:-missing}" == "0" ]]; then
            say "PASS floor-measure: the relay carried no plaintext"
        else
            say "FAIL floor-measure: the relay's plaintext count is ${plaintext:-missing}"
            FAILED=1
        fi
    fi
    cp "$WORK/rendezvous.log" "$WORK/coturn.log" "$FLOOR_OUT/" 2>/dev/null || true
    cp "$WORK/coturn-tls.log" "$FLOOR_OUT/" 2>/dev/null || true
fi

if (( FAILED )); then
    say "logs:"
    for log in rendezvous coturn station client core session unbound; do
        echo "── $log ──" >&2
        tail -n 40 "$WORK/$log.log" >&2 2>/dev/null || true
    done
    exit 1
fi
say "all scenarios passed"
