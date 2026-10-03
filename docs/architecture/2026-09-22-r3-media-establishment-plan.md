# R3 media establishment and audio measurement enablers implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.

**Goal:** close the R3 software gaps that stand between today's remote audio
and the hardware sessions that must measure it: media that never finishes
connecting or refuses to start recovers with growing backoff; the 48 kHz mixed
audio contract is proven through rate, mode, band and slice changes; the Core
can run the 48 kbit/s profile the listening comparison needs, and its SDP says
what the encoder really does; drift is visible in telemetry for the soak.

**Architecture:** four independent tasks. Media establishment gets a deadline
and routes an initial start refusal into the existing typed retry path, and
the control backoff resets only once media is established. Transition tests
drive a real daemon media controller through each change while audio streams.
A validated `audio_bitrate` Core setting selects 24000 or 48000 bit/s, and the
Core's Opus SDP parameters match the encoder. The receiver publishes its
measured drift ratio as a telemetry value, not only inside fault text.

**Tech stack:** C++20, Qt 6, `StationClient`, `RemoteMediaController`,
`MediaPeer`, `LibDataChannelMediaTransport` (libdatachannel v0.24.5, libjuice),
`DaemonMediaController`, `DaemonAudioSender`, `OpusAudioCodec`,
`RemoteAudioReceiver`, `DaemonConfig`, Qt Test.

**Spec:** [R3 plan](2026-09-20-remote-daemon-r3-plan.md) section 5 (lines
1021-1036) and section 6 (lines 1254-1266, 1289-1292); requirements R-R3-06,
R-R3-07, R-R3-09, R-R3-23, R-R3-28, R-R3-33; the
[media recovery evidence](2026-09-20-remote-daemon-r3-verification/media-recovery.md)
"Explicit remaining boundary" (lines 55-63).

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash
  characters. Stage explicit paths only. Every commit names its R-R3 IDs.
- NereusSDR-original code; no Thetis port. Keep existing headers; new files
  carry the house header plus `// no-port-check: NereusSDR-original. <reason>`.
- `src/core` stays GUI-free. Use `AppSettings` in the GUI, never `QSettings`.
- Manual Disconnect and teardown still cancel every retry; stale peer
  callbacks still cannot reconnect a newer or intentionally stopped session;
  ordinary audio jitter, device errors and permanent protocol refusals still
  do not trigger a whole-session retry (R-R3-28 text).
- No change to Opus frame size, sample rate, channel count, the mixer cadence,
  RTP timestamp or sequence semantics, or the default bitrate (24000).
- Every new timing constant is derived from a named source (library code,
  an existing constant or a measured value) cited in a comment.
- Tests run off-screen by default. Build exact targets, then
  `ctest --test-dir build-integration -R '^(<targets>)$' --no-tests=error --output-on-failure`.
  Record load averages with timing. No unfiltered suite. Never build the
  `NereusSDR` target. Hardware is off limits to implementers.

## What already exists

Read-only investigation, 2026-09-22 (line numbers at 6bf8d086):
- `StationClient` resets its reconnect attempts at every successful handshake
  (`src/core/session/StationClient.cpp:1178-1179`), so repeated media
  failures after good handshakes retry at the first backoff step forever.
- `RemoteMediaController` stops without retrying when the backend refuses to
  start (`src/gui/RemoteMediaController.cpp:838-841`;
  `src/core/session/media/MediaPeer.cpp:285-291`), and nothing bounds a
  negotiation that never reaches a terminal state.
- Typed terminal transport failures already reach the StationClient retry
  (installed 706b9a5f; `media-recovery.md`).
- `DaemonAudioSender` builds the default 24 kbit/s encoder
  (`src/core/session/media/DaemonAudioSender.cpp:18`); `DaemonConfig` has no
  bitrate key (`src/core/daemon/DaemonConfig.cpp:70`; `remote_port` at 93-100
  and 175 shows the validation style). `OpusAudioCodec` accepts 24000 and
  48000 (`OpusAudioCodec.cpp:48-50`), FEC off (`:219`), decoder never uses
  FEC (`:341-344`).
- The Core's offer advertises
  `minptime=10;maxaveragebitrate=96000;stereo=1;sprop-stereo=1;useinbandfec=1`
  (`LibDataChannelMediaTransport.cpp:379-382`).
