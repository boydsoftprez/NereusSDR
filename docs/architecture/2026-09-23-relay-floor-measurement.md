# The relay floor: measurement and choice

This document is Step 1 of the [iPhone app plan](2026-09-23-iphone-app-plan.md)
Task 29 (R-IOS-16, R-IOS-08): the options for the web-only fallback, the
relay rung that still works on a network that passes nothing but DNS and
TCP 443, measured side by side before one is chosen (the Core/GUI session's
R5 decision D2). The design it serves is the
[station identity and pairing design](2026-08-02-remote-station-identity-and-pairing-design.md)
section 5.4 (two relay rungs, raced; "TLS over TCP 443, the guaranteed
floor"), which left open whether the floor is TURN or a relay of our own
(its open question 7). [D38](2026-09-23-iphone-app-design.md) puts the
rendezvous and the relay on the website's server.

JJ's choice and its reason go in the last section. Step 2 (the chosen
floor, the path racer, the make-before-break switch and the media
`replace`) waits for it.

## 1. The options

Every option keeps ICE, DTLS, SCTP and SRTP end to end between the device
and the Core. The relay only ever carries their ciphertext.

- **(B) libjuice patched to speak TURN over TCP and TLS.** libjuice
  (MPL-2.0, pinned at `3c40a354`) speaks TURN over UDP only, and
  libdatachannel v0.24.5 refuses a TCP or TLS relay with libjuice
  (`src/impl/icetransport.cpp:159`). (B) teaches libjuice a TCP connection
  per relay, TURN's stream framing, and TLS, in each of its three
  connection back ends (`conn_poll`, `conn_mux`, `conn_thread`), and lifts
  libdatachannel's refusal. The relay is coturn's TLS listener.
- **(C) a local shim.** libjuice is given a TURN server on a loopback UDP
  port; the shim carries each datagram (STUN or ChannelData) over one TLS
  connection to coturn's TLS listener and back, framing them as TURN does
  on a stream. No library changes. The relay is coturn's TLS listener.
- **(E) our own relay on the rendezvous service.** Each end opens a
  WebSocket to `wss://rv.<domain>/v1/relay` naming the same pairing token;
  the service forwards each binary frame to the other end. At each end a
  loopback UDP socket stands between the WebSocket and the ICE agent: the
  device's agent is given that socket as a remote candidate, and the Core's
  agent learns the Core's socket as a peer-reflexive one. No library
  changes, no TURN.

libnice is excluded: it needs GLib, which is LGPL, and cannot go into the
App Store app.

## 2. How it was measured

On the traversal harness (`tests/scripts/traversal-harness.sh`, in the
`nereus-traversal:trixie` container with `--network none`), by
`tests/scripts/floor-measurement.sh`, which runs the harness's
`floor-measure` scenario and prints the tables below.

- **The prototypes.** (C) is `tests/tools/floor/TurnTlsShimPrototype`, (E)
  is `tests/tools/floor/WsRelayPrototype` with the service side in
  `tests/tools/floor_relay_prototype.py` (the service exactly as it runs,
  plus a `/v1/relay` path). Both are test tooling, not product code.
- **(B) is measured through (C).** (B) puts the same bytes on the same wire
  to the same listener, allocation for allocation; (C) adds one loopback
  UDP hop and one pass through the Qt event loop at the device. (C)'s
  numbers are (B)'s to within that hop (well under a millisecond on this
  computer). What (B) alone costs (code, patched upstream lines, a TLS
  library inside libjuice) is estimated from libjuice's source in section
  4, not built: building it is a week of C in three back ends for numbers
  the wire already gives.
- **The server side**, as D38 would need it: coturn's TLS listener on TCP
  443 at a second address of the service's host (`rvtls.harness.test`,
  198.51.100.66), since the service's TLS front holds TCP 443 on the
  service's own address; the WebSocket relay behind that same front. The
  front sends each write at once (TCP_NODELAY, as Caddy does).
