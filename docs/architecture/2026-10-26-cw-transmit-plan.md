# CW Transmit Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** An operator on any supported radio sends CW from the radio's jack, CWX, CAT,
TCI or a serial line, with Off, Semi or QSK break-in and a sidetone, on the carrier
they tuned to, and nothing about CW can key the radio while a transmit check refuses.

**Architecture:** The radio's own keyer makes CW (the TX DSP channel stays off in CW).
A Core `CwKeyerController` on `RadioModel` evaluates a pure `CwKeyerGate` and composes
one `CwWireState` that both protocols encode; host key bits travel through an atomic
`CwKeyLine` that the P1 EP2 sender samples every frame and that wakes an immediate P2
high-priority send. A Core `CwxSender` thread times CWX elements; a `CwSerialKeyInput`
thread reads serial key lines; a port of ChannelMaster `sidetone.c` makes the computer
sidetone on the audio drain. The receiver shifts by the pitch in `composedShiftHz`.

**Tech Stack:** C++20, Qt6 (QtTest), WDSP 2.10 (vendored), Qt6::SerialPort, the
session mirror (`MirrorPolicy`, `StateMirror`, `StationServer`), the CAT and TCI
servers; Swift (NereusKit, SwiftUI) for Task 15.

**Spec:** [CW transmit](2026-10-25-cw-transmit-design.md). Every decision is settled
there (C1 to C14, D1 to D16, requirements R-CW-01 to R-CW-52); this is how it gets
built. Mockup: [cw-mockup.html](2026-10-25-cw-transmit-design/cw-mockup.html).

## Global Constraints

- Read `CLAUDE.md`, `CONTRIBUTING.md`, `docs/development/fast-test-loop.md` and,
  before any port, `docs/attribution/HOW-TO-PORT.md`. They bind every task.
- Ported code: in the same commit, the file header carries the upstream file's header
  byte for byte (every `Copyright (C)` line, the GPL block, the Samphire dual-licence
  statement if present) plus a "Modification history (NereusSDR)" block; a file porting
  from several upstream files gets each header under `// --- From [filename] ---`. New
  ports add a row to `docs/attribution/THETIS-PROVENANCE.md`. Missing notices block the
  PR.
- Every `//` comment inside ported logic is copied verbatim, developer tags above all
  (`//MW0LGE`, `//-W2PA`, `//[2.10.3.13]MW0LGE`, `//MI0BOT`, `//DH1KLM`, ...). Moved
  lines carry `[original inline comment from file:line]`.
  `scripts/verify-inline-tag-preservation.py` enforces it in pre-commit.
- Cites: `// From Thetis file:line [v2.10.3.15]` (pin `3759d09`); mi0bot
  `[v2.10.3.13-beta2]`; deskHPSDR `[@f3d857c]`; Anvelina gateware `[@8e86a61]`;
  AetherSDR `[@1e0718ad]`. Constants keep their exact values as named `constexpr` with
  the cite. WDSP calls match `third_party/wdsp/` exactly.
- New NereusSDR-original files start with `// no-port-check: NereusSDR-original.` and a
  Modification history block; no provenance row.
- Every touched source or test file gets a Modification history line in its existing
  style: `2026-10-26: CW transmit, <what>. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.`
- C++ style: no `goto`, no raw `new`/`delete` (Qt parent or `unique_ptr`), no `#define`
  constants, braces on all control flow, `PascalCase` classes, `camelCase` methods,
  `kPascalCase` constants, `m_camelCase` members, `auto` only when the type is obvious,
  `qCWarning(lcCategory)` for errors, no exceptions. Platform guards `Q_OS_WIN` /
  `Q_OS_MAC` / `Q_OS_LINUX` only.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header
  (`tst_core_has_no_gui_includes`).
- Threads: main (GUI and all models), connection, audio, spectrum, plus the threads in
  `ThreadPlacement.h`. Cross-thread traffic is auto-queued signals or `std::atomic`;
  never hold a mutex in the audio callback.
- Settings: `AppSettings`, never `QSettings`; PascalCase keys; booleans `"True"` /
  `"False"`.
- Receivers are called slices. Words a user sees are plain English with no cites, codes
  or internal names; American spelling; no em dashes in any new text.
- Don't remove code you didn't add. (The `UnbuiltFeature::Cwx` gate sites, the CW
  placeholder fields named in R-CW-43 and the CWX placeholder overlays are replaced as
  the spec requires.)
- Never guess a protocol layout, enum value or limit: the spec's section 3 holds the
  sourced facts. If a step needs one that is not there, stop and report NEEDS_CONTEXT.
- Nothing may key the radio in a test. Tests use the fake connections and codecs; no
  test opens a radio socket.
- Commits are GPG-signed (`git commit -S`); never `--no-gpg-sign`. No `Co-Authored-By`
  trailer. If signing fails, stop and report BLOCKED.
- Builds and test runs go through the load gate, niced:
  `~/.config/nereus/work/lane-admit.sh take cw-transmit-<task>` before, `release` after,
  every command under `nice -n 20`. Run `take` in the background, never as a chain of
  foreground sleeps. Stop if free disk is under 40 GB.
- Build dir: `trunk/build`, one for the whole plan. If it does not exist, configure it
  with the network denied, never downloading:

  ```bash
  DEPS=~/.config/nereus/work/pr-merge-review/review-deps
  ARGS=()
  for d in "$DEPS"/*-src; do
    n=$(basename "$d" -src | tr '[:lower:]' '[:upper:]')
    ARGS+=("-DFETCHCONTENT_SOURCE_DIR_${n}=${d}")
  done
  sandbox-exec -p '(version 1)(allow default)(deny network*)' \
    nice -n 20 cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON \
      -DENABLE_DFNR=OFF -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
      -DOPUS_URL=/Users/j.j.boyd/.config/nereus/work/deps-cache/nereus-core-pins-0368ff16/opus/opus-940d4e5-with-model.zip \
      -DOPUS_URL_HASH=SHA256=740a5220180aee546f97030118517eedafa7956fdfbc8e8432d91f2d20aac4b7 \
      "${ARGS[@]}"
  ```

  Then grep the configure output for "Performing download step"; it must not appear.
- Build single test targets, never the whole suite by default:
  `nice -n 20 cmake --build build --target <tst_name> && ctest --test-dir build -R '^<tst_name>$' --output-on-failure`.
  A direct binary run sets `HOME`, `CFFIXED_USER_HOME` and `NEREUS_TEST_HOME` to one
  scratch folder with `qttest` in its path, `QT_QPA_PLATFORM=offscreen`, and passes
  `-maxwarnings 0`. New tests register with `nereus_add_test()` in `tests/CMakeLists.txt`.
- A failed test is never ignored or retried away. A failure under load is a finding:
  record the load average and the assertion.
- JJ's running NereusSDR, its settings and his Cores are off limits. Nothing is pushed
  by an implementer.
- Release notes are the shepherd sessions' job: wording a task owes the release notes
  (R-CW-29) goes in its report, not in `CHANGELOG.md`.
- Branch `claude/cw-transmit`, cut from `origin/main` `36f4a279e`; the spec is first committed in
  `56b87231c`. Open PRs nearby: #662 (force-31 gates) and #668 (design doc). Do not
  touch `StepAttenuatorController`'s PS-A gates.

## What already exists

- Settings pattern: `TransmitModel::voxHangTimeMs` end to end
  (`src/models/TransmitModel.h:1269-1270, 1613-1614`, `.cpp:3094-3106, 2175-2190`,
  `src/core/session/MirrorPolicy.cpp:511-600`, `StationServer.cpp:13437-13518`,
  `settingRangeRefusal` `TransmitModel.cpp:1067, 1108-1112`).
  `kTransmitSettingsCfcProfileVersion = 15` (`src/core/session/StationCapabilities.h:291`).
- Pitch: station key `CWPitch` (`SettingsScope.cpp:565-566`), read at
  `SliceModel.cpp:2809, 2934, 3066`; `RadioModel::kApfCwPitchHz` (`RadioModel.h:1197-1201`).
- Codecs: `src/core/codec/CodecContext.h` (P2 CW fields `p2Cwx`, `p2Dot`, `p2Dash`
  `:196-201`, `p2CwModeControl` .. `p2CwEdgeLength{7}` `:262-270`, `txStepAttn[3]`
  `:101`); `P1CodecStandard`, `P1CodecAnvelinaPro3`, `P1CodecRedPitaya`, `P1CodecHl2`,
  `P2CodecOrionMkII` under `src/core/codec/`. Selection: `P1RadioConnection::selectCodec`
  (`P1RadioConnection.cpp:4126-4151`), `P2RadioConnection::selectCodec` (`:4059-4111`).
- P1 send path: `P1RadioConnection::composeCommandFrame` on the `P1Ep2Sender` thread
  (`P1RadioConnection.h:709`, `.cpp:5196-5330, 5927-5928`), `fillTxZone`
  (`.cpp:3315-3368`, early return under the cut `:3321-3325`), bank flush flags
  (`.cpp:5867-5900`), HL2 low-bit mask (`.cpp:3216-3222`).
- P2: `sendCmdHighPriority`, `sendCmdTx` (`P2RadioConnection.h:583-586`), `m_cw`
  (`:1080-1090`), `m_tx[0]` (`:1054-1067`), status parse (`.cpp:5195-5219`).
- Cut: `RadioConnection::cutTransmit()` and `transmitCut()` (`RadioConnection.h:1000-1001`),
  `src/core/safety/TransmitCutLine.{h,cpp}`, `RemoteTxCutGuard` (`std::thread` pattern).
