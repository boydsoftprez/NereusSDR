# Radio speaker control and Audio Setup redesign: implementation plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** the radio's speaker gets its own Core-owned RADIO level and mute beside the
computer's PC level, with the speaker amplifier switch on the boards that have it, our
own icons in place of every desktop emoji, and Setup > Audio regrouped by where the
sound goes, on the desktop and the iPhone.

**Architecture:** `AudioEngine` gains its own atomic RADIO gain and mute for the radio
codec tap, so the PC master no longer touches it. `RadioModel` owns three new settable
properties and two read-only reports, saved per radio and mirrored Bidirectional to
remote windows and the phone behind a new feature and capability. The P2 connection
works out the amplifier bit (high-priority byte 1400 bit 1) from the choice, RADIO
mute, transmit state and a new "CW or Tune" flag. The GUI adds a RADIO group to the
title bar, SVG icons through one renderer, and five regrouped Audio pages; the Setup
description goes to version 25 with an Outputs page the phone renders.

**Tech Stack:** C++20, Qt 6 (Core, Widgets, Svg, Test), CMake and Ninja; the station
link (`MirrorPolicy`, `StationServer`, `StationClient`, `StationCapabilities`, the
link document and its surface manifest); the Setup description service; Swift 6 with
SwiftUI and swift-testing for the iPhone app (`ios/`).

**Spec:** [2026-10-05-radio-speaker-and-audio-setup-design.md](2026-10-05-radio-speaker-and-audio-setup-design.md),
approved by JJ 2026-10-06 and amended the same day (pin icons, planning corrections).
Every decision is settled there (D1 to D17, R-SPK-01 to R-SPK-24, V-SW-1 to V-SW-8,
V-UI-1 to V-UI-5, V-HW-1 to V-HW-6); this is how it gets built. Mockups are in the
spec's sibling folder `2026-10-05-radio-speaker-and-audio-setup-design/`.

## Global Constraints

- **Repository rules.** `CLAUDE.md` and `CONTRIBUTING.md` bind every task. No `goto`,
  no raw `new`/`delete` (unique_ptr or Qt parent), `constexpr` not `#define`, braces on
  all control flow, `PascalCase` classes, `camelCase` methods, `kPascalCase` constants,
  `m_camelCase` members, `Q_OS_WIN` / `Q_OS_MAC` / `Q_OS_LINUX` guards, errors through
  `qCWarning(lcCategory)`, no exceptions. Don't remove code you didn't add except where
  a task says to replace it.
- **Settings.** `AppSettings`, never `QSettings`. PascalCase keys; booleans are the
  strings `"True"` / `"False"`. New keys: `hardware/<mac>/RadioSpeaker/Volume` (int
  0 to 100), `hardware/<mac>/RadioSpeaker/Muted`, `hardware/<mac>/RadioSpeaker/AmplifierMode`
  (int 0 Normal, 1 Off while transmitting, 2 Always off). Existing keys
  (`audio/Master/Volume` as `"0.720"`, `audio/Master/Muted`) and every existing Setup
  key and `nereusSetupId` keep their names (R-SPK-22).
- **Rule R1.** Nothing under `src/core/` or `src/models/` includes a GUI header;
  `tst_core_has_no_gui_includes` must stay green (V-SW-8). Icons and `Qt6::Svg` are GUI
  only.
- **Threads.** The GUI writes `RadioModel` on the main thread. Cross-thread traffic is
  auto-queued signals or `QMetaObject::invokeMethod`. Audio-thread values are
  `std::atomic`, loaded once per block; never take a lock in the audio callback.
- **Safety boundary.** Connecting never keys. Nothing in this plan keys a transmitter
  or changes what keys it. Byte 1400 bits 0 (transverter out) and 2 (ATU tune) stay
  zero exactly as today. On every board outside the amplifier list the byte stays zero.
- **Ports and cites.** Read `docs/attribution/HOW-TO-PORT.md` before touching ported
  files. Cites as `// From Thetis file:line [v2.10.3.15]`; piHPSDR as
  `// From piHPSDR src/file:line [@4aa95c5]`. The amplifier rule (R-SPK-09) is written
  from the spec, citing piHPSDR as a fact, not translated from its code; if code is
  translated, the port rules apply in full (header byte-for-byte, a new
  `PIHPSDR-PROVENANCE.md`). Inline comments in ported logic are kept verbatim
  (`scripts/verify-inline-tag-preservation.py`). No cites in user-facing strings.
- **Wire compatibility.** New mirrored state goes only to a peer that declared feature
  `radioSpeaker` 1; the Core advertises capability `radioSpeakerVersion` 1. Never change
  `kSessionProtocolMinor`. Existing property ordinals and capability order don't move;
  older clients and older Cores keep today's behaviour and goldens stay byte-for-byte
  except the surface manifest entries this plan adds.
- **Operator wording.** Plain user words, no internal names. Strings fixed by the spec
  are used exactly: "No radio connected", "This Core can't set the radio speaker.
  Update the Core.", "This radio has no switchable speaker amplifier.", "Not yet tested
  on the ANAN-G2E.", "Radio speaker at the Core (shared with every window and the
  phone)", "Amplifier is off now: radio speaker muted." / "Amplifier is off now:
  transmitting." / "Amplifier is off now.", "Mute this phone", "Mute radio speaker".
  New strings pass `OperatorWording::isPlain` where the area already uses it.
- **Commits.** GPG-signed with hooks, never `--no-gpg-sign` or `--no-verify`. No
  `Co-Authored-By` trailer. No em-dash characters in commit messages, code comments or
  docs. Stage explicit paths only. Every commit names its R-SPK IDs.
- **Tests.** Read `docs/development/fast-test-loop.md`. Build exact targets, then run
  them offscreen:
  `cmake --build build --target <tests> && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(<tests>)$' --no-tests=error --output-on-failure`.
  Never run the unfiltered suite inside a task; never `ctest` without building the
  targets first. New tests register with `nereus_add_test(...)` in `tests/CMakeLists.txt`
  (add `target_compile_definitions(<name> PRIVATE NEREUS_BUILD_TESTS)` when it uses
  `ForTest` hooks); `scripts/verify-test-registration.py` must pass.
- **iPhone.** NereusKit tests: `ios/scripts/swift-test.sh --jobs 2 --filter <Suite>`.
  App tests: reserve `/tmp/nereus-ios-full.lock` with `mkdir` first (record the token,
  release only your own in an exit trap), one simulator, jobs 2, then
  `ios/scripts/generate-project.sh` and
  `xcodebuild -project ios/NereusSDR.xcodeproj -scheme NereusSDR -destination 'platform=iOS Simulator,name=iPhone 17' -collect-test-diagnostics never -parallel-testing-enabled NO -jobs 2 test`
  (see `ios/README.md`).
- **Hardware.** No subagent touches a radio. Every V-HW case stays pending for JJ's
  bench.

## What already exists

- **Radio feed:** `AudioEngine::rxBlockReady` loads `m_masterVolume` (`AudioEngine.h:1433`,
  default `0.5f`) at `AudioEngine.cpp:2661` and, at `:2696-2704`, zero-fills or scales the
  radio sum before `invokeMixTap(m_radioOutputTap, ...)`. Setters `setVolume(float)`
  (`.cpp:2922`, clamps 0..1, emits `volumeChanged`) and `setMasterMuted(bool)` (`.cpp:2934`,
  flushes the speaker bus, emits `masterMutedChanged`). The remote-playback path at
  `:1349` / `:1361` also uses the master and is PC, correctly.
- **Radio tap wiring:** `RadioModel::connectRadioSpeakerOutput()` / `disconnectRadioSpeakerOutput()`
  (`RadioModel.cpp:20314` / `20328`), called from `wireConnectionSignals(int)` (`:19891`) and
  on disconnect (`:24703`); `carriesRadioAudio()` is `true` on P1 and P2 connections
  (`P1RadioConnection.h:160`, `P2RadioConnection.h:306`). Tests pinning today's coupling:
  `tests/tst_radio_codec_speaker_out.cpp:349` (`engine_radioOutputTap_followsVolumeAndMute`)
  and `:395`.
- **Who sets the master:** only `MasterOutputWidget` (`src/gui/widgets/MasterOutputWidget.cpp:86-188`,
  seeds from `audio/Master/Volume`) and `RadioModel::setAfLinear(int)` (`RadioModel.cpp:27626`,
  TCI AF). `nereusd` sets neither.