- **The network.** The device's network passes only DNS and TCP 443 (UDP
  except port 53, and TCP except port 443, dropped at its router). netem
  adds the loss and the delay on both directions of the device's link:
  0.5 % and 1 % loss, 30 ms and 80 ms each way. The Core's side is clean,
  with UDP. For reference, "today" is TURN over UDP with UDP open and the
  direct path blocked, under the same loss and delay.
- **The traffic.** One media connection, introduced through the service
  as the traversal peer does, on the Core's own
  `LibDataChannelMediaTransport`: Opus-shaped RTP every 20 ms (72 bytes),
  a display frame 30 times a second on the display channel (never
  retransmitted), sized so that audio and display make 145 or 520 kbit/s
  of payload (the pairing design's two session shapes), and a 32-byte keyed
  event every 100 ms on the `tx` channel (the transmit keepalive's
  cadence). It flows device to Core: an introduced connection's offerer is
  the device, and the offerer sends the main audio line. The impairments
  are the same both ways, so the direction does not change what is
  measured. Arrival times are taken at the Core as the transport hands each
  message over (it drains every 2 ms).
- **What is reported**: inter-arrival p50, p95, p99 and maximum for each
  stream over 60 seconds, what arrived of what was sent, the worst stall
  (the longest gap in any stream), the cold time from the device's start
  (before it reaches the service) to the first audio packet at the Core,
  CPU time at each end and on the server, and, in a run of its own, what a
  TCP reset of the floor connection 15 seconds in cost.
- **Every display frame carries a known text**; the relay prototype counts
  frames holding it. It found none: the relayed traffic never contains the
  plaintext.

## 3. Results

Run 2026-09-26 at `d2de232d` plus this step's tooling, one run of 60
seconds per row, in `nereus-traversal:trixie` on JJ's Mac (Apple silicon,
the container held to 6 cores), `--network none`. The Mac was shared: its
1-minute load average was 50.7 when the run started and 24.1 when it ended
(18 cores), so every row saw the same background and the rows compare with
each other; absolute tails would be a little lower on a quiet machine. The
whole traversal harness passed after the run (13 of 13 scenarios).

### 3.1 Inter-arrival (ms: p50 / p95 / p99 / max) and what arrived

