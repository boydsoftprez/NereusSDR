# Remote audio status and retry implementation plan

> **Execution:** run with `crew` under `yonder-cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort
> follow the risk-based policy. No review between tasks; one whole-branch
> review at the end.

**Goal:** the remote GUI shows a lasting, plain-English audio status (what
Core is actually encoding, why audio is off, whether this computer's speaker
is working, and measured stream health) with a Retry action, instead of a
five-second toast and a three-word banner.

**Architecture:** Core reports its actual encoder settings and the reason
audio is off inside the existing `audio-context` reply, only to GUIs that
negotiated session protocol minor 8 (older GUIs keep the exact eight-field
message). The GUI receiver measures RFC 3550 arrival jitter and missing
packets and reports typed faults. `RemoteMediaController` owns one
`RemoteAudioStatus` with a failure identity that only matching recovery plus
real speaker progress can clear; the Core connection panel and title bar
present it.

**Tech stack:** C++20, Qt6 (Core/Widgets/Test), libopus through the existing
`OpusAudioCodec`, the authenticated WSS session (`StationServer` /
`StationClient`), CMake/Ninja, Qt Test with `tests/fakes/LoopbackTransport`
and `tests/fakes/PacedAudioBus.h`.

**Spec:** [R3 plan, task 5a](2026-09-20-remote-daemon-r3-plan.md) (R-R3-23,
first checklist item), with the corrections recorded in the September 22
session handoff: compatible wire contract first; never infer "no radio
signal" from silence; show Core-reported encoder settings, labelled as a
target; speaker buffer is not end-to-end latency; a selected output name is
the selection, not proof of the device in use; failures persist until
matching recovery with real output progress; keep the banner short. The
24/48 kbit/s listening comparison and any quality selector (task 5a items 2
and 3) are out of scope and stay open.

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
  Never edit or build in `/Users/j.j.boyd/NereusSDR` or any other checkout.
- The microphone design (`docs/architecture/2026-09-22-optional-microphone-capture-design.md`)
  was approved and committed in 5c4dbc23; it is not part of this plan. Stage
  explicit paths only.
- Every change traces to R-R3-23; R-R3-06/07/17/21 behavior is preserved.
- Commits: GPG-signed (`git commit -S`), hooks run with
  `NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`; never `--no-gpg-sign`, never
  `--no-verify`. No `Co-Authored-By` trailer. No em-dash characters in
  commit messages or docs.
- New source files carry the house header plus
  `// no-port-check: NereusSDR-original. <reason>` (see
  `src/core/session/media/DaemonAudioSender.h`). No Thetis logic is ported in
  this plan; if a task finds it needs Thetis behavior, stop and report.
- `src/core` stays GUI-free (`tst_core_has_no_gui_includes`). Use
  `AppSettings`, never `QSettings`.
- C++ style: braces on every control-flow body; no raw `new`/`delete` except
  Qt parent ownership; `m_camelCase` members, `kPascalCase` constants;
  `qCWarning`/`qCInfo` logging, no exceptions.
- Operator-visible strings are plain English. They never contain RTP, SSRC,
  generation, epoch, revision, context, codec internals beyond the codec
  line below, bracketed diagnostic detail, requirement IDs, or phase names.
  Raw diagnostic detail goes to the log only.
- No change to DSP constants, Opus encoder settings, bitrate, frame size,
  jitter-buffer timing, or retry cadence. No quality/profile selector.
- A measurement is labelled with what it is: "Arrival jitter" (RFC 3550
  interarrival jitter measured on this computer), "Missing packets" (RTP
  sequence numbers never received), "Gaps filled" (40 ms intervals played as
  concealment), "Speaker buffer" (audio queued for this computer's speaker,
  not total delay). Arrival spacing is never called loss; packet age is
  never called jitter; control RTT is never called audio latency.
- Tests: build the exact targets first, then run
  `ctest --test-dir build-integration -R '^(<targets>)$' --no-tests=error --output-on-failure`.
  Record `uptime` load averages next to any timing you report. Do not run
  the unfiltered suite; the controller runs it once at the end. Never weaken
  an existing assertion to get green; never emit a receiver signal directly
  in a test to fake a device fault.
- Hardware (Rock, Saturn, the running GUI) is off limits to implementers.

## What already exists

- `src/core/session/SessionMessages.h:177-188`: `kSessionProtocolMinor = 7`
  and one `k<Feature>SessionProtocolMinor` per optional wire feature. Each
  feature is gated by agreed minor plus a `StationCapabilities` version
  field. Pattern to mirror exactly: `remoteWidebandDisplayVersion`
  (`StationCapabilities.h:111`, `.cpp:61` serialize, `.cpp:170-173` parse,
  `StationServer.cpp:1235-1240` `remoteWidebandAvailable()`,
  `StationServer.cpp:1308` publish, `StationClient.cpp:2440-2444`
  `remoteWidebandAvailable()`). Unknown capability names are ignored by
  older parsers.
- `src/core/session/media/DaemonMediaController.cpp:1683-1777`:
  `reconcileAudio()` decides `shouldRun = m_audioDesiredEnabled && m_peer &&
  m_peer->isReady() && m_radioModel && m_radioModel->isConnected()`, starts
  `DaemonAudioSender`, then `sendAudioContext(actualEnabled)` sends exactly
  `op, connectionId, revision, generation, enabled, ssrc, firstSequence,
  firstTimestamp`. `onRadioConnectionStateChanged` (`:632-645`) sends
  `sendAudioContext(false)` when the radio drops. `handleAudio` (`:1019-1037`)
  accepts `{op:"audio", connectionId, revision, enabled}`.
