# R-R3-32/33: Core banner and connection/audio history

## Scope and implementation

The user selected the Core banner and connection/audio graphs first. CPU and
memory history remain a follow-on. The source-first Aether port is pinned to
`0dea0dd7d73e25a40c8c01d46873af5834e23921`; see
[provenance](../../attribution/AETHER-TELEMETRY-PROVENANCE.md) and
[design](../2026-09-21-core-telemetry-design.md).

Core publishes typed, capability-negotiated `station.metrics.v1` observations
only after authentication and snapshot completion. The collector requests
radio observations on the connection's owning thread and discards late replies
from retired sessions/connections. Audio rates use actual elapsed intervals
and reset their baselines at context changes. Missing values remain unavailable.

The GUI combines those observations with actual WebSocket payload/RTT counters
and local receiver activity. The banner distinguishes radio rates, Core RTT
and playback. A remote diagnostics dialog adds bounded in-memory history,
explicit reconnect/missing-data gaps, source labels and separate units. RTT is
a labelled last-observed gauge with actual age, not an invented fresh sample.
History persists when the dialog closes. Underflow/overflow lifetime totals
preserve interruptions that occur between GUI polls across receiver restarts.

## Verification to date

Focused tests cover the typed codec, negotiated/authenticated delivery,
old-epoch suppression, actual queued cross-thread radio observations, context
resets, real packet admission and device progress, transport payload/pong
measurements, bounded weighted history, gap-preserving graph paths, the
production banner/controller and real authenticated dialog graph wiring.

One consolidated independent review found two issues: restart boundaries could
lose underflow/overflow events between one-second polls, and the dialog test
did not exercise its production graph refresh. Both are corrected. The event
regression drives a real receiver and independent device-render thread to force
an interruption, then verifies its lifetime count after restart. The dialog
test receives authenticated production samples and inspects actual graph series.
The corrected targets pass. Fixture failures and their corrections are retained
in the private build logs; no production behavior was relaxed to satisfy them.

The integrated `all_tests`/`nereusd` build passed. The unfiltered full test
run passed **692/692 executables**, with eleven pre-existing inner Qt skips
and no skips in the new telemetry tests. Native production configuration, installation, live banner/graph
readability, receive-only reconnect gaps and smooth playback acceptance remain
pending. No telemetry work in progress has been installed.

The telemetry checkpoint is ready for the native production build, but its
installation is held while the installed RADE checkpoint's live sample-cadence
failure is investigated. The passing telemetry suite does not close that separate
hardware failure; see [RADE multislice](rade-multislice.md).

## Native production build, signed d6ce05a5

All 105 source-overlay hashes matched the signed checkpoint. The Rock 5C's
production configuration (`NEREUS_BUILD_TESTS=OFF`) built and staged `nereusd`,
NereusCore, its dependencies and licences successfully; dependency resolution
and staged `--help` passed. The installed service remains `dd2a9ebf` while the
separate RADE cadence correction is prepared. No telemetry live acceptance is
claimed from this build-only checkpoint.

## Matching live installation, 0b408427

The combined telemetry/cadence checkpoint was installed and authenticated at
21:42 on September 21. Core and GUI identities, native production dependencies
and GUI signatures were verified. The banner visibly displayed Core connected,
about 18.7 Mbps radio ingress, measured Core RTT and Audio playing. Clicking it
opened the production Remote Network Diagnostics dialog. Connection and Audio
history plots updated, including about 48,000 source frames/s, 25 encoded and
accepted packets/s, and actual audio interruption/context gaps. Core RTT varied
with its measurement age; unavailable radio RTT was not fabricated.

The Audio tab exposed clipped unit labels at the fixed-width left gutter. A
font-measured gutter correction passes `tst_time_series_graph` and
`tst_remote_diagnostics` (2/2, 2.48 seconds), and awaits installation/visual
verification. The operator closed the dialog during live listening; it was
left closed. Roundtrip-tab readability, receive-only reconnect gaps and final
layout acceptance remain pending. Ongoing RADE-related playback underflows
are tracked separately and prevent any smooth-audio acceptance claim.

## Readability verified on installed 200d2a0e

At 22:23–22:24, the production Connection, Round trip and Audio tabs were
visually inspected after the matching installation. Packet and frame units,
current-value hints and legends now fit. The banner and history update from
live Core observations, with unavailable radio RTT called out and its historical
samples retained as history. The Audio plots distinguish red Core source-drop
events from GUI underflows/overflows; source drops remain under investigation.
Receive-only reconnect gap acceptance and the real audio soak remain open.


