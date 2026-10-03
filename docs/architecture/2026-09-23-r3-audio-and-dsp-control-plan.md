# R3 audio and DSP control implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.
> This plan runs in the second builder lane (worktree
> `/Users/j.j.boyd/.codex/worktrees/nereus-lane-b/NereusSDR`, branch
> `codex/lane-b`, build directory `build-lane-b`) while the DSP overload plan
> runs in the integration worktree; the controller carries commits across.

**Goal:** close the operator decisions of 2026-09-23 that need no further input:
a lossless audio choice for digital modes, wider audio at 48 kbit/s, a measured
audio delay with its accuracy, NR3 models chosen from the Core, notch edits that
the Core owns and every window sees, and the remaining load-sensitive display
tests. The decisions still open with the operator (TCI in a remote window,
Connections after a dropped link, Setup while disconnected, the pan status line
and the wording review) and CPU-adaptive spectrum (which needs the DSP overload
plan's shared load sampler first) are not in this plan.

**Architecture:** each feature is gated by its own capability version so this
lane never collides with the other lane's protocol minor. Lossless audio adds an
L16 RTP profile beside Opus on the same audio line, a Core admission rule and a
GUI-owned link trial. The delay measurement adds a once-a-second clock probe on
the media control channel and a pure estimator in the GUI. NR3 models reuse the
NNR station-asset path. Notches become a Core-owned mirrored object changed only
by commands.

**Tech stack:** C++20, Qt 6, libdatachannel (RTP, SDP), Opus, rnnoise, Qt Test
(off-screen), `DaemonMediaController`, `RemoteMediaController`,
`RemoteAudioReceiver`, `AudioJitterBuffer`, `DspAssetService`, `NotchModel`,
`StationServer`/`StationClient`.

**Spec:** [R3 plan](2026-09-20-remote-daemon-r3-plan.md) requirements R-R3-03,
R-R3-05, R-R3-21, R-R3-23, R-R3-33, R-R3-35; design
[§3.1, §9.3, §9.5](2026-07-28-remote-daemon-architecture-design.md). Operator
answers of 2026-09-23 (R3 questions 1, 2, 5, 6, 11): a "Lossless" remote audio
choice sending uncompressed 48 kHz audio, refused with a plain reason when the
link cannot carry it; the delay shown as a measured value with its accuracy, never
half the round trip; notch add/move/delete commands with a Core-owned list; NR3
models kept on the Core and chosen like NNR models; sound up to 20 kHz at
48 kbit/s. Private scout notes with file:line detail:
`/Users/j.j.boyd/.config/nereus/work/r3-next-audio-scout-2026-09-23.md` and
`r3-next-control-ui-scout-2026-09-23.md`.

## Global Constraints

- Work in the worktree and build directory the dispatch names (this lane:
  `/Users/j.j.boyd/.codex/worktrees/nereus-lane-b/NereusSDR`, branch
  `codex/lane-b`, `build-lane-b`). Commit only on that branch; never checkout,
  rebase, merge, reset or push. Never touch the integration worktree
  `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR` except its
  `.crew/` files when a dispatch says so.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash
  characters in commit messages, docs or new operator strings. Stage explicit
  paths only. Every commit names its R-R3 IDs.
- Protocol: do not change `kSessionProtocolMinor` (the other lane's Task 6 takes
  minor 11). Gate each feature with its own capability version in
  `StationCapabilities` following the `dspAssetVersion` / `stationTelemetryVersion`
  pattern; a peer without the version sees exactly today's behaviour (golden
  comparison where a test exists).
- Operator wording (standing directive): everything a user reads is plain user
  words, never internal terms (grant, budget, capability, minor, slot, epoch,
  SSRC, endpoint, allocation, revision, handshake, snapshot, codec, payload,
  peer, protocol, session, telemetry, RTP, PCM). Wire reason strings the app
  compares stay unchanged; translate them for display.
- NereusSDR-original files carry the house header plus
  `// no-port-check: NereusSDR-original. <reason>`. Any WDSP file change follows
  `docs/attribution/WDSP-PROVENANCE.md` and passes
  `python3 scripts/audit-wdsp-headers.py` and
  `python3 scripts/verify-thetis-headers.py --all-kinds`.