- **Per-MAC settings:** `RadioModel::currentRadioMac()` (`RadioModel.h:3683`),
  `AppSettings::hardwareValue(mac, key, default)` / `setHardwareValue` (`AppSettings.h:684-685`);
  connect-time load pattern `applyAlexHpfSwitchSettings()` (`RadioModel.cpp:29689`).
- **Board facts:** `HardwareProfile` (`src/core/HardwareProfile.h:79`, set per model in
  `profileForModel`, `HardwareProfile.cpp`, already carries the `clsHardwareSpecific.cs`
  header); `m_hardwareProfile.model`, `m_lastRadioInfo.protocol` (`ProtocolVersion::Protocol2`,
  `RadioDiscovery.h:79`). `HPSDRModel` (`src/core/HpsdrModel.h:161`): `ANAN7000D` 9,
  `ANAN8000D` 10, `ANAN_G2` 11, `ANAN_G2_1K` 12, `ANVELINAPRO3` 13, `REDPITAYA` 15,
  `ANAN_G2E` 16. HL2 add-on: `BoardCapabilities::radioMicNeedsAddOn` (`BoardCapabilities.h:521`).
- **P2 high priority:** `P2CodecOrionMkII::composeCmdHighPriority(const CodecContext&, quint8 buf[1444])`
  (comment at `:283-289` marks byte 1400 "not ported here"); Saturn, Hermes, HermesII and
  the G2E (`HermesC10`) codecs inherit it. `CodecContext` (`src/core/codec/CodecContext.h:50`)
  is filled in `P2RadioConnection::buildCodecContext() const` (`P2RadioConnection.cpp:4134`).
  Queued setter pattern: virtual on `RadioConnection` storing the value
  (`setHpfBypassOnTx`, `RadioConnection.h:540`), P2 override that calls the base and
  `if (m_running) sendCmdHighPriority();` (`P2RadioConnection.cpp:1797`), called from
  `RadioModel` with `QMetaObject::invokeMethod(conn, [conn, ...]{ ... })` (`RadioModel.cpp:29741`).
  MOX sends the packet at once (`:1611`) and every 100 ms while keyed (`:597`). Byte tests:
  `tests/tst_p2_band_outputs_and_rx_lpf.cpp` (`composeCmdHighPriorityForTest`, `setBoardForTest`),
  `tests/tst_p2_codec_orionmkii.cpp`, `tst_hardware_profile`.
- **Tune and CW:** `TransmitModel::isTune()` / `tuneChanged` (`TransmitModel.h:394`);
  `MoxController::onModeChanged(DSPMode)` (wired at `RadioModel.cpp:14846`); Tune swaps CW
  to LSB/USB while it runs (`MoxController.h:700`).
- **Mirror:** `src/core/session/MirrorPolicy.{h,cpp}` (RadioModel entries `:1108-1179`,
  `featureGates()` `:1244`, entries like `{"RadioModel","paTransmitBand","paTransmitBand",1}`);
  Core inbound `StateMirror.cpp:740-778`; `kPeerOnlyProperties` (`StationServer.cpp:1477-1510`);
  capabilities (`StationCapabilities.h:~421`, `toUpdates` / `fromUpdates` in the `.cpp`,
  set per peer at `StationServer.cpp:~13172`); client declares features
  (`StationClient.cpp:875-880`), applies (`applyOne`, `:4752`) and sends outbound
  (`watchForOutbound`, `:3584`). Remote availability pattern: `IStationLink.h:684-698`
  (`xxxAvailable()` plus a static reason), e.g. `StationClient::radioHardwareAvailable`
  (`:5791`). `RadioModel` has no writable mirrored property yet; the last mirrored one is
  `diversityState` (`RadioModel.h:943`). Link document
  `docs/architecture/2026-09-23-station-link-v1.md` (section 6.3 and the ordinal table),
  manifest `tests/data/link/v1/surface.json` regenerated by `tst_link_surface_manifest_regen`.
  Templates: `tests/tst_shared_input_filters_link.cpp`, `tests/tst_rade_reason_link.cpp`,
  `tests/tst_mirror_schema.cpp`, `tests/tst_mirror_inbound.cpp`.
- **Header:** `MasterOutputWidget(AudioEngine*, QWidget*)` with children `speakerBtn`,
  `masterSlider`, `dbLabel`, styles `kSliderStyle` (`:50`), `kDbLabelStyle` (`:56`), emoji
  `kSpeakerOn` / `kSpeakerOff` (`:68-69`); `TitleBar(AudioEngine*, QWidget*)`, strip height 32,
  order "... UTC, gap 18, master, spacing, featureButton"; bulb painted by the `makeBulbIcon`
  lambda (`TitleBar.cpp:740-777`); built from `MainWindow.cpp:1347`. Tests:
  `tst_master_output_widget` (asserts the muted emoji text at `:170`),
  `tst_master_output_widget_signal_refresh`, `tst_title_bar`, `tst_audio_engine_master_mute`.
- **Other emoji sites:** `RxApplet.cpp:672`, `682-683`, `2009-2010`, `2125-2126` (padlock,
  `m_lockBtn`); `VfoWidget.cpp:3216`, `3282` (flag lock); `VfoWidget.cpp:1250` (speaker tab,
  `buildTabBar()` `:1240-1314`, `m_tabButtons[0]`, no objectName; found by its emoji in
  `tst_multi_device_screens.cpp:1435`); `ContainerWidget.cpp:182`, `499` (pin, `setPinOnTop`).
  SVG pattern: `src/gui/widgets/StatusBadge.cpp:140-175` (renders into a `QImage` sized by
  `devicePixelRatioF()`; it tints, the new full-colour icons must not). GUI resources:
  `resources.qrc` (`/icons` prefix aliases `resources/icons/*.svg`), added at
  `CMakeLists.txt:1979`; `Qt6::Svg` linked in `nereus_apply_gui_deps`.
- **Slice mute on the flag:** `SliceModel::mutedChanged(bool)`, bound by
  `SliceFlagPresentationBinding.cpp:50` to `VfoWidget::setMuted` (`:2875`, stores
  `m_modelMuted`; returns early on a listened flag); `applyAudioBinding()` (`:4170`) picks
  `m_listenMuted` or `m_modelMuted`.
- **Setup:** `SetupPage` adds its trailing stretch at `SetupPage.cpp:113`; `addSection()`
  (`:203-219`) inserts before it. Audio pages are registered in `SetupDialog.cpp:1596-1670`,
  each wrapped by `wrapWithAudioBackendStrip` (`:1332-1345`) except TX Profile. Pages:
  `AudioDevicesPage.cpp`, `AudioTxInputPage.cpp` (`buildPcMicGroup` `:554`, radio-mic groups
  `:1026/1110/1164`), `AudioVaxPage.cpp` (`VaxChannelCard`, hidden `m_deviceCard` `:226-228`,
  status texts `:577-702`, Windows gate `:703-712`), `AudioTciPage.cpp`, `AudioAdvancedPage.cpp`,
  `TxProfileSetupPage.cpp`; `DeviceCard.{h,cpp}` (WASAPI boxes `m_exclusiveChk`,
  `m_eventDrivenChk`, `m_bypassMixerChk`, `m_driverApiCombo` from `PortAudioBus::hostApis()`);
  `AudioBackendStrip`; `AudioEngine::linuxBackend()` / `rescanLinuxBackend()` /
  `linuxBackendChanged` under `Q_OS_LINUX`; `LinuxAudioBackend {PipeWire, Pactl, None}`
  (`src/core/audio/LinuxAudioBackend.h:26`). Label navigation: `MainWindow.cpp:11412`
  ("VAX"), `:10320`, `:10324` ("TX Profile"), `VaxFirstRunDialog.cpp:1000`, `:1086`,
  `PhoneCwApplet.cpp:1057-1058` ("TX Input"). Gating tests that name page labels:
  `tst_remote_gui_gating`, `tst_settings_scope:923`, `tst_receive_only:803,858,892`,
  `tst_vax_first_run_dialog:196`, `tst_audio_vax_page_auto_detect`.
- **Setup description:** `resources/setup/audio.json` version 24 (`audio.txInput`,
  `audio.txProfile`); validator `src/core/setup/SetupDescriptionService.cpp` (closed rows
  `kAudioV24Controls` `:226`, `audioV24Controls()` `:483`, `validateAudioV24Control`
  `:2317`, `validateAudioPropertyBinding` `:2365` accepts only `transmit` today; version
  acceptance `:762`, `:814`, `:823`; downgrade block `:2957`; ceiling `:3079`); `"radio"`
  maps to `RadioModel` in `SetupDescriptionV15.cpp:75`. Tests: `tst_setup_description_service`,
  `tst_setup_description_parity` (asserts two audio pages at `:1666`), `tst_setup_description_live`.