| Option | Loss, delay each way | Rate | Audio (20 ms) | Display (33 ms) | Keyed (100 ms) | Worst stall | Arrived (audio, display, keyed) | Cold first audio |
|---|---|---|---|---|---|---|---|---|
| today (TURN/UDP) | 0.5 %, 30 ms | 145 kbit/s | 20.0 / 22.2 / 23.4 / 42.2 | 33.7 / 34.4 / 36.3 / 66.9 | 100.0 / 102.2 / 104.0 / 202.0 | 202.0 | 99.5 %, 99.6 %, 99.3 % | 1108.8 |
| today (TURN/UDP) | 0.5 %, 30 ms | 520 kbit/s | 20.0 / 22.1 / 22.3 / 42.1 | 33.8 / 36.4 / 125.8 / 130.4 | 100.0 / 102.4 / 216.1 / 394.2 | 394.2 | 99.6 %, 98.6 %, 99.3 % | 1016.8 |
| today (TURN/UDP) | 0.5 %, 80 ms | 145 kbit/s | 20.0 / 22.1 / 22.4 / 42.0 | 32.4 / 36.0 / 66.0 / 84.3 | 100.0 / 112.0 / 197.7 / 200.1 | 200.1 | 99.5 %, 99.5 %, 98.8 % | 2258.8 |
| today (TURN/UDP) | 1 %, 30 ms | 145 kbit/s | 20.0 / 22.2 / 28.5 / 153.3 | 33.6 / 35.2 / 39.2 / 175.1 | 100.0 / 102.2 / 200.0 / 200.3 | 200.3 | 99.1 %, 99.2 %, 98.8 % | 1157.4 |
| today (TURN/UDP) | 1 %, 80 ms | 145 kbit/s | 20.0 / 36.1 / 90.5 / 830.2 | 33.0 / 63.9 / 150.5 / 766.7 | 100.0 / 149.1 / 252.6 / 1141.7 | 1141.7 | 98.8 %, 99.2 %, 99.1 % | 3444.3 |
| (C) shim, TURN/TLS | 0.5 %, 30 ms | 145 kbit/s | 20.0 / 23.5 / 35.6 / 108.2 | 33.0 / 36.8 / 85.6 / 126.1 | 100.0 / 105.4 / 171.8 / 185.2 | 185.2 | 100.0 %, 100.0 %, 100.0 % | 1295.3 |
| (C) shim, TURN/TLS | 0.5 %, 30 ms | 520 kbit/s | 20.0 / 26.3 / 66.3 / 122.3 | 32.8 / 42.1 / 95.9 / 129.6 | 100.0 / 117.6 / 178.0 / 186.0 | 186.0 | 100.0 %, 100.0 %, 100.0 % | 1234.1 |
| (C) shim, TURN/TLS | 0.5 %, 80 ms | 145 kbit/s | 20.0 / 28.2 / 52.1 / 229.3 | 32.3 / 47.4 / 216.1 / 257.9 | 100.0 / 136.0 / 299.9 / 323.7 | 323.7 | 100.0 %, 100.0 %, 100.0 % | 2867.4 |
| (C) shim, TURN/TLS | 1 %, 30 ms | 145 kbit/s | 20.0 / 30.1 / 99.7 / 126.1 | 32.4 / 51.2 / 111.1 / 130.1 | 100.0 / 127.9 / 180.0 / 192.2 | 192.2 | 100.0 %, 100.0 %, 100.0 % | 1198.5 |
| (C) shim, TURN/TLS | 1 %, 80 ms | 145 kbit/s | 19.8 / 48.1 / 210.3 / 244.2 | 32.0 / 63.5 / 227.8 / 261.2 | 99.9 / 232.5 / 293.3 / 315.9 | 315.9 | 100.0 %, 100.0 %, 100.0 % | 2792.7 |
| (E) WebSocket relay | 0.5 %, 30 ms | 145 kbit/s | 19.5 / 42.1 / 43.3 / 114.8 | 41.9 / 63.9 / 100.1 / 129.5 | 108.0 / 138.0 / 178.4 / 205.9 | 205.9 | 100.0 %, 99.9 %, 100.0 % | 1596.0 |
| (E) WebSocket relay | 0.5 %, 30 ms | 520 kbit/s | 19.1 / 42.0 / 47.7 / 184.3 | 36.1 / 61.4 / 106.3 / 232.0 | 100.0 / 140.2 / 166.9 / 203.4 | 232.0 | 100.0 %, 99.9 %, 100.0 % | 1610.3 |
| (E) WebSocket relay | 0.5 %, 80 ms | 145 kbit/s | 18.0 / 42.2 / 93.8 / 238.2 | 23.9 / 67.6 / 221.3 / 286.8 | 102.3 / 157.9 / 283.9 / 343.1 | 343.1 | 100.0 %, 99.9 %, 100.0 % | 3685.7 |
| (E) WebSocket relay | 1 %, 30 ms | 145 kbit/s | 19.0 / 42.3 / 90.8 / 144.4 | 30.2 / 65.9 / 118.8 / 152.3 | 104.2 / 142.0 / 178.0 / 192.2 | 192.2 | 100.0 %, 99.9 %, 100.0 % | 1545.3 |
| (E) WebSocket relay | 1 %, 80 ms | 145 kbit/s | 12.2 / 45.1 / 202.2 / 261.2 | 19.9 / 80.9 / 229.7 / 270.5 | 98.0 / 224.5 / 299.8 / 334.4 | 334.4 | 99.9 %, 99.9 %, 100.0 % | 3727.4 |

### 3.2 CPU (seconds of CPU per second of session)

