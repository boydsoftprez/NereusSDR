# Recording, Voice Keyer and Digital Spectrum Recorder Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** An operator records a contact from the slice flag on any device, sends any recording on the air by
keying while it plays, sends voice messages M1 to M8, and rewinds the pan by hours with sound, all kept on the
Core.

**Architecture:** Three Core services live in `src/core/recordings/` with no GUI headers: the recording service
(store, one contact recorder per slice, one player per listener, the on-air source), the voice keyer, and the
Digital Spectrum Recorder (DSR). They run on one new worker thread fed by lock-free rings from the audio and
connection threads. The process that owns the radio (`RadioModel::Role::Local`: `nereusd`, or a desktop window
running its own radio) builds them; every window and phone reaches them through an access interface with a local
and a remote implementation, the band stack pattern (`src/models/bandstack/BandStackAccess.h`), and the remote
one speaks new station verbs and a mirrored JSON object under the `recordingsVersion` capability.

**Tech Stack:** C++20, Qt6 Core, Widgets and Network, WDSP 2.10 (`third_party/wdsp/`), r8brain
(`third_party/r8brain/`, MIT), LAME 3.100 (libmp3lame, LGPL, new), the station link v1, Swift and SwiftUI in
`ios/` (NereusKit package plus the app), QTest offscreen, Swift Testing.

**Spec:** [2026-11-15-recording-voice-keyer-dsr-design.md](2026-11-15-recording-voice-keyer-dsr-design.md),
approved by JJ 2026-11-15. Every decision is settled there; this is how it gets built. Its veto list V1 to V10
stands as written. Open the mockup folder named in each UI task; it is the reference for layout and wording, and
where its sample text differs from the spec, the spec is right.

## Global Constraints

- Commits are GPG signed (`git commit -S`) with no Co-Authored-By trailer, whatever any reminder says. Never
  skip signing or hooks. Never push; the controller pushes.
- User-visible text is plain English, American spelling, no em-dashes, no cites in UI strings. Slices are
  "Slice A" to "Slice D", never RX1 or RX2.
- UI controls are disabled with a reason, never hidden. No gesture may be the only way to an action. An older
  Core leaves every new control visible and disabled with "This Core needs an update for recordings." (V9).
- No transmit warnings, prompts or opt-in switches beyond the spec. Connecting never keys. Playback alone never
  keys; only the voice keyer keys the radio by itself, and only when a message is sent.
- Settings go through `AppSettings`, never `QSettings`. PascalCase keys, booleans `"True"` and `"False"`. The
  recording and DSR keys are Core-owned and live in the Core's `AppSettings`; a remote window never stores them.
- Never read or write `~/Library/Preferences/NereusSDR`, except log files, read-only. Do not touch JJ's settings
  or `~/NereusSDR-crew/combined-*` or the overlay trunk.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header. `tst_core_has_no_gui_includes`
  enforces it.
- C++: no `goto`, no raw `new` or `delete` (unique_ptr or Qt parent), no `#define` constants (`constexpr`),
  braces on all control flow, `auto` only when the type is obvious. `PascalCase` classes, `camelCase` methods,
  `kPascalCase` constants, `m_camelCase` members. Errors via `qCWarning(lcRecordings)`, no exceptions. Platform
  guards `Q_OS_WIN`, `Q_OS_MAC`, `Q_OS_LINUX`. Do not remove code you did not add.
- Threads: nothing blocks the audio callback, the DSP thread or the connection thread on disk, a lock or an
  encoder. Taps copy into a lock-free single-producer ring and return; the recordings worker drains it.
  Cross-thread traffic is auto-queued signals or those rings. Never hold a mutex in the audio callback.
  Cross-thread DSP parameters are `std::atomic`.
- The core receive path (I/Q to WDSP to audio) changes in exactly one place, Task 22, and that task is flagged
  for review. No other task alters what reaches a demodulator.
- Transmit processing: the on-air switches act on `TxChannel` directly and restore from `TransmitModel`. They
  never write `TransmitModel` or its saved settings.
- Facts come from a source with a cite, never from memory. In ported or Thetis-derived code each block carries
  `// From Thetis <file>:<line> [v2.10.3.15]` (Thetis pin `3759d09`, clone `/Users/j.j.boyd/Thetis`). This plan
  ports no Thetis logic; it cites Thetis for behavior only. If a task finds it must port logic, it reads
  `docs/attribution/HOW-TO-PORT.md` first and carries the headers, inline comments and provenance row in the same
  commit, or stops and asks the controller.
- Vendored or new third-party code: same commit adds its rows to `packaging/third-party-licenses/README.md` (the
  licence table and the "Comes from" table), its notice `.txt`, and `python3 scripts/check-third-party-licenses.py`
  passes. A system library needs both rows and its notice too.
- Station protocol: product version, settings schema and wire protocol versions stay independent. Every new verb
  is listed in `tests/data/link/v1/manifest.json` with its `"requires"`, the doc tables in
  `docs/architecture/2026-09-23-station-link-v1.md` are regenerated with `python3 scripts/render-link-tables.py`
  and pass `--check`, and `tests/data/link/v1/surface.json` is regenerated by `tst_link_surface_manifest_regen`.
  Keep the PGXL discovery beacon at `0.5.2`.
- Build and test only through the lane script, with the Bash tool's `run_in_background: true`:
  `/Users/j.j.boyd/NereusSDR-crew/2026-11-15-recording-voice-keyer-dsr/run-test.sh tst_a tst_b`. It takes the
  Mac's load gate, builds with network denied, runs ctest, and releases the gate. You are notified when it exits;
  do not poll it and do not use foreground sleeps. Build single tests, never the whole suite. The trunk is
  `/Users/j.j.boyd/NereusSDR-crew/2026-11-15-recording-voice-keyer-dsr/trunk`; work only there. Do not delete or
  reconfigure its `build/`. Read `docs/development/fast-test-loop.md` once before your first test.
- Register a test with `nereus_add_test(tst_x ...)` in `tests/CMakeLists.txt` (defined at :202), before the
  `all_tests` block. Tests with timers use an injected clock or drive the logic directly: a test must not depend
  on machine load. Run a binary by hand only as
  `D=$(mktemp -d); HOME=$D CFFIXED_USER_HOME=$D NEREUS_TEST_HOME=$D QT_QPA_PLATFORM=offscreen NEREUS_WORKSPACE_PREVIEW=0 ./build/tests/tst_x`.
  Exit 97 means the home guard refused. Never put a test window on screen. Audio tests use `tests/fakes/` and
  never open a sound device.
- iOS: regenerate the project with `ios/scripts/generate-project.sh` after adding files. Package tests:
  `ios/scripts/swift-test.sh`. App tests: `xcodebuild -project ios/NereusSDR.xcodeproj -scheme NereusSDR
  -destination 'platform=iOS Simulator,name=iPhone 17' -derivedDataPath <dd> build-for-testing`, then
  `ios/scripts/sim-test.sh "iPhone 17" <dd> -only-testing:NereusSDRTests/<Suite>`. Run them in the background.
  `python3 scripts/verify-ios-provenance.py` and `python3 scripts/verify-ios-workspace-colours.py` pass.
- Before committing: `python3 scripts/verify-thetis-headers.py`, `python3 scripts/verify-inline-tag-preservation.py`
  (pre-commit runs them), `python3 scripts/verify-test-registration.py`.
- Windows and Linux code can't be built on this Mac. Guard it, keep its logic in platform-free files that the Mac
  tests cover, and say in your report which lines no compiler has seen. CI checks them when the controller pushes.
- Testing rule: while working, run only the tests for the code that changed, and after a fix only what failed.
  No stress runs, mutation runs or repeats of passing tests. The full suite runs once on GitHub when the PR is
  marked ready.

## Before Task 1 (controller)

- Create the lane at `/Users/j.j.boyd/NereusSDR-crew/2026-11-15-recording-voice-keyer-dsr/`: a `trunk` worktree
  on branch `claude/recording-voice-keyer-dsr` from `main` with this plan and the spec merged in, a primed
  `build/`, and `run-test.sh` copied from `/Users/j.j.boyd/NereusSDR-crew/2026-11-05-hot-keys/run-test.sh` with
  `R` and the lane name changed. Write its `how-to-build.md` the same way.
- **Flag for an earlier independent review, JJ decides:** Task 4 (the player writes into the mixer that feeds
  every speaker), Task 10 (what the radio transmits while keyed), Task 11 (the voice keyer keys the radio by
  itself), Task 22 (the core receive path). Ask JJ once, before Task 1, whether he wants any of them looked at
  as soon as it lands.

## Corrections to the spec, found while planning