- `src/gui/RemoteMediaController.cpp:1662-1683`: GUI parser requires
  `payload.size() == 8`, current revision, newer generation, matching SSRC.
  `requestAudio()` (`:1480-1494`) bumps `audioRevision` and sends
  `enabled = model->isConnected() && !masterMuted()`. Receiver
  `errorOccurred` (`:364-375`) stops audio, sends a disable control, and
  re-emits the raw reason (toast in `MainWindow.cpp:1067-1070`, 5000 ms).
  Receiver `restartRequested` (`:376-389`) retries through `requestAudio()`
  after a 1 s debounce. `stop()` (`:573-619`) resets session state.
  `audioTelemetry()` is public.
- `src/core/session/media/OpusAudioCodec.{h,cpp}`: `OpusAudioCodecConfig`
  (48 kHz, 2 ch, 1920-sample frames, bitrate 24000 default, 48000 only
  alternate); encoder ctl calls at `.cpp:184-195` (AUDIO, MUSIC, WIDEBAND,
  constrained VBR, complexity 10, FEC off, DTX off). `inspectOpusRtp`
  returns `OpusPacketInfo {channels, bandwidth, samplesPerChannel}`.
- `src/core/session/media/RemoteAudioReceiver.{h,cpp}`: worker loop
  admission (`.cpp:271-303`), fault sites through `notify(reason, fatal)`
  (`.cpp:200-225`; fatal emits `errorOccurred`, otherwise
  `restartRequested`, both queued and generation-guarded), synchronous
  `errorOccurred` when `beginRemotePlayback` fails in `start()`
  (`.cpp:140-145`). `submit()` (`.cpp:408-427`) stamps arrival with a
  monotonic clock after `inspectOpusRtp`. `telemetry()` uses the
  odd/even `telemetrySequence` snapshot pattern.
- `AudioEngine`: `masterMuted()`, `masterMutedChanged(bool)`,
  `speakersConfigChanged(AudioDeviceConfig)`,
  `AudioDeviceConfig::loadFromSettings("audio/Speakers")`. No device-failure
  signal exists; the receiver's faults are the evidence.
- `src/gui/RemoteConnectionController.{h,cpp}`: `RemoteConnectionPanel`
  (`QDialog`, title "Core connection") with one details `QLabel`
  (`coreConnectionDetails`) and Connect/Disconnect/Close buttons; opened by
  `MainWindow::showRemoteConnectionPanel()` (`MainWindow.cpp:1178-1187`).
- `src/gui/RemoteTelemetryController.cpp:275-303`: banner audio word from
  `playbackActive` (running, decoded > 0, device progress < 500 ms).
- Tests and fakes: `tests/tst_opus_audio_codec.cpp`,
  `tst_remote_audio_receiver.cpp`, `tst_remote_audio_session.cpp` (real
  DTLS/SRTP over `LoopbackTransport`; `latestAudioContext` helper `:38-47`),
  `tst_remote_media_controller.cpp`, `tst_daemon_media_controller.cpp`
  (old-client hello example `:1692`), `tst_station_session.cpp`,
  `tst_display_budget_contract.cpp` (pins `kSessionProtocolMinor == 7` at
  `:161`), `tst_remote_connection_controls.cpp`, `tst_remote_telemetry.cpp`;
  `tests/fakes/PacedAudioBus.h` (`blockOutputPacingAfterCalls` makes device
  timing disappear through the real path). Tests register with
  `nereus_add_test(<name> [extra sources])` in `tests/CMakeLists.txt`;
  core sources are listed in the root `CMakeLists.txt` near
  `src/core/session/media/DaemonAudioSender.cpp` (`:969`).

## File Structure

| Create | Responsibility |
| --- | --- |
| `src/core/session/media/RemoteAudioContext.{h,cpp}` | Shared wire codec for the minor-8 audio-context detail: encoder object, off reason, accepted-context value |
| `src/core/session/media/RtpReceptionStats.{h,cpp}` | RFC 3550 interarrival jitter and sequence-based expected/missing accounting for one context |
| `src/gui/RemoteAudioStatus.{h,cpp}` | Status value, pure state derivation, operator wording and detail formatting |
| `tests/tst_remote_audio_context.cpp` | Wire codec round trip and malformed rejection |
| `tests/tst_rtp_reception_stats.cpp` | Deterministic RFC 3550 cases |
| `tests/tst_remote_audio_status.cpp` | Derivation table and wording |

| Modify | Change |
| --- | --- |
| `src/core/session/media/OpusAudioCodec.{h,cpp}` | `OpusEncoderProfile`, `OpusAudioEncoder::profile()` read back from libopus |
| `src/core/session/media/DaemonAudioSender.{h,cpp}` | `encoderProfile()` |
| `src/core/session/SessionMessages.h` | minor 8 constants |
| `src/core/session/StationCapabilities.{h,cpp}`, `StationServer.{h,cpp}`, `StationClient.{h,cpp}` | `remoteAudioStatusVersion` capability and `remoteAudioStatusAvailable()` |
| `src/core/session/media/DaemonMediaController.{h,cpp}` | off reason and extended context for minor-8 sessions |
| `src/core/session/media/RemoteAudioReceiver.{h,cpp}` | typed `Fault`, new telemetry fields, sequence carried to the worker |
| `src/gui/RemoteMediaController.{h,cpp}` | accept both context shapes; own `RemoteAudioStatus`, failure identity, `retryAudio()` |
| `src/gui/RemoteConnectionController.{h,cpp}` | "Remote audio" section and Retry button in the Core connection panel |
| `src/gui/RemoteTelemetryController.cpp` | banner audio word from `RemoteAudioStatus`; new measurements in detail text |
| `src/gui/MainWindow.cpp` | pass the media controller to the panel |
| `CMakeLists.txt`, `tests/CMakeLists.txt` | register new sources and tests |