- MOX: `src/core/MoxController.{h,cpp}` (states `.h:306-314`, `setMoxImpl` `.cpp:1188`,
  `refusalBeforeTheGate` `.cpp:785-797`, `onCwPtt` `.cpp:3137-3143`, mic PTT dispatch
  `.cpp:1786-1791`, `onRfDelayElapsed` `.cpp:2362-2380`, `onKeyUpDelayElapsed`
  `.cpp:2416-2435`); `PttMode::Cw` (`PttMode.h`), `txReady` consumer
  (`RadioModel.cpp:29930-29960`), `openTxRfGate` (`RadioModel.cpp:35213-35239`).
- Checks: `src/core/safety/BandPlanGuard.{h,cpp}` (`isModeAllowedForTx`
  `.h:211`, `checkMoxAllowed` `.h:234`), `m_moxCheck` (`RadioModel.cpp:23827-24004`),
  `TxInterlockPolicy` (`src/core/TxInterlockPolicy.{h,cpp}`), `TxTimeOutTimer`
  (`src/core/safety/TxTimeOutTimer.{h,cpp}`, wired `RadioModel.cpp:2851-2858`),
  `TxRefusal` (`src/core/safety/TxRefusal.{h,cpp}`).
- Receive: `RadioModel::composedShiftHz` (`RadioModel.cpp:24470-24527`),
  `pushNotchOrigin` (`:24455-24468`), shift triggers (`:25474-25486`),
  `SliceModel::demodulatedRxFrequency` (`SliceModel.h:1493-1501`),
  `src/models/FilterPresetStore.{h,cpp}`, `src/models/NotchModel.cpp:292-345`,
  `TciProtocol.h:840-862`, TUNE (`RadioModel.cpp:30311-30342, 30604, 30630-30650`).
- Audio: `AudioEngine::kTxMonitorSlotId` (`AudioEngine.h:1701`), `drainMixes`
  (`AudioEngine.cpp:4516-4520`), `pumpTxMonitorHandoff` (`:5428-5462`).
- Serial: `src/core/cat/CatSerialTransport.{h,cpp}` (Bill Tracey header, 20 ms poll),
  CAT PTT input `CatService.cpp:880-1000`.
- CAT: `src/core/cat/CatGlobalCommands.cpp` (CW handlers at `:306-721` answer `?;`),
  `tests/data/cat/compatibility.csv`, `requests.json`. TCI: `src/core/TciProtocol.cpp`
  (`:2216-2221`, stubs `:4116-4138`, init `:1155-1168, 1380-1382, 1886-1904`).
- UI: `src/gui/applets/PhoneCwApplet.cpp` (CW page `:648-671`), `CwxApplet.{h,cpp}`,
  `TxApplet.cpp` (gate layers `:460-487, 1606-1660`, `onMoxModeChanged` `:1564-1580`),
  `src/gui/setup/DspSetupPages.cpp` (`:2108-2316`), `src/gui/SpectrumWidget.{h,cpp}`,
  `src/gui/MainWindow.cpp` (CWX construction commented `:11784`).
- Gates: `src/core/UnbuiltFeatureList.{h,cpp}` (`Cwx`), and the 13 sites in the spec's
  research (`MainWindow.cpp:13094-13098, 13586-13633, 14293-14297`,
  `PhoneCwApplet.cpp:299-302, 1673`, `DspSetupPages.cpp:2143, 2192`,
  `StationCatalog.cpp:882, 900, 940-941`, `tests/visual/PreviewScenes.cpp:3385`).
- Threads: `src/core/platform/ThreadPlacement.h:108-117` (`ThreadRole`).
- Remote: `SessionCommandDispatcher.cpp:940-958` (`tx.*` verbs), `RemoteTransmitClient`,
  `StationCapabilities.h:825` (`remoteTxVersion`).
- Phone: `ios/NereusApp/Main/TransmitModel.swift`, `Main/TxPanel.swift`,
  `Modes/TransmitSection.swift`, `App/ToolsTab.swift`, `Band/BandSlicesModel.swift`,
  `Band/ForeignSlicesModel.swift`, NereusKit `NereusBand/BandSlice.swift`.

## File Structure

Create:

| Path | Responsibility |
| --- | --- |
| `src/core/CwTypes.h`, `.cpp` | CW enums, pitch limits, `clampCwPitchHz`, `stationCwPitchHz` (NereusSDR-original) |
| `src/core/codec/CwWireState.h`, `.cpp` | The protocol-neutral CW wire state and its clamp and level helpers (NereusSDR-original, cites Thetis and deskHPSDR facts) |
| `src/core/CwKeyLine.h`, `.cpp` | Atomic host key bits shared by key sources and the senders (NereusSDR-original) |
| `src/core/safety/CwKeyerGate.h`, `.cpp` | Pure gate evaluation (NereusSDR-original) |
| `src/models/CwKeyerController.h`, `.cpp` | Gate inputs, re-evaluation, QSK availability, wire composition (NereusSDR-original with Thetis cites) |
| `src/core/CwxSender.h`, `.cpp` | CWX text, memories, element timing thread (port of `cwx.cs`, plus mi0bot `cwx.cs` for HL2) |
| `src/core/CwSerialKeyInput.h`, `.cpp` | Edge-driven serial key reader thread (port of `SDRSerialPortII.cs` pin roles) |
| `src/core/CwSidetone.h`, `.cpp` | Port of ChannelMaster `sidetone.c` / `sidetone.h` |
| tests listed in each task | |

Modify: the files named in each task.

## Earlier review candidates (keying)

These tasks touch what keys the radio (MOX, PTT, keyer enable, key bits, keyer
timing). The plan runs with one whole-branch review at the end; the controller asks
JJ, before Task 1, whether to add one earlier review:

| Task | What it touches |
| --- | --- |
| 2 | Encodes the CW enable and break-in bits and the radio's keyer timing fields |
| 3 | Writes the host key bits on the P1 sender thread and P2 high priority; the cut latch on those bits |
| 4 | The keyer gate: whether the radio's keyer may run at all |
| 5 | MOX in CW: radio PTT keys MOX, QSK ignores PTT, the TX channel stays off |
| 7 | TUNE in CW (the TX mode switch) |
| 8 | CWX element timing on a new thread, HL2 CWX PTT |
| 9 | CAT `KY` and TCI `keyer` key the radio |
| 10 | Serial paddles, straight key and PTT key the radio |

Recommendation: one earlier review of Tasks 2 to 5 together, after Task 5 and before
Task 7, because every later key source runs through that gate and wire path.

## Task 1: CW settings, pitch property and the mirror

**Requirements:** R-CW-01, R-CW-02 (the property, the accessor and its clamp; the
consumers move in Tasks 6, 7 and 9), R-CW-03; R-CW-51 and R-CW-52 for the touched
headers.

**Files:**
- Create: `src/core/CwTypes.h`, `src/core/CwTypes.cpp`
- Modify: `src/models/TransmitModel.h`, `.cpp` (properties, ranges, persistence,
  `settingRangeRefusal` rows)
- Modify: `src/models/RadioModel.h`, `.cpp` (`cwPitchHz`), `src/models/SliceModel.cpp`
  (the three `CWPitch` reads use `stationCwPitchHz()`; P3 at `:3053-3076` has no caller
  and is changed only in its read)
- Modify: `src/core/session/MirrorPolicy.cpp`, `src/core/session/StationCapabilities.h`
  (`kTransmitSettingsCwVersion = 16`), `src/core/session/StationServer.cpp`
  (`transmitSettingsVersion()` returns 16 with its comment step)
- Modify: `CMakeLists.txt` (new sources)
- Test: `tests/tst_cw_settings.cpp` (new), `tests/tst_filter_presets_thetis.cpp` (clamp
  rows), the mirror policy and capability tests that pin the version (find with
  `grep -rn "transmitSettingsVersion\|kTransmitSettingsCfcProfileVersion" tests`)

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `src/core/CwTypes.h`, namespace `NereusSDR`:

    ```cpp
    enum class CwKeyerMode : int { Straight = 0, IambicA = 1, IambicB = 2 };
    enum class CwBreakInMode : int { Off = 0, Semi = 1, Qsk = 2 };
    enum class CwSerialLine : int { None = 0, Dsr = 1, Cts = 2 };
    // From Thetis setup.designer.cs:38256-38285 [v2.10.3.15] udDSPCWPitch
    inline constexpr int kCwPitchMinHz = 200;
    inline constexpr int kCwPitchMaxHz = 2250;
    inline constexpr int kCwPitchDefaultHz = 600;
    inline constexpr int kCwPitchStepHz = 10;
    int clampCwPitchHz(int hz);      // clamp to [200, 2250], round to nearest 10
    int stationCwPitchHz();          // AppSettings "CWPitch", default 600, clamped
    ```
  - `TransmitModel` int and bool `Q_PROPERTY`s exactly as the spec's R-CW-01 table
    (names, ranges, defaults, keys under `hardware/<mac>/tx/`), enums stored as `int`,
    each with `<name>Changed` NOTIFY, and `k<Name>Min` / `k<Name>Max` constants beside
    `kVoxHangTimeMsMin`. Memories are `cwxMemory1` .. `cwxMemory8` (QString).
  - `RadioModel`: `int cwPitchHz() const;`, `void setCwPitchHz(int hz);`, signal
    `void cwPitchChanged(int hz);`, and `void notifyStationCwPitchMaybeChanged();`,
    which re-reads the key and emits on a change (called wherever the catalogue applies
    a Station key on a remote window).
  - `kTransmitSettingsCwVersion = 16` in `StationCapabilities.h`.