| Option | Loss, delay | Rate | Core | Device | coturn UDP | coturn TLS | Service |
|---|---|---|---|---|---|---|---|
| today (TURN/UDP) | 0.5 %, 30 ms | 145 kbit/s | 0.043 | 0.041 | 0.009 | 0.001 | 0.000 |
| today (TURN/UDP) | 0.5 %, 30 ms | 520 kbit/s | 0.042 | 0.041 | 0.009 | 0.000 | 0.000 |
| today (TURN/UDP) | 0.5 %, 80 ms | 145 kbit/s | 0.039 | 0.036 | 0.008 | 0.000 | 0.000 |
| today (TURN/UDP) | 1 %, 30 ms | 145 kbit/s | 0.045 | 0.041 | 0.010 | 0.000 | 0.000 |
| today (TURN/UDP) | 1 %, 80 ms | 145 kbit/s | 0.133 | 0.110 | 0.044 | 0.005 | 0.001 |
| (C) shim, TURN/TLS | 0.5 %, 30 ms | 145 kbit/s | 0.059 | 0.071 | 0.001 | 0.019 | 0.000 |
| (C) shim, TURN/TLS | 0.5 %, 30 ms | 520 kbit/s | 0.068 | 0.084 | 0.001 | 0.024 | 0.000 |
| (C) shim, TURN/TLS | 0.5 %, 80 ms | 145 kbit/s | 0.040 | 0.049 | 0.001 | 0.013 | 0.000 |
| (C) shim, TURN/TLS | 1 %, 30 ms | 145 kbit/s | 0.048 | 0.058 | 0.001 | 0.015 | 0.000 |
| (C) shim, TURN/TLS | 1 %, 80 ms | 145 kbit/s | 0.046 | 0.056 | 0.001 | 0.012 | 0.000 |
| (E) WebSocket relay | 0.5 %, 30 ms | 145 kbit/s | 0.046 | 0.049 | 0.001 | 0.000 | 0.027 |
| (E) WebSocket relay | 0.5 %, 30 ms | 520 kbit/s | 0.055 | 0.060 | 0.001 | 0.001 | 0.034 |
| (E) WebSocket relay | 0.5 %, 80 ms | 145 kbit/s | 0.065 | 0.071 | 0.001 | 0.001 | 0.034 |
| (E) WebSocket relay | 1 %, 30 ms | 145 kbit/s | 0.077 | 0.083 | 0.001 | 0.001 | 0.042 |
| (E) WebSocket relay | 1 %, 80 ms | 145 kbit/s | 0.060 | 0.070 | 0.001 | 0.001 | 0.030 |

### 3.3 A TCP reset of the floor connection

| Option | Reset at | Largest audio gap after | First audio after | Largest display gap | Largest keyed gap | Floor connections | Media connection failed at |
|---|---|---|---|---|---|---|---|
| (C) shim, TURN/TLS | 15 s | the rest of the run | never (audio stopped at the reset) | - | - | 2 | never |
| (E) WebSocket relay | 15 s | 260.8 | 13.3 | 608.0 | 671.2 | 2 | never |

Notes on the tables:

- **Arrived** is at the Core over what the device handed the transport.
  Through a TCP floor nothing is lost on the way (the stream retransmits);
  over UDP the display and keyed channels never retransmit, so their losses
  show.
- **Keyed** p50 of 104 to 108 ms for (E) at 30 ms delay (every keyed
  event arrived) is not explained by what the prototype records; the p95
  and maximum are the numbers that bound a keyed session's watchdog.
- **CPU** is CPU seconds per second of session on one Apple silicon core,
  not a Pi 4. At the Core, (C) and (B) change nothing (the Core still
  reaches the relay over UDP), so its (C) figures are today's and their
  spread is noise; (E) adds a TLS WebSocket leg at the Core. **Pi 4 CPU at
  145 and 520 kbit/s is pending**: it needs the Pi 4 bench, which only the
  controller or JJ may touch.