- No test changes sample rate, mode (including RADE), band or slices while
  audio streams (`tests/tst_daemon_audio_session.cpp:123` fixes 192000);
  `DaemonMediaController` re-evaluates audio only on control, radio or peer
  events (`DaemonMediaController.cpp:636-647, 1686-1756`).
- The receiver's drift ratio appears only in fault text
  (`src/core/session/media/RemoteAudioReceiver.cpp:237-245`).

## Task 1: Media establishment deadline, start refusal and backoff

**Requirements:** R-R3-28.

**Files:**
- Modify: `src/core/session/StationClient.{h,cpp}`, `src/gui/RemoteMediaController.{h,cpp}`,
  `src/core/session/media/MediaPeer.cpp` (only if the refusal needs a typed reason)
- Test: `tests/tst_remote_media_controller.cpp`, `tests/tst_session_link_loss.cpp`,
  `tests/tst_media_peer.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `StationClient::noteMediaEstablished()` (or an equivalent explicit
  call) that the media layer makes once media is ready; a named deadline
  constant in `RemoteMediaController` with its derivation.

**Acceptance:**
- When the media session has not reached ready by the deadline, it is treated
  as a typed media failure and enters the existing cancellable authenticated
  retry. The deadline is longer than the longest time the pinned peer library
  takes to report connection failure by itself (read it from the libjuice and
  libdatachannel sources under `build-integration/_deps`, cite file and line),
  so the library's own typed reason wins whenever it arrives.
- A backend that refuses to start because the transport could not be
  constructed (an exception from the transport factory) enters the same retry
  path with a plain reason. Permanent refusals (MediaPeer preconditions such as
  an already started peer, a non-canonical connection id or an invalid SSRC,
  and a factory that returns no transport) keep the stop-and-error behaviour
  with the reason shown and control left up, as R-R3-28 exempts permanent
  refusals. Once retries reach the backoff ceiling, a start refusal stops
  retrying and leaves a control-only session with the reason shown.
  (Amended 2026-09-23 after the final review.)
- With media negotiated, the reconnect attempts reset only after media is
  established, not at the control handshake; with no media negotiated they
  reset at the handshake as today. Three consecutive media failures after
  good handshakes produce growing retry delays (backoff steps 1, 2, 5 s).
- Manual Disconnect during any of these waits cancels them; a stale callback
  from an old peer after a newer session exists changes nothing.
- Existing link-loss and media recovery tests pass unchanged.

**Verification:** consequential recovery state machine: the three new cases
fail first on the current tree.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_media_controller tst_session_link_loss tst_media_peer tst_remote_connection_controls -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_media_controller|tst_session_link_loss|tst_media_peer|tst_remote_connection_controls)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator session S2): live media loss with a healthy
control session recovers and backs off.

**Execution note (advisory):** opus (session recovery).

- [ ] **Step 1:** Deadline, refusal and growing-backoff regressions (fail today).
- [ ] **Step 2:** Implement; run the commands; commit.

## Task 2: The 48 kHz audio contract through live transitions

**Requirements:** R-R3-06, R-R3-07, R-R3-09.

**Files:**
- Test: `tests/tst_daemon_audio_session.cpp` (or a new
  `tests/tst_daemon_audio_transitions.cpp` if the existing fixture cannot
  change the rate)
- Modify (only where a new test exposes a defect): `src/core/session/media/DaemonMediaController.cpp`,
  `src/core/session/media/DaemonAudioSender.cpp`, `src/core/session/media/DaemonAudioSource.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: no new API.

**Acceptance:**
- While mixed audio streams to a peer, each of these changes keeps the output
  at 48000 Hz stereo in 1920-frame packets with a contiguous RTP sequence and
  timestamps on the packet grid (a genuinely lost packet still advances the
  timestamp by 1920): radio sample rate 192000 to 384000 and back; the audio
  slice's mode SSB to RADE and back; a band change; adding and removing a
  second receive slice.
- Any change that requires a new audio context produces exactly one, with a
  newer generation, and the GUI side never plays audio from the older context
  after it.
- If a test exposes a defect, the fix lands in this task with the test that
  proves it, and the report names it.