**Acceptance:**
- A fresh profile reads every R-CW-01 default; each setter clamps (speed 0 reads 1,
  61 reads 60; weight 20 reads 33; key-down 300 reads 255; break-in delay 1500 reads
  1000; return 0 reads 1, 10005 reads 10000 and rounds to the 10 ms step), is idempotent
  (no signal on an equal value) and persists under `hardware/<mac>/tx/<Key>` with
  booleans as `"True"`/`"False"`; a second `TransmitModel` loading the same MAC reads
  them back.
- `clampCwPitchHz`: 150 to 200, 2400 to 2250, 604 to 600, 605 to 610, 600 to 600.
  `stationCwPitchHz()` with `CWPitch` = 100 returns 200.
- `setCwPitchHz(705)` stores 710 and emits `cwPitchChanged(710)` once; a second call
  with 710 emits nothing.
- `tst_filter_presets_thetis`: the existing 700 and 300 rows still pass; new rows for
  150 (behaves as 200) and 2400 (behaves as 2250).
- Mirror: every new property is `Bidirectional` on `transmit`; a remote `TransmitModel`
  round-trips each through a test Core; a peer declaring `transmitSettingsVersion` 15
  gets the write refused; a remote write of speed 80 is refused with
  "Choose a keyer speed from 1 to 60 WPM."
- No existing transmit setting test changes behavior except the version number.

**Verification:** Low to medium risk (settings and wire version). Unit and integration
tests: `tst_cw_settings`, `tst_filter_presets_thetis`, the mirror and capability tests
found above, `tst_core_has_no_gui_includes`.

**Execution note (advisory):** opus. No keying. First task; nothing runs in parallel.

- [ ] **Step 1:** Add `CwTypes` and the pitch accessor; switch the three `SliceModel`
      reads to it.
- [ ] **Step 2:** Add the `TransmitModel` properties, persistence and range refusals.
- [ ] **Step 3:** Add `RadioModel::cwPitchHz`, its setter and the catalogue hook.
- [ ] **Step 4:** Mirror rows, version 16, and the tests above; commit.

## Task 2: CW wire state and the codec fields

**Requirements:** R-CW-11, R-CW-13, R-CW-14, R-CW-16 (the CmdTx composition and edge
length; the sending is Task 3), R-CW-38 (the level formula), R-CW-51.

**Files:**
- Create: `src/core/codec/CwWireState.h`, `.cpp`
- Modify: `src/core/codec/CodecContext.h` (add `CwWireState cw;`; `p2CwEdgeLength`
  default 9)
- Modify: `src/core/codec/P1CodecStandard.cpp` (banks 12 C2 bit 6, C3, C4; bank 13
  C1-C3; bank 14 C1-C4), `P1CodecRedPitaya.cpp` (its `bank12`), `P1CodecHl2.cpp`
  (banks 12-14); `P1CodecAnvelinaPro3` inherits and is not edited
- Modify: `src/core/P2RadioConnection.h` (`CwState::edgeLength{9}`)
- Modify: `tests/data/p1_baseline_bytes.json`, `tests/data/p2_baseline_bytes.json`
  and the header notes in `tests/tst_p1_regression_freeze.cpp`,
  `tests/tst_p2_regression_freeze.cpp`
- Test: `tests/tst_cw_wire_state.cpp` (new), `tests/tst_p1_codec_standard.cpp`,
  `tst_p1_codec_hl2.cpp`, `tst_p1_codec_redpitaya.cpp`, `tst_p1_codec_anvelina_pro3.cpp`,
  `tst_p2_codec_orionmkii.cpp`

**Interfaces:**
- Consumes: `CwKeyerMode`, `CwBreakInMode` (Task 1).
- Produces, in `src/core/codec/CwWireState.h`, namespace `NereusSDR`:

  ```cpp
  struct CwWireState {
      bool enable{false};          // radio keyer may run (R-CW-07); P1 bank 13 C1, P2 byte 5 bit 1
      bool breakIn{false};         // P2 byte 5 bit 7
      int  keyerMode{2};           // CwKeyerMode
      int  speedWpm{25};
      int  weight{50};
      bool strictSpacing{false};
      bool reversePaddles{false};
      int  sidetoneLevel{0};       // 0..127; P2 byte 5 bit 4 set when > 0
      int  sidetoneFreqHz{600};
      int  rfDelayMs{7};
      int  hangMs{310};
      int  edgeLengthMs{9};        // P2 byte 17; From Thetis setup.cs:29327 [v2.10.3.15]
      bool qskTxAttenuation{false};// P2 CmdTx bytes 57-59 = 31 (R-CW-16)
      bool operator==(const CwWireState&) const = default;
  };
  void applyCwWireClamps(CwWireState& s);   // speed 1..60, weight 33..66, level 0..127,
                                            // rfDelay 0..255, hang 0..1023
  int cwHangMs(CwBreakInMode mode, int breakInDelayMs, int keyUpDelayMs);
      // Semi: min(d + u, 1023); Qsk: min(u, 1023); Off: 0
  int cwRfDelayMs(int keyDownMs, int speedWpm);
      // min(keyDownMs, 255, 900 / max(speedWpm, 1)); From deskHPSDR old_protocol.c:2823-2831 [@f3d857c]
  int cwRadioSidetoneLevel(bool sidetone, bool radioSidetone, bool radioMuted,
                           CwBreakInMode mode, float monitorVolume);
      // 0 unless all three allow; txaf = round(v*100); (int)(txaf*1.27) Semi/Qsk, (int)(txaf*0.73) Off;
      // From Thetis console.cs:13046-13089 [v2.10.3.15]
  quint8 composeP2CwModeControl(const CwWireState& s, bool eer);
      // bit0 eer, 1 enable, 2 reverse, 3 iambic (mode != Straight), 4 level > 0,
      // 5 mode B, 6 strict spacing, 7 breakIn; From Thetis network.h:151-166 [v2.10.3.15]
  ```

  `CodecContext::cw` is what the P1 codecs read. The P2 codec keeps reading its `p2Cw*`
  fields, which Task 3 fills from the same `CwWireState`.

**Acceptance:**
- Helpers: `cwHangMs(Semi, 300, 10)` = 310, `(Semi, 1000, 500)` = 1023, `(Qsk, 300, 10)`
  = 10, `(Off, 300, 10)` = 0. `cwRfDelayMs(7, 25)` = 7, `(300, 5)` = 180,
  `(200, 30)` = 30, `(50, 0)` = 50 (speed treated as 1, then 255 cap, then 50).
  `cwRadioSidetoneLevel(true, true, false, Semi, 0.5f)` = 63 (Thetis truncates),
  `(..., Off, 0.5f)` = 36, `(true, true, true, Semi, 0.5f)` = 0,
  `(true, false, false, Semi, 1.0f)` = 0, `(true, true, false, Semi, 1.0f)` = 127.
  `composeP2CwModeControl` for every bit alone, EER kept.
- P1 Standard, Anvelina Pro3, Red Pitaya and HL2, from a context with speed 30,
  IambicA, weight 55, strict on, reverse on, enable on, level 90, RF delay 12, hang 310,
  freq 600: bank 12 C2 bit 6 = 1 and the attenuator bits unchanged, C3 = `0x5E`
  (`30 | 0x40`), C4 = `0xB7` (`55 | 0x80`); IambicB gives C3 `30 | 0x80`, Straight gives
  `30`; bank 13 C1 = 1, C2 = 90, C3 = 12, C4 = 0; bank 14 C1 = `310 >> 2` = 77, C2 =
  `310 & 3` = 2, C3 = `600 >> 4` = 37, C4 = `600 & 0x0F` = 8. Enable off gives bank 13
  C1 = 0 with the other fields still sent. Each C0 keeps its MOX bit and address.
- P2 CmdTx from the matching `p2Cw*` fields: byte 5 = `composeP2CwModeControl`, byte 6
  level, 7-8 frequency, 9 speed, 10 weight, 11-12 hang, 13 RF delay, 17 = 9.
- The two freeze baselines are regenerated for the new defaults and each freeze test's
  header says why (CW defaults now sent at all times, as Thetis; edge length 9). The
  legacy composers are unchanged (R-CW-14).