---

## Task 1: Core reports its actual audio context to minor-8 GUIs

**Requirements:** R-R3-23 (actual accepted codec profile; distinguish radio
offline, media not ready, local choice and Core failure). Compatibility with
minor-7 peers in both directions is part of the requirement.

**Files:**
- Create: `src/core/session/media/RemoteAudioContext.h`, `src/core/session/media/RemoteAudioContext.cpp`
- Modify: `src/core/session/media/OpusAudioCodec.h`, `src/core/session/media/OpusAudioCodec.cpp`
- Modify: `src/core/session/media/DaemonAudioSender.h`, `src/core/session/media/DaemonAudioSender.cpp`
- Modify: `src/core/session/SessionMessages.h`
- Modify: `src/core/session/StationCapabilities.h`, `src/core/session/StationCapabilities.cpp`
- Modify: `src/core/session/StationServer.h`, `src/core/session/StationServer.cpp`
- Modify: `src/core/session/StationClient.h`, `src/core/session/StationClient.cpp`
- Modify: `src/core/session/media/DaemonMediaController.h`, `src/core/session/media/DaemonMediaController.cpp`
- Modify: `src/gui/RemoteMediaController.h`, `src/gui/RemoteMediaController.cpp`
- Modify: `CMakeLists.txt` (add `src/core/session/media/RemoteAudioContext.cpp` beside `DaemonAudioSender.cpp`), `tests/CMakeLists.txt` (`nereus_add_test(tst_remote_audio_context)`)
- Test: `tests/tst_remote_audio_context.cpp` (new), `tests/tst_opus_audio_codec.cpp`, `tests/tst_daemon_media_controller.cpp`, `tests/tst_remote_audio_session.cpp`, `tests/tst_station_session.cpp`, `tests/tst_display_budget_contract.cpp`

**Interfaces:**
- Consumes: nothing from other tasks.
- Produces (exact):
  ```cpp
  // OpusAudioCodec.h
  struct OpusEncoderProfile {
      int sampleRate {0};       // RTP clock and decoder rate, Hz
      int channels {0};
      int frameSamples {0};     // per channel per packet
      int targetBitrate {0};    // constrained-VBR encoder target, bit/s; not measured traffic
      int audioBandwidthHz {0}; // coded audio bandwidth limit
      friend bool operator==(const OpusEncoderProfile&, const OpusEncoderProfile&) = default;
  };
  // OpusAudioEncoder
  std::optional<OpusEncoderProfile> profile() const; // nullopt when !isReady()

  // DaemonAudioSender
  std::optional<OpusEncoderProfile> encoderProfile() const;

  // SessionMessages.h
  inline constexpr quint16 kSessionProtocolMinor = 8;
  inline constexpr quint16 kRemoteAudioStatusSessionProtocolMinor = 8;

  // StationCapabilities: int remoteAudioStatusVersion = 0;  wire name "remoteAudioStatusVersion"
  // StationServer / StationClient
  bool remoteAudioStatusAvailable() const;

  // RemoteAudioContext.h  (namespace NereusSDR). The one codec both Core
  // and GUI use for the audio-context message, legacy and extended shapes.
  enum class RemoteAudioOffReason { ClientDisabled, MediaNotReady, RadioOffline, EncoderUnavailable };
  QString remoteAudioOffReasonToWire(RemoteAudioOffReason reason);
  std::optional<RemoteAudioOffReason> remoteAudioOffReasonFromWire(const QJsonValue& value);
  QJsonObject remoteAudioEncoderToJson(const OpusEncoderProfile& profile);
  std::optional<OpusEncoderProfile> remoteAudioEncoderFromJson(const QJsonValue& value);
  struct RemoteAudioContextMessage {
      QString connectionId;
      quint32 revision = 0;
      quint32 generation = 0;
      bool enabled = false;
      quint32 ssrc = 0;
      quint16 firstSequence = 0;
      quint32 firstTimestamp = 0;
      std::optional<OpusEncoderProfile> encoder;     // set only when enabled and detail negotiated
      std::optional<RemoteAudioOffReason> offReason; // set only when disabled and detail negotiated
  };
  // detailNegotiated=false: exactly today's eight keys (op, connectionId,
  // revision, generation, enabled, ssrc, firstSequence, firstTimestamp) with
  // today's JSON number types; encoder/offReason are not written.
  // detailNegotiated=true: those eight plus "encoder" (enabled) or "reason" (disabled).
  QJsonObject encodeRemoteAudioContext(const RemoteAudioContextMessage& message, bool detailNegotiated);
  // Shape and field validation only (identity checks stay with the caller).
  // Accepts exactly the shape selected by detailNegotiated; nullopt otherwise.
  std::optional<RemoteAudioContextMessage> decodeRemoteAudioContext(const QJsonObject& payload, bool detailNegotiated);

  // RemoteMediaController (public)
  std::optional<RemoteAudioContextMessage> acceptedAudioContext() const; // nullopt before the first accepted context and after stop()
  bool audioDetailNegotiated() const; // d->client && d->client->remoteAudioStatusAvailable()
  signals: void audioContextAccepted();
  ```

**Acceptance:**
- Wire strings are exactly `client-disabled`, `media-not-ready`,
  `radio-offline`, `encoder-unavailable`; `remoteAudioOffReasonFromWire`
  rejects any other string and any non-string.