- **iPhone:** `ios/NereusApp/Audio/SoundPanel.swift` ("Mute" toggle id `soundMute`
  `:81-89`, then divider, then "Play the band through"); mirror reads like
  `TransmitModel.swift:1653`, writes through `PropertyWriteQueue` (`:170`, `:1509`),
  capability check `mirror.capabilityVersion("...Version") >= n` (`:1451`);
  `NereusKit/Sources/NereusLink/LinkFeatures.swift:173`; tests
  `NereusApp/Tests/SoundPanelTests.swift`,
  `NereusKit/Tests/NereusLinkTests/LinkSurfaceOrdinalTests.swift`,
  `NereusKit/Tests/NereusMirrorTests/SetupDescriptionLatestTests.swift` (expects 24).

## File Structure

**Create**

| Path | Responsibility |
|---|---|
| `resources/icons/emoji/*.svg` (`pc-on`, `pc-muted`, `radio-on`, `radio-muted`, `radio-none`, `lock`, `unlock`, `bulb`, `pin`, `pinned`) | The approved icons, copied from the spec folder's `icons/`. |
| `src/gui/widgets/AppIcon.{h,cpp}` | `NereusSDR::AppIcon`: render a named SVG to a `QIcon` / `QPixmap` at a logical size and the widget's device pixel ratio, untinted. |
| `src/gui/widgets/RadioSpeakerWidget.{h,cpp}` | The header's RADIO group. |
| `src/gui/setup/AudioOutputsPage.{h,cpp}` | The Outputs page. |
| `src/gui/setup/SoundSystemLine.{h,cpp}` | The "Sound system" status line (replaces `AudioBackendStrip` on Audio pages). |
| `src/gui/setup/AudioDigitalModesPage.{h,cpp}` | Digital modes: VAX plus TCI. |
| `tests/tst_radio_speaker_model.cpp`, `tests/tst_p2_speaker_amplifier.cpp`, `tests/tst_radio_speaker_link.cpp`, `tests/tst_app_icon.cpp`, `tests/tst_radio_speaker_widget.cpp`, `tests/tst_setup_pages_start_at_top.cpp`, `tests/tst_audio_setup_regroup.cpp` | Task tests (each task names its own). |

**Modify** (main changes; each task lists its own exactly)

| Path | Change |
|---|---|
| `src/core/AudioEngine.{h,cpp}` | RADIO atomics and setters; radio tap reads them. |
| `src/core/HardwareProfile.{h,cpp}` | `hasAudioAmplifier` per model. |
| `src/core/RadioConnection.h`, `src/core/P2RadioConnection.{h,cpp}`, `src/core/codec/CodecContext.h`, `src/core/codec/P2CodecOrionMkII.cpp` | Amplifier inputs and byte 1400 bit 1. |
| `src/models/RadioModel.{h,cpp}` | Five properties, persistence, availability, wiring. |
| `src/core/session/*` (MirrorPolicy, StationServer, StationClient, StationCapabilities, IStationLink) | Mirroring, feature and capability. |
| `src/gui/widgets/MasterOutputWidget.cpp`, `src/gui/TitleBar.{h,cpp}`, `src/gui/MainWindow.cpp` | PC icons, RADIO group, bulb. |
| `src/gui/widgets/VfoWidget.cpp`, `src/gui/applets/RxApplet.cpp`, `src/gui/containers/ContainerWidget.cpp` | Icons, tab follows mute. |
| `src/gui/SetupPage.{h,cpp}` and every page listed in Task 8 | Content starts at the top. |
| `src/gui/SetupDialog.cpp`, `src/gui/setup/Audio*Page.cpp`, `src/gui/setup/DeviceCard.cpp` | Regroup. |
| `resources/setup/audio.json`, `src/core/setup/SetupDescriptionService.cpp` | Version 25 and `audio.outputs`. |
| `ios/NereusApp/Audio/SoundPanel.swift`, `ios/NereusKit/Sources/NereusLink/LinkFeatures.swift` | Phone. |

## Before Task 1

The controller flags these for JJ as candidates for an earlier independent review
(`cost-aware-execution`): **Task 1** and **Task 3** touch the core RX audio path
(I/Q to WDSP to audio, CLAUDE.md asks it to be flagged); **Task 2** writes a byte the
radio acts on during transmit; **Task 4** changes what crosses the station link.
**JJ's decision (2026-10-06):** one independent review of Tasks 1 to 4 as a group,
run as soon as Task 4 is committed and before Task 5's dependants (6, 9 onward) start;
its findings get one fix wave and one scoped re-review, as at the finish. The
whole-branch review at the end still runs.

Order: 1, 2, 3, 4, then 5 to 11 (GUI; 5 before 6, 7; 8 before 9 to 11), then 12, then
13. Tasks 5, 7 and 8 share no files with 1 to 4 and may run in a worktree in parallel
with them. Tasks 9 to 11 all touch `SetupDialog.cpp`, so they run one at a time.

## Task 1: RADIO level and mute in the audio engine

**Requirements:** R-SPK-01, R-SPK-02, R-SPK-03, R-SPK-04, R-SPK-15 (audio half).

**Files:**
- Modify: `src/core/AudioEngine.h` (members `std::atomic<float> m_radioSpeakerVolume{0.5f}`,
  `std::atomic<bool> m_radioSpeakerMuted{false}`; methods and signals below),
  `src/core/AudioEngine.cpp` (setters; the radio block at `:2696-2704` loads the RADIO
  values instead of `vol` and `m_masterMuted`; the speaker mix keeps `vol`)
- Test: `tests/tst_radio_codec_speaker_out.cpp` (rewrite the two master-coupled cases)

**Interfaces:**
- Consumes: nothing.
- Produces: `void AudioEngine::setRadioSpeakerVolume(float linear)` (clamps 0..1, emits
  on change), `float AudioEngine::radioSpeakerVolume() const`,
  `void AudioEngine::setRadioSpeakerMuted(bool)` (no flush), `bool AudioEngine::radioSpeakerMuted() const`,
  signals `radioSpeakerVolumeChanged(float)` and `radioSpeakerMutedChanged(bool)`. Linear
  gain is percent / 100, the same mapping as PC (R-SPK-03).

**Acceptance:**
- PC 0.30, RADIO 0.80, a slice at unity: the speakers bus receives the mix at 0.30 and
  the radio tap at 0.80 (sample values within float tolerance of the reference mix
  times each gain).
- PC muted, RADIO unmuted: speakers silent, radio tap at the RADIO gain. RADIO muted, PC
  unmuted: radio tap receives zero-filled blocks of the normal length at the normal
  cadence (one per mixed block, never skipped); speakers at the PC gain.
- A slice's own AF level and mute change both outputs the same way (R-SPK-04).
- Changing PC never changes `radioSpeakerVolume()` or `radioSpeakerMuted()`, and the
  reverse.
- Remote-playback path (`:1349`, `:1361`) unchanged: still the PC level and mute.
- `engine_radioOutputTap_followsVolumeAndMute` is replaced by cases that assert the
  independence above; `engine_radioOutputTap_takesRemoteOwnedSlices` keeps its slice
  assertions with the RADIO gain.

**Verification:** ordinary feature on the core RX path: test the new contract first.
Unit (offscreen):
`cmake --build build --target tst_radio_codec_speaker_out tst_audio_engine_master_mute && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_radio_codec_speaker_out|tst_audio_engine_master_mute)$' --no-tests=error --output-on-failure`.
Hardware V-HW-1, V-HW-3, V-HW-4 (by ear) stay pending.

**Execution note (advisory):** opus. Core RX path: flagged above. No prerequisites;
could run in a worktree beside Tasks 5, 7, 8.

- [ ] **Step 1:** Rewrite the two tap tests to the acceptance cases above and see them
  fail against today's coupling.
- [ ] **Step 2:** Add the RADIO atomics, setters and signals; switch the radio block to
  them (each loaded once per block, `memory_order_acquire`, as `vol` is); commit
  (R-SPK-01 to R-SPK-04).

## Task 2: The speaker amplifier bit on Protocol 2

**Requirements:** R-SPK-08, R-SPK-09, R-SPK-15 (connection half), D8, D17.

**Files:**
- Create: `src/core/SpeakerAmplifier.h` (the rule, citing piHPSDR
  `src/new_protocol.c:868-882 [@4aa95c5]` as the source of the CW and Tune exception)