| Spec | What the code says | What this plan does |
|---|---|---|
| 6.4, A4: the name carries the other station's call "when the log knows it" | No logbook exists (searched `src/core`, `src/models`). Roundtable does know who talked: each turn in a slice's room (`RoundtableModel::sliceRoomId`, `room()`) has `start`, `end`, `talkers[].callsign` (reported by the talker's own station) and `names[]` (given by listeners), as read in `RoundtableModel.cpp:1208-1215`. | The call comes from Roundtable (JJ, 2026-11-15): when a recording stops, the station heard longest in that slice's room over the recording's time, leaving out your own call (`SpotSourceHost::roundtableCallsign()`, compared by `RoundtableWire::baseCallsign`). A reported talker callsign beats a listener-given name. In a net, the one heard longest. Roundtable off or nobody named: no call. The file is named when the recording stops. The logbook case stays open until a log exists. Nothing guesses a call from spots or decodes. |
| 6.9: one capability, `recordingsVersion` 1 | The parts land one at a time, and a client must tell a Core with recordings but no voice keyer from one with both. | `recordingsVersion` rises per part: 1 recordings and playback (Task 5), 2 on the air (Task 10), 3 voice keyer (Task 11), 4 DSR (Task 21). A control needing a higher version than the Core offers is disabled with the V9 reason. |
| 6.1: the voice keyer records "from the client's mic stream" | The remote mic stream (`RemoteMicReceiver`, one `RemoteMicFeed` for the Core) is a transmit path. | Each client records the clip from its own mic, before processing, at 48 kHz mono, and uploads it with the import verb. One path for Record and Import WAV. A desktop running its own radio records from its own mic directly. |
| 6.1: "the transmitted audio (as sent, after processing)" | The only post-processing audio tap is the transmit monitor: `TxChannel::sip1OutputReady` into `AudioEngine::txMonitorBlockReady` (RadioModel.cpp:22285-22286), 48 kHz on P1 and 192 kHz on P2. | "Your voice" in a recording is that monitor signal, resampled to 24 kHz: exactly what MON plays. Whether it sounds right in AM and FM is a hardware check in Task 9; a bad result goes to JJ. |
| 5: Voice gates the flag buttons | `UnbuiltFeature::Voice` also gates the container PLAY and REC buttons, the status bar DVK label and the voice record/play meter item (section 11 keeps that hidden). | `UnbuiltFeature::Voice` stays and keeps gating those. The flag buttons, panels and hot keys built here stop using it and are gated by the Core's `recordingsVersion` instead. |
| V8: F1 to F8 send voice or CW by mode, and the old keys stay as alternates | One chord binds one action, and `tst_hot_key_bindings` requires no two actions to share a default key. | New actions `msg.1` to `msg.8` ("Message 1" to "Message 8") on F1 to F8 send the voice message in voice modes and the CW message in CW modes. `cw.msgN` keep Ctrl+F1 to F8; `voice.msg1` to `voice.msg8` take Ctrl+Shift+F1 to F8. Escape joins Ctrl+Shift+F10 on `voice.stop`. |

**Small calls settled here** (each follows something the code or the approved mockups already do):

- **Where files live:** on the chosen disk, `<mount>/NereusSDR/Recordings/` and
  `<mount>/NereusSDR/Spectrum Recorder/`; on the Core's own disk (the default), under
  `QStandardPaths::AppDataLocation`. The index is `recordings.json` in the recordings folder, written with
  `QSaveFile`.
- **Disks** come from `QStorageInfo::mountedVolumes()`: valid, ready, writable, not read-only, not a pseudo file
  system. **An SD card** is a volume whose `device()` starts with `/dev/mmcblk` (Linux MMC block devices, major
  179, in the kernel's `Documentation/admin-guide/devices.txt`).
- **Kept notice** stays 6 seconds, then fades; its Play still works from the right-click list afterward.
- **While a recording goes on the air** it stops coming out of the speaker; you hear it through MON exactly as
  you hear your own voice, as MON is set. Unkeying puts it back on the speaker at the new position.
- **"Show in Finder"** is that on macOS, "Show in Explorer" on Windows, "Show in folder" on Linux. It reveals
  and selects the file (`open -R`, `explorer /select,`, the folder on Linux).
- **Download** ("Save a copy here") uses offset chunks, the `dspAssets.export{id, offset}` pattern
  (DspAssetService.cpp:417-450) with `DspAssetStore::kTransferChunkBytes`.
- **DSR file format** is this plan's own: one ring file per disk, made of one-second blocks, each a fixed header
  (magic `NDSR`, format 1, UTC time in nanoseconds, receiver index, sample rate, centre Hz, transmit flag, sample
  count) followed by interleaved 16-bit I/Q scaled by 32767 and clipped. The index is rebuilt by scanning headers
  at start.

## What already exists

- **Receive audio:** `AudioEngine::rxBlockReady(int sliceId, const float*, int frames)` (AudioEngine.h:1269,
  .cpp:4301), interleaved stereo float at 48 kHz from `RxDspWorker` on the DSP thread. Per-slice taps:
  `SliceAudioTap { consume(const float*, int frames, int sampleRateHz) noexcept; skip(...) }` (AudioEngine.h:457-462),
  `setSliceAudioTap(int, SliceAudioTap*)` and `clearSliceAudioTap` (AudioEngine.cpp:4195, 4226), at most
  `kMaxSliceAudioTaps` = 8 (AudioEngine.h:646), fed by `feedSliceTaps` (.cpp:4450) after the MOX gate, before AF,
  pan and mix.
- **Per-listener audio:** `MasterMixer::OwnerOutput` and `OwnerMonitor` (src/core/audio/MasterMixer.h:335-372):
  one mix per owner, with slots outside 0..31 (the transmit monitor's) routed into one owner's speakers or
  headphones sum. `DaemonMediaController::acquireOwnerMix` / `ownerMixSlot()` (DaemonMediaController.h:437,
  826-828), `DaemonAudioSource::setOwnerMix` (DaemonAudioSource.h:107-121), `DaemonAudioSender` (Opus or L16).
- **Transmit input:** `TxWorkerThread::dispatchOneBlock` (src/core/TxWorkerThread.cpp:573-720), 64-frame blocks at
  48 kHz (`kBlockFrames`, TxWorkerThread.h:189), precedence TCI > remote mic > radio mic > VAX > PC (:625-630),
  TCI spliced into `m_in` at :655-675. `CompositeTxMicRouter` (src/core/audio/CompositeTxMicRouter.h).
  Transmit monitor: `TxChannel::sip1OutputReady` into `AudioEngine::txMonitorBlockReady`
  (RadioModel.cpp:22285-22286).
- **Processing switches:** `TxChannel::setTxEqRunning` :1813, `setTxLevelerOn` :1936, `setTxCfcRunning` :2012,
  `setTxCpdrOn` :2083, `setStageRunning(Stage::PhRot, bool)` :2345, `stageRunning` :787. User values in
  `TransmitModel`: `txEqEnabled` :2361, `txLevelerOn` :2370, `phaseRotatorEnabled` :2236, `cfcEnabled` :2242,
  `cpdrOn` :2260; `RadioModel` re-applies them in its restore lambda (RadioModel.cpp:8990-9014).
- **Keying:** `MoxController::setMox(bool, const KeyerIdentity&)` (MoxController.h:1373), `currentKeyer()` :628,
  `KeyerIdentity` :349-362, signals `moxChanged` :1596. `RadioModel::keyedBy()` (RadioModel.h:3239-3255),
  `setMoxFromButton` :6389. Remote verbs `tx.key`, `tx.unkey`, `tx.take` (SessionCommandDispatcher.cpp:1074-1095),
  `RemoteKeying::handle` (RemoteKeying.cpp:223). Who holds transmit: `TransmitHolder`
  (src/core/safety/TransmitHolder.h:155-249), mirrored as `txState`.
- **Receiver I/Q:** `RadioModel::forkIqToTaps(int receiverIndex, const QVector<float>&)` (RadioModel.cpp:40910),
  on the connection thread from `ReceiverManager::iqDataForReceiver` (RadioModel.cpp:23778-23781), interleaved
  float. `streamSampleRateHz(int)`, `streamCentreHz(int)` (RadioModel.h:8268-8273).
- **Capability pattern:** `bandStackVersion` (StationCapabilities.h:975, .cpp:664-665 and 754-761), set per peer
  in StationServer.cpp:15251, declared by the client at StationClient.cpp:1066, verb table entries
  `{"verb", {args}, "bandStackVersion", 1, minor}` (SessionCommandDispatcher.cpp:883-922), commit `d9f7da384` as
  the full model. Core setting written remotely: `setStationTciSettings` (SessionCommandDispatcher.cpp:972-979,
  1721; StationServer.cpp:6658), sent from CatNetworkSetupPages.cpp:545-556.
- **Local and remote access:** `BandStackAccess` with `State { Ready, NeedsNewerCore, NotConnected }`,
  `LocalBandStackAccess`, `RemoteBandStackAccess` (src/models/bandstack/), built in RadioModel.cpp:4055-4062 by
  role; `BandStackFacade` (src/core/session/BandStackFacade.h) publishes a JSON document with a revision.
- **Daemon assembly:** `DaemonApp::start` (src/core/daemon/DaemonApp.cpp:256); worker thread pattern
  `m_widebandThread` (DaemonApp.h:628, .cpp:423-426, joined at :656-657). `RadioModel::Role { Local, Remote }`
  (RadioModel.h:1399), `ownsLocalDsp()` :1682.
- **Logging:** src/core/LogCategories.h:20-35 and .cpp:18-72 (`LogManager::m_categories`),
  `tests/tst_log_category_list.cpp`.
- **Flag:** `VfoWidget::m_recBtn`, `m_playBtn` (VfoWidget.h:1287-1290), signals `recordToggled(bool)`,
  `playToggled(bool)` :911-913, test hooks :692-693; `buildFloatingButtons()` (VfoWidget.cpp:3749-3875, 20x20
  buttons, styles :3716-3740), `positionFloatingButtons()` (:3934-4000, order array :3970, step 22 :3991), the same
  order repeated at :999, :4012, :4085, :4316. Gate at :3857-3858, :3967.
- **Applets:** `CwxApplet` (src/gui/applets/CwxApplet.{h,cpp}) is the shape to copy. Registering one touches
  MainWindow.cpp:13858 (construct), :14244 (`m_appletsById`), :14308 (`registerApplet`),
  ContainerContentRegistry.cpp:304-312 (alias), :497 (descriptor), :297 (first launch). `DvkApplet`
  (src/gui/applets/DvkApplet.{h,cpp}, CMakeLists.txt:2345), construction commented out at MainWindow.cpp:14441.
- **TX row:** `TxKeyingSection::buildUI()` (src/gui/applets/TxKeyingSection.cpp:114-149), TUNE and MOX
  checkable, fixed height 22, stretch 1; accessors TxKeyingSection.h:125-126.
- **Waterfall history:** all in `SpectrumWidget`: `kMaxWaterfallHistoryRows` (SpectrumWidget.h:715),
  `kDefaultWaterfallHistoryMs` (:4011), state :3989-4026, `drawTimeScale` (.cpp:9201), `waterfallTimeScaleRect`
  :9782, `waterfallLiveButtonRect` :9792, `maxWaterfallHistoryOffsetRows` :9498, `historyRowIndexForAge` :9503,
  `pausedTimeLabelForAge` :9512, `appendHistoryRow` :9591, `rebuildWaterfallViewport` :9666, `setWaterfallLive`
  :9759; mouse at :15145 (LIVE), :15153-15162, :15250-15270, :16019.
- **Setup:** `SetupDialog::registerPage(parent, label, SetupScope, factory, requiresTransmit)` (SetupDialog.cpp:689),
  Cores category :1856 ("Your Cores" :1857, "Audio with the Core" :1870), Hardware at :1877.
- **Hot keys:** src/core/hotkeys/HotKeyActionList.cpp:106-110 (voice rows), :101 (`cw.stop`);
  `MainWindowHotKeyTargets::unavailableReason` (.cpp:328-338), `voiceNotBuiltReason` :94,
  `transmitSliceInCw()` :560-573; `HotKeyDispatcher::execute` (HotKeyDispatcher.cpp:360-382) with the fallback
  comment at :377-378.
- **Core on this computer:** `StationClient::coreServesTciOnThisComputer()` (StationClient.cpp:9377-9409), test
  seam `setCoreOnThisComputerForTest` (StationClient.h:1327).
- **iOS:** `ios/NereusKit/` (NereusLink `LinkCodec.swift:60`, `LinkMessage.swift`; NereusMirror
  `MirrorStore.swift:51, 272, 317`; `capabilityName` convention, e.g. `BandStack.swift:14`). App: flag
  `NereusApp/Band/VfoFlagView.swift:142`, `Band/FlagSideButtons.swift:44-52`, placed from
  `BandGestureLayer.swift:537`; PTT `Main/PttButton.swift:43`, `MainScreen.swift:437`; Tools
  `Tools/StationToolList.swift:33, 87, 120-131`, `App/ToolsTab.swift`, `Tools/CwxPage.swift`,
  `Tools/CwxModel.swift:93-112`, `Tools/ToolPageParts.swift:13` (Reason); Setup `Setup/SetupTree.swift:24,
  119-165`; iPad `iPad/IPadLayout.swift`, `iPad/AppletColumn.swift:23, 168-175`, `AppletKind` and `AppletOrder`
  (`Roundtable/RoundtableApplet.swift:19, 30`). Core tool list: src/core/session/StationCatalog.cpp:900, 940-941.
  Shot tests: `ios/NereusApp/Tests/*ShotTests.swift` with `NEREUS_BAND_SHOTS`.
- **Desktop captures:** `tests/visual/capture.py` (profiles :63-69), scenes in `tests/visual/PreviewScenes.cpp`
  (`fixedScenes()` :10264, flag example :10505-10521, Setup pages automatic as `dialog-setup-*` :11835).
- **Unbuilt gating tests:** `tests/tst_unbuilt_features.cpp:685-718` (`map[F::Voice]`), :892, :981, :1042,
  :1202-1278.
- **Resampler:** `third_party/r8brain/`. **LAME on this Mac:** `/opt/homebrew/Cellar/lame/3.100`, `lame.h`
  declares `hip_decode_init` :1051, `hip_decode` :1078; the dylib exports them; no pkg-config file.

## File Structure

**Create**

| Path | Responsibility |
|---|---|
| `src/core/recordings/AudioFileWriter.{h,cpp}` | WAV and MP3 (LAME) writers, 16-bit, mono or stereo, one interface |
| `src/core/recordings/AudioFileReader.{h,cpp}` | WAV and MP3 (LAME `hip_decode`, or minimp3 per Task 1) readers to float |
| `src/core/recordings/SpscFloatRing.h` | Lock-free single-producer ring the taps write into |
| `src/core/recordings/RecordingNames.{h,cpp}` | File names from spec 6.4, renames, collisions |
| `src/core/recordings/RecordingStore.{h,cpp}` | Folder, `recordings.json` index, kinds, rename, delete, disk use |
| `src/core/recordings/RecordingDisks.{h,cpp}` | Volume list, SD card test, free space |
| `src/core/recordings/RecordingSettings.{h,cpp}` | The Core keys of spec 6.5 with defaults |
| `src/core/recordings/ContactRecorder.{h,cpp}` | One per slice: receive tap, monitor tap, resample, together or apart, start and stop rules |
| `src/core/recordings/QuietRule.h` | V4 as pure logic with an injected clock |
| `src/core/recordings/RecordingPlayer.{h,cpp}` | One per listener: decode, position, seek, feed that listener's mix |
| `src/core/recordings/OnAirSource.{h,cpp}` | Transmit input from a playback or a voice message, and the five switches |
| `src/core/recordings/VoiceKeyer.{h,cpp}` | M1 to M8 store and sender |
| `src/core/recordings/RecordingService.{h,cpp}` | Owns the worker thread and all of the above; built by the Local radio owner |
| `src/core/recordings/dsr/DsrRingFile.{h,cpp}` | Ring file, block format, index, wrap |
| `src/core/recordings/dsr/DsrRecorder.{h,cpp}` | I/Q tap, 16-bit packing, transmit marks, size limit, disk rules |
| `src/core/recordings/dsr/DsrReplay.{h,cpp}` | Past I/Q to a slice, past spectrum and rows, stretch copy and sound-only render |
| `src/core/session/RecordingsFacade.{h,cpp}` | The mirrored `recordings` object (state, index, settings, voice keyer, DSR) |
| `src/models/recordings/RecordingsAccess.h`, `LocalRecordingsAccess.{h,cpp}`, `RemoteRecordingsAccess.{h,cpp}` | What windows talk to, local or over the link |
| `src/gui/recordings/FlagRecordPlay.{h,cpp}` | Flag Record and Play behavior, Kept notice, right-click list |
| `src/gui/recordings/RecordingsPanel.{h,cpp}` | Right-column applet |
| `src/gui/recordings/RecordingsWindow.{h,cpp}` | All recordings window |
| `src/gui/recordings/RevealFile.{h,cpp}` | Show in Finder / Explorer / folder |
| `src/gui/setup/RecordingsSetupPage.{h,cpp}`, `DsrSetupPage.{h,cpp}` | Setup > Cores pages |
| `src/gui/applets/VoiceKeyerApplet.{h,cpp}` | Voice keyer panel (replaces `DvkApplet`) |
| `src/gui/recordings/VoiceButton.{h,cpp}`, `LocalMicClipRecorder.{h,cpp}` | VOICE in the TX row and its box; recording a clip from this computer's mic |
| `src/gui/recordings/DsrHoursMap.{h,cpp}`, `DsrSaveBox.{h,cpp}` | Hours map and SAVE bracket and box |
| `ios/NereusKit/Sources/NereusModels/Recordings*.swift` | Kit model, verbs, mirror decode |
| `ios/NereusApp/Recordings/*.swift` | Flag buttons, playing bar, pages, voice keyer, iPad panel, DSR |
| `tests/tst_recording_*.cpp`, `tests/tst_voice_keyer*.cpp`, `tests/tst_dsr_*.cpp`, `ios/NereusKit/Tests/*Recordings*`, `ios/NereusApp/Tests/Recording*Tests.swift` | Tests, named per task |
| `docs/attribution/LAME-PROVENANCE.md`, notice `.txt` under `packaging/third-party-licenses/` | LAME (and minimp3 if Task 1 needs it) |

**Modify**

| Path | Change |
|---|---|
| `CMakeLists.txt`, `tests/CMakeLists.txt` | Sources, LAME find and link, tests |
| `src/core/LogCategories.{h,cpp}`, `tests/tst_log_category_list.cpp` | `lcRecordings` (`"nereus.recordings"`) |
| `src/core/audio/MasterMixer.{h,cpp}` | A playback slot routed to one owner (Task 4) |
| `src/core/TxWorkerThread.{h,cpp}` | On-air source at the top of the precedence (Task 10) |
| `src/models/RadioModel.{h,cpp}` | Build the service in Local role, access by role, DSR I/Q tap, rewind feed |
| `src/core/daemon/DaemonApp.{h,cpp}` | Service lifetime in `nereusd` |
| `src/core/session/*` (StationCapabilities, SessionCommandDispatcher, StationServer, StationClient, IStationLink, MirrorPolicy, MirrorSchema, StationCatalog), `tests/LinkSurface.cpp`, `tests/data/link/v1/*`, `docs/architecture/2026-09-23-station-link-v1.md` | `recordingsVersion` and the verbs |
| `src/gui/widgets/VfoWidget.{h,cpp}` | Flag order and the two buttons wired |
| `src/gui/MainWindow.{h,cpp}`, `src/gui/containers/ContainerContentRegistry.cpp`, `src/gui/SetupDialog.cpp` | Applets, pages, menus |
| `src/gui/applets/TxKeyingSection.{h,cpp}` | VOICE |
| `src/gui/SpectrumWidget.{h,cpp}` | DSR rewind, PAST, past rows, hours map, SAVE |
| `src/core/hotkeys/HotKeyActionList.cpp`, `src/gui/hotkeys/MainWindowHotKeyTargets.cpp`, `src/gui/hotkeys/HotKeyDispatcher.cpp` | Voice actions |
| `tests/tst_unbuilt_features.cpp`, `tests/tst_hot_key_bindings.cpp`, `tests/visual/PreviewScenes.cpp` | Gating moves, new defaults, scenes |
| `packaging/third-party-licenses/README.md`, `.github/workflows/ci.yml`, `.github/workflows/release.yml`, `.github/docker/Dockerfile` | LAME on every platform |
| `ios/NereusApp/...` (flag side buttons, PTT row, Tools list, Setup tree, iPad column) | Phone and iPad surfaces |

---

# Part 1: Recordings on the Core, desktop first

## Task 1: Audio files, LAME and MP3 decoding

**Requirements:** spec 6.1 (Contact recorder writes WAV or MP3), V2, V10, section 10, error "MP3 encoder fails".

**Files:**
- Create: `src/core/recordings/AudioFileWriter.{h,cpp}`, `AudioFileReader.{h,cpp}`, `docs/attribution/LAME-PROVENANCE.md`, notice under `packaging/third-party-licenses/`
- Modify: `CMakeLists.txt` (find `lame/lame.h` and `mp3lame` with `find_path` and `find_library`; define `NEREUS_HAVE_LAME` on the core library), `.github/workflows/ci.yml`, `.github/workflows/release.yml`, `.github/docker/Dockerfile`, `packaging/third-party-licenses/README.md`, `src/core/LogCategories.{h,cpp}`, `tests/tst_log_category_list.cpp`, `tests/CMakeLists.txt`
- Test: `tests/tst_recording_audio_files.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  ```cpp
  namespace NereusSDR {
  Q_DECLARE_LOGGING_CATEGORY(lcRecordings)            // "nereus.recordings", in LogCategories.h
  enum class AudioFileFormat { Wav, Mp3 };
  struct AudioFileSpec { AudioFileFormat format; int sampleRateHz; int channels; int mp3KbitPerSec; };
  class AudioFileWriter {                              // not thread-safe; used on the recordings worker only
  public:
      static std::unique_ptr<AudioFileWriter> open(const QString& path, const AudioFileSpec& spec, QString* error);
      virtual ~AudioFileWriter() = default;
      virtual bool write(const float* interleaved, int frames) = 0;   // false on a disk error
      virtual bool finish() = 0;                                      // fixes WAV header, flushes LAME
      virtual qint64 bytesWritten() const = 0;
      static bool mp3Available();                                     // false without NEREUS_HAVE_LAME
  };
  class AudioFileReader {
  public:
      static std::unique_ptr<AudioFileReader> open(const QString& path, QString* error);
      virtual ~AudioFileReader() = default;
      virtual int sampleRateHz() const = 0; virtual int channels() const = 0;
      virtual qint64 lengthFrames() const = 0;
      virtual bool seekFrame(qint64 frame) = 0;
      virtual int read(float* interleaved, int maxFrames) = 0;        // 0 at the end
  };
  }
  ```

**Acceptance:**
- A 1 kHz sine at -6 dBFS, 24 kHz mono, 10 s, written as WAV and read back: 240000 frames, peak within 0.01 of
  0.5, header sample rate 24000, 16-bit, 1 channel. Stereo with a sine left and silence right keeps the sides.
- The same as MP3 at 64 kbit/s mono and 128 kbit/s stereo: file size within 5 percent of 64000/8*10 and
  128000/8*10 bytes; decoded length within 1152 frames of 240000 frames; decoded tone peak frequency 1 kHz within 10 Hz.
- A WAV at 48 kHz 16-bit mono written by another tool (a fixture in `tests/data/recordings/`) reads back.
- A writer whose disk refuses the write returns false from `write` and leaves a readable file up to that point.
- `mp3Available()` is false in a build without LAME, and `open` with `Mp3` then fails with a reason, so the
  caller can fall back to WAV.
- **Decoder check (V10), done first and written in the report:** on macOS (Homebrew), the Linux CI image, the Pi
  image (Debian arm64, the Core's packaging) and Windows, whether the LAME the build gets exports `hip_decode_init`
  and `hip_decode` (`nm -D` / `nm -gU` / `dumpbin /exports`, run in CI where this Mac can't). If every platform has
  them, the reader uses LAME's decoder. If any lacks them, vendor minimp3 (public domain / CC0) under
  `third_party/minimp3/` with its licence rows, and use it on every platform for one decode path.
- `lcRecordings` is in `LogManager::m_categories` and `tst_log_category_list` passes.
- `python3 scripts/check-third-party-licenses.py` passes with LAME (and minimp3 if added).

**Verification:** Ordinary feature plus a packaging boundary. Unit layer:
`run-test.sh tst_recording_audio_files tst_log_category_list tst_core_has_no_gui_includes`. Integration: the
controller's push runs the Windows, Linux and Pi builds that prove LAME links there; pending until CI shows it.

**Execution note (advisory):** opus. Touches CI and packaging, no network at runtime, no device. No
prerequisites. Not parallel with any task that edits `CMakeLists.txt` dependency blocks.

- [ ] **Step 1:** Run the decoder check on this Mac and add the CI lines that print it on the other platforms;
  decide LAME decoder or minimp3 by the rule above.
- [ ] **Step 2:** Add LAME to the build on every platform the way each CI leg already gets its libraries (read
  `ci.yml` and `release.yml`; on Windows, a DLL shipped beside the app like `third_party/fftw3`), the notices and
  provenance. Write the writer, reader and tests. Commit.

## Task 2: Recording store, names, disks and settings

**Requirements:** spec 6.4 (kinds, names, rename, delete), 6.5 (keys, defaults, where they are kept), 6.7 (disk
rules shared with the DSR), section 7 (disk full).

**Files:**
- Create: `src/core/recordings/RecordingNames.{h,cpp}`, `RecordingStore.{h,cpp}`, `RecordingDisks.{h,cpp}`, `RecordingSettings.{h,cpp}`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_recording_store.cpp`

**Interfaces:**
- Consumes: `AudioFileFormat` (Task 1).
- Produces:
  ```cpp
  namespace NereusSDR {
  enum class RecordingKind { Contact, WholePan, Sound };
  struct RecordingEntry {
      QString id;                 // stable, a UUID without braces
      RecordingKind kind; int sliceId;          // -1 for WholePan
      QDateTime startUtc; qint64 lengthMs;
      qint64 frequencyHz; QString mode; QString call;  // call empty when unknown
      QString name;               // shown name, also the file's base name
      QString fileName;           // in the store folder
      qint64 sizeBytes;
  };
  QString recordingBaseName(const QDateTime& startUtc, qint64 frequencyHz, const QString& mode, const QString& call);
      // "2026-11-15 1432Z 7.236.400 LSB K4ABC"; no trailing space when call is empty
  class RecordingStore : public QObject {
      Q_OBJECT
  public:
      explicit RecordingStore(QObject* parent = nullptr);
      bool openFolder(const QString& folder, QString* error);   // loads recordings.json, drops missing files
      QString folder() const;
      QList<RecordingEntry> entries() const;                     // newest first
      std::optional<RecordingEntry> find(const QString& id) const;
      QString pathForNew(const QString& baseName, const QString& suffix) const;  // adds " (2)" on collision
      QString add(const RecordingEntry& e);                      // returns id
      bool rename(const QString& id, const QString& newName, QString* error);
      bool remove(const QStringList& ids, QString* error);
      qint64 usedBytes() const; qint64 freeBytes() const;
  signals:
      void changed();
  };
  struct RecordingVolume { QString rootPath; QString displayName; qint64 freeBytes; qint64 totalBytes; bool isSdCard; bool isCoreOwnDisk; };
  class RecordingDisks { public: static QList<RecordingVolume> volumes(); static bool isSdCardDevice(const QByteArray& device); };
  struct RecordingSettings {   // Core AppSettings keys of spec 6.5 and 6.7
      bool keepMyVoice = true; bool sidesApart = false;
      bool autoStartOnTransmit = false; bool autoStopWhenQuiet = true; int quietMinutes = 2;   // 1..60
      AudioFileFormat format = AudioFileFormat::Mp3; QString disk;                           // empty = Core's own disk
      bool onAirUseTxEq = false, onAirUseCompressor = false, onAirUseLeveler = false,
           onAirUseCfc = false, onAirUsePhaseRotator = false;
      bool dsrEnabled = true; QString dsrDisk; int dsrSizeLimitGb = 10;
      static RecordingSettings load(); void save() const;
      QString recordingsFolder() const;   // per the plan's "Where files live"
      QString dsrFolder() const;
  };
  }
  ```

**Acceptance:**
- `recordingBaseName(2026-11-15T14:32:10Z, 7236400, "LSB", "K4ABC")` is `2026-11-15 1432Z 7.236.400 LSB K4ABC`;
  with an empty call `2026-11-15 1432Z 7.236.400 LSB`; 14074000 Hz gives `14.074.000`; 146520000 gives
  `146.520.000`.
- Two recordings in the same minute get `... LSB.mp3` and `... LSB (2).mp3`.
- `rename` changes the shown name and the file on disk; renaming to a name with `/`, `\`, `:` or a leading dot
  refuses with a reason; renaming onto an existing name refuses.
- `remove` of three ids deletes three files and their rows; a missing file still removes its row.
- After `add` and a fresh `openFolder`, the same entries come back; a row whose file vanished is dropped with a
  `qCWarning(lcRecordings)`; a file with no row is not added and not deleted.
- The store never deletes anything by itself.
- `RecordingSettings::load()` on fresh settings gives every default above; `save()` writes the PascalCase keys
  `RecordingsKeepMyVoice`, `RecordingsSidesApart`, `RecordingsAutoStartOnTransmit`, `RecordingsAutoStopWhenQuiet`,
  `RecordingsQuietMinutes`, `RecordingsFormat` (`Mp3` / `Wav`), `RecordingsDisk`, `RecordingsOnAirUseTxEq`,
  `RecordingsOnAirUseCompressor`, `RecordingsOnAirUseLeveler`, `RecordingsOnAirUseCfc`,
  `RecordingsOnAirUsePhaseRotator`, `DsrEnabled`, `DsrDisk`, `DsrSizeLimitGb`, booleans as `"True"` / `"False"`.
  `quietMinutes` 0 loads as 1, 99 as 60.
- `isSdCardDevice("/dev/mmcblk0p2")` true; `"/dev/sda1"`, `"/dev/nvme0n1p1"`, `"/dev/disk3s1"` false.

**Verification:** Ordinary feature, unit layer. `run-test.sh tst_recording_store tst_core_has_no_gui_includes`.

**Execution note (advisory):** opus. No network or device. Prerequisite Task 1. Could run in a worktree beside
Task 1 if the controller merges `CMakeLists.txt` by hand; serial is simpler.

- [ ] **Step 1:** Write the four units with the test. Commit.

## Task 3: Contact recorder and the recording service

**Requirements:** spec 6.1 (Contact recorder, threads), 6.2 (all five steps), V1, V2, V4, section 7 (disk full,
MP3 fails, link drops), A2, A3, A17 for the recorder.

**Files:**
- Create: `src/core/recordings/SpscFloatRing.h`, `QuietRule.h`, `ContactRecorder.{h,cpp}`, `RecordingService.{h,cpp}`
- Modify: `src/models/RadioModel.{h,cpp}` (build the service in Role::Local, pass it the audio engine taps and the
  monitor signal, `recordingService()` accessor), `src/core/daemon/DaemonApp.cpp` (stop it before the radio in
  `stop()`), `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_recording_contact.cpp`, `tests/tst_recording_quiet_rule.cpp`

**Interfaces:**
- Consumes: `AudioFileWriter` (Task 1); `RecordingStore`, `RecordingSettings`, `recordingBaseName` (Task 2);
  `AudioEngine::setSliceAudioTap` / `clearSliceAudioTap`, `SliceAudioTap`; `AudioEngine::txMonitorBlockReady`;
  `MoxController::moxChanged`; the slice's squelch state and level and its noise floor from `SliceModel` and the
  meter path; `RadioModel::roundtableModel()` (`sliceRoomId`, `room`), `SpotSourceHost::roundtableCallsign()`,
  `RoundtableWire::baseCallsign` (src/core/roundtable/RoundtableWire.h:177).
- Produces:
  ```cpp
  namespace NereusSDR {
  enum class RecordState { Idle, Armed, Recording };
  enum class KeptReason { StoppedHere, StoppedOnDevice, StoppedQuiet, DiskFull, KeptAsWav };
  struct RecordingSliceState { int sliceId; RecordState state; QDateTime startUtc; QString startedBy; qint64 elapsedMs; bool hearingMyVoice; };
  struct KeptEvent { int sliceId; QString id; QString name; qint64 lengthMs; KeptReason reason; QString deviceName; };
  class QuietRule {           // V4, pure, injected clock in ms
  public:
      void setMinutes(int m);
      void onTransmit(qint64 nowMs);
      void onLevel(qint64 nowMs, bool squelchOn, bool aboveSquelch, double levelDb, double noiseFloorDb); // +6 dB rule when squelch off
      bool quiet(qint64 nowMs) const;
  };
  class RecordingService : public QObject {   // lives on the main thread; owns the worker thread
      Q_OBJECT
  public:
      RecordingService(RadioModel* radio, QObject* parent = nullptr);
      ~RecordingService() override;            // joins the worker
      RecordingStore* store() const;
      RecordingSettings settings() const; void setSettings(const RecordingSettings& s);
      void startRecording(int sliceId, const QString& byDeviceName);
      void stopRecording(int sliceId, const QString& byDeviceName);
      QList<RecordingSliceState> sliceStates() const;
  signals:
      void sliceStateChanged(int sliceId);
      void kept(const NereusSDR::KeptEvent& e);
  };
  }
  ```

**Acceptance:**
- Driving the recorder with fake receive blocks (48 kHz stereo) and fake monitor blocks (48 kHz and, separately,
  192 kHz) for 10 s: the file is 24 kHz; together, mono with both; apart, receive left and monitor right (A2).
  `keepMyVoice` off leaves the monitor out.
- Monitor blocks only count while MOX is on; `hearingMyVoice` is true only then.
- Two slices record at once into two files; a second start on a recording slice does nothing (V1).
- Start by itself: armed slice state is `Armed`; MOX on the transmit slice starts a recording there; Record while
  armed starts at once (spec 6.2 step 4).
- QuietRule (A3, V4): with squelch on, nothing above squelch and no transmit for 2 minutes is quiet at
  120000 ms and not at 119999 ms; a transmit at 60000 ms moves it to 180000 ms; with squelch off, a level 6.1 dB
  above the floor resets it and 5.9 dB does not. A quiet stop emits `kept` with `StoppedQuiet` and, when start by
  itself is on, returns the slice to `Armed`.
- Stop from another device: `kept` carries `StoppedOnDevice` and that device's name.
- A writer that fails with a full disk stops the recording, keeps what was written, and emits `DiskFull`.
- With MP3 chosen and `mp3Available()` false, the recording is WAV and `kept` says `KeptAsWav`.
- The tap's `consume` only copies into an `SpscFloatRing` and returns; a test asserts it takes no lock and does
  no allocation after setup (an instrumented allocator hook or a counted fake), and that a stalled worker drops
  blocks with a counted overflow rather than blocking (A17 for the recorder).
- Names come from Task 2 with the slice's frequency and mode at the start. The recorder writes to a working name
  and renames the file when it stops, with the call chosen by
  `QString callForRecording(const QJsonObject& room, qint64 startMs, qint64 endMs, const QString& ownCall)`
  (a pure function in `ContactRecorder.h`, tested on its own):
  - A room whose turns overlapping the recording are K4ABC as a reported talker for 90 s and W1AW as a
    listener-given name for 200 s gives `K4ABC`.
  - Two reported talkers, K4ABC for 40 s and N0XYZ for 120 s, give `N0XYZ`.
  - A turn by your own call (also as `KG4VCF/P` against `KG4VCF`) never counts.
  - Turns wholly outside the recording's time don't count; a turn that overlaps counts only for the overlap.
  - An empty room, no room for the slice, or Roundtable off gives an empty call.
- `RecordingService` exists only in Role::Local; a Role::Remote `RadioModel` has none.

**Verification:** Ordinary feature with an audio-thread boundary: the non-blocking assertion is the invariant.
Unit layer. `run-test.sh tst_recording_contact tst_recording_quiet_rule tst_core_has_no_gui_includes`.

**Execution note (advisory):** opus. Touches the audio engine taps (read-only use). No network. Prerequisite
Task 2.

- [ ] **Step 1:** Write `QuietRule` and its test first (its contract is exact).
- [ ] **Step 2:** Write the ring, recorder and service, wire them in `RadioModel` for Role::Local and in
  `DaemonApp::stop`, with the recorder test. Commit.

## Task 4: The player

**Requirements:** spec 6.1 (Player), 6.3 (playback goes to the pressing device only), V6, A5 for the Core.

**Files:**
- Create: `src/core/recordings/RecordingPlayer.{h,cpp}`
- Modify: `src/core/audio/MasterMixer.{h,cpp}` (a playback slot per owner, outside 0..31, routed only into that
  owner's speakers sum; for the local window, into the local speakers sum), `src/core/recordings/RecordingService.{h,cpp}`,
  `src/core/session/media/DaemonMediaController.{h,cpp}` (give each session's owner mix its playback slot),
  `tests/CMakeLists.txt`
- Test: `tests/tst_recording_player.cpp`

**Interfaces:**
- Consumes: `AudioFileReader` (Task 1), `RecordingStore` (Task 2), `RecordingService` (Task 3),
  `MasterMixer::OwnerOutput` and its owner slots, `DaemonMediaController::ownerMixSlot()`.
- Produces:
  ```cpp
  namespace NereusSDR {
  using ListenerId = QString;           // "local" for the window that runs its own radio; else the session's device id
  struct PlaybackState { ListenerId listener; QString recordingId; qint64 positionMs; qint64 lengthMs; bool playing; bool onAir; };
  // added to RecordingService:
  //   void play(const ListenerId& who, const QString& recordingId);
  //   void stopPlayback(const ListenerId& who);
  //   void seek(const ListenerId& who, qint64 positionMs);
  //   std::optional<PlaybackState> playback(const ListenerId& who) const;
  //   int pullPlayback(const ListenerId& who, float* mono48k, int frames);  // audio-thread safe, used by Task 10
  // signals: void playbackChanged(const NereusSDR::ListenerId& who);
  }
  ```

**Acceptance:**
- Two owners A and B: A plays a recording; A's speakers sum carries it; B's and the local sum do not (A5).
- Playback is resampled to the mixer's 48 kHz; a 24 kHz 1 kHz tone arrives as a 1 kHz tone.
- `seek` to 5000 ms on a 10 s file continues from 5 s; position reports advance in real time from the mixer's
  consumption, not a wall clock.
- At the end, `playing` goes false and `playbackChanged` fires once.
- A WholePan entry refuses `play` with "Open it on the waterfall." (spec 6.3; its replay is Task 25).
- The mixer's playback slot never holds up the local drain, its barrier or its ramps: the existing
  `tst_master_mixer*` tests pass unchanged.
- The decode runs on the recordings worker into a ring; the mixer side only reads the ring.

**Verification:** High risk by data path (every speaker's mix): regression first. Before changing
`MasterMixer`, run its existing tests to set the baseline; then `run-test.sh tst_recording_player` plus every
`tst_master_mixer*` and `tst_daemon_media*` test the lane has (list them with `ctest -N | grep -E
'master_mixer|daemon_media'` and pass them all). Flagged for an earlier review (Before Task 1).

**Execution note (advisory):** opus. Audio output path; no network change. Prerequisite Task 3.

- [ ] **Step 1:** Baseline the mixer tests. Add the playback slot and its owner route with a mixer test.
- [ ] **Step 2:** Write the player in the service and its test. Commit.

## Task 5: Station protocol, version 1, and the access

**Requirements:** spec 6.9 (rows for recording state, start and stop, list, play, rename and delete, copy
download, settings), V9, section 7 (link drops), A1, A4 and A15 at the protocol layer.

**Files:**
- Create: `src/core/session/RecordingsFacade.{h,cpp}`, `src/models/recordings/RecordingsAccess.h`, `LocalRecordingsAccess.{h,cpp}`, `RemoteRecordingsAccess.{h,cpp}`
- Modify: `src/core/session/StationCapabilities.{h,cpp}`, `SessionCommandDispatcher.cpp`, `StationServer.{h,cpp}`,
  `StationClient.{h,cpp}` (split a general `coreIsOnThisComputer()` out of `coreServesTciOnThisComputer()`, keep
  the old one calling it), `IStationLink.h`, `MirrorPolicy.cpp`, `MirrorSchema.cpp`, `src/models/RadioModel.{h,cpp}`
  (`recordings()` access by role, as band stack at RadioModel.cpp:4055-4062), `tests/LinkSurface.cpp`,
  `tests/data/link/v1/manifest.json`, `tests/data/link/v1/sessions/verbs-recordings.json`,
  `tests/data/link/v1/surface.json`, `docs/architecture/2026-09-23-station-link-v1.md`, `tests/CMakeLists.txt`
- Test: `tests/tst_recordings_link.cpp`, `tests/tst_recordings_access.cpp`

**Interfaces:**
- Consumes: Tasks 2 to 4.
- Produces:
  - Capability `int recordingsVersion = 0;` in `StationCapabilities`, encoded only when above 0, offered per peer
    from the hello's declared feature `"recordings"`. This task sets it to 1.
  - The mirrored object `recordings` (class `RecordingsFacade`, `Q_PROPERTY(QString json)` and `revision`, the
    `BandStackFacade` shape) with the document:
    ```json
    { "version": 1,
      "slices": [ { "slice": 0, "state": "idle|armed|recording", "startUtc": "...", "startedBy": "Desktop", "elapsedMs": 0, "myVoice": false } ],
      "kept":   { "seq": 7, "slice": 0, "id": "...", "name": "...", "lengthMs": 0, "reason": "here|device|quiet|diskFull|wav", "device": "iPhone" },
      "index":  { "revision": 12, "count": 40, "usedBytes": 0, "freeBytes": 0, "folder": "/path" },
      "settings": { "keepMyVoice": true, "sidesApart": false, "autoStart": false, "autoStop": true, "quietMinutes": 2,
                    "format": "mp3", "disk": "", "disks": [ { "root": "...", "name": "...", "free": 0, "total": 0, "sd": false, "own": true } ] } }
    ```
  - Verbs, each `"requires": "recordingsVersion" 1`: `recordings.start{slice}`, `recordings.stop{slice}`,
    `recordings.list{offset, count}` (returns entries, newest first), `recordings.play{id}`, `recordings.stopPlay{}`,
    `recordings.seek{positionMs}`, `recordings.rename{id, name}`, `recordings.delete{ids}`,
    `recordings.export{id, offset}` (base64 `data`, `offset`, `size`), `recordings.setSettings{optional fields}`.
    Per-listener playback state travels back on the session as `recordings.playback{id, positionMs, lengthMs,
    playing, onAir}`, never mirrored to other sessions.
  - ```cpp
    class RecordingsAccess : public QObject {   // NEREUS_CORE_EXPORT
        Q_OBJECT
    public:
        enum class State { Ready, NeedsNewerCore, NotConnected };
        virtual State state() const = 0;
        virtual int coreVersion() const = 0;        // the Core's recordingsVersion, 0 if none
        virtual QString unavailableReason(int needsVersion = 1) const = 0;
        virtual QJsonObject document() const = 0;
        virtual void start(int sliceId) = 0; virtual void stop(int sliceId) = 0;
        virtual void requestList(int offset, int count) = 0;
        virtual QList<RecordingEntry> listed() const = 0;
        virtual void play(const QString& id) = 0; virtual void stopPlay() = 0; virtual void seek(qint64 ms) = 0;
        virtual std::optional<PlaybackState> playback() const = 0;
        virtual void rename(const QString& id, const QString& name) = 0;
        virtual void remove(const QStringList& ids) = 0;
        virtual void download(const QString& id, const QString& toPath) = 0;   // progress and done signals
        virtual void setSettings(const QJsonObject& changed) = 0;
        virtual bool coreIsOnThisComputer() const = 0;
        static QString needsNewerCoreReason() { return QStringLiteral("This Core needs an update for recordings."); }
        static QString notConnectedReason() { return QStringLiteral("Connect to a radio to use recordings."); }
    signals:
        void documentChanged(); void stateChanged(); void listChanged(); void playbackChanged();
        void kept(const QJsonObject& event); void downloadProgress(const QString& id, qint64 done, qint64 total);
        void downloadFinished(const QString& id, bool ok, const QString& error); void refused(const QString& reason);
    };
    ```

**Acceptance:**
- Station loopback (the `LinkSurface` harness): a client starts slice A; the facade on a second client shows
  `recording` within one mirror update (A1 at the protocol layer); stop from the second client produces a `kept`
  with `reason: "device"` and its name on both.
- A client that keeps its session open while the link drops and returns sees the slice still `recording` with the
  elapsed time continuing (section 7).
- Rename and delete from one client change `index.revision` and the next `list` on the other (A4).
- Two sessions playing different recordings each get only their own `recordings.playback`.
- `export` of a 3 MB file in chunks reassembles byte-identical.
- A Core without the capability: `RemoteRecordingsAccess::state()` is `NeedsNewerCore` and every verb is refused
  locally without sending; the reason is the V9 string (A15 at the access layer). With the capability present but
  `coreVersion()` 1, `unavailableReason(2)` is the same V9 string.
- `LocalRecordingsAccess` in a window that runs its own radio does the same operations directly on the service.
- `render-link-tables.py --check` passes, the manifest lists every verb, and `tst_link_surface_manifest_regen`
  regenerates `surface.json` cleanly.

**Verification:** Networking and a protocol boundary: integration layer (station loopback) plus unit.
`run-test.sh tst_recordings_link tst_recordings_access tst_link_surface tst_link_surface_manifest_regen
tst_core_has_no_gui_includes` (check the exact link test names with `ctest -N | grep link` first and include any
the change touches).

**Execution note (advisory):** opus. Touches networking (new verbs and a download path): a candidate for the
controller to read closely, though not flagged. Prerequisite Task 4.

- [ ] **Step 1:** Add the capability, facade, verbs and manifest with the loopback test.
- [ ] **Step 2:** Add the access interface, both implementations and the role switch in `RadioModel`, with its
  test. Regenerate the link tables and surface. Commit.

## Task 6: Desktop flag Record and Play

**Requirements:** spec section 3 (Flag order, Desktop flag look, Older recordings right-click, Recording
together), 6.2, 6.3, V1, V9, A1, A5 and A16 for the desktop. Mockups:
`2026-11-15-recording-voice-keyer-dsr-design/desktop-flag/` and `recording-together/` (the desktop half).

**Files:**
- Create: `src/gui/recordings/FlagRecordPlay.{h,cpp}`, `src/gui/recordings/RevealFile.{h,cpp}`
- Modify: `src/gui/widgets/VfoWidget.{h,cpp}` (order ×, lock, Record, Play, ··· at every place the order array
  appears: :999, :3970, :4012, :4085, :4316; record and play stop using `UnbuiltFeature::Voice`; disabled with the
  access's reason instead), `src/gui/MainWindow.cpp` (one `FlagRecordPlay` per flag, fed by `RadioModel::recordings()`),
  `tests/tst_unbuilt_features.cpp` (move the slice flag record and play rows out of `map[F::Voice]`),
  `tests/visual/PreviewScenes.cpp`, `tests/CMakeLists.txt`
- Test: `tests/tst_flag_record_play.cpp`

**Interfaces:**
- Consumes: `RecordingsAccess` (Task 5), `VfoWidget::recordToggled`, `playToggled`, `recordButtonForTest`,
  `playButtonForTest`.
- Produces: `FlagRecordPlay(VfoWidget*, RecordingsAccess*, int sliceId, QObject* parent)`; scenes
  `flag-recplay--idle`, `--recording`, `--recording-myvoice`, `--armed`, `--playing`, `--kept`, `--menu`,
  `--older-core`.

**Acceptance:**
- Order top to bottom is ×, lock, Record, Play, ··· on every flag; slice A still has no × slot. Buttons stay
  20x20, step 22.
- Record idle: round, same as the lock. Recording: blinks red with the elapsed time beside it, "+ your voice"
  while `myVoice`. Armed: a hollow red ring and "Waits for you to transmit". Play idle ▶; playing a blue stop
  square with the label (frequency, call when known, time). Matches the mockup at the capture profile.
- Kept: a notice under the flag for 6 s with the name, length, the reason in words ("Stopped on the iPhone",
  "Stopped by itself after two quiet minutes", "Stopped: the disk is full", "Kept as WAV: the MP3 encoder
  failed"), Play and All recordings….
- Right-click Play: the slice's last eight, All recordings…, Show in Finder (disabled with "The Core is on another
  computer." when it is), Recording settings…. Play with no recording yet is disabled with "Nothing recorded on
  Slice A yet."
- Record on the flag calls `start(slice)`; Record again `stop(slice)`; Play plays the slice's latest Contact
  recording; Play again stops.
- Older Core: both buttons visible, disabled, tooltip and accessible description "This Core needs an update for
  recordings." (A15 for the flag). Not connected: disabled with "Connect to a radio to use recordings."
- `tst_unbuilt_features` passes with the moved rows; the container PLAY and REC buttons and the DVK label stay
  under `UnbuiltFeature::Voice`.

**Verification:** UI interaction plus a command: functional test (`tst_flag_record_play` drives the buttons
against a fake access and asserts the calls and states), then captures read by the controller next to the mockup:
`python3 tests/visual/capture.py --out <scratch> --scenes 'flag-recplay-*' --profiles 'mac-1512x869@2' --jobs 4`.
Read the `look-*` and proposed scenes, not `dialog-*`. Geometry unchanged on the existing `flag-fold--states`
scene except the new order. `run-test.sh tst_flag_record_play tst_unbuilt_features tst_remote_gui_gating`.

**Execution note (advisory):** opus. No network. Prerequisite Task 5.

- [ ] **Step 1:** Reorder the flag, wire the buttons and the right-click list, with the functional test.
- [ ] **Step 2:** Add the scenes, capture, compare with the mockup, fix. Commit.

## Task 7: Recordings panel and Recordings window

**Requirements:** spec section 3 (Recordings window, Recordings panel), 6.4, A4 and A16 for these. Mockups:
`recordings-panel/`, `recordings-window/`.

**Files:**
- Create: `src/gui/recordings/RecordingsPanel.{h,cpp}`, `src/gui/recordings/RecordingsWindow.{h,cpp}`
- Modify: `src/gui/MainWindow.{h,cpp}` (construct, `m_appletsById["Recordings"]`, `registerApplet("Recordings",
  "Recordings", false)`, All recordings… opens the window), `src/gui/containers/ContainerContentRegistry.cpp`
  (alias `{"Recordings","recordings"}`, descriptor `applet:recordings` in `ContentGroup::Panels`), `CMakeLists.txt`,
  `tests/visual/PreviewScenes.cpp`, `tests/CMakeLists.txt`
- Test: `tests/tst_recordings_panel_window.cpp`

**Interfaces:**
- Consumes: `RecordingsAccess` (Task 5), `RevealFile` (Task 6).
- Produces: applet id `"Recordings"`; `RecordingsWindow::showFor(RecordingsAccess*, QWidget* parent)`; scenes
  `recordings-panel--list`, `--recording-row`, `--menu`, `recordings-window--all`, `--filtered`, `--delete-confirm`.

**Acceptance:**
- Panel: the six latest, a play button per row, a red row while that slice records, right-click Play / Rename /
  Save a copy here / Show in Finder / Delete, All recordings…, no settings of its own. Listed in Containers >
  Applets.
- Window: "On the Core" totals (saved recordings, whole pan versus sound, free space, the DSR loop not counted),
  filter All / Contact / Whole pan / Sound, search over name and call, sortable columns play, when, kind,
  frequency, mode, call, name, length, size; Play, Rename, Save sound only (enabled for Whole pan only; disabled
  with "Available when the Digital Spectrum Recorder is built." until Task 25), Save a copy here (a file dialog,
  then `download` with progress), Show in Finder (Core on this computer only, else disabled with the reason),
  Delete several with a confirm that says it cannot be undone.
- A rename in the window shows in the panel and the flag's list (A4 at the window).
- Older Core or not connected: both stay in place, dimmed, with the reason.

**Verification:** UI interaction plus commands: functional test against a fake access (rename, delete several
with confirm, download progress, filter and sort), then captures read next to the mockups:
`python3 tests/visual/capture.py --out <scratch> --scenes 'recordings-*' --profiles 'mac-1512x869@2' --jobs 4`.
`run-test.sh tst_recordings_panel_window`.

**Execution note (advisory):** opus. No network. Prerequisite Task 6.

- [ ] **Step 1:** Panel, window and their functional test; scenes and captures. Commit.

## Task 8: Setup > Cores > Recordings

**Requirements:** spec 6.5 (every group), section 3 (Recording settings), A16 for the page. Mockup:
`recording-settings/`.

**Files:**
- Create: `src/gui/setup/RecordingsSetupPage.{h,cpp}`
- Modify: `src/gui/SetupDialog.cpp` (register "Recordings" under Cores after "Audio with the Core", scope
  `SetupScope::Core`), `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_recordings_setup_page.cpp`

**Interfaces:**
- Consumes: `RecordingsAccess::document()` settings and `setSettings` (Task 5).
- Produces: the page; its capture appears automatically as `dialog-setup-*` and in the `look-*` set.

**Acceptance:**
- Groups and defaults exactly as spec 6.5: What the Record button keeps (what you hear, always; your own voice:
  on; together or apart: together), Starting and stopping (start by itself: off; stop by itself: on; quiet for 2
  minutes, 1 to 60), Files (MP3 or WAV with "about 29 MB an hour" / "about 173 MB an hour", and a name example
  from `recordingBaseName`), Where they are kept (disk choice, count and space used, Recordings › opening the
  window; an SD card is allowed with the note that recordings do not wear it like the recorder would), Playing back
  (the sentence "Keying while a recording plays sends it on the air." and five switches EQ, compressor, leveler,
  CFC, phase rotator, each off; the switches are disabled with the V9 reason until the Core offers version 2).
- Each change sends only the changed field; a change made on another device updates the page.
- Not connected: every group dims with a note. Older Core: the V9 reason.

**Verification:** UI plus configuration: functional test (toggle each control against a fake access, assert the
exact JSON sent; a document change updates the controls without echo, guarded by `m_updatingFromModel`), then
the page's captures read next to the mockup. `run-test.sh tst_recordings_setup_page tst_remote_gui_gating`.

**Execution note (advisory):** opus. Config apply to the Core (no rollback needed: each field is independent).
Prerequisite Task 5; parallel with Tasks 6 and 7 in a worktree if the controller wants.

- [ ] **Step 1:** Page and functional test, captures. Commit.

## Task 9: Part 1 on JJ's radio

**Requirements:** spec 8 H evidence for A1, A2, A5; the monitor check from the corrections table.

**Files:** none unless a finding needs a fix. The controller does this task with JJ, not an implementer.

**Interfaces:**
- Consumes: Tasks 1 to 8.
- Produces: a findings list. It does not block Part 2; findings become fixes on the branch.

**Acceptance:**
- **Build:** a desktop build and an `nereusd` build with the branch and short SHA in their names, installed per
  the `NereusSDR Dev.app` rule; the Core install on a bench Core follows the Core installs rule. Tell the manager
  before launching on his Mac.
- **JJ's checklist:** record a contact on slice A from the desktop flag, with transmit; play it back; the other
  side and his voice are both there; apart puts him on the right; record on slice B at the same time; Kept shows
  the right reason; right-click list, panel and window; rename and delete; Setup page against the mockup; his
  voice in AM and FM recordings sounds like MON; a contact with a Roundtable station in the room gets its call
  in the name.
- **Decides:** anything he rejects goes to a fix round; a bad AM or FM monitor result goes to JJ as one question.
  Hardware stays pending until he reports.

**Verification:** Human smoke, JJ, on his radio. Report "ready for his hardware run", never "ready to merge".

**Execution note (advisory):** controller and JJ. Prerequisite Task 8. Push once for this round, with a draft PR.

- [ ] **Step 1:** Build, install, tell the manager Part 1 is ready for his run. Crew continues with Part 2.

---

# Part 2: On the air

## Task 10: The on-air source and the processing switches

**Requirements:** spec section 3 (Playback and the air, Processing on the air), 6.1 (On-air source), 6.3, V5,
V6, V7, A6, A7. Thetis behavior cited, not ported: playback feeds the transmitter while MOX is on
(clsAudioRecordPlayback.cs:3993 `GetPlayBuffer` [v2.10.3.15]); the five switches turn off TX EQ, compressor, CFC,
phase rotator and leveler while playing (clsAudioRecordPlayback.cs:270-283, 533-543 [v2.10.3.15]). Mockup:
`recording-together/` (ON AIR).

**Files:**
- Create: `src/core/recordings/OnAirSource.{h,cpp}`
- Modify: `src/core/TxWorkerThread.{h,cpp}` (an on-air input checked first in `dispatchOneBlock`, above TCI, at
  48 kHz in 64-frame blocks), `src/core/recordings/RecordingService.{h,cpp}`, `src/core/session/*` (capability to
  2, `recordings.playback` `onAir` field live), `src/models/recordings/*Access*`, `src/gui/recordings/FlagRecordPlay.cpp`
  (ON AIR look), `src/gui/setup/RecordingsSetupPage.cpp` (switches enabled at version 2), `tests/data/link/v1/*`,
  `docs/architecture/2026-09-23-station-link-v1.md`, `tests/visual/PreviewScenes.cpp`, `tests/CMakeLists.txt`
- Test: `tests/tst_recording_on_air.cpp`

**Interfaces:**
- Consumes: `RecordingService::pullPlayback`, `playback(who)` (Task 4); `MoxController::currentKeyer()`,
  `KeyerIdentity`, `moxChanged`; `TxChannel::setTxEqRunning`, `setTxCpdrOn`, `setTxLevelerOn`, `setTxCfcRunning`,
  `setStageRunning(Stage::PhRot, …)`; `TransmitModel` getters; `RecordingSettings` on-air keys.
- Produces:
  ```cpp
  namespace NereusSDR {
  class OnAirSource {                         // owned by RecordingService, read by TxWorkerThread
  public:
      enum class Kind { None, Playback, VoiceMessage };
      // audio thread: fills frames when active, returns false to use the normal mic path
      bool pull(float* mono48k, int frames) noexcept;
      Kind active() const noexcept;           // std::atomic inside
      // main thread:
      void beginPlayback(const ListenerId& who);   // on MOX by that listener while it plays
      void beginVoiceMessage(const QString& slotId, std::function<int(float*, int)> pullClip);  // Task 11
      void end();                                  // restores the switches
  };
  // capability recordingsVersion = 2 from this task
  }
  ```

**Acceptance:**
- Listener A plays; A keys: the transmit input is A's playback from its current position (V5); A's speaker stops
  getting it; `onAir` is true for A. A unkeys: back to A's speaker at the new position (A6).
- Listener B (not playing) keys while A plays: B's mic goes out, A keeps hearing its playback (V6, A6).
- The recording ends while A is still keyed: from the next block the mic path takes over, no gap longer than one
  64-frame block, and the radio stays keyed (V5).
- With all five on-air settings off (the default), while on the air `TxChannel` shows EQ, compressor, leveler,
  CFC and phase rotator not running; after unkey they match `TransmitModel` again, whatever the user had (A7).
  With `RecordingsOnAirUseCompressor` on, the compressor keeps its user value while on the air. `TransmitModel`
  and its saved settings never change.
- A user change to a processing control while on the air is kept in `TransmitModel` and applied at the end.
- Keying while nothing plays changes nothing in the transmit path: the existing transmit tests (list with
  `ctest -N | grep -E 'tx_worker|mox|transmit'`) pass unchanged.
- Another station holding transmit: the listener's PTT is refused as today; playback continues to its speaker.
- `pull` takes no lock and does no allocation.
- Desktop: while on the air the playing label turns red with ON AIR and the Play ring turns red (scene
  `flag-recplay--onair`), matching the mockup.

**Verification:** High risk (what the radio transmits): invariant tests first, then the change. The transmit
tests above set the baseline before `TxWorkerThread` changes. `run-test.sh tst_recording_on_air` plus that
baseline set, plus the link tests from Task 5. Hardware: A6 on JJ's radio in Task 14. Flagged for an earlier
review (Before Task 1).

**Execution note (advisory):** opus. Touches what makes the radio transmit. Prerequisite Task 5 (and Task 8 for the
page). Serial.

- [ ] **Step 1:** Baseline the transmit tests. Write the on-air test cases first (they are exact).
- [ ] **Step 2:** Write `OnAirSource`, the `TxWorkerThread` input and the switches; capability 2; desktop ON AIR
  look and Setup switches. Commit.

---

# Part 3: Voice keyer

## Task 11: Voice keyer on the Core

**Requirements:** spec section 3 (Voice keyer sending), 6.1 (Voice keyer), 6.6 (Transmit held elsewhere), V3,
section 7 (another station has transmit), A8 and A9 on the Core, and the corrections table row on recording by
upload.

**Files:**
- Create: `src/core/recordings/VoiceKeyer.{h,cpp}`
- Modify: `src/core/recordings/RecordingService.{h,cpp}`, `src/core/recordings/OnAirSource.cpp`
  (`beginVoiceMessage`), `src/core/session/*` (capability 3, voice keyer part of the `recordings` document, verbs),
  `src/models/recordings/*Access*`, `tests/data/link/v1/*`, `docs/architecture/2026-09-23-station-link-v1.md`,
  `tests/CMakeLists.txt`
- Test: `tests/tst_voice_keyer.cpp`

**Interfaces:**
- Consumes: `OnAirSource` (Task 10), `AudioFileReader`, `AudioFileWriter` (Task 1), `MoxController::setMox(bool,
  const KeyerIdentity&)`, `TransmitHolder::askKey`, `holder()`, `isHeldBy`.
- Produces:
  - Document part `"voiceKeyer": { "slots": [ { "n": 1, "name": "CQ", "lengthMs": 0, "empty": false } x8 ],
    "repeat": false, "repeatDelaySec": 5, "sending": { "n": 2, "positionMs": 0, "lengthMs": 0, "waitingMs": 0,
    "by": "Desktop" } | null }`.
  - Verbs, each `"requires": "recordingsVersion" 3`: `voiceKeyer.send{n}`, `voiceKeyer.stop{}`,
    `voiceKeyer.beginImport{n, name, size}`, `voiceKeyer.chunk{data}`, `voiceKeyer.finishImport{}` (the
    `dspAssets` import pattern), `voiceKeyer.export{n, offset}`, `voiceKeyer.rename{n, name}`,
    `voiceKeyer.clear{n}`, `voiceKeyer.setRepeat{on, delaySec}`, `voiceKeyer.listen{n}` (plays on that listener's
    speaker through the Task 4 player path).
  - `RecordingsAccess` gains the matching methods: `sendVoice(int n)`, `stopVoice()`, `importVoice(int n,
    const QString& wavPath)`, `exportVoice(int n, const QString& toPath)`, `renameVoice`, `clearVoice`,
    `setVoiceRepeat(bool, int)`, `listenVoice(int n)`, `firstEmptyVoiceSlot()`.
  - `static constexpr int kVoiceKeyerSlots = 8; static constexpr int kVoiceClipMaxMs = 120000;`

**Acceptance:**
- Messages are stored per station as 48 kHz 16-bit mono WAV (V3). An import at 44.1 kHz stereo is converted to
  48 kHz mono; an import longer than 2 minutes is refused with "Voice messages can be up to 2 minutes long.";
  export round-trips byte-identical audio (A9).
- `send(2)` keys the radio as the requesting device, sends M2 through the on-air source with the user's normal
  processing (the five switches do not apply), and unkeys at the end (A8).
- Repeat on with 5 s: after the end it unkeys, waits 5 s (`waitingMs` counts down), sends again, until stopped.
- `stop`, a PTT or MOX press from any device, and an unkey from the sender cut it within one block; the radio
  unkeys unless the press itself keys (PTT then keeps the mic on the air).
- Another station holding transmit: `send` is refused with the holder's reason, nothing keys.
- An empty message: `send` refused with "M3 is empty.".
- Import fills the given slot; `firstEmptyVoiceSlot()` returns 1..8 or 0 when full.
- The voice keyer keys only on `send` and never on connect, import, listen or a setting change.

**Verification:** High risk (it keys the radio by itself): invariant tests first (keys only on send; every stop
path unkeys; refused when held elsewhere), then the rest. `run-test.sh tst_voice_keyer tst_recording_on_air`
plus the Task 5 link tests. Hardware in Task 14. Flagged for an earlier review (Before Task 1).

**Execution note (advisory):** opus. Keys the radio. Prerequisite Task 10. Serial.

- [ ] **Step 1:** Write the keying invariant tests first.
- [ ] **Step 2:** Write the keyer, its store, verbs and access methods; capability 3. Commit.

## Task 12: Desktop voice keyer panel and VOICE

**Requirements:** spec section 3 (Voice keyer panel, VOICE buttons), 6.6 (Desktop panel, Desktop VOICE,
Transmit held elsewhere), A8 and A9 on the desktop, A16. Mockups: `voice-keyer-panel/` (shape A),
`desktop-voice-button/`.

**Files:**
- Create: `src/gui/applets/VoiceKeyerApplet.{h,cpp}`, `src/gui/recordings/VoiceButton.{h,cpp}`, `src/gui/recordings/LocalMicClipRecorder.{h,cpp}`
- Modify: `src/gui/applets/TxKeyingSection.{h,cpp}` (VOICE as the third button in the TUNE / MOX row, same height
  22 and stretch 1, `voiceButton()` accessor), `src/gui/MainWindow.{h,cpp}` (register applet `"VoiceKeyer"`,
  "Voice keyer"; leave `DvkApplet`'s commented construction and its files in place, unbuilt), 
  `src/gui/containers/ContainerContentRegistry.cpp`, `CMakeLists.txt`, `tests/visual/PreviewScenes.cpp`,
  `tests/CMakeLists.txt`
- Test: `tests/tst_voice_keyer_desktop.cpp`

**Interfaces:**
- Consumes: `RecordingsAccess` voice methods (Task 11); the desktop's mic capture used for transmit (find the PC
  mic input behind `CompositeTxMicRouter`'s `MicSource::Pc` and read it before processing).
- Produces: applet id `"VoiceKeyer"`; `LocalMicClipRecorder { void start(); void stop(); QString wavPath() const;
  float level() const; signals: levelChanged, finished(QString path, qint64 ms) }` writing 48 kHz mono 16-bit,
  capped at 2 minutes; `VoiceButton(TxKeyingSection*, RecordingsAccess*)`.

**Acceptance:**
- Panel (shape A): Record and Stop, M1 to M8 in a 2 by 4 grid with names, Repeat and its delay, mic level,
  right-click Listen / Rename / Load WAV… / Save as WAV… / Clear, a visible Import WAV… that fills the first
  empty message ("All eight messages are full." when none). Record lights; click a message to record into it;
  Stop ends and uploads.
- Clicking a message sends it (keys by itself). While sending, its tile shows progress.
- VOICE: click opens a box just above the row with M1 to M8, Repeat, "Voice keyer panel ›"; sending closes it;
  VOICE turns red with "M2 0:01" and fills left to right; between repeats "M2 again in 5 s"; VOICE, Escape or MOX
  stops; an empty message opens the panel.
- Another station holds transmit: sends dimmed with Take transmit, as TUNE and MOX are today.
- Older Core (version below 3): panel and VOICE visible, disabled, V9 reason.
- The clip recorder writes 48 kHz mono and stops itself at 2 minutes.

**Verification:** UI plus a command that keys: functional test against a fake access (click sends `sendVoice(n)`,
VOICE stop paths call `stopVoice`, import picks the first empty slot, held-elsewhere dims), clip recorder unit
case with a fake mic source, captures read next to the mockups:
`python3 tests/visual/capture.py --out <scratch> --scenes 'voice-*' --profiles 'mac-1512x869@2' --jobs 4`.
`run-test.sh tst_voice_keyer_desktop`.

**Execution note (advisory):** opus. Reads the mic; no network. Prerequisite Task 11.

- [ ] **Step 1:** Clip recorder, panel, VOICE, functional test; scenes and captures. Commit.

## Task 13: Voice hot keys

**Requirements:** spec 6.6 (Keys), V8, the corrections table row on F1 to F8, A8 (F1 to F8 in voice modes).

**Files:**
- Modify: `src/core/hotkeys/HotKeyActionList.cpp` (voice list to eight with Ctrl+Shift+F1 to F8, Escape added to
  `voice.stop`, `msg.1` to `msg.8` on F1 to F8 in a "Messages" group), `src/gui/hotkeys/MainWindowHotKeyTargets.cpp`
  (`runVoiceMessage`, `msg.N` choosing by `transmitSliceInCw()`, voice reasons from `RecordingsAccess` instead of
  `voiceNotBuiltReason`), `src/gui/hotkeys/HotKeyDispatcher.cpp` (route `voice.*` and `msg.*`),
  `tests/tst_hot_key_bindings.cpp`, `tests/tst_hot_key_window_actions.cpp`
- Test: those two

**Interfaces:**
- Consumes: `RecordingsAccess::sendVoice`, `stopVoice` (Task 11); the CW message path behind `runCwMessage`
  (MainWindowHotKeyTargets.cpp:598).
- Produces: action ids `voice.msg1` to `voice.msg8`, `voice.stop`, `msg.1` to `msg.8`.

**Acceptance:**
- Fresh settings: `actionForChord("F3") == "msg.3"`, `("Ctrl+Shift+F7") == "voice.msg7"`, `("Ctrl+F3") ==
  "cw.msg3"`, `("Escape") == "voice.stop"`; still no two actions share a default key; the total id count in
  `tst_hot_key_bindings` is updated to the new number.
- `msg.3` in USB sends voice M3; in CWU sends CW message 3; in CW with no CW message 3 it gives the CW reason.
- With a Core below version 3, voice actions refuse with the V9 reason; `msg.N` in a voice mode refuses the same
  way and in CW still works.
- Escape in a text field or an open popup keeps its usual meaning (the router's existing rules); it stops a
  message only when the router would act on a key.
- The macOS exception for message 5 at HotKeyActionList.cpp:92-95 is checked against the new F5 default and
  applied the same way if it covers F5.

**Verification:** Ordinary feature, unit layer.
`run-test.sh tst_hot_key_bindings tst_hot_key_window_actions tst_core_has_no_gui_includes`.

**Execution note (advisory):** opus. No network or device. Prerequisite Task 11; parallel with Task 12 in a
worktree.

- [ ] **Step 1:** Change the list, targets and dispatcher with the tests. Commit.

## Task 14: Parts 2 and 3 on JJ's radio

**Requirements:** spec 8 H evidence for A6 and A8.

**Files:** none unless a finding needs a fix. Controller and JJ.

**Interfaces:**
- Consumes: Tasks 10 to 13.
- Produces: a findings list.

**Acceptance:**
- **Build** as in Task 9.
- **JJ's checklist:** play a recording and key: it goes out from where it was (a second receiver or a WebSDR
  hears it), processing off by default and on when switched; unkey and it keeps playing; it ends while keyed and
  his mic takes over; key from another device while one plays: that device's mic goes out. Record M1 from the
  desktop mic; send it; Repeat; stop with VOICE, Escape, PTT and MOX; F1 to F8 in SSB and in CW; Import WAV.
- Hardware stays pending until he reports.

**Verification:** Human smoke, JJ, on his radio, into a dummy load or on the air as he chooses.

**Execution note (advisory):** controller and JJ. Prerequisite Task 13. Push once, draft PR updated.

- [ ] **Step 1:** Build, install, tell the manager Parts 2 and 3 are ready for his run.

---

# Part 4: Phone and iPad for Parts 1 to 3

## Task 15: NereusKit recordings and voice keyer

**Requirements:** spec 6.9 for the phone, V9, A15 for the phone.

**Files:**
- Create: `ios/NereusKit/Sources/NereusModels/Recordings.swift`, `RecordingsClient.swift`, `VoiceKeyer.swift`
- Modify: `ios/NereusKit/Sources/NereusLink/LinkCodec.swift`, `LinkMessage.swift` (verbs and replies),
  `ios/NereusKit/Sources/NereusMirror/MirrorStore.swift` (the `recordings` object), the client's declared features
  (`"recordings"`)
- Test: `ios/NereusKit/Tests/NereusModelsTests/RecordingsTests.swift`

**Interfaces:**
- Consumes: the wire contract of Tasks 5, 10 and 11 (document JSON, verbs, `recordings.playback`).
- Produces: `public static let capabilityName = "recordingsVersion"`; an `@Observable` `RecordingsClient` with
  `state`, `coreVersion`, `unavailableReason(needs:)`, `slices`, `kept`, `index`, `settings`, `voiceKeyer`,
  `playback`, `list(offset:count:)`, `start(slice:)`, `stop(slice:)`, `play(id:)`, `stopPlay()`, `seek(ms:)`,
  `rename(id:name:)`, `delete(ids:)`, `download(id:to:)` (async, progress), `setSettings(_:)`, `sendVoice(_:)`,
  `stopVoice()`, `importVoice(_:url:)`, `exportVoice(_:to:)`, `renameVoice`, `clearVoice`, `setVoiceRepeat`,
  `listenVoice`.

**Acceptance:**
- Decoding the fixture documents and replies from `tests/data/link/v1/sessions/verbs-recordings.json` (shared with
  the desktop tests) gives the same values the C++ tests assert.
- Encoded verbs are byte-identical to the fixtures.
- Without the capability every call throws the V9 reason without sending; with version 1 the voice calls throw it.
- `download` reassembles chunked `export` replies.

**Verification:** Ordinary feature plus protocol: package tests, `ios/scripts/swift-test.sh` (filter to the new
suite if the script allows; read it first).

**Execution note (advisory):** opus. Networking (wire contract). Prerequisite Task 11.

- [ ] **Step 1:** Kit model, codec and mirror with tests. Commit.

## Task 16: iPhone flag Record and Play and the playing bar

**Requirements:** spec section 3 (Phone flag look, Recording together, Start and stop by themselves), 6.2, 6.3,
6.8, A1, A5, A16 for the iPhone. Mockups: `phone/` (flag screens), `recording-together/` (the phone half, ON AIR).

**Files:**
- Modify: `ios/NereusApp/Band/FlagSideButtons.swift` (column ×, lock, Record, Play, ··· inside the flag's
  height, gray icons, no box), `ios/NereusApp/Band/BandGestureLayer.swift`, `ios/NereusApp/Band/VfoFlagView.swift`
- Create: `ios/NereusApp/Recordings/PlayingBar.swift`, `ios/NereusApp/Recordings/KeptNotice.swift`
- Test: `ios/NereusApp/Tests/RecordingFlagShotTests.swift`, `ios/NereusApp/Tests/RecordingFlagTests.swift`

**Interfaces:**
- Consumes: `RecordingsClient` (Task 15).
- Produces: shots `flagbtn-rec-idle`, `-recording`, `-armed` ("AUTO"), `-playing`, `-onair`, `-kept`,
  `-older-core`.

**Acceptance:**
- Record blinks red with the time under it; armed shows the hollow ring and "AUTO"; Play becomes a blue stop
  square; the blue playing bar under the flag has Others (last eight, All recordings…, Recording settings…) and
  Stop, only while playing. While on the air the bar turns red and reads "ON AIR · 0:12 / 3:25".
- Kept notice with the reason and Play, as the desktop's words.
- Every control works by tap; no gesture is the only way.
- Older Core: visible, disabled, the reason in `ToolPageParts.Reason` style.

**Verification:** UI plus commands: unit tests of the button logic against a fake client, then the shot tests
written with `NEREUS_BAND_SHOTS` and read by the controller next to the mockup, on the iPhone 17 simulator.

**Execution note (advisory):** opus. No network beyond the kit. Prerequisite Task 15.

- [ ] **Step 1:** Flag column, playing bar, Kept, tests and shots. Commit.

## Task 17: iPhone Recordings pages and Recording settings

**Requirements:** spec 6.4 (Phone and iPad), 6.5, 6.8, A4, A16. Mockup: `phone/` (Recordings, Recording settings).

**Files:**
- Create: `ios/NereusApp/Recordings/RecordingsPage.swift`, `RecordingSettingsPage.swift`
- Modify: `ios/NereusApp/Tools/StationToolList.swift`, `ios/NereusApp/App/ToolsTab.swift` (Tools > Recordings
  with the Core chip), `ios/NereusApp/Setup/SetupTree.swift` (Setup > Recording settings, a native page),
  `src/core/session/StationCatalog.cpp` (the `recordings` tool row, offered when the Core has the service)
- Test: `ios/NereusApp/Tests/RecordingsPagesTests.swift`, `RecordingsPagesShotTests.swift`

**Interfaces:**
- Consumes: `RecordingsClient` (Task 15).
- Produces: tool id `recordings`; shots `tools-recordings-*`, `setup-recording-settings-*`.

**Acceptance:**
- Recordings: filter, search, sort, a round more button per row, swipe left, Select for several, share sheet with
  a warning on big whole-pan copies ("This is a whole-pan stretch of 1.4 GB."), delete with a confirm that says
  it cannot be undone.
- Recording settings: the groups of spec 6.5, as the desktop page, phone width; not connected dims every group.
- A rename on the phone reaches the desktop list (A4, checked in Task 20).

**Verification:** UI plus commands: unit tests against a fake client (delete confirm, select several, settings
sends only the changed field), shot tests read next to the mockup.

**Execution note (advisory):** opus. Prerequisite Task 15; parallel with Task 16 in a worktree.

- [ ] **Step 1:** Pages, tool row, settings page, tests and shots. Commit.

## Task 18: iPhone voice keyer and VOICE

**Requirements:** spec 6.6 (Phone and iPad), section 3 (VOICE buttons), A8 on the phone, A9, A16. Mockup:
`phone/` (VOICE, Voice Keyer).

**Files:**
- Create: `ios/NereusApp/Recordings/VoiceKeyerPage.swift`, `VoiceButton.swift`, `PhoneMicClipRecorder.swift`
- Modify: `ios/NereusApp/Main/MainScreen.swift` (round VOICE beside PTT), `ios/NereusApp/Tools/StationToolList.swift`,
  `ios/NereusApp/App/ToolsTab.swift`, `src/core/session/StationCatalog.cpp` (the `voiceKeyer` tool row)
- Test: `ios/NereusApp/Tests/VoiceKeyerTests.swift`, `VoiceKeyerShotTests.swift`

**Interfaces:**
- Consumes: `RecordingsClient` voice calls (Task 15).
- Produces: tool id `voiceKeyer`; `PhoneMicClipRecorder` writing 48 kHz mono 16-bit WAV up to 2 minutes, then
  `importVoice`.

**Acceptance:**
- Voice Keyer page like the CW page: a Record card (record into M#, phone mic level), M1 to M8 rows (tap sends;
  ··· for Send, Listen, Record again, Rename, Import WAV, Save as WAV, Clear), Import WAV…, Repeat and wait.
- VOICE beside PTT opens M1 to M8 over the pan; sending shows the number and a red progress ring; tap it or PTT to
  stop. Dimmed with Take transmit when another station holds transmit.
- The mic permission prompt appears only when Record is first tapped.

**Verification:** UI plus a command that keys: unit tests against a fake client (tap sends, both stop paths),
shot tests next to the mockup. Hardware in Task 20.

**Execution note (advisory):** opus. Reads the mic. Prerequisite Task 15.

- [ ] **Step 1:** Page, VOICE, clip recorder, tests and shots. Commit.

## Task 19: iPad

**Requirements:** spec section 3 (iPad), 6.8, A16 for the iPad. Mockup: `ipad/`.

**Files:**
- Create: `ios/NereusApp/iPad/RecordingsApplet.swift`
- Modify: `ios/NereusApp/iPad/AppletColumn.swift`, `ios/NereusApp/Roundtable/RoundtableApplet.swift` (`AppletKind`
  gains `.recordings`, optional, off by default; when on it pushes TX down and the column scrolls), the iPad layout
  for the flag column, playing bar and VOICE beside PTT
- Test: `ios/NereusApp/Tests/IPadShotTests.swift` (new shots), `IPadRecordingsAppletTests.swift`

**Interfaces:**
- Consumes: Tasks 15 to 18.
- Produces: shots `ipad-recordings-*`.

**Acceptance:**
- The pan uses the phone's design (flag column, playing bar, VOICE beside PTT). The optional Recordings panel in
  the right column lists the latest with play and the same row menu; Tools, Recordings and Recording settings are
  the phone's pages, wider.
- Shots at the iPad simulator match the mockup.

**Verification:** UI: shot tests read next to the mockup, plus the applet's unit test.

**Execution note (advisory):** opus. Prerequisite Tasks 16 to 18.

- [ ] **Step 1:** iPad column and layout, tests and shots. Commit.

## Task 20: Part 4 on JJ's iPhone and iPad

**Requirements:** spec 8 H evidence for A1, A5, A6, A8 on the phone and iPad; A4 across devices.

**Files:** none unless a finding needs a fix. Controller and JJ.

**Interfaces:**
- Consumes: Tasks 15 to 19.
- Produces: a findings list.

**Acceptance:**
- **Build:** the phone build installed on JJ's iPhone by its UDID (project memory) and on the iPad, with the
  Core from Task 14.
- **JJ's checklist:** Record on the desktop, the phone shows it within a second, Stop on the phone, both show Kept
  (A1); Play on the phone sounds only on the phone (A5); key from the phone while it plays (A6); record M1 from
  the phone mic and send it (A8); rename on the phone, see it on the desktop (A4); play a recording away from home
  over the rendezvous server (spec 6.8); the iPad column.
- Hardware stays pending until he reports.

**Verification:** Human smoke, JJ, on his devices.

**Execution note (advisory):** controller and JJ. Prerequisite Task 19. Push once.

- [ ] **Step 1:** Build, install, tell the manager Part 4 is ready for his run.

---

# Part 5: Digital Spectrum Recorder on the Core and desktop

## Task 21: DSR ring file and recorder

**Requirements:** spec section 3 (Digital Spectrum Recorder, Your transmit in the recorder), 6.7 (Writing, Disk),
section 7 (DSR disk gone), A10 (writes and wraps), A14 (SD card rule), A17.

**Files:**
- Create: `src/core/recordings/dsr/DsrRingFile.{h,cpp}`, `src/core/recordings/dsr/DsrRecorder.{h,cpp}`
- Modify: `src/models/RadioModel.cpp` (`forkIqToTaps` also copies into the DSR ring for each active receiver,
  copy only), `src/core/recordings/RecordingService.{h,cpp}`, `src/core/session/*` (capability 4, `dsr` part of the
  document: enabled, disk, limit, hours held, oldest and newest time, write position, reason when off),
  `tests/data/link/v1/*`, `docs/architecture/2026-09-23-station-link-v1.md`, `tests/CMakeLists.txt`
- Test: `tests/tst_dsr_ring.cpp`, `tests/tst_dsr_recorder.cpp`

**Interfaces:**
- Consumes: `RadioModel::forkIqToTaps` (connection thread), `streamSampleRateHz`, `streamCentreHz`,
  `MoxController::moxChanged`, the transmit monitor signal (Task 3), `RecordingSettings` DSR keys,
  `RecordingDisks` (Task 2).
- Produces:
  ```cpp
  namespace NereusSDR {
  struct DsrBlockHeader {   // the plan's format: magic "NDSR", format 1
      quint32 magic; quint16 format; quint16 receiver; qint64 utcNs; qint32 sampleRateHz; qint64 centreHz;
      quint8 transmit; quint32 frames;
  };
  class DsrRingFile {
  public:
      bool open(const QString& path, qint64 sizeLimitBytes, QString* error);  // creates, or scans an existing one
      bool append(const DsrBlockHeader& h, const qint16* iq, int frames);       // overwrites the oldest at the limit
      qint64 oldestUtcNs(int receiver) const; qint64 newestUtcNs(int receiver) const;
      // reading for Task 22 and 23:
      int read(int receiver, qint64 fromUtcNs, qint16* iq, int maxFrames, DsrBlockHeader* hdr) const;
      QList<QPair<qint64,bool>> transmitMarks(qint64 fromNs, qint64 toNs) const;
  };
  class DsrRecorder { /* owned by RecordingService; tap side is lock-free */ };
  // capability recordingsVersion = 4 from this task
  }
  ```

**Acceptance:**
- One receiver at 192 kHz for 60 fake seconds writes 60 blocks of 192000 frames; the file grows by about 46 MB
  (2.8 GB an hour, spec 6.7); 384, 768 and 1536 kHz scale the same.
- At a 100 MB limit, block 61 overwrites block 1; `oldestUtcNs` moves on by one second (A10).
- Reading back 1 s from the middle returns the same 16-bit samples written, at the right time and centre
  frequency (A10).
- Transmit seconds carry the flag and the transmit audio beside them; `transmitMarks` returns them.
- Restart: opening an existing ring scans headers and recovers oldest, newest and the write position.
- The tap in `forkIqToTaps` copies into a lock-free ring and returns; a counted fake proves no lock, no
  allocation and no disk call on the connection thread; a stalled writer drops with a counted overflow (A17).
- Disk rules: on by default on the Core's own disk at 10 GB; when every candidate volume is an SD card it stays
  off and the document carries "The only disk is an SD card." (A14 at the Core); a disk that vanishes stops the
  recorder with "The recorder's disk is missing." and recordings keep working.
- Live receivers, slices and the spectrum are unaffected: the existing tests for `forkIqToTaps` consumers (list
  with `ctest -N | grep -E 'iq_tap|tci_iq|spectrum_fork|auto_agc'`) pass unchanged.

**Verification:** Ordinary feature with a connection-thread boundary: the non-blocking invariant first.
`run-test.sh tst_dsr_ring tst_dsr_recorder` plus the baseline set above. A17 at full load is measured on a Core
in Task 27.

**Execution note (advisory):** opus. Touches the I/Q fork (copy only; flag the diff to the reviewer). Prerequisite
Task 3 (service and worker); can follow Part 4 or run beside it in a worktree.

- [ ] **Step 1:** Ring file with its tests.
- [ ] **Step 2:** Recorder, tap, disk rules, capability 4. Commit.

## Task 22: Rewind sound on a slice

**Requirements:** spec section 3 (Rewind sound, PTT while rewound), 6.7 (Rewind, "This changes the core receive
path"), A11 at the Core.

**Files:**
- Create: `src/core/recordings/dsr/DsrReplay.{h,cpp}`
- Modify: the receiver input junction (`ReceiverManager::iqDataForReceiver` to `RxDspWorker`; find where a
  slice's WDSP channel takes its I/Q and add one switch there), `src/models/RadioModel.{h,cpp}`,
  `src/core/recordings/RecordingService.{h,cpp}`, `src/core/session/*` (verbs), `src/models/recordings/*Access*`,
  `tests/data/link/v1/*`, `docs/architecture/2026-09-23-station-link-v1.md`, `tests/CMakeLists.txt`
- Test: `tests/tst_dsr_rewind.cpp`

**Interfaces:**
- Consumes: `DsrRingFile::read` (Task 21).
- Produces:
  - Verbs, `"requires": "recordingsVersion" 4`: `dsr.rewind{slice, utcNs, frequencyHz}`, `dsr.live{slice}`,
    `dsr.seek{slice, utcNs}`.
  - In the `dsr` document part (Task 21): `"rewound": [ { "slice": 0, "utcNs": 0, "frequencyHz": 0, "liveFrequencyHz": 0 } ]`.
  - `RecordingsAccess::rewind(int slice, qint64 utcNs, qint64 hz)`, `live(int slice)`, `seekPast(int slice, qint64 utcNs)`.

**Acceptance:**
- Rewind slice A to a time and frequency inside the ring: from the next block its demodulator is fed from the
  ring at that time, the slice's frequency is set to the clicked frequency, and its audio is the past signal
  (a fake ring holding a 1 kHz offset carrier at 7.100 MHz, demodulated in USB at 7.099 MHz, gives a 1 kHz tone).
- The past plays in real time; position advances with consumption.
- Other slices and other receivers keep running from the radio: their input is unchanged during and after.
- `live` returns slice A to live I/Q and to the frequency it had before rewinding (`liveFrequencyHz`).
- PTT while rewound: slice A goes live on the frequency being played (not the old one), then keys as usual.
- Reaching the newest block plays on into live without a gap longer than one block.
- A rewind outside the ring's range is refused with "That moment is no longer kept."

**Verification:** High risk (the core receive path): regression first. Before the change, run the receive-path
tests (list with `ctest -N | grep -E 'rx_dsp|receiver_manager|slice_audio|wdsp'`) as the baseline; after, they
pass unchanged and `run-test.sh tst_dsr_rewind` passes. Hardware in Task 27. Flagged for an earlier review
(Before Task 1); the PR says this is the one place the design touches I/Q to WDSP to audio.

**Execution note (advisory):** opus. Core receive path. Prerequisite Task 21. Serial.

- [ ] **Step 1:** Baseline the receive-path tests. Write the rewind test cases first.
- [ ] **Step 2:** Add the one switch, the replay feed, the verbs and access. Commit.

## Task 23: Past spectrum and past waterfall rows from the Core

**Requirements:** spec section 3 (Rewind picture, Moving through hours), 6.7 (Hours map, typed time, drawing),
A12 at the Core.

**Files:**
- Modify: `src/core/recordings/dsr/DsrReplay.{h,cpp}` (render rows and a spectrum frame from ring I/Q using the
  pan's own FFT settings and the same FFT engine the spectrum thread uses), `src/core/session/*` (verbs),
  `src/models/recordings/*Access*`, `tests/data/link/v1/*`, `docs/architecture/2026-09-23-station-link-v1.md`,
  `tests/CMakeLists.txt`
- Test: `tests/tst_dsr_past_rows.cpp`

**Interfaces:**
- Consumes: Tasks 21, 22; the spectrum FFT settings of a pan (`PanadapterModel`).
- Produces:
  - Verbs, version 4: `dsr.rows{pan, fromUtcNs, toUtcNs, rowCount}` returning rows in the pan's bin layout and
    their times, in chunks; `dsr.spectrum{pan, utcNs}` returning one frame; `dsr.hours{}` returning, per kept
    hour, its start time and an activity level 0 to 1 (band activity shading).
  - `RecordingsAccess::requestPastRows(...)`, `requestPastSpectrum(...)`, `requestHours()` with result signals.

**Acceptance:**
- Rows rendered from a fake ring holding a carrier at a known offset put its peak in the same bin the live
  spectrum would for that pan's span and FFT size.
- Rows come back newest first, timestamped; a request beyond the ring returns only what is kept.
- `dsr.hours` over a 3 hour ring returns three entries; an hour of silence has activity near 0 and an hour with
  carriers higher.
- Rendering runs on the recordings worker and never on the spectrum thread or the connection thread.

**Verification:** Ordinary feature, unit plus loopback for the verbs. `run-test.sh tst_dsr_past_rows` plus the
Task 5 link tests.

**Execution note (advisory):** opus. Networking (row transfer size): note the bytes per request in the report.
Prerequisite Task 22.

- [ ] **Step 1:** Rows, spectrum and hours with tests, verbs. Commit.

## Task 24: Desktop rewind

**Requirements:** spec section 3 (Rewind sound, Rewind picture, PTT while rewound, Moving through hours), 6.7,
A11 and A12 on the desktop, A16. Mockups: `dsr-rewind/` (index and hours).

**Files:**
- Create: `src/gui/recordings/DsrHoursMap.{h,cpp}`
- Modify: `src/gui/SpectrumWidget.{h,cpp}` (`maxWaterfallHistoryOffsetRows`, `historyRowIndexForAge`,
  `pausedTimeLabelForAge`, `rebuildWaterfallViewport`, `drawTimeScale` and the drag handler reach past the
  window's own history into Core rows; a click on a past signal calls `rewind`; PAST tag on the spectrum; the
  "Drawing" dim while rows arrive; a click on a strip time opens a typed-time box), the S-meter feed (follows the
  moment played), `src/gui/MainWindow.cpp`, `tests/visual/PreviewScenes.cpp`, `tests/CMakeLists.txt`
- Test: `tests/tst_dsr_desktop_rewind.cpp`

**Interfaces:**
- Consumes: `RecordingsAccess` DSR methods (Tasks 22, 23).
- Produces: scenes `dsr-rewind--past`, `--hours`, `--drawing`, `--typed-time`, `--older-core`.

**Acceptance:**
- Drag the strip: inside the window's 20 minutes it behaves as today; past that, rows come from the Core and the
  picture dims with "Drawing" until they arrive. The strip drag stays 20 minutes per height.
- Click a past signal: slice A moves to that frequency and plays from that moment; the waterfall scrolls with the
  sound (top row is the moment heard); the spectrum shows the past with PAST; the S-meter follows.
- Red LIVE returns to live audio at the frequency before rewinding. PTT while rewound takes slice A live on the
  frequency being played.
- Hours map beside the strip: newest at the top, shaded by activity, a white mark where you are; click or drag
  jumps (A12). Typed time jumps.
- Your transmit rows are marked as yours.
- Older Core or DSR off: the strip works as today within 20 minutes; the hours map and past click are disabled
  with the reason ("This Core needs an update for recordings." or the DSR's own off reason).

**Verification:** UI plus commands: functional test against a fake access (drag past 20 minutes requests rows;
click calls `rewind` with the clicked time and frequency; LIVE calls `live`), captures read next to the mockup:
`python3 tests/visual/capture.py --out <scratch> --scenes 'dsr-*' --profiles 'mac-1512x869@2' --jobs 4`.
Existing waterfall history tests pass unchanged (list with `ctest -N | grep -i waterfall`).
`run-test.sh tst_dsr_desktop_rewind` plus those.

**Execution note (advisory):** opus. No network beyond the access. Prerequisite Task 23.

- [ ] **Step 1:** Strip, click, PAST, S-meter, LIVE and PTT behavior with the functional test.
- [ ] **Step 2:** Hours map, typed time, drawing dim; scenes and captures. Commit.

## Task 25: SAVE a stretch

**Requirements:** spec section 3 (Saving a stretch), 6.3 (a whole-pan stretch opens on the waterfall), 6.4
(Kinds, Save sound only), 6.7 (SAVE), A13. Mockup: `dsr-rewind/hours.html`.

**Files:**
- Create: `src/gui/recordings/DsrSaveBox.{h,cpp}`
- Modify: `src/core/recordings/dsr/DsrReplay.{h,cpp}` (copy a stretch out of the ring into the store as WholePan;
  demodulate slice A over a stretch into a Sound recording in the chosen format; replay a saved WholePan stretch
  through the same rewind feed), `src/core/recordings/RecordingStore.cpp`, `src/core/session/*` (verbs
  `dsr.saveStretch{fromUtcNs, toUtcNs, kind}`, `recordings.saveSoundOnly{id}`, `recordings.openStretch{id}`),
  `src/models/recordings/*Access*`, `src/gui/SpectrumWidget.cpp` (the yellow bracket), `src/gui/recordings/RecordingsWindow.cpp`
  (Save sound only enabled), `tests/data/link/v1/*`, `docs/architecture/2026-09-23-station-link-v1.md`,
  `tests/visual/PreviewScenes.cpp`, `tests/CMakeLists.txt`
- Test: `tests/tst_dsr_save.cpp`

**Interfaces:**
- Consumes: Tasks 21 to 24, `RecordingStore::add` (Task 2), `AudioFileWriter` (Task 1).
- Produces: the three verbs at version 4; `RecordingsAccess::saveStretch(qint64 fromNs, qint64 toNs, bool wholePan)`,
  `saveSoundOnly(const QString& id)`, `openStretch(const QString& id)`; scenes `dsr-save--bracket`, `--box`.

**Acceptance:**
- SAVE above the hours map shows a yellow bracket, 10 minutes by default; its ends and middle drag. The box shows
  start and end times, length, the size of each choice, and "Save stretch (whole pan)", "Save sound only (MP3)"
  (or WAV per the Files setting) and Cancel.
- Whole pan: a WholePan entry whose length is the bracket's within one second and whose size matches the ring's
  bytes for that stretch; it is never overwritten by the ring (A13).
- Sound only: a Sound entry of the right length in the chosen format, slice A's audio over the stretch (A13).
- Playing a WholePan entry opens it on the waterfall and plays like rewind, any station, by clicking (A13); "Play
  its sound only" plays just its sound.
- The Recordings window's Save sound only on a WholePan row makes a Sound entry.
- A stretch partly outside the ring saves only what is kept and says so.

**Verification:** Ordinary feature plus UI: unit and loopback for save and replay, functional test for the bracket
and box against a fake access, captures next to the mockup. `run-test.sh tst_dsr_save` plus the Task 22 and 24
tests.

**Execution note (advisory):** opus. Large disk copies on the worker only. Prerequisite Task 24.

- [ ] **Step 1:** Core save, sound only and replay with tests.
- [ ] **Step 2:** Bracket, box and window wiring; scenes. Commit.

## Task 26: Setup > Cores > Digital Spectrum Recorder

**Requirements:** spec section 3 (Recorder Setup), 6.7 (Disk, Setup page), A14 on the page, A16. Mockup:
`dsr-setup/`.

**Files:**
- Create: `src/gui/setup/DsrSetupPage.{h,cpp}`
- Modify: `src/gui/SetupDialog.cpp` (register "Digital Spectrum Recorder" under Cores, before "Recordings" as the
  spec places Recordings under it), `src/core/session/*` (verb `dsr.eraseAll{}`; DSR settings through
  `recordings.setSettings`), `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_dsr_setup_page.cpp`

**Interfaces:**
- Consumes: the `dsr` document part (Task 21), `setSettings`, `dsr.eraseAll`.
- Produces: the page; captured as `dialog-setup-*` and `look-*`.

**Acceptance:**
- On/off and status; disk, size limit, the hours it holds at the current receivers and rates; an in-use bar with
  a moving write head and a red "erased next" zone; the plain "It is always erasing" note; saved stretches are
  never overwritten; erase what is kept now (asks first, says it cannot be undone).
- SD card only: the switch off and disabled, a strong red warning to connect a USB SSD or NVMe drive, "thumb
  drives are not enough" (A14 on the page).
- A size change applies on the Core and the hours figure updates.

**Verification:** UI plus configuration: functional test against a fake access (each control sends its field;
erase asks first; SD card state disables the switch), captures next to the mockup.
`run-test.sh tst_dsr_setup_page tst_remote_gui_gating`.

**Execution note (advisory):** opus. Config apply on the Core, including an erase: the confirm is the safeguard.
Prerequisite Task 21; parallel with Tasks 22 to 25 in a worktree.

- [ ] **Step 1:** Page, erase verb, functional test and captures. Commit.

## Task 27: Part 5 on JJ's radio and a Pi

**Requirements:** spec 8 H evidence for A11, A12, A13, A14 (Pi), A17 (timing under a full DSR load).

**Files:** none unless a finding needs a fix. Controller and JJ.

**Interfaces:**
- Consumes: Tasks 21 to 26.
- Produces: a findings list and the A17 measurement.

**Acceptance:**
- **Builds:** desktop and `nereusd` as in Task 9; the Core installed on a bench Pi with only its SD card (the
  bench notes in project memory), and on the Core JJ uses with a real disk.
- **A17:** on the Core with the most receivers at the highest rate JJ runs, 30 minutes of DSR writing with the
  existing audio underrun and late-block counters showing no rise over a run with the DSR off. Tell the manager
  before any timing run on his Mac.
- **A14:** the Pi with only an SD card shows the DSR off with the red warning; recordings still work there.
- **JJ's checklist:** rewind 5 minutes and 2 hours; tap a signal; LIVE; PTT while rewound; hours map; typed time;
  SAVE whole pan and sound only; replay a whole-pan stretch and tune another station in it; the Setup page.
- Hardware stays pending until he reports.

**Verification:** Human smoke and a timing measurement on a Core. Device steps are the controller's; deploy to an
explicit address and leave the device as found.

**Execution note (advisory):** controller and JJ. Touches a bench device. Prerequisite Task 26. Push once.

- [ ] **Step 1:** Build, install on the Pi and his Core, run A17, tell the manager Part 5 is ready for his run.

---

# Part 6: Digital Spectrum Recorder on the phone and iPad

## Task 28: Phone and iPad DSR

**Requirements:** spec 6.7 (Rewind on phone and iPad), 6.8 (the DSR strip, hours map, LIVE, SAVE and the clock on
the pan; Setup > Digital Spectrum Recorder), A11 and A12 on the phone, A16. Mockups: `phone/` (DSR), `ipad/`.

**Files:**
- Modify: `ios/NereusKit/Sources/NereusModels/RecordingsClient.swift` (DSR verbs and document part),
  `ios/NereusKit/Sources/NereusLink/*`, the phone and iPad pan views (time strip, past rows, PAST, LIVE, hours map,
  SAVE bracket and box, the clock), `ios/NereusApp/Setup/SetupTree.swift` (Setup > Digital Spectrum Recorder)
- Create: `ios/NereusApp/Recordings/DsrStrip.swift`, `DsrHoursMap.swift`, `DsrSaveSheet.swift`, `DsrSetupPage.swift`
- Test: `ios/NereusKit/Tests/NereusModelsTests/DsrTests.swift`, `ios/NereusApp/Tests/DsrShotTests.swift`, `DsrTests.swift`

**Interfaces:**
- Consumes: the DSR wire contract of Tasks 21 to 26.
- Produces: shots `dsr-phone-*`, `dsr-ipad-*`.

**Acceptance:**
- Kit: DSR verbs encode byte-identical to the shared fixtures; rows decode to the same bins.
- Drag the strip back; tap a signal; slice A plays the past; LIVE returns; PTT while rewound takes it live; hours
  map jumps; SAVE with the bracket and the box; Setup page with the SD card warning.
- Over the rendezvous server it works the same (rows arrive in chunks; "Drawing" while they do).
- Older Core or DSR off: disabled with the reason.

**Verification:** UI plus commands: package tests, unit tests against a fake client, shots read next to the mockups.

**Execution note (advisory):** opus. Networking (row transfer to a phone away from home). Prerequisite Task 26.

- [ ] **Step 1:** Kit DSR with tests.
- [ ] **Step 2:** Phone and iPad pan, hours map, SAVE, Setup page, tests and shots. Commit.

## Task 29: Part 6 on JJ's iPhone and iPad

**Requirements:** spec 8 H evidence for A11 and A12 on the phone.

**Files:** none unless a finding needs a fix. Controller and JJ.

**Interfaces:**
- Consumes: Task 28.
- Produces: a findings list.

**Acceptance:**
- **Build** as in Task 20.
- **JJ's checklist:** rewind on the phone at home and away over the rendezvous server; tap a past signal; LIVE;
  PTT while rewound; hours map; SAVE; the iPad.
- Hardware stays pending until he reports.

**Verification:** Human smoke, JJ, on his devices.

**Execution note (advisory):** controller and JJ. Prerequisite Task 28.

- [ ] **Step 1:** Build, install, tell the manager Part 6 is ready for his run.

## Task 30: Whole-feature checks

**Requirements:** every acceptance row A1 to A17 in spec section 8.

**Files:**
- Create: `docs/architecture/2026-11-15-recording-voice-keyer-dsr-evidence.md`
- Modify: `docs/architecture/2026-11-15-recording-voice-keyer-dsr-design.md` (status line), `README.md` (one
  feature line), `CHANGELOG.md` only if the release docs owner asks (release docs belong to the docs shepherd)

**Interfaces:**
- Consumes: everything.
- Produces: the evidence table, one row per A1 to A17 with its test names, layer (S, I, H) and state (passed,
  pending, failed), and what stays unproven (A4's call from a logbook, until a log exists).

**Acceptance:**
- Every A row names tests that exist and pass, or a hardware state with who saw it. Nothing says tested on a
  device unless JJ reported it.
- `tst_core_has_no_gui_includes`, `tst_unbuilt_features`, `tst_remote_gui_gating`, `tst_link_surface`,
  `tst_link_surface_manifest_regen`, `tst_hot_key_bindings` and every test this plan added pass together on the
  finished revision.
- Repo gates pass: `python3 scripts/verify-thetis-headers.py`, `verify-inline-tag-preservation.py`,
  `verify-inline-cites.py`, `check-new-ports.py`, `verify-provenance-sync.py`, `check-third-party-licenses.py`,
  `compliance-inventory.py --fail-on-unclassified`, `verify-test-registration.py`,
  `generate-contributor-indexes.py --check`, `render-link-tables.py --check`, `verify-no-gui-dsp-access.py`,
  `verify-ios-provenance.py`, `verify-ios-workspace-colours.py`.
- Captures of every new scene read once more by the controller next to the mockups, geometry unchanged on scenes
  this plan didn't touch.
- `nereusd` builds and links LAME; a window running its own radio records too.

**Verification:** Combined final checks once, on the finished revision. Subsystems by
`cmake --build build --target tests_core && ctest --test-dir build -L core` and the same for `gui`, through the
lane's load gate. The full suite runs on GitHub when the PR is marked ready.

**Execution note (advisory):** sonnet for the evidence document, opus if a check fails. Prerequisites: all tasks.
The whole-branch review follows this, by crew's rule.

- [ ] **Step 1:** Write the evidence table from what actually ran, then run the combined checks once. Commit.