- Local operation is unchanged unless a task says otherwise; every remote change
  keeps a local-mode test passing.
- Tests run off-screen by default. Build exact targets, then
  `ctest --test-dir <build dir> -R '^(<targets>)$' --no-tests=error --output-on-failure`.
  No unfiltered suite. Never build the `NereusSDR` target. Record `uptime` load
  averages with any timing; timing tests must pass while the other lane builds.
- Hardware is off limits to implementers (the Rock, radios, the running GUI).

## What already exists

(Details and line numbers in the scout notes above; highlights.)
- Audio is one send-only Opus line, payload type 111, 40 ms stereo blocks, RTP
  packets capped at 940 bytes under a 1000-byte MTU
  (`LibDataChannelMediaTransport.cpp:273-290, 449-455, 339`;
  `OpusAudioCodec.h:26`); the Core alone picks the bitrate from
  `nereusd.conf audio_bitrate` 24000|48000; the accepted profile reaches minor-8
  GUIs in the audio context (`RemoteAudioContext.cpp:80-90, 105-121`); the GUI's
  only audio control is `{op:"audio", connectionId, revision, enabled}`
  (`RemoteMediaController.cpp:1907-1909`, exact keys at
  `DaemonMediaController.cpp:1247`). The jitter buffer assumes 1920-frame, 40 ms
  packets (`AudioJitterBuffer.h:14-17`); there is no RTCP.
- The encoder forces wideband for both bitrates (`OpusAudioCodec.cpp:23-28, 214,
  249`); the wire already accepts 20000 Hz; the GUI text comes from the reported
  profile.
- The heartbeat is a Qt WebSocket ping with no timestamps; audio blocks carry no
  clock time (`DaemonAudioSource.cpp:208-265`); the GUI stamps arrival at
  `RemoteAudioReceiver.cpp:583` with a static `monotonicNs` (`:19-23`); PortAudio
  output latency is ignored (`PortAudioBus.cpp:636`); Network Diagnostics shows
  speaker buffering and says latency is not measured
  (`RemoteTelemetryController.cpp:428-430`).
- NR3's "Use Model..." writes a local path to the station-scoped `Nr3ModelPath`
  that the Core reads only at radio connect (`DspSetupPages.cpp:973-1013`,
  `RadioModel.cpp:7982-7993`); `RNNRloadModel` is global and live
  (`third_party/wdsp/src/rnnr.c:370-405`); built-in weights are compiled out, so
  "" or a failed load leaves a NULL model. The NNR asset path (kinds, commands,
  mirrored selection, `DspAssetDialog`) is reusable (`DspAssetService.cpp:177-376`).
- Remote notch edits run on the window's own `NotchModel`, which persists the
  whole `Notch*` set to the Core; the Core reads it only at construction; the
  remote model starts empty, so its first edit overwrites the Core's whole list
  (`MainWindow.cpp:2345-2415`, `NotchModel.cpp:579-620`, `RadioModel.cpp:1832,
  4208-4291`, `StationServer.cpp:1150-1154`).
- `tst_daemon_media_controller` cases fail under heavy load (37-70):
  `Harness::startReadyPeer` (`:462`) sends "start" when `establishSession` has
  seen only `server.mediaAvailable()`; `spectrumAndPs3ShareALaggingWindowAndBothProgress`
  counts too few frames (10 in 6 cycles, needs 11); one loaded single-case run
  took 315 s.

## Task 1: The display media controller tests hold under load

**Requirements:** R-R3-03, R-R3-05.

**Files:**
- Modify: `tests/tst_daemon_media_controller.cpp` (and a fake under
  `tests/fakes/` only if a case needs a readiness signal it lacks)

**Interfaces:** none.

**Acceptance:**
- `Harness::startReadyPeer` waits for the condition a "start" media control
  actually needs before sending it (name it in the report); the lagging-window
  case measures progress by a condition, not a fixed cycle count; no assertion
  is weakened and each case still fails on the defect it guards.
- Find the cause of the 315 s single-case run or show it cannot recur; any
  per-test timeout stays within the registered `TIMEOUT`.
- The whole binary passes `--repeat until-fail:10` while a heavy build runs
  alongside (record load averages).