- Modify: `src/core/HardwareProfile.h` (`bool hasAudioAmplifier = false;` beside
  `mkiiBpf`), `src/core/HardwareProfile.cpp` (true for `ANAN7000D`, `ANAN8000D`,
  `ANVELINAPRO3`, `ANAN_G2`, `ANAN_G2_1K`, `REDPITAYA`; `// From Thetis
  clsHardwareSpecific.cs:459-467 [v2.10.3.15]`; `ANAN_G2E` stays false with a comment
  naming D17 and V-HW-6), `src/core/RadioConnection.h` (virtual queued setters storing
  the values: `setSpeakerAmplifierMode(int)`, `setRadioSpeakerMuted(bool)`,
  `setSidetoneExpected(bool)`), `src/core/P2RadioConnection.{h,cpp}` (overrides: compare,
  call the base, `if (m_running) sendCmdHighPriority();`; `buildCodecContext` sets
  `ctx.p2SpeakerAmpOff`), `src/core/codec/CodecContext.h` (`bool p2SpeakerAmpOff{false};`),
  `src/core/codec/P2CodecOrionMkII.cpp` (`buf[1400] = ctx.p2SpeakerAmpOff ? 0x02 : 0x00;`
  replacing the "not ported here" note, with the Thetis line kept as the cite:
  `// From Thetis ChannelMaster/network.c:1028 [v2.10.3.15]` and
  `// From piHPSDR src/alex.h:129 [@4aa95c5]` for the bit name)
- Test: `tests/tst_p2_speaker_amplifier.cpp` (new), `tests/tst_hardware_profile.cpp`
  (amplifier column)

**Interfaces:**
- Consumes: nothing.
- Produces: `HardwareProfile::hasAudioAmplifier`; `RadioConnection::setSpeakerAmplifierMode(int mode)`
  (0 Normal, 1 Off while transmitting, 2 Always off), `RadioConnection::setRadioSpeakerMuted(bool)`,
  `RadioConnection::setSidetoneExpected(bool)` (true while CW or Tune), all safe to call
  before the connection runs; the rule as an inline function in a new header
  `src/core/SpeakerAmplifier.h`:
  `namespace NereusSDR { constexpr bool speakerAmplifierOff(bool hasAmp, int mode, bool muted, bool transmitting, bool sidetoneExpected); }`,
  used by the connection here and by `RadioModel` in Task 3.

**Acceptance (V-SW-3):**
- The rule: `hasAmp && (mode == 2 || muted || (mode == 1 && transmitting && !sidetoneExpected))`.
  Table-driven test over all 2 x 3 x 2 x 2 x 2 inputs.
- G2 on P2: byte 1400 is `0x02` for Always off; for muted (any mode, including CW and
  Tune while transmitting); for Off while transmitting while transmitting with
  `sidetoneExpected` false; it is `0x00` for Off while transmitting while transmitting
  with `sidetoneExpected` true, while receiving, and for Normal unmuted.
- Every other model (including `ANAN_G2E`, the HL2, every P1 board) and the G2 with any
  inputs on P1: byte 1400 is `0x00`.
- Bits 0 and 2 are zero in every case.
- A change of any input while running sends one high-priority packet; MOX keying with
  Off while transmitting carries the bit in the packet MOX already sends (no extra
  packet, no gap).
- `tst_hardware_profile`: `hasAudioAmplifier` true for exactly the six models.

**Verification:** a byte the radio acts on during transmit: invariant table first, then
implementation. Unit:
`cmake --build build --target tst_p2_speaker_amplifier tst_hardware_profile tst_p2_band_outputs_and_rx_lpf tst_p2_codec_orionmkii && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_p2_speaker_amplifier|tst_hardware_profile|tst_p2_band_outputs_and_rx_lpf|tst_p2_codec_orionmkii)$' --no-tests=error --output-on-failure`.
V-HW-1, V-HW-2 and V-HW-6 stay pending.

**Execution note (advisory):** opus. Radio protocol during transmit: flagged above.
No prerequisites.

- [ ] **Step 1:** The rule's table test and the byte cases; see them fail.
- [ ] **Step 2:** Profile flag, context field, setters, codec byte; commit (R-SPK-08,
  R-SPK-09, R-SPK-15).

## Task 3: Radio speaker state on RadioModel

**Requirements:** R-SPK-05, R-SPK-06 (local half), R-SPK-07, R-SPK-11, R-SPK-12, R-SPK-15,
D4, D11, D17.

**Files:**
- Modify: `src/models/RadioModel.h` (five `Q_PROPERTY`s declared after `diversityState`:
  `int radioSpeakerVolume READ WRITE setRadioSpeakerVolume NOTIFY radioSpeakerVolumeChanged`,
  `bool radioSpeakerMuted ...`, `int speakerAmplifierMode ...`, read-only
  `int radioSpeakerAvailability ...`, `bool speakerAmplifierAvailable ...`; plus
  `QString radioSpeakerUnavailableReason() const` and `QString speakerAmplifierUnavailableReason() const`),
  `src/models/RadioModel.cpp` (setters clamp, save, forward; load on connect; wiring),
  `tests/CMakeLists.txt`
- Test: `tests/tst_radio_speaker_model.cpp` (new)

**Interfaces:**
- Consumes: Task 1's `AudioEngine` setters; Task 2's `RadioConnection` setters and
  `HardwareProfile::hasAudioAmplifier`.
- Produces: the five properties with their signals (`radioSpeakerVolumeChanged(int)`,
  `radioSpeakerMutedChanged(bool)`, `speakerAmplifierModeChanged(int)`,
  `radioSpeakerAvailabilityChanged(int)`, `speakerAmplifierAvailableChanged(bool)`);
  availability values 0 no radio, 1 available, 2 available but needs an add-on board;
  the two reason getters returning the spec's strings ("No radio connected"; for the
  amplifier "This radio has no switchable speaker amplifier." or, on `ANAN_G2E`, "Not
  yet tested on the ANAN-G2E."; empty when available). Task 4 extends the reasons for
  remote windows. Also `QString speakerAmplifierStatus() const` with signal
  `speakerAmplifierStatusChanged()`: empty while the amplifier is on, otherwise
  "Amplifier is off now: radio speaker muted." (muted), "Amplifier is off now:
  transmitting." (Off while transmitting, keyed, not CW or Tune) or "Amplifier is off
  now." (Always off), computed with `speakerAmplifierOff` from the model's own mode,
  mute, MOX and CW-or-Tune state (in a remote window from the mirrored values); empty
  when the amplifier is unavailable.

**Acceptance:**
- V-SW-2: station with `audio/Master/Volume` `"0.720"` and no RadioSpeaker keys for the
  radio: on connect RADIO is 72, unmuted, mode 0, and the three keys are not written
  until a value changes. Precisely, the seed is `AudioEngine::volume()` at connect
  (R-SPK-05 as amended): on a headless Core with nothing set it is 50. Saved keys
  (55, `"True"`, 1) load as 55, muted, mode 1.
- Two radios with different MACs keep separate values; switching radios loads each
  one's own.
- Setters clamp (volume 0..100, mode 0..2), emit only on change, write the per-MAC keys,
  and forward: volume and mute to `AudioEngine` (volume / 100) and mute and mode to the
  connection through `QMetaObject::invokeMethod`.
- "CW or Tune": `setSidetoneExpected(true)` is sent while the TX mode is CWL or CWU or
  while `TransmitModel::isTune()` is true, and reaches the connection before Tune keys
  (test: the recorded order of calls on a fake connection shows the flag before MOX on).
- Connecting never keys and sends no MOX; the saved level is applied before the radio
  tap starts.
- V-SW-5 (local): no radio gives availability 0 and "No radio connected"; HL2 gives 2;
  every other P1/P2 board 1. `speakerAmplifierAvailable` matches the Task 2 list on P2
  and is false on P1; the G2E reason is "Not yet tested on the ANAN-G2E.".
- Without a connection, setting a value still stores it for the next connect to the
  same MAC only when a MAC is known; otherwise it is held in memory.

**Verification:** ordinary feature with a consequential state transition (connect):
tests first. Unit:
`cmake --build build --target tst_radio_speaker_model tst_radio_codec_speaker_out tst_core_has_no_gui_includes && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_radio_speaker_model|tst_radio_codec_speaker_out|tst_core_has_no_gui_includes)$' --no-tests=error --output-on-failure`.

**Execution note (advisory):** opus. Core RX path: flagged above. After Tasks 1 and 2.