**Verification:** High risk (bits the radio's keyer obeys). Test-first on the helper
and codec byte cases above. Unit: `tst_cw_wire_state`, the five codec tests, both
freeze tests.

**Execution note (advisory):** opus. Keying: encodes the CW enable and break-in bits
(earlier review candidate). After Task 1.

- [ ] **Step 1:** `CwWireState` and helpers with their tests.
- [ ] **Step 2:** P1 codec banks 12-14 for all four codecs, with Thetis v2.10.3.15 cites
      and the mi0bot cites in `P1CodecHl2`.
- [ ] **Step 3:** P2 edge length 9 and the CmdTx byte cases.
- [ ] **Step 4:** Regenerate the freeze baselines with their notes; commit.

## Task 3: Connection CW state, key bits, the cut, and radio key status

**Requirements:** R-CW-07 (the sends), R-CW-08, R-CW-12, R-CW-15 (the HL2 bit 3 and the
store rule on the wire), R-CW-16 (assigning `m_cw` and `m_tx[0]`, immediate sends, QSK
attenuation bytes), R-CW-17, R-CW-51.

**Files:**
- Create: `src/core/CwKeyLine.h`, `.cpp`
- Modify: `src/core/RadioConnection.h`, `.cpp` (`setCwWireState`, `cwKeyLine()`,
  `radioKeyStatus` signal)
- Modify: `src/core/P1RadioConnection.h`, `.cpp` (store the wire state, bank 13 next on
  an enable change, key overlay in `composeCommandFrame`, dot and dash status)
- Modify: `src/core/P2RadioConnection.h`, `.cpp` (assign `m_cw` and `m_tx[0]`, immediate
  CmdTx and high priority, bytes 57-59 under QSK, dot, dash and key-out status)
- Modify: `CMakeLists.txt`
- Test: `tests/tst_cw_key_line.cpp` (new), `tests/tst_p1_cw_key_overlay.cpp` (new),
  `tests/tst_p2_cw_wire.cpp` (new), `tests/tst_p1_status_telemetry.cpp`,
  `tests/tst_p2_status_telemetry.cpp`, `tests/tst_p1_tx_iq_wire.cpp` (HL2 mask still
  applies outside CW)

**Interfaces:**
- Consumes: `CwWireState`, `composeP2CwModeControl` (Task 2).
- Produces:

  ```cpp
  // src/core/CwKeyLine.h
  class CwKeyLine {
  public:
      void setCwx(bool down); void setDot(bool down); void setDash(bool down);
      void setCwxPtt(bool on);
      bool cwx() const; bool dot() const; bool dash() const; bool cwxPtt() const;
      // P1 order: dot<<2 | dash<<1 | cwx; HL2 adds cwxPtt<<3.
      // From Thetis networkproto1.c:738-741 [v2.10.3.15]; mi0bot networkproto1.c:1247-1258 [v2.10.3.13-beta2]
      quint8 p1Bits(bool hl2) const;
      // Called on the writer's thread after any change. Must not block.
      void setOnChange(std::function<void()> fn);
  private:
      std::atomic<quint8> m_bits{0};
      std::function<void()> m_onChange;   // set once before any writer starts
  };
  ```

  - `RadioConnection`: `virtual void setCwWireState(const CwWireState& s);` (called
    queued on the connection thread); `std::shared_ptr<CwKeyLine> cwKeyLine() const;`
    (created with the connection, never null); signal
    `void radioKeyStatus(bool dot, bool dash, bool keyOut);` emitted on change.

**Acceptance:**
- `CwKeyLine`: each setter alone gives the right `p1Bits(false)` (dot 4, dash 2, cwx 1)
  and `p1Bits(true)` with cwxPtt (8); concurrent writers from two threads never lose a
  bit (a 100,000-iteration stress with distinct bits per thread).
- P1 overlay (fake socket, frames captured on the sender): with `enable` 1 and key bits
  5, every I and Q 16-bit big-endian sample in both zones of the frame equals `0x0005`;
  with `enable` 0 the zones are byte-identical to `fillTxZone`'s output (HL2: low-bit
  mask intact); with `enable` 1 and `transmitCut()` set the samples are 0 and bank 13
  C1 is 0. HL2 with cwx down, breakIn 0, MOX 0, cwxPtt 0 gives bit 0 clear (mi0bot
  store rule); with cwxPtt 1, bits 0 and 3 set. An enable change puts bank 13 in the
  next frame.
- P2: `setCwWireState` with a change sends CmdTx once at once with byte 5 composed and
  `p2CwEdgeLength` 9; a key change from another thread sends high priority once on the
  connection thread with byte 5 = `dash<<2 | dot<<1 | cwx` (a burst of 50 changes posts
  a bounded number of sends and the last packet carries the last state); with
  `qskTxAttenuation` true bytes 57-59 are 31; under the cut, CmdTx byte 5 bits 1 and 7
  and high-priority byte 5 are 0 and both packets go out at once.
- Status: P1 C0 `0x02` gives dash 1, `0x04` dot 1, `0x01` PTT only, I2C subframes
  ignored; P2 byte 4 `0x02` dot, `0x04` dash, `0x80` key-out; `radioKeyStatus` fires
  only on change. Each read carries a comment noting how it differs from Thetis
  (nP1:329-331 left shifts; nP2:686-693 swap) and why.

**Verification:** High risk (key bits and the cut on the send threads). Test-first on
the overlay and cut cases. Unit and integration with the fake connections. The cut
case is a must-pass.

**Execution note (advisory):** opus. Keying: earlier review candidate. After Task 2.

- [ ] **Step 1:** `CwKeyLine` and its tests.
- [ ] **Step 2:** P1 wire state, bank 13 flush, overlay with the cut check, HL2 rule.
- [ ] **Step 3:** P2 assignment, immediate sends, QSK bytes, cut.
- [ ] **Step 4:** Status parsing on both protocols and `radioKeyStatus`; commit.

## Task 4: The keyer gate and the CW keyer controller

**Requirements:** R-CW-05, R-CW-06, R-CW-07 (composition), R-CW-10, R-CW-13
(composition), R-CW-16 (`qskTxAttenuation`), R-CW-20 (availability only), R-CW-22
(availability only), R-CW-38 (composition), R-CW-51.

**Files:**
- Create: `src/core/safety/CwKeyerGate.h`, `.cpp`; `src/models/CwKeyerController.h`,
  `.cpp`
- Modify: `src/core/safety/BandPlanGuard.cpp` (`isModeAllowedForTx` allows CWL and CWU;
  the check uses the carrier only for CW, as Thetis `console.cs:6778-6813`)
- Modify: `src/models/RadioModel.h`, `.cpp` (own the controller; new signals
  `bandPlanRegionChanged()`, `extendedTransmitChanged()`,
  `preventTxOnDifferentBandChanged()` emitted from every writer of those three keys;
  push the wire state to the connection; time-out on QSK key-out)
- Modify: every writer of `BandPlanRegion`, `ExtendedTransmit`,
  `PreventTxOnDifferentBandToRx` (find with `grep -rn` in `src/`), to call the
  `RadioModel` notifier
- Modify: `CMakeLists.txt`
- Test: `tests/tst_cw_keyer_gate.cpp` (new), `tests/tst_cw_keyer_controller.cpp` (new),
  `tests/tst_band_plan_guard_mox_rejection.cpp` (CW rows),
  `tests/tst_operator_wording_sweep.cpp` (the removed sentence)

**Interfaces:**
- Consumes: Task 1 properties and pitch; Task 2 helpers; Task 3 `setCwWireState`,
  `radioKeyStatus`.
- Produces:

  ```cpp
  // src/core/safety/CwKeyerGate.h, namespace NereusSDR::safety
  struct CwKeyerGateInputs {
      bool hasTxSlice{false};
      bool txSliceInCw{false};          // CWL or CWU
      MoxCheckResult bandPlan;          // checkMoxAllowed for the carrier in CW
      bool receiveOnly{false};          // setting, board, Core policy or role
      bool txInhibited{false};
      bool paTripped{false};
      bool radioLinkDown{false};
      bool radioChangeUnderway{false};
      bool stopLatched{false};          // emergency stop or TX time-out latch
      QString interlockRefusal;         // empty when TxInterlockPolicy passes
      bool interlockArmed{false};
      bool interlockGranted{false};
  };
  struct CwKeyerGateResult { bool allowed{false}; TxRefusal refusal; };
  CwKeyerGateResult evaluateCwKeyerGate(const CwKeyerGateInputs& in);
      // order of refusals: no TX slice, not CW, receive only, inhibit, PA trip,
      // link down, radio change, stop latch, band plan, interlock, interlock not granted
  ```

  ```cpp
  // src/models/CwKeyerController.h
  class CwKeyerController : public QObject {
      Q_OBJECT
  public:
      CwKeyerController(RadioModel& radio, QObject* parent = nullptr);
      bool keyerAllowed() const;
      TxRefusal refusal() const;
      CwBreakInMode effectiveBreakInMode() const;   // after QSK and D16 fallbacks
      bool qskAvailable() const; QString qskUnavailableReason() const;
      bool semiAvailable() const; QString semiUnavailableReason() const; // D16
      CwWireState wireState() const;
  signals:
      void keyerStateChanged();      // allowed, refusal or effective mode changed
      void wireStateChanged(const NereusSDR::CwWireState& s);
  };
  ```

  QSK availability is exactly R-CW-20's list: not HL2; board Orion or OrionMKII, or
  Saturn or SaturnMKII; P2 connection; Orion or OrionMKII with
  `RadioInfo::firmwareVersion >= 17`; TX antenna
  equals RX antenna for the TX slice; not a transverter band (always true while
  transverters are unbuilt, and the check is written against the transverter state so it
  applies when they land); no armed amplifier interlock. Reasons, in that order:
  "The Hermes Lite 2 can't do QSK.", "This radio can't do QSK.",
  "QSK needs newer radio firmware.", "QSK needs radio firmware 1.7 or later.", "QSK needs the
  same antenna for receive and transmit.", "QSK is off on transverter bands.", and the
  R-CW-22 sentence. (HL2 comes first so an HL2, a Protocol 1 radio, is told it by name.)

**Acceptance:**
- Gate truth table: each input alone, with all others passing, gives `allowed` false
  and its own refusal, in the order above; all passing gives true. The PC microphone
  check is not an input.
- Band plan: a CWU carrier inside a band edge with the filter crossing it is allowed
  (carrier only); 1 Hz outside is refused with the existing band-plan words; CW on US
  60 m is allowed per `console.cs:29466`.
- Controller: changing slice frequency, mode, XIT, TX slice, inhibit, PA trip, link,
  receive only, interlock, either latch, or any of the three new band-plan signals
  re-evaluates once and emits `wireStateChanged` only when the state changed.