**Verification:** test reliability.
```sh
cmake --build build-lane-b --target tst_daemon_media_controller -j6
ctest --test-dir build-lane-b -R '^tst_daemon_media_controller$' --repeat until-fail:10 --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Fix the readiness wait and the frame count; loaded repeat run; commit.

## Task 2: Sound up to 20 kHz at 48 kbit/s

**Requirements:** R-R3-23.

**Files:**
- Modify: `src/core/session/media/OpusAudioCodec.{h,cpp}`,
  `src/core/daemon/DaemonConfig.h` (comment), `packaging/nereusd.conf.sample`
  (comment),
  `docs/architecture/2026-09-20-remote-daemon-r3-verification/opus-profile-probe.{c,txt}`
- Test: `tests/tst_opus_audio_codec.cpp`, `tests/tst_daemon_audio_sender.cpp`,
  `tests/tst_remote_audio_status.cpp`, `tests/tst_daemon_media_controller.cpp`

**Interfaces:**
- Produces: `bandwidthForBitrate(int bitrate)` (24000 wideband, 48000 fullband),
  used for the encoder setting and the reported `audioBandwidthHz`.

**Acceptance:**
- At 48000 bit/s every encoded packet reports fullband and the reported profile
  says 20000 Hz; at 24000 it stays wideband and 8000 Hz.
- A stereo 1 kHz + 15 kHz tone keeps its 15 kHz energy through encode/decode at
  48 kbit/s and loses it at 24 kbit/s (reuse `frequencyEnergy`).
- The GUI's remote audio text shows "up to 20 kHz" from the reported profile with
  no GUI code change (test pins it).
- The probe covers fullband at 48 kbit/s stereo (packet bandwidth code and Mac
  CPU per second of audio recorded in the .txt).

**Verification:** unit.
```sh
cmake --build build-lane-b --target tst_opus_audio_codec tst_daemon_audio_sender tst_remote_audio_status tst_daemon_media_controller -j6
ctest --test-dir build-lane-b -R '^(tst_opus_audio_codec|tst_daemon_audio_sender|tst_remote_audio_status|tst_daemon_media_controller)$' --no-tests=error --output-on-failure
```
Hardware (pending, controller): Rock CPU at 48 kbit/s fullband.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Bandwidth by bitrate, tests, probe; commit.

## Task 3: NR3 models live on the Core

**Requirements:** R-R3-21.

**Files:**
- Modify: `src/core/dsp/DspAssetValidation.h`, `src/core/dsp/DspAssetService.{h,cpp}`,
  the station asset store, `src/core/session/MirrorPolicy.cpp`,
  `src/core/session/StationServer.cpp` (capability `dspAssetVersion = 2`),
  `src/core/session/StationClient.cpp`, `src/models/RadioModel.cpp` (apply at
  connect and live), `src/core/settings/SettingsScope.cpp` (`Nr3ModelPath`
  model-owned), `src/gui/setup/DspSetupPages.cpp` (NR3 tab),
  `src/gui/DspAssetDialog.cpp`
- Test: `tests/tst_dsp_asset_store.cpp`, `tests/tst_dsp_asset_service.cpp`,
  `tests/tst_station_session.cpp`, `tests/tst_dsp_asset_dialog.cpp` 

**Interfaces:**
- Produces: `DspAssetKind::Nr3Model = 2`; command `dspAssets.selectNr3Model {id}`;
  mirrored properties `nr3ModelAsset`, `nr3ModelStatus`; settings key
  `DspAssets/Nr3Model` (not app-writable).

**Acceptance:**
- NR3 model files are imported, listed, exported and selected through the
  existing asset commands; the bundled large and small models are always
  selectable by id; validation loads a model trial off the audio thread and
  rejects junk with a plain reason; size cap about 16 MiB.
- The Core applies a selection live with `RNNRloadModel(<resolved file path>)` and
  uses it at radio connect instead of `Nr3ModelPath`; "Default" is the bundled
  file's path, never "" and never a NULL model (red first: a test proves today's
  Default/failed-load path would leave NULL, or the report shows why that cannot
  be tested and how the new code makes it impossible).
- The NR3 tab offers a model list and "Models..." in local and remote windows;
  uploads use the chunked import; against an older Core the tab says "This Core
  cannot change the NR3 model."; a local install with an existing valid
  `Nr3ModelPath` imports it once.
- Older apps: `Nr3ModelPath` writes are refused with a plain reason; unknown
  asset rows are skipped (golden).

**Verification:** DSP asset feature, unit plus session integration.
```sh
cmake --build build-lane-b --target tst_dsp_asset_store tst_dsp_asset_service tst_station_session tst_dsp_asset_dialog -j6
ctest --test-dir build-lane-b -R '^(tst_dsp_asset_store|tst_dsp_asset_service|tst_station_session|tst_dsp_asset_dialog)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Asset kind, selection, Core apply with tests.
- [ ] **Step 2:** NR3 tab and dialog, old-Core wording; commit.