- **The reset** aborts the device's floor connection 15 seconds in and
  opens a new one at once. (C) opened its new TLS connection, but coturn had
  deleted the allocation with the old one (an allocation belongs to its TCP
  connection, RFC 8656 section 3), so every datagram libjuice sent after it
  went nowhere: audio stopped at the reset and never came back, and the
  media connection did not even report a failure in the 45 seconds left.
  (B) behaves the same way, since it is the same allocation on the same
  kind of connection. Recovering either needs the stall detected and a new
  connection made (a cold start: 1.2 to 2.9 s above, plus the detection).
  (E) reconnected under the same token and the loopback addresses did not
  change, so ICE never noticed: 261 ms without audio, 608 ms without
  display, then nothing lost.
- **The plaintext check** passed: over every (E) run the relay carried
  frames holding the display frames' known text zero times.

## 4. Cost, licences and the server

| | (B) libjuice TURN/TCP+TLS | (C) local shim | (E) our WebSocket relay |
|---|---|---|---|
| NereusSDR code (prototype, code lines without comments) | none new beyond (C)'s use of the relay | 194 (C++: shim 150, header 44) | 127 C++ (relay leg 87, header 40) plus 92 Python on the service |
| Patched upstream lines | estimated 900 to 1500 in about 6 files: a TCP socket and TLS state per relay in each of libjuice's three connection back ends (`conn_poll.c` 679 lines, `conn_mux.c` 703, `conn_thread.c`), TURN's stream framing and the send path in `agent.c` (about 120 TURN references), a TLS library linked into libjuice (it has none today), and lifting libdatachannel's refusal (`icetransport.cpp:159`) | 0 | 0 |
| Compiled size (arm64, `__text` of the prototype) | not built; libjuice grows by its TLS glue, and on the phone it needs a TLS library of its own | 6.6 KB | 3.3 KB |
| New libraries | a TLS library inside libjuice (OpenSSL on the desktop, already linked; on the phone, OpenSSL built for iOS or Apple's TLS wired into C) | none (Qt Network, already linked) | none (Qt WebSockets, already linked) |
| Licence, the Core and desktop (GPLv3) | MPL-2.0 files changed: the changed files must be published, as Task 28's libjuice change already is | all NereusSDR code | all NereusSDR code |
| Licence, the App Store app | MPL-2.0 is on the app's allowed list; the changed files published | NereusSDR code under the app's licence | NereusSDR code under the app's licence |
| The server (D38) | coturn's TLS listener on TCP 443, which the website's Caddy holds: a second IPv4 and IPv6 address on the server, or a splitter by TLS name (SNI) in front of Caddy that every website byte then passes through; a certificate for the relay's own name, renewed outside Caddy | the same as (B) | nothing new: `/v1/relay` rides `rv.<domain>` behind Caddy, which already sends every request for that name to the service |
| What it costs the server per floor session (from 3.2) | coturn TLS, about 0.013 to 0.024 CPU s/s | the same | the Python service, about 0.027 to 0.042 CPU s/s: about twice coturn's, still some 25 to 35 floor sessions per core |
| Relay allocations (the quota of 4 per Core) | uses them, as TURN/UDP does | the same | none: the floor is outside coturn |
| A TCP reset (3.3) | the allocation is lost; a new connection is needed | the same | resumes in about a quarter second |
| Reached through | a second name or address on TCP 443 | the same | the rendezvous's own name and connection type: a device that reached the service at all can use it |

What the phone's side (Task 29a) would carry: (B) the patched libjuice
built for iOS with a TLS library; (C) a loopback UDP socket and a TLS
connection (Network.framework), about the size of the desktop's; (E) a
loopback UDP socket and a WebSocket (URLSessionWebSocketTask or
Network.framework), about the size of the desktop's. (C) and (E) need of
the phone's ICE library only that it take a TURN server (C) or a remote host
candidate (E) on 127.0.0.1.

## 5. What the numbers say

- **On a lossy network all three floors beat today's TURN/UDP on the
  worst stall** once delay grows: at 1 % loss and 80 ms, TURN/UDP stalled
  1142 ms (SCTP recovering the display and keyed channels' losses late),
  (C) 316 ms, (E) 334 ms. At 30 ms, the TCP floors' tails are longer than
  UDP's (head-of-line blocking behind a retransmitted segment) but their
  worst stall stays at or under 232 ms.
- **(C) (and so (B)) has the tighter jitter at low delay**: audio p95 23.5
  to 30 ms against (E)'s 42 ms at 30 ms delay. (E) has two TCP legs (the
  Core's own leg to the service as well as the device's) and a Python hop;
  at 80 ms the two are within a few ms of each other (p99 audio 52 against
  94 at 0.5 %, 210 against 202 at 1 %). A jitter buffer of 60 ms covers
  either at p99 with low delay; at 80 ms with 1 % loss both need about
  250 ms, or a late packet is a gap.
- **Cold start**: (E) was 300 to 900 ms slower (1.55 to 3.73 s against
  1.20 to 2.87 s). The prototype opens the device's WebSocket only when the
  Core's answer arrives; opening it at the introduction, as a product would,
  takes most of that back.
- **A reset is where they part.** A TCP connection through a mobile
  carrier or a hotel's middlebox does get reset. (E) rides through it in a
  quarter second; (C) and (B) lose their allocation and, as measured, the
  media connection goes silent without failing, so the product would need
  its own stall detection and a full reconnect, seconds of silence at best.
- **The server**: (E) is the only one that needs no change to D38's server;
  (B) and (C) both need a second address or an SNI splitter in front of the
  website, and a second certificate. (E) costs about twice the server CPU
  per floor session, which is small at the floor's expected use (the last
  rung, raced, used only when nothing else works).