## R-R3-35 bandwidth and speaker-buffer extension

The GUI now collects application traffic across control and media. The
Connection tab starts with Core→GUI received, GUI→Core outgoing, and total;
all share adaptive SI kbps/Mbps units. Audio starts with bounded binary
media-track bytes and validated Opus payload in kbps. Both are subsets of total,
not additional traffic. The delay tab includes worker-sampled speaker PCM-ring
buffering in ms. Control RTT remains a round trip; capture-to-playback latency
is not measured. The banner adds Core→GUI, GUI→Core and total with shared adaptive kbps/Mbps units, plus the separate Opus payload rate.

Counter boundaries are explicit: GUI control text plus bounded display and
audio-track callbacks; receive counts precede local queue pruning. Outgoing
media counts submissions after adapter validation, including transport false
returns or exceptions. It does not prove delivery, and the figures exclude
framing, encryption, ICE, VPN and lower network overhead. Opus bytes come from
actual validated RTP parsing, including variable header/extension/padding
handling, at the receiver boundary after the media queue. Duplicates count as
traffic, while invalid packets do not count as Opus payload.

The rebuilt seven affected executables passed in **13.88 seconds**:
`media_transport`, `media_peer`, `opus_audio_codec`, `remote_audio_receiver`,
`remote_telemetry`, `remote_diagnostics`, and `telemetry_history`. Coverage
includes real encrypted peers, exact byte counts, invalid-send exclusion,
parser payload lengths, a gated worker for deterministic ingress overflow,
measured queue duration, true elapsed rates, no double counting, independent
lifetime resets, unknown versus zero, graph units/rendering and reconnect gaps.
Logs: `r3-bandwidth-focused-{build,test}.log`. The upstream transport API offers
no deterministic public hook for a post-validation false/throw; its submitted
counter placement is source-reviewed, with successful and preflight-refused
calls covered by real transport tests.

The consolidated independent review found one banner presentation gap: it
showed only aggregate traffic with a fixed Mbps unit. The corrected banner
includes both directions and total, with assertions for kbps and Mbps. The
rebuilt presentation checks passed **2/2**. The final `all_tests`/`nereusd`
build and unfiltered suite then passed **695/695 executables in 139.81 seconds**,
with eleven existing inner Qt skips. Logs:
`r3-bandwidth-reviewed-build.log`, `r3-bandwidth-banner-reviewed-test.log`, and
`r3-bandwidth-reviewed-test.log`. Native installation and live graph acceptance
remain pending; these software checks do not establish end-to-end audio latency
or resolve the separate hardware audio-soak requirement.


## R-R3-35 matching live installation, c9065756

The signed checkpoint was installed on September 21 at 23:56 EDT after all
139 source-overlay hashes matched and the Rock 5C production build, staged
runtime dependencies and `--help` passed. The matching Mac GUI passed strict,
deep code-signature verification and private Core/GUI library UUID checks,
and ran with the saved `radxa_5c_r3` profile. The service remained active with
zero automatic restarts; a recoverable `200d2a0e` installation was retained.
Private proof: `r3-bandwidth-native-build.log`, `r3-bandwidth-install.log`,
`r3-bandwidth-gui-identity.json`, and `r3-bandwidth-core-startup.log`.

The live banner fit at the operator's window size. Connection visibly showed
Core→GUI, GUI→Core and total with kbps labels (about 550–570 kbps after restart);
Audio showed about 24 kbps Opus payload and 26 kbps binary audio-track traffic.
The Round trip / buffering tab separately showed sampled speaker-ring duration
around 20 ms. All three tabs were visually inspected in the actual application;
these observations are samples, not fixed bandwidth or latency promises.

The Core restart ended the session at 23:55:56 and the GUI automatically
reauthenticated at 23:56:14. Traffic, audio and buffering histories retained a
visible gap and resumed after fresh baselines. The Audio tab was left visible.
This completes the R-R3-35 software, installation and graph-readability check.
It does not measure capture-to-playback latency or close the audio soak: an
underflow-triggered audio restart was still logged before the Core installation.
The separate R-R3-34 receiver-persistence gap also reproduced: Core restarted
with its configured single receiver and the GUI retired the missing second slice.