## Task 4: Notches the Core owns

**Requirements:** R-R3-21, R-R3-09.

**Files:**
- Modify: `src/models/NotchModel.{h,cpp}` (mirror mode), `src/models/RadioModel.cpp`
  (notch commands on the Core), `src/core/session/StationServer.cpp` (mirrored
  `notches` object, capability `notchControlVersion = 1`),
  `src/core/session/SessionCommandDispatcher.cpp`,
  `src/core/session/StationClient.{h,cpp}`, `src/core/session/MirrorPolicy.cpp`,
  `src/core/settings/SettingsScope.cpp` (`Notch*` model-owned),
  `src/gui/MainWindow.cpp`, `src/gui/setup/DspSetupPages.cpp` (TNF page)
- Test: `tests/tst_station_session.cpp`, `tests/tst_tnf_ui_wiring.cpp`,
  `tests/tst_notch_channel_sync.cpp`

**Interfaces:**
- Produces: mirrored singleton `notches` with `listJson` (id, centreHz, widthHz,
  active; at most 1024 entries) and `revision`; commands `notch.add {sliceId,
  centreHz, widthHz}` (returns the Core's id), `notch.move {id, centreHz,
  widthHz}`, `notch.setActive {id, active}`, `notch.delete {id}`;
  `globalEnabled` and `autoIncrease` as two-way properties.

**Acceptance:**
- Red first: a test reproduces today's defect: a remote window's first notch edit
  replaces the Core's whole notch list.
- A remote window's add, move, toggle and delete change the Core's notch list and
  every receiver channel's notches; ids survive the mirror; a stale id and a
  full list are refused in plain words; drags send at most about 10 updates a
  second plus a final one on release.
- The remote `NotchModel` never writes or restores `Notch*` settings; it replaces
  its list from the mirror. A change made on the Core (for example a TCI notch)
  reaches the window. `Notch*` settings writes from an app are refused.
- Local mode keeps today's behaviour and tests.
- An app without `notchControlVersion` sees no new object behaviour (golden).

**Verification:** remote control correctness; red first.
```sh
cmake --build build-lane-b --target tst_station_session tst_tnf_ui_wiring tst_notch_channel_sync -j6
ctest --test-dir build-lane-b -R '^(tst_station_session|tst_tnf_ui_wiring|tst_notch_channel_sync)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator S1): notches from the remote window on the Rock.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Overwrite regression (red), mirrored object and commands.
- [ ] **Step 2:** Mirror-mode NotchModel and UI wiring; commit.

## Task 5: Lossless audio, Core and transport

**Requirements:** R-R3-23.

**Files:**
- Create: `src/core/session/media/PcmAudioCodec.{h,cpp}`
- Modify: `src/core/session/media/OpusAudioCodec.{h,cpp}` (profile-neutral
  packetiser seam), `src/core/session/media/DaemonAudioSender.{h,cpp}`,
  `src/core/session/media/IMediaTransport.h` (start option),
  `src/core/session/media/LibDataChannelMediaTransport.cpp` (L16 rtpmap on the
  same line, both payload types accepted), `src/core/session/media/RemoteAudioContext.{h,cpp}`
  (an "l16" encoder shape and a refusal code),
  `src/core/session/media/DaemonMediaController.cpp` (audio control `profile`,
  admission), `src/core/daemon/DaemonConfig.{h,cpp}` and the sample config
  (`audio_lossless = allow|deny`, default `allow`), `src/core/session/StationCapabilities.{h,cpp}`
  (`audioProfileVersion = 1`), `CMakeLists.txt`
