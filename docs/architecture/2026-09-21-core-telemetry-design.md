# Core / GUI connection and audio telemetry

> Execution: yonder-cost-aware-execution. This extends R3 requirements
> R-R3-32/33; the existing authentication, retry and media policies remain
> authoritative.

## Operator outcome and agreed scope

The user selected **banner and connection/audio graphs first** on September
21. A connected Core shows useful live measurements in the title banner,
and Network Diagnostics opens a remote-capable history view. Core CPU and
memory history are a follow-on. History is in memory for this GUI run, with
no new station settings, disk recording or pairing behavior.

The banner retains Core connection state and shows radio RX/TX Mbps, Core
round-trip time when measured, and local playback activity. Its tooltip names
each source and measurement age. A successful authentication is distinct from
radio connectivity and from audio reaching the speaker path. Until telemetry
is available, show an explicit waiting/unavailable state. Older Core builds
remain connectable and indicate that station telemetry is unsupported.

The graphs distinguish:

| Measurement | Source and meaning |
| --- | --- |
| Radio RX/TX Mbps | Core's `RadioConnection::rxByteRate/txByteRate(1000)`; these existing getters already return Mbps |
| Radio RTT | An actual Core-side `pingRttMeasured` sample, with age; unavailable if that radio transport has no measurement |
| Core RTT | GUI WebSocket pong elapsed time; measures the control link round trip, not one-way audio latency |
| Control payload RX/TX | GUI session UTF-8 application bytes divided by actual elapsed time; excludes RTP, display media, TLS and WebSocket overhead |
| Core audio production | Existing capture frames, source drop events, encoded packets, transport accepted/refused counters sampled per audio context |
| GUI playback | Actual receiver admission, decode, concealment, late/invalid/duplicate, rate-matcher under/overflow and last admitted packet age |

Successful sends are **transport acceptance**, not delivery. Concealment and
source drops are distinct events, not a measured packet-loss percentage.
Use three graph groups: link throughput, round-trip time, and audio activity/
interruptions. Keep rates and latency on separate axes/cards. Detailed current
values and source labels accompany the charts; no invented health score.

## Source-first port boundary

### September 21 bandwidth and audio-delay extension (R-R3-35)

The operator requests total Core↔GUI bandwidth and separate Opus bandwidth
graphs. The implementation must count actual control and media bytes in both
directions, graph direction and sum, and label payload versus transport/wire
accounting. Packets/s and decoded frames/s remain useful but do not substitute
for kbit/s or Mbit/s. Obtain counters at the actual send/receive boundary and
avoid summing an audio subset twice. Establish fresh baselines after counter,
transport or audio-context resets; unavailable data is not zero.

Core RTT measures the control-channel round trip. It does not measure Opus
capture-to-playback latency. Graph safely sampled client audio queue duration
as buffering delay, explicitly excluding unmeasured network/encoder/device
delay. Do not estimate one-way latency from RTT and label it as measured.
True end-to-end audio latency requires a separate timing contract.

The first bandwidth increment uses GUI-local observations and requires no wire
version bump: control text bytes plus display and raw RTP bytes at the media
transport boundary. Dedicated transmit-keepalive and raw I/Q application
payload bytes were added to the same local media totals later. Receive counts
precede bounded local queue drops. Outgoing media records validated submissions
to the transport API, including queued or failed sends; it is not a
delivery/wire-byte measurement. Separately routed authenticated watch attach,
ACK and keepalive payload bytes join the GUI's total through a separate
per-primary-session observation; they are not also counted as control text or
media-channel bytes. Crypto, framing, ICE, VPN and lower
network overhead are excluded. The separate audio graph
shows raw received RTP and validated Opus payload as subsets already included
in total. Opus validation occurs after the bounded media handoff, so locally
dropped-before-validation RTP remains visible in the RTP/total series only.
The total/direction graph shares adaptive SI kbps/Mbps units; audio uses kbps.
Speaker buffering is a worker-observed PCM ring duration at 48 kHz and excludes
network, encoder, jitter/matcher and device latency. Missing counter providers,
peer changes, context changes and decreasing counters establish fresh baselines
and explicit graph gaps. Measured silence remains zero.

September 22 follow-up: the running development GUI exposed an empty-state
presentation gap. While disconnected, the graph returned before painting its
legend, hiding the bandwidth units and the distinction between total traffic
and Opus. Empty graphs now retain their existing series names, units and
legend selection without synthesizing zero-valued samples. This is a GUI
presentation correction; it adds no counters or audio-latency estimate.

### Where the operator finds the measurements

Open **Tools → Network Diagnostics** or click the **Core RTT** status segment.
The menu and status segment use the same role-aware handler: a remote session
opens Remote Network Diagnostics, and a direct-radio session opens its existing
local diagnostics. Opening diagnostics must not connect, disconnect or retry a
station session, including when the remote session is currently disconnected.