**Verification:** real-time audio path: tests through the real daemon media
controller and sender with a fake transport.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_daemon_audio_session tst_daemon_audio_sender tst_daemon_audio_source tst_daemon_media_controller -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_daemon_audio_session|tst_daemon_audio_sender|tst_daemon_audio_source|tst_daemon_media_controller)$' --no-tests=error --output-on-failure
```
(Add `tst_daemon_audio_transitions` to both lines if created.)

**Execution note (advisory):** opus.

- [ ] **Step 1:** Transition tests; record which fail.
- [ ] **Step 2:** Fix what they expose; run the commands; commit.

## Task 3: Core audio bitrate setting and honest Opus SDP

**Requirements:** R-R3-23.

**Files:**
- Modify: `src/core/daemon/DaemonConfig.{h,cpp}`, `src/core/daemon/DaemonApp.cpp`
  (pass the setting), `src/core/session/media/DaemonAudioSender.{h,cpp}`,
  `src/core/session/media/DaemonMediaController.cpp` (if the sender is built
  there), `src/core/session/media/LibDataChannelMediaTransport.cpp` (SDP
  parameters), `packaging/nereusd.conf.sample` (documented key)
- Test: `tests/tst_daemon_config.cpp`, `tests/tst_daemon_audio_sender.cpp`,
  `tests/tst_media_transport.cpp`, `tests/tst_remote_audio_status.cpp` (the
  reported profile at 48000)

**Interfaces:**
- Consumes: `OpusEncoderProfile` and `OpusAudioEncoder::profile()`.
- Produces: config key `audio_bitrate` (integer, 24000 or 48000, default
  24000) and its accessor on `DaemonConfig`.

**Acceptance:**
- `audio_bitrate=48000` makes the Core encode at 48000 bit/s and report that
  target in the minor-8 audio context; 24000 and a missing key behave exactly
  as today; any other value logs one plain warning and keeps 24000, in the
  same style as `remote_port` validation.
- The Core's Opus SDP parameters describe the real encoder: in-band FEC is
  not advertised while FEC is off, and the bitrate parameter is not higher
  than the configured target. Keep `stereo=1;sprop-stereo=1` and
  `minptime=10`. The GUI accepts the new offer and an old Core's offer (test
  both strings through the real answerer).
- The sample config documents the key in one comment line.

**Verification:** configuration plus wire text; the 48k encode is checked by
reading the encoder's profile back.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_daemon_config tst_daemon_audio_sender tst_media_transport tst_remote_audio_status nereusd -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_daemon_config|tst_daemon_audio_sender|tst_media_transport|tst_remote_audio_status)$' --no-tests=error --output-on-failure
```
Hardware (pending, session S4): the 24/48 listening and CPU comparison on the
Rock uses this setting.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Config and SDP tests (fail today).
- [ ] **Step 2:** Implement; document the key; run the commands; commit.

## Task 4: Drift ratio as telemetry

**Requirements:** R-R3-07, R-R3-33.

**Files:**
- Modify: `src/core/session/media/RemoteAudioReceiver.{h,cpp}` (telemetry
  field), `src/gui/RemoteTelemetryController.cpp` (diagnostics text, if the
  audio section lists receiver values), and the diagnostics log line that
  already prints receiver counters
- Test: `tests/tst_remote_audio_receiver.cpp`, `tests/tst_remote_telemetry.cpp`

**Interfaces:**
- Consumes: the receiver's existing drift measurement (`RemoteAudioReceiver.cpp:237-245`).
- Produces: telemetry field `driftRatio` (optional double, absent until
  measured) beside the existing receiver counters.

**Acceptance:**
- The value the fault text reports is available in telemetry while audio
  plays, updates as the measurement updates, is absent before the first
  measurement and after stop, and appears in the periodic GUI diagnostics log
  line so a two-hour soak records it.
- If the Network Diagnostics audio section shows it, the label is plain
  ("Clock drift: %1 parts per million" or the unit the measurement really
  uses), with the number and unit kept together.

**Verification:** telemetry value, unit tests.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_audio_receiver tst_remote_telemetry -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_audio_receiver|tst_remote_telemetry)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Implement with its tests; run the commands; commit.