- Test: a new `tests/tst_pcm_audio_codec.cpp`, `tests/tst_daemon_audio_sender.cpp`,
  `tests/tst_media_transport.cpp`, `tests/tst_remote_audio_context.cpp`,
  `tests/tst_daemon_media_controller.cpp`, `tests/tst_daemon_config.cpp`

**Interfaces:**
- Produces: audio control `{op:"audio", connectionId, revision, enabled, profile}`
  with `profile` `"opus"` or `"lossless"` (the key only for peers with
  `audioProfileVersion`); the audio context's encoder shape
  `{codec:"l16", sampleRate:48000, channels:2, frameSamples:192, ...}` and a
  refusal code; L16 packets: big-endian 16-bit interleaved stereo, 192 frames
  (4 ms, 768-byte payload) per packet, a dynamic payload type, clipped to +-1.

**Acceptance:**
- A lossless request from a capable peer switches the Core's audio to L16 at the
  next block boundary, flushing queued Opus audio, and acknowledges with the
  accepted profile; a refusal (Core setting `deny`, or the packetiser
  unavailable) leaves Opus running and reports the reason code; stale revisions
  are ignored.
- L16 round trip is bit-exact after 16-bit quantisation; timestamps advance 192
  per packet; ten packets per 1920-frame block; every packet stays under the
  940-byte cap.
- The L16 rtpmap is offered only to capable peers; older peers get exactly
  today's SDP and context shapes (golden). Whether libdatachannel's answer keeps
  the L16 map is established by test, not assumed.
- Real DTLS/SRTP loopback (`tst_remote_audio_session` or `tst_media_transport`)
  carries about 1.54 Mbit/s of L16 payload.

**Verification:** media protocol change; unit plus real transport loopback.
```sh
cmake --build build-lane-b --target tst_pcm_audio_codec tst_daemon_audio_sender tst_media_transport tst_remote_audio_context tst_daemon_media_controller tst_daemon_config nereusd -j6
ctest --test-dir build-lane-b -R '^(tst_pcm_audio_codec|tst_daemon_audio_sender|tst_media_transport|tst_remote_audio_context|tst_daemon_media_controller|tst_daemon_config)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** L16 codec and sender with tests.
- [ ] **Step 2:** Transport, context, control, admission, capability; commit.

## Task 6: Lossless audio, in the app

**Requirements:** R-R3-23, R-R3-35.

**Files:**
- Modify: `src/core/session/media/AudioJitterBuffer.{h,cpp}` (packet frames and
  duration as parameters; window in time, 320 ms; hold 80 ms),
  `src/core/session/media/RemoteAudioReceiver.{h,cpp}` (decoder by payload type,
  lost L16 packet becomes silence, arrival queue bounded in time, matcher
  configured from the packet size), `src/gui/RemoteMediaController.cpp`
  (profile request, replay on reconnect, link trial),
  `src/gui/RemoteAudioStatus.cpp` and the Remote audio section of the connection
  panel (`src/gui/RemoteConnectionController.cpp`), `src/gui/RemoteDiagnosticsDialog.cpp`
  and `src/gui/TelemetryHistory.{h,cpp}` (audio traffic labels that are no longer
  Opus-only)
- Test: `tests/tst_audio_jitter_buffer.cpp`, `tests/tst_remote_audio_receiver.cpp`,
  `tests/tst_remote_media_controller.cpp`, `tests/tst_remote_audio_status.cpp`,
  `tests/tst_remote_audio_session.cpp`

**Interfaces:**
- Consumes: Task 5's control, context shape, refusal code and capability.
- Produces: a remote audio setting stored on this computer (Opus or Lossless).

**Acceptance:**
- The Remote audio section offers Opus and Lossless; the choice is stored on this
  computer and replayed on reconnect; the section shows what the Core accepted,
  or a plain reason ("This Core does not allow lossless audio.", "The network
  could not carry lossless audio; staying on Opus.").
- Link trial owned by the app: about 5 s after switching, then a sustained rule
  on missing packets, concealment and burst restarts; on failure it returns to
  Opus with the plain reason; thresholds are named constants with their
  reasoning; a lossy transport injected through the `MediaPeer::TransportFactory`
  seam proves the fallback.
- The jitter buffer and receiver handle 192-frame, 4 ms packets without the old
  32 ms stall limit; Opus behaviour is unchanged (existing tests).
- Diagnostics call the audio traffic by what it is, in user words.

**Verification:** GUI and receiver behaviour, off-screen; real loopback session.
```sh
cmake --build build-lane-b --target tst_audio_jitter_buffer tst_remote_audio_receiver tst_remote_media_controller tst_remote_audio_status tst_remote_audio_session -j6
ctest --test-dir build-lane-b -R '^(tst_audio_jitter_buffer|tst_remote_audio_receiver|tst_remote_media_controller|tst_remote_audio_status|tst_remote_audio_session)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator checkpoint): Lossless on the LAN to the Rock; a
digital-mode decode comparison.

