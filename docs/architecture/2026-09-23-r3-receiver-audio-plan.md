# R3 receiver audio: TCI and VAX in a remote window implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.
> Runs in the second builder lane after the lane B carry (the lane is reset to the
> integration head first). Tasks 4 and 5 need the Setup page scope from the remote
> window Setup plan's Task 1; carry that into the lane before starting them.

**Goal:** apps on the operator's computer (WSJT-X, JTDX, fldigi and TCI clients)
work through a remote window the way they do with a local radio: TCI is served
by the window, VAX channels carry a receiver's audio, and each receiver's audio
comes from the Core on its own stream at the quality the operator chose.

**Architecture:** the Core taps each receiver's audio at the same point local VAX
does (before the mix, level-independent of the speaker volume) and sends a
receiver on its own stream only while an app on the client asks for it. The
streams share the one media connection, declared up front for apps that ask, and
follow the session's single audio quality choice with one link trial and one
fallback for all of them. In the window, a receiver stream is consumed without a
speaker and feeds the TCI server's per-receiver rings or a VAX output paced by
that output's own clock. Transmit through TCI or VAX waits for remote transmit.

**Tech stack:** C++20, Qt 6, Qt Test (off-screen), libdatachannel (RTP),
libopus, `AudioEngine`, `DaemonAudioSource`, `DaemonAudioSender`,
`DaemonMediaController`, `MediaPeer`, `LibDataChannelMediaTransport`,
`StationCapabilities`, `StationServer`, `RemoteAudioReceiver`,
`RemoteMediaController`, `TciServer`, `TciProtocol`, VAX buses
(`CoreAudioHalBus`, PipeWire, PortAudio), WSJT-X command-line tools for the
decode bench.

**Spec:** operator answers of 2026-09-23: a remote window serves TCI (commands to
the Core, receive audio from the network, transmit with remote transmit, raw I/Q
not offered remotely with a plain reason); per-receiver streams sent only while
an app listens; VAX works in a remote window whenever the link can carry it and
falls back to Opus; VAX and TCI streams follow the one Audio quality choice;
measure how FT8 and other digital modes decode through Opus. R3 plan requirements
R-R3-21, R-R3-23, R-R3-35 and new R-R3-42, R-R3-43, R-R3-44 (added to the R3
plan's requirement table with this plan). Scout notes with file:line (re-locate by
text): `/Users/j.j.boyd/.config/nereus/work/r3-receiver-audio-scout-2026-09-23.md`
and `/Users/j.j.boyd/.config/nereus/work/r3-remote-vax-audio-pages-scout-2026-09-23.md`
(sections D and "Other bug").

## Global Constraints

- Work in `/Users/j.j.boyd/.codex/worktrees/nereus-lane-b/NereusSDR`, branch
  `codex/lane-b`, build directory `build-lane-b`; never checkout, rebase, merge,
  reset or push; never touch the integration worktree.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash
  characters. Stage explicit paths only. Every commit names its R-R3 IDs.
- Protocol: a per-feature capability version (`receiverAudioVersion`), never a
  change to `kSessionProtocolMinor`. Older apps and older Cores see exactly
  today's wire (the goldens listed in the scout notes, section B fact 11, stay
  byte-for-byte). Wire reason strings are new constants documented in the media
  control document; the app shows them through `OperatorReasonText`.
- Operator wording: plain user words; every new string passes
  `OperatorWording::isPlain`.
- Nothing on the DSP callback does network or codec work; taps hand off through
  bounded non-blocking queues like `DaemonAudioSource`.