- Composition: `enable` = allowed; `breakIn` = allowed and effective mode is Semi or
  QSK; `hangMs` = `cwHangMs(effective mode, delay, key-up)`; `rfDelayMs` =
  `cwRfDelayMs`; level = `cwRadioSidetoneLevel(..., RADIO muted, ...)`; frequency =
  pitch; `qskTxAttenuation` = allowed and effective mode QSK. A pitch, setting or RADIO
  mute change re-emits.
- Fallbacks: QSK selected on a board without it gives effective Semi with the reason;
  an armed interlock gives effective Off and Semi unavailable with the R-CW-22 sentence;
  in Off with an armed interlock, `allowed` waits for `interlockGranted`.
- Time-out: in QSK, `radioKeyStatus(keyOut = true)` runs the TX time-out as MOX does;
  its expiry latches and turns the gate false (R-CW-10).
- `tst_operator_wording_sweep` and `tst_band_plan_guard_mox_rejection` no longer expect
  "CW transmit is not available on this Core".

**Verification:** High risk (the safety rule). Test-first on the truth table and the
fallbacks. Unit and integration with a test `RadioModel` and the fake connection.

**Execution note (advisory):** opus. Keying: earlier review candidate. After Task 3.

- [ ] **Step 1:** `evaluateCwKeyerGate` and its truth table.
- [ ] **Step 2:** Band plan CW allow, carrier-only check.
- [ ] **Step 3:** The three new setting signals and their writers.
- [ ] **Step 4:** `CwKeyerController`: inputs, availability, composition, push to the
      connection; the QSK time-out; commit.

## Task 5: MOX in CW, break-in and QSK

**Requirements:** R-CW-09, R-CW-18, R-CW-19, R-CW-20 (PTT and mode behavior), R-CW-21,
R-CW-22 (fallback words reach the operator), R-CW-51, R-CW-52 (the `MoxController.h`
delay cites refreshed to v2.10.3.15).

**Files:**
- Modify: `src/core/MoxController.h`, `.cpp` (CW branch; `onCwPtt` carries the radio's
  key in CW; QSK ignores every PTT source; release on the radio's PTT drop)
- Modify: `src/models/RadioModel.cpp` (the `txReady` consumer skips `openTxRfGate` in
  CW; route `radioKeyStatus` and `micPttFromRadio` in CW; QSK enter and leave side
  effects; leaving CW turns QSK off and remembers it)
- Test: `tests/tst_mox_controller_cw.cpp` (new), `tests/tst_cw_qsk_side_effects.cpp`
  (new), `tests/tst_mox_controller_ptt_sources.cpp`, `tests/tst_mox_controller_timers.cpp`

**Interfaces:**
- Consumes: `CwKeyerController` (`keyerAllowed`, `refusal`, `effectiveBreakInMode`,
  `keyerStateChanged`) and `radioKeyStatus` (Tasks 3, 4).
- Produces:
  - `MoxController::setCwModeActive(bool txSliceInCw)` and
    `MoxController::setCwBreakInMode(CwBreakInMode effective)`, set by `RadioModel` from
    the controller.
  - `MoxController::reportCwRefusal(const TxRefusal& r)`, which emits `moxRejected` and
    `moxRefused` under the existing quiet-repeat rule.
  - QSK restore keys under `hardware/<mac>/tx/`: `CW_QskSavedAgcMode` (int),
    `CW_QskSavedHangThreshold` (int), `CW_QskActive` (bool).

**Acceptance:**
- CW, Semi: radio PTT on keys MOX as `PttMode::Cw` with the "radio key" identity, the
  hardware flips, the TX channel's `setRunningAsync(true)` is never called, no RF-delay
  wait; PTT off unkeys with no drain, key-up or MOX wait and with `ptt_out_delay`
  (state walk observed through the existing phase signals).
- CW, Off: the same for radio PTT; the serial PTT line (simulated through the source
  API) keys only in Semi.
- CW, QSK: radio PTT, CAT, TCI, VOX and serial PTT never key MOX.
- Voice modes: unchanged (`tst_mox_controller_*` stay green).
- Gate refused: a dot or dash in `radioKeyStatus`, or radio PTT in CW, emits one
  `moxRejected` with the gate's words per hold and does not key MOX.
- QSK enter: TX slice AGC becomes Custom, a hang threshold of 50 becomes 100 (80 stays
  80); saved values persist; leaving QSK restores them; a restart with `CW_QskActive`
  true and QSK no longer selected restores them at load. The break-in delay setting is
  never written.
- Leaving CW with QSK selected sets Off and remembers QSK; returning to CW selects QSK
  again if available. A QSK fallback (antenna change) shows its reason through
  `moxRejected` once.

**Verification:** High risk (MOX). Test-first on the state walk. Unit and integration.

**Execution note (advisory):** opus. Keying: earlier review candidate; the recommended
earlier review covers Tasks 2 to 5 after this task. After Task 4.

- [ ] **Step 1:** MoxController CW branch and its tests.
- [ ] **Step 2:** RadioModel routing, TX channel skip, refusal reporting.
- [ ] **Step 3:** QSK side effects with persisted restore; commit.

## Task 6: Receive shift, passband origin, notch, TCI note, APF and pitch follow

**Requirements:** R-CW-23, R-CW-24 (the model helper only; drawers are Tasks 13 and 15),
R-CW-25, R-CW-26, R-CW-27, R-CW-28, R-CW-29 (wording in the report), R-CW-52 (the
stale Longpath headers and the APF cite).

**Files:**
- Modify: `src/models/RadioModel.cpp`, `.h` (CW term in `composedShiftHz(slice, centre)`;
  re-push on `cwPitchChanged`; APF uses the pitch; remove the use of `kApfCwPitchHz`
  in favor of the pitch, keeping the constant only if a test still names it)
- Modify: `src/models/SliceModel.h`, `.cpp` (`demodulatedRxFrequency` term;
  `double passbandOriginHz() const`; recenter stored per-band CW filters on a pitch
  change)
- Modify: `src/models/FilterPresetStore.{h,cpp}` (recenter overridden CW slots keeping
  width)
- Modify: `src/models/NotchModel.cpp` (comments only), `src/core/TciProtocol.h` (note)
- Modify: `src/gui/setup/DspSetupPages.cpp` (APF Center range follows the pitch)
- Test: `tests/tst_notch_tune_frequency.cpp`, `tests/tst_notch_add_at_slice.cpp`,
  `tests/tst_tnf_ui_wiring.cpp`, `tests/tst_tx_frequency_follows_tx_slice.cpp`,
  `tests/tst_tci_live_broadcast_per_slice.cpp`, `tests/tst_filter_preset_store.cpp`,
  `tests/tst_setup_controls_connected.cpp`, `tests/tst_cw_pitch_follow.cpp` (new)

**Interfaces:**
- Consumes: `RadioModel::cwPitchHz`, `cwPitchChanged` (Task 1).
- Produces: `double SliceModel::passbandOriginHz() const` = `frequency()` minus the
  pitch in CWU, plus the pitch in CWL, `frequency()` otherwise (pitch from
  `stationCwPitchHz()`), and `double SliceModel::cwShiftHz() const` (the same signed
  term alone).

**Acceptance:**
- CWU at 14.030000 MHz, pitch 600, RIT off: `composedShiftHz` equals the stream term
  minus 600; `tunefreq + shift == demodulatedRxFrequency()` == 14.029400 MHz; CWL gives
  14.030600. `txFrequencyForSlice` stays 14.030000 (plus XIT when on).
- Pitch 600 to 700 re-pushes every CW slice's shift once and the APF center (pitch plus
  tune); no dial or carrier change; CWU preset 400..800 becomes 500..900; an overridden
  CWU slot 300..700 becomes 500..900 (recentered on the new pitch, width kept); a stored per-band CWU filter 450..750 becomes
  550..850; the current filter is re-applied.
- +TNF in CWU at dial 14.030000 lands at 14.030000. `NotchModel` stores clicked
  frequencies exactly (`tst_notch_model_guards` unchanged).
- TCI `dds` and `if` in CWU equal today's values and `vfo = dds + if`.
- USB, LSB, DIGU, DIGL and AM rows in the touched tests are unchanged.

**Verification:** High risk (receive path, flagged). Test-first on the shift and
invariant rows. Unit tests above; the bench receive check is V-HW-1 (Task 16).

**Execution note (advisory):** opus. Receive path, not keying. Independent of Tasks 2
to 5 in files; it could run in a worktree after Task 1, but run it in order.

- [ ] **Step 1:** The shift term, `demodulatedRxFrequency`, re-push, invariant tests.
- [ ] **Step 2:** `passbandOriginHz` and `cwShiftHz`.
- [ ] **Step 3:** APF and the Setup APF range; filters follow the pitch.
- [ ] **Step 4:** Notch comments, TCI note, stale headers; R-CW-29 wording in the
      report; commit.

## Task 7: TUNE in CW and the pitch tone

**Requirements:** R-CW-30, R-CW-52 (TUNE tone cites refreshed to `console.cs:30082/30085`).