**Execution note (advisory):** opus. Requires Task 5.

- [ ] **Step 1:** Jitter buffer and receiver parameterisation with tests.
- [ ] **Step 2:** Choice, replay, link trial, status and labels; commit.

## Task 7: Measured audio delay with its accuracy

**Requirements:** R-R3-35, R-R3-33.

**Files:**
- Create: `src/gui/AudioClockEstimator.{h,cpp}` (pure)
- Modify: `src/core/session/media/DaemonAudioSource.{h,cpp}` (block capture time,
  injectable clock), `src/core/session/media/DaemonMediaController.cpp`
  (`clock-probe` / `clock-echo` media control ops),
  `src/core/session/media/RemoteAudioReceiver.{h,cpp}` (injectable clock,
  playout time), the PortAudio output pacing (device output latency where the
  backend reports it), `src/gui/RemoteMediaController.cpp` (probe once a second
  while audio runs), `src/gui/TelemetryHistory.{h,cpp}`,
  `src/gui/RemoteTelemetryController.cpp`, `src/gui/RemoteDiagnosticsDialog.cpp`,
  `src/gui/RemoteAudioStatus.cpp`, `src/core/session/StationCapabilities.{h,cpp}`
  (`audioClockVersion = 1`)
- Test: a new `tests/tst_audio_clock_estimator.cpp`,
  `tests/tst_remote_audio_receiver.cpp`, `tests/tst_daemon_media_controller.cpp`,
  `tests/tst_remote_audio_session.cpp`, `tests/tst_remote_telemetry.cpp`,
  `tests/tst_remote_diagnostics.cpp`, `tests/tst_telemetry_history.cpp`

**Interfaces:**
- Produces: `{op:"clock-probe", id, t0}` from the app and `{op:"clock-echo", id,
  t0, t1, t2, generation, rtpTimestamp, capturedNs}` from the Core (t1 on entry,
  t2 just before the reply); estimator offset = ((t1-t0)+(t2-t3))/2, round trip
  = (t3-t0)-(t2-t1), keeping the lowest-round-trip sample over about 16 s plus
  drift times age in the bound.

**Acceptance:**
- One-way audio delay = playout time minus the Core capture time mapped through
  the offset; shown as, for example, "Audio delay 85 ms ± 1 ms" where ± is half
  the chosen round trip plus drift; half the round trip is never shown as the
  delay. Where the device output latency is unknown the figure says it excludes
  the device. Delivery delay (Core capture to jitter release) is shown separately.
- With asymmetric simulated paths the true delay always lies inside value ± bound;
  with symmetric paths and no buffering it equals the injected one-way delay.
- A RemoteAudioSessionHarness run with the Core clock offset by +5 s and a known
  paced speaker queue reports the expected delay.
- Nothing is reported when echoes stop or the audio context changes; reconnects
  show gaps in the history. Peers without `audioClockVersion` see today's
  behaviour.

**Verification:** measurement correctness; unit plus harness.
```sh
cmake --build build-lane-b --target tst_audio_clock_estimator tst_remote_audio_receiver tst_daemon_media_controller tst_remote_audio_session tst_remote_telemetry tst_remote_diagnostics tst_telemetry_history -j6
ctest --test-dir build-lane-b -R '^(tst_audio_clock_estimator|tst_remote_audio_receiver|tst_daemon_media_controller|tst_remote_audio_session|tst_remote_telemetry|tst_remote_diagnostics|tst_telemetry_history)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator checkpoint): the delay on the Rock over the LAN and
over the internet path.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Estimator with tests; Core stamps and ops.
- [ ] **Step 2:** Playout time, diagnostics and status; commit.
