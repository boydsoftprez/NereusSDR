# Native audio engines and live device lists: implementation plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** speakers, headphones, the PC mic and Windows VAX play and record through each
system's own sound engine (Core Audio, Windows audio shared and exclusive, ASIO,
PipeWire, PulseAudio, and ALSA direct on the Core), with device lists that update by
themselves, a clock-matched buffer on every local output that sizes itself for the
lowest delay, "not connected" handling that never moves the mic or a VAX channel on its
own, and a Core speaker that any window and the phone can set.

**Architecture:** every engine implements `IAudioBus` behind a new
`IAudioEngineBackend`, so `AudioEngine`, the mixers and remote playback keep their
interface. A new `AudioDeviceCatalog` on its own thread is the one source of device
lists, fed by each engine's notices, and a new `AudioStreamSupervisor` owns fallback,
return, retry and default following per role. A new lock-free `DeviceRateMatcher`
(rmatchV's control and slew logic ported, WDSP's `xvarsamp` called as vendored) sits
between the DSP and each device callback, and the same ring in shared memory carries
the PC mic, and ASIO audio, between the helper process and the window. The Core gets
ALSA direct, new mirrored `coreSpeaker*` properties behind feature `coreSpeaker` 1 and
capability `coreSpeakerVersion` 1, a card in Setup and a section in the phone's Sound
panel.