- `remoteAudioEncoderToJson` produces exactly
  `{"codec":"opus","sampleRate":48000,"channels":2,"frameSamples":1920,"targetBitrate":24000,"audioBandwidthHz":8000}`
  for the default encoder. `remoteAudioEncoderFromJson` accepts only that
  exact key set, `codec == "opus"`, integral JSON numbers, `sampleRate ==
  48000`, `channels == 2`, `frameSamples == 1920`, `targetBitrate` in
  6000..510000, `audioBandwidthHz` in {4000, 6000, 8000, 12000, 20000};
  anything else (extra key, missing key, string number, 44100, 1 channel,
  fractional value, 510001) returns nullopt.
- `OpusAudioEncoder::profile()` reads the bitrate back from the live
  encoder (`OPUS_GET_BITRATE`) and reports the audio bandwidth from the one
  forced constant the constructor passes to `OPUS_SET_BANDWIDTH` (mapping
  NARROWBAND 4000, MEDIUMBAND 6000, WIDEBAND 8000, SUPERWIDEBAND 12000,
  FULLBAND 20000). `OPUS_GET_BANDWIDTH` is not used: it reports the last
  encoded frame and says FULLBAND on a new or reset encoder, and Core sends
  the context before the first encode (Task 1 ruling). The profile is
  checked immediately after construction and after reset. Default encoder:
  `{48000, 2, 1920, 24000, 8000}`; a
  48000 bit/s config reports 48000. Every packet the default encoder
  produces from a 997/1703 Hz stereo test signal inspects as
  `channels == profile.channels`, `samplesPerChannel == profile.frameSamples`,
  `bandwidth == OPUS_BANDWIDTH_WIDEBAND`.
- Capability: `StationServer::buildCapabilities().remoteAudioStatusVersion`
  is 1 with media enabled and 0 without; it round-trips through
  `toUpdates()`/`fromUpdates()`; `fromUpdates({})` gives 0.
- Core, minor-8 session: an enabled context has the eight existing keys plus
  `encoder` (and no `reason`), equal to `remoteAudioEncoderToJson(sender
  profile)`. A disabled context has the eight keys plus `reason` (and no
  `encoder`). Reason precedence in `reconcileAudio()`: not desired ->
  `client-disabled`; radio model missing or not connected -> `radio-offline`;
  peer missing or not ready -> `media-not-ready`; sender start failed ->
  `encoder-unavailable`. `onRadioConnectionStateChanged` (radio dropped)
  sends `radio-offline` when audio was desired, else `client-disabled`.
- Core, minor-7 session (client hello with minor 7): every audio context has
  exactly the eight existing keys, byte-for-byte the same shape as today.
- `decodeRemoteAudioContext(payload, false)` accepts exactly what today's
  GUI parser accepts (eight keys, `op == "audio-context"`, string
  `connectionId`, unsigned 32-bit integral `revision`/`generation`/`ssrc`,
  bool `enabled`, integral `firstSequence` 0..65535 and `firstTimestamp`
  0..4294967295) and rejects a nine-key message.
  `decodeRemoteAudioContext(payload, true)` requires nine keys: enabled with
  a valid `encoder` and no `reason`, or disabled with a valid `reason` and
  no `encoder`; both, neither, an eight-key message, an unknown reason or a
  malformed encoder give nullopt. `encode` then `decode` round-trips every
  field for both shapes and all four reasons.
- Core uses `encodeRemoteAudioContext(message, m_server->remoteAudioStatusAvailable())`;
  the GUI uses `decodeRemoteAudioContext(payload, audioDetailNegotiated())`
  and then applies its existing revision, generation, connection and SSRC
  checks. A rejected message is ignored exactly like today's malformed
  context (no generation advance, no receiver change, no signal).
- End to end with real peers: a minor-8 session delivers the encoder
  profile (enabled) and each reachable reason (disabled). A minor-7 session
  in both directions is emulated without production test hooks: a
  `LoopbackTransport` subclass in the test (the file already subclasses it,
  e.g. `ClosingGuiControlTransport`) rewrites the protocol minor inside the
  hello messages to 7 in both directions, so Core and GUI both agree minor
  7; audio still plays, contexts carry eight keys and
  `acceptedAudioContext()` has no encoder and no reason.
- `acceptedAudioContext()` is replaced on every accepted context and reset
  by `stop()`; `audioContextAccepted()` is emitted once per accepted context
  after the receiver start/stop decision. The receiving log line names the
  reported profile or says it was not reported, never a hard-coded format.
- `tst_display_budget_contract` keeps pinning
  `kRemoteDisplayBudgetSessionProtocolMinor == 7` and pins
  `kSessionProtocolMinor == 8` and `kRemoteAudioStatusSessionProtocolMinor == 8`.