- **Code**: (B) is by far the largest and the only one that patches
  upstream, in three back ends, on both platforms, and would have to be
  carried across every libjuice update. (C) and (E) are small and ours.

## 6. Recommendation

**(E), our own relay over a WebSocket on the rendezvous service**, with
the Step 2 work closing its two gaps:

1. It works wherever the rendezvous works: same name, same port, same kind
   of connection the device already made to reach the service. (B) and (C)
   add a second name on TCP 443 that a restrictive network may treat
   differently.
2. It survives a TCP reset (261 ms without audio, nothing lost) where
   TURN over TLS loses its allocation and goes silent.
3. It needs nothing new on D38's server, and uses none of coturn's per-Core
   allocations (Task 28's concern: a relayed session already takes all 4).
4. No upstream patch to carry, and no TLS library inside libjuice on the
   phone.

Its costs, for JJ to weigh: its jitter at low delay is higher (audio p95
42 ms against (C)'s 24 to 30 ms, similar at 80 ms), which the jitter buffer
absorbs; the service carries relayed bytes in Python, about twice coturn's
CPU for the same session; and the relay is our code to secure: Step 2 gives
it a relay grant bound to the introduction (both ends already authenticated
to the service), per-session byte and rate limits, the data-use counting the
relay already has, and an idle limit. Step 2 also opens the device's
WebSocket at the introduction, not at the answer (the cold-start gap).

(C) is the fallback if JJ prefers TURN: its numbers are (B)'s without
patching upstream, at the price of a second address or an SNI splitter on
the website's server and a reconnect path of our own after a reset. (B) is
not recommended: the same wire as (C) for ten times the code, in upstream
C, on two platforms.

**Relay allocations (carried from Task 28).** A relayed session today takes
2 allocations for control and 2 for media, all 4 of a Core's quota, so a
second device relayed at the same time gets 486 and can use only direct
paths (or, with (E), the floor, which is outside the quota). Control and
media can share one ICE connection: the control channel is a data channel,
and media's tracks and channels can join the same peer connection by
renegotiation, which halves the allocations (2 per session, so two relayed
devices fit the quota) and gives the path racer and the make-before-break
switch one connection to move instead of two. It changes Task 28's shape
(control's connection is made before the session and offered by the
device; media's is made in the session and offered by the Core), so it is
the controller's and JJ's call; raising the quota to 8 is the smaller
alternative, and with (E) as the floor the question only affects the
TURN/UDP rung.

## 7. JJ's choice

Pending. The controller brings JJ the recommendation above; his choice and
its reason are recorded here before Step 2 starts.