- [ ] **Step 1:** Tests for the acceptance cases with a fake connection.
- [ ] **Step 2:** Properties, persistence, availability, wiring; commit (R-SPK-05 to
  R-SPK-07, R-SPK-11, R-SPK-12, R-SPK-15).

## Task 4: Mirroring the radio speaker to remote windows and the phone

**Requirements:** R-SPK-06 (remote half), R-SPK-13, R-SPK-14, R-SPK-16.

**Files:**
- Modify: `src/core/session/MirrorPolicy.cpp` (three Bidirectional and two Outbound
  `RadioModel` entries; five `featureGates()` rows `{"RadioModel","<property>","radioSpeaker",1}`),
  `src/core/session/StationServer.cpp` (`kPeerOnlyProperties` rows; set
  `caps.radioSpeakerVersion` for a declaring peer), `src/core/session/StationCapabilities.{h,cpp}`
  (`int radioSpeakerVersion = 0;` appended in the current block, before `coreBuildInfo`,
  in `toUpdates` and `fromUpdates`), `src/core/session/StationClient.cpp`
  (`m_declaredFeatures.insert(QByteArrayLiteral("radioSpeaker"), 1);`, apply and outbound
  for the three settable properties), `src/core/session/IStationLink.h` and
  `StationClient` (`radioSpeakerAvailable()` default false plus
  `radioSpeakerUnavailableReason()` "This Core can't set the radio speaker. Update the
  Core."), `src/models/RadioModel.cpp` (remote role: setters write through the mirror,
  values come from the Core; reasons use the link), `docs/architecture/2026-09-23-station-link-v1.md`
  (section 6.3 rows, ordinal table, capability, feature), `tests/data/link/v1/surface.json`
  (regenerated)
- Test: `tests/tst_radio_speaker_link.cpp` (new), `tests/tst_mirror_schema.cpp`,
  `tests/tst_mirror_inbound.cpp`

**Interfaces:**
- Consumes: Task 3's properties.
- Produces: feature `radioSpeaker` 1; capability `radioSpeakerVersion` 1 (read by
  clients and by Task 12's description gate); in a remote window the same five
  properties and reason getters behave as locally, with "This Core can't set the radio
  speaker. Update the Core." when the Core lacks the capability.

**Acceptance (V-SW-4):**
- Core and remote `RadioModel` over the loopback transport: setting volume, mute or mode
  on the remote lands through the Core's real setter, is saved on the Core under its
  radio's MAC, and a second remote sees the change.
- A peer that did not declare `radioSpeaker` 1 receives none of the five properties and
  no `radioSpeakerVersion`; its capability list and radio object are byte-for-byte
  today's (existing goldens unchanged).
- A client of a Core without the capability: availability 0 and the "Update the Core"
  reason; setters do nothing and send nothing.
- `everyMirroredPropertyHasAnExplicitPolicyEntry`, `everyReadOnlyPropertyIsDeniedInbound`,
  `policyEntriesAreUnique` pass; the two reports are denied inbound.
- No existing property ordinal or capability position changes; the surface manifest
  differs only by the added rows; the link document lists them.
- R-SPK-16: `QString RadioModel::radioSpeakerToolTip() const` returns, in a remote
  window, "Radio speaker at the Core (shared with every window and the phone)"; locally
  "Radio speaker" (with the add-on note on an HL2); when unavailable, the reason.

**Verification:** link change, networking: invariant coverage first. Unit and session:
`cmake --build build --target tst_radio_speaker_link tst_mirror_schema tst_mirror_inbound tst_link_surface_manifest_regen tst_shared_input_filters_link && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_radio_speaker_link|tst_mirror_schema|tst_mirror_inbound|tst_link_surface_manifest_regen|tst_shared_input_filters_link)$' --no-tests=error --output-on-failure`.
V-HW-5 stays pending.

**Execution note (advisory):** opus. Networking and the station link: flagged above.
After Task 3. Touches `RadioModel`, `StationServer` and the link document, so nothing
else touching those runs beside it.

- [ ] **Step 1:** Link tests (round trip, withheld, old Core) and policy test updates.
- [ ] **Step 2:** Policy, gates, capability, client plumbing, remote role, document and
  manifest; commit (R-SPK-06, R-SPK-13, R-SPK-14, R-SPK-16).

## Task 5: The icon set and its renderer

**Requirements:** R-SPK-19, D7.

**Files:**
- Create: `resources/icons/emoji/` with the ten SVGs copied byte-for-byte from
  `docs/architecture/2026-10-05-radio-speaker-and-audio-setup-design/icons/`
  (`pc-on`, `pc-muted`, `radio-on`, `radio-muted`, `radio-none`, `lock`, `unlock`,
  `bulb`, `pin`, `pinned`), `src/gui/widgets/AppIcon.{h,cpp}`, `tests/tst_app_icon.cpp`
- Modify: `resources.qrc` (aliases under `/icons/emoji/`), `CMakeLists.txt` (sources),
  `src/gui/applets/RxApplet.cpp` (`m_lockBtn` uses lock/unlock icons at `:672`,
  `682-683`, `2009-2010`, `2125-2126`; text cleared), `src/gui/widgets/VfoWidget.cpp`
  (flag lock button `:3216`, `:3282`), `src/gui/containers/ContainerWidget.cpp` (pin
  `:182`, `:499`: `pin` unpinned, `pinned` pinned; tooltip unchanged),
  `src/gui/TitleBar.cpp` (feature button uses `bulb`; the `makeBulbIcon` painter is
  replaced), tests that assert the old emoji text (`tst_rx_applet_*`, `tst_vfo_widget_*`,
  `tst_title_bar`) move to checking the icon name
- Test: `tests/tst_app_icon.cpp`, `tests/tst_title_bar.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `namespace NereusSDR::AppIcon { QIcon icon(const QString& name, int logicalPx, const QWidget* forDpr = nullptr); QPixmap pixmap(const QString& name, int logicalPx, qreal dpr); }`
  (renders `:/icons/emoji/<name>.svg` through `QSvgRenderer` into a transparent image of
  `logicalPx * dpr`, sets the pixmap's device pixel ratio, no tint; caches by name, size
  and dpr; an unknown name logs `qCWarning` and returns a null icon). Each button that
  uses it sets the dynamic property `nereusIcon` to the name, for tests and captures.

**Acceptance:**
- Each of the ten names renders a non-null pixmap at 16, 18 and 22 px and at dpr 1 and
  2, with non-transparent pixels and the expected size; the result is untinted (a
  sampled pixel of `pc-on`'s cyan wave stays cyan).
- Resource SVGs are byte-identical to the spec folder's: `tst_app_icon` reads both
  copies (spec folder path from `NEREUS_SOURCE_DIR`) and compares bytes.
- RX applet and VFO flag lock buttons show `lock` when locked and `unlock` when not,
  following the slice from any source; the container pin shows `pin` / `pinned`;
  the feature button shows `bulb`. No colour emoji remains in those sites (grep test
  for the replaced code points in the modified files).
- V-UI-3: captures of the RX applet lock, the flag lock, the container title bar
  pinned and not, and the bulb, at 1x and 2x, saved when `NEREUS_ICON_CAPTURE_DIR` is
  set, looked at by the controller; Linux capture pending JJ (the reason for D7).

**Verification:** styling plus a shared widget helper: unit for the renderer, captures
for the sites. Unit:
`cmake --build build --target tst_app_icon tst_title_bar tst_rx_applet_slice_access && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_app_icon|tst_title_bar|tst_rx_applet_slice_access)$' --no-tests=error --output-on-failure`,
plus every test that asserted the old text, by name, after `grep -rn` for the code
points in `tests/`.

**Execution note (advisory):** opus. No prerequisites; may run in a worktree beside
Tasks 1 to 4.

- [ ] **Step 1:** Resources, `AppIcon` and its tests.
- [ ] **Step 2:** Padlocks, pin and bulb; tests that asserted the emoji; captures;
  commit (R-SPK-19, D7).

## Task 6: PC and RADIO in the header

**Requirements:** R-SPK-16, R-SPK-17, R-SPK-06, R-SPK-07, D1, D5, D10.