**Verification:** networking-adjacent protocol change, so invariant
coverage first for the compatibility rows (minor 7 both directions), then the
new shape. Unit plus real-loopback session tests. Commands:
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_audio_context tst_opus_audio_codec tst_daemon_media_controller tst_remote_audio_session tst_remote_media_controller tst_station_session tst_display_budget_contract tst_receive_layout_session -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_audio_context|tst_opus_audio_codec|tst_daemon_media_controller|tst_remote_audio_session|tst_remote_media_controller|tst_station_session|tst_display_budget_contract|tst_receive_layout_session)$' --no-tests=error --output-on-failure
```
Then, because the protocol minor touches every session test, build
`tests_session` and run `ctest --test-dir .../build-integration -L session --no-tests=error --output-on-failure`.
Hardware (pending, controller/operator): installed minor-8 Core keeps audio
for the running minor-7 GUI; a minor-8 GUI against the old Core plays audio
and says the codec was not reported.

**Execution note (advisory):** opus (session protocol and compatibility).
No prerequisites. Serial.

- [ ] **Step 1:** Add the minor-7 compatibility tests first: in
  `tst_daemon_media_controller`, a client that says hello with minor 7
  receives audio contexts with exactly the eight keys for enabled and
  disabled cases; in `tst_remote_audio_session`, the hello-rewriting
  transport makes both ends agree minor 7 and playback still starts on the
  eight-key context. Confirm they pass on the current tree before changing
  production code.
- [ ] **Step 2:** Implement `OpusEncoderProfile` and `profile()`, the
  sender accessor, the shared `RemoteAudioContext` codec (encode and
  decode), minor 8, the capability and both `remoteAudioStatusAvailable()`
  predicates, mirroring every `remoteWidebandDisplayVersion` site.
- [ ] **Step 3:** Compute the off reason in `reconcileAudio()` and
  `onRadioConnectionStateChanged`, pass it to `sendAudioContext(bool enabled,
  RemoteAudioOffReason reason)`, and build the message with
  `encodeRemoteAudioContext(..., m_server->remoteAudioStatusAvailable())`.
- [ ] **Step 4:** Replace the GUI's inline audio-context field checks with
  `decodeRemoteAudioContext(payload, audioDetailNegotiated())` followed by
  the existing identity checks, store `acceptedAudioContext`, emit
  `audioContextAccepted()`, reset in `stop()`, and replace the hard-coded
  "48 kHz stereo Opus" log wording.
- [ ] **Step 5:** Add the acceptance tests above (new
  `tst_remote_audio_context` for the codec, codec profile/packet agreement,
  capability round trip, minor-8 enabled/disabled shapes with each
  reachable reason end to end), update the minor pin, run the commands,
  commit.

## Task 2: Receiver measures jitter and missing packets and types its faults

**Requirements:** R-R3-23 (useful measured health with clear meanings),
R-R3-07 (jitter/loss handling unchanged).

**Files:**
- Create: `src/core/session/media/RtpReceptionStats.h`, `src/core/session/media/RtpReceptionStats.cpp`
- Modify: `src/core/session/media/RemoteAudioReceiver.h`, `src/core/session/media/RemoteAudioReceiver.cpp`
- Modify: `CMakeLists.txt` (add `RtpReceptionStats.cpp`), `tests/CMakeLists.txt` (`nereus_add_test(tst_rtp_reception_stats)`)
- Test: `tests/tst_rtp_reception_stats.cpp` (new), `tests/tst_remote_audio_receiver.cpp`

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces (exact):
  ```cpp
  // RtpReceptionStats.h: RFC 3550 section 6.4.1 and appendix A.1/A.8 for one context.
  class RtpReceptionStats {
  public:
      explicit RtpReceptionStats(int clockRateHz = 48'000);
      void reset();
      // One call per packet the receiver admitted as Accepted or Late.
      void observe(quint16 sequence, quint32 rtpTimestamp, qint64 arrivalNs);
      quint64 receivedPackets() const;
      quint64 expectedPackets() const; // extended highest - first + 1; 0 before any packet
      quint64 missingPackets() const;  // max(0, expected - received)
      std::optional<double> jitterMs() const; // nullopt until two packets
  };

  // RemoteAudioReceiver
  enum class Fault {
      SpeakerOpenFailed, SpeakerTimingUnavailable, SpeakerCallbackTooLarge,
      SpeakerStalled, SpeakerWriteFailed, DecoderUnavailable,
      ArrivalBurst, StreamGap, NoPackets, DecodeFailed, ClockBuffer,
  };
  Q_ENUM(Fault)
  signals:
      void restartRequested(const QString& reason, NereusSDR::RemoteAudioReceiver::Fault fault);
      void errorOccurred(const QString& reason, NereusSDR::RemoteAudioReceiver::Fault fault);

  // RemoteAudioReceiverTelemetry additions
  std::optional<double> arrivalJitterMs;  // RFC 3550 jitter, this computer, this context
  quint64 expectedPackets = 0;
  quint64 missingPackets = 0;
  std::optional<double> reorderQueuedMs;  // packets held for ordering x 40 ms
  ```
  The reason string stays the first signal argument so existing
  `spy.first().first().toString()` assertions and existing `(const QString&)`
  connections keep working.

**Acceptance:**
- Jitter follows RFC 3550 A.8 exactly: for consecutive observed packets,
  `D = (arrival_i - arrival_prev) - int32(ts_i - ts_prev) * 1e9 / clockRate`
  in nanoseconds (signed 32-bit timestamp difference handles wrap),
  `J += (|D| - J) / 16`, reported in ms.
- Sequence accounting follows RFC 3550 A.1 without probation: first packet
  sets base and max; a packet with `uint16(seq - max) < 0x8000` advances
  max and adds 65536 to cycles when `seq < max`; older packets do not move
  max; received counts every observed packet; expected = cycles + max -
  base + 1; missing = expected - received floored at 0.
- `tst_rtp_reception_stats` cases, all deterministic:
  regular 100 packets, 40 ms and 1920 ticks apart: jitter exactly 0,
  expected 100, missing 0. Sequence 50 withheld: expected 100, received 99,
  missing 1, jitter 0. Three packets arriving together (timestamps 0, 1920,
  3840; equal arrival): jitter 2.5 ms after the second, 4.84375 ms after
  the third. Reorder 0,1,3,2,4: expected 5, missing 0. Sequence wrap from
  65530 over 12 packets: expected 12, missing 0. Timestamp wrap near
  0xFFFFFFFF with regular spacing: jitter 0. Before any packet: jitter
  nullopt, expected 0; after one packet: jitter nullopt; after reset():
  back to that initial state.
- Receiver: sequence travels from `submit()` into the worker packet;
  `observe()` runs for `Accepted` and `Late` admissions only (not
  Duplicate, Invalid, OutsideWindow); values publish through atomics inside
  the existing snapshot pattern, reset at every successful `start()` inside
  the odd telemetry sequence, and are unavailable (nullopt / 0) before a
  measurement and after `stop()` for `reorderQueuedMs`.
- In `tst_remote_audio_receiver`, the existing withheld-packet test
  (`rtpPlaybackLossAndFreshGeneration`, packet 8 never submitted, packet 5
  duplicated) additionally shows `missingPackets == 1`,
  `expectedPackets >= 24`, and a jitter value; after the second `start()`
  the new fields are back to their initial values. The coalesced-arrival
  test (`delayedCoalescedArrivalsKeepPlaybackContinuous`) shows jitter above
  5 ms while `missingPackets` equals the one deliberately missing frame, so
  spacing never counts as loss.
- Every `notify()` site and the synchronous start failure pass the fault
  listed here: beginRemotePlayback failure SpeakerOpenFailed; timing
  unavailable at start or during play SpeakerTimingUnavailable; callback
  exceeds capacity (both sites) SpeakerCallbackTooLarge; stopped consuming
  SpeakerStalled; write failure SpeakerWriteFailed; decoder or matcher
  init DecoderUnavailable; arrival queue bound ArrivalBurst; outside window
  StreamGap; no packets for 500 ms NoPackets; decode failure (both sites)
  DecodeFailed; clock buffer ClockBuffer. A test using
  `PacedAudioBus::blockOutputPacingAfterCalls` observes
  `errorOccurred` with `Fault::SpeakerTimingUnavailable` through the real
  worker path.
- No change to thresholds, hold times, queue bounds or restart behavior.

**Verification:** ordinary feature on a worker thread: deterministic unit
tests for the math, real-receiver tests for publication and reset.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_rtp_reception_stats tst_remote_audio_receiver tst_remote_audio_session tst_remote_media_controller tst_remote_telemetry -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_rtp_reception_stats|tst_remote_audio_receiver|tst_remote_audio_session|tst_remote_media_controller|tst_remote_telemetry)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** sonnet. Independent of Task 1 by files;
run after it, serially.

- [ ] **Step 1:** Write `tst_rtp_reception_stats` with the cases above and
  implement `RtpReceptionStats` until it passes.
- [ ] **Step 2:** Add `Fault`, change both signals, and pass the fault at
  every site; keep the reason text and bracketed detail unchanged.
- [ ] **Step 3:** Carry the sequence into the worker, feed
  `RtpReceptionStats`, publish the four telemetry fields, reset them on
  start, and extend the two receiver tests plus the fault-type test.
- [ ] **Step 4:** Run the commands and commit.

## Task 3: GUI owns a persistent remote audio status with Retry

**Requirements:** R-R3-23 (persistent playback/media/output status and a
recovery action; local controls separate from station slice mix), R-R3-17
(radio state distinct from session state), R-R3-07.

**Files:**
- Create: `src/gui/RemoteAudioStatus.h`, `src/gui/RemoteAudioStatus.cpp`
- Modify: `src/gui/RemoteMediaController.h`, `src/gui/RemoteMediaController.cpp`
- Modify: `CMakeLists.txt` (GUI source list), `tests/CMakeLists.txt` (`nereus_add_test(tst_remote_audio_status)`)
- Test: `tests/tst_remote_audio_status.cpp` (new), `tests/tst_remote_media_controller.cpp`

**Interfaces:**
- Consumes: Task 1 `acceptedAudioContext()` (returns
  `std::optional<RemoteAudioContextMessage>`), `audioDetailNegotiated()`,
  `audioContextAccepted()`, `RemoteAudioOffReason`, `OpusEncoderProfile`;
  Task 2 `RemoteAudioReceiver::Fault` and the two-argument signals.
- Produces (exact):
  ```cpp
  // RemoteAudioStatus.h
  struct RemoteAudioStatus {
      enum class State {
          NotConnected,     // no remote media session
          WaitingForAudio,  // requested, awaiting Core or the media link
          MutedHere,        // master mute on this computer
          RadioOffline,     // station radio not connected
          CoreCouldNotStart,// Core reported encoder-unavailable
          Starting,         // Core sending, speaker progress not yet seen
          Playing,          // speaker consuming audio now
          Reconnecting,     // interruption, automatic retry scheduled
          PlaybackProblem,   // local output/decoder failure, persists
      };
      State state = State::NotConnected;
      bool detailNegotiated = false;               // Core reports codec detail
      std::optional<OpusEncoderProfile> encoder;   // current accepted context only
      QString selectedOutput;                      // selected speakers device name, "System default" when default
      std::optional<RemoteAudioReceiver::Fault> problem; // persistent local fault
      bool retryAvailable = false;
      friend bool operator==(const RemoteAudioStatus&, const RemoteAudioStatus&) = default;
  };
  struct RemoteAudioStatusInputs {
      bool mediaSession = false;
      bool muted = false;
      bool radioConnected = false;
      std::optional<RemoteAudioContextMessage> context;
      bool receiverRunning = false;
      bool playing = false;       // running, decoded > 0, device progress younger than 500 ms
      bool restarting = false;
      std::optional<RemoteAudioReceiver::Fault> problem;
  };
  RemoteAudioStatus::State deriveRemoteAudioState(const RemoteAudioStatusInputs& in);
  QString remoteAudioHeadline(RemoteAudioStatus::State state);   // panel status words
  QString remoteAudioBannerWord(RemoteAudioStatus::State state); // title bar words
  QString remoteAudioProblemText(RemoteAudioReceiver::Fault fault);
  QString remoteAudioCodecText(const RemoteAudioStatus& status);

  // RemoteMediaController (public)
  RemoteAudioStatus audioStatus() const;
  public slots: void retryAudio();
  signals: void audioStatusChanged();
  ```

**Acceptance:**
- `deriveRemoteAudioState` precedence, first match wins: no media session
  NotConnected; muted MutedHere; problem present PlaybackProblem; radio not
  connected or context reason RadioOffline gives RadioOffline; disabled
  context with EncoderUnavailable CoreCouldNotStart; restarting
  Reconnecting; no context or disabled context WaitingForAudio; enabled
  context but receiver not running WaitingForAudio; playing Playing;
  otherwise Starting. The test is a table covering every row and the
  precedence between muted/problem/radio offline.
- Wording (exact): headlines NotConnected "Not connected to a Core",
  WaitingForAudio "Waiting for audio from Core", MutedHere "Muted on this
  computer", RadioOffline "Radio offline at the station", CoreCouldNotStart
  "Core could not start audio", Starting "Starting audio", Playing
  "Playing", Reconnecting "Audio interrupted, reconnecting",
  PlaybackProblem "Playback problem on this computer". Banner words: Playing "Audio playing",
  MutedHere "Audio muted", RadioOffline "Radio offline",
  CoreCouldNotStart and PlaybackProblem "Audio unavailable",
  WaitingForAudio/Starting/Reconnecting "Audio waiting", NotConnected
  "Audio stopped". Problem texts: SpeakerOpenFailed "The selected speaker
  device could not be opened.", SpeakerTimingUnavailable "The speaker
  device stopped reporting its timing.", SpeakerCallbackTooLarge "The
  speaker device buffer is larger than remote playback supports. Choose a
  smaller buffer or another device.", SpeakerStalled "The speaker device
  stopped playing audio.", SpeakerWriteFailed "Audio could not be sent to
  the speaker device.", DecoderUnavailable "The audio decoder could not
  start on this computer." Codec text: reported
  "Opus stereo, 24 kbit/s target, 40 ms packets, audio up to 8 kHz"
  (numbers from the profile: bitrate/1000, frameSamples*1000/sampleRate,
  audioBandwidthHz/1000, "stereo" for 2 channels, "mono" for 1); not
  negotiated "Not reported by this Core"; negotiated without a current
  encoder "Audio is off".
- Persistent failure: a receiver `errorOccurred` records
  `{fault, epoch, connectionId, contextGeneration, receiverGeneration}` from
  the controller's current values and the receiver's
  `telemetry().generation`, keeps the existing disable control, logs the
  raw reason with `qCWarning`, and emits `errorOccurred` with
  `remoteAudioProblemText(fault)` (never the bracketed detail). The problem
  is cleared only when, in the same epoch and connection, a newer accepted
  context (wrap-aware `newer()`) has a running receiver whose generation is
  above the recorded one and whose `deviceConsumedFrames > 0`. Mute,
  unmute, a device change, a disabled context, a restart or time alone
  never clear it. `stop()` clears it with the rest of the session.
- Restarts: a `restartRequested` sets `restarting` until the next accepted
  context or `stop()`; the existing 1 s debounce and retry are unchanged.
- `retryAudio()`: no-op without a media session or while muted; otherwise
  calls `requestAudio()` (new revision, enabled per mute/radio as today).
  `retryAvailable` is true only for PlaybackProblem or CoreCouldNotStart when
  not muted and a media session exists.
- `audioStatus()` is recomputed on accepted context, receiver fault,
  restart, mute change, speakers config change, radio connection change,
  `stop()`, and by a 250 ms `QTimer` owned by the controller that runs only
  while a receiver runs or a problem awaits recovery.
  `audioStatusChanged()` fires only when the value changes.
  `selectedOutput` comes from `AudioDeviceConfig::loadFromSettings("audio/Speakers")`
  and refreshes on `speakersConfigChanged`; it is labelled as the selection.
- Controller tests in `tst_remote_media_controller`, using real receivers
  and the real session harness: (a) a speaker fault produced through
  `PacedAudioBus` (timing withdrawn) yields PlaybackProblem with Retry
  available and a plain-English `errorOccurred`; it survives a subsequent
  mute/unmute and a disabled context; (b) `retryAudio()` sends an audio
  control with a higher revision and the problem clears only after the new
  context plays with device progress; (c) after `stop()` and a new session
  a late fault from the old session changes nothing; (d) mute gives
  MutedHere and does not change Core slice gain, pan or mute (extend the
  existing assertion); (e) with the Task 1 hello-rewriting transport (moved to `tests/fakes/` if a second file needs it), a minor-7 Core gives Playing with
  `detailNegotiated == false`; (f) a Core `radio-offline` context gives
  RadioOffline, `encoder-unavailable` gives CoreCouldNotStart with Retry.

**Verification:** consequential state transitions: invariant tests for
stale identity and clearing before wiring UI. Unit plus real-session tests.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_audio_status tst_remote_media_controller tst_remote_audio_session tst_remote_audio_receiver tst_remote_telemetry -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_audio_status|tst_remote_media_controller|tst_remote_audio_session|tst_remote_audio_receiver|tst_remote_telemetry)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus (lifecycle identity and queued
callbacks). Requires Tasks 1 and 2. Serial.

- [ ] **Step 1:** Write `tst_remote_audio_status` (derivation table and
  exact wording) and implement `RemoteAudioStatus.{h,cpp}`.
- [ ] **Step 2:** Add the failure record, restart flag, status timer,
  `audioStatus()`, `audioStatusChanged()` and `retryAudio()` to
  `RemoteMediaController`, with the clearing rule above.
- [ ] **Step 3:** Add controller tests (a) to (f), run the commands,
  commit.

## Task 4: Show remote audio status in the Core connection panel and title bar

**Requirements:** R-R3-23, R-R3-21 (visible controls act on their owner and
show accepted state), R-R3-32/35 (measurement sources and units stated).

**Files:**
- Modify: `src/gui/RemoteConnectionController.h`, `src/gui/RemoteConnectionController.cpp`
- Modify: `src/gui/RemoteTelemetryController.cpp`
- Modify: `src/gui/MainWindow.cpp`
- Modify: `src/gui/RemoteAudioStatus.h`, `src/gui/RemoteAudioStatus.cpp` (add `formatRemoteAudioDetails`)
- Test: `tests/tst_remote_audio_status.cpp`, `tests/tst_remote_connection_controls.cpp`, `tests/tst_remote_telemetry.cpp`

**Interfaces:**
- Consumes: Task 3 `RemoteAudioStatus`, `audioStatus()`,
  `audioStatusChanged()`, `retryAudio()`, the wording functions; Task 2
  telemetry fields; existing `RemoteMediaController::audioTelemetry()`.
- Produces:
  ```cpp
  // RemoteAudioStatus.h
  QString formatRemoteAudioDetails(const RemoteAudioStatus& status,
                                   const RemoteAudioReceiverTelemetry& playback);
  // RemoteConnectionPanel constructor gains a trailing
  // RemoteMediaController* media = nullptr argument.
  ```

**Acceptance:**
- `formatRemoteAudioDetails` returns these lines, in order, each omitted
  only where stated:
  `Remote audio: <headline>`;
  `Problem: <problem text>` only when a problem exists;
  `Codec: <codec text>`;
  `Output: <selectedOutput> (selected)`;
  `Arrival jitter: <n> ms` or `Arrival jitter: not measured yet`;
  `Missing packets: <missing> of <expected>` or `Missing packets: none received yet`;
  `Gaps filled: <concealedPackets>`;
  `Speaker buffer: <n> ms on this computer` or `Speaker buffer: not measured yet`.
  Health lines are omitted when the state is NotConnected, MutedHere or
  RadioOffline. Numbers are integers (jitter and buffer rounded).
- Panel: when constructed with a media controller, a "Remote audio"
  section appears below the existing details: a `QLabel` with objectName
  `remoteAudioDetails` showing `formatRemoteAudioDetails(...)`, and a
  `QPushButton` "Retry audio" with objectName `retryRemoteAudio`, enabled
  exactly when `audioStatus().retryAvailable`. Clicking it calls
  `retryAudio()`. The section refreshes on `audioStatusChanged()` and on a
  1 s timer while the panel is visible. Without a media controller the
  panel is unchanged.
- `MainWindow::showRemoteConnectionPanel()` passes `m_remoteMedia`. The
  existing media toast stays; it now shows the plain-English problem text.
- Title bar: `RemoteTelemetryController::bannerText()` uses
  `remoteAudioBannerWord(m_media->audioStatus().state)` when the media
  controller exists; the existing `playbackActive` wording remains the
  fallback when it does not, so existing deterministic banner tests stay
  valid. `detailText()` adds the four measurements with their definitions
  (arrival jitter measured on this computer; missing packets are sequence
  numbers never received; gaps filled are concealed 40 ms intervals;
  speaker buffer is not total delay) and no RTP/generation words.
- Tests: formatting rows for Playing with a reported codec, a minor-7 Core,
  PlaybackProblem, MutedHere (health omitted), unmeasured values; panel test
  (offscreen) finds `remoteAudioDetails` and `retryRemoteAudio`, button
  disabled while playing and enabled after a real speaker fault, click
  produces a new audio control revision; banner test shows "Audio muted"
  with mute and "Audio unavailable" after a speaker fault. Grab the panel
  with `QWidget::grab()` in the panel test and save it under the test's
  temporary directory so the controller can inspect the rendering.

**Verification:** console interaction: functional checks that exercise the
button and observe the resulting control, plus a rendered capture; native
look and feel on macOS is pending until the operator sees it.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_audio_status tst_remote_connection_controls tst_remote_telemetry tst_remote_media_controller NereusSDR -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_audio_status|tst_remote_connection_controls|tst_remote_telemetry|tst_remote_media_controller)$' --no-tests=error --output-on-failure
```
Human smoke (pending, operator on the Mac with the Rock): open Core
connection details while listening; see Playing and the codec line; mute
shows "Muted on this computer"; select a speaker device and then remove it
(or turn off Bluetooth headphones) to see the playback problem persist past the
toast; restore the device and press Retry audio; Playing returns.

**Execution note (advisory):** sonnet. Requires Task 3. Serial.

- [ ] **Step 1:** Add `formatRemoteAudioDetails` and its tests.
- [ ] **Step 2:** Add the panel section, the MainWindow argument and the
  banner/detail wording, with the panel and banner tests.
- [ ] **Step 3:** Run the commands, including the `NereusSDR` target, and
  commit.

## Final checks (controller)

1. Whole-branch review of the four tasks, one fix wave, one scoped
   re-review.
2. Full gate on the finished revision, recording load averages:
   ```sh
   cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target NereusSDR nereusd all_tests -j6
   ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -j6 --no-tests=error --output-on-failure
   ```
3. Evidence document
   `docs/architecture/2026-09-20-remote-daemon-r3-verification/remote-audio-status.md`
   and the R3 plan's 5a status, recording source, installation and
   operator acceptance separately.
4. Hardware checkpoint (controller with the operator): native Rock build and
   durable install of the signed revision; confirm the running older GUI
   keeps audio against the minor-8 Core; relaunch the new GUI from the exact
   bundle; the operator performs the human smoke above. Until then hardware
   rows stay pending.