**Tech Stack:** C++20, Qt 6 (Core, Widgets, Test), CMake and Ninja; vendored WDSP
(`third_party/wdsp/`, `varsamp.h`); macOS Core Audio and AudioToolbox (AUHAL),
`os_workgroup`; Windows WASAPI (`IAudioClient3`, `IMMNotificationClient`), COM, MMCSS,
the Steinberg ASIO SDK 2.3.4 (GPL-3.0-only, vendored after JJ's yes); libpipewire-0.3,
libpulse, ALSA (`libasound`), inotify; POSIX shared memory and named semaphores, Windows
file mappings and events; PortAudio (older drivers); the station link (`MirrorPolicy`,
`StationServer`, `StationClient`, `StationCapabilities`, the link document and its
surface manifest); Swift 6 with SwiftUI and swift-testing for the iPhone app (`ios/`).

**Spec:** [2026-10-08-native-audio-engines-design.md](2026-10-08-native-audio-engines-design.md),
approved by JJ 2026-10-08 ("write the plan"). Every decision is settled there (D1 to
D34, R-AUD-01 to R-AUD-34, V-SW-1 to V-SW-10, V-UI-1 to V-UI-5, V-HW-1 to V-HW-9);
this is how it gets built. Mockups are in the spec's sibling folder
`2026-10-08-native-audio-engines-design/`: `asio-setup-mockup.html`,
`header-menu-mockup.html`, `tx-mic-mockup.html`, `core-speaker-mockup.html`,
`phone-core-speaker-mockup.html`.

## Global Constraints

- **Repository rules.** `CLAUDE.md` and `CONTRIBUTING.md` bind every task. No `goto`,
  no raw `new`/`delete` (unique_ptr or Qt parent), `constexpr` not `#define`, braces on
  all control flow, `auto` only when the type is obvious, `PascalCase` classes,
  `camelCase` methods, `kPascalCase` constants, `m_camelCase` members, `Q_OS_WIN` /
  `Q_OS_MAC` / `Q_OS_LINUX` guards (never `_WIN32` / `__APPLE__`), errors through
  `qCWarning(lcAudio)` (or the area's category), no exceptions. Don't remove code you
  didn't add except where a task says to replace it.
- **Settings.** `AppSettings`, never `QSettings`. PascalCase keys; booleans are the
  strings `"True"` / `"False"`. New keys under each device prefix (`audio/Speakers`,
  `audio/Headphones`, `audio/TxInput`, `audio/Vax1` to `audio/Vax4`): `Engine` (an
  engine key below), `DeviceId`, `FirstChannel` (1-based int, default 1), `MicChannel`
  (`Left`, `Right` or `Both`, default `Left`), `DelayMs` (0 automatic, else 2, 3, 5,
  10, 20 or 40). `DeviceName` keeps its name and meaning. The existing keys
  (`DriverApi`, `DeviceName`, `SampleRate`, `BitDepth`, `Channels`, `BufferSamples`,
  `ExclusiveMode`, `EventDriven`, `BypassMixer`, `ManualLatencyMs`) stay readable for
  migration and are never deleted. ASIO keys: `audio/Asio/Driver`,
  `audio/Asio/BufferFrames`, `audio/Asio/SampleRate`. The Core speaker uses the Core's
  own `audio/Speakers/*` keys and its existing `audio/Master/Volume` (string `"0.720"`
  form) and `audio/Master/Muted`.
- **Engine keys and labels** (exact, used by every task): keys `"PortAudio"`,
  `"CoreAudio"`, `"WindowsShared"`, `"WindowsExclusive"`, `"ASIO"`, `"PipeWire"`,
  `"PulseAudio"`, `"AlsaDirect"`; labels "Older drivers", "Core Audio", "Windows audio,
  shared", "Windows audio, exclusive", "ASIO", "PipeWire", "PulseAudio", "ALSA, direct".
  The PortAudio host API name (today's `DriverApi`) is shown under "Older drivers" as
  "MME", "DirectSound", "WDM-KS", "JACK" or "ALSA".
- **Rule R1.** Nothing under `src/core/` or `src/models/` includes a GUI header;
  `tst_core_has_no_gui_includes` must stay green (V-SW-10). The catalogue, the
  supervisor, the matcher and every engine live under `src/core/audio/`.
- **Threads.** The GUI and all models run on the main thread. The catalogue has its own
  `QThread`. A device callback (any engine) takes no lock, allocates nothing, makes no
  Qt call and no system call other than the engine's own buffer calls and a semaphore
  or event post; it never re-lists devices. System notices are posted to the
  catalogue's thread; stream events (lost, busy, format changed, reset) are posted to
  the main thread. Cross-thread traffic is auto-queued signals or
  `QMetaObject::invokeMethod`. Values shared with a callback are `std::atomic`, loaded
  once per callback.
- **Safety boundary.** Connecting never keys. Nothing in this plan keys a transmitter or
  changes what keys it: R-R3-36's keying rule stays exactly as it is (voice-mode MOX
  refused with "Microphone is not ready. Check Audio settings and retry." while the PC
  mic is not ready; losing it mid-transmission releases MOX). The PC mic and a VAX
  channel are never moved to another device on their own. NereusSDR never opens a
  Bluetooth mic unless it is picked by name.
- **Core RX path.** Tasks 1, 2, 6, 7 and 8 to 12 touch the audio path from WDSP to the
  speakers (I/Q to WDSP to audio). CLAUDE.md asks for this to be flagged; it is, in the
  "Before Task 1" section and in each task's execution note.
- **Ports and cites.** Read `docs/attribution/HOW-TO-PORT.md` before any port. In the
  same commit as the port, the file header gets the upstream header byte-for-byte plus
  a "Modification history (NereusSDR)" block (date, J.J. Boyd (KG4VCF), the AI tooling
  used), and `docs/attribution/THETIS-PROVENANCE.md` gets a row
  (`| NereusSDR file | Thetis source | Line ranges | Type | Variant | Notes |`). Cites
  are `// From Thetis Project Files/Source/wdsp/rmatch.c:256-273 [v2.10.3.15 @3759d09]`.
  Constants keep their exact upstream values as named `constexpr` with the cite. Inline
  comments inside ported logic are copied verbatim, commented-out upstream lines
  included (`scripts/verify-inline-tag-preservation.py`). WDSP calls match
  `third_party/wdsp/` exactly: `create_varsamp(int run, int size, double* in,
  double* out, int in_rate, int out_rate, double fc, double fc_low, int R, double gain,
  double var, int varmode)`, `destroy_varsamp(VARSAMP)`, `int xvarsamp(VARSAMP a,
  double var)` (`third_party/wdsp/src/varsamp.h:61-68`), declared `extern "C"` under `HAVE_WDSP` as
  `RemoteAudioRateMatcher.cpp:165-186` does. No cites in user-facing strings.
- **ASIO licence.** The ASIO SDK 2.3.4 is GPL-3.0-only (NereusSDR elects GPL-3,
  `LICENSE-NOTICE`). It is vendored at `third_party/asiosdk/` only after JJ's yes. Our
  ASIO host code is written against the SDK's own sample and headers; Thetis's
  `hostsample.cpp` (Steinberg's sample patched, no GPL header) is read as a reference
  and never copied.
- **Wire compatibility.** New mirrored state goes only to a peer that declared feature
  `coreSpeaker` 1; the Core advertises capability `coreSpeakerVersion` 1. Never change
  `kSessionProtocolMinor`. Existing property ordinals and capability order don't move;
  older clients and older Cores keep today's behaviour and goldens stay byte-for-byte
  except the surface manifest entries this plan adds.
- **Capture protocol.** `CaptureProtocol::kVersion` is 1 today
  (`CaptureProtocol.h:35`). Task 1 makes it 2 (probe records), Task 13 makes it 3
  (shared-memory hand-off), Task 15 makes it 4 (ASIO). The window and its helper ship
  together; a mismatch stays a `ProtocolError` as today.
- **Downloads.** Nothing downloads without JJ's yes, asked by the controller in chat:
  the ASIO SDK, apt packages in a container, a deps cache refresh, a new Docker image. A
  fresh CMake build directory downloads Opus, so reuse `build/` on the Mac and the
  `nereus-native-audio-linux` volume in the Linux lane.
- **No devices in tests (R-AUD-32).** Tests never open a real audio device on any
  engine. Every engine has a seam (a system interface the test fakes) and every test
  that reaches `AudioEngine` installs fakes. `audioDevicesBarredForTestRun()` (Task 3)
  makes every native engine refuse to open, exactly as `portAudioBarredForTestRun()`
  does for PortAudio today (`PortAudioBus.cpp:1003-1009`).
- **Wording.** Plain user words, no internal names, no "yet" in user strings, no cites.
  Every string the spec fixes is used exactly as the spec's R-AUD text writes it; where
  a mockup differs from the spec, the spec wins ("Set in the ASIO control panel",
  R-AUD-20; the R-AUD-14 Bluetooth note). The middle dot in labels is U+00B7, written in
  C++ as `QChar(0x00B7)`. New strings pass `OperatorWording::isPlain`
  (`tests/OperatorWording.h`) where the area already uses it.
- **Disabled, never hidden.** A choice that does not apply is shown greyed with its
  reason, never removed (ASIO control panel button, Rescan on the Mac, a not-running
  engine, an unsupported ASIO format's pairs). The one exception the spec makes is the
  Core speaker card in a window that runs the radio itself (R-AUD-27), and the phone
  section's no-card rule (R-AUD-29).
- **Commits.** GPG-signed with hooks (`git commit -S`), never `--no-gpg-sign` or
  `--no-verify`. No `Co-Authored-By` trailer. No em-dash or en-dash characters in commit
  messages, code comments or docs. Stage explicit paths only. Every commit names its
  R-AUD IDs (or V-HW-8 for Task 1). Never push; the controller asks JJ.
- **Tests (Mac).** Read `docs/development/fast-test-loop.md`. Build exact targets, then
  run them offscreen:
  `cmake --build build --target <tests> && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(<tests>)$' --no-tests=error --output-on-failure`.
  Never run the unfiltered suite inside a task; never `ctest` without building the
  targets first. New tests register with `nereus_add_test(...)` in `tests/CMakeLists.txt`
  (flags `CORE_ONLY`, `NATIVE_WINDOW`, `REALTIME`, `EXTRA_LABELS`; platform-only tests
  go inside `if(APPLE)` / `if(UNIX AND NOT APPLE)` blocks as at
  `tests/CMakeLists.txt:1610` and `:1628-1673`); `scripts/verify-test-registration.py`
  must pass. Fakes live in `tests/fakes/` as headers, as `tests/fakes/FakeAudioBus.h` and
  `tests/fakes/PacedAudioBus.h` do; the existing `FakeAudioBus` (37 tests use it) is not
  changed, and the new matcher-holding fake is a separate `FakeMatcherAudioBus`.
- **Linux lane.** Linux code is compiled and tested in the offline container, never on
  the Mac:
  `docker run --rm --network none -v "$PWD":/src:ro --tmpfs /src/build -v ~/.config/nereus/work/deps-cache/nereus-core-pins-0368ff16:/deps:ro -v nereus-native-audio-linux:/work --entrypoint bash nereusd-core-gui-c2b00a54-compiled:verification -c '<script>'`.
  The controller configures `/work/build` once in pre-flight. Every task's script is
  `cmake --build /work/build --target <t> -j4 && QT_QPA_PLATFORM=offscreen ctest --test-dir /work/build -R '^(<t>)$' --no-tests=error --output-on-failure`.
  The image has PipeWire and ALSA headers; libpulse headers only if JJ approves a
  derived image (pre-flight), otherwise PulseAudio code compiles in CI only.
- **Windows.** Windows code compiles only in CI (MinGW in `.github/workflows/ci.yml`
  with tests off, MSVC in `release.yml`), and CI runs only after JJ approves a push.
  Windows-only code lives in files built only on Windows; every decision it makes
  (format order, pair mapping, notice handling, ASIO session rules) lives in portable
  code tested on the Mac with fakes.
- **iPhone.** NereusKit tests: `ios/scripts/swift-test.sh --jobs 2 --filter <Suite>`.
  App tests: reserve `/tmp/nereus-ios-full.lock` with `mkdir` first (record the token,
  release only your own in an exit trap), one simulator, jobs 2, then
  `ios/scripts/generate-project.sh` and
  `xcodebuild -project ios/NereusSDR.xcodeproj -scheme NereusSDR -destination 'platform=iOS Simulator,name=iPhone 17' -collect-test-diagnostics never -parallel-testing-enabled NO -jobs 2 test`
  (see `ios/README.md`).
- **Hardware.** No subagent touches a radio, a sound device, a Core or a phone. Every
  V-HW case stays pending for JJ's bench. Core installs are the Core session's job.

## What already exists

- **The bus interface.** `src/core/IAudioBus.h`: `AudioFormat`; `OutputPacing`
  (`consumedFrames`, `queuedFrames`, `capacityFrames`, `callbackFrames`, optional
  `deviceLatencyNs`, `:33-42`); virtuals `open`, `close`, `isOpen`, `push`, `pull`,
  `flush` (`:66`), `outputPacing` (`:72`), `outputHasReader` (`:79`), `rxLevel`,
  `txLevel`, `backendName`, `negotiatedFormat`, `errorString`.
- **AudioEngine** (`src/core/AudioEngine.{h,cpp}`): bus choice in `makeBus`
  (`.cpp:815-851`), `makeVaxBus` (`:853-930`); `ensureSpeakersOpen` (`:1121`),
  `applySpeakersConfig` (`:1394`), `reopenHeadphones` (`:1446`); `rxBlockReady`
  (`:2206`) with the speakers push under `try_to_lock` at `:2738-2760` through
  `m_speakersConverter` (`SpeakerFormatConverter`, `.h:1333`), headphones at
  `:2650-2657`, VAX at `:2415`; `writeRemotePlayback` (`:1329-1371`);
  `vaxOutputPacing` / `writeVaxOutput` (`:1646-1686`); `rescanLinuxBackend`
  (`:503-537`); `Pa_Initialize` (`:344`), `Pa_Terminate` (`:442`); `pullTxMic`
  (`:2811`, declared `.h:841`), called by `TxWorkerThread.cpp:986` once per TX block,
  which is paced by the radio's mic frames. Remote API at `.h:495-524`. Test seams at
  `.h:680-735`: `setVaxBusFactoryForTest`, `DeviceBusFactory` /
  `setDeviceBusFactoryForTest`, `paInitializeCallsForTest`, `setSpeakersBusForTest`,
  `setHeadphonesBusForTest`, `setTxInputBusForTest`,
  `setCaptureSupervisorOptionsForTest`. `m_paInitialized`, `m_deviceLayerReady`,
  `m_running` (`.h:1540-1550`), `m_linuxBackend` (`.h:1556-1561`). Unused PipeWire
  outputs `makeTxInputBus` / `makePrimaryOut` / `makeSidetoneOut` / `makeMonitorOut`
  (`.cpp:1013-1119`) stay as they are.
- **PortAudio** (`src/core/audio/PortAudioBus.{h,cpp}`): `AudioDirection{Output, Input}`
  (`.h:55`); the output ring `m_ring` with its atomics (`.h:282-289`,
  `kDefaultRingSamples` `.h:205`); `paCallback` (`.cpp:675-937`); `m_outputLatencyNs`
  (`.h:294`, `.cpp:500-505`); `outputPacing` (`:628-655`); `resolveDevice` (`:76-230`,
  with bug 6's `#ifdef __APPLE__` at `:119`); `matchNamedDevice` (`:939-977`, bug 1:
  it matches a name across host APIs); `portAudioBarredForTestRun` (`:1003-1009`);
  static `hostApis` (`:1012`), `outputDevicesFor` (`:1023`), `inputDevicesFor` (`:1040`).
- **Saved device config** (`src/core/AudioDeviceConfig.{h,cpp}`): fields `deviceName`
  (empty = platform default), `sampleRate`, `channels`, `bufferSamples` (128),
  `exclusiveMode`, `hostApiIndex`, `driverApi` (empty = PortAudio default), `bitDepth`,
  `eventDriven`, `bypassMixer`, `manualLatencyMs`; `loadFromSettings(prefix)` /
  `saveToSettings(prefix)` read and write `audio/<prefix>/<Key>` (`.cpp:33-65`,
  `:83-96`). `DeviceCard` saves `driverApi` as the combo text when its index is above
  0, else empty (`DeviceCard.cpp:750-755`).
- **The existing rate matcher.** `src/core/session/media/RemoteAudioRateMatcher.{h,cpp}`
  wraps WDSP rmatchV for remote playback (header pattern with the verbatim `ivac.c` and
  `rmatch.c` headers and a Modification history block; `extern "C"` declarations under
  `HAVE_WDSP` at `.cpp:165-186`; `filterDelayFrames` at `:213-221`; stats
  `{underflows, overflows, currentRatio, ringCapacityFrames, ringFillFrames,
  controlActive}` at `.h:135-146`). Its users: `RemoteAudioReceiver` (playback),
  `RemoteMicReceiver` (phone mic TX pump), `RemoteVaxFeeder` (VAX).
- **rmatch.c** (`third_party/wdsp/src/rmatch.c`, identical to Thetis v2.10.3.15):
  `control` `:256-273`, `blend` `:275-283`, `upslew` `:285-298`, `xrmatchIN`
  `:301-362` (overflow `:318-352`), `dslew` `:364-425`, `xrmatchOUT` `:428-467`,
  `calc_rmatch` (`:132-158`), `create_rmatchV` `:501-527`. Its header reads
  "Copyright (C) 2017, 2018, 2022 Warren Pratt, NR0V", GPL v2 or later,
  warren@wpratt.com.
- **Remote playback.** `src/core/session/media/RemoteAudioReceiver.cpp`: the sink-mode
  branch released by the jitter hold alone (`:954-998`); the normal path
  (`:1033-1150`: room check, `takeReady`, `matcher.push`, pacing, 500 ms stall checks,
  `speakerTargetFrames`, the early conceal loop, `matcher.take`, the mono mix,
  `writeRemotePlayback`); `jitter.setDownstreamExcessNs` (`:1009-1015`).
- **The PC mic helper.** `src/core/audio/CaptureHelper.{h,cpp}` (opens the mic mono,
  `.cpp:400`; pump `:144`; `Pa_Initialize` `:378`), `CaptureSupervisor.{h,cpp}`
  (states and reasons `.h:78-88`; `helloTimeoutMs` 3000, `openTimeoutMs` 10000,
  `stopTimeoutMs` 2000; the 10 s delivery-delay log `kDelayWindowMs`, `.cpp:44`),
  `CaptureProtocol.h` (`kVersion` 1, `RecordType{Hello=1, Status=2, Pcm=3,
  Configure=16, Open=17, Stop=18, Shutdown=19}`, `kHelperPcmFrames` 480, PCM header
  with `sentMonotonicNs`), `CaptureAudioBus.{h,cpp}` (`writeFrames` `.h:66`, `pull`
  returns 48 kHz mono float), `src/capture_main.cpp`, target `nereus-audio-capture`
  (`CMakeLists.txt:2264-2276`).
- **Real-time priority.** `src/core/audio/RealtimeAudioPriority.{h,cpp}`:
  `fetchDefaultOutputWorkgroup` (`.cpp:91-131`), `elevateAudioThreadPriority`
  (`:135+`). `src/models/RxDspWorker.cpp` joins at `onThreadStarted` (`:195-219`) and
  leaves at `onThreadFinished` (`:221-231`).
- **Linux today.** `src/core/audio/LinuxAudioBackend.{h,cpp}`
  (`LinuxAudioBackendProbes{pipewireSocketReachable, pactlBinaryRunnable,
  forcedBackendOverride}`, `detectLinuxBackend` `.cpp:61-90`, override key
  `Audio/LinuxBackendPreferred`); `PipeWireStream.h` (`StreamConfig`, `open` / `close`
  / `push` / `pull`, telemetry, `outputCounters`); `PipeWireThreadLoop::connect()`.
- **Setup pages** (`src/gui/setup/`): `AudioOutputsPage` (Rescan `:403-432` with
  objects `rescanDevices` / `rescanDevicesResult`; `thisComputerGroup` `:167-244`;
  headphones `:247-258`; `radioSpeakerGroup` `:261-400`), `AudioTxInputPage`
  (`pcMicrophoneGroup` `:518-562`, `wirePcMicCard` `:308-326`), `AudioDigitalModesPage`,
  `AudioVaxPage` (`VaxChannelCard` with a hidden `DeviceCard` `:265-266`,
  `detectedCablesRescan` `:1524-1531`, `onRescan` `:1408-1428`), `AudioAdvancedPage`,
  `AudioTciPage`, `CoreAudioSetupPage`, `CaptureStatusText.cpp:58`,
  `SoundSystemLine::describe` (`SoundSystemLine.cpp:75-99`). `DeviceCard.{h,cpp}`:
  constructor `DeviceCard(const QString& prefix, Role role, bool enableCheckbox =
  false, QWidget* parent)`; `currentConfig`, `updateNegotiatedPill`, `loadFromSettings`,
  `setEnableAllowed`, `setDetailsExpanded`, `addAboveDevice` / `addBelowDevice`,
  `setGreyedUntilEnabled`, `rescanDevices` (`:602-607`); signal
  `configChanged(AudioDeviceConfig)`; Driver API combo `:322-330` filled at `:656-695`
  from `PortAudioBus::hostApis` / `outputDevicesFor` / `inputDevicesFor`; missing
  devices kept as "(not available)" `:704-719`; WASAPI checkboxes `:449-467`; the
  `deviceDetailsToggle` / `deviceDetails` fold `:304-305`, `:369-384`. Setup
  description version 25 (`SetupDescriptionService.cpp:235-244`). The Audio Setup
  capture pattern: images saved when `NEREUS_AUDIO_SETUP_CAPTURE_DIR` is set
  (`tests/tst_audio_setup_regroup.cpp:305`, `:1524-1528`).
- **VAX.** `src/gui/VaxFirstRunDialog.cpp` (`btnRescanNow` connected to nothing,
  `:647-653`, bug 3); `src/core/audio/VirtualCableDetector.h`;
  `src/core/audio/RemoteVaxFeeder.{h,cpp}`.
- **Header.** `src/gui/widgets/MasterOutputWidget.cpp` (`speakerBtn` `:189-201`,
  tooltip `:194-195`, `onSpeakerContextMenu` `:320-360`, `selectOutputDevice`
  `:362-373`); `MainWindow.cpp:1419-1427`; `TitleBar.cpp:773`, `:779`.
- **TX badge and keying.** `src/gui/applets/TxApplet.cpp` (`m_micSourceBadge`
  `:451-474`, wiring `:1826-1832`, `refreshMicSourceBadge` `:2022-2043`); `RadioModel`
  `onCaptureStatusChanged` (`:21237-21250`), the MOX refusal (`:21154-21162`,
  `TxRefusals::kMicNotReady`), `pcCaptureGatesKeying` (`:21172-21214`),
  `pcCaptureReady` (`:21222-21231`), `updatePcCaptureDemand` (`:25216-25226`).
- **The Core and the link.** `DaemonApp::applyConfigToSettings` (`:903`, `audio_device`
  seeding `:947-971`); `StationServer` `kPeerOnlyProperties` (`:1502-1550`, radio
  speaker entries `:1543-1549`), `fitPeerOnlyProperties` (`:9665-9712`),
  `peerGetsFeatureProperties` (`:9649-9655`), the write refusal (`:1733-1745`, applied
  `:8009-8013`), capabilities (`:13223-13229`); `MirrorPolicy` radio speaker entries
  (`:1204-1214`) and feature gates (`:1274`, `:1327-1334`); `StationClient`
  `m_declaredFeatures` (`:934-938`), capability encode / decode (`:550-555`,
  `:619-627`), `radioSpeakerAvailable` (`:5864-5874`), `applyUpdates` with its
  `InboundGuard` (`:4588`), `applyOne` (`:4781+`); `StationCapabilities.h:615-620`.
  Structured values already cross as JSON text (`logCategoryList` from
  `LogManager::instance().categoryListJson()`). The link document
  `docs/architecture/2026-09-23-station-link-v1.md` (feature text `:981-990`, capability
  tables `:1183` and `:2512`, `radioSpeakerVersion` 109 and `coreBuildInfo` 110); the
  surface manifest `tests/data/link/v1/surface.json` (capabilities end
  `radioSpeakerVersion`, `coreBuildInfo`), rendered by `scripts/render-link-tables.py`
  and regenerated by `tst_link_surface_manifest_regen`.
- **The phone.** `ios/NereusApp/Audio/SoundPanel.swift` (`SoundPanelRadioSpeaker`
  `:146-200`), `RadioSpeakerModel.swift` (`ToolMirrorWatch`,
  `capabilityVersion("radioSpeakerVersion")`, `PropertyWriteQueue`),
  `AppModel.swift:85`, `LinkFeatures.swift:179-202`, tests
  `NereusApp/Tests/SoundPanelTests.swift` (`FakeStation`), `LinkSurfaceOrdinalTests.swift`.
- **Packaging.** `packaging/nereusd.service.in:135-149` (the device access comment);
  `packaging/station-image/common/nereusd-serial.conf` (`[Service]` /
  `SupplementaryGroups=dialout`); `install-station.sh` installs drop-ins into
  `/etc/systemd/system/nereusd.service.d/`; pi-gen `00-run.sh:15-18`; Armbian
  `customize-image.sh:13`; `verify-package.sh`; the station-image README.
- **Build.** `CMakeLists.txt`: PortAudio FetchContent `:526-530`, `PA_USE_ASIO OFF`
  `:532-541` (bug 7's comment), Linux ALSA/JACK `:549-552`, PipeWire
  `pkg_check_modules` `:817-824` linked at `:2436-2449`, Windows links `avrt`
  (`:2367-2372`), Mac links AVFoundation and CoreAudio only, `CORE_SOURCES` audio block
  `:1125-1140`, APPLE sources `:1573-1576`, Linux sources `:1588+`, `NereusCore`
  (`:2060`) gets `NEREUS_BUILD_TESTS` in test builds (`:3318`). libpulse is not
  referenced. CI apt lines to extend: `ci.yml:400`, `:582`; `release.yml:149`, `:407`;
  `codeql.yml:50`; `packaging/station-image/build-trixie-deb.sh:20`.

## File Structure

Create:

| Path | Responsibility |
|---|---|
| `src/core/audio/AudioDelayProbe.{h,cpp}` | Clicker, detector, pairing matcher and summary log for the V-HW-8 delay measurement; `audioProbeNowNs()` |
| `src/core/audio/MatcherRing.h` | The lock-free ring header laid out for shared memory, its static asserts, construct and attach |
| `src/core/audio/DeviceRateMatcher.{h,cpp}` | The rmatchV port: writer (resample, control, sizing, overrun, upslew) and `MatcherReader` (copy, dry-run slew, skip and blend, fade out) |
| `src/core/audio/AudioDelayParts.h` | `AudioDelayParts` and `DeviceRateMatcherStats` |
| `src/core/audio/AudioDeviceTypes.{h,cpp}` | Engine and backend enums, device info, channel pairs, keys, labels |
| `src/core/audio/DeviceSampleFormat.{h,cpp}` | Sample formats a device takes and the conversions to and from float |
| `src/core/audio/AudioTestBarrier.{h,cpp}` | `audioDevicesBarredForTestRun()` |
| `src/core/audio/IAudioEngineBackend.h` | One engine: list, default, notices, stream creation, control panel, rescan |
| `src/core/audio/IAudioDeviceCatalog.h`, `AudioDeviceCatalog.{h,cpp}` | The one source of device lists, on its own thread, debounced |
| `src/core/audio/AudioDeviceMatching.{h,cpp}` | Saved identity matching and the once-only migration |
| `src/core/audio/AudioStreamSupervisor.{h,cpp}`, `IAudioStreamHost.h` | Per-role fallback, return, retry, busy, default following, mic hold while transmitting |
| `src/core/audio/PortAudioBackend.{h,cpp}` | Older drivers as an engine |
| `src/core/audio/AudioBackendRegistry.{h,cpp}` | `makeSystemAudioBackends(const AudioBackendContext&)` per system and process |
| `src/core/audio/CoreAudioSystem.{h,cpp}`, `CoreAudioBackend.{h,cpp}`, `CoreAudioOutputBus.{h,cpp}`, `CoreAudioInputStream.{h,cpp}` | Mac engine behind the `ICoreAudioSystem` seam |
| `src/core/audio/WasapiPolicy.{h,cpp}` | Portable Windows audio decisions (format order, period choice, notice mapping, transport) |
| `src/core/audio/WasapiSystemWin.{h,cpp}`, `WasapiBackendWin.{h,cpp}`, `WasapiOutputBusWin.{h,cpp}`, `WasapiInputStreamWin.{h,cpp}` | Windows adapter, built only on Windows |
| `src/core/audio/PipeWireDeviceBackend.{h,cpp}`, `PipeWireDeviceSystem.{h,cpp}` | PipeWire engine with registry listener |
| `src/core/audio/PulseAudioBackend.{h,cpp}`, `PulseAudioSystem.{h,cpp}`, `PulseAudioBus.{h,cpp}` | PulseAudio engine |
| `src/core/audio/LinuxEngineSelection.{h,cpp}` | Which of PipeWire and PulseAudio runs (R-AUD-31) |
| `src/core/audio/AlsaDirectBackend.{h,cpp}`, `AlsaDirectSystem.{h,cpp}`, `AlsaDirectBus.{h,cpp}` | Core engine with inotify on `/dev/snd` |
| `src/core/audio/CaptureShm.{h,cpp}` | Shared-memory ring and wake signal for the helper (POSIX and Windows) |
| `src/core/audio/AsioSession.{h,cpp}`, `IAsioDriver.h` | Portable ASIO rules (one driver, shared buffer and rate, reset, format support) |
| `src/core/audio/AsioBackend.{h,cpp}` | The window's view of ASIO, through the mic helper |
| `src/core/audio/AsioDriverWin.{h,cpp}`, `cmasio.{h,cpp}` | Windows ASIO adapter and the cmASIO port, built only on Windows |
| `src/core/audio/CoreSpeakerJson.{h,cpp}` | JSON forms of the Core speaker properties |
| `src/gui/setup/AudioDriverList.{h,cpp}` | Builds a card's Driver list and device entries from the catalogue |
| `src/gui/setup/AsioSwitchAllDialog.{h,cpp}` | "One ASIO driver at a time" prompt |
| `src/gui/setup/CoreSpeakerCard.{h,cpp}` | The Core speaker card on Outputs |
| `third_party/asiosdk/` | The ASIO SDK 2.3.4 as released, with `LICENSE.txt`, `VERSION.txt`, `CMakeLists.txt` |
| `docs/attribution/ASIO-SDK-PROVENANCE.md` | Where the SDK came from and its licence |
| `packaging/station-image/common/nereusd-audio.conf` | `[Service]` / `SupplementaryGroups=audio` |
| `tests/fakes/FakeAudioEngineBackend.h`, `tests/fakes/FakeMatcherAudioBus.h` | Fake engine and fake device bus with a virtual clock |
| `tests/fakes/FakeCoreAudioSystem.h`, `tests/fakes/FakeAsioDriver.h` | Fakes behind the Core Audio and ASIO seams |
| `ios/NereusApp/Audio/CoreSpeakerModel.swift` | The phone's Core speaker level, mute and state |
| Tests | `tst_audio_delay_probe`, `tst_device_rate_matcher`, `tst_device_rate_matcher_threads`, `tst_audio_device_catalog`, `tst_audio_device_identity`, `tst_audio_device_migration`, `tst_audio_stream_supervisor`, `tst_audio_engine_matcher_push`, `tst_port_audio_backend`, `tst_audio_engine_native_routing`, `tst_core_audio_backend`, `tst_wasapi_policy`, `tst_pipewire_device_backend`, `tst_pulse_audio_backend`, `tst_linux_engine_selection`, `tst_alsa_direct_backend`, `tst_capture_shm_ring`, `tst_asio_session`, `tst_audio_driver_list`, `tst_device_card_pairs_asio`, `tst_core_speaker_model`, `tst_core_speaker_link`, `tst_core_speaker_card` |

Modify:

| Path | Change |
|---|---|
| `src/core/IAudioBus.h` | Stream events, delay parts, stereo-mix matcher hooks (default no-ops) |
| `src/core/AudioEngine.{h,cpp}` | Engine choice through the registry, supervisor, catalogue, remote into the matcher, delay readout, probe hook |
| `src/core/AudioDeviceConfig.{h,cpp}` | `engine`, `deviceId`, `firstChannel`, `micChannel`, `delayMs`, new keys |
| `src/core/audio/PortAudioBus.{h,cpp}` | Matcher replaces the output ring; matching within the saved host API (bug 1); `Q_OS_MAC` (bug 6) |
| `src/core/audio/CaptureHelper.{h,cpp}`, `CaptureSupervisor.{h,cpp}`, `CaptureProtocol.h`, `CaptureAudioBus.{h,cpp}`, `src/capture_main.cpp` | Probe records, shared-memory hand-off, native engines and ASIO in the helper |
| `src/core/audio/RealtimeAudioPriority.{h,cpp}`, `src/models/RxDspWorker.cpp` | Workgroup follow on the Mac |
| `src/core/session/media/RemoteAudioReceiver.{h,cpp}`, `src/core/audio/RemoteVaxFeeder.{h,cpp}` | Write into the device matcher when the bus takes a stereo mix |
| `src/core/audio/LinuxAudioBackend.{h,cpp}` | Detection moves into `LinuxEngineSelection` |
| `src/core/audio/PipeWireStream.{h,cpp}` | Matcher output mode, input sink mode, audio position and no-remix |
| `src/core/daemon/DaemonApp.cpp` | Backend context for the Core, the Core speaker host, the desktop rule |
| `src/core/audio/VirtualCableDetector.{h,cpp}` | Reads the catalogue |
| `src/models/RadioModel.{h,cpp}` | Core speaker properties, role status for the badge and tooltips |
| `src/core/session/*` link files, `IStationLink.h` | `coreSpeaker` feature, `coreSpeakerVersion` capability, mirroring, availability |
| `src/gui/setup/DeviceCard.{h,cpp}`, `AudioOutputsPage`, `AudioTxInputPage`, `AudioDigitalModesPage`, `AudioVaxPage`, `SoundSystemLine`, `CaptureStatusText` | Driver list, live lists, states, delay line, pairs, ASIO details, Rescan |
| `src/gui/VaxFirstRunDialog.cpp` | Catalogue lists, "Rescan now" wired |
| `src/gui/widgets/MasterOutputWidget.cpp` | Header menu option A and tooltip |
| `src/gui/applets/TxApplet.cpp` | Badge states |
| `src/main.cpp` | `--audio-delay-probe` |
| `CMakeLists.txt`, `tests/CMakeLists.txt` | Sources, libraries, the ASIO SDK, tests |
| `.github/workflows/ci.yml`, `release.yml`, `codeql.yml`, `packaging/station-image/build-trixie-deb.sh` | `libpulse-dev` |
| `packaging/station-image/...` installers and images, `scripts/pi4/install-core-pi4.sh`, `scripts/pi4/upgrade-core-pi4.sh`, `packaging/nereusd.service.in` | The audio drop-in |
| `ios/NereusApp/Audio/SoundPanel.swift`, `ios/NereusApp/App/AppModel.swift`, `LinkFeatures.swift`, tests | Phone Core speaker section |
| Docs | Link document, surface manifest, `docs/attribution/THETIS-PROVENANCE.md`, `packaging/third-party-licenses/README.md`, remote-controls rows, `docs/manual/*`, `README.md`, `docs/architecture/overview.md`, CHANGELOG, project status |

## Before Task 1

The controller does these in order, before dispatching anything.

1. **Flags for an earlier review (JJ decides).** Core RX audio path: Tasks 1, 2, 6, 7,
   8, 9, 10, 11, 12. Mic and transmit input, which feeds R-R3-36's keying gate: Tasks
   13 and 15. The station link: Task 21. Device access on the Core (packaging): Task
   24. Recommendation to JJ: one independent review of Tasks 2, 6 and 7 together after
   Task 7 lands, because the matcher and its wiring carry every speaker's audio; the
   rest wait for the whole-branch review.
2. **Order.** Tasks run in number order. Tasks 8 to 12 depend on 2 to 7; 13 on 2, 3, 5
   and 8 to 11; 15 on 13 and 14; 16 to 20 on 3 to 7; 21 to 23 on 7 and 12; 24 and 25
   last. Nothing runs in parallel unless JJ asks for speed.
3. **Pre-flight permissions, one question at a time, each with a recommendation:**
   - the ASIO SDK 2.3.4 download from Steinberg for Task 14 (name, source, size);
   - `libpulse-dev` in a derived container image for Task 11 (else PulseAudio compiles
     in CI only);
   - a deps cache refresh, only if `offline-deps.py check` fails;
   - a branch push after Task 15, so Windows CI compiles Tasks 9 and 15.
4. **Linux lane setup.** Create the `nereus-native-audio-linux` volume and configure
   `/work/build` once:
   `python3 /deps/offline-deps.py extract /deps /src /work/build /work/offline-args.txt; mapfile -t offline_args < /work/offline-args.txt; cmake -S /src -B /work/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON -DENABLE_DFNR=OFF "${offline_args[@]}"`.
5. **The V-HW-8 baseline.** After Task 1 lands, JJ measures the delay on today's engines
   with `--audio-delay-probe` (Mac, the Windows PC with MME, WASAPI shared and
   exclusive checkbox states, and the PipeWire desktop). Those numbers are the "before"
   of R-AUD-15. The plan carries on without waiting for them.
6. **Settled calls for JJ's veto (36).** These are the plan's own choices inside the
   spec's scope; each can be vetoed before Task 1 and none needs an answer to proceed.
   1. The engine key strings and labels in Global Constraints.
   2. `FirstChannel` is 1-based.
   3. The delay setting is the matcher's target fill; its ring holds twice that
      (rmatchV's `rsize`).
   4. Automatic sizing: start at the smallest step at or above the device callback
      plus one write block; after the 3 s start-up, raise to the smallest step at or
      above the callback plus the largest gap seen between writes; one step up on each
      dry run; never down while the stream is open.
   5. A name match saves the matched ID back; with two same-name devices and no ID
      match, the first in the system's order is taken.
   6. Default device changes are not debounced; device list changes are (500 ms).
   7. A Bluetooth mic picked by name opens before the outputs; the outputs wait up to
      2 s for it.
   8. A mic on "(platform default)" whose system default is Bluetooth uses the
      built-in mic, else another input that is not Bluetooth, else stays silent.
   9. The Linux Core (nereusd) registers only ALSA direct; on the Mac and Windows the
      Core uses the desktop engines.
   10. The Core's default card is ALSA's `defaults.pcm.card` when set, else the
       lowest-numbered card with playback.
   11. On a desktop Core box, `audio_device` in the config file counts as a pick.
   12. "(none)" is saved as `DeviceId` `(none)` and stays in the list.
   13. "Starts into a desktop" is read from the `default.target` symlink, with no
       `systemctl` call.
   14. ALSA format order S32_LE, S24_3LE, S24_LE, S16_LE; Windows exclusive order
       float32, int32, 24-in-32, int16.
   15. Structured Core speaker values cross the link as JSON text.
   16. ASIO host code is written against the SDK 2.3.4 host sample; Thetis's
       `hostsample.cpp` is a reference only.
   17. A driver switch keeps each device's pair numbers where the new driver has them,
       else its first pair.
   18. The helper describes an ASIO driver's channels on request, loading an idle
       driver briefly.
   19. The probe measures through the PC mic path, timed with `steady_clock` in both
       processes.
   20. Probe constants: a 48-frame click at 0.5 on every channel every 48000 frames;
       threshold the larger of 0.02 and 8 times the RMS of the last 100 ms; 500 ms
       hold-off; 500 ms pairing window; a summary every 30 clicks.
   21. PulseAudio counts as running when a context connects within 1 s to a server
       whose name is not PipeWire.
   22. An odd last channel is listed alone ("Output 5").
   23. With neither PipeWire nor PulseAudio running, migration is postponed (tried
       again next start) and the older drivers are used meanwhile.
   24. Older drivers listed: MME, DirectSound, WDM-KS on Windows; JACK, ALSA on Linux;
       PortAudio's own WASAPI and Core Audio entries are not offered; one "Older
       drivers" heading.
   25. "Both" adds the two channels into both, as Thetis's `combinebuff` does (`ivac.c:694-698`).
   26. An explicit MME, DirectSound, WDM-KS or JACK choice stays under Older drivers;
       only an empty Driver API moves (R-AUD-05 row 1).
   27. ALSA with an empty name, `default`, `pulse` or `pipewire` moves to the running
       native engine on "(platform default)".
   28. An ASIO driver whose sample format NereusSDR does not convert shows
       "<driver> uses a sample format NereusSDR can't play or record." with its pairs
       greyed. Converted: Int16LSB, Int24LSB, Int32LSB, Float32LSB, Float64LSB.
   29. The Core speaker plays at 50 when no master level was ever saved, as today.
   30. Remote audio written into a device matcher is released by the jitter hold
       alone, as R-R3-43's sink mode is.
   31. The mic helper's input thread is already in its device's workgroup (it is the
       AUHAL IO thread), so the cross-process workgroup port the spec mentions is not
       used; V-HW-9 measures the result.
   32. `MicChannel` defaults to `Left`, which is what today's one-channel open records
       (`CaptureHelper.cpp:400`).
   33. On the Linux Core every saved speaker choice moves to ALSA direct, matched by
       name with the `(hw:C,D)` allowance, because ALSA direct is the Core's only engine.
   34. The Pi 4 and Pi 5 install and upgrade scripts add the audio drop-in too, not
       only the station images.
   35. A busy device uses the not-connected sentences with "is in use by another
       program" in place of "is not connected", word for word (R-AUD-11); the transmit
       badge reads "PC mic in use by another program".
   36. The delay line names the device, as the spec writes it ("Now 12 ms from the
       radio to <name>", the mic "Now 12 ms from <name> to the radio"), not the
       mockup's "these speakers".
   Also: on Windows, Bluetooth is read from the endpoint's enumerator name
   (`PKEY_Device_EnumeratorName` `BTHENUM` or `BTHHFENUM`), pending the bench.


## Task 1: Delay probe on today's engines

**Requirements:** V-HW-8 ("Taken on the build before this change (with the hook added
first) and after"). Spec sections "Expected delay" and "Verification".

**Files:**
- Create: `src/core/audio/AudioDelayProbe.{h,cpp}`, `tests/tst_audio_delay_probe.cpp`
- Modify: `src/core/AudioEngine.{h,cpp}` (clicker on the speakers push, pairing,
  summary log, a `TestMic` capture lease while the probe runs),
  `src/core/audio/PortAudioBus.{h,cpp}` (an optional input-callback hook for the
  detector), `src/core/audio/CaptureProtocol.{h,cpp}` (version 2, two records),
  `src/core/audio/CaptureHelper.cpp` (runs the detector, sends hits),
  `src/core/audio/CaptureSupervisor.{h,cpp}` (sends `ProbeEnable`, forwards hits),
  `src/main.cpp` (`--audio-delay-probe`), `CMakeLists.txt`, `tests/CMakeLists.txt`,
  `tests/tst_capture_protocol.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces (`AudioDelayProbe.h`, namespace `NereusSDR`):
  ```cpp
  std::int64_t audioProbeNowNs();   // std::chrono::steady_clock ns, the clock CaptureProtocol.h:92 already uses in both processes

  class AudioDelayProbeClicker {    // DSP thread only
  public:
      static constexpr int kClickFrames = 48;
      static constexpr float kClickLevel = 0.5f;
      static constexpr int kIntervalFrames = 48000;
      // Adds the click into an interleaved float block in place. Returns true when a
      // click starts at frame 0 of this block (the caller stamps the push time).
      bool process(float* interleaved, int frames, int channels);
  };

  class AudioDelayProbeDetector {   // device callback only; no lock, no allocation
  public:
      static constexpr float kMinThreshold = 0.02f;
      static constexpr float kRmsFactor = 8.0f;
      static constexpr int kRmsWindowMs = 100;
      static constexpr int kHoldOffMs = 500;
      explicit AudioDelayProbeDetector(int sampleRate);
      // captureNsOfFrame0: when frame 0 of this buffer reached the converter.
      // Returns the capture time of a detected click, else std::nullopt.
      std::optional<std::int64_t> process(const float* mono, int frames, std::int64_t captureNsOfFrame0);
  };

  class AudioDelayProbeMatcher {    // thread-safe
  public:
      static constexpr std::int64_t kPairWindowNs = 500'000'000;
      static constexpr int kSummaryEvery = 30;
      void addClick(std::int64_t clickNs);
      void addHit(std::int64_t captureNs);          // pairs with the latest click within the window
      void setReadoutMs(std::optional<double> ms);  // Task 6 feeds it
      // The log line once every kSummaryEvery pairs, else empty.
      QString takeSummary();
  };
  ```
- Produces (`CaptureProtocol.h`): `kVersion = 2`; `RecordType::ProbeHit = 4` (helper
  to window, JSON payload `{"captureNs":"<decimal string>"}`, a string so the 64-bit
  value is exact) and `RecordType::ProbeEnable = 20` (window to helper, JSON payload
  `{"enabled":true}` or `{"enabled":false}`), in the file's existing JSON style (exact
  key set, `kMaxJsonBytes` bound, `encodeX` returns a complete record):
  `QByteArray encodeProbeHit(std::int64_t captureNs)`,
  `std::optional<std::int64_t> decodeProbeHit(const QByteArray& json)`,
  `QByteArray encodeProbeEnable(bool enabled)`,
  `std::optional<bool> decodeProbeEnable(const QByteArray& json)`. `RecordReader`
  accepts types 4 and 20 (today they give `UnknownType`).
- Produces: `AudioEngine::setDelayProbeEnabled(bool)` (main thread) and the
  `--audio-delay-probe` flag (no argument) that calls it after start.

**Acceptance:**
- Clicker: blocks of 480 frames, 2 channels: the first click starts at frame 0 of the
  first block whose start is at least 48000 frames after the previous click start (the
  very first click at the first block); 48 frames on both channels are `+0.5f` added to
  what was there; a click that starts in a 32-frame block continues for 16 frames into
  the next block; `process` returns true only for the block a click starts in.
- Detector at 48 kHz: noise at RMS 0.001 with a 0.5 step at frame 1000 of a buffer whose
  frame 0 is at 1,000,000,000 ns returns 1,000,000,000 + 1000 × 1e9 / 48000 ns
  (± one frame); a second step 200 ms later returns nothing (hold-off); one 600 ms later
  returns a hit; with a background tone at RMS 0.1 the threshold is 0.8, so a 0.5 click
  returns nothing.
- Matcher: a hit 23.0 ms after a click pairs as 23.0 ms; a hit 600 ms after the last
  click is dropped; after 30 pairs of 20, 21 and 22 ms (10 each) the summary is exactly
  `Audio delay probe: median 21.0 ms, min 20.0, max 22.0 over 30 clicks; readout not available`,
  and with `setReadoutMs(19.5)` it ends `; readout 19.5 ms` in place of
  `; readout not available`. It is logged with `qCInfo(lcAudio)`.
- The click goes only into the speakers block, after every gain and mute, just before
  the speakers push (`AudioEngine.cpp:2738-2760`); headphones, VAX, the radio codec tap,
  TCI and remote feeds never carry it. With the probe off the speakers block is
  bit-identical to today (`tst_audio_engine_speaker_format` stays green).
- Capture time in the helper's PortAudio input callback: with `inputBufferAdcTime`
  nonzero, `nowNs − (currentTime − inputBufferAdcTime) × 1e9`; otherwise
  `nowNs − frames × 1e9 / rate − inputLatency × 1e9` (the stream's reported input
  latency). The click time is `audioProbeNowNs()` taken just before the speakers push.
- Protocol: both records round-trip (`captureNs` −1, 0 and 9223372036854775807
  exactly); `{"captureNs":12}` (a number, not a string), `{"captureNs":"1x"}`, an extra
  key and a missing key each decode to nullopt; `RecordReader` reads types 4 and 20 and
  still gives `UnknownType` for 5; an unknown version is still rejected; a helper of
  version 1 is a `ProtocolError` as today.
- While the probe runs, the engine holds a `CaptureSupervisor::Demand::TestMic` lease
  and sends `ProbeEnable` enabled once the helper is Ready; turning it off sends disabled and
  releases the lease.

**Verification:** a measurement hook on the core RX path, ordinary feature: test the
contract first. Unit:
`cmake --build build --target tst_audio_delay_probe tst_capture_protocol tst_audio_engine_speaker_format tst_capture_supervisor && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_audio_delay_probe|tst_capture_protocol|tst_audio_engine_speaker_format|tst_capture_supervisor)$' --no-tests=error --output-on-failure`.
Hardware (pending, JJ): the V-HW-8 baseline, run with a receiving radio, the slice AF
turned down and a loopback cable from a speaker output into an input of the same
interface, on the Mac, the Windows PC (nothing picked, WASAPI) and the PipeWire desktop.

**Execution note (advisory):** opus. Touches the core RX path (the speakers block) and
the mic helper protocol; flagged. No prerequisites.

- [ ] **Step 1:** Tests for the clicker, detector, matcher and the two records, with the
  exact values above.
- [ ] **Step 2:** Implement the probe classes, the protocol records, the helper hook,
  the engine wiring and the flag; run the unit command; commit (V-HW-8).

## Task 2: The clock matcher

**Requirements:** R-AUD-15 (matcher part: "The device's callback reads it directly,
taking no lock and allocating nothing, and the two sides of the matcher never move each
other's index"), D2, D7, D34, V-SW-5. Spec "Clock matching and the delay readout".

**Files:**
- Create: `src/core/audio/MatcherRing.h`, `src/core/audio/DeviceRateMatcher.{h,cpp}`,
  `src/core/audio/AudioDelayParts.h`, `tests/tst_device_rate_matcher.cpp`,
  `tests/tst_device_rate_matcher_threads.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`,
  `docs/attribution/THETIS-PROVENANCE.md` (row for `DeviceRateMatcher.cpp`, Type `port`,
  Variant `thetis-no-samphire`, sources `Project Files/Source/wdsp/rmatch.c` lines
  20-125, 128-171, 256-298, 301-467, 501-527 and `varsamp.c` 41-60)

**Interfaces:**
- Consumes: nothing.
- Produces (`AudioDelayParts.h`):
  ```cpp
  struct AudioDelayParts {
      double matcherFillMs = -1.0;   // -1: no matcher on this stream
      double resamplerMs = 0.0;
      double deviceBufferMs = 0.0;
      double deviceLatencyMs = 0.0;
      double totalMs() const;        // the sum of the four, or -1 when matcherFillMs < 0
  };
  struct DeviceRateMatcherStats {
      std::uint64_t dryRuns = 0;
      std::uint64_t overruns = 0;
      double ratio = 1.0;
      double fillFrames = 0.0;
      int rsizeFrames = 0;
      int capacityFrames = 0;
      int delayStepMs = 0;
      bool controlActive = false;
  };
  ```
- Produces (`MatcherRing.h`): `kMatcherRingMagic = 0x4E524D52u`,
  `kMatcherNoSkip = std::numeric_limits<std::uint64_t>::max()`, and
  `struct MatcherRingHeader` (standard layout, built in place with `std::construct_at`,
  usable in shared memory) with plain `magic`, `capacityFrames` (a power of two),
  `channels` (2), `slewFrames` (ntslew), and `std::atomic` members `written`, `read`,
  `requested`, `readCalls`, `skipTo`, `dryRuns`, `overruns` (all `uint64_t`),
  `lastWriteNs` (`int64_t`), `upslewPending` (`uint32_t`), `rsizeFrames` (`uint32_t`),
  `ratio` and `fillFrames` (`double`); `static_assert(std::atomic<T>::is_always_lock_free)`
  for every atomic type used, `static_assert(std::is_standard_layout_v<MatcherRingHeader>)`.
  The float frames follow the header. Functions:
  `std::size_t matcherRingBytes(std::uint32_t capacityFrames, std::uint32_t channels)`,
  `MatcherRingHeader* constructMatcherRing(void* memory, std::uint32_t capacityFrames, std::uint32_t channels, std::uint32_t slewFrames)`,
  `MatcherRingHeader* attachMatcherRing(void* memory, std::size_t bytes)` (nullptr
  unless the magic, channel count and sizes agree with `bytes`).
- Produces (`DeviceRateMatcher.h`):
  ```cpp
  class MatcherReader {              // the device side; one reader per ring
  public:
      MatcherReader() = default;
      explicit MatcherReader(MatcherRingHeader* ring);   // allocates its slew buffer here, never in read()
      bool valid() const;
      void read(float* interleaved, int frames);         // no lock, no allocation, no system call
      void requestFadeOut();                             // any thread
      bool fadedOut() const;                             // any thread
  };

  class DeviceRateMatcher {          // the DSP side; one writer thread at a time
  public:
      static constexpr std::array<int, 6> kDelayStepsMs{2, 3, 5, 10, 20, 40};
      struct Config {
          int inRate = 48000;
          int outRate = 48000;
          int writeBlockFrames = 64;    // xvarsamp's fixed size
          int callbackFrames = 128;     // the device callback
          int delayMs = 0;              // 0 automatic, else one of kDelayStepsMs
      };
      static std::size_t ringBytes(const Config& config);
      explicit DeviceRateMatcher(const Config& config);                        // owns its ring
      DeviceRateMatcher(const Config& config, void* memory, std::size_t bytes); // ring in the caller's memory
      ~DeviceRateMatcher();
      void write(const float* interleavedStereo, int frames, std::int64_t nowNs); // writer thread
      void requestFlush();     // any thread; the writer drops what is queued at its next write
      void requestRestart();   // any thread; the writer restarts the control at its next write
      MatcherReader makeReader();
      MatcherRingHeader* ring();
      double ratio() const;
      int delayStepMs() const;
      double fillFrames() const;
      int resamplerDelayFrames() const;
      AudioDelayParts delayParts(double deviceBufferMs, double deviceLatencyMs) const;
      DeviceRateMatcherStats stats() const;
      // Tests only (NEREUS_BUILD_TESTS): the matcher plays at a fixed ratio with control off.
      void forceRatioForTest(std::optional<double> ratio);
  };
  ```
  Without `HAVE_WDSP` the matcher copies, and its constructor leaves it invalid
  (`ringBytes` still works) when `inRate != outRate`; callers treat an invalid matcher
  as an open failure.

**Acceptance:** the port follows `rmatch.c` with these rules, which a test or a code read
can check one by one.
- Header and cites: `DeviceRateMatcher.cpp` carries the `rmatch.c` header byte-for-byte
  (Warren Pratt, NR0V, "Copyright (C) 2017, 2018, 2022") and the `varsamp.c` header,
  under `// --- From rmatch.c ---` and `// --- From varsamp.c ---`, with a Modification
  history block; every ported block carries
  `// From Thetis Project Files/Source/wdsp/rmatch.c:<lines> [v2.10.3.15 @3759d09]`; the
  inline comments of the ported code are kept, including the commented-out lines
  `// a->n_ring = a->rsize / 2;`, `// a->iout = (a->iout + ovfl + a->rsize / 2) % a->rsize;`,
  `// zeros = a->outsize + a->rsize / 2 - n;`, `// a->n_ring = a->outsize + a->rsize / 2;`
  and `// a->iin = (a->iout + a->outsize + a->rsize/2) % a->rsize;` beside the lines that
  replace them.
- Constants (named `constexpr`, cited to `create_rmatchV` `:501-527` and `calc_rmatch`):
  fc_high 0.0, fc_low −1.0, gain 1.0, start-up 3.0 s, R 1024, feed-forward mav 4096 /
  262144, ff_alpha 0.01, proportional mav 4096 / 16384, prop_gain 4.0e-06, varmode 1,
  tslew 0.003 s, initial var 1.0; `pr_gain = prop_gain × 48000 / outRate`; var clamp
  0.96 to 1.04; `maxNewsamps = (int)(1.0 + writeBlockFrames × (1.05 × outRate / inRate))`;
  `ntslew = (int)(0.003 × outRate)`, capped at `rsizeMin / 2 − 1` where rsizeMin is the
  rsize at the 2 ms step, fixed for the life of the stream; `cslew[m] = 0.5 × (1 − cos(m × π / ntslew))`
  for m = 0..ntslew. `mav` and `aamav` are ported as written (`rmatch.c:20-125`,
  including aamav's separate positive and negative sums).
- Sizes: target fill = `delayStepMs × outRate / 1000` frames; `rsize = max(2 × target, 2 × maxNewsamps, 2 × callbackFrames)`;
  capacity = the next power of two at or above the rsize of the 40 ms step plus
  `maxNewsamps`, allocated at construction. The ring starts holding `rsize / 2` frames
  of silence.
- Writer: input is staged into `writeBlockFrames` chunks (xvarsamp's size is fixed at
  create); each chunk is resampled with `xvarsamp(v, var)` at the last var; when an
  upslew is pending (`upslewPending` exchanged to 0) the next `ntslew + 1` written frames
  are multiplied by `cslew[ntslew − ucnt]` as `upslew` does; frames are copied in and
  `written` is published with release order, then `lastWriteNs = nowNs`.
- Control feed (writer only): before a chunk's own feed, it replays the reads since its
  last write: with k = the `readCalls` delta, R = the `requested` delta and D = the
  `read` delta, for i = 1..k it feeds `change = −(R / k)` (the remainder on the last)
  with `deviation = (writtenAtLast − (readAtLast + D × i / k)) − rsize / 2`; then for
  its own chunk `change = +writeBlockFrames` with `deviation = (written − effectiveRead) − rsize / 2`.
  `control()` is otherwise rmatch's: `xaamav`, the feed-forward smoothing,
  `xmav(deviation)`, `var = feed_forward − pr_gain × av_deviation`, clamped. It starts
  only after 3 s of reads (`requested` ≥ 3 × outRate) and 3 s of writes (input frames
  ≥ 3 × inRate), as `control_flag` does. The ratio is published to `ratio`.
- Reader: copies `min(frames, written − read)` frames with acquire order and publishes
  `read`, then adds `frames` to `requested` and 1 to `readCalls`. It takes no lock,
  allocates nothing and never writes the ring's frames or `written`.
- Dry run (fewer frames than asked): the reader slews the last `min(available, ntslew + 1)`
  frames down along `cslew` as `dslew` does, continuing from the last frame value
  (`dlast`) when fewer than `ntslew + 1` are there, a tail that may continue into later
  reads; the rest is silence; it counts `dryRuns` and sets `upslewPending`.
- Overrun (`fill + newsamps > rsize` before a write): the writer counts `overruns` and
  publishes `skipTo = written − rsize` (after the write lands); the reader, at its next
  read, saves the `ntslew + 1` frames it would have played next, jumps `read` to
  `skipTo` (consuming the request with a compare-exchange back to `kMatcherNoSkip`) and
  crossfades its output from the saved frames to the new ones over `cslew` as `blend`
  does. The writer measures fill against the pending `skipTo` when one is unconsumed.
- Stalled reader (a write would pass the physical capacity): the block is dropped,
  `overruns` counts, `skipTo = written − rsize / 2`; when reads resume, the writer
  restarts the control (mavs flushed, var 1.0, the 3 s wait again).
- Automatic size: starts at the smallest step at or above `callbackFrames + writeBlockFrames`
  in milliseconds; when the control starts, raises to the smallest step at or above the
  callback period plus the largest gap between two `write` calls seen so far (the fill
  then follows the control, with no padding); on each dry run steps up one and pads
  silence up to the new `rsize / 2`; never steps down; at 40 ms it stays. A manual
  `delayMs` fixes the size.
- `requestFlush`: at the next write the writer publishes `skipTo = written` before
  writing. `requestRestart`: at the next write the writer restarts the control as above
  and drops what is queued. `MatcherReader::requestFadeOut`: the next read slews to
  silence over `ntslew` frames, then reads only silence; `fadedOut()` turns true after
  that read.
- Readout: `fillFrames` is `rsize / 2 + av_deviation` while the control runs, else
  `written − read`; `resamplerDelayFrames()` is `(int)(140.0 × norm_rate / min_rate) / 2 − 1`
  as `RemoteAudioRateMatcher.cpp:213-221` computes it (69 at 48 kHz in and out);
  `delayParts()` returns fill and resampler in ms at outRate plus the two device parts
  given, and `totalMs()` equals their sum.
- V-SW-5 (deterministic, simulated time, no threads): a writer clock at 48000 Hz and a
  reader clock 200 ppm fast, then 200 ppm slow, with 64-frame writes and 128-frame
  reads, for 10 simulated minutes each: no dry run after the automatic size settles
  (after the first 10 s), no overrun after 10 s, and the ratio over the last minute is
  within 10 ppm of the ratio WDSP's own rmatchV (`create_rmatchV(64, 128, 48000, 48000, 0, 1.0)`
  through the `extern "C"` declarations) reaches fed the same clocks. A forced dry run
  (the reader skips 50 ms of calls) steps the size up exactly once. On a 1 kHz sine at
  0.5, a forced dry run and a forced overrun (the writer delivers 100 ms at once) leave
  no sample-to-sample step larger than twice the sine's own largest step
  (2 × 0.5 × 2π × 1000 / 48000). `delayParts().totalMs()` equals the sum of its parts.
- V-SW-5 (threads, `REALTIME`): a writer thread and a reader thread running 60 s at a
  ±200 ppm clock offset produce no torn frames (a ramp's continuity holds outside dry
  runs and overruns); a test hook counts no allocation on the reader thread (global
  `operator new` / `operator delete` replaced in the test binary, counting per thread)
  and, on Linux, no `pthread_mutex_lock`, `pthread_mutex_trylock`, `malloc`, `calloc`,
  `realloc` or `free` (interposed with `dlsym(RTLD_NEXT, ...)`) while the reader runs.
  Use `tests/RealtimeTestLoad.h` as the other real-time tests do.

**Verification:** the audio path every local output will use, invariant coverage first.
Unit (Mac):
`cmake --build build --target tst_device_rate_matcher tst_device_rate_matcher_threads && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_device_rate_matcher|tst_device_rate_matcher_threads)$' --no-tests=error --output-on-failure`.
Linux lane: the same two targets (the interposition test runs only there). Also
`python3 scripts/verify-inline-tag-preservation.py`, `python3 scripts/verify-thetis-headers.py`
and `python3 scripts/verify-provenance-sync.py` pass.

**Execution note (advisory):** opus. Core RX path; flagged (recommended for the early
review with Tasks 6 and 7). No prerequisites; could run beside Task 1 in a worktree.

- [ ] **Step 1:** The deterministic V-SW-5 tests and the readout test, against the
  rmatchV reference.
- [ ] **Step 2:** The ring, the writer and the reader, ported with headers, cites and
  comments; the provenance row; the deterministic tests green; commit (R-AUD-15).
- [ ] **Step 3:** The threaded test with the no-allocation and no-lock hooks; Mac and
  Linux lane green; commit (R-AUD-15, V-SW-5).

## Task 3: Device types, engines and the catalogue

**Requirements:** R-AUD-03 ("Every device list ... shows a device added or removed
within 1 s of the system reporting it, without Rescan, and without interrupting any
stream on another device"), R-AUD-07 (the pair model), R-AUD-32, V-SW-1 (lists part),
V-SW-10. Spec "The device catalogue".

**Files:**
- Create: `src/core/audio/AudioDeviceTypes.{h,cpp}`, `src/core/audio/AudioTestBarrier.{h,cpp}`,
  `src/core/audio/DeviceSampleFormat.{h,cpp}`, `src/core/audio/IAudioEngineBackend.h`,
  `src/core/audio/IAudioDeviceCatalog.h`, `src/core/audio/AudioDeviceCatalog.{h,cpp}`,
  `tests/fakes/FakeAudioEngineBackend.h`, `tests/fakes/FakeMatcherAudioBus.h`,
  `tests/tst_audio_device_catalog.cpp`
- Modify: `src/core/IAudioBus.h` (default no-op additions),
  `src/core/audio/PortAudioBus.cpp` (`portAudioBarredForTestRun` calls
  `audioDevicesBarredForTestRun`), `CMakeLists.txt`, `tests/CMakeLists.txt`,
  `docs/attribution/THETIS-PROVENANCE.md` (row for `DeviceSampleFormat.cpp`, sources
  `Project Files/Source/ChannelMaster/cmasio.c` lines 148-200 and `ivac.c` 694-698)

**Interfaces:**
- Consumes: Task 2 `AudioDelayParts`, `DeviceRateMatcherStats`, `MatcherReader`.
- Produces (`AudioDeviceTypes.h`):
  ```cpp
  enum class AudioEngineKind { PortAudio, CoreAudio, WindowsShared, WindowsExclusive, Asio, PipeWire, PulseAudio, AlsaDirect };
  enum class AudioBackendId { PortAudio, CoreAudio, Wasapi, Asio, PipeWire, PulseAudio, AlsaDirect };
  enum class AudioDeviceDirection { Output, Input };
  enum class AudioTransport { Unknown, BuiltIn, Usb, Bluetooth, Hdmi, Virtual };
  enum class AudioDeviceState { Present, NotConnected, InUse };
  struct AudioChannelPair { int firstChannel = 1; int channelCount = 2; friend bool operator==(const AudioChannelPair&, const AudioChannelPair&) = default; };
  struct AudioDeviceInfo {
      AudioBackendId backend = AudioBackendId::PortAudio;
      AudioDeviceDirection direction = AudioDeviceDirection::Output;
      QString id;          // the engine's saved identity (spec "Saved identity")
      QString name;        // display name
      QString hostApi;     // older drivers only: PortAudio's host API name
      AudioTransport transport = AudioTransport::Unknown;
      AudioDeviceState state = AudioDeviceState::Present;
      int channelCount = 2;
      int alsaCard = -1;   // where the engine reports them (Linux)
      int alsaDevice = -1;
      bool isDefault = false;
      friend bool operator==(const AudioDeviceInfo&, const AudioDeviceInfo&) = default;
  };
  QString audioEngineKey(AudioEngineKind);
  std::optional<AudioEngineKind> audioEngineFromKey(const QString& key);
  QString audioEngineLabel(AudioEngineKind);
  AudioBackendId audioBackendFor(AudioEngineKind);          // both Windows kinds map to Wasapi
  QList<AudioChannelPair> audioChannelPairs(int channelCount);
  QString audioPairLabel(AudioDeviceDirection, const AudioChannelPair&);
  QString audioDeviceEntryLabel(const AudioDeviceInfo&, const AudioChannelPair&);
  enum class MicChannelPick { Left, Right, Both };   // key MicChannel "Left", "Right", "Both"
  QString micChannelKey(MicChannelPick);
  std::optional<MicChannelPick> micChannelFromKey(const QString& key);
  ```
- Produces (`AudioTestBarrier.h`): `bool audioDevicesBarredForTestRun();` true under
  the same condition as today's `portAudioBarredForTestRun` (`PortAudioBus.cpp:1003-1009`).
- Produces (`DeviceSampleFormat.h`): `enum class DeviceSampleFormat { Float32, Float64, Int16, Int24Packed, Int24In32Lsb, Int32 }`;
  `void writeStereoToDevice(const float* stereo, int frames, void* dst, DeviceSampleFormat format, int deviceChannels, AudioChannelPair pair, bool interleaved, void* const* planes)`
  (the pair's channels get left and right, every other channel zero; a one-channel
  device or a one-channel pair gets left plus right halved); `void readDeviceToStereo(const void* src, const void* const* planes, bool interleaved, DeviceSampleFormat format, int deviceChannels, AudioChannelPair pair, MicChannelPick pick, int frames, float* stereo)`.
  The mic pick is ported from cmASIO's input mode
  (`// From Thetis Project Files/Source/ChannelMaster/cmasio.c:148-171 [v2.10.3.15 @3759d09]`,
  "left = ch1, right = ch2"): Left copies the pair's first channel to both stereo
  channels, Right its second, and Both adds the two into both channels as Thetis's
  `combinebuff` does (`ivac.c:694-698`, `combined[i] = combined[i + 1] = a[i] + a[i + 1]`).
  `planes` is used when `interleaved` is false (ASIO's one buffer per channel, Core
  Audio's non-interleaved lists); `dst` / `src` otherwise. Scaling follows cmASIO's
  callback (`cmasio.c:150-200`): integer = float × 2^(bits − 1) as a double
  (`scale = 2147483648.0` for 32 bits), clamped to −2^(bits − 1) .. 2^(bits − 1) − 1
  (`max_i32 = 2147483647.0`, `min_i32 = -2147483648.0`), then cast (truncation toward
  zero); float = integer × 1 / 2^(bits − 1) (`inv_scale = 1.0 / 2147483648.0`). The
  same rule with 2^15 and 2^23 for Int16 and the 24-bit forms. `DeviceSampleFormat.cpp`
  carries `cmasio.c`'s header byte-for-byte (Bryan Rambo W4WMT, with the Samphire
  dual-licence statement) under `// --- From cmasio.c ---` and the `ivac.c` header under
  `// --- From ivac.c ---`, a Modification history block, and a `THETIS-PROVENANCE.md`
  row (Type `port`, Variant `thetis-samphire`). No allocation.
- Produces (`IAudioBus.h`, all with default implementations so every existing bus
  compiles unchanged):
  ```cpp
  struct AudioStreamEvent { enum class Kind { DeviceLost, DeviceBusy, FormatChanged, ResetRequested }; Kind kind; QString detail; };
  virtual void setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink) {}  // may be called from a device thread; the sink only posts
  virtual AudioDelayParts delayParts() const { return {}; }
  virtual bool takesStereoMix() const { return false; }  // true: push() takes 48 kHz stereo float into a DeviceRateMatcher
  virtual std::optional<DeviceRateMatcherStats> matcherStats() const { return std::nullopt; }
  virtual void restartClockMatch() {}
  ```
- Produces (`IAudioEngineBackend.h`):
  ```cpp
  enum class AudioNotice { DevicesChanged, DefaultOutputChanged, DefaultInputChanged };
  struct AudioStreamRequest {
      AudioDeviceDirection direction = AudioDeviceDirection::Output;
      QString deviceId;             // empty: the system default
      QString hostApi;              // older drivers only
      AudioChannelPair pair;
      int sampleRate = 48000;
      int bufferFrames = 0;         // 0: the engine's smallest
      int delayMs = 0;              // 0: automatic
      bool exclusive = false;       // Windows audio, exclusive
  };
  class IAudioInputSink {           // called on the input device's callback thread
  public:
      virtual ~IAudioInputSink() = default;
      // Stereo float at the device rate; captureNsOfFrame0 on audioProbeNowNs()'s clock.
      virtual void onInput(const float* stereo, int frames, int sampleRate, std::int64_t captureNsOfFrame0) = 0;
  };
  class IAudioInputStream {
  public:
      virtual ~IAudioInputStream() = default;
      virtual bool open() = 0;
      virtual void close() = 0;
      virtual bool isOpen() const = 0;
      virtual QString errorString() const = 0;
      virtual int sampleRate() const = 0;
      virtual std::optional<std::int64_t> inputLatencyNs() const = 0;
      virtual void setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink) = 0;
  };
  class IAudioEngineBackend {
  public:
      virtual ~IAudioEngineBackend() = default;
      virtual AudioBackendId id() const = 0;
      virtual bool running() const = 0;                                   // its sound server answers
      virtual QList<AudioDeviceInfo> enumerate() = 0;                     // catalogue thread only
      virtual std::optional<QString> defaultDeviceId(AudioDeviceDirection) = 0; // catalogue thread only
      virtual void setNoticeSink(std::function<void(AudioNotice)> sink) = 0;    // the sink may be called from any thread
      virtual std::unique_ptr<IAudioBus> createOutput(const AudioStreamRequest&) = 0;
      virtual std::unique_ptr<IAudioInputStream> createInput(const AudioStreamRequest&, MicChannelPick pick, IAudioInputSink* sink) = 0;
      virtual bool hasControlPanel() const { return false; }
      virtual void openControlPanel(const QString& deviceId) {}
      virtual void rescan() {}
  };
  ```
- Produces (`IAudioDeviceCatalog.h`, `AudioDeviceCatalog.h`):
  ```cpp
  class IAudioDeviceCatalog : public QObject {
      Q_OBJECT
  public:
      using QObject::QObject;
      virtual QList<AudioBackendId> backends() const = 0;                // R-AUD-01 order
      virtual bool backendRunning(AudioBackendId) const = 0;
      virtual QList<AudioDeviceInfo> devices(AudioBackendId, AudioDeviceDirection) const = 0;
      virtual std::optional<AudioDeviceInfo> defaultDevice(AudioBackendId, AudioDeviceDirection) const = 0;
      virtual void rescanOlderDrivers() = 0;
  signals:
      void devicesChanged();
      void defaultChanged(NereusSDR::AudioDeviceDirection direction);
  };
  class AudioDeviceCatalog final : public IAudioDeviceCatalog {
  public:
      static constexpr int kDebounceMs = 500;
      explicit AudioDeviceCatalog(std::vector<std::shared_ptr<IAudioEngineBackend>> backends, QObject* parent = nullptr);
      ~AudioDeviceCatalog() override;      // stops its thread
      void start();                        // lists once; waits at most 3 s for the first list
      void stop();
      void setDebounceIntervalForTest(int ms);
  };
  ```
  Getters are main-thread calls returning the last snapshot. Signals arrive on the
  main thread.
- Produces (`tests/fakes/FakeAudioEngineBackend.h`, `tests/fakes/FakeMatcherAudioBus.h`): a fake engine
  whose device list, default, running flag and notices the test sets; records every
  `createOutput` / `createInput` request; makes `FakeMatcherAudioBus` outputs that hold a real
  `DeviceRateMatcher` when `takesStereoMix`, with `pumpForTest(int frames)` reading as a
  device callback would and `emitEventForTest(AudioStreamEvent)`; and fake inputs that
  call their sink from `pumpForTest`.

**Acceptance:**
- Keys and labels exactly as Global Constraints; `audioEngineFromKey` of an unknown
  string is `nullopt`.
- Pairs: 2 channels gives one entry {1, 2}; 1 gives {1, 1}; 10 gives 1-2, 3-4, 5-6, 7-8,
  9-10; 5 gives 1-2, 3-4, then {5, 1}. Labels: "Outputs 3-4", "Inputs 1-2", "Output 5";
  `audioDeviceEntryLabel` is "Focusrite USB ASIO" + " " + U+00B7 + " " + "Outputs 3-4"
  only when `channelCount > 2`, else the name alone.
- Sample formats: `writeStereoToDevice` with (0.5, −0.25) into Int16 on 4 channels, pair
  3-4 interleaved gives 0, 0, 16384, −8192; into Int32 gives 0, 0, 1073741824,
  −536870912; into Int24Packed on 2 channels gives the little-endian bytes 00 00 40 and
  00 00 E0; 1.5 into Int32 gives 2147483647 and −1.5 gives −2147483648;
  `readDeviceToStereo` on a 2-channel Int32 device holding (1073741824, 536870912) gives
  (0.5, 0.5) with Left, (0.25, 0.25) with Right and (0.75, 0.75) with Both; planar input
  through `planes` gives the same values as interleaved.
- Catalogue: a fake engine adding a device shows it in `devices()` within the debounce
  plus 100 ms of the notice; ten notices within 200 ms re-list once (count
  `enumerate()` calls); a later notice inside the window does not restart the 500 ms;
  a default change emits `defaultChanged` without waiting for the debounce; removing
  and re-adding a device re-lists both times; an open `FakeMatcherAudioBus` on another device
  keeps reading with no gap through all of it; `enumerate()` runs only on the catalogue's
  thread (the fake asserts its thread); a notice posted from a foreign thread is
  handled; `start()` returns after the first list, or after 3 s when an engine blocks.
- No-device rule: with `audioDevicesBarredForTestRun()` true, a real engine's
  `createOutput` returns a bus whose `open()` fails with a test-run error (each engine
  task adds its own case); `tst_core_has_no_gui_includes` stays green.

**Verification:** ordinary feature, test-first (the contract is known). Unit:
`cmake --build build --target tst_audio_device_catalog tst_core_has_no_gui_includes tst_port_audio_bus && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_audio_device_catalog|tst_core_has_no_gui_includes|tst_port_audio_bus)$' --no-tests=error --output-on-failure`;
`python3 scripts/verify-inline-tag-preservation.py`, `python3 scripts/verify-thetis-headers.py`
and `python3 scripts/verify-provenance-sync.py` pass.

**Execution note (advisory):** opus. After Task 2. Interfaces here are used by every
later task; read them twice before committing.

- [ ] **Step 1:** Tests for keys, labels, pairs, sample formats and the catalogue.
- [ ] **Step 2:** Types, barrier, formats, interfaces, catalogue, fakes; commit
  (R-AUD-03, R-AUD-07, R-AUD-32).

## Task 4: Saved identity, matching and migration

**Requirements:** R-AUD-04, R-AUD-05 (the table, verbatim in the spec), bug 1 (a WASAPI
choice reopening on MME), V-SW-2, V-SW-3. Spec "Saved identity".

**Files:**
- Create: `src/core/audio/AudioDeviceMatching.{h,cpp}`,
  `tests/tst_audio_device_identity.cpp`, `tests/tst_audio_device_migration.cpp`
- Modify: `src/core/AudioDeviceConfig.{h,cpp}`, `tests/tst_audio_device_config_roundtrip.cpp`,
  `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 3 `AudioEngineKind`, `AudioDeviceInfo`, `MicChannelPick`.
- Produces (`AudioDeviceConfig.h`, new fields; every existing field and key unchanged):
  ```cpp
  inline constexpr char kAudioDeviceNone[] = "(none)";
  std::optional<AudioEngineKind> engine;   // key Engine; nullopt when the key is absent
  QString deviceId;                        // key DeviceId; empty: platform default; "(none)": none
  int firstChannel = 1;                    // key FirstChannel
  MicChannelPick micChannel = MicChannelPick::Left;  // key MicChannel: "Left", "Right", "Both"
  int delayMs = 0;                         // key DelayMs
  bool isPlatformDefault() const;          // deviceId and deviceName both empty, not none
  bool isNone() const;                     // deviceId == kAudioDeviceNone
  ```
  `loadFromSettings` reads the new keys when present; `saveToSettings` writes them only
  when `engine` is set (so an unmigrated profile is not marked migrated by a save).
- Produces (`AudioDeviceMatching.h`):
  ```cpp
  struct AudioDeviceMatch { AudioDeviceInfo device; bool byId = false; };
  // ID first, then name within the same engine only, with the two allowances.
  std::optional<AudioDeviceMatch> matchSavedAudioDevice(const AudioDeviceConfig& saved, const QList<AudioDeviceInfo>& candidates);
  enum class AudioMigrationResult { Migrated, Unchanged, Postponed };
  struct AudioMigrationContext {
      std::optional<AudioEngineKind> nativeEngine;   // R-AUD-02's first native choice here
      bool nativeRunning = false;                    // its sound server answers
      bool windows = false;                          // Windows naming rules
      bool linux = false;
      bool alsaDirectOnly = false;                   // the Linux Core (Task 12): ALSA direct is the only engine
  };
  AudioMigrationResult migrateAudioDeviceKeys(const QString& prefix, const AudioMigrationContext& context);
  void migrateAllAudioDeviceKeys(const AudioMigrationContext& context);  // Speakers, Headphones, TxInput, Vax1..Vax4
  ```

**Acceptance:**
- Matching (V-SW-2): an ID match wins over a different device with the saved name; a
  name match never crosses engines (a saved `WindowsShared` "Speakers (Realtek(R) Audio)"
  never matches a PortAudio MME entry of that name: bug 1); two devices with one name
  and different IDs: the saved ID picks the right one; with no ID match the first in
  the list order is taken and `byId` is false (the caller saves the ID back, settled
  call 5); a PortAudio match needs the same `hostApi` and name.
- Allowances: on `WindowsShared` / `WindowsExclusive`, a saved name of exactly 31
  characters matches an endpoint whose name starts with it ("Speakers (Focusrite USB Aud"
  matches "Speakers (Focusrite USB Audio)"); on Linux engines a saved ALSA name ending
  in "(hw:2,0)" matches the device with `alsaCard` 2 and `alsaDevice` 0.
- Migration (V-SW-3), one profile per row, `Engine` absent before:
  - `DriverApi` empty, `DeviceName` "Speakers (Realtek(R) Audio)", Windows, native
    `WindowsShared` running: `Engine` "WindowsShared", name kept, `DeviceId` empty,
    result `Migrated`.
  - `DriverApi` "Windows WASAPI", `ExclusiveMode` "False": "WindowsShared"; "True":
    "WindowsExclusive".
  - `DriverApi` "Core Audio" on the Mac: "CoreAudio", name kept.
  - `DriverApi` "ALSA" with `DeviceName` empty, "default", "pulse" or "pipewire", native
    `PipeWire` running: "PipeWire" with `DeviceName` cleared (platform default).
  - `DriverApi` "MME", "Windows DirectSound", "Windows WDM-KS" or "JACK Audio Connection Kit"
    with a device name, or "ALSA" with "USB Audio Device: - (hw:1,0)": `Engine`
    "PortAudio", every old key unchanged, result `Unchanged`.
  - Linux with `nativeRunning` false and a row-1 profile: nothing written, result
    `Postponed`; the next call with `nativeRunning` true migrates it.
  - Any system with `nativeEngine` nullopt (no native engine registered, as on each
    system before its engine task lands): every profile that would move is `Postponed`
    and nothing is written.
  - `alsaDirectOnly` true: every profile moves to "AlsaDirect" whatever its `DriverApi`,
    `DeviceName` kept (matched later with the "(hw:C,D)" allowance), result `Migrated`
    (settled call 33).
  - A profile that already has `Engine` is never touched (result `Unchanged`, no key
    written); running the migration twice gives the same keys as once.
  - The old keys stay in the file after migration.
- `tst_audio_device_config_roundtrip` covers the new keys, and an old profile loads with
  `engine` nullopt.

**Verification:** consequential state transition (an upgrade must never lose a device):
invariant tests first. Unit:
`cmake --build build --target tst_audio_device_identity tst_audio_device_migration tst_audio_device_config_roundtrip && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_audio_device_identity|tst_audio_device_migration|tst_audio_device_config_roundtrip)$' --no-tests=error --output-on-failure`.
Hardware (pending, JJ): V-HW-2's upgrade from a saved MME and a saved WASAPI choice;
V-HW-4's ALSA card-number match.

**Execution note (advisory):** opus. After Task 3. Config migration; a wrong row costs a
user their device on upgrade.

- [ ] **Step 1:** The matching and migration tables as tests.
- [ ] **Step 2:** Config fields, matching, migration; commit (R-AUD-04, R-AUD-05).

## Task 5: The stream supervisor

**Requirements:** R-AUD-08, R-AUD-09, R-AUD-10, R-AUD-11, R-AUD-12, R-AUD-13, R-AUD-14
(the open order and never opening a Bluetooth mic on its own), D4 to D6, D27 to D30,
V-SW-4, V-SW-1 (retry part: "a returning Bluetooth device that fails its first opens is
retried on the schedule").

**Files:**
- Create: `src/core/audio/IAudioStreamHost.h`, `src/core/audio/AudioStreamSupervisor.{h,cpp}`,
  `tests/tst_audio_stream_supervisor.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 3 catalogue and types; Task 4 `AudioDeviceConfig`,
  `matchSavedAudioDevice`.
- Produces:
  ```cpp
  enum class AudioRole { Speakers, Headphones, TxInput, Vax1, Vax2, Vax3, Vax4 };
  enum class AudioRoleState { Playing, PlayingOnDefault, Silent, WaitingForPick, Off };
  enum class AudioRoleReason { None, NotConnected, InUse, NoDevice };
  struct AudioRoleStatus {
      AudioRoleState state = AudioRoleState::Off;
      AudioRoleReason reason = AudioRoleReason::None;
      AudioDeviceConfig chosen;
      QString chosenName;          // empty for the platform default
      QString playingName;         // the device in use now; empty when silent or off
      bool playingBluetooth = false;
      friend bool operator==(const AudioRoleStatus&, const AudioRoleStatus&) = default;
  };
  enum class AudioOpenResult { Opened, Pending, InUse, NotFound, Failed };
  class IAudioStreamHost {                       // main thread
  public:
      virtual ~IAudioStreamHost() = default;
      // device nullopt: the engine's system default.
      virtual AudioOpenResult openRole(AudioRole role, AudioEngineKind engine, const std::optional<AudioDeviceInfo>& device, const AudioDeviceConfig& config) = 0;
      virtual void closeRole(AudioRole role) = 0;
  };
  class AudioStreamSupervisor final : public QObject {
      Q_OBJECT
  public:
      static constexpr std::array<int, 4> kRetryMs{250, 500, 1000, 2000};  // then every 2000 ms while present
      static constexpr int kBluetoothMicFirstWaitMs = 2000;
      AudioStreamSupervisor(IAudioDeviceCatalog& catalogue, IAudioStreamHost& host, QObject* parent = nullptr);
      void setChoice(AudioRole role, const AudioDeviceConfig& config);
      void setRoleEnabled(AudioRole role, bool enabled);   // a disabled role is Off
      void setTransmitting(bool transmitting);
      void setNoneMeansWaitingForPick(AudioRole role, bool waiting);   // a "(none)" choice reads WaitingForPick, not Off
      void onStreamEvent(AudioRole role, const AudioStreamEvent& event);   // main thread
      void onOpenFinished(AudioRole role, AudioOpenResult result);          // completes a Pending open (the mic helper)
      AudioRoleStatus status(AudioRole role) const;
      void setRetryScaleForTest(double scale);
  signals:
      void statusChanged(NereusSDR::AudioRole role, const NereusSDR::AudioRoleStatus& status);
      void savedIdentityLearned(NereusSDR::AudioRole role, const QString& deviceId);  // name match: save the ID back
  };
  ```

**Acceptance (V-SW-4, with a fake catalogue and a fake host):**
- Speakers on a chosen device that goes away (stream `DeviceLost`): the host gets
  `openRole` on the default at once, before any list change; status `PlayingOnDefault`,
  `NotConnected`, `playingName` the default's name. The device returns in the list: the
  chosen device opens again (retry at 250 ms) and status is `Playing`. Headphones the
  same.
- An open that fails while the device is listed is retried at 250, 500, 1000, 2000 ms,
  then every 2000 ms while it stays listed, and stops when it leaves the list.
- `DeviceBusy` (or `openRole` returning `InUse`): as lost, with `InUse`; it moves back
  when an open succeeds.
- Mic (`TxInput`) chosen device lost: `closeRole`, status `Silent`, `NotConnected`; the
  host is never asked to open any other input; back by itself on return. VAX 1 to 4 the
  same.
- No default device at all: outputs `Silent`, `NoDevice`.
- Platform default outputs follow `defaultChanged(Output)` at once and report the new
  `playingName`.
- Platform default mic follows `defaultChanged(Input)` only when not transmitting; a
  change during transmit applies at `setTransmitting(false)`; a Bluetooth default is not
  followed (the mic stays where it is); starting with a Bluetooth system default opens
  the built-in input, else another non-Bluetooth input, else `Silent` with `NoDevice`.
- A Bluetooth mic picked by name opens; when it is the role's choice at start, outputs
  are opened after its `onOpenFinished(Opened)`, or after 2000 ms if that has not come.
- `isNone()` choice: `WaitingForPick` after `setNoneMeansWaitingForPick(Speakers, true)`
  (Task 21 calls it on a Core whose box starts into a desktop), else `Off`; nothing
  opens either way; `setNoneMeansWaitingForPick(Speakers, false)` turns it back to `Off`.
- A name-only match emits `savedIdentityLearned` with the matched ID once.
- A burst of `devicesChanged` while a role plays its chosen device causes no reopen.

**Verification:** consequential state transitions that decide what plays and what the
mic records: invariant tests first. Unit:
`cmake --build build --target tst_audio_stream_supervisor && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_audio_stream_supervisor)$' --no-tests=error --output-on-failure`.

**Execution note (advisory):** opus. After Task 4. Decides the mic's device, which feeds
the keying gate; the rule "never another mic" is the safety point.

- [ ] **Step 1:** The V-SW-4 cases as tests.
- [ ] **Step 2:** The supervisor; commit (R-AUD-08 to R-AUD-14).

## Task 6: The matcher on every local output

**Requirements:** R-AUD-15 (wiring), R-AUD-33 (bug 6), D2, D34 design choices 15 and 16,
settled call 30. Spec "Clock matching and the delay readout" (remote audio paragraph).

**Files:**
- Create: `tests/tst_audio_engine_matcher_push.cpp`
- Modify: `src/core/audio/PortAudioBus.{h,cpp}` (the output ring becomes a
  `DeviceRateMatcher`; `takesStereoMix` true for outputs; the callback converts to the
  device's format; `outputPacing`, `delayParts`, `matcherStats`, `restartClockMatch`,
  `flush`; `#ifdef Q_OS_MAC` at `:119` with `<QtGlobal>` included),
  `src/core/AudioEngine.{h,cpp}` (stereo 48 kHz float into a bus that takes it, skipping
  `SpeakerFormatConverter`; `writeRemotePlayback` and `writeVaxOutput` the same;
  `remotePlaybackIntoMatcher()`; probe readout),
  `src/core/session/media/RemoteAudioReceiver.{h,cpp}`,
  `src/core/audio/RemoteVaxFeeder.{h,cpp}`, `tests/tst_port_audio_bus.cpp`,
  `tests/tst_remote_audio_receiver.cpp`, `tests/tst_remote_vax_feeder.cpp`,
  `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 2 matcher; Task 3 `IAudioBus` additions and `FakeMatcherAudioBus`; Task 1
  `AudioDelayProbeMatcher::setReadoutMs`.
- Produces: `bool AudioEngine::remotePlaybackIntoMatcher() const` (speakers bus takes a
  stereo mix); `std::optional<DeviceRateMatcherStats> AudioEngine::remotePlaybackMatcherStats() const`;
  `AudioDelayParts AudioEngine::speakersDelayParts() const` (Task 7 generalises it per
  role).

**Acceptance:**
- PortAudio output: `push()` of 48 kHz stereo float writes into the matcher; the PortAudio
  callback reads it (`MatcherReader::read`) and converts to the stream's format and
  channels with `writeStereoToDevice`; no lock or allocation is added to the callback;
  the old `m_ring` and `kDefaultRingSamples` are gone from the output path (the input
  path keeps its ring until Task 13).
- `outputPacing()`: `consumedFrames` the matcher's `read`, `queuedFrames` its fill,
  `capacityFrames` its rsize, `callbackFrames` the stream's, `deviceLatencyNs` as today.
- `delayParts()`: matcher fill and resampler from the matcher, device buffer = callback
  frames at the device rate, device latency = PortAudio's output latency.
- `flush()` calls `requestFlush()`; mute still silences within one block
  (`tst_audio_engine_master_mute` green).
- AudioEngine: for a bus with `takesStereoMix()`, `rxBlockReady` pushes the 48 kHz stereo
  float block (after every gain and mute) and does not run `m_speakersConverter`; for a
  bus without it, today's path is unchanged. Headphones and the Windows VAX bus the same.
- Remote playback with `remotePlaybackIntoMatcher()` true: `RemoteAudioReceiver`
  releases frames by the jitter hold alone (the sink-mode rule at `:954-998`), writes
  48 kHz stereo through `writeRemotePlayback` into the bus's matcher and does not use
  its `RemoteAudioRateMatcher`; it still detects a stall from `consumedFrames` (500 ms,
  as today), publishes stats from the bus's `matcherStats()`, and sets
  `setDownstreamExcessNs` to (matcher fill − target fill). `beginRemotePlayback` and
  `endRemotePlayback` call `restartClockMatch()`. With it false, today's path is
  unchanged. `RemoteMicReceiver` and VAX on the Mac and Linux keep
  `RemoteAudioRateMatcher`.
- `RemoteVaxFeeder` on a VAX bus that takes a stereo mix writes 48 kHz stereo into its
  matcher; otherwise unchanged.
- The probe's summary carries `readout <speakersDelayParts().totalMs()> ms` when the
  speakers bus has a matcher.
- A `FakeMatcherAudioBus` speakers bus pumped at a 200 ppm offset for 60 simulated seconds of
  local playback shows no dry run after sizing (`tst_audio_engine_matcher_push`).

**Verification:** core RX path, behaviour-changing refactor: baseline first (run the
listed existing tests before changing anything), then the new test. Unit:
`cmake --build build --target tst_audio_engine_matcher_push tst_port_audio_bus tst_port_audio_named_match tst_audio_engine_speaker_format tst_audio_engine_speakers_live_reconfig tst_audio_engine_master_mute tst_remote_audio_receiver tst_remote_vax_feeder tst_radio_codec_speaker_out && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_audio_engine_matcher_push|tst_port_audio_bus|tst_port_audio_named_match|tst_audio_engine_speaker_format|tst_audio_engine_speakers_live_reconfig|tst_audio_engine_master_mute|tst_remote_audio_receiver|tst_remote_vax_feeder|tst_radio_codec_speaker_out)$' --no-tests=error --output-on-failure`.
Human smoke (JJ, pending): local receive and a remote window both play cleanly for ten
minutes on the Mac.

**Execution note (advisory):** opus. After Tasks 2 and 3. Core RX path and remote
playback; flagged for the early review with Tasks 2 and 7.

- [ ] **Step 1:** Baseline run of the existing tests; the new push test.
- [ ] **Step 2:** PortAudio output on the matcher, bug 6; the engine push paths; commit
  (R-AUD-15, R-AUD-33).
- [ ] **Step 3:** Remote playback and the Windows VAX feeder into the matcher; commit
  (R-AUD-15).

## Task 7: Engines, catalogue and supervisor in AudioEngine

**Requirements:** R-AUD-02, R-AUD-06 (engine part), R-AUD-34, R-AUD-15 (readout per
role), R-AUD-05 (running the migration at start), R-AUD-32. D1, D8, D11.

**Files:**
- Create: `src/core/audio/PortAudioBackend.{h,cpp}`, `src/core/audio/AudioBackendRegistry.{h,cpp}`,
  `tests/tst_port_audio_backend.cpp`, `tests/tst_audio_engine_native_routing.cpp`
- Modify: `src/core/AudioEngine.{h,cpp}`, `src/core/audio/PortAudioBus.{h,cpp}`
  (`matchNamedDevice` within the saved host API, bug 1), `src/core/daemon/DaemonApp.cpp`
  (`setAudioBackendContext({.daemon = true})` beside `setVaxOutputsAllowed(false)` at
  `:297`), `tests/tst_audio_engine_no_portaudio.cpp`,
  `tests/tst_audio_engine_pull_tx_mic.cpp`, `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Tasks 3 to 6.
- Produces (`AudioBackendRegistry.h`):
  ```cpp
  struct AudioBackendContext {
      bool daemon = false;     // nereusd
      bool helper = false;     // the mic helper process
  };
  // In R-AUD-01 order for this system and process. Engine tasks add theirs here.
  std::vector<std::shared_ptr<IAudioEngineBackend>> makeSystemAudioBackends(const AudioBackendContext& context);
  // R-AUD-02: the first native engine whose backend is registered and running, else PortAudio.
  AudioEngineKind defaultAudioEngine(const std::vector<std::shared_ptr<IAudioEngineBackend>>& backends);
  ```
- Produces (`PortAudioBackend.h`): `class PortAudioBackend final : public IAudioEngineBackend`
  (id `PortAudio`, `running()` true, `enumerate()` lists the older host APIs:
  "MME", "Windows DirectSound", "Windows WDM-KS" on Windows, "JACK Audio Connection Kit"
  and "ALSA" on Linux; device `id` is the PortAudio device name and `hostApi` its host
  API name; no notices; `rescan()` re-initialises PortAudio). Its constructor takes
  `PortAudioBackend(PortAudioListFn list, OlderDriverPlatform platform, bool includeReplacedHostApis)`
  where `using PortAudioListFn = std::function<QList<PortAudioDeviceRecord>()>` (the
  default reads `PortAudioBus::hostApis` / `outputDevicesFor` / `inputDevicesFor`),
  `struct PortAudioDeviceRecord { QString hostApi; QString name; int outputChannels; int inputChannels; bool isDefaultOutput; bool isDefaultInput; }`
  and `enum class OlderDriverPlatform { Mac, Windows, Linux }`. While
  `includeReplacedHostApis` is true it also lists the host APIs a native engine replaces
  ("Core Audio" on the Mac, "Windows WASAPI" on Windows), so nothing goes silent before
  that system's engine task lands; the registry passes true until Tasks 8 and 9 set it
  false (the Mac then registers no PortAudio backend at all, R-AUD-01).
- Produces (`AudioEngine.h`):
  ```cpp
  IAudioDeviceCatalog* catalogue() const;
  AudioRoleStatus roleStatus(AudioRole role) const;
  AudioDelayParts delayParts(AudioRole role) const;   // replaces Task 6's speakersDelayParts
  AudioEngineKind defaultEngine() const;
  void rescanOlderDrivers();                          // R-AUD-06
  void setAudioBackendContext(const AudioBackendContext& context);   // before start(); DaemonApp sets daemon = true
  void setAudioBackendsForTest(std::vector<std::shared_ptr<IAudioEngineBackend>> backends);  // before start()
  signals:
  void roleStatusChanged(NereusSDR::AudioRole role, const NereusSDR::AudioRoleStatus& status);
  ```

**Acceptance:**
- Start: the engine builds its backends with `makeSystemAudioBackends` (or the test's),
  runs `migrateAllAudioDeviceKeys` with the context from `defaultAudioEngine`, starts
  the catalogue, then hands each role's saved choice to the supervisor; speakers,
  headphones and the PC mic with no saved choice use `defaultEngine()` on
  "(platform default)" (R-AUD-02).
- `makeBus` opens through the backend for the config's `engine`
  (`audioBackendFor(engine)`), with `AudioStreamRequest` built from the config; a config
  with no `engine` (only possible before migration) uses PortAudio as today. The
  `DeviceBusFactory` test seam still wins when set.
- `IAudioStreamHost` is implemented by the engine: `openRole` opens or reopens the
  role's bus (speakers and headphones) through `makeBus`, keeps today's bus swap
  under its existing lock, connects the bus's event sink to post to the main thread and
  into `AudioStreamSupervisor::onStreamEvent`, and saves a learned ID with
  `saveToSettings`. Speakers and headphones changes from Setup go through `setChoice`.
- The PC mic role: `openRole(TxInput, ...)` hands the matched config (with `engine`,
  `deviceId` and `micChannel`) to `CaptureSupervisor::configure` and returns `Pending`;
  the capture status maps to the supervisor: Ready to `onOpenFinished(Opened)`, Failed
  with `DeviceNotFound` to `NotFound`, Failed with a busy detail to `InUse` (Task 13 adds
  the reason), any other failure to `Failed`, and Ready then Failed with `InputLost` to
  `onStreamEvent(TxInput, DeviceLost)`. `closeRole(TxInput)` releases nothing the keying
  rule needs: the demand leases stay as `updatePcCaptureDemand` holds them, and the
  capture is configured with `kAudioDeviceNone` so the helper opens nothing. Until
  Task 13 the helper keeps opening by `deviceName` through PortAudio as today.
- VAX on Windows: `makeVaxBus` opens through the backend for the channel's `engine`
  the same way, and the supervisor's `Vax1` to `Vax4` roles are enabled only on Windows
  (VAX on the Mac and Linux is NereusSDR's own device and keeps its buses as today).
- `rescanOlderDrivers()`: each role on PortAudio fades out (`requestFadeOut`, at most
  20 ms), closes, PortAudio is terminated and initialised again, the catalogue re-lists
  the older drivers, and those roles reopen; a role on any other engine is never closed
  (a fake native bus keeps reading through it). If the mic uses older drivers, the
  capture supervisor's `retry()` restarts the helper.
- R-AUD-34: a remote window's `AudioEngine` (no radio) opens its speakers through the
  same registry, and remote playback reaches that bus's matcher.
- `PortAudioBus::matchNamedDevice` only matches within the saved host API; a saved
  "Windows WASAPI" device name that only exists under MME does not open on MME
  (`tst_port_audio_named_match` covers it).
- `tst_port_audio_backend` (an injected list, no PortAudio call): on Windows a list with
  MME, Windows DirectSound, Windows WDM-KS and Windows WASAPI entries gives the first
  three only when `includeReplacedHostApis` is false and all four when true; on Linux
  JACK and ALSA entries are listed; on the Mac nothing is listed unless the flag is true
  (then "Core Audio"); each entry's `id` is the device name, `hostApi` the host API name
  and `backend` `PortAudio`; the default output is the record with `isDefaultOutput`.
- `tst_audio_engine_no_portaudio` and every existing engine test stay green with fakes;
  `tst_audio_engine_native_routing` covers: engine choice by `Engine`, the default
  engine, the role status signal, the delay readout per role, Rescan touching only
  older-driver roles, and a remote window's speakers.

**Verification:** core RX path wiring with consequential state transitions: tests
first for routing and Rescan. Unit:
`cmake --build build --target tst_port_audio_backend tst_audio_engine_native_routing tst_audio_engine_no_portaudio tst_audio_engine_pull_tx_mic tst_port_audio_named_match tst_audio_engine_speakers_live_reconfig tst_audio_engine_matcher_push && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_port_audio_backend|tst_audio_engine_native_routing|tst_audio_engine_no_portaudio|tst_audio_engine_pull_tx_mic|tst_port_audio_named_match|tst_audio_engine_speakers_live_reconfig|tst_audio_engine_matcher_push)$' --no-tests=error --output-on-failure`.

**Execution note (advisory):** opus. After Tasks 4 to 6. Core RX path; flagged. After this
task lands, the controller offers JJ the early review of Tasks 2, 6 and 7.

- [ ] **Step 1:** Routing, default engine and Rescan tests with fake engines.
- [ ] **Step 2:** Registry, PortAudio backend, engine wiring, bug 1; commit (R-AUD-02,
  R-AUD-06, R-AUD-34).


## Task 8: Core Audio on the Mac

**Requirements:** R-AUD-01 (Mac: "Core Audio", greyed, the only native choice), R-AUD-02
(Mac), R-AUD-03 (Mac notices), R-AUD-07 (Mac pairs), R-AUD-11 (hog mode), R-AUD-14
(transport), R-AUD-18 (the DSP thread part: "the DSP thread leaves and rejoins the audio
workgroup of the device it plays to whenever that device changes"), D1, D17, D27.
V-HW-1 pending.

**Files:**
- Create: `src/core/audio/CoreAudioSystem.{h,cpp}` (the `ICoreAudioSystem` seam in the
  header; the real adapter in the `.cpp`), `src/core/audio/CoreAudioBackend.{h,cpp}`,
  `src/core/audio/CoreAudioOutputBus.{h,cpp}`, `src/core/audio/CoreAudioInputStream.{h,cpp}`,
  `tests/fakes/FakeCoreAudioSystem.h`, `tests/tst_core_audio_backend.cpp`
- Modify: `src/core/audio/AudioBackendRegistry.cpp` (the Mac registers Core Audio only,
  in the window, the Core and the helper), `src/core/IAudioBus.h`
  (`audioWorkgroupDevice()`), `src/core/audio/RealtimeAudioPriority.{h,cpp}`
  (`rejoinAudioWorkgroup`), `src/core/AudioEngine.{h,cpp}` (workgroup generation),
  `src/models/RxDspWorker.cpp` (rejoin check once per block), `CMakeLists.txt` (APPLE
  sources beside `:1573-1576`; link `-framework AudioToolbox` and
  `-framework AudioUnit` where AVFoundation and CoreAudio are linked), `tests/CMakeLists.txt`
  (inside the `if(APPLE)` block at `:1610`)

**Interfaces:**
- Consumes: Task 3 types and `IAudioEngineBackend`; Task 2 `MatcherReader`,
  `DeviceRateMatcher`; Task 7 registry and `AudioBackendContext`.
- Produces (`CoreAudioSystem.h`):
  ```cpp
  struct CoreAudioDeviceRecord {
      std::uint32_t objectId = 0;      // AudioObjectID, valid until the device goes
      QString uid;                     // kAudioDevicePropertyDeviceUID: the saved DeviceId
      QString name;                    // kAudioObjectPropertyName
      std::uint32_t transportType = 0; // kAudioDevicePropertyTransportType
      int outputChannels = 0;          // kAudioDevicePropertyStreamConfiguration, output scope
      int inputChannels = 0;           // the same, input scope
      bool alive = true;               // kAudioDevicePropertyDeviceIsAlive
      std::int32_t hogPid = -1;        // kAudioDevicePropertyHogMode
  };
  class ICoreAudioSystem {
  public:
      virtual ~ICoreAudioSystem() = default;
      virtual QList<CoreAudioDeviceRecord> devices() = 0;                      // catalogue thread
      virtual std::optional<std::uint32_t> defaultDevice(AudioDeviceDirection) = 0;
      virtual void setNoticeSink(std::function<void(AudioNotice)> sink) = 0;  // listeners on a private serial queue
      virtual std::int32_t ownPid() const = 0;
      virtual std::unique_ptr<IAudioBus> createOutput(const CoreAudioDeviceRecord&, const AudioStreamRequest&) = 0;
      virtual std::unique_ptr<IAudioInputStream> createInput(const CoreAudioDeviceRecord&, const AudioStreamRequest&, MicChannelPick, IAudioInputSink*) = 0;
  };
  std::unique_ptr<ICoreAudioSystem> makeCoreAudioSystem();   // Q_OS_MAC only
  AudioTransport coreAudioTransport(std::uint32_t transportType);
  ```
- Produces: `class CoreAudioBackend final : public IAudioEngineBackend` (id `CoreAudio`,
  constructed with `std::unique_ptr<ICoreAudioSystem>`); `IAudioBus::audioWorkgroupDevice()`
  (`virtual std::uint32_t audioWorkgroupDevice() const { return 0; }`, the bus's
  AudioObjectID on Core Audio, 0 elsewhere);
  `bool rejoinAudioWorkgroup(AudioPriorityToken* token, std::uint32_t audioObjectId)`
  (`RealtimeAudioPriority.h`; leaves the token's current workgroup and joins
  `kAudioDevicePropertyIOThreadOSWorkgroup` of that device on the calling thread;
  returns false and stays out of any workgroup on failure; returns true and does nothing
  on other systems); `std::uint32_t AudioEngine::speakersWorkgroupGeneration() const`
  and `std::uint32_t AudioEngine::speakersWorkgroupDevice() const` (both atomic loads).

**Acceptance:**
- `coreAudioTransport` maps the codes in `AudioHardwareBase.h:607-624` (macOS SDK 27.0):
  `'bltn'` BuiltIn; `'usb '` Usb; `'blue'` and `'blea'` Bluetooth; `'hdmi'` and `'dprt'`
  Hdmi; `'virt'` Virtual; everything else Unknown.
- Enumerate with a fake system: one `AudioDeviceInfo` per direction a device has
  channels for (a USB interface with 10 outputs and 8 inputs gives one output entry with
  `channelCount` 10 and one input entry with 8); `id` is the UID, `name` the name;
  `hogPid` other than −1 and other than `ownPid()` gives `InUse`; `alive` false gives
  `NotConnected`; `isDefault` follows `defaultDevice`.
- Notices: the fake's device-list notice reaches the sink as `DevicesChanged`, the two
  default notices as `DefaultOutputChanged` and `DefaultInputChanged`; the real adapter
  registers block listeners on `kAudioHardwarePropertyDevices`,
  `kAudioHardwarePropertyDefaultOutputDevice`, `kAudioHardwarePropertyDefaultInputDevice`
  and, per device, `kAudioDevicePropertyDeviceIsAlive` and `kAudioDevicePropertyHogMode`,
  on one private serial queue as `CoreAudioHalBus.cpp:105-260` does, and its blocks
  only post the notice; per-device listeners are added and removed inside `devices()`.
- Output bus (`CoreAudioOutputBus`): an AUHAL output unit
  (`kAudioUnitSubType_HALOutput`) on the device; client format 2-channel interleaved
  float32 at the device's nominal rate; `kAudioOutputUnitProperty_ChannelMap` sized to
  the device's output channels with the pair's two channels set to 0 and 1 and every
  other entry −1; buffer frames = the request's, or 128 when 0, clamped to
  `kAudioDevicePropertyBufferFrameSizeRange`; the render callback only calls
  `MatcherReader::read` into the buffer. `takesStereoMix()` is true; the matcher is
  `{inRate 48000, outRate <device rate>, writeBlockFrames 64, callbackFrames <buffer frames>, delayMs <request>}`.
  `outputPacing()` and `delayParts()` as Task 6 defines them, with device latency =
  (`kAudioDevicePropertyLatency` + `kAudioDevicePropertySafetyOffset` +
  `kAudioStreamPropertyLatency`, output scope) frames at the nominal rate. Bus-level
  listeners: alive false posts `DeviceLost`, hog taken by another process posts
  `DeviceBusy`, `kAudioDevicePropertyNominalSampleRate` changing posts `FormatChanged`
  (design choice 3: the supervisor reopens at the new rate, no prompt).
  `audioWorkgroupDevice()` returns the device's AudioObjectID.
- Input stream (`CoreAudioInputStream`, used by the helper from Task 13): an AUHAL with
  input enabled and output disabled, the pair picked through the input channel map,
  client format 2-channel float32 at the device rate, a buffer allocated at open; the
  input callback calls `AudioUnitRender`, applies the `MicChannelPick` as
  `readDeviceToStereo` does, and calls `IAudioInputSink::onInput` with
  `captureNsOfFrame0 = AudioConvertHostTimeToNanos(mHostTime) − input latency`. A test
  checks `AudioConvertHostTimeToNanos(AudioGetCurrentHostTime())` and `audioProbeNowNs()`
  agree within 1 ms (no device is opened for this); if they do not, the stream measures
  both clocks once at open and applies the offset.
- With `audioDevicesBarredForTestRun()` true, `createOutput` and `createInput` on the
  real adapter return streams whose `open()` fails with a test-run error.
- Registry on the Mac: Core Audio only; the PortAudio backend is no longer registered
  (R-AUD-01); `defaultAudioEngine` is `CoreAudio`; migration runs with
  `nativeEngine = CoreAudio`, `nativeRunning = true`.
- Workgroup follow: the engine bumps `speakersWorkgroupGeneration` and stores
  `speakersWorkgroupDevice` whenever the speakers bus opens on a device (including a
  fall-back to the default and the return); `RxDspWorker` loads the generation once per
  block and, when it changed, calls `rejoinAudioWorkgroup(token, device)` on the DSP
  thread; a test with a fake engine checks the call happens once per change and never on
  an unchanged block (the join itself is V-HW-1's evidence).

**Verification:** a device the system answers, plus the core RX path: the contract
(mapping, notices, open rules) with fakes now; real-device behaviour pending. Unit
(Mac):
`cmake --build build --target tst_core_audio_backend tst_audio_engine_native_routing tst_realtime_audio_priority tst_core_has_no_gui_includes && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_core_audio_backend|tst_audio_engine_native_routing|tst_realtime_audio_priority|tst_core_has_no_gui_includes)$' --no-tests=error --output-on-failure`.
Build: `cmake --build build` (the app links the new frameworks). Hardware (pending, JJ):
V-HW-1 with AirPods and a USB interface; JJ also runs `--audio-delay-probe` on the Mac
for V-HW-8's "after" number.

**Execution note (advisory):** opus. After Task 7. Core RX path; flagged. Not parallel
with Tasks 9 to 12 (they edit the registry).

- [ ] **Step 1:** Fake-system tests for mapping, notices, pairs, states and the barrier.
- [ ] **Step 2:** The backend, the AUHAL output and input, the real adapter; registry
  and CMake; commit (R-AUD-01, R-AUD-02, R-AUD-07, R-AUD-11).
- [ ] **Step 3:** Workgroup follow with its test; commit (R-AUD-18).

## Task 9: Windows audio, shared and exclusive

**Requirements:** R-AUD-01 (Windows, without ASIO), R-AUD-02 (Windows audio shared),
R-AUD-03 (`IMMNotificationClient`), R-AUD-11 (exclusive busy), R-AUD-12, R-AUD-14
(Windows transport), R-AUD-16 (engine part: "Windows audio, shared runs the device at
the engine's smallest period in the device's own format. Exclusive runs event-driven in
the device's best format"), D8, D10, D12 (VAX on the native engine), design choice 1.
V-HW-2 pending.

**Files:**
- Create: `src/core/audio/WasapiPolicy.{h,cpp}` (portable), `src/core/audio/WasapiSystemWin.{h,cpp}`,
  `src/core/audio/WasapiBackendWin.{h,cpp}`, `src/core/audio/WasapiOutputBusWin.{h,cpp}`,
  `src/core/audio/WasapiInputStreamWin.{h,cpp}`, `tests/tst_wasapi_policy.cpp`
- Modify: `src/core/audio/AudioBackendRegistry.cpp` (Windows registers Windows audio
  then PortAudio with `includeReplacedHostApis` false), `CMakeLists.txt` (the `*Win`
  files inside `if(WIN32)` only; link `ole32`, `uuid` and `ksuser` beside `avrt` at
  `:2367-2372`), `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Tasks 2, 3, 7.
- Produces (`WasapiPolicy.h`, all portable and tested on the Mac):
  ```cpp
  enum class WasapiSampleType { Float, Int };
  struct WasapiFormatCandidate { WasapiSampleType type; int containerBits; int validBits; };
  // Exclusive try order (settled call 14): float32, int32, 24 valid in a 32-bit container, int16.
  QList<WasapiFormatCandidate> wasapiExclusiveFormatOrder();
  DeviceSampleFormat wasapiDeviceFormat(const WasapiFormatCandidate&);   // 24-in-32 writes as Int32
  struct WasapiEnginePeriods { int defaultFrames; int fundamentalFrames; int minFrames; int maxFrames; };
  // Shared: IAudioClient3's smallest period (or the request rounded up to a multiple of
  // the fundamental, clamped to min..max); nullopt periods mean IAudioClient3 failed and the
  // stream uses the ordinary event-driven shared stream at the engine's default period.
  int wasapiSharedPeriodFrames(const std::optional<WasapiEnginePeriods>& periods, int requestedFrames);
  // Exclusive: the device's minimum period, or the request if larger; and the realignment
  // after AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED: hns = (10'000'000 × frames / rate) + 0.5.
  std::int64_t wasapiExclusivePeriodHns(std::int64_t minimumPeriodHns, int requestedFrames, int rate);
  std::int64_t wasapiAlignedPeriodHns(int bufferFrames, int rate);
  enum class WasapiFlow { Render, Capture, All };
  enum class WasapiRole { Console, Multimedia, Communications };
  enum class WasapiNotification { DeviceAdded, DeviceRemoved, StateChanged, DefaultChanged, NameChanged, OtherPropertyChanged };
  std::optional<AudioNotice> wasapiNoticeFor(WasapiNotification, WasapiFlow, WasapiRole);
  AudioTransport wasapiTransport(const QString& enumeratorName, int formFactor);
  enum class WasapiResult { Ok, DeviceInvalidated, DeviceInUse, ExclusiveNotAllowed, UnsupportedFormat, BufferSizeNotAligned, ServiceNotRunning, Other };
  WasapiResult wasapiResultFor(std::uint32_t hresult);
  AudioOpenResult wasapiOpenResult(WasapiResult);
  std::optional<AudioStreamEvent::Kind> wasapiStreamEvent(WasapiResult);
  ```
- Produces: `class WasapiBackendWin final : public IAudioEngineBackend` (id `Wasapi`);
  its `createOutput` / `createInput` honour `AudioStreamRequest::exclusive`.

**Acceptance:**
- Format order is exactly float32 (32/32), int32 (32/32), 24-in-32 (32/24), int16 (16/16);
  `wasapiDeviceFormat` gives Float32, Int32, Int32, Int16.
- Shared period: periods {480, 48, 48, 480}, request 0 gives 48; request 100 gives 144;
  request 1000 gives 480; nullopt gives the default path (the function returns 0 and the
  caller initialises the ordinary event-driven stream).
- `wasapiAlignedPeriodHns(441, 44100)` gives 100000; `wasapiExclusivePeriodHns(30000, 0, 48000)`
  gives 30000; with a request of 256 frames at 48 kHz gives 53333.
- Notices: DeviceAdded, DeviceRemoved, StateChanged and NameChanged give
  `DevicesChanged`; DefaultChanged with role Console gives `DefaultOutputChanged`
  (Render) or `DefaultInputChanged` (Capture); with Multimedia or Communications gives
  nothing (design choice 1); OtherPropertyChanged gives nothing.
- Transport: enumerator "BTHENUM" or "BTHHFENUM" (any case) gives Bluetooth; "USB" gives
  Usb; form factor 9 (`DigitalAudioDisplayDevice`, named `HDMI` in older headers,
  `mmdeviceapi.h` enum) gives Hdmi; otherwise Unknown. The Bluetooth rule is pending the
  bench (Global Constraints note).
- Result mapping, values mirrored from `audioclient.h` (`AUDCLNT_ERR(n)` =
  `0x88890000 | n`): 0x88890004 DeviceInvalidated, 0x8889000A DeviceInUse, 0x8889000E
  ExclusiveNotAllowed, 0x88890008 UnsupportedFormat, 0x88890010 ServiceNotRunning (all
  five as in PortAudio's bundled `mingw-include/audioclient.h:1139-1150`), 0x88890019
  BufferSizeNotAligned; `WasapiSystemWin.cpp` `static_assert`s each mirror against the
  real macro. DeviceInUse and ExclusiveNotAllowed open as `InUse` and post `DeviceBusy`;
  DeviceInvalidated posts `DeviceLost`; others `Failed`.
- The Windows adapter (compiled in CI only): `IMMDeviceEnumerator` with an
  `IMMNotificationClient` whose methods only post the mapped notice (no blocking, no
  register or unregister inside it, no last release, as the spec's "System notices"
  says); `id` is the endpoint ID string, `name` `PKEY_Device_FriendlyName`,
  transport from `PKEY_Device_EnumeratorName` and `PKEY_AudioEndpoint_FormFactor`,
  channels from the mix format; state from `DEVICE_STATE_ACTIVE` (others are not listed);
  the output stream runs on its own thread registered with MMCSS ("Pro Audio" through
  `AvSetMmThreadCharacteristicsW`, as `RealtimeAudioPriority` does), event-driven
  (`AUDCLNT_STREAMFLAGS_EVENTCALLBACK`); shared uses `IAudioClient3::InitializeSharedAudioStream`
  with the period above in the mix format; exclusive tries the formats in order with
  `IsFormatSupported` and realigns once on `BufferSizeNotAligned`; each event reads
  `MatcherReader::read` into stereo float and writes the device format with
  `writeStereoToDevice` (pair mapping included); `takesStereoMix()` is true; device
  latency from `GetStreamLatency`; device buffer = the period. COM is initialised on
  each thread that uses it. `audioDevicesBarredForTestRun()` makes every open fail.
- VAX: with the registry change, Windows VAX channels (Task 7's `makeVaxBus` path) open
  through Windows audio shared (D12).

**Verification:** a device the system answers, plus the core RX path: all decisions
tested on the Mac now; the adapter compiles in Windows CI (MinGW `ci.yml`, MSVC
`release.yml`) after JJ approves a push; behaviour pending the bench. Unit (Mac):
`cmake --build build --target tst_wasapi_policy && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_wasapi_policy)$' --no-tests=error --output-on-failure`.
CI: Windows builds green (pending the push). Hardware (pending, JJ): V-HW-2 and V-HW-8 on
the Windows PC.

**Execution note (advisory):** opus. After Task 8. Core RX path; flagged. Code the
implementer cannot compile locally: keep Windows-only files small and every decision in
`WasapiPolicy`.

- [ ] **Step 1:** `tst_wasapi_policy` with the exact cases above.
- [ ] **Step 2:** The policy, then the Windows adapter files, registry and CMake;
  commit (R-AUD-01, R-AUD-02, R-AUD-11, R-AUD-16).

## Task 10: PipeWire

**Requirements:** R-AUD-01 (Linux with PipeWire), R-AUD-02 (Linux), R-AUD-03 (registry
listener), R-AUD-07 (PipeWire pairs), R-AUD-14 (bluez transport), design choice 11.
V-HW-4 pending.

**Files:**
- Create: `src/core/audio/PipeWireDeviceSystem.{h,cpp}` (seam and real adapter),
  `src/core/audio/PipeWireDeviceBackend.{h,cpp}`, `tests/tst_pipewire_device_backend.cpp`
- Modify: `src/core/audio/PipeWireStream.{h,cpp}` (matcher output mode, input sink
  mode, audio position and no-remix in `StreamConfig`), `src/core/audio/AudioBackendRegistry.cpp`,
  `CMakeLists.txt` (Linux sources under `NEREUS_HAVE_PIPEWIRE`), `tests/CMakeLists.txt`
  (inside the `if(UNIX AND NOT APPLE)` block), `tests/tst_pipewire_stream_config.cpp`

**Interfaces:**
- Consumes: Tasks 2, 3, 7.
- Produces (`PipeWireDeviceSystem.h`):
  ```cpp
  struct PipeWireNodeRecord {
      std::uint32_t id = 0;
      QString nodeName;          // node.name: the saved DeviceId
      QString description;       // node.description: the display name
      QString mediaClass;        // "Audio/Sink", "Audio/Source", "Audio/Duplex"
      QString deviceApi;         // device.api: "alsa", "bluez5", ...
      QStringList positions;     // the node's audio.position, e.g. FL,FR or AUX0..AUX9
      int alsaCard = -1;         // api.alsa.pcm.card when present
      int alsaDevice = -1;       // api.alsa.pcm.device when present
  };
  class IPipeWireDeviceSystem {
  public:
      virtual ~IPipeWireDeviceSystem() = default;
      virtual bool running() = 0;                        // the daemon answers
      virtual QList<PipeWireNodeRecord> nodes() = 0;
      virtual QString defaultNodeName(AudioDeviceDirection) = 0;   // metadata default.audio.sink / default.audio.source
      virtual void setNoticeSink(std::function<void(AudioNotice)> sink) = 0;
      virtual std::unique_ptr<IAudioBus> createOutput(const PipeWireNodeRecord&, const AudioStreamRequest&) = 0;
      virtual std::unique_ptr<IAudioInputStream> createInput(const PipeWireNodeRecord&, const AudioStreamRequest&, MicChannelPick, IAudioInputSink*) = 0;
  };
  ```
  `class PipeWireDeviceBackend final : public IAudioEngineBackend` (id `PipeWire`).
- Produces (`PipeWireStream.h`): `StreamConfig` gains `QStringList audioPosition`
  (empty: the server's) and `bool dontRemix = false` (`stream.dont-remix`);
  `void PipeWireStream::setMatcherReader(MatcherReader reader)` (before `open()`; the
  output process callback then reads from it instead of `m_ring`) and
  `void PipeWireStream::setInputSink(IAudioInputSink* sink, MicChannelPick pick)`. With
  neither set, the stream behaves exactly as today (Linux VAX is unchanged).

**Acceptance:**
- Nodes to devices: "Audio/Sink" is an output, "Audio/Source" an input, "Audio/Duplex"
  both; nodes whose `node.name` ends in ".monitor" and stream nodes are not listed;
  `deviceApi` "bluez5" gives Bluetooth; "alsa" with a card gives `alsaCard` and
  `alsaDevice` (the R-AUD-05 card allowance); `channelCount` is the number of positions.
- Pairs: a node with positions AUX0 to AUX9 gives 10 channels; a stream opened on pair
  3-4 sets `audioPosition` to the node's positions, `dontRemix` true, `target.object` to
  the node, and writes the pair into channels 3 and 4 with every other position zero
  (through `writeStereoToDevice` on the interleaved buffer); a 2-channel node opens with
  2 channels as today. (Pending V-HW-4: whether `stream.dont-remix` holds the pair on a
  real interface.)
- `configToProperties` sets `node.latency` to "<bufferFrames>/<rate>" (default 128/48000,
  design choice 11), `stream.dont-remix` "true" only when set, `audio.position` only when
  set (`tst_pipewire_stream_config` cases added; its existing cases unchanged).
- Notices: a registry global added or removed for a node posts `DevicesChanged`; the
  metadata `default.audio.sink` / `default.audio.source` changing posts the default
  notice; the listener runs on the thread loop with its lock held and only posts.
- The matcher output mode: the process callback takes no lock of ours and allocates
  nothing (PipeWire holds its own loop lock around it); `takesStereoMix()` is true;
  device buffer = the quantum the graph reports (`SPA_PARAM_Latency` or the position's
  duration), device latency from `pw_stream_get_time_n`'s delay.
- `running()` false (no daemon): `PipeWire` is listed by the catalogue as not running,
  greyed by the UI (Task 16).
- `audioDevicesBarredForTestRun()` makes every open fail.

**Verification:** a device the system answers, plus the core RX path: contract tests
with a fake system, run in the Linux lane; the adapter compiles there too. Linux lane:
targets `tst_pipewire_device_backend tst_pipewire_stream_config tst_pipewire_output_frames`.
Hardware (pending, JJ): V-HW-4 and V-HW-8 on the PipeWire desktop.

**Execution note (advisory):** opus. After Task 8 (registry order). Core RX path;
flagged. The Mac build must stay green (`NEREUS_HAVE_PIPEWIRE` off there).

- [ ] **Step 1:** Fake-system tests and the new `configToProperties` cases.
- [ ] **Step 2:** The backend, the adapter, the stream modes, registry and CMake; Linux
  lane green; Mac build green; commit (R-AUD-01, R-AUD-03, R-AUD-07).

## Task 11: PulseAudio and which Linux engine runs

**Requirements:** R-AUD-01 (Linux with PulseAudio, and neither running), R-AUD-02,
R-AUD-07 (PulseAudio pairs "where the interface's profile exposes its channels"),
R-AUD-31 ("On Linux one build carries both the PipeWire and PulseAudio engines; which is
offered depends on which sound server answers at run time."), D16, design choice 6,
settled calls 21 and 23. V-HW-5 stays untested on hardware.

**Files:**
- Create: `src/core/audio/PulseAudioSystem.{h,cpp}`, `src/core/audio/PulseAudioBackend.{h,cpp}`,
  `src/core/audio/PulseAudioBus.{h,cpp}`, `src/core/audio/LinuxEngineSelection.{h,cpp}`,
  `tests/tst_pulse_audio_backend.cpp`, `tests/tst_linux_engine_selection.cpp`
- Modify: `src/core/audio/LinuxAudioBackend.{h,cpp}` (detection reads
  `LinuxEngineSelection`; `Audio/LinuxBackendPreferred` still forces), `src/core/AudioEngine.cpp`
  (`rescanLinuxBackend` at `:503-537` uses the selection), `src/core/audio/AudioBackendRegistry.cpp`,
  `CMakeLists.txt` (`pkg_check_modules(PULSE IMPORTED_TARGET libpulse)` beside
  PipeWire's at `:817-824`, `HAVE_PULSEAUDIO`, Linux sources, link at `:2436-2449`),
  `tests/CMakeLists.txt`, `.github/workflows/ci.yml` (`:400`, `:582`),
  `.github/workflows/release.yml` (`:149`, `:407`), `.github/workflows/codeql.yml` (`:50`),
  `packaging/station-image/build-trixie-deb.sh` (`:20`), and the package dependency
  lists that name `libpipewire` (add `libpulse0` beside it)

**Interfaces:**
- Consumes: Tasks 2, 3, 7, 10.
- Produces (`LinuxEngineSelection.h`):
  ```cpp
  struct LinuxSoundServerProbe {
      bool pipewireAnswers = false;      // IPipeWireDeviceSystem::running()
      std::optional<QString> pulseServerName;   // pa_server_info.server_name when a context connected within 1 s
      QString forced;                    // Audio/LinuxBackendPreferred, as today
  };
  struct LinuxEngineChoice { bool pipewireRunning; bool pulseRunning; };
  // PipeWire runs only when NereusSDR's PipeWire engine answers; otherwise any
  // PulseAudio server, PipeWire's own PulseAudio service included, runs the
  // PulseAudio engine (R-AUD-01).
  LinuxEngineChoice chooseLinuxEngines(const LinuxSoundServerProbe&);
  ```
- Produces (`PulseAudioSystem.h`): `struct PulseDeviceRecord { QString name; QString description; bool isSink; bool isMonitor; QStringList channelMap; QString busProperty; int alsaCard = -1; int alsaDevice = -1; }`
  and `class IPulseAudioSystem` with `running()`, `devices()`, `defaultName(direction)`,
  `setNoticeSink(...)`, `createOutput(...)`, `createInput(...)` in the same shape as
  Task 10's seam; `class PulseAudioBackend final : public IAudioEngineBackend` (id
  `PulseAudio`).

**Acceptance:**
- Selection: PipeWire answering gives PipeWire running and PulseAudio not; PipeWire not
  answering with a server named "pulseaudio" gives PulseAudio running; a server named
  "PulseAudio (on PipeWire 1.0.5)" gives PipeWire when PipeWire answers, and PulseAudio
  when PipeWire does not answer or NereusSDR is built without the PipeWire engine;
  neither gives both not running (the
  older drivers remain, migration is `Postponed`, settled call 23); `forced` "pipewire"
  or "pulse" behaves as `detectLinuxBackend` does today
  (`LinuxAudioBackend.cpp:61-90`, its eight combinations in `tst_linux_audio_backend`
  still pass).
- Amended 2026-10-09 after the final review: a PulseAudio server named for PipeWire
  counts as PipeWire only when NereusSDR's PipeWire engine is built and answering;
  otherwise the PulseAudio engine runs (`LinuxEngineSelection.cpp`
  `chooseLinuxEngines`).
- Registry on Linux desktops: PipeWire then PulseAudio then PortAudio
  (`includeReplacedHostApis` false); both native backends are always registered, and
  `running()` reports the selection, so the UI shows "PipeWire (not running)" or both
  greyed (R-AUD-01). `defaultAudioEngine` picks PipeWire, else PulseAudio, else PortAudio.
- PulseAudio devices: monitor sources are not listed; `name` is the saved DeviceId,
  `description` the display name; `busProperty` "bluetooth" gives Bluetooth, "usb"
  gives Usb; `channelMap` gives `channelCount`; pairs are offered only when the map
  lists more than two channels (pending: no PulseAudio desktop, V-HW-5).
- Notices: `pa_context_subscribe` for sinks, sources and the server posts
  `DevicesChanged`, and a server change event with a changed default sink or source
  posts the default notice; the subscribe callback only posts.
- The stream: a playback `pa_stream` on the threaded mainloop with `tlength` and
  `minreq` from the buffer frames (default 128 frames each) and `PA_STREAM_ADJUST_LATENCY`;
  its write callback (run under libpulse's own mainloop lock) reads
  `MatcherReader::read` into a buffer from `pa_stream_begin_write`; our code there takes
  no lock and allocates nothing; `takesStereoMix()` true; device latency from
  `pa_stream_get_latency`.
- Without libpulse at build time (`HAVE_PULSEAUDIO` off) the backend is not registered
  and every test except the selection test is skipped by CMake, as PipeWire's are.
- `audioDevicesBarredForTestRun()` makes every open fail.

**Verification:** a device the system answers: the selection and mapping tested with
fakes; the adapter compiles in the Linux lane only if JJ approved the libpulse image in
pre-flight, otherwise in CI after the push. Linux lane:
targets `tst_linux_engine_selection tst_pulse_audio_backend tst_linux_audio_backend`.
Mac: `tst_linux_engine_selection` builds and runs on the Mac too (portable).
Hardware: V-HW-5 is "not tested on hardware" in the release notes.

**Execution note (advisory):** opus. After Task 10. Touches CI and packaging lists;
flagged with the core RX path.

- [ ] **Step 1:** Selection and device-mapping tests.
- [ ] **Step 2:** Selection, backend, stream, CMake, CI and package lines; commit
  (R-AUD-01, R-AUD-02, R-AUD-31).

## Task 12: ALSA direct on the Core

**Requirements:** R-AUD-25 ("The Core plays to its sound card through the ALSA direct
engine. Cards plugged in or out appear and go within 1 s, and a chosen card that goes
away falls back to the Core's default card and back, as R-AUD-08."), R-AUD-30 (detection
part), R-AUD-01 (the Core line), D18, D31, design choices 4, 5 and 7, settled calls 9,
10, 13, 14 and 33. V-HW-6 and V-HW-7 pending.

**Files:**
- Create: `src/core/audio/AlsaDirectSystem.{h,cpp}`, `src/core/audio/AlsaDirectBackend.{h,cpp}`,
  `src/core/audio/AlsaDirectBus.{h,cpp}`, `tests/tst_alsa_direct_backend.cpp`
- Modify: `src/core/audio/AudioBackendRegistry.cpp` (the Linux Core registers ALSA
  direct only), `src/core/AudioEngine.cpp` (migration context `alsaDirectOnly` on the
  Linux Core), `CMakeLists.txt` (`find_package(ALSA)`, Linux sources, link
  `ALSA::ALSA` to `NereusCore` on Linux), `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Tasks 2, 3, 5, 7.
- Produces (`AlsaDirectSystem.h`):
  ```cpp
  struct AlsaCardRecord {
      int card = -1;
      QString cardId;            // snd_ctl_card_info_get_id, e.g. "Headphones", "Device"
      QString cardName;          // snd_ctl_card_info_get_name, e.g. "bcm2835 Headphones"
      int device = 0;            // PCM device number with playback
      int channels = 2;
      QString bus;               // "usb" when the card's /proc/asound/cardN/usbid exists, else ""
  };
  class IAlsaDirectSystem {
  public:
      virtual ~IAlsaDirectSystem() = default;
      virtual QList<AlsaCardRecord> playbackCards() = 0;
      virtual std::optional<int> configuredDefaultCard() = 0;   // ALSA's defaults.pcm.card when set
      virtual void setNoticeSink(std::function<void(AudioNotice)> sink) = 0;  // inotify on /dev/snd, own thread
      virtual std::unique_ptr<IAudioBus> createOutput(const AlsaCardRecord&, const AudioStreamRequest&) = 0;
      virtual QString defaultTargetPath() = 0;                  // where default.target points, "" when absent
  };
  // Settled call 13: a box "starts into a desktop" when default.target resolves to
  // graphical.target, read from /etc/systemd/system/default.target, else
  // /lib/systemd/system/default.target, else /usr/lib/systemd/system/default.target.
  bool coreBoxStartsIntoDesktop(IAlsaDirectSystem& system);
  // Settled call 14: S32_LE, S24_3LE, S24_LE, S16_LE.
  QList<DeviceSampleFormat> alsaFormatOrder();
  ```
  `class AlsaDirectBackend final : public IAudioEngineBackend` (id `AlsaDirect`; device
  `id` is "<cardId>,<device>", `name` the card name; `alsaCard` / `alsaDevice` set;
  `isDefault` from settled call 10).

**Acceptance:**
- Listing with a fake system: two cards ("bcm2835 Headphones" card 0, "USB Audio Device"
  card 2 with bus "usb") give two outputs, IDs "Headphones,0" and "Device,0"; transport
  Usb for the second; the default is card `configuredDefaultCard()` when set, else the
  lowest-numbered card with playback; an HDMI card (`cardId` starting "vc4hdmi") gets
  Hdmi transport.
- Notices: an inotify event for a `controlC*` or `pcmC*D*p` node created or deleted in
  `/dev/snd` posts `DevicesChanged`; events for other names are ignored; the inotify
  thread only posts. A card plugged in shows in the catalogue within the debounce plus
  100 ms (with the fake's notice).
- Fall-back and return through the supervisor (Task 5) exactly as R-AUD-08: the chosen
  card's stream reports `DeviceLost` (`-ENODEV` from a write), the default card opens at
  once, the chosen card reopens when listed again.
- The stream (`AlsaDirectBus`): opens `hw:<card>,<device>` with `SND_PCM_NONBLOCK` off,
  interleaved access, the first format of `alsaFormatOrder()` the card accepts, the
  card's nearest rate to 48000 (the matcher resamples), period 128 frames and 3 periods
  unless the request sets a buffer; a writer thread at `SCHED_FIFO` (through
  `elevateAudioThreadPriority`) loops `snd_pcm_writei` from `MatcherReader::read` and
  `writeStereoToDevice`; `-EPIPE` recovers with `snd_pcm_prepare` and counts; `-EBUSY`
  at open returns `InUse` (R-AUD-11); `-ENODEV` posts `DeviceLost`; `takesStereoMix()`
  true; device buffer = period frames, device latency = `snd_pcm_delay` at open.
- Desktop detection: `defaultTargetPath()` "/lib/systemd/system/graphical.target" gives
  true; "…/multi-user.target" gives false; "" gives false.
- On the Linux Core (`AudioBackendContext::daemon` on Linux) only ALSA direct is
  registered; a saved speakers config of any `DriverApi` migrates to `AlsaDirect`
  (settled call 33), and "USB Audio Device: - (hw:2,0)" matches the card with
  `alsaCard` 2, `alsaDevice` 0. On the Mac and Windows the Core keeps the desktop
  engines (settled call 9).
- `audioDevicesBarredForTestRun()` makes every open fail.

**Verification:** a device the system answers: contract with fakes in the Linux lane;
real cards pending. Linux lane: targets `tst_alsa_direct_backend tst_audio_device_migration`.
Hardware (pending, JJ via the Core session's install): V-HW-6 on the Pi 4 and Pi 5,
V-HW-7 on a Pi desktop image.

**Execution note (advisory):** opus. After Task 11. Core RX path on the Core; flagged.

- [ ] **Step 1:** Fake-system tests for listing, notices, defaults, formats and desktop
  detection.
- [ ] **Step 2:** The system adapter, backend, bus, registry and CMake; Linux lane
  green; commit (R-AUD-25, R-AUD-30).

## Task 13: The PC mic over shared memory, native engines in the helper

**Requirements:** R-AUD-17 ("The PC mic hand-off follows "The PC mic hand-off". The mic's
Device details show its delay now: the parts of R-AUD-15 in the other direction, plus the
hop. Target: under 1 ms for the hop between the helper and the window."), R-AUD-18 (the
helper part, through settled call 31), R-AUD-09 and R-AUD-14 (the helper's side), D22,
V-SW-6, V-HW-9. R-R3-36's protection against a hanging mic stays.

**Files:**
- Create: `src/core/audio/CaptureShm.{h,cpp}`, `tests/tst_capture_shm_ring.cpp`
- Modify: `src/core/audio/CaptureProtocol.{h,cpp}` (version 3), `src/core/audio/CaptureHelper.{h,cpp}`
  (native input through the registry, the matcher as writer, the probe detector in the
  input sink), `src/core/audio/CaptureSupervisor.{h,cpp}` (creates the ring and the wake
  signal, waiter thread, hop statistics), `src/core/audio/CaptureAudioBus.{h,cpp}` (reads
  the ring), `src/capture_main.cpp`, `src/core/audio/AudioDelayProbe.{h,cpp}` (the
  detector takes stereo input from `IAudioInputSink`), `src/core/AudioEngine.cpp` (the
  mic's `delayParts`, the busy reason), `tests/tst_capture_protocol.cpp`,
  `tests/tst_capture_helper_process.cpp`, `tests/tst_capture_supervisor.cpp`,
  `tests/fakes/FakeCaptureChild.{h,cpp}`, `tests/CMakeLists.txt`, `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 2 (`DeviceRateMatcher` with caller memory, `MatcherReader`,
  `matcherRingBytes`, `attachMatcherRing`), Task 3 (`IAudioInputSink`,
  `IAudioInputStream`), Tasks 7 to 11 (registry with `AudioBackendContext{helper = true}`),
  Task 4 (`matchSavedAudioDevice`), Task 1 (detector).
- Produces (`CaptureShm.h`):
  ```cpp
  inline constexpr int kCaptureShmNameMaxChars = 30;
  struct CaptureShmNames { QString memory; QString wake; };   // POSIX "/nrsc-<pid>-<8hex>m" and "...w"; Windows "Local\\nrsc-<pid>-<8hex>m" / "...w"
  CaptureShmNames makeCaptureShmNames(qint64 pid, quint32 random);
  class CaptureShmRegion {                  // owner side (the window) or attach side (the helper)
  public:
      static std::unique_ptr<CaptureShmRegion> create(const CaptureShmNames&, std::size_t bytes);
      static std::unique_ptr<CaptureShmRegion> attach(const CaptureShmNames&, std::size_t bytes);
      void* data() const;
      std::size_t size() const;
      void unlinkNames();                   // POSIX: shm_unlink and sem_unlink; Windows: nothing
      void postWake();                      // sem_post / SetEvent; the only call the helper's callback makes
      bool waitWake();                      // sem_wait / WaitForSingleObject(INFINITE); false after shutdownWake()
      void shutdownWake();                  // sets the stop flag and posts once
  };
  ```
- Produces (`CaptureProtocol.h`): `kVersion = 3`; `RecordType::RingAttached = 5`
  (helper to window, JSON `{"generation": n}`) and `RecordType::AttachRing = 21` (window
  to helper, JSON `{"generation": n, "memory": "...", "wake": "...", "bytes": n, "inRate": n}`);
  `Configure`'s device object gains the keys `engine`, `deviceId`, `firstChannel`,
  `micChannel` and `delayMs` (exact key set, as today's decoder enforces); `FailReason`
  gains `DeviceInUse`. The `Pcm` record type and its codec stay in the file (the tests of
  the codec stay), but in version 3 the helper never sends one and the window treats one
  as a `ProtocolError`.
- Produces: `CaptureSupervisor::Status` gains `std::optional<double> hopMs` (the last
  10 s median); `AudioEngine::delayParts(AudioRole::TxInput)` returns the mic's parts
  (device latency and buffer from the helper's `Status`, the matcher fill and resampler
  read from the ring header, the hop as part of the fill).

**Acceptance:**
- Names: `makeCaptureShmNames(4194303, 0xdeadbeef)` gives "/nrsc-4194303-deadbeefm" and
  "/nrsc-4194303-deadbeefw" on POSIX (23 characters, under the macOS 31 limit) and
  "Local\nrsc-4194303-deadbeefm" on Windows.
- Ring (V-SW-6, `tst_capture_shm_ring`): a region created in this process and attached
  through the names in a child process started by the test passes a numbered ramp of
  1,000,000 stereo frames in order with no frame lost or repeated outside a counted dry
  run, with the reader running 10 % slow, then 10 % fast (both through a forced matcher
  ratio of 1.0, so the order check is exact); the dry runs and overruns counted equal
  the gaps found.
- Hand-off: the window creates the region (size `matcherRingBytes` for a matcher with
  `inRate` the device rate, `outRate` 48000, `callbackFrames` the device buffer) and the
  wake signal, sends `AttachRing` after `Configure`, and unlinks the names when
  `RingAttached` arrives (or when the generation ends); the helper attaches, builds its
  `DeviceRateMatcher` in that memory, sends `RingAttached`, opens the input through the
  registry's backend for the config's `engine`, and in `onInput` writes into the matcher
  and calls `postWake()`; nothing else runs in the callback.
- The window: `CaptureAudioBus::pull` reads through a `MatcherReader` on the attached
  header (left channel of the stereo ring, 48 kHz mono float as today); a waiter thread
  blocks in `waitWake()`, records the hop (`audioProbeNowNs()` − `lastWriteNs`) for the
  10 s delay log that `kDelayWindowMs` already drives (V-HW-9 reads it), and publishes
  the last wake time; the first wake of a generation ends the open deadline in place of
  the first PCM record; shutdown calls `shutdownWake()` and joins the thread within
  `stopTimeoutMs`.
- Engine use: the helper's registry has the input engines of its system (Mac Core Audio;
  Windows Windows audio, then PortAudio; Linux PipeWire, PulseAudio, PortAudio); it
  matches the saved device with `matchSavedAudioDevice`; not found gives `Status Failed`
  with `DeviceNotFound`; an open refused as busy gives `DeviceInUse` (the window maps it
  to `InUse`, R-AUD-11); a device lost mid-stream gives `InputLost` (R-AUD-09: the
  window's supervisor keeps the mic silent). A Bluetooth device picked by name opens
  (R-AUD-14); the helper never picks a device itself.
- The probe detector runs in the helper's `onInput` on the picked channel; `ProbeHit`
  keeps its record from Task 1.
- R-R3-36 (V-SW-6): a helper that hangs while opening (the fake child's "hang" mode)
  still leaves the window responsive and fails with `Timeout` after `openTimeoutMs`;
  every existing `tst_capture_supervisor` and `tst_capture_helper_process` case passes
  with the version bumped.
- `tst_pc_mic_source`, `tst_tx_mic_source` and `tst_audio_engine_pull_tx_mic` stay green.

**Verification:** transmit input feeding the keying gate: invariant tests first
(order, no loss, hang). Unit (Mac):
`cmake --build build --target tst_capture_shm_ring tst_capture_protocol tst_capture_supervisor tst_capture_helper_process tst_pc_mic_source tst_tx_mic_source tst_audio_engine_pull_tx_mic tst_audio_delay_probe nereus-audio-capture && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_capture_shm_ring|tst_capture_protocol|tst_capture_supervisor|tst_capture_helper_process|tst_pc_mic_source|tst_tx_mic_source|tst_audio_engine_pull_tx_mic|tst_audio_delay_probe)$' --no-tests=error --output-on-failure`.
Linux lane: `tst_capture_shm_ring tst_capture_supervisor`. Hardware (pending, JJ):
V-HW-9 on the Mac and the Windows PC; V-HW-1's AirPods mic case.

**Execution note (advisory):** opus. After Tasks 8 to 11. Mic and transmit input,
flagged (it feeds R-R3-36's gate). The Windows half of `CaptureShm` compiles in CI only.

- [ ] **Step 1:** Name and ring tests, the version 3 protocol cases.
- [ ] **Step 2:** `CaptureShm`, the protocol, the helper side; commit (R-AUD-17).
- [ ] **Step 3:** The window side (waiter, bus, supervisor), the mic delay parts, the
  probe move; every listed test green; commit (R-AUD-17, R-AUD-18).

## Task 14: The ASIO SDK and the licence notes

**Requirements:** R-AUD-33 (bug 7: "`CMakeLists.txt:532-541` and
`2026-04-19-vax-design.md` section 8.5 say what the ASIO licence is now"), D3, Global
Constraints "ASIO licence".

**Files:**
- Create: `third_party/asiosdk/` (the SDK as released, unchanged), `third_party/asiosdk/VERSION.txt`,
  `third_party/asiosdk/CMakeLists.txt` (a static library of the host files, Windows
  only), `docs/attribution/ASIO-SDK-PROVENANCE.md`, the SDK's licence text in
  `packaging/third-party-licenses/`
- Modify: `CMakeLists.txt` (the comment at `:532-541`: PortAudio's own ASIO host stays
  off because NereusSDR hosts ASIO itself; the SDK is GPL-3.0-only since 2.3.4 and
  NereusSDR elects GPL-3 (`LICENSE-NOTICE`); `add_subdirectory(third_party/asiosdk)` in
  `if(WIN32)`), `docs/architecture/2026-04-19-vax-design.md` (section 8.5 states the
  licence now, keeping the history), `packaging/third-party-licenses/README.md` (a
  licence row and a sources row for `third_party/asiosdk`)

**Interfaces:**
- Consumes: JJ's yes for the download (pre-flight).
- Produces: CMake target `asiosdk_host` (Windows only; the SDK's `common/asio.cpp`,
  `host/asiodrivers.cpp`, `host/pc/asiolist.cpp` and their include folders), linked only
  into `nereus-audio-capture` by Task 15.

**Acceptance:**
- The SDK files are byte-identical to the release archive; `VERSION.txt` records the
  version (2.3.4), the download URL, the date and the archive's SHA-256, as the
  controller observed them.
- `python3 scripts/check-third-party-licenses.py` passes with the new rows;
  `PA_USE_ASIO` stays OFF.
- No SDK file is compiled on the Mac or Linux; `cmake --build build` on the Mac is
  unchanged.

**Verification:** documentation and vendoring: the licence check and a read-through;
the Windows library builds in CI (pending the push).

**Execution note (advisory):** sonnet (vendoring and text). The controller downloads the
SDK after JJ's yes and hands the archive path to the implementer; the implementer never
downloads. After Task 13.

- [ ] **Step 1:** Vendor the SDK, `VERSION.txt`, the CMake library, provenance and
  licence rows, the two notes; licence check green; commit (R-AUD-33).

## Task 15: ASIO in the mic helper

**Requirements:** R-AUD-01 (the "ASIO" entry), R-AUD-07 (ASIO pairs), R-AUD-19,
R-AUD-20, R-AUD-21, R-AUD-22 (the engine side), D3, D9, D14, D15, D33, V-SW-7.
V-HW-3 pending.

**Files:**
- Create: `src/core/audio/IAsioDriver.h`, `src/core/audio/AsioSession.{h,cpp}` (portable
  rules), `src/core/audio/AsioDriverWin.{h,cpp}` and `src/core/audio/cmasio.{h,cpp}`
  (Windows only, in the helper), `src/core/audio/AsioBackend.{h,cpp}` (the window's view
  of ASIO through the helper), `tests/fakes/FakeAsioDriver.h`, `tests/tst_asio_session.cpp`
- Modify: `src/core/audio/CaptureProtocol.{h,cpp}` (version 4, ASIO records),
  `src/core/audio/CaptureHelper.cpp`, `src/core/audio/CaptureSupervisor.{h,cpp}`
  (`Demand::AsioDevice`; the helper runs while any device uses ASIO),
  `src/core/audio/AudioBackendRegistry.cpp` (Windows: Windows audio, ASIO, PortAudio),
  `src/core/AudioEngine.cpp` (ASIO outputs write into a shared-memory ring the helper
  reads), `CMakeLists.txt` (the Windows files into `nereus-audio-capture` only, linked
  with `asiosdk_host`), `tests/CMakeLists.txt`, `docs/attribution/THETIS-PROVENANCE.md`
  (rows for `cmasio.h` and `cmasio.cpp`, Type `port`, Variant `thetis-samphire`)

**Interfaces:**
- Consumes: Task 13 (`CaptureShmRegion`, the matcher in shared memory, the protocol),
  Task 14 (`asiosdk_host`), Task 3 (`DeviceSampleFormat`, `writeStereoToDevice`,
  `readDeviceToStereo`).
- Produces (`IAsioDriver.h`, portable):
  ```cpp
  enum class AsioSampleType { Int16Lsb, Int24Lsb, Int32Lsb, Float32Lsb, Float64Lsb, Unsupported };
  struct AsioDriverCaps {
      QString name;
      int inputChannels = 0;
      int outputChannels = 0;
      AsioSampleType sampleType = AsioSampleType::Int32Lsb;
      int minBufferFrames = 0, maxBufferFrames = 0, preferredBufferFrames = 0, granularity = 0;
      QList<double> sampleRates;      // the rates canSampleRate accepted, from 44100, 48000, 88200, 96000, 176400, 192000
      double currentRate = 0.0;
      std::int64_t inputLatencyFrames = 0, outputLatencyFrames = 0;
  };
  enum class AsioMessage { ResetRequest, BufferSizeChange, ResyncRequest, LatenciesChanged };
  class IAsioDriver {
  public:
      virtual ~IAsioDriver() = default;
      virtual QStringList installedDrivers() = 0;
      virtual std::optional<AsioDriverCaps> load(const QString& name) = 0;      // loads and initialises
      virtual bool createBuffers(const QList<int>& inputChannels, const QList<int>& outputChannels, int bufferFrames) = 0;
      virtual bool setSampleRate(double rate) = 0;
      virtual bool start() = 0;
      virtual void stop() = 0;
      virtual void disposeAndUnload() = 0;
      virtual void openControlPanel() = 0;
      virtual void setCallbacks(std::function<void(int bufferIndex)> bufferSwitch, std::function<void(AsioMessage)> message) = 0;
  };
  ```
- Produces (`AsioSession.h`, portable):
  ```cpp
  struct AsioUse { AudioRole role; QString driver; AudioChannelPair pair; AudioDeviceDirection direction; };
  struct AsioSwitchPlan { QString toDriver; QList<AsioUse> moves; };   // every other ASIO use and the pair it moves to
  // Settled call 17: each use keeps its pair numbers where the new driver has them, else its first pair.
  AsioSwitchPlan planAsioSwitch(const QList<AsioUse>& current, const AsioUse& requested, const AsioDriverCaps& newDriver);
  bool asioBufferSizeFixed(const AsioDriverCaps&);                     // min == max (asio.h:640-642)
  std::optional<DeviceSampleFormat> asioDeviceFormat(AsioSampleType);   // nullopt: unsupported (settled call 28)
  class AsioSession {                       // runs in the helper; one driver at a time (R-AUD-19)
  public:
      explicit AsioSession(IAsioDriver& driver);
      bool open(const QString& driver, int bufferFrames, double rate, const QList<AsioUse>& uses);
      void close();
      void onDriverMessage(AsioMessage message);      // from the driver's thread: only posts
      int restartCount() const;
      AsioDriverCaps caps() const;
  signals:  // through a QObject member: restarted(), failed(QString)
  };
  ```
- Produces (`CaptureProtocol.h`): `kVersion = 4`; records `AsioDescribe = 22` (window
  to helper, `{"driver": "..."}`), `AsioCaps = 6` (helper to window, the caps as JSON),
  `AsioOpen = 23` (window to helper: driver, buffer, rate, and per use its role, pair,
  direction and ring names), `AsioState = 7` (helper to window: `running`,
  `restarted`, `inUse`, `failed` with detail), `AsioControlPanel = 24`.
- Produces: `class AsioBackend final : public IAudioEngineBackend` (id `Asio`, in the
  window; lists the installed drivers as devices with their channel counts from
  `AsioCaps`; `hasControlPanel()` true; `openControlPanel` sends `AsioControlPanel`;
  `createOutput` returns a bus whose `push()` writes into a shared-memory matcher ring
  that the helper's buffer switch reads; `createInput` is the mic path of Task 13 with
  the ASIO pair).

**Acceptance (V-SW-7, with `FakeAsioDriver`):**
- `planAsioSwitch`: speakers on "Focusrite USB ASIO" 3-4, headphones on its 1-2 and VAX 1
  on its inputs 1-2; requesting the mic on "MOTU M Series" (4 outputs, 4 inputs) plans
  speakers to 3-4, headphones to 1-2 and VAX 1 to inputs 1-2; a new driver with 2
  outputs moves speakers 3-4 to 1-2; the plan lists every other ASIO use, VAX included;
  an empty plan when nothing else uses ASIO. Cancel (the UI's, Task 17) applies nothing:
  the session is not touched until a plan is accepted.
- Buffer and rate are one value for the whole session: opening uses with two different
  buffer requests uses the latest; caps with min 256 and max 256 make
  `asioBufferSizeFixed` true (R-AUD-20).
- `asioDeviceFormat`: Int16Lsb, Int24Lsb (packed), Int32Lsb, Float32Lsb, Float64Lsb map
  to Int16, Int24Packed, Int32, Float32, Float64; anything else nullopt.
- Reset (R-AUD-21): `onDriverMessage(ResetRequest)` from a foreign thread returns at
  once; the session stops, disposes and unloads, loads, creates buffers and starts again
  on its own thread; `restartCount()` becomes 1; `restarted()` is emitted; streams on
  other engines are untouched (a fake Windows audio bus keeps reading). `BufferSizeChange`
  is handled as a reset ("Restarted with the driver's new settings." is Task 17's text).
- Busy: `load` failing because another program holds the driver gives `inUse` and the
  window maps it to `InUse` for every role on that driver (R-AUD-11).
- The buffer switch (portable part, tested with the fake calling it): for each output
  use it reads its `MatcherReader` into the driver's planar buffers with
  `writeStereoToDevice` (`interleaved` false, the use's pair); for each input use it
  writes `readDeviceToStereo` output into that use's ring and posts its wake; it takes
  no lock and allocates nothing (the test hook of Task 2 counts it).
- The callbacks reach the session through one `static std::atomic<AsioSession*>`, as the
  SDK's C callbacks require; `cmasio.cpp` keeps cmASIO's header byte-for-byte (Bryan
  Rambo W4WMT, GPL v2 or later, with the Samphire dual-licence statement) and a
  Modification history block, and ports `create_cmasio`'s set-up order and its base
  channel and input-mode handling (`cmasio.c:47-102`, with the `//[2.10.3.13]MW0LGE`
  comments kept verbatim); where ours differs it says so in the header: the matcher in
  place of rmatchV and its semaphores (D34), the control panel (R-AUD-22), and a restart
  in place of a stop on reset (D14).
- The helper runs while any role uses ASIO (`Demand::AsioDevice` held by the engine),
  and ASIO outputs keep playing with the PC mic off.
- `audioDevicesBarredForTestRun()` keeps `AsioBackend` from starting a helper in tests.

**Verification:** a device the system answers, mic input and outputs: the session rules
and the buffer switch tested on the Mac with the fake driver; the Windows files compile
in CI after the push; behaviour pending the bench. Unit (Mac):
`cmake --build build --target tst_asio_session tst_capture_protocol tst_capture_supervisor && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_asio_session|tst_capture_protocol|tst_capture_supervisor)$' --no-tests=error --output-on-failure`;
`python3 scripts/verify-thetis-headers.py`, `python3 scripts/verify-inline-tag-preservation.py`
and `python3 scripts/verify-provenance-sync.py` pass. CI: Windows builds green (the
controller asks JJ for the push after this task). Hardware (pending, JJ): V-HW-3, and
V-HW-8 with ASIO.

**Execution note (advisory):** opus. After Tasks 13 and 14. Mic and outputs through a
driver of unknown quality; flagged. Read `docs/attribution/HOW-TO-PORT.md` first.

- [ ] **Step 1:** `tst_asio_session` with the fake driver and the exact cases above.
- [ ] **Step 2:** Session, protocol version 4, backend and the window side; Mac tests
  green; commit (R-AUD-19, R-AUD-20, R-AUD-21).
- [ ] **Step 3:** The Windows driver adapter and the cmASIO port with headers and
  provenance rows; header checks green; commit (R-AUD-01, R-AUD-22).


## Task 16: Setup device cards, with the Driver list, live lists, states and delay

**Requirements:** R-AUD-01 (the lists and the Sound system line), R-AUD-02 (UI part),
R-AUD-03 (Setup's cards), R-AUD-06 (the Outputs page's Rescan devices), R-AUD-08,
R-AUD-09, R-AUD-11 and R-AUD-14 (their Setup texts), R-AUD-12 (the readout and the PC
tooltip follow the default), R-AUD-15 (the Delay line), R-AUD-16 (its notes), R-AUD-17
(the mic's delay line), D10, design choices 12 and 13, settled calls 1, 12, 24 and 36.
V-UI-1 (Outputs and Microphone).

**Files:**
- Create: `src/gui/setup/AudioDriverList.{h,cpp}`, `tests/tst_audio_driver_list.cpp`
- Modify: `src/gui/setup/DeviceCard.{h,cpp}` (the Driver list replaces the Driver API
  list and the three WASAPI checkboxes; devices from the catalogue; states; the Delay
  line; the engine notes), `src/gui/setup/AudioOutputsPage.{h,cpp}` (catalogue wiring,
  Rescan devices and its note), `src/gui/setup/AudioTxInputPage.{h,cpp}` (the mic card's
  states, the Bluetooth note), `src/gui/setup/SoundSystemLine.{h,cpp}` (R-AUD-01's
  wording), `src/gui/setup/CaptureStatusText.cpp` (the mic's not connected and in use
  texts), `tests/tst_device_card.cpp`, `tests/tst_audio_setup_regroup.cpp`,
  `tests/tst_capture_status_text.cpp`, `tests/CMakeLists.txt`, `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 3 types and `IAudioDeviceCatalog`; Task 5 `AudioRoleStatus`; Task 7
  `AudioEngine::catalogue()`, `roleStatus`, `delayParts`, `roleStatusChanged`,
  `rescanOlderDrivers`, `defaultEngine`.
- Produces (`AudioDriverList.h`, GUI, pure functions over catalogue data so they test
  without widgets):
  ```cpp
  struct AudioDriverEntry {
      std::optional<AudioEngineKind> engine;   // nullopt: a heading row ("Older drivers")
      QString hostApi;                          // older drivers: the PortAudio host API name
      QString label;                            // "Windows audio, shared", "MME", "PipeWire (not running)"
      bool enabled = true;
      QString disabledReason;
  };
  // R-AUD-01 order for this system, from the catalogue's backends and their running flags.
  QList<AudioDriverEntry> audioDriverEntries(const IAudioDeviceCatalog& catalogue, bool daemonCore);
  struct AudioDeviceEntry {
      QString deviceId;          // empty: "(platform default)"
      QString label;             // audioDeviceEntryLabel(), or "(platform default)", or "<name> (not connected)"
      QString group;             // the interface name when it has pairs, else empty
      AudioChannelPair pair;
      AudioDeviceState state = AudioDeviceState::Present;
      bool bluetooth = false;
  };
  // The Device list for one driver and direction: "(platform default)" first, then each
  // device (pairs grouped), then the saved choice as "<name> (not connected)" when it is missing.
  QList<AudioDeviceEntry> audioDeviceEntries(const IAudioDeviceCatalog& catalogue, AudioEngineKind engine, const QString& hostApi, AudioDeviceDirection direction, const AudioDeviceConfig& saved);
  QString audioRoleNote(AudioRole role, const AudioRoleStatus& status);   // the R-AUD-08 to R-AUD-11 sentence, empty when playing
  QString audioDelayLine(AudioRole role, const AudioDelayParts& parts, const QString& deviceName);
  QString rescanNote(const IAudioDeviceCatalog& catalogue);               // R-AUD-06
  ```
- Produces (`DeviceCard.h`): `void setAudioEngine(AudioEngine* engine)` (the card reads
  the catalogue and its role's status from it and follows their signals);
  `driverApiCombo()` keeps its name and now holds the Driver list; new object names
  `deviceStateNote`, `deviceDelayCombo`, `deviceDelayNow`, `engineNote`.

**Acceptance:**
- Driver lists (`tst_audio_driver_list`, fake catalogues), labels exactly:
  - Mac: one entry "Core Audio", disabled with the reason "The only sound system on the
    Mac."; nothing under Older drivers (settled call 24).
  - Windows: "Windows audio, shared", "Windows audio, exclusive", "ASIO", a heading
    "Older drivers", then "MME", "DirectSound", "WDM-KS".
  - Linux with PipeWire running: "PipeWire", "Older drivers", "JACK", "ALSA".
  - Linux with PulseAudio: "PipeWire (not running)" disabled, "PulseAudio", then the
    older drivers; neither running: "PipeWire (not running)" and "PulseAudio (not
    running)" disabled, then the older drivers.
  - The Core (`daemonCore` true): one entry "ALSA, direct", disabled, with the reason
    "The Core runs without a desktop, so it plays straight to the sound card."
- The three WASAPI checkboxes and `wasapiOnlyNote` are gone from the card (D10); the
  saved `ExclusiveMode`, `EventDriven` and `BypassMixer` keys stay in the file and are
  no longer written.
- Device lists: "(platform default)" first; a device added by the fake catalogue shows
  in the open card within 1 s with no Rescan, keeping the current selection; a saved
  device that is missing stays selected as "<name> (not connected)"; a device in use
  shows "<name> (in use by another program)"; `(none)` shows as "(none)" when saved
  (settled call 12). Picking an entry saves `Engine`, `DeviceId`, `DeviceName` and
  `FirstChannel` and reaches `AudioStreamSupervisor::setChoice` through the engine.
- State notes (`deviceStateNote`), exact:
  - speakers or headphones missing: "<name> is not connected. Playing on the system
    default, <default>, until it comes back.";
  - mic missing: "<name> is not connected. The mic stays silent until it comes back;
    NereusSDR never switches to another mic on its own.";
  - in use: the same sentences with "is in use by another program" in place of "is not
    connected" (R-AUD-11, settled call 35);
  - a Bluetooth mic picked: "Bluetooth headsets switch to phone-call quality, for
    listening too, while they are your mic. For the best sound, listen on <name> and
    talk on a wired or built-in mic." (R-AUD-14).
  - the note clears by itself when the status returns to `Playing`.
- Engine notes (`engineNote`): exclusive "Other apps cannot play through this device
  while NereusSDR has it."; older drivers "An older driver: more delay, and its list
  updates only with Rescan devices."
- Delay line in Device details: a `deviceDelayCombo` with "Automatic", "2 ms", "3 ms",
  "5 ms", "10 ms", "20 ms", "40 ms" saving `DelayMs` 0, 2, 3, 5, 10, 20, 40; beside it
  `deviceDelayNow` "Now <total, rounded to the nearest ms> ms from the radio to <device
  name>" (outputs) or "Now <total> ms from <device name> to the radio" (the mic), "Now
  -- ms" while the role is not playing, refreshed once a second from
  `AudioEngine::delayParts(role)` and at once on `roleStatusChanged` (R-AUD-12); the
  readout names the device actually playing, the default included (settled call 36).
- Sound system line (`soundSystemLine`), exact: "Windows audio (WASAPI)", plus " and
  ASIO (<driver>)" while ASIO is in use; "Core Audio"; "PipeWire. NereusSDR talks to it
  directly."; "PulseAudio. PipeWire was not found, so NereusSDR talks to PulseAudio
  directly."; then " Older drivers in use: <names>." when a card uses one (names
  joined with ", ").
- Rescan devices: calls `AudioEngine::rescanOlderDrivers()`; its note
  (`rescanDevicesResult`) reads "Only the older drivers need this. <labels> lists update
  by themselves." with <labels> "Windows audio and ASIO", "PipeWire" or "PulseAudio";
  on the Mac the button is disabled with "Core Audio lists update by themselves, so
  there is nothing to rescan." On Linux it no longer calls `rescanLinuxBackend()`
  directly (Task 11 moved detection into the engine).
- The PC tooltip text the header uses (Task 19) and the card's status come from the
  same `AudioRoleStatus`, so a default change updates both (R-AUD-12).
- `tst_audio_setup_regroup` and `tst_device_card` stay green with their cases updated
  for the new list (every saved key the cards wrote before is still read the same way).
- V-UI-1 captures, saved when `NEREUS_AUDIO_SETUP_CAPTURE_DIR` is set (the existing
  pattern at `tests/tst_audio_setup_regroup.cpp:305`): Outputs and Microphone for the
  Mac, Windows, Linux with PipeWire and Linux with PulseAudio lists (fake catalogues,
  the list contents decided by the fake, not by the build's system), with a device
  missing, a device in use, the Bluetooth mic note, and Device details unfolded showing
  the Delay line; the controller compares them with `asio-setup-mockup.html`.

**Verification:** UI plus controls that change what plays: functional tests and
captures. Unit:
`cmake --build build --target tst_audio_driver_list tst_device_card tst_audio_setup_regroup tst_capture_status_text && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_audio_driver_list|tst_device_card|tst_audio_setup_regroup|tst_capture_status_text)$' --no-tests=error --output-on-failure`.
UI: captures in `NEREUS_AUDIO_SETUP_CAPTURE_DIR`, compared by the controller with the
mockup (a `sonnet` reader may do the comparison and return its verdict). Human smoke
(JJ, pending): the Outputs and Microphone pages on the Mac with AirPods connecting and
leaving.

**Execution note (advisory):** opus. After Tasks 7 to 13 (the lists need every engine's
backend id). Not parallel with Tasks 17 to 22 (they share `DeviceCard` and the pages).

- [ ] **Step 1:** `tst_audio_driver_list` with the exact lists and strings; the card and
  regroup test updates.
- [ ] **Step 2:** `AudioDriverList`, the card, the pages, the Sound system line and
  Rescan; captures; commit (R-AUD-01, R-AUD-03, R-AUD-06, R-AUD-15).

## Task 17: Interface pairs, ASIO details and the one-driver prompt

**Requirements:** R-AUD-07 (UI: pairs grouped, the same-pair note, "Mic is on"),
R-AUD-19 (the prompt), R-AUD-20 (shared buffer and rate, the greyed size), R-AUD-21
(the restarted note), R-AUD-22 (the control panel button), D9, D14, D15, settled calls
17, 22 and 28. V-SW-7 (the prompt part), V-UI-1 (pairs and ASIO).

**Files:**
- Create: `src/gui/setup/AsioSwitchAllDialog.{h,cpp}`, `tests/tst_device_card_pairs_asio.cpp`
- Modify: `src/gui/setup/DeviceCard.{h,cpp}`, `src/gui/setup/AudioOutputsPage.cpp`,
  `src/gui/setup/AudioTxInputPage.cpp`, `src/gui/setup/AudioDriverList.{h,cpp}`,
  `src/core/AudioEngine.{h,cpp}` (ASIO session status for the cards),
  `tests/CMakeLists.txt`, `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 15 `planAsioSwitch`, `AsioSwitchPlan`, `AsioUse`, `AsioDriverCaps`,
  `asioBufferSizeFixed`, `asioDeviceFormat`, the `AsioState` record; Task 16
  `AudioDriverList`, `DeviceCard::setAudioEngine`.
- Produces (`AudioEngine.h`):
  ```cpp
  struct AsioStatus {
      QString driver;                    // empty: no ASIO in use
      std::optional<AsioDriverCaps> caps;
      int bufferFrames = 0;
      double sampleRate = 0.0;
      QList<AudioRole> users;            // every role on this driver
      bool restartedRecently = false;    // true for 5 s after a restart (R-AUD-21)
      bool formatUnsupported = false;    // settled call 28
  };
  AsioStatus asioStatus() const;
  AsioSwitchPlan planAsioSwitchFor(AudioRole role, const QString& driver, AudioChannelPair pair) const;
  void applyAsioSwitch(const AsioSwitchPlan& plan);   // saves and reopens every moved role
  void setAsioBufferAndRate(int bufferFrames, double sampleRate);   // saves audio/Asio/BufferFrames and audio/Asio/SampleRate
  void openAsioControlPanel();
  signals:
  void asioStatusChanged();
  ```
- Produces: `class AsioSwitchAllDialog : public QDialog` with
  `AsioSwitchAllDialog(const QString& device, const QString& driver, const QList<QPair<QString, QString>>& moves, QWidget* parent)`
  (each move is the device's name and the pair it moves to); object names
  `asioSwitchAllOk`, `asioSwitchAllCancel`.

**Acceptance:**
- Pairs in the Device list: a 10-output interface is a group "Focusrite USB ASIO" with
  entries "Outputs 1-2" to "Outputs 9-10" indented under it; a 5-channel one ends with
  "Output 5" (settled call 22); the same on Core Audio and PipeWire fakes. Picking a
  pair saves `FirstChannel`.
- Speakers and headphones on the same pair of one interface: both cards show
  "Speakers and headphones are on the same pair, so they play together."; different
  pairs show nothing.
- The mic card on an interface with pairs shows "Mic is on:" with Left, Right and Both,
  saving `MicChannel`; hidden never, greyed with the mic off.
- The prompt (V-SW-7, through `planAsioSwitchFor`): speakers on "Focusrite USB ASIO"
  3-4 and VAX 1 on its inputs 1-2; picking the mic on "MOTU M Series" opens the dialog
  with "NereusSDR can use one ASIO driver at a time. Switching Microphone to MOTU M
  Series also moves:" and the lines "Speakers: Outputs 3-4" and "VAX 1: Inputs 1-2",
  with the buttons "Switch all to MOTU M Series" and "Cancel". Cancel restores the
  card's previous selection and writes no key; OK calls `applyAsioSwitch` and every
  moved card shows its new pair.
- ASIO Device details: "Buffer size" and "Sample rate" lists from `AsioDriverCaps`
  (rates it accepts, buffer sizes from min to max by granularity, or powers of two
  when granularity is −1, as `asio.h` documents), changing either in one card changes
  it in every card on that driver (`setAsioBufferAndRate`); the note "Buffer size and
  sample rate are shared with <devices>, on the same ASIO driver." lists the other
  roles by their page names joined with ", " and " and "; a driver with min equal to
  max shows the size greyed with "Set in the ASIO control panel" (R-AUD-20).
- "ASIO control panel" button (`asioControlPanel`): enabled on an ASIO card, calling
  `openAsioControlPanel`; on other drivers and on the Mac and Linux it is shown greyed
  (R-AUD-22).
- After a reset the card shows "Restarted with the driver's new settings." for 5 s
  (R-AUD-21).
- An ASIO driver whose format is unsupported lists its pairs greyed with "<driver> uses
  a sample format NereusSDR can't play or record." (settled call 28).
- V-UI-1 captures (ASIO pairs, the prompt, details with a shared note, the greyed
  size), saved in `NEREUS_AUDIO_SETUP_CAPTURE_DIR`, compared with
  `asio-setup-mockup.html`.

**Verification:** UI plus controls that move several devices at once: functional tests
first, then captures. Unit:
`cmake --build build --target tst_device_card_pairs_asio tst_audio_driver_list tst_device_card tst_audio_setup_regroup && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_device_card_pairs_asio|tst_audio_driver_list|tst_device_card|tst_audio_setup_regroup)$' --no-tests=error --output-on-failure`.
Hardware (pending, JJ): V-HW-3.

**Execution note (advisory):** opus. After Tasks 15 and 16.

- [ ] **Step 1:** `tst_device_card_pairs_asio` with the cases above, on a fake engine
  with a fake ASIO status.
- [ ] **Step 2:** Pairs, the mic pick, the dialog, the details and the engine's ASIO
  status; captures; commit (R-AUD-07, R-AUD-19, R-AUD-20, R-AUD-21, R-AUD-22).

## Task 18: Digital modes, VAX lists and "Rescan now"

**Requirements:** R-AUD-03 (the VAX cable rows and the VAX first-run dialog), R-AUD-06
(the first-run dialog's "Rescan now", bug 3), R-AUD-10 ("<name> is not connected. VAX
<N> stays silent until it comes back; NereusSDR never sends it anywhere else."), R-AUD-11
(VAX in use), D12, D13. V-UI-1 (Digital modes).

**Files:**
- Modify: `src/gui/setup/AudioVaxPage.{h,cpp}` (cable rows from the catalogue, live;
  `VaxChannelCard` shows its role status; `onRescan` calls
  `AudioEngine::rescanOlderDrivers()`), `src/gui/setup/AudioDigitalModesPage.cpp`,
  `src/gui/VaxFirstRunDialog.{h,cpp}` (lists from the catalogue, live; `btnRescanNow`
  connected), `src/core/audio/VirtualCableDetector.{h,cpp}` (reads catalogue entries in
  place of PortAudio names), `tests/tst_vax_first_run_dialog.cpp`,
  `tests/tst_audio_vax_page_auto_detect.cpp`, `tests/tst_virtual_cable_detector.cpp`
  (where present; otherwise the detector's existing test)

**Interfaces:**
- Consumes: Task 7 engine API, Task 16 `AudioDriverList` and `audioRoleNote`, Task 17
  pairs.
- Produces: `QList<DetectedCable> VirtualCableDetector::detect(const QList<AudioDeviceInfo>& devices)`
  (pure, beside the existing PortAudio-name overload, which stays for older drivers);
  `VaxFirstRunDialog::setAudioEngine(AudioEngine*)`.

**Acceptance:**
- A cable added while the VAX page or the first-run dialog is open shows within 1 s,
  with no Rescan; removed, it goes (or stays as "<name> (not connected)" when chosen).
- "Rescan now" in the first-run dialog calls `rescanOlderDrivers()` and refreshes its
  list (bug 3: today it is connected to nothing, `VaxFirstRunDialog.cpp:647-653`).
- A VAX channel whose cable goes away shows R-AUD-10's sentence exactly, and its
  in-use sentence with "is in use by another program"; the role is `Silent` and nothing
  else opens; it resumes by itself.
- On Windows the cable rows list Windows audio devices (D12) and ASIO pairs under the
  one-driver rule (Task 17's prompt); on the Mac and Linux VAX keeps NereusSDR's own
  devices and the page shows them as today.
- `tst_vax_first_run_dialog`, `tst_audio_vax_page_auto_detect` and the detector's test
  stay green, extended with the live add and remove and the Rescan now wiring.
- V-UI-1 capture of Digital modes with a cable missing, saved in
  `NEREUS_AUDIO_SETUP_CAPTURE_DIR`.

**Verification:** UI plus a control that changes what a third-party program hears:
functional tests and a capture. Unit:
`cmake --build build --target tst_vax_first_run_dialog tst_audio_vax_page_auto_detect tst_audio_setup_regroup && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_vax_first_run_dialog|tst_audio_vax_page_auto_detect|tst_audio_setup_regroup)$' --no-tests=error --output-on-failure`
plus the detector's test target. Hardware (pending, JJ): V-HW-2's VB-CABLE case.

**Execution note (advisory):** opus. After Task 17.

- [ ] **Step 1:** Live list, Rescan now and the not connected cases as tests.
- [ ] **Step 2:** Page, dialog and detector; capture; commit (R-AUD-03, R-AUD-06,
  R-AUD-10).

## Task 19: The header's speaker menu and tooltip

**Requirements:** R-AUD-23 ("The PC icon's right-click menu (`MasterOutputWidget`)
follows D20 and `header-menu-mockup.html` option A. Left click still mutes. The tooltip:
"PC volume. Click to mute, right-click for speakers." plus the device playing now."),
R-AUD-03 (the header menu), R-AUD-08 and R-AUD-11 (header texts), R-AUD-12 (the
tooltip follows the default), D20. V-UI-2.

**Files:**
- Modify: `src/gui/widgets/MasterOutputWidget.{h,cpp}` (`onSpeakerContextMenu`,
  `selectOutputDevice`, the tooltip), `src/gui/MainWindow.cpp` (the widget gets the
  engine; "Sound setup…" opens Setup at Audio, Outputs, through the existing
  open-Setup-at-page call), `tests/tst_master_output_widget.cpp`,
  `tests/tst_master_output_widget_signal_refresh.cpp`

**Interfaces:**
- Consumes: Task 7 engine API, Task 16 `audioDeviceEntries`.
- Produces: `void MasterOutputWidget::setAudioEngine(AudioEngine*)`;
  `QMenu* MasterOutputWidget::buildSpeakerMenuForTest()`; signal
  `soundSetupRequested()`.

**Acceptance (option A, exact):**
- The menu's first row is the heading "Speakers" + " " + U+00B7 + " " + the speakers'
  driver label ("Speakers · Windows audio, shared"); only that driver's devices follow,
  with "(platform default)" first and pairs as indented entries under their interface's
  name; the current choice is ticked.
- A missing chosen device is the first item, ticked, in amber, "<name> (not
  connected)", with the line "Playing on <default> until it comes back." under it and
  a separator; in use reads "<name> (in use by another program)" with the same line.
- ASIO with no device present shows the disabled item "No ASIO devices present".
- The last item, after a separator, is "Sound setup…" (U+2026), which emits
  `soundSetupRequested` and opens Setup at Audio, Outputs.
- Picking a device saves the speakers' choice exactly as the Outputs card does
  (`Engine`, `DeviceId`, `DeviceName`, `FirstChannel`) and the open Outputs card
  follows.
- A device added while the menu is closed is in the next menu with no Rescan.
- Tooltip: "PC volume. Click to mute, right-click for speakers. Playing on <device>."
  and, with the chosen device missing, "PC volume. Click to mute, right-click for
  speakers. <name> is not connected; playing on <default> meanwhile." (in use: "is in
  use by another program"); it changes at once when the default changes on
  "(platform default)".
- Left click still mutes (`tst_master_output_widget` green).
- V-UI-2 captures of the menu (normal, missing device, ASIO pairs) and the tooltip,
  saved when `NEREUS_HEADER_CAPTURE_DIR` is set (the existing header capture variable),
  compared with `header-menu-mockup.html` option A.

**Verification:** UI plus a control that changes what plays: functional tests and
captures. Unit:
`cmake --build build --target tst_master_output_widget tst_master_output_widget_signal_refresh tst_radio_speaker_widget && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_master_output_widget|tst_master_output_widget_signal_refresh|tst_radio_speaker_widget)$' --no-tests=error --output-on-failure`.

**Execution note (advisory):** opus. After Task 16.

- [ ] **Step 1:** Menu and tooltip tests with the exact strings.
- [ ] **Step 2:** The menu, the tooltip, the Setup link; captures; commit (R-AUD-23).

## Task 20: The transmit badge with the mic missing

**Requirements:** R-AUD-24 (verbatim in the spec: badge "PC mic not connected" in amber,
its tooltip, cleared when the device returns, Retry kept for other failures, Tune,
two-tone and TCI key normally), R-AUD-13 (the badge's tooltip names the mic in use),
R-AUD-09 (the badge part), D21, settled call 35. V-SW-8, V-UI-3.

**Files:**
- Modify: `src/models/RadioModel.{h,cpp}` (`pcMicStatus()` from the engine's `TxInput`
  role status, a signal), `src/gui/applets/TxApplet.cpp` (`refreshMicSourceBadge`),
  `tests/tst_tx_applet_mic_source_badge.cpp`, `tests/tst_pc_mic_source.cpp`

**Interfaces:**
- Consumes: Task 7 `roleStatus`, `roleStatusChanged`; Task 13's mic reasons.
- Produces: `AudioRoleStatus RadioModel::pcMicStatus() const` and signal
  `pcMicStatusChanged()`.

**Acceptance (V-SW-8):**
- Mic source PC and the mic `Silent` with `NotConnected`: the badge reads "PC mic not
  connected" in amber (the area's existing warning colour), tooltip "<device> is not
  connected. Transmit audio is silent until it comes back; NereusSDR never switches to
  another mic by itself. Change the source in Settings > Audio > Microphone."
- With `InUse`: "PC mic in use by another program", tooltip "<device> is in use by
  another program. Transmit audio is silent until it comes back; NereusSDR never
  switches to another mic by itself. Change the source in Settings > Audio >
  Microphone." (settled call 35).
- Playing: the badge reads "PC mic" as today with the tooltip "PC mic: <device in
  use>" (R-AUD-13); a returning mic clears the amber state with no click.
- Other failures keep today's badge and Retry (`refreshMicSourceBadge`,
  `TxApplet.cpp:2022-2043`).
- R-R3-36 unchanged: voice-mode MOX with the mic missing is still refused with
  "Microphone is not ready. Check Audio settings and retry."; Tune, two-tone and TCI
  key; every existing keying test (`tst_pc_mic_source`, `tst_tx_mic_source`,
  `tst_radio_mic_source`) passes unchanged.
- Mic source radio: the badge never shows the PC mic states.
- V-UI-3 captures (normal, not connected, in use) saved when
  `NEREUS_AUDIO_SETUP_CAPTURE_DIR` is set, compared with `tx-mic-mockup.html`.

**Verification:** the transmit panel next to the keying gate: the keying tests first
and unchanged, then the badge. Unit:
`cmake --build build --target tst_tx_applet_mic_source_badge tst_pc_mic_source tst_tx_mic_source tst_radio_mic_source && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_tx_applet_mic_source_badge|tst_pc_mic_source|tst_tx_mic_source|tst_radio_mic_source)$' --no-tests=error --output-on-failure`.

**Execution note (advisory):** opus. After Task 16. Next to the keying rule; flagged
with the mic tasks. Nothing here changes what keys.

- [ ] **Step 1:** Badge cases and a check that the keying tests are untouched.
- [ ] **Step 2:** The model status and the badge; captures; commit (R-AUD-24).

## Task 21: The Core speaker on the Core and over the link

**Requirements:** R-AUD-25 (the Core's speaker uses ALSA direct with R-AUD-08's
fall-back, now chosen remotely), R-AUD-28 ("The Core speaker's level and mute change
only what the Core's sound card plays: never the audio sent to windows, the phone, TCI
or the radio."), R-AUD-30 (the Core side), D23, D31, design choices 8 and 9, settled
calls 11, 12, 15 and 29. Spec "The Core speaker" (the property table). V-SW-9.

**Files:**
- Create: `src/core/audio/CoreSpeakerJson.{h,cpp}`, `tests/tst_core_speaker_model.cpp`,
  `tests/tst_core_speaker_link.cpp`
- Modify: `src/models/RadioModel.{h,cpp}` (six `coreSpeaker*` properties, the Core
  binding, the remote role), `src/core/daemon/DaemonApp.cpp` (`setCoreSpeakerHost(true)`
  beside `setVaxOutputsAllowed(false)` at `:297`; the desktop rule), `src/core/audio/AudioStreamSupervisor.{h,cpp}`
  (nothing new: uses `setNoneMeansWaitingForPick`), `src/core/session/MirrorPolicy.cpp`,
  `src/core/session/StationServer.cpp`, `src/core/session/StationCapabilities.{h,cpp}`,
  `src/core/session/StationClient.cpp`, `src/core/session/IStationLink.h`,
  `docs/architecture/2026-09-23-station-link-v1.md`, `tests/data/link/v1/surface.json`
  (regenerated), `tests/tst_mirror_schema.cpp`, `tests/tst_mirror_inbound.cpp`,
  `tests/CMakeLists.txt`, `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 5 (`setNoneMeansWaitingForPick`), Task 7 (`roleStatus`, catalogue),
  Task 12 (`coreBoxStartsIntoDesktop`, ALSA direct).
- Produces (`CoreSpeakerJson.h`):
  ```cpp
  enum class CoreSpeakerStateKind { Playing, NotConnected, InUse, NoCard, WaitingForPick };
  struct CoreSpeakerState {
      CoreSpeakerStateKind kind = CoreSpeakerStateKind::NoCard;
      QString playingName;      // the card playing now; empty when silent
      QString chosenName;       // empty for "(the Core's default)"
      bool desktop = false;     // the box starts into a desktop (R-AUD-30)
  };
  struct CoreSpeakerCard { QString id; QString name; AudioDeviceState state; };
  struct CoreSpeakerDetails { int bufferFrames = 0; int delayMs = 0; int sampleRate = 48000; QString negotiated; double delayNowMs = -1.0; };
  QString coreSpeakerStateToJson(const CoreSpeakerState&);      // {"state":"playing|notConnected|inUse|noCard|waitingForPick","playing":"...","chosen":"...","desktop":false}
  std::optional<CoreSpeakerState> coreSpeakerStateFromJson(const QString&);
  QString coreSpeakerDevicesToJson(const QList<CoreSpeakerCard>&);   // [{"id":"Device,0","name":"USB Audio Device","state":"present|notConnected|inUse"}]
  std::optional<QList<CoreSpeakerCard>> coreSpeakerDevicesFromJson(const QString&);
  QString coreSpeakerDeviceToJson(const QString& id, const QString& name);   // {"id":"...","name":"..."}; {"id":"","name":""} for the default
  std::optional<QPair<QString, QString>> coreSpeakerDeviceFromJson(const QString&);
  QString coreSpeakerDetailsToJson(const CoreSpeakerDetails&);     // {"bufferFrames":128,"delayMs":0,"sampleRate":48000,"negotiated":"...","delayNowMs":12.5}
  std::optional<CoreSpeakerDetails> coreSpeakerDetailsFromJson(const QString&);
  CoreSpeakerState coreSpeakerStateFor(const AudioRoleStatus&, bool desktop);
  ```
  Each decoder takes the exact key set (as `CaptureProtocol`'s JSON does) and returns
  nullopt otherwise.
- Produces (`RadioModel.h`), mirrored on object `radio`:
  `Q_PROPERTY(int coreSpeakerVolume ...)` (0 to 100), `bool coreSpeakerMuted`,
  `QString coreSpeakerDevice`, `QString coreSpeakerDevices` (read-only),
  `QString coreSpeakerState` (read-only), `QString coreSpeakerDetails` (settable
  fields: buffer, delay, rate; the rest read-only), each with a `...Changed` signal;
  `void setCoreSpeakerHost(bool host)` (the Core binding: volume and mute are
  `AudioEngine::setVolume` / `setMasterMuted`, loaded from and saved to
  `audio/Master/Volume` (string `"0.720"` form, default `"0.500"`) and
  `audio/Master/Muted`; the device and details are the speakers role's
  `audio/Speakers/*` keys and `setChoice`); `bool coreSpeakerAvailable() const`,
  `bool coreSpeakerNeedsNewerCore() const`, `QString coreSpeakerUnavailableReason() const`
  ("This Core can't set its speaker from here. Update the Core." for an older Core;
  "Connect to the Core to change these." while unreachable).
- Produces (link): feature `coreSpeaker` 1 (declared by `StationClient` as
  `m_declaredFeatures.insert(QByteArrayLiteral("coreSpeaker"), 1)`); capability
  `coreSpeakerVersion` 1 appended after `radioSpeakerVersion` and before
  `coreBuildInfo` (which stays last) in `toUpdates` and `fromUpdates`;
  `IStationLink::coreSpeakerAvailable()` default false.

**Acceptance (V-SW-9):**
- JSON: each form round-trips; an extra or missing key decodes to nullopt; the state
  mapping from `AudioRoleStatus`: Playing gives `playing`; PlayingOnDefault with
  NotConnected gives `notConnected` with `playing` the default card; with InUse gives
  `inUse`; Silent with NoDevice gives `noCard`; Silent with NotConnected (no other card)
  gives `notConnected` with `playing` empty; WaitingForPick gives `waitingForPick`.
- On the Core (`setCoreSpeakerHost(true)`): with no saved master level the volume reads
  50 (settled call 29); setting 72 saves `audio/Master/Volume` "0.720" and the engine's
  volume is 0.72; mute saves "True"; a device set as `{"id":"Device,0","name":"USB Audio Device"}`
  saves `Engine` "AlsaDirect", `DeviceId` "Device,0", `DeviceName` "USB Audio Device"
  and the speakers role reopens on it; `{"id":"","name":""}` is "(the Core's default)".
- R-AUD-28: with the Core speaker at 0 and muted, the remote-window feed, the phone
  feed, TCI and the radio codec tap carry the same samples as at 100 and unmuted
  (the engine's master level is applied after those taps, `AudioEngine.cpp:2666`; the
  test pumps a block through `rxBlockReady` with fake taps on each and compares).
- The desktop rule (R-AUD-30, D31): with `coreBoxStartsIntoDesktop` true, no saved
  speakers `Engine` or `DeviceId`, and no `audio_device` in the config file (settled
  call 11), the Core calls `setNoneMeansWaitingForPick(Speakers, true)`, chooses
  "(none)" without saving it, opens no card, and reports `waitingForPick` with
  `desktop` true; picking a card from a window plays it and keeps `desktop` true; on a
  box without a desktop the default card opens at start.
- Link (loopback transport, two remote `RadioModel`s): setting volume, mute, device or
  details on one remote lands through the Core's real setter, is saved on the Core,
  and the other remote sees it; the read-only three are denied inbound; a peer that did
  not declare `coreSpeaker` 1 receives none of the six and no `coreSpeakerVersion`, and
  its capability list and radio object are byte-for-byte today's; a remote of an older
  Core: `coreSpeakerAvailable()` false, `coreSpeakerNeedsNewerCore()` true and its
  reason; setters send nothing.
- `everyMirroredPropertyHasAnExplicitPolicyEntry`, `everyReadOnlyPropertyIsDeniedInbound`
  and `policyEntriesAreUnique` pass; no existing ordinal or capability position moves;
  `tst_link_surface_manifest_regen` regenerates `surface.json`, which differs only by
  the added rows; the link document gets the feature text, the six property rows and
  the capability row, rendered by `scripts/render-link-tables.py`.

**Verification:** link change, networking, and what the Core opens: invariant tests
first. Unit and session:
`cmake --build build --target tst_core_speaker_model tst_core_speaker_link tst_mirror_schema tst_mirror_inbound tst_link_surface_manifest tst_link_surface_manifest_regen tst_radio_speaker_link && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_core_speaker_model|tst_core_speaker_link|tst_mirror_schema|tst_mirror_inbound|tst_link_surface_manifest|tst_link_surface_manifest_regen|tst_radio_speaker_link)$' --no-tests=error --output-on-failure`;
`python3 scripts/render-link-tables.py` leaves the document unchanged after the edit.
Linux lane: `tst_core_speaker_model`. Hardware (pending, JJ): V-HW-6, V-HW-7.

**Execution note (advisory):** opus. Networking and the station link; flagged. After
Task 12. Touches `RadioModel`, `StationServer` and the link document, so nothing else
touching those runs beside it.

- [ ] **Step 1:** JSON, Core binding, R-AUD-28 and link tests.
- [ ] **Step 2:** JSON forms, properties, the Core binding and the desktop rule;
  commit (R-AUD-25, R-AUD-28, R-AUD-30).
- [ ] **Step 3:** Policy, gates, capability, client plumbing, document and manifest;
  commit (R-AUD-27, R-AUD-28).

## Task 22: The Core speaker card

**Requirements:** R-AUD-27 (verbatim in the spec: the card, its texts and its five
states), R-AUD-30 (the window's texts: the list starting at "(none)" with "The Core's
computer runs a desktop, which uses its sound cards. Pick one here to play the Core
speaker on it." and, once picked, "The desktop can't play through <card> while the Core
has it."), D24, settled call 12. V-UI-4.

**Files:**
- Create: `src/gui/setup/CoreSpeakerCard.{h,cpp}`, `tests/tst_core_speaker_card.cpp`
- Modify: `src/gui/setup/AudioOutputsPage.{h,cpp}` (the card between Headphones and
  Radio speaker, only in a remote window), `tests/tst_audio_setup_regroup.cpp`,
  `tests/CMakeLists.txt`, `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 21's properties, JSON forms and availability; Task 16
  `audioDelayLine`.
- Produces: `class CoreSpeakerCard : public QGroupBox` with
  `CoreSpeakerCard(RadioModel* model, QWidget* parent)`; object names
  `coreSpeakerGroup`, `coreSpeakerVolume`, `coreSpeakerMute`, `coreSpeakerDevice`,
  `coreSpeakerNote`, `coreSpeakerStateNote`, `coreSpeakerDetails`.

**Acceptance (exact strings from R-AUD-27 and R-AUD-30):**
- In a remote window (`RadioModel::Role::Remote`) the card sits between
  `headphonesGroup` and `radioSpeakerGroup`, titled "Core speaker", with "A speaker or
  sound card plugged into <Core>." (the Core's name); Volume with its readout and
  "Mute Core speaker"; Device listing "(the Core's default)" then the Core's cards
  from `coreSpeakerDevices`; the note "This is the speaker at the Core, for listening
  where the Core sits. Changes here reach every window and the phone. Each slice's AF
  level and mute still apply."; Device details (folded) with the Driver line "ALSA,
  direct" greyed with "The Core runs without a desktop, so it plays straight to the
  sound card.", Sample rate, Channels, Buffer size, Delay (Task 16's list and "Now"
  line) and the negotiated format from `coreSpeakerDetails`.
- In a window that runs the radio itself the card is absent (the one exception the
  spec makes to "disabled, never hidden").
- Unreachable: every control greyed with "Connect to the Core to change these."
- Older Core: greyed with "This Core can't set its speaker from here. Update the Core."
- No card: shown, with "No sound card is plugged into the Core. Plug in a USB sound
  card or speaker and it shows up here by itself."
- Chosen card missing: "<name> is not connected at the Core. Playing on the Core's
  default, <card>, until it comes back."; with no other card: "<name> is not connected
  at the Core, and the Core has no other sound card, so it is silent until it comes
  back."; in use: the same with "is in use by another program at the Core" in place of
  "is not connected at the Core" (settled call 35).
- Desktop box (`desktop` true): the list starts at "(none)" selected with R-AUD-30's
  first sentence; after a pick, the second sentence with the card's name.
- A card plugged in at the Core appears in the open list when `coreSpeakerDevices`
  changes, without reopening Setup.
- Moving the slider writes `coreSpeakerVolume`, the mute writes `coreSpeakerMuted`, a
  device pick writes `coreSpeakerDevice`, and a Core-side change moves the controls
  without echo (`QSignalBlocker`, as `radioSpeakerGroup` does).
- V-UI-4 captures of each state above, saved in `NEREUS_AUDIO_SETUP_CAPTURE_DIR`,
  compared with `core-speaker-mockup.html`.

**Verification:** UI plus controls that change Core state: functional tests and
captures. Unit:
`cmake --build build --target tst_core_speaker_card tst_audio_setup_regroup && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_core_speaker_card|tst_audio_setup_regroup)$' --no-tests=error --output-on-failure`.
Hardware (pending, JJ): V-HW-6's card plugged in and out at a Pi Core, seen in a
window.

**Execution note (advisory):** opus. After Tasks 16 and 21.

- [ ] **Step 1:** Card tests for every state with the exact strings.
- [ ] **Step 2:** The card and its place on Outputs; captures; commit (R-AUD-27,
  R-AUD-30).

## Task 23: The phone's Core speaker section

**Requirements:** R-AUD-29 (verbatim in the spec: a "Core speaker" section under Radio
speaker, a cyan slider with SF Symbol `hifispeaker`, "Mute Core speaker", the amber
note with the chosen card missing, left out while the Core plays on no card and back
by itself, the described Setup pages unchanged), D25, D26, design choice 8. V-UI-5.

**Files:**
- Create: `ios/NereusApp/Audio/CoreSpeakerModel.swift`
- Modify: `ios/NereusApp/Audio/SoundPanel.swift` (a `SoundPanelCoreSpeaker` view after
  `SoundPanelRadioSpeaker`), `ios/NereusApp/App/AppModel.swift` (`lazy var coreSpeaker`
  beside `radioSpeaker` at `:85`), `ios/NereusKit/Sources/NereusLink/LinkFeatures.swift`
  (declare `"coreSpeaker": 1` and its doc paragraph), `ios/NereusKit/Tests/NereusLinkTests/LinkSurfaceOrdinalTests.swift`
  (the new capability and the six properties appended; existing ordinals unchanged),
  `ios/NereusApp/Tests/SoundPanelTests.swift`

**Interfaces:**
- Consumes: Task 21's properties on object `radio` and capability `coreSpeakerVersion`.
- Produces: `CoreSpeakerModel` (reads `coreSpeakerVolume`, `coreSpeakerMuted`,
  `coreSpeakerState`; writes the first two through `PropertyWriteQueue`, as
  `RadioSpeakerModel` does); accessibility ids `coreSpeakerSection`,
  `coreSpeakerSlider`, `coreSpeakerMute`, `coreSpeakerNote`.

**Acceptance (against `FakeStation`):**
- With `coreSpeakerVersion` 1 and the state `playing`: the section shows under Radio
  speaker, headed "Core speaker", with a cyan slider and `hifispeaker` symbol and
  "Mute Core speaker"; moving the slider writes `coreSpeakerVolume`; a Core-side change
  moves it; the toggle writes `coreSpeakerMuted`.
- State `notConnected` with a playing card: the amber note "<name> is not connected at
  the Core. Playing on the Core's default, <card>, until it comes back."
- States `noCard`, `waitingForPick`, and `notConnected` with nothing playing: the
  section is not in the panel; changing to `playing` brings it back with no action.
- Without the capability: the section is not shown (the phone has nothing to offer for
  an older Core; R-AUD-29 lists no greyed form for the phone).
- The setup description stays at version 25 and `SetupDescriptionLatestTests` passes
  unchanged (R-AUD-29: the described pages do not gain the card).
- "Mute this phone" and the Radio speaker section behave as today.
- V-UI-5: simulator screenshots of the panel playing, with the note, and with no card,
  compared with `phone-core-speaker-mockup.html` (option 1).

**Verification:** UI on the phone plus a control that changes Core state. NereusKit:
`ios/scripts/swift-test.sh --jobs 2 --filter LinkSurfaceOrdinalTests`; app: under the
simulator lock (Global Constraints), `ios/scripts/generate-project.sh`, then the
Global Constraints' `xcodebuild` command run once as `build-for-testing` and then as
`test-without-building -only-testing:NereusSDRTests/SoundPanelTests`. Hardware (pending, JJ): V-HW-6's level and mute from the
phone.

**Execution note (advisory):** opus. After Task 21. Holds the simulator lock; no other
Xcode build runs meanwhile.

- [ ] **Step 1:** Ordinal and Sound panel tests.
- [ ] **Step 2:** The model, the section, the feature; simulator captures; commit
  (R-AUD-29).

## Task 24: Sound access for the Core

**Requirements:** R-AUD-26 ("The Core's installers and station images add a drop-in
granting the `audio` group, next to the serial one (D19)."), D19, settled call 34.

**Files:**
- Create: `packaging/station-image/common/nereusd-audio.conf`
- Modify: `packaging/station-image/common/install-station.sh` (installs it as
  `/etc/systemd/system/nereusd.service.d/audio.conf` beside `serial.conf` at `:34-35`),
  `packaging/station-image/pi-gen/stage-nereus/00-install/00-run.sh` (copies it beside
  `nereusd-serial.conf` at `:17-18`), `scripts/pi4/install-core-pi4.sh` and
  `scripts/pi4/upgrade-core-pi4.sh` (write the same drop-in before `systemctl
  daemon-reload`; the upgrade adds it when absent and leaves an existing one as found),
  `packaging/nereusd.service.in` (the comment at `:135-158` says the images and
  installers add the audio group by drop-in, as the serial one; the unit itself still
  sets no group), `packaging/station-image/README.md` (the drop-in beside the serial
  one at `:65`), `scripts/pi4/README.md` (what the
  install adds, beside the unit)

**Interfaces:**
- Consumes: nothing.
- Produces: the drop-in, content exactly:
  ```
  # Sound cards on a Core: /dev/snd/* is root:audio on Debian, and a DynamicUser
  # account has no groups of its own.
  [Service]
  SupplementaryGroups=audio
  ```

**Acceptance:**
- A station image built from these scripts has
  `/etc/systemd/system/nereusd.service.d/audio.conf` with that content, beside
  `serial.conf`; `systemd-analyze verify` (already run by `install-core-pi4.sh:102`)
  passes with it.
- `install-core-pi4.sh` writes it on a first install; `upgrade-core-pi4.sh` adds it to
  a Core that lacks it and does not rewrite one that exists.
- The Armbian path gets it through `install-station.sh` (the overlay already carries
  `common/*`, `packaging/station-image/armbian/customize-image.sh:11-13`).
- `python3 scripts/pi4/test_pi4_retention.py` still passes; `bash -n` passes on every
  edited script; `shellcheck` passes where the repository already runs it on them.

**Verification:** packaging and device access: wiring, so integration evidence. Read
through and the script checks now; the image build itself and V-HW-6's "the drop-in
lets a fresh station image play out of the box" are pending JJ's bench (the Core
session installs).

**Execution note (advisory):** sonnet (packaging text, no code logic). Device access
on the Core; flagged. Any time after Task 12.

- [ ] **Step 1:** The drop-in, the four install paths, the comment and the README;
  script checks; commit (R-AUD-26).

## Task 25: Documentation

**Requirements:** R-AUD-01 to R-AUD-34 as the operator sees them; spec "What the
operator gets". The manual's own rules (`docs/manual/authoring.md`).

**Files:**
- Modify: `docs/manual/01-desktop-connect.md` (choosing speakers, headphones and the mic
  with the Driver list), `docs/manual/02-desktop-window.md` (the PC icon's speaker menu;
  the stale "Audio > Devices" link at `:220` becomes Audio > Outputs),
  `docs/manual/05-transmit.md` (the badge with the mic missing),
  `docs/manual/08-shared-core.md` (the Core speaker card),
  `docs/manual/07-iphone-operate.md` (the Sound panel's Core speaker section),
  `docs/manual/11-troubleshooting.md` (rows for "not connected", "in use by another
  program", Bluetooth call quality, the delay line), `docs/manual/12-reference.md`
  (Audio rows: Outputs, Microphone, Digital modes), `docs/manual/coverage.md` (rows
  C032, C033 and C048 point at the updated sections; a row for the Core speaker),
  `docs/architecture/overview.md` (the audio path: engines, the catalogue, the
  supervisor, the matcher, the mic hand-off), `README.md` (the Linux package list at
  `:271` gains `libpulse-dev` and `libasound2-dev`; the audio text at `:282-286` says
  what each Linux engine is for; ASIO on Windows comes from the vendored SDK), `docs/development/fast-test-loop.md` (one line:
  audio tests use the fakes in `tests/fakes/`, never a device)

**Interfaces:**
- Consumes: the strings of Tasks 16 to 23 exactly as built.
- Produces: nothing for later tasks.

**Acceptance:**
- Every label and message quoted in the manual matches the built string exactly (the
  implementer copies them from the code, not from the spec).
- `python3 docs/manual/check_manual.py` passes; `python3 docs/manual/test_build_preview.py`
  passes.
- Where a chapter already shows a screenshot of a screen this plan changed, it uses
  the matching capture from Tasks 16 to 23 at the manual's image size; no new
  screenshot is invented for a screen nobody captured.
- No cite, internal name or "yet" in operator text; no em-dash or en-dash characters.

**Verification:** documentation: the manual's checks and a read-through.

**Execution note (advisory):** sonnet. Last, after Tasks 16 to 24.

- [ ] **Step 1:** Manual chapters, overview, README and the test-loop line; checks
  green; commit (R-AUD-01, R-AUD-23, R-AUD-24, R-AUD-27, R-AUD-29).

## Final checks (once, on the finished branch)

- Desktop (Mac): `cmake --build build --target all_tests`, then
  `QT_QPA_PLATFORM=offscreen ctest --test-dir build -LE realtime --output-on-failure`,
  then `QT_QPA_PLATFORM=offscreen ctest --test-dir build -L realtime --output-on-failure`
  alone (`docs/development/fast-test-loop.md`); `cmake --build build` for the app,
  `nereusd` and `nereus-audio-capture`.
- Repository checks: `python3 scripts/verify-test-registration.py`,
  `python3 scripts/verify-thetis-headers.py`, `python3 scripts/verify-inline-tag-preservation.py`,
  `python3 scripts/verify-provenance-sync.py`, `python3 scripts/verify-inline-cites.py`,
  `python3 scripts/check-new-ports.py`, `python3 scripts/check-third-party-licenses.py`,
  and the pre-commit checks run by the hooks.
- Linux lane: the full `/work/build` build and
  `QT_QPA_PLATFORM=offscreen ctest --test-dir /work/build -LE realtime --output-on-failure`.
- iPhone: `ios/scripts/swift-test.sh --jobs 2` and the full scheme test under the lock.
- Windows: MinGW (`ci.yml`) and MSVC (`release.yml`) builds green after JJ approves the
  push; until then reported as pending.
- Docs: the `docs/development/project-status.md` row for the design moves to "Built,
  hardware pending"; `CHANGELOG.md` gets the user-facing lines (device lists that
  update by themselves, the Driver list, ASIO, interface pairs, the Delay line, not
  connected and in use, the header's speaker menu, the transmit badge, the Core
  speaker in windows and on the phone).
- Report code, UI and hardware separately, each passed, pending, failed or not
  applicable. Hardware pending for JJ: V-HW-1 to V-HW-9 (V-HW-5 stays "not tested on
  hardware" in the release notes), with the V-HW-8 before-and-after delay table on the
  Mac, the Windows PC (shared, exclusive, ASIO) and the PipeWire desktop.