| Tab | Graph | Units and accounting |
| --- | --- | --- |
| Connection | Total Core ↔ GUI application traffic | Core→GUI, GUI→Core and their sum, sharing adaptive kbps/Mbps units; includes Opus once |
| Audio | Opus audio traffic received at GUI | Separate received audio-packet and validated Opus-payload series in kbps; both are subsets of total traffic |
| Round trip / buffering | Round-trip time | Core control RTT and Core-to-radio RTT in ms, where measured |
| Round trip / buffering | Client speaker buffering | Sampled client PCM queue in ms; not capture-to-playback latency |

These application totals exclude encryption, transport framing and network/VPN
overhead. Exact link usage is not measured by these counters. Keep the separate
end-to-end audio timing work open; neither RTT nor half-RTT closes that gap.

Fetched `ten9876/AetherSDR` upstream/main on September 21 and inspected
`0dea0dd7d73e25a40c8c01d46873af5834e23921` in a detached source worktree.
The user's Aether feature checkout was preserved.

Port the protocol-neutral Qt graph from
`src/gui/TimeSeriesGraphWidget.h`, extracted upstream from
`NetworkDiagnosticsDialog.cpp` in #2554. Adapt the history behavior from
`NetworkDiagnosticsHistory::{sampleNow,pruneSamples}` and the dialog's series
aggregation. Preserve labelled series, legend selection, time ranges, bounded
rendering, explicit gaps, and applicable inline comments. Source has no per-file
license header; Aether's project GPLv3 LICENSE applies. Follow
`docs/attribution/HOW-TO-PORT.md` rule 6, record source URLs/revision/author and
Nereus modifications in the same commit. Do not fabricate a Thetis GPLv2 header.

No Flex/VITA/DAX/SmartLink/HL2 collectors or protocol assumptions are ported.
Nereus's collector, authenticated message and lifecycle adapter are original
Core/GUI integration. Explicit segment breaks and per-field valid observation
weights are necessary adaptations: an outage, missing metric or quick reconnect
must not become a continuous line or a zero-valued measurement.

## Wire and lifecycle contract

Add `StationTelemetrySnapshot` in
`src/core/session/StationTelemetry.{h,cpp}` and a dedicated `StationTelemetry`
session kind, encoded as `station.metrics.v1`. Advertise capability
`stationTelemetryVersion=1` at negotiated protocol minor 3 or newer. Coordinate
later minor additions with task 4d; do not reuse the telemetry threshold for a
different feature. Protocol minor 10 adds the optional Core host section and
advertises `stationTelemetryVersion = 2`; minor-9 and older peers keep version 1
without the host section. Protocol minor 11 adds the optional `receivers`
section (per slice: `sliceId`, `loadPercent`, `inputDelayMs`,
`skippedInputMs`; see `docs/architecture/2026-09-23-r3-dsp-overload-plan.md`
Task 6) and advertises `stationTelemetryVersion = 3`; minor-10 peers receive
exactly the radio, audio and host sections. The snapshot is observational; it is neither a mirrored
property nor an inbound command.

The typed snapshot contains a sequence, Core sample elapsed milliseconds,
radio connected state, optional radio rates/RTT with RTT age, audio active state
and context generation, and optional elapsed-time audio counter rates. Omitted
means unavailable; measured zero remains zero. JSON numbers must be finite and
nonnegative where appropriate. Bound one encoded snapshot to 16 KiB and reject
malformed required fields without applying partial current state. Ignore unknown
optional fields for forward compatibility. Do not put secrets, addresses,
operator identifiers, audio or spectrum content in a metrics message.

`DaemonTelemetryController(StationServer*, RadioModel*,
DaemonMediaController*, QObject*)` owns a 1 Hz timer and baseline state. It reads
existing safe media snapshots on the control thread. RadioConnection's rolling
rate getters traverse lists written on the connection thread: collect those
values through a queued request/reply on that owning thread, never a direct
cross-thread getter. Keep at most one request outstanding, tag it with the
current connection/session generation, and discard late replies after either
changes. Use the request time as a conservative freshness bound, or carry a
same-process monotonic sample timestamp. Obtain RTT with its actual measurement
age rather than renewing it on each publication. No new DSP locks, radio requests
or audio callbacks. First sample,
changed radio object, changed audio context or reset counters establish a new
baseline; no fictitious rate is calculated across those boundaries.

`StationServer::sendTelemetry(snapshot, expectedSessionEpoch)` requires the
current authenticated, snapshot-complete peer and negotiated support. Reuse the
server's existing per-session epoch value without making telemetry depend on
media being enabled. End publication before session/radio teardown. Delayed work
carrying a previous epoch cannot target a replacement client.

`StationClient::telemetryReceived(snapshot, localSessionEpoch)` is emitted only
for the current transport, after snapshot completion and capability admission.
Retain existing transport-pointer/epoch guards. GUI consumers compare that local
epoch and monotonic sequence, never a Core clock against the Mac clock. Record
GUI receipt time with a monotonic clock for current-value age. Core elapsed time
supports rate intervals and ordering, not cross-host latency claims.