**Files:**
- Create: `src/gui/widgets/RadioSpeakerWidget.{h,cpp}`, `tests/tst_radio_speaker_widget.cpp`
- Modify: `src/gui/widgets/MasterOutputWidget.{h,cpp}` (icons `pc-on` / `pc-muted` via
  `AppIcon` instead of the emoji; a "PC" word label `pcLabel` between icon and slider;
  behaviour, keys and right-click device menu unchanged), `src/gui/TitleBar.{h,cpp}`
  (takes `RadioModel*` through a setter `setRadioModel(RadioModel*)`; layout: UTC, gap
  18, PC group, 16 px, RADIO group, spacing, feature button; strip stays 32 high),
  `src/gui/MainWindow.cpp` (passes the model), `tests/tst_master_output_widget.cpp`
  (icon name instead of text at `:170`), `tests/CMakeLists.txt`
- Test: `tests/tst_radio_speaker_widget.cpp`, `tests/tst_master_output_widget.cpp`,
  `tests/tst_title_bar.cpp`

**Interfaces:**
- Consumes: Task 3/4 properties and reasons, Task 4's `RadioModel::radioSpeakerToolTip()`;
  Task 5 `AppIcon`.
- Produces: `RadioSpeakerWidget(RadioModel*, QWidget*)` with children `radioSpeakerBtn`
  (checkable 20x20, icons `radio-on` / `radio-muted` / `radio-none`), `radioLabel`
  ("RADIO"), `radioSlider` (100x16, 0 to 100, `kSliderStyle` with the amber fill
  `#e0a030`), `radioValueLabel` (`kDbLabelStyle`, shows the value or "--").

**Acceptance:**
- The slider and the button write `RadioModel` (volume, muted); model changes from any
  source update the widget without echo (`m_updatingFromModel` / `QSignalBlocker`).
- Moving RADIO never changes PC and the reverse (both widgets in one title bar, real
  `AudioEngine`).