**Files:**
- Modify: `src/models/RadioModel.cpp` (`signedTuneToneHz` and `txNcoFrequencyForSlice`
  use the pitch latched at TUNE start; the CW swap at `:30630-30650` sets only the TX
  channel's mode)
- Test: `tests/tst_radio_model_set_tune.cpp`, `tests/tst_tx_channel_tune_tone.cpp`,
  `tests/tst_tx_analyzer_skirt.cpp`

**Interfaces:**
- Consumes: `cwPitchHz` (Task 1), the CW TX-channel skip (Task 5).
- Produces: nothing new.

**Acceptance:**
- Pitch 700: TUNE tone is +700 in USB, DIGU, AM, FM, CWU and -700 in LSB, DIGL, CWL;
  the NCO compensation keeps the carrier on the dial plus XIT; a pitch change during
  TUNE does not move the tone until the next TUNE.
- TUNE in CWU: the slice stays CWU (shift, filter, flag unchanged); the TX channel runs
  USB; TUNE off restores it; the existing four swap tests change to assert the TX mode
  and an unchanged slice mode.
- TUNE in CW starts the TX channel (the Task 5 skip applies to MOX keying, not TUNE).

**Verification:** High risk (TUNE keys the radio). Unit tests above.

**Execution note (advisory):** opus. Keying (TUNE). After Task 5.

- [ ] **Step 1:** Latched pitch tone and NCO; tests.
- [ ] **Step 2:** TX-mode-only swap in CW; tests; commit.

## Task 8: The CWX sender

**Requirements:** R-CW-15 (CWX PTT drive on HL2), R-CW-31, R-CW-32, R-CW-33 (the
request gate), R-CW-51.

**Files:**
- Create: `src/core/CwxSender.h`, `.cpp` (Thetis `cwx.cs` header byte for byte; mi0bot
  `cwx.cs` header under `// --- From [cwx.cs (mi0bot)] ---`)
- Modify: `src/core/platform/ThreadPlacement.h`, `.cpp` (`ThreadRole::CwxSender`,
  demand flag)
- Modify: `src/models/RadioModel.h`, `.cpp` (own the sender; connect it to the current
  connection's `CwKeyLine`; abort on leaving CW and on the gate turning false)
- Modify: `docs/architecture/overview.md` (thread table: every thread that exists today,
  plus `CwxSender` and `CwSerialKeyInput`)
- Modify: `docs/attribution/THETIS-PROVENANCE.md`, `CMakeLists.txt`
- Test: `tests/tst_cwx_sender.cpp` (new)

**Interfaces:**
- Consumes: `CwKeyLine` (Task 3), `CwKeyerController` (Task 4), `TransmitModel` CWX
  properties (Task 1).
- Produces:

  ```cpp
  class CwxSender : public QObject {
      Q_OBJECT
  public:
      using Clock = std::function<std::chrono::steady_clock::time_point()>;
      using Sleeper = std::function<void(std::chrono::steady_clock::time_point until)>;
      explicit CwxSender(QObject* parent = nullptr);
      CwxSender(Clock clock, Sleeper sleeper, QObject* parent = nullptr); // tests
      ~CwxSender() override;                         // stops and joins its thread
      void setKeyLine(std::shared_ptr<CwKeyLine> line);
      void setHl2(bool hl2);                         // drive cwxPtt (R-CW-15)
      void setSpeedWpm(int wpm);                     // 5..60
      void setRepeat(bool on, int delaySeconds);     // 1..60
      void setBreakInActive(bool on);                // false in Off
      void setMoxActive(bool on);
      // Returns false with words when refused (not CW, or nothing to send).
      bool sendText(const QString& text);
      bool sendMemory(int number);                   // 1..8, using the memory texts
      void setMemories(const QStringList& eight);
      void stop();                                   // abort and clear
      void setKeyHeld(bool down);                    // the Key hand key, 3 s watchdog
      int queuedCharacters() const;                  // CAT KY/ZZKY buffer answers
  signals:
      void characterStarted(int index);              // for the applet's sent/current text
      void sendingChanged(bool sending);
      void emptied();                                // TCI cw_macros_empty
      void refused(const QString& words);
  };
  ```

  Element time `1200 / wpm` ms (`cwx.cs:404-407`); every element boundary is scheduled
  from the message's start time (no drift); the character map, prosigns
  (`TCIServer.cs:8659-8676`), `"` repeat and `>` / `<` speed marks as Thetis.

**Acceptance:**
- With the injected clock and sleeper: "E" at 20 WPM keys 60 ms down, then the
  inter-character gap; "PARIS " at 20 WPM takes 50 elements of 60 ms (3000 ms) with
  each key edge on the schedule exactly; `>` raises the next segment by 5 WPM.
- Repeat: a memory ending in `"` repeats after the repeat delay; the Rpt switch repeats
  without the marker; `stop()` mid-element releases the key at once and clears the
  queue.
- Key: `setKeyHeld(true)` keys; with no release, the key drops 3 s after the last
  press; `setKeyHeld(false)` releases at once.
- HL2: cwxPtt is set before the first key-down of a message and cleared after the last
  element plus the hang; non-HL2 never sets it.
- Refusals: outside CW, or with the gate false, `sendText` returns false and emits
  `refused` with the gate's words; the gate turning false mid-message aborts.
- One real-time test on the thread: "TEST" at 30 WPM; each key edge within 2 ms of its
  schedule at load under 12 (record the load in the test log; a miss is a finding, not
  a retry).
- `ThreadRole::CwxSender` exists and `overview.md` lists it.

**Verification:** High risk (keyer timing on a new thread). Test-first on the scheduled
edges with the injected clock. Unit tests; the on-air shape is V-HW-1.

**Execution note (advisory):** opus. Keying timing, new thread (architecture flag):
earlier review candidate. After Task 4.

- [ ] **Step 1:** Port the character map and element scheduling with the clock seam.
- [ ] **Step 2:** The thread, key line writes, HL2 CWX PTT, Key watchdog.
- [ ] **Step 3:** RadioModel ownership, aborts, the request gate; overview.md;
      provenance; commit.

## Task 9: CAT and TCI CW commands

**Requirements:** R-CW-02 (`ZZCL`), R-CW-34, R-CW-35, R-CW-51, R-CW-52 (TCI keyer
watchdog cite `TCIServer.cs:8233, 9066-9093, 8287`).

**Files:**
- Modify: `src/core/cat/CatGlobalCommands.cpp` (`KY`, `KS`, `ZZKY`, `ZZKS`, `ZZKM`,
  `ZZKO`, `ZZSS`, `ZZCS`, `ZZCL`, `ZZCB`, `ZZQK`, `ZZCD`, `ZZCI`; `ZZCM`, `ZZCF` stay
  `?;`)
- Modify: `src/core/TciProtocol.cpp`, `.h` (the R-CW-35 commands, `cw_macros_empty`,
  init burst values)
- Modify: `tests/data/cat/compatibility.csv`, `tests/data/cat/requests.json`
- Test: `tests/tst_cat_tx_commands.cpp` (or a new `tests/tst_cat_cw_commands.cpp`),
  `tests/tst_tci_cw.cpp` (new), `tests/tst_tci_init_burst_live_state.cpp`

**Interfaces:**
- Consumes: `CwxSender` (Task 8), `TransmitModel` settings (Task 1),
  `CwKeyerController::qskAvailable` (Task 4), the `CwKeyLine` for TCI `keyer` (Task 3),
  MOX for TCI `keyer` in Semi (Task 5).
- Produces: nothing new.

**Acceptance:**
- `ZZKY` set "CQ TEST;" queues through the sender; read answers `0`, `1` or `2` per
  `CATCommands.cs:3581-3645`; `KY` read never changes the mode; `KY` set in USB on 40 m
  switches to CWL first, on 20 m to CWU (D13 note in the code).
- `ZZKS020;` sets CWX speed 20; `ZZKS099;` clamps to 60; `ZZKM3;` sends memory 3;
  `ZZKM9;` answers `?;`; `ZZSS;` stops; `ZZCS025;` sets the keyer speed; `ZZCL0700;`
  sets the pitch 700 and reads back `ZZCL0700;`; `ZZQK1;` answers `?;` where QSK is
  unavailable; `ZZCD1500;` clamps to 1000 (note about Thetis's 150 to 5000).
- TCI: `cw_macros:0,CQ TEST;` sends; `cw_macros_speed:28;` sets and reports;
  `keyer:0,true;` keys the CWX bit and, in Semi, MOX; with no release it drops 3000 ms
  later; `keyer:0,true,120;` releases after 120 ms; `cw_macros_stop;` aborts; the init
  burst carries the real keyer and macro speeds.
- Refused by the gate: each keying command answers as Thetis does for a refusal and the
  sender emits the gate's words.

**Verification:** Medium to high risk (computer keying). Unit tests with the fake
sender and connection.

**Execution note (advisory):** opus. Keying (KY, keyer): earlier review candidate.
After Task 8.

- [ ] **Step 1:** CAT commands and data files.
- [ ] **Step 2:** TCI commands, watchdog, notifications, init burst; commit.

## Task 10: Serial key input and the auto mode switch

**Requirements:** R-CW-04, R-CW-36, R-CW-37, R-CW-51.

**Files:**
- Create: `src/core/CwSerialKeyInput.h`, `.cpp` (Thetis `SDRSerialPortII.cs` header
  byte for byte, its existing NereusSDR port `CatSerialTransport` shows the form)
- Modify: `src/core/platform/ThreadPlacement.h`, `.cpp` (`ThreadRole::CwSerialKey`)
- Modify: `src/models/RadioModel.cpp` (own the reader; port conflict with CAT and CAT
  PTT; auto mode switch and return)
- Modify: `docs/attribution/THETIS-PROVENANCE.md`, `CMakeLists.txt` (under
  `HAVE_SERIALPORT` where the platform reader needs it)
- Test: `tests/tst_cw_serial_key_input.cpp` (new), `tests/tst_cw_auto_mode_switch.cpp`
  (new)

**Interfaces:**
- Consumes: `CwKeyLine` (Task 3), `MoxController` PTT source API (Task 5),
  `CwKeyerController` (Task 4), Task 1 properties.
- Produces:

  ```cpp
  class CwSerialKeyInput : public QObject {
      Q_OBJECT
  public:
      // The line source is a seam: production opens the port, tests inject edges.
      struct LineSource {
          virtual ~LineSource() = default;
          virtual bool open(const QString& port, QString* error) = 0;
          // Blocks until DSR or CTS changes or stop() is called; returns false on stop.
          virtual bool waitForChange(bool* dsr, bool* cts) = 0;
          virtual void stop() = 0;
      };
      explicit CwSerialKeyInput(std::unique_ptr<LineSource> source, QObject* parent = nullptr);
      ~CwSerialKeyInput() override;
      bool start(const QString& port, bool paddles, CwSerialLine keyLine,
                 CwSerialLine pttLine, QString* error);
      void stopReading();
      void setKeyLine(std::shared_ptr<CwKeyLine> line);
  signals:
      void pttLineChanged(bool down);     // queued to the main thread
      void paddleActivity();              // for the auto mode switch
      void failed(const QString& words);
  };
  ```

  Production line sources: Linux `TIOCMIWAIT`, Windows `WaitCommEvent`
  (`EV_DSR | EV_CTS`), macOS a 1 ms poll of `TIOCMGET` (D14).

**Acceptance:**
- Injected edges: paddles on, DSR down sets dot, CTS down sets dash; key line DSR sets
  CWX; PTT line CTS emits `pttLineChanged(true)`; with the gate false, key bits stay 0
  and PTT is refused through Task 5's report.
- A port already used by CAT or CAT PTT is refused with "That port is in use by CAT."
  (or the CAT PTT wording) and nothing reads it.
- Auto mode switch on: a dot while the TX slice is LSB switches it to CWL, USB to CWU;
  with return on, it returns after `cwAutoModeReturnMs` from the last dot or dash; a
  new dot restarts the wait; off does nothing.
- The reader thread stops and joins on `stopReading()` and destruction within 50 ms.

**Verification:** High risk (serial keying, new thread). Unit tests with the injected
line source. The real line sources are checked on the bench (V-HW-1 serial paddles).

**Execution note (advisory):** opus. Keying, new thread: earlier review candidate.
After Task 5.

- [ ] **Step 1:** Reader with the seam and the three platform sources.
- [ ] **Step 2:** Roles, gate, port conflict.
- [ ] **Step 3:** Auto mode switch and return; provenance; commit.

## Task 11: The computer sidetone and the capability flag

**Requirements:** R-CW-39, R-CW-40, R-CW-51.

**Files:**
- Create: `src/core/CwSidetone.h`, `.cpp` (ChannelMaster `sidetone.c` and `sidetone.h`
  headers byte for byte, NR0V; `sidetone.h`'s NEL (0x85) terminators converted to
  newlines with the header text unchanged)
- Modify: `src/core/AudioEngine.h`, `.cpp` (produce into `kTxMonitorSlotId` from
  `drainMixes` while running; an atomic key and run state)
- Modify: `src/models/RadioModel.cpp` (run, volume, pitch and key wiring: CWX sender,
  TCI key and serial straight-key line key it; the paddle never does)
- Modify: `src/core/BoardCapabilities.cpp` (`hasSidetoneGenerator` per R-CW-40 with the
  cite fixed)
- Modify: `docs/attribution/THETIS-PROVENANCE.md`, `CMakeLists.txt`
- Test: `tests/tst_cw_sidetone.cpp` (new), `tests/tst_board_capability_flag_wiring.cpp`,
  `tests/tst_remote_tx_monitor.cpp`

**Interfaces:**
- Consumes: `CwKeyLine` (Task 3) only as a writer's partner: the sidetone has its own
  atomic key, set by the same sources; Task 1 properties; `monitorVolume`.
- Produces:

  ```cpp
  class CwSidetone {   // one per engine; no Qt
  public:
      void configure(int sampleRate, int blockSize);
      void setPitchHz(double hz); void setVolume(double v); void setRun(bool on);
      void setEdge(int edgeType, double edgeSeconds);  // 0 raised cosine, 0.005 s
      void setKey(bool down);                           // atomic, any thread
      // Fills interleaved stereo float frames; returns false when not running.
      bool render(float* out, int frames);
  };
  ```

**Acceptance:**
- At 48 kHz, pitch 600, volume 0.5: key down gives a 600 Hz tone (FFT peak within one
  bin) reaching 0.5 after a 5 ms raised-cosine rise; key up falls in 5 ms to exact zero;
  not running renders false and the slot gets nothing.
- `AudioEngine`: with CW, `cwSidetone` and `cwComputerSidetone` on, a keyed sidetone
  reaches the monitor slot at the MON volume; with either switch off, silence; a remote
  owner mix receives it as MON does (`tst_remote_tx_monitor`).
- Paddle dot and dash in `radioKeyStatus` never key it.
- `hasSidetoneGenerator`: false for both HL2 rows, true for every other board row
  whose Thetis path sends the level; `tst_board_capability_flag_wiring` updated.

**Verification:** Medium risk (audio, not keying). Unit tests above; the ear check is
V-HW-1.

**Execution note (advisory):** opus. No keying. After Task 8.

- [ ] **Step 1:** Port `sidetone.c` into `CwSidetone` with its tests.
- [ ] **Step 2:** AudioEngine producer and RadioModel wiring.
- [ ] **Step 3:** Capability flag; provenance; commit.

## Task 12: Desktop CW page, TX applet in CW, Setup > DSP > CW

**Requirements:** R-CW-41, R-CW-42, R-CW-43, R-CW-47.

**Files:**
- Modify: `src/gui/applets/PhoneCwApplet.h`, `.cpp` (`buildCwPage` as the mockup;
  AetherSDR `PhoneCwApplet.cpp:440-724` layout constants `[@1e0718ad]`)
- Modify: `src/gui/applets/TxApplet.cpp`, `.h` (CW gate layer with its own saved keys;
  MON shows `cwSidetone` in CW; MOX tooltip)
- Modify: `src/gui/setup/DspSetupPages.cpp`, `.h` (Keyer, Break-in, Sidetone, Key input,
  Options groups as R-CW-43; placeholders removed; bound with `nereusSetupId`s)
- Modify: `src/gui/MainWindow.cpp` (wiring to the controller and models)
- Test: `tests/tst_controls_connected.cpp`, `tests/tst_setup_controls_connected.cpp`,
  `tests/tst_tx_applet_mon.cpp`, `tests/tst_tx_applet_lev_eq_cfc.cpp`,
  `tests/tst_tx_applet_vox_toggle.cpp`, `tests/tst_receive_only.cpp`,
  `tests/tst_operator_wording_sweep.cpp`, `tests/tst_cw_page.cpp` (new)

**Interfaces:**
- Consumes: Task 1 properties and pitch, `CwKeyerController` (state, refusal,
  availability reasons), `radioKeyStatus` for the KEY lamp, MOX for the TX lamp.
- Produces: object names for the UI gate and tests: `cwDelaySlider`, `cwSpeedSlider`,
  `cwSidetoneButton`, `cwSidetoneSlider`, `cwPanSlider`, `cwBreakInButton`,
  `cwIambicButton`, `cwPitchValue`, `cwKeyLamp`, `cwTxLamp`, `cwStatusLine`; Setup ids
  under `dsp.cw.keyer.*`, `dsp.cw.breakIn.*`, `dsp.cw.sidetone.*`, `dsp.cw.serial.*`,
  `dsp.cw.options.*`.

**Acceptance:**
- CW page: every control writes its model property and follows model changes under
  `m_updatingFromModel`; Break-in steps Off, Semi, QSK and skips an unavailable QSK with
  its reason in the tooltip and status line; Delay greys in Off and QSK with the reason;
  Pitch arrows step 10 Hz within 200..2250; the KEY lamp follows dot, dash or key-out;
  the status line shows ready, off with the reason, or the refusal words.
- TX applet in CW: VOX, LEV, EQ, CFC and TX BW greyed with "Not used in CW: the radio
  makes the signal." in tooltip and `accessibleDescription`; leaving CW restores each
  control's prior state, also when the receive-only layer was applied in between; MON
  and Mon Vol show and set `cwSidetone` and `monitorVolume`.
- Setup: ranges and defaults as R-CW-43 and R-CW-01; no "Bug", "Letter Spacing" or
  "Dot-Dash Ratio"; the hang readout follows delay and key-up; the key-down note shows
  the speed clamp; the HL2 notes appear on HL2.
- Every new string passes the wording sweep.

**Verification:** Medium risk (UI). GUI unit tests above, offscreen. UI gate captures
come in Task 13.

**Execution note (advisory):** opus. No keying. After Tasks 4, 5 and 11.

- [ ] **Step 1:** CW page.
- [ ] **Step 2:** TX applet CW layer.
- [ ] **Step 3:** Setup > DSP > CW; commit.

## Task 13: CWX applet, passband drawing, the flag, and turning CW on

**Requirements:** R-CW-24 (desktop drawers), R-CW-44, R-CW-45, R-CW-46, and the spec's
UI checks.

**Files:**
- Modify: `src/gui/applets/CwxApplet.h`, `.cpp` (wired; Stop, Key; sent and current
  characters; right-click edit; NYI overlays removed)
- Modify: `src/gui/MainWindow.cpp` (construct the CWX applet; Tools > CWX; status-bar
  CWX label)
- Modify: `src/gui/SpectrumWidget.h`, `.cpp` (every passband drawer and the filter-edge
  grab and drag use `passbandOriginHz()`; the VFO line and TX marker stay on the dial)
- Modify: `src/core/UnbuiltFeatureList.cpp` (`Cwx` built) and every gate site in
  "What already exists"
- Modify: `tests/visual/PreviewScenes.cpp` (cw-digital without the "not built" plate;
  new HL2 and QSK-unavailable poses)
- Test: `tests/tst_unbuilt_features.cpp`, `tests/tst_station_tools.cpp`,
  `tests/tst_remote_gui_gating.cpp`, `tests/tst_pan_flag_positions.cpp`,
  `tests/tst_workspace_slider_handles.cpp`, `tests/tst_cwx_applet.cpp` (new),
  `tests/tst_passband_origin_drawing.cpp` (new)

**Interfaces:**
- Consumes: `CwxSender` (Task 8), `passbandOriginHz` (Task 6), Task 12 pages.
- Produces: nothing new.

**Acceptance:**
- CWX applet: Send queues the text; Stop aborts; Key holds while pressed; M1 to M8 send
  their memory, right-click opens an editor that writes `cwxMemoryN`; Rpt and its delay
  write the settings; KB to CW sends as typed; sent characters dim and the current one
  is marked from `characterStarted`.
- Passband: CWU at 14.030 MHz with 400..900 draws from 14.029800 to 14.030300 MHz on the
  pan and waterfall; dragging the high edge to 14.030400 sets the filter high to 1000;
  USB is unchanged.
- `UnbuiltFeature::Cwx` is built; the menu, status label, CW tab, page 1, Setup groups
  and station tool `cwx` show; the tests that expected them hidden now expect them shown.
- UI gate: captures of the CW page (Semi, QSK unavailable, refused), TX applet in CW,
  Setup > DSP > CW (scrolled or min variants if it outgrows the dialog), CWX applet,
  the pan and flag in CWU, and an HL2 pose, in each theme, run per `tests/visual/README.md`
  with `--jobs 4`, compared by the controller against the mockup.

**Verification:** Medium risk (UI). GUI tests above; UI gate captures per
`ui-verification`, read by the controller, with any changed baseline listed in the
report for the controller to accept, never accepted automatically.

**Execution note (advisory):** opus. No keying (the CWX applet calls the sender). After
Task 12.

- [ ] **Step 1:** CWX applet wiring.
- [ ] **Step 2:** Passband drawers.
- [ ] **Step 3:** Cwx built, gates and tests; preview scenes; captures; commit.

## Task 14: Remote CW verbs and the remote window

**Requirements:** R-CW-48, R-CW-49.

**Files:**
- Modify: `src/core/session/StationCapabilities.h`, `.cpp` (`remoteCwVersion` 1, sent
  after `remoteTxVersion`)
- Modify: `src/core/session/SessionCommandDispatcher.cpp` (`cw.send`, `cw.memory`,
  `cw.stop`, `cw.key`), `src/core/session/RemoteTransmitClient.{h,cpp}` (client calls),
  `src/core/session/StationServer.cpp` (Core side, keying gate, 3 s watchdog for
  `cw.key`)
- Modify: `tests/data/link/v1/surface.json` (new verbs and capability, as the link
  conformance rules require)
- Modify: the remote window wiring so the CW page, Setup page and CWX applet bind to the
  mirror and send the verbs
- Test: `tests/tst_remote_cw.cpp` (new), `tests/tst_remote_window_transmit.cpp`,
  `tests/tst_remote_gui_gating.cpp`

**Interfaces:**
- Consumes: `CwxSender` (Task 8), the keying gate, Tasks 12 and 13 widgets.
- Produces: verbs `cw.send {text}`, `cw.memory {number}`, `cw.stop {}`,
  `cw.key {down: bool}`; replies carry `TxRefusal` on refusal; capability
  `remoteCwVersion = 1`.

**Acceptance:**
- A remote window on a test Core sends "CQ" and the Core's sender queues it; a window
  that does not hold transmit is refused with the holder words; `cw.key down` with no
  `up` releases on the Core after 3 s; a peer without `remoteCwVersion` sees the CWX
  controls disabled with the update reason.
- The remote window's CW page and Setup page round-trip every setting through the
  mirror.
- Older peers keep their exact bytes: the link conformance fixtures change only by the
  new verbs and capability.

**Verification:** Medium to high risk (remote keying). Integration tests through a test
Core.

**Execution note (advisory):** opus. Keying from remote devices runs through Task 8's
sender and the keying gate. After Task 13.

- [ ] **Step 1:** Capability and verbs on both sides.
- [ ] **Step 2:** Remote window binding; commit.

## Task 15: The phone

**Requirements:** R-CW-24 (phone drawers), R-CW-50.

**Files:**
- Modify: `ios/NereusApp/Main/TransmitModel.swift`, `Main/TxPanel.swift`,
  `Modes/TransmitSection.swift` (speed, pitch, break-in mode and delay, sidetone level
  in CW, behind `transmitSettingsVersion` 16)
- Create: a CWX page under `ios/NereusApp/Tools/` (text, send, stop, memories) and map
  the `cwx` tool to it in `App/ToolsTab.swift`, behind `remoteCwVersion` 1
- Modify: NereusKit `NereusBand/BandSlice.swift`, `Band/BandSlicesModel.swift`,
  `Band/ForeignSlicesModel.swift` (passband origin from mode and `CWPitch`)
- Modify: `tests/tst_setup_description_parity.cpp` (a named desktop-only list for the
  new `dsp.cw.*` ids; `resources/setup/dsp.json` unchanged)
- Test: `ios/NereusApp/Tests/ToolsPagesTests.swift`, `TransmitScreenTests.swift`,
  `ModesTabBindingTests.swift`, NereusKit band tests

**Interfaces:**
- Consumes: the mirrored `transmit` properties (Task 1), the `cw.*` verbs (Task 14).
- Produces: nothing new.

**Acceptance:**
- In CWU the phone's transmit panel shows speed, pitch, break-in and delay, and level,
  each writing its property; against a version 15 Core they are disabled with the
  update note.
- Tools shows CWX as a working page when offered; send, stop and memories call the
  verbs; against a Core without `remoteCwVersion` the page says why it is disabled.
- The phone's pan draws the CWU passband around the dial (pitch 600, 400..900: dial
  minus 200 to dial plus 300).
- The described Setup page DSP > CW is unchanged on the phone.

**Verification:** Medium risk. Swift tests:
`ios/scripts/swift-test.sh --jobs 2 --filter <Suite>`; app tests:
`ios/scripts/generate-project.sh` then
`xcodebuild -project ios/NereusSDR.xcodeproj -scheme NereusSDR -destination 'platform=iOS Simulator,name=iPhone 17' -collect-test-diagnostics never -parallel-testing-enabled NO -jobs 2 test`,
both through the load gate.

**Execution note (advisory):** opus. No keying on the phone (it sends text and verbs).
After Task 14.

- [ ] **Step 1:** Transmit panel CW controls.
- [ ] **Step 2:** CWX tools page.
- [ ] **Step 3:** Passband origin; parity list; commit.

## Task 16: Bench (hardware evidence)

**Requirements:** the spec's V-HW-1 to V-HW-7.

**Files:** none in the repository. The evidence goes in the PR body and the ledger.

**Interfaces:**
- Consumes: the finished branch, built on JJ's Mac by the controller (never his running
  app, never his settings: a scratch profile).
- Produces: a recorded pass or fail per item.

**Acceptance (pending until observed, JJ at the radio into a dummy load):**
- V-HW-1, ANAN-G2 (P2): paddle and straight key in Off and Semi, CWX, CAT `KY`, TCI
  `keyer`, serial paddles; both sidetones heard; a second receiver puts the carrier on
  the dial within 10 Hz.
- V-HW-2, ANAN-G2 QSK: receive heard between dots at 20 WPM; AGC and attenuation back
  after QSK off.
- V-HW-3, a P1 radio: as V-HW-1, plus whether Off keys from the jack without MOX (D10);
  the answer goes in the spec as a note.
- V-HW-4: a paddle held while tuning past the band edge stops RF at the edge; transmit
  inhibit and a pulled network cable stop RF.
- V-HW-5: key-down and key-up delays measured at PTT out and RF (scope) for 7 and 30 ms
  settings.
- V-HW-6, Hermes Lite 2: straight key, CWX with the CWX PTT hold; whether it has a
  paddle keyer and a sidetone (sets R-CW-40's HL2 rows if they do).
- V-HW-7: a remote window and the phone through a Core send CWX; a held Key releases
  when the link drops.

**Verification:** Hardware. The controller schedules it with JJ; nothing here is
claimed until observed.

**Execution note (advisory):** controller with JJ. Keys the radio, by JJ. After Task 15
and the whole-branch review.

- [ ] **Step 1:** Run each item with JJ and record the result.

## Final checks

Once, on the finished branch, through the load gate: build `all_tests`, run the full
`ctest --test-dir build --output-on-failure -j 8` and show the summary line; the
pre-commit verifiers (`scripts/verify-thetis-headers.py`,
`scripts/verify-inline-tag-preservation.py`, `scripts/check-new-ports.py`,
`scripts/verify-inline-cites.py`); the UI gate captures from Task 13; the Swift and app
tests from Task 15.