Expose WebSocket RTT and payload counters as read-only observations on
`SessionTransport`; keep `pongReceived` and all heartbeat decisions unchanged.
No added pings. RTT has its own age: a 1 Hz telemetry update does not renew an
old RTT measurement. A session pong is normally 20 seconds apart; do not expire
it at the three-second station snapshot threshold. Use the existing heartbeat
period to bound RTT freshness separately and clear it immediately on teardown.
RTT charts are explicitly labelled last-observed gauges: hold the last measured
value between pings while its age advances; expire Core RTT after 60 seconds.
Holding a gauge does not represent a fresh RTT measurement or add new pings.

Expose a thread-safe, read-only receiver diagnostic snapshot from
`RemoteAudioReceiver`, including generation and running state. Reuse the actual
admission decisions and counters. A snapshot must not access worker-owned jitter
or resampler objects concurrently, block the audio callback, or alter restart
policy. Preserve lifetime underflow/overflow totals across receiver context
restarts, including events at worker exit, so an interruption between GUI
polls remains observable. Unavailable lifecycle snapshots must not reset those
baselines. Derive playback activity from actual decoded/device progress, not from
`isRunning()` alone. Preserve output mute/volume behavior.

## Current display and history

`RemoteTelemetryController` is GUI-owned alongside
`RemoteConnectionController`. It combines typed station samples, current
transport observations and receiver snapshots, and emits a read-only current
view plus history updates. `MainWindow` only wires it into `ConnectionSegment`
and a remote `RemoteDiagnosticsDialog`; collection and policy live outside
MainWindow. The existing text-only remote override must no longer suppress the
remote metric rendering. Direct-radio banner and diagnostics behavior remain
valid.

Sample history at 1 Hz using actual elapsed intervals. Station snapshots older
than three expected periods lose current eligibility. Disconnect clears current
values immediately. Retain prior history for inspection, but start new segments
for reconnect, media-context changes and metric unavailability as applicable.
Do not repeatedly append a stale station value as if newly measured.

Adapt Aether's retention: one hour of raw samples, then minute buckets through
seven days. Store per-metric valid weights and weighted means so repeated compaction
preserves averages without overflowing sums of finite values; unavailable fields do not contribute zero. Windows up to five minutes
use raw points; longer windows use `max(5 s, range/300)` buckets at their centres.
Keep gaps at more than three expected intervals, and explicit breaks for a
disconnect even if it lasts less than that. Bucket compaction must preserve
segment identity or conservatively omit a bucket that crosses a break. Avoid
unbounded growth from rapid reconnects; maximum storage and output bounds must
hold independently of the number of lifecycle events. Chart lines must break
before pixel-column downsampling can combine points on different segments.

Refreshing a hidden diagnostics dialog must not repaint it at spectrum cadence.
Sample once per second; render open charts at that cadence. Keep the history
owned outside the dialog so closing/reopening does not discard it.

## Implementation and verification

1. Add typed codec/capability and Core collector; unit-test missing/invalid
   fields, real elapsed rates, zero versus unavailable, context resets and size
   bound. Use session integration tests for old peers, pre-auth/pre-snapshot
   suppression, teardown and a late old-epoch send to a replacement session.
2. Add observational transport/receiver snapshots. Verify real pong elapsed
   and UTF-8 byte accounting while existing heartbeat outcomes remain unchanged.
   Exercise receiver start/stop/restart and actual admission outcomes; no mock
   constants standing in for playback.
3. Port graph/history with attribution. Test retention, weighted re-compaction,
   sparse validity, quick reconnect, media restart, stale gap, bucket centres,
   bounds under rapid reconnect and gap preservation through downsampling.
4. Wire banner and remote diagnostics. Test fallback/fresh/stale/disconnected/
   unsupported states through the production controller and widget, with distinct
   radio Mbps and control RTT values to catch source/units confusion. Existing
   direct-mode tests remain green.
5. Build affected targets before focused `ctest`; build `all_tests`, then run
   unfiltered `ctest --test-dir build-integration --output-on-failure` at the
   combined boundary. Use one consolidated independent review for protocol,
   threading, provenance and misleading-telemetry risks.
6. Install a signed matching Core/GUI checkpoint and observe live radio rates,
   Core RTT, playback activity and charts on the Rock 5C. Receive-only reconnect
   must clear current values and visibly break history. Compare with actual
   Core/receiver diagnostics and record GUI render/audio impact. User confirms
   readability and continued smooth playback; hardware acceptance stays pending
   until observed. No OS/VPN/switch changes are part of this work.

Advisory delegation: Terra can own the protocol-neutral graph/history after
this contract is fixed; Sol is suitable for the cross-thread collector/session
boundary. Shared session schemas, CMake, MainWindow, Git and hardware remain
serialized under the lead.