- Local (direct radio) operation unchanged except the two local bugs this plan
  fixes (the VAX tee's AF gain, the TCI shared ring), each with its own test.
- Tests: prefix every ctest and test binary with `QT_QPA_PLATFORM=offscreen`.
  Build exact targets, then `ctest -R '^(...)$' --no-tests=error
  --output-on-failure`. No unfiltered suite. Never build the `NereusSDR` target.
  No hardware; hardware evidence stays pending for the operator checkpoint.

## What already exists

- One media connection per Core with one send-only audio line carrying the stereo
  mix (Opus, or lossless L16 when negotiated), a revisioned `audio` control op with
  an `audio-context` reply, a lossless link trial with automatic fallback, and a
  clock probe (scout notes A3, B7-B10).
- The local VAX tee in `AudioEngine::rxBlockReady` sends each slice's pre-mix audio
  to its VAX channel, scaled by the channel gain and 1 / AF gain (scout D13). Bug:
  it divides by receiver 1's AF gain for every slice.
- `RemoteAudioReceiver`: jitter buffer, decoder, WDSP rate matcher paced by the
  speaker, clock estimator (scout C12).
- `TciServer` two `AudioRingSpsc` rings, a 5 ms drain with per-client resampling,
  receivers 0 and 1; TCI receiver N is slice N (scout D19-D20). Known local defects:
  two clients on one receiver split the audio; mono paths read a stereo ring as
  mono (scout E6).
- VAX outputs per platform: macOS HAL plugin shared-memory ring, Linux PipeWire or
  pactl, Windows user-bound virtual cables; opened only when the local engine
  starts (scout D14).
- Spectrum endpoints' keyed map with revisions, a cap and retirement on slice
  removal: the bookkeeping pattern to reuse (scout A6).

## Task 1: Several audio streams on one media connection

**Requirements:** R-R3-43, R-R3-03, R-R3-05.

**Files:**
- Modify: `src/core/session/media/IMediaTransport.h`,
  `src/core/session/media/LibDataChannelMediaTransport.{h,cpp}`,
  `src/core/session/media/MediaPeer.{h,cpp}`,
  `docs/architecture/2026-09-20-remote-media-control-v1.md`
- Test: `tests/tst_media_transport.cpp`, `tests/tst_media_peer.cpp`

**Interfaces:**
- Produces: receiver stream ids derived from
  `"NereusSDR/media-receiver-ssrc/v1:<n>:" + connectionId` for n in 0..3, distinct
  from the main id; a transport option that declares them in the offer and lets
  send and receive filters accept exactly the declared set.

**Acceptance:**
- First, a transport test records what libdatachannel does with an RTP packet
  whose stream id the offer never declared (delivered or dropped); the report
  states the observed behaviour. The design declares the receiver ids either way.
- Without the option, the offer, answer and every audio line are byte-identical to
  today (existing golden).
- With the option, a declared second stream arrives intact beside the main one; an
  undeclared id is refused and reported, as today.
- The app's receive queue keeps at least 256 ms of cushion with the main stream and
  four lossless receiver streams (size it from the declared set).

**Verification:** transport contract, unit and loopback.
```sh
cmake --build build-lane-b --target tst_media_transport tst_media_peer -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir build-lane-b -R '^(tst_media_transport|tst_media_peer)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus. First task; the rest build on it.

- [ ] **Step 1:** Undeclared-stream test (observe and record), then declared ids,
  filters, queue sizing and the golden; commit.

## Task 2: The Core sends a receiver's audio on its own stream

**Requirements:** R-R3-43, R-R3-06, R-R3-09, R-R3-21.

**Files:**
- Modify: `src/core/AudioEngine.{h,cpp}` (per-slice tap slots beside the VAX tee,
  with the same channel-independent scaling local VAX uses; the VAX tee divides
  by the feeding slice's own AF gain, fixing the receiver-1 bug),
  `src/core/session/media/DaemonAudioSource.{h,cpp}` and
  `DaemonAudioSender.{h,cpp}` (a master-or-slice source; 1920-frame blocks with
  gap-aware positions), `src/core/session/media/DaemonMediaController.{h,cpp}`
  (a `receiver-audio` op, a per-slice stream map capped at 4, lossless admission
  through the existing profile rules, retirement), `src/core/session/StationCapabilities.{h,cpp}`
  and `src/core/session/StationServer.cpp` (`receiverAudioVersion` 1),
  `src/core/session/media/RemoteAudioContext.{h,cpp}` (`receiver-audio-context`),
  `docs/architecture/2026-09-20-remote-media-control-v1.md`
- Test: a new `tests/tst_audio_engine_slice_tap.cpp`,
  `tests/tst_daemon_audio_source.cpp`, `tests/tst_daemon_media_controller.cpp`,
  `tests/tst_remote_audio_context.cpp`, `tests/tst_audio_engine_vax_tee.cpp`,
  `tests/tst_audio_engine_vax_gain.cpp`

**Interfaces:**
- Consumes: Task 1's declared receiver stream ids.
- Produces: `{op:"receiver-audio", connectionId, sliceId, revision, enabled,
  profile}` and its `receiver-audio-context` reply (full profile shape; reasons
  `slice-removed`, `receiver-limit`, `radio-offline` only in this context);
  `receiverAudioVersion` capability 1; the app declares `receiverAudioVersion:1` in
  its media start only when the Core advertises it.

**Acceptance:**
- The slice tap delivers exactly slice B's samples whatever slice mute, slice gain,
  pan or master volume say, scaled the way local VAX scales them; the master tap
  and today's mix stream are unchanged byte-for-byte.
- Local VAX: a channel fed by slice B is scaled by slice B's AF gain, not
  receiver 1's (red first).
- A request from an app that did not declare the version is ignored; a stale
  revision is ignored; a fifth concurrent stream gets `receiver-limit`; the main
  stream and each receiver stream start, stop and restart independently (starting
  a receiver stream never restarts the speakers' stream).
- A stream follows the session's audio profile: Opus at the main stream's current
  bitrate and bandwidth, or lossless when the operator chose lossless and the
  profile rules admit it. (Superseded 2026-09-24 by the R3 completion plan,
  Task 7: compressed receiver streams run Opus at 48 kbit/s fullband whatever
  the main stream's bitrate.)
- Removing the slice, the radio going offline and the session ending each retire
  the stream with its reason; nothing leaks.
- Goldens for older apps and Cores unchanged.

**Verification:** Core media contract, unit and controller integration.
```sh
cmake --build build-lane-b --target tst_audio_engine_slice_tap tst_audio_engine_vax_tee tst_audio_engine_vax_gain tst_daemon_audio_source tst_daemon_media_controller tst_remote_audio_context -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir build-lane-b -R '^(tst_audio_engine_slice_tap|tst_audio_engine_vax_tee|tst_audio_engine_vax_gain|tst_daemon_audio_source|tst_daemon_media_controller|tst_remote_audio_context)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus. Requires Task 1.

- [ ] **Step 1:** Slice tap, VAX tee fix and source option with tests.
- [ ] **Step 2:** Protocol op, stream map, admission, retirement, capability and
  goldens; commit.

## Task 3: The window receives receiver streams without a speaker

**Requirements:** R-R3-43, R-R3-07, R-R3-23, R-R3-35.

**Files:**
- Modify: `src/core/session/media/RemoteAudioReceiver.{h,cpp}` (a PCM-sink mode:
  no speaker, paced by the Core's RTP clock through the jitter release, no rate
  matcher), `src/gui/RemoteMediaController.{h,cpp}` (`receiverAudioNegotiated()`,
  per-slice wanted flags driven by consumers, split by stream id before the speaker
  gate, receiver streams keep flowing while the speakers are muted, one lossless
  link trial across every lossless stream with one fallback for all),
  `src/gui/RemoteAudioStatus.{h,cpp}` (each stream's state and measured health),
  `tests/fakes/RemoteAudioSessionHarness.h` (two-slice tones; a knob that hides
  receiver audio)
- Test: `tests/tst_remote_audio_receiver.cpp`, `tests/tst_remote_media_controller.cpp`,
  `tests/tst_remote_audio_session.cpp`

**Interfaces:**
- Consumes: Task 2's op, reply and capability.
- Produces: `RemoteMediaController::requestReceiverAudio(int sliceId, IReceiverPcmSink*)`
  and `releaseReceiverAudio(int sliceId, IReceiverPcmSink*)` (reference-counted per
  slice); `IReceiverPcmSink` receives interleaved stereo 48 kHz float blocks and a
  stopped-with-reason notice.

**Acceptance:**
- A consumer starts with no speaker device configured; blocks arrive in order;
  Opus loss is concealed and lossless loss becomes silence.
- With the speakers muted, the slice-B consumer receives slice B's tone (1579 Hz),
  not slice A's (617 Hz); the speakers' mix is untouched.
- A retired stream stops with its reason and the consumer idles on silence (no
  restart loop).
- The lossless trial counts every lossless stream; one fallback moves all of them
  to Opus with one notice in the remote audio status.
- An older Core (no capability): no request is sent and consumers get a plain
  "this Core cannot send a receiver's audio" stop; with receiver audio hidden the
  controls on the wire are byte-for-byte today's.

**Verification:** remote audio contract, unit and harness integration.
```sh
cmake --build build-lane-b --target tst_remote_audio_receiver tst_remote_media_controller tst_remote_audio_session -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir build-lane-b -R '^(tst_remote_audio_receiver|tst_remote_media_controller|tst_remote_audio_session)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus. Requires Task 2.

- [ ] **Step 1:** Sink mode, controller wiring, status and harness cases; commit.

## Task 4: TCI in a remote window

**Requirements:** R-R3-42, R-R3-21, R-R3-25.

**Files:**
- Modify: `src/core/settings/SettingsScope.cpp` (the `Tci` keys belong to this
  computer; the two log-window keys stay as they are; values stored on a Core are
  ignored, not migrated), `src/core/TciServer.{h,cpp}` and
  `src/core/TciProtocol.{h,cpp}` (remote mode: no RxChannel or I/Q taps; receive
  audio for receiver N from the window's receiver stream for Core slice N through
  Task 3's sink; per-client read positions so two clients on one receiver each get
  the full audio, local and remote; correct mono from the stereo ring, local and
  remote; transmit refused: no MOX write, no TX-audio lock, no TX_CHRONO, reply
  `trx:N,false`; init burst `tx_enable` false and `receive_only` true in a remote
  window; `iq_start` refused with no subscription and no echo; an older Core:
  `audio_start` not echoed), `src/gui/MainWindow.cpp`,
  `src/gui/applets/TciApplet.cpp`, `src/gui/setup/AudioTciPage.cpp` and the TCI Server
  page in `src/gui/setup/CatNetworkSetupPages.cpp` (usable in a remote window; scope
  ThisComputer)
- Test: a new `tests/tst_tci_remote_window.cpp`, `tests/tst_tci_tx_mutex.cpp`,
  `tests/tst_tci_iq_roundtrip.cpp`, `tests/tst_tci_init_burst_golden.cpp` (local
  golden unchanged; a remote variant), `tests/tst_settings_scope.cpp`, a
  two-client and a mono case for the local server

**Interfaces:**
- Consumes: Task 3's `requestReceiverAudio` / `releaseReceiverAudio`; the remote
  window Setup plan's `SetupScope`.

**Acceptance:**
- In a remote window, TCI `vfo` and `modulation` change the Core's slice; the
  reply reflects the Core's accepted value; a refused write re-broadcasts the
  current value.
- `audio_start` for receiver N plays the Core's slice N at the session's quality
  (lossless when chosen); `audio_stop` releases the stream; the stream runs only
  while a client listens.
- Transmit and raw I/Q are refused as listed; the plain reason goes to the TCI log
  window, the TCI applet and a toast, never onto the TCI wire.
- Two local or remote clients on receiver 0 each receive the full audio (red
  first); a mono client receives correct mono (red first if wrong today).
- The local init-burst golden is unchanged; the remote burst differs only in
  `tx_enable` and `receive_only`.

**Verification:** TCI contract, unit and remote-window harness.
```sh
cmake --build build-lane-b --target tst_tci_remote_window tst_tci_tx_mutex tst_tci_iq_roundtrip tst_tci_init_burst_golden tst_settings_scope -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir build-lane-b -R '^(tst_tci_remote_window|tst_tci_tx_mutex|tst_tci_iq_roundtrip|tst_tci_init_burst_golden|tst_settings_scope)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator checkpoint): JTDX over TCI in a remote window, Opus and
lossless, speakers muted.

**Execution note (advisory):** opus. Requires Task 3 and the remote window Setup
plan's Task 1.

- [ ] **Step 1:** Remote mode, refusals, settings scope and tests.
- [ ] **Step 2:** Ring fixes (local and remote) with red-first tests; commit.

## Task 5: VAX in a remote window

**Requirements:** R-R3-44, R-R3-21, R-R3-23.

**Files:**
- Modify: `src/core/AudioEngine.{h,cpp}` (a remote window opens its VAX outputs;
  nereusd opens none on the Core host; each VAX output reports its playback
  timing: the HAL ring's read and write positions on macOS, PipeWire timing on
  Linux, PortAudio timing where the output is a PortAudio device), a new
  `src/core/audio/RemoteVaxFeeder.{h,cpp}` (a receiver-stream sink per VAX channel,
  rate-matched against that output's own clock), `src/gui/applets/VaxApplet.cpp`,
  `src/gui/widgets/VfoWidget.cpp` (VAX selector), `src/gui/SpectrumOverlayPanel.cpp`
  (VAX combo), `src/gui/setup/AudioVaxPage.cpp` and the VAX groups of
  `src/gui/setup/AudioAdvancedPage.cpp` (usable in a remote window; scope
  ThisComputer), the channel-to-slice assignment for a remote window stored on this
  computer per Core and slice (not the Core's `Slice<N>/VaxChannel`), consumer
  count wiring where the platform reports it
- Test: a new `tests/tst_remote_vax_feeder.cpp`, `tests/tst_remote_gui_gating.cpp`
  (the VAX rows move to enabled), `tests/tst_settings_scope.cpp`

**Interfaces:**
- Consumes: Task 3's receiver sink API; the remote window Setup plan's `SetupScope`.

**Acceptance:**
- Assigning slice B to VAX 1 in a remote window puts slice B's audio on VAX 1 at
  the session's quality, at the same level local VAX gives for the same signal.
- Where the platform reports whether an app is reading a VAX output, the stream
  runs only while one is; elsewhere it runs while the channel is assigned. No
  reader never raises a playback stall fault.
- The feeder keeps the VAX output fed without growing delay over a 10-minute run
  of the harness clock with a 200 ppm clock difference.
- Send IQ to VAX is refused in a remote window with a plain reason; VAX as the
  microphone source stays behind the transmit gate until remote transmit.
- nereusd publishes no VAX devices; the VAX first-run check stays skipped in
  remote windows.

**Verification:** VAX feed contract, unit and gating.
```sh
cmake --build build-lane-b --target tst_remote_vax_feeder tst_remote_gui_gating tst_settings_scope -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir build-lane-b -R '^(tst_remote_vax_feeder|tst_remote_gui_gating|tst_settings_scope)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator checkpoint): WSJT-X on VAX in a remote window, Opus and
lossless, side by side with a local decode.

**Execution note (advisory):** opus. Requires Task 3 and the remote window Setup
plan's Task 1.

- [ ] **Step 1:** Output timing and the feeder with tests.
- [ ] **Step 2:** Remote VAX surfaces, assignment storage, refusals; commit.

## Task 6: Remote playback on any speaker format

**Requirements:** R-R3-23, R-R3-07.

**Files:**
- Modify: `src/core/AudioEngine.{h,cpp}` (remote playback accepts the device rates
  and channel counts the Devices page offers), `src/core/session/media/RemoteAudioReceiver.{h,cpp}`
  (the rate matcher targets the device's rate; mono devices get a downmix)
- Test: `tests/tst_remote_audio_receiver.cpp`, `tests/tst_remote_media_controller.cpp`

**Acceptance:**
- 44.1 kHz, 48 kHz and 96 kHz stereo devices and a mono device all play remote
  audio; today's refusal for anything but 48 kHz stereo is gone.
- The delay readout stays inside its accuracy at each rate.

**Verification:**
```sh
cmake --build build-lane-b --target tst_remote_audio_receiver tst_remote_media_controller -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir build-lane-b -R '^(tst_remote_audio_receiver|tst_remote_media_controller)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus. Independent of Tasks 4-5.

- [ ] **Step 1:** Device-rate matching and downmix with tests; commit.

## Task 7: How digital modes decode through Opus

**Requirements:** R-R3-43, R-R3-23.

**Files:**
- Create: `tools/digital-mode-opus-bench/` (a script and a small codec tool that
  runs audio through the Core's exact Opus encoder and decoder settings for each
  quality choice), `docs/architecture/2026-09-20-remote-daemon-r3-verification/digital-modes-over-opus.md`
  (method and results)

**Acceptance:**
- FT8 signals generated with WSJT-X's `ft8sim` at SNRs from -10 to -24 dB (at
  least 20 signals per step) are decoded with `jt9 -8` from the untouched audio and
  from the audio after each Opus setting; the results table gives decodes per step
  and the weakest SNR decoded for each. The same for FST4 or Q65 with their
  simulators where available. The tools are in `/Applications/wsjtx.app/Contents/MacOS/`.
- The report says plainly whether Opus at each setting costs decodes, and by how
  much; any change to what the operator sees (a note on the VAX page, a different
  Opus setting for receiver streams) comes back to the operator as a decision, not
  built here.
- The run is reproducible from one documented command.

**Verification:** measurement; the results document is the evidence. Hardware
(pending, operator checkpoint): a live side-by-side decode.

**Execution note (advisory):** opus. Independent; can run any time after Task 2
(it needs the Core's Opus settings, which exist today).

- [ ] **Step 1:** Codec tool, generation and decode script, results document;
  commit.