- Availability 0: slider and button disabled, icon `radio-none`, readout "--", tooltip
  the reason ("No radio connected" or "This Core can't set the radio speaker. Update
  the Core."). Availability 2: enabled, tooltip names the audio add-on board. Remote
  window: tooltip "Radio speaker at the Core (shared with every window and the phone)".
- Clicking either icon toggles its mute; the icon shows the state (D5).
- `tst_title_bar` `fixedHeight32` still passes; the order of the strip's children is as
  above.
- V-UI-1: captures of the header at 1x and 2x for PC on/muted, RADIO on/muted/
  unavailable, saved when `NEREUS_HEADER_CAPTURE_DIR` is set, compared by the controller
  with `header-layouts.html` layout A; Windows and Linux captures pending JJ.

**Verification:** UI and a control that changes Core state: functional tests plus
captures. Unit:
`cmake --build build --target tst_radio_speaker_widget tst_master_output_widget tst_master_output_widget_signal_refresh tst_title_bar && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_radio_speaker_widget|tst_master_output_widget|tst_master_output_widget_signal_refresh|tst_title_bar)$' --no-tests=error --output-on-failure`.

**Execution note (advisory):** opus. After Tasks 4 and 5.

- [ ] **Step 1:** Widget tests (writes, echo guard, independence, availability).
- [ ] **Step 2:** `RadioSpeakerWidget`, PC label and icons, title bar wiring; captures;
  commit (R-SPK-16, R-SPK-17).

## Task 7: The VFO flag's speaker tab follows the slice's mute

**Requirements:** R-SPK-18, D6.

**Files:**
- Modify: `src/gui/widgets/VfoWidget.cpp` (`buildTabBar()`: tab 0 gets objectName
  `audioTabButton` and `AppIcon` `pc-on`; `applyAudioBinding()` sets `pc-muted` /
  `pc-on` from the mute it already chooses, `m_listenMuted` on a listened flag,
  `m_modelMuted` otherwise), `tests/tst_multi_device_screens.cpp` (find the tab by
  objectName at `:1435`)
- Test: `tests/tst_vfo_widget_audio_tab.cpp` (new, registered in `tests/CMakeLists.txt`)

**Interfaces:**
- Consumes: Task 5 `AppIcon`.
- Produces: `audioTabButton` objectName.

**Acceptance:**
- Slice muted through `SliceModel::setMuted` (as the RX applet, a remote window or the
  phone does) and through the flag's own mute button: the tab icon becomes `pc-muted`;
  unmuted: `pc-on`.
- Listened flag: the icon follows its listen mute, not the slice's.
- Clicking the tab still opens the audio controls.
- V-UI-2: captures playing and muted, saved when `NEREUS_FLAG_CAPTURE_DIR` is set.

**Verification:** UI interaction: functional test plus captures. Unit:
`cmake --build build --target tst_vfo_widget_audio_tab tst_multi_device_screens && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_vfo_widget_audio_tab|tst_multi_device_screens)$' --no-tests=error --output-on-failure`.

**Execution note (advisory):** opus. After Task 5; may run beside Tasks 1 to 4.

- [ ] **Step 1:** Tab test, objectName, binding, captures; commit (R-SPK-18).

## Task 8: Setup page content starts at the top

**Requirements:** R-SPK-21 (problem 3, every page).

**Files:**
- Modify: `src/gui/SetupPage.{h,cpp}` (public `void addContent(QWidget*)` and
  `void addContent(QLayout*)` that insert before the trailing stretch, as `addSection()`
  does), and each page that appends after the stretch, moved to `addContent` or
  `addSection`: `AudioAdvancedPage.cpp`, `AudioDevicesPage.cpp`, `AudioTciPage.cpp` (also
  drop its stray `addStretch` at `:47`), `AudioTxInputPage.cpp`,
  `AppearanceSetupPages.cpp` (`ColorsThemePage`, `MeterStylesPage`, `SkinsPage`,
  `CollapsibleDisplayPage`), `CatNetworkSetupPages.cpp` (`CatSerialPortsPage`,
  `CatTciServerPage`, `CatTcpIpPage`, `CatMidiControlPage`), `DiagnosticsSetupPages.cpp`
  (`DiagSignalGeneratorPage`, `DiagHardwareTestsPage`, `DiagLoggingPage`),
  `DisplaySetupPages.cpp` (`SpectrumDefaultsPage`, `WaterfallDefaultsPage`,
  `GridScalesPage`, `TxDisplayPage`, `Display3DSetupPage`), `TransmitSetupPages.cpp`
  (`PowerPage`, `TxProfilesPage`, `DexpVoxPage`), `DspOptionsPage`, `GeneralOptionsPage`,
  `HardwareDdcRoutingPage`, `HardwarePage`, `KeyboardShortcutsPage`, `MultimeterPage`,
  `SpectrumPeaksPage`, `diagnostics/DiagnosticsPhaseHPages.cpp` (`ConnectionQualityPage`,
  `SettingsValidationPage`, `ExportImportConfigPage`, `LogsPage`),
  `diagnostics/RadioStatusPage.cpp`. Pages that end with their own extra `addStretch`
  drop it. Pages that already replace the stretch (`NrAnfSetupPage`, `AgcAlcSetupPage`,
  `CoresSetupPage`) are left alone.
- Test: `tests/tst_setup_pages_start_at_top.cpp` (new)

**Interfaces:**
- Consumes: nothing.
- Produces: `SetupPage::addContent(QWidget*)`, `SetupPage::addContent(QLayout*)`.

**Acceptance:**
- For every page `SetupDialog` registers, shown at 1400 px tall offscreen: the first
  visible group's top is within 24 px of the content area's top, and the last item in
  the content layout is the stretch.
- No page's controls, order or behaviour change otherwise (existing page tests pass).

**Verification:** behaviour-preserving layout change across many screens: baseline
first (run the existing page tests before editing), then the new test and the same
tests again. Unit:
`cmake --build build --target tst_setup_pages_start_at_top tst_setup_dialog_lazy_pages tst_setup_dialog_qt_warnings tst_setup_controls_connected tst_audio_advanced_page && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_setup_pages_start_at_top|tst_setup_dialog_lazy_pages|tst_setup_dialog_qt_warnings|tst_setup_controls_connected|tst_audio_advanced_page)$' --no-tests=error --output-on-failure`.
Captures of a sample of five pages before and after, looked at by the controller.

**Execution note (advisory):** sonnet would do the mechanical moves, but the test
walks every page through `SetupDialog`: opus. No prerequisites; may run beside Tasks
1 to 4. Must finish before Tasks 9 to 11.

- [ ] **Step 1:** Baseline runs; the new test (fails on today's pages).
- [ ] **Step 2:** `addContent`, every page moved; commit (R-SPK-21).

## Task 9: The Outputs page

**Requirements:** R-SPK-21 (Outputs), R-SPK-22, R-SPK-24 (sound system line, WASAPI
options), D13, D14, D15.

**Files:**
- Create: `src/gui/setup/AudioOutputsPage.{h,cpp}`, `src/gui/setup/SoundSystemLine.{h,cpp}`,
  `tests/tst_audio_setup_regroup.cpp`
- Modify: `src/gui/SetupDialog.cpp` (Audio category: Outputs first; Audio pages no
  longer wrapped by `wrapWithAudioBackendStrip`; Outputs registered `Mixed`),
  `src/gui/setup/AudioDevicesPage.cpp` (its speakers and headphones cards move to
  Outputs; the page goes once Task 10 takes its mic card), `src/gui/setup/DeviceCard.{h,cpp}`
  (a foldable "Device details" section holding Driver API, Sample rate with auto-match,
  Bit depth, Channels, Buffer size with milliseconds, Options, Negotiated; WASAPI boxes
  disabled with the tooltip "These three work only with WASAPI on Windows." unless the
  card's driver API is WASAPI), `CMakeLists.txt`
- Test: `tests/tst_audio_setup_regroup.cpp`, `tests/tst_device_card.cpp`,
  `tests/tst_linux_backend_detection.cpp`

**Interfaces:**
- Consumes: Task 3/4 properties and reasons (Radio speaker section), Task 8 `addContent`.
- Produces: page label "Outputs"; objectNames `soundSystemLine`, `thisComputerGroup`,
  `headphonesGroup`, `radioSpeakerGroup`, `radioSpeakerVolume`, `radioSpeakerMute`,
  `speakerAmplifierChoice` (three radio buttons `ampNormal`, `ampOffOnTx`, `ampAlwaysOff`),
  `speakerAmplifierStatus`, `rescanDevices`; `nereusSetupId`s `audio.outputs.radioSpeakerVolume`,
  `audio.outputs.radioSpeakerMuted`, `audio.outputs.speakerAmplifierMode` (Task 12 uses them).

**Acceptance:**
- Sound system line: Mac "Core Audio"; Windows "Windows audio", naming WASAPI as the
  default driver; Linux PipeWire / PulseAudio through pactl / "None found" in red with
  what to start, following `linuxBackendChanged`. Built per platform with `Q_OS_*`.
- This computer: Volume with Mute is the PC control (moving it moves the header's PC);
  Device; Device details folded by default, unfolding shows every field the card has.
- Headphones: Enabled, Device, Device details; greyed until Enabled; the note says PC
  volume does not affect headphones.
- Radio speaker: status line naming what the radio has (HL2 with the add-on note);
  Volume and "Mute radio speaker" write `RadioModel` and follow it; the amplifier choice
  writes `speakerAmplifierMode`, is disabled with the model's reason when unavailable
  (including the G2E text), shows the R-SPK-10 explanation, and the status line shows
  `RadioModel::speakerAmplifierStatus()` and updates on its signal; remote windows
  show the Core text.
- One "Rescan devices"; on Linux it also calls `rescanLinuxBackend()`.
- The Devices page's note at `:75-77` is gone; no page shows `AudioBackendStrip`.
- Every saved key of the moved cards is unchanged (test reads and writes the same keys).
- V-UI-4 (Outputs): captures folded and unfolded, local and remote, saved when
  `NEREUS_AUDIO_SETUP_CAPTURE_DIR` is set, compared with `audio-setup.html`.

**Verification:** UI plus controls that change Core state: functional tests plus
captures. Unit:
`cmake --build build --target tst_audio_setup_regroup tst_device_card tst_linux_backend_detection tst_setup_dialog_lazy_pages && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_audio_setup_regroup|tst_device_card|tst_linux_backend_detection|tst_setup_dialog_lazy_pages)$' --no-tests=error --output-on-failure`.
V-HW-2 decides the final amplifier wording; until then the explanation names the
speaker jacks only.

**Execution note (advisory):** opus. After Tasks 4 and 8.

- [ ] **Step 1:** Regroup test for Outputs (controls present once, keys unchanged,
  writes reach the model).
- [ ] **Step 2:** Sound system line, folding Device details, WASAPI greying, the page;
  captures; commit (R-SPK-21, R-SPK-22, R-SPK-24).

## Task 10: The Microphone page

**Requirements:** R-SPK-21 (Microphone), R-SPK-22.

**Files:**
- Modify: `src/gui/setup/AudioTxInputPage.cpp` (label "Microphone"; one PC microphone
  section holding Device, Test Mic with meter, capture status, one "Retry microphone",
  "Monitor TX input during transmit", the tone check and Device details, merged from the
  Devices page's input card and today's PC Mic group; radio-mic groups unchanged; Mic
  gain as its own group; sources not picked stay visible and greyed),
  `src/gui/setup/AudioDevicesPage.{h,cpp}` (removed once empty, with its registration and
  `CMakeLists.txt` entry), `src/gui/SetupDialog.cpp`, `src/gui/applets/PhoneCwApplet.cpp`
  (`:1057-1058` opens "Microphone"), `tests/tst_audio_tx_input_*`, `tests/tst_remote_gui_gating.cpp`,
  `tests/tst_settings_scope.cpp`, `tests/tst_receive_only.cpp` (labels)
- Test: `tests/tst_audio_setup_regroup.cpp` (Microphone cases), the five `tst_audio_tx_input_*`

**Interfaces:**
- Consumes: Task 8 `addContent`, Task 9's `DeviceCard` details.
- Produces: page label "Microphone"; every existing `audio.txInput.*` `nereusSetupId`
  kept on the same control.

**Acceptance:**
- The PC microphone device, its retry and its details appear once in Setup; both former
  places wrote the same keys and still do.
- All 13 existing `nereusSetupId`s on the page are present exactly once.
- Radio mic follows `radioMicSelectable()` and its reasons; greyed sections stay visible.
- "Open Setup at TX Input" from the Phone/CW applet opens Microphone.
- V-UI-4 (Microphone) captures as Task 9.

**Verification:** UI regroup: functional tests plus captures. Unit:
`cmake --build build --target tst_audio_setup_regroup tst_remote_gui_gating tst_settings_scope tst_receive_only tst_setup_description_parity && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_audio_setup_regroup|tst_remote_gui_gating|tst_settings_scope|tst_receive_only|tst_setup_description_parity|tst_audio_tx_input_.*)$' --no-tests=error --output-on-failure`
(build the `tst_audio_tx_input_*` targets by name first).

**Execution note (advisory):** opus. After Task 9.

- [ ] **Step 1:** Merge, labels, retargets, tests, captures; commit (R-SPK-21, R-SPK-22).

## Task 11: Digital modes and Advanced

**Requirements:** R-SPK-21 (Digital modes, Advanced), R-SPK-22, R-SPK-24 (VAX rows),
D16.

**Files:**
- Create: `src/gui/setup/AudioDigitalModesPage.{h,cpp}` (VAX section, then TCI section)
- Modify: `src/gui/setup/AudioVaxPage.cpp` (cards: On, Device, Format, Used by,
  Activity renamed from Level, Rename, Copy name; Device row shows the name only on Mac
  and Linux, a picker of detected cables on Windows with "On" disabled until one is
  picked; the per-platform sentence and failure line reworded for operators from the
  meanings at `:577-702`, never "PipeWire" on Mac or Windows; "Detected virtual cables"
  with Rescan moved here from Advanced), `src/gui/setup/AudioTciPage.cpp` (one sentence
  that TCI audio is separate from the PC and radio speaker volumes replaces the "Master
  Mute Behavior" box; Audio stream and Transmit groups), `src/gui/setup/AudioAdvancedPage.cpp`
  (Logs with "Open logs folder", Feature Flags, Reset; cables section removed; DSP group
  stays hidden), `src/gui/SetupDialog.cpp`, `src/gui/MainWindow.cpp` (`:11412` opens
  "Digital modes"), `src/gui/VaxFirstRunDialog.cpp` (`:1000`, `:1086`),
  `tests/tst_audio_vax_page_auto_detect.cpp`, `tests/tst_vax_first_run_dialog.cpp`,
  `tests/tst_remote_gui_gating.cpp`, `tests/tst_audio_advanced_page.cpp`, `CMakeLists.txt`
- Test: `tests/tst_audio_setup_regroup.cpp` (Digital modes and Advanced cases)

**Interfaces:**
- Consumes: Task 8 `addContent`.
- Produces: page labels "Digital modes" and "Advanced"; the Audio category order
  Outputs, Microphone, Digital modes, TX Profile, Advanced.

**Acceptance:**
- Category order exactly as above; each control from today's Devices, TX Input, VAX,
  TCI and Advanced pages appears once across the five (V-SW-6), found by
  `nereusSetupId` or objectName (added where missing).
- Windows build: a VAX card's cable picker is visible and "On" is enabled once a cable
  is picked; Mac and Linux builds: the card shows "NereusSDR VAX <n>" with no picker.
- No string on the VAX page says "PipeWire" on a Mac or Windows build.
- TCI keeps every setting and key; the Opus note for remote windows still shows.
- Advanced holds Logs, Feature Flags and Reset only (plus the hidden DSP group).
- Everything that opened "VAX" opens "Digital modes".
- V-UI-4 (Digital modes, Advanced) captures as Task 9; Windows VAX row capture pending
  JJ.

**Verification:** UI regroup with platform rows: functional tests plus captures. Unit:
`cmake --build build --target tst_audio_setup_regroup tst_audio_vax_page_auto_detect tst_vax_first_run_dialog tst_remote_gui_gating tst_audio_advanced_page tst_setup_pages_start_at_top && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_audio_setup_regroup|tst_audio_vax_page_auto_detect|tst_vax_first_run_dialog|tst_remote_gui_gating|tst_audio_advanced_page|tst_setup_pages_start_at_top)$' --no-tests=error --output-on-failure`.

**Execution note (advisory):** opus. After Task 10.

- [ ] **Step 1:** Regroup tests for the two pages and the order.
- [ ] **Step 2:** Pages, VAX rows, TCI text, retargets; captures; commit (R-SPK-21,
  R-SPK-22, R-SPK-24).

## Task 12: Setup description version 25

**Requirements:** R-SPK-23, R-SPK-14 (description gate).

**Files:**
- Modify: `resources/setup/audio.json` (version 25; `audio.txInput` retitled
  "Microphone", id unchanged; new page `audio.outputs`, `where: station`, with a Radio
  speaker section: `audio.outputs.radioSpeakerVolume` (slider 0 to 100, binding
  `{"property":{"object":"radio","name":"radioSpeakerVolume"}}`),
  `audio.outputs.radioSpeakerMuted` (toggle, "Mute radio speaker"),
  `audio.outputs.speakerAmplifierMode` (choice: Normal, Off while transmitting, Always
  off; values 0, 1, 2), each `gate` `{"capability":"radioSpeakerVersion","min":1}` and
  `requiresDescriptionVersion` 25; the amplifier row's availability follows
  `speakerAmplifierAvailable`), `src/core/setup/SetupDescriptionService.cpp`
  (`kAudioV25Controls` and its validator; accept 25 at `:762`, `:814`, `:823`; downgrade
  to 24 drops the page and the retitle; ceiling at `:3079`; `validateAudioPropertyBinding`
  accepts `radio` bindings to exactly these three properties, checked against
  `MirrorPolicy` as Bidirectional), `tests/tst_setup_description_parity.cpp` (three audio
  pages: `audio.txInput`, `audio.outputs`, `audio.txProfile` in the description's order;
  the Outputs rows match Task 9's `nereusSetupId`s)
- Test: `tests/tst_setup_description_service.cpp`, `tests/tst_setup_description_parity.cpp`,
  `tests/tst_setup_description_live.cpp`

**Interfaces:**
- Consumes: Task 4 capability, Task 9 `nereusSetupId`s.
- Produces: audio description version 25 with `audio.outputs`.

**Acceptance (V-SW-7):**
- Version 25 validates; `audio.txInput` is kept; `audio.outputs` is described with the
  three bound rows.
- A peer asking for 24 gets today's description byte-for-byte.
- A `radio` binding to any other property, or to a read-only one, is rejected.
- Parity: each described Outputs row matches the desktop control with that id.

**Verification:** a shared description other clients render: tests first. Unit:
`cmake --build build --target tst_setup_description_service tst_setup_description_parity tst_setup_description_live tst_mirror_schema && QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_setup_description_service|tst_setup_description_parity|tst_setup_description_live|tst_mirror_schema)$' --no-tests=error --output-on-failure`.

**Execution note (advisory):** opus. After Tasks 4 and 9. Schemas and generated files
are serialised: nothing else edits `resources/setup/` meanwhile.

- [ ] **Step 1:** Service and parity tests for 25 and for 24 unchanged.
- [ ] **Step 2:** Description, validator, ceiling; commit (R-SPK-23).

## Task 13: The iPhone

**Requirements:** R-SPK-20, R-SPK-23 (phone side), D12.

**Files:**
- Modify: `ios/NereusKit/Sources/NereusLink/LinkFeatures.swift` (declare `radioSpeaker`
  1; `setupDescription` 25), `ios/NereusKit/Tests/NereusLinkTests/LinkSurfaceOrdinalTests.swift`
  (the new capability and the five radio properties appended; existing ordinals
  unchanged), `ios/NereusKit/Tests/NereusMirrorTests/SetupDescriptionLatestTests.swift`
  (25), `ios/NereusApp/Audio/SoundPanel.swift` ("Mute" becomes "Mute this phone", same
  binding and id `soundMute`; after the divider a "Radio speaker" section: amber slider
  with readout and "Mute radio speaker", reading `radio` `radioSpeakerVolume` /
  `radioSpeakerMuted` and writing them through the `PropertyWriteQueue`; disabled with a
  note when `capabilityVersion("radioSpeakerVersion") < 1` ("This Core can't set the
  radio speaker. Update the Core."), no radio ("No radio connected") or the add-on case;
  ids `radioSpeakerSlider`, `radioSpeakerMute`), `ios/NereusApp/Tests/SoundPanelTests.swift`.
  The described-page renderer needs no new kind: `audio.outputs` uses `slider`,
  `toggle` and `choice`, which it already draws.
- Test: `SoundPanelTests`, `LinkSurfaceOrdinalTests`, `SetupDescriptionLatestTests`

**Interfaces:**
- Consumes: Task 4 feature and capability, Task 12 description.
- Produces: nothing for later tasks.

**Acceptance:**
- Against `FakeStation` with the capability: moving the slider writes the Core's
  `radioSpeakerVolume`; a Core-side change moves the slider; "Mute radio speaker"
  toggles `radioSpeakerMuted`; "Mute this phone" still mutes only the phone.
- Without the capability, or with no radio: the section is disabled with the note.
- Setup on the phone shows Audio > Outputs with the three rows, and the amplifier
  choice is not in the Sound panel.
- V-UI-5: simulator screenshots available and unavailable, compared with
  `phone-sound-panel.html`.

**Verification:** UI on the phone plus a control that changes Core state. NereusKit:
`ios/scripts/swift-test.sh --jobs 2 --filter LinkSurfaceOrdinalTests` and
`--filter SetupDescriptionLatestTests`; app: under the simulator lock, generate the
project and run the `xcodebuild ... test` command from Global Constraints (or
`-only-testing:NereusAppTests/SoundPanelTests` with `test-without-building` after one
`build-for-testing`). V-HW-5 (phone and desktop following each other on a real Core)
stays pending.

**Execution note (advisory):** opus. After Tasks 4 and 12. Holds the simulator lock;
no other Xcode build runs meanwhile.

- [ ] **Step 1:** Ordinal, description and Sound panel tests.
- [ ] **Step 2:** Feature, version, Sound panel, described page; simulator captures;
  commit (R-SPK-20, R-SPK-23).

## Final checks (once, on the finished branch)

- Desktop: `cmake --build build --target all_tests`, then
  `QT_QPA_PLATFORM=offscreen ctest --test-dir build -LE realtime --output-on-failure`
  and `QT_QPA_PLATFORM=offscreen ctest --test-dir build -L realtime --output-on-failure`
  alone (fast-test-loop.md); `cmake --build build` for the app and `nereusd`;
  `scripts/verify-test-registration.py`; the pre-commit checks run by the hooks.
- iPhone: `ios/scripts/swift-test.sh --jobs 2` and the full scheme test under the lock.
- Docs: `docs/development/project-status.md` row for the design moves to "Built,
  hardware pending"; `CHANGELOG.md` gets the user-facing lines (separate PC and RADIO
  volumes, the amplifier choice, new icons, the regrouped Audio Setup).
- Report code, UI and hardware separately. Hardware pending for JJ: V-HW-1 to V-HW-6
  (G2 independence, amplifier choices in SSB, CW and Tune, CW and Tune silent with
  RADIO muted, headphone and line-out jacks, a P1 board, the HL2 with its audio board,
  remote and phone following each other, the G2E bench test for D17). UI captures on
  Windows and Linux are pending for JJ as well.
