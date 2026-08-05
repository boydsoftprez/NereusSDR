# ANAN-G2E (HermesC10) Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Source-first directive (CLAUDE.md, NereusSDR):** Every Thetis-cited port carries `[v2.10.3.15]` version stamp. Every `//N1GP G2E added` (or `//N1GP G2E added (HermesC10)` / `//N1GP G2E added //DK1HLM`) inline tag MUST be preserved verbatim in the C++ translation per the inline-comment preservation rule. The pre-commit hook `verify-inline-tag-preservation.py` enforces this mechanically. Use `git -C ../Thetis describe --tags` (currently `v2.10.3.15`) when stamping new cites.

**Goal:** Port Apache Labs ANAN-G2E (HermesC10 board, formerly G1) support from Thetis v2.10.3.15 by Rick N1GP into NereusSDR as a complete Thetis-parity port — every G2E touchpoint absorbed, no deferrals.

**Architecture:** Single-ADC HERMES-architecture RX silicon + OrionMKII-class TX/PA/exciter + MKII BPF filter bank. New `HPSDRHW::HermesC10=20` (relocating NereusSDR-native `Andromeda` to 21) and `HPSDRModel::ANAN_G2E=16`. Touches enum layer, BoardCapabilities (incl. new HasVolts/HasAmps/AutoPACalibrate/BypassPaSettings fields), HardwareProfile per-board init, codec wire-byte emission (closes pre-existing ADC supply + LR audio swap gaps), SkuUiProfile (new EXT1/EXT2 label override fields), PaGainProfile, PaTelemetryScaling, plus 30 console.cs branches scattered across codec/RadioConnection/UI files.

**Tech Stack:** C++20, Qt6, CMake, ctest. Repo root `/Users/j.j.boyd/NereusSDR`. Thetis source at `/Users/j.j.boyd/Thetis` (currently `v2.10.3.15`). Design spec: [2026-05-21-anan-g2e-port-design.md](2026-05-21-anan-g2e-port-design.md). All commits GPG-signed (never `--no-gpg-sign`).

**Reference open: keep these files in another pane while implementing —**
- `../Thetis/Project Files/Source/ChannelMaster/network.h:425, 446` — enum byte values
- `../Thetis/Project Files/Source/Console/clsHardwareSpecific.cs:85-264, 699-803` — init + properties + PA gains
- `../Thetis/Project Files/Source/Console/setup.cs:19904-19929` — full G2E PA UI block
- `../Thetis/Project Files/Source/Console/console.cs` — search `G2E\|HermesC10` for all 32 branches

---

## File-Touch Inventory (decomposition map)

Files this plan creates or modifies, by responsibility:

| File | Responsibility | Phase |
|---|---|---|
| `src/core/HpsdrModel.h` | Enum slots, board-model mapping, display names | A |
| `src/core/BoardCapabilities.h` | Struct definition (+ 4 new fields) | A |
| `src/core/BoardCapabilities.cpp` | kHermesC10 row + apply new fields to existing 12 rows | A |
| `src/core/RadioDiscovery.cpp` | P1 byte 20 → HermesC10 case | A |
| `src/core/HardwareProfile.cpp` + `.h` | ANAN_G2E init case + verify existing per-board values | B |
| `src/core/codec/CodecContext.h` | adcSupplyVoltage + lrAudioSwap fields | B |
| `src/core/codec/P1CodecStandard.cpp` | Emit LR swap wire byte | B |
| `src/core/codec/P2CodecOrionMkII.cpp` | Emit ADC supply wire byte | B |
| `src/core/P1RadioConnection.cpp` | buildCodecContext populate new fields | B |
| `src/core/P2RadioConnection.cpp` | buildCodecContext populate new fields + RX1 att HermesC10 case + VFO split MOX HermesC10 case | B, E |
| `src/core/SkuUiProfile.h` + `.cpp` | ext1OutOnTxLabel/ext2OutOnTxLabel fields + ANAN_G2E case | C |
| `src/gui/setup/hardware/AntennaAlexAntennaControlTab.cpp` | Consume new label fields | C |
| `src/core/PaGainProfile.cpp` | ANAN_G2E case in kAnan7000dRow group | D |
| `src/core/PaTelemetryScaling.cpp` + `.h` | ANAN_G2E fwd/rev triplets | D |
| `src/gui/setup/PaSetupPages.cpp` + `.h` | chkAutoPACalibrate visibility + new chkBypassANANPASettings UI + ATT-on-TX visibility + PaValues volts/amps gating | D |
| `src/models/TransmitModel.cpp` + `.h` | paSettingsBypass Q_PROPERTY | D |
| `src/core/codec/P1CodecStandard.cpp` (again) | HermesC10 in RX1 attenuator switch + RX1 output path + setAlex1HPF equivalent | E |
| `src/core/codec/AlexFilterMap.cpp` (or wherever BPF1 algorithm dispatches) | HermesC10 to OrionMKII+Saturn group for BPF1 | E |
| `src/gui/widgets/VfoWidget.cpp` (or wherever VFO split MOX renders) | HermesC10 case for VFO sub-display during split | E |
| `src/core/RadioModel.cpp` | ANAN_G2E TX exciter formula path + audio mix states + P1 user I/O inhibit bit | F |
| `src/core/codec/P1CodecStandard.cpp` (again) | ANAN_G2E preamp combo items via preampItemsForBoard | F |
| `docs/attribution/THETIS-PROVENANCE.md` | Bump cite versions to `[v2.10.3.15]` on touched files | G |
| `CLAUDE.md` | Update Thetis-version reference | G |
| `docs/architecture/anan-g2e-verification/README.md` (NEW) | 12-row bench matrix | G |
| `tests/tst_board_capabilities.cpp` | Pins + kHermesC10 assertions + HasVolts/HasAmps coverage | each phase |
| `tests/tst_hardware_profile.cpp` (new or existing) | Per-board init verification table | B |
| `tests/tst_codec_wire_bytes.cpp` (new) | LR swap + ADC supply emission | B |
| `tests/tst_sku_ui_profile.cpp` | EXT label override + G2E case | C |
| `tests/tst_pa_gain_profile.cpp` | ANAN_G2E case | D |
| `tests/tst_pa_telemetry_scaling.cpp` (new or existing) | ANAN_G2E fwd/rev triplets | D |

---

## Phase A — Foundation: Enums + Capability Row Shape

Goal of Phase A: any file in the codebase can refer to `HPSDRHW::HermesC10`, `HPSDRModel::ANAN_G2E`, and the new `kHermesC10` capability row without compile errors. Andromeda has been relocated to slot 21.

### Task A1: HpsdrModel.h — enum slots + mappings

**Files:**
- Modify: `src/core/HpsdrModel.h` (multiple edits)
- Test: `tests/tst_board_capabilities.cpp` (extend existing)

**Source cite to read before implementing:**
- `../Thetis/Project Files/Source/ChannelMaster/network.h:425` — `HermesC10 = 20`
- `../Thetis/Project Files/Source/ChannelMaster/network.h:446` — `HPSDRModel_ANAN_G2E = 16 //N1GP G2E added`

**STOP-AND-ASK rule:** if Thetis cites disagree with what's here, ASK before deviating.

- [ ] **Step 1: Write failing test for enum values**

Open `tests/tst_board_capabilities.cpp` and add:

```cpp
// Phase A1 — pin G2E enum values
static_assert(static_cast<int>(HPSDRHW::HermesC10) == 20,
              "HermesC10 must be 20 per Thetis network.h:425 [v2.10.3.15]");
static_assert(static_cast<int>(HPSDRHW::Andromeda) == 21,
              "Andromeda relocated to 21 to free Thetis byte 20 for HermesC10");
static_assert(static_cast<int>(HPSDRModel::ANAN_G2E) == 16,
              "ANAN_G2E must be 16 per Thetis network.h:446 [v2.10.3.15]");
static_assert(static_cast<int>(HPSDRModel::LAST) == 17,
              "LAST sentinel bumped from 16 to 17 when ANAN_G2E added");
```

- [ ] **Step 2: Run test, confirm compile failure**

Run: `cmake --build build -j$(nproc) --target tst_board_capabilities 2>&1 | head -20`
Expected: failure — `HermesC10`, `ANAN_G2E` are not enum members.

- [ ] **Step 3: Edit HpsdrModel.h — relocate Andromeda + add HermesC10**

In `src/core/HpsdrModel.h`, find the HPSDRHW enum (around line 117-134). Change:

```cpp
    Andromeda = 20,  // NereusSDR-native; reserved for future Apache Labs Andromeda console hardware
```

to:

```cpp
    // From Thetis network.h:425 [v2.10.3.15] //N1GP G2E added (HermesC10)
    HermesC10 = 20,
    // NereusSDR-native; relocated from 20 to 21 on 2026-05-21 (G2E port) to free Thetis byte 20.
    // See docs/architecture/2026-05-21-anan-g2e-port-design.md §4 for rationale.
    Andromeda = 21,
```

In the HPSDRModel enum (around line 105-120, before `LAST`), add:

```cpp
    // From Thetis network.h:446 [v2.10.3.15] //N1GP G2E added
    ANAN_G2E = 16,
    LAST = 17,  // was 16; bumped for ANAN_G2E
```

Locate the `boardForModel()` switch and add (preserving alphabetical/source order):

```cpp
    case HPSDRModel::ANAN_G2E:     return HPSDRHW::HermesC10;  // //N1GP G2E added
```

Locate the `displayName()` switch and add:

```cpp
    case HPSDRModel::ANAN_G2E:     return "ANAN-G2E";  // From Thetis setup.designer.cs:8572 [v2.10.3.15]
```

Locate the `boardCodeName()` switch and add:

```cpp
    case HPSDRHW::HermesC10:       return "HermesC10";  // //N1GP G2E added
```

- [ ] **Step 4: Run test, confirm pass**

Run: `cmake --build build -j$(nproc) --target tst_board_capabilities && ctest --test-dir build -R tst_board_capabilities -V 2>&1 | tail -20`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core/HpsdrModel.h tests/tst_board_capabilities.cpp
git commit -m "feat(boards): add HermesC10 + ANAN_G2E enum slots; relocate Andromeda to 21

From Thetis network.h:425 (HermesC10=20) and network.h:446
(HPSDRModel_ANAN_G2E=16) [v2.10.3.15] //N1GP G2E added.

NereusSDR-native Andromeda slot moved from 20 to 21 to free
Thetis byte 20 for HermesC10. Andromeda has zero wire-discovery /
persistence touchpoints today (only declarative in BoardCapabilities,
canDriveGanymede safety net, and test rows), so the move is
namespace-only.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task A2: BoardCapabilities.h — 4 new struct fields

**Files:**
- Modify: `src/core/BoardCapabilities.h` (struct definition)

**Source cites to read:**
- `../Thetis/Project Files/Source/Console/clsHardwareSpecific.cs:245-254` — HasVolts
- `../Thetis/Project Files/Source/Console/clsHardwareSpecific.cs:255-264` — HasAmps
- `../Thetis/Project Files/Source/Console/setup.cs:19918` — chkAutoPACalibrate.Visible=false (G2E)
- `../Thetis/Project Files/Source/Console/setup.cs:19920` — chkBypassANANPASettings.Visible=true (G2E)

- [ ] **Step 1: Open `src/core/BoardCapabilities.h` and locate the struct definition.**

It's around line 200-300 (the prior agent inventory said line 221-421).

- [ ] **Step 2: Add four new fields**

After the existing `canDriveGanymede` line (or in a coherent grouping next to PA-related flags), add:

```cpp
    // From Thetis HasVolts (clsHardwareSpecific.cs:245-254 [v2.10.3.15]).
    // True for boards with on-board PA voltage telemetry sensor.
    bool hasPaVoltsTelemetry = false;

    // From Thetis HasAmps (clsHardwareSpecific.cs:255-264 [v2.10.3.15]).
    // True for boards with on-board PA current telemetry sensor.
    bool hasPaAmpsTelemetry = false;

    // From Thetis setup.cs:19918 [v2.10.3.15] //N1GP G2E added —
    // chkAutoPACalibrate.Visible=false for G2E (auto-cal UI hidden).
    // Defaults true so existing boards retain auto-cal visibility.
    bool allowsAutoPaCalibrate = true;

    // From Thetis setup.cs:19920 [v2.10.3.15] //N1GP G2E added —
    // chkBypassANANPASettings.Visible=true for G2E ("Bypass ANAN PA Settings"
    // checkbox visible). Defaults false; G2E (and any other Thetis SKU with
    // chkBypassANANPASettings.Visible=true) opts in.
    bool showsBypassPaSettingsUi = false;
```

- [ ] **Step 3: Compile the codebase to confirm no breakage**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -20`
Expected: clean build (struct field additions with defaults shouldn't break callers).

- [ ] **Step 4: Commit (struct field addition only; row population is Task A3)**

```bash
git add src/core/BoardCapabilities.h
git commit -m "feat(caps): add HasVolts/HasAmps/AutoPACalibrate/BypassPaSettings fields

Four new BoardCapabilities fields per Thetis v2.10.3.15:
- hasPaVoltsTelemetry (clsHardwareSpecific.cs:245-254)
- hasPaAmpsTelemetry  (clsHardwareSpecific.cs:255-264)
- allowsAutoPaCalibrate / showsBypassPaSettingsUi  (setup.cs:19918-19920)

All default-initialized so existing rows compile unchanged. Per-row
values populated in Task A3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task A3: BoardCapabilities.cpp — kHermesC10 row + apply 4 new fields to 12 existing rows

**Files:**
- Modify: `src/core/BoardCapabilities.cpp` (every row + add new row + extend kTable)
- Test: `tests/tst_board_capabilities.cpp`

**Source cites:**
- §6.2.2 of design doc (full kHermesC10 row text)
- `../Thetis/Project Files/Source/Console/clsHardwareSpecific.cs:245-264` for HasVolts/HasAmps SKU grouping

**HasVolts / HasAmps SKU mapping (both fields same set, per Thetis):**
- **true:** ANAN7000D, ANAN8000D, ANVELINAPRO3, ANAN_G2, ANAN_G2_1K, REDPITAYA, **+ new ANAN_G2E**
- **false:** all others

In NereusSDR rows, this maps to:
- `kOrionMKII` (ANAN-7000DLE/8000DLE) → **true**
- `kSaturn` (ANAN-G2/G2_1K) → **true**
- `kHermesC10` (G2E) → **true** (new row)
- All other existing rows (`kAtlas`, `kHermes`, `kHermesII`, `kAngelia`, `kOrion`, `kHermesLite`, `kHermesLiteRxOnly`, `kSaturnMKII`, `kAndromeda`, `kUnknown`) → **false** (default, no change needed unless explicitly set)

**allowsAutoPaCalibrate SKU mapping:** default `true`; set **false** explicitly for `kHermesC10` only.

**showsBypassPaSettingsUi SKU mapping:** default `false`; set **true** explicitly for `kHermesC10` only.

- [ ] **Step 1: Write failing tests for HermesC10 row + field values**

Extend `tests/tst_board_capabilities.cpp`:

```cpp
void TestBoardCapabilities::hermesC10Row()
{
    using namespace NereusSDR;
    const auto& caps = BoardCapabilities::forBoard(HPSDRHW::HermesC10);

    // §6.2.2 of design doc — kHermesC10 row
    QCOMPARE(caps.board, HPSDRHW::HermesC10);
    QCOMPARE(caps.protocol, ProtocolVersion::Protocol1);
    QCOMPARE(caps.adcCount, 1);
    QCOMPARE(caps.maxReceivers, 4);
    QCOMPARE(caps.maxSampleRate, 192000);
    QVERIFY(caps.hasAlex2);
    QVERIFY(caps.hasPureSignal);
    QCOMPARE(caps.psDefaultPeak, 0.2899);
    QCOMPARE(caps.psSampleRate, 192000);
    QVERIFY(!caps.hasDiversityReceiver);  // 1 ADC, no diversity
    QVERIFY(!caps.canDriveGanymede);
    QVERIFY(caps.preamp.present);
    QVERIFY(!caps.preamp.hasBypassAndPreamp);  // RX2 preamp absent — preamp[1]=false
    QCOMPARE(caps.attenuator.maxDb, 31);  // NOT 61 (Hermes-class)
    QCOMPARE(caps.attenuator.minDb, 0);
    QVERIFY(caps.attenuator.present);

    // New fields per Task A2
    QVERIFY(caps.hasPaVoltsTelemetry);
    QVERIFY(caps.hasPaAmpsTelemetry);
    QVERIFY(!caps.allowsAutoPaCalibrate);   // hidden for G2E
    QVERIFY(caps.showsBypassPaSettingsUi);  // visible for G2E

    QCOMPARE(QString(caps.displayName), QString("ANAN-G2E"));
}

void TestBoardCapabilities::hasVoltsAmps_perSkuGrouping()
{
    using namespace NereusSDR;
    // From Thetis clsHardwareSpecific.cs:245-264 [v2.10.3.15]
    // HasVolts/HasAmps true SKUs (NereusSDR mapping)
    QVERIFY(BoardCapabilities::forBoard(HPSDRHW::OrionMKII).hasPaVoltsTelemetry);
    QVERIFY(BoardCapabilities::forBoard(HPSDRHW::OrionMKII).hasPaAmpsTelemetry);
    QVERIFY(BoardCapabilities::forBoard(HPSDRHW::Saturn).hasPaVoltsTelemetry);
    QVERIFY(BoardCapabilities::forBoard(HPSDRHW::Saturn).hasPaAmpsTelemetry);
    QVERIFY(BoardCapabilities::forBoard(HPSDRHW::HermesC10).hasPaVoltsTelemetry);
    QVERIFY(BoardCapabilities::forBoard(HPSDRHW::HermesC10).hasPaAmpsTelemetry);

    // false SKUs
    QVERIFY(!BoardCapabilities::forBoard(HPSDRHW::Atlas).hasPaVoltsTelemetry);
    QVERIFY(!BoardCapabilities::forBoard(HPSDRHW::Hermes).hasPaVoltsTelemetry);
    QVERIFY(!BoardCapabilities::forBoard(HPSDRHW::HermesII).hasPaVoltsTelemetry);
    QVERIFY(!BoardCapabilities::forBoard(HPSDRHW::Angelia).hasPaVoltsTelemetry);
    QVERIFY(!BoardCapabilities::forBoard(HPSDRHW::Orion).hasPaVoltsTelemetry);
    QVERIFY(!BoardCapabilities::forBoard(HPSDRHW::HermesLite).hasPaVoltsTelemetry);
}
```

Add the method declarations to the test class's `private slots:` section.

- [ ] **Step 2: Run, confirm FAIL**

Run: `cmake --build build -j$(nproc) --target tst_board_capabilities 2>&1 | tail -20`
Expected: linker error or runtime failure — `kHermesC10` not in table.

- [ ] **Step 3: Add kHermesC10 row to `src/core/BoardCapabilities.cpp`**

Insert before `kAndromeda` (or near `kOrionMKII` / `kSaturn` for source-order grouping). Paste the full row from §6.2.2 of the design doc:

```cpp
// ─── HermesC10 (ANAN-G2E, formerly G1) ──────────────────────────────────────
// Source: network.h:425 (HermesC10=20) [v2.10.3.15], clsHardwareSpecific.cs:129-135 [v2.10.3.15]
//   N1GP G2E added — single-ADC entry-level G2 SKU; HERMES-class 4-DDC RX.
// Differs from ANAN_G2/G2_1K: no RX2 preamp, no RX2 stepped att, 1 ADC, no diversity.
// Shares Alex-2 (MKII BPF) routing with G2 family; PA gain table shared with G2/7000D tier.
// PA telemetry: HasVolts=true, HasAmps=true (clsHardwareSpecific.cs:245-264 [v2.10.3.15]).
const BoardCapabilities kHermesC10 = {
    .board            = HPSDRHW::HermesC10,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 1,
    .maxReceivers     = 4,
    .sampleRates      = {48000, 96000, 192000, 0, 0, 0},
    .maxSampleRate    = 192000,
    .attenuator       = {0, 31, 1, true, 0x1F, 0x20, false},
    .preamp           = {true, false},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasAlex2         = true,
    .hasRxBypassRelay = true,
    .rxOnlyAntennaCount = 3,
    .hasPureSignal    = true,
    .psDefaultPeak    = 0.2899,
    .psSampleRate     = 192000,
    .hasDiversityReceiver = false,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = false,
    .hasAlex          = true,
    .hasPennyLane     = true,
    .canDriveGanymede = false,
    .hasPaVoltsTelemetry = true,
    .hasPaAmpsTelemetry  = true,
    .allowsAutoPaCalibrate = false,
    .showsBypassPaSettingsUi = true,
    .minFirmwareVersion = 0,
    .knownGoodFirmware  = 0,
    .p2PreampPerAdc   = false,
    .displayName      = "ANAN-G2E",
    .sourceCitation   = "network.h:425, clsHardwareSpecific.cs:129-135 / 245-264 / 699-730 / 790-803, "
                        "console.cs:8388/14835/25007 [v2.10.3.15]; N1GP G2E added",
};
```

- [ ] **Step 4: Register in `kTable`**

Locate the `kTable` constexpr array. Add `kHermesC10` to it. Update array size to 13 entries.

- [ ] **Step 5: Apply hasPaVoltsTelemetry/hasPaAmpsTelemetry=true to kOrionMKII and kSaturn**

In `kOrionMKII` and `kSaturn` row definitions, add (right before `displayName`):

```cpp
    .hasPaVoltsTelemetry = true,  // HasVolts (clsHardwareSpecific.cs:251 [v2.10.3.15])
    .hasPaAmpsTelemetry  = true,  // HasAmps  (clsHardwareSpecific.cs:261 [v2.10.3.15])
```

- [ ] **Step 6: Run test, confirm pass**

Run: `cmake --build build -j$(nproc) --target tst_board_capabilities && ctest --test-dir build -R tst_board_capabilities -V 2>&1 | tail -30`
Expected: PASS for both new test methods.

- [ ] **Step 7: Commit**

```bash
git add src/core/BoardCapabilities.cpp tests/tst_board_capabilities.cpp
git commit -m "feat(caps): add kHermesC10 row + HasVolts/HasAmps for ANAN_G2/G2_1K/7000D/8000D

kHermesC10 row mirrors Thetis clsHardwareSpecific.cs:129-135 [v2.10.3.15]
(SetRxADC=1, SetMKIIBPF=1, SetADCSupply=33, LRAudioSwap=0) plus the
G2E-specific UI flags from setup.cs:19918-19920 (allowsAutoPaCalibrate=false,
showsBypassPaSettingsUi=true). HasVolts/HasAmps=true matches the SKU
grouping at clsHardwareSpecific.cs:245-264.

kOrionMKII (ANAN-7000DLE/8000DLE) and kSaturn (ANAN-G2/G2_1K) get
hasPaVoltsTelemetry/hasPaAmpsTelemetry=true to complete the Thetis
grouping. Other existing rows default to false.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task A4: RadioDiscovery.cpp — P1 byte 20 → HermesC10

**Files:**
- Modify: `src/core/RadioDiscovery.cpp` (around line 233-241)
- Test: `tests/tst_radio_discovery.cpp` (or extend tst_board_capabilities)

- [ ] **Step 1: Write failing test**

```cpp
void TestRadioDiscovery::p1Byte20_mapsToHermesC10()
{
    // From Thetis network.h:425 [v2.10.3.15] //N1GP G2E added (HermesC10)
    // P1 device-type byte at offset 10 of discovery reply: 20 → HermesC10
    QByteArray reply(64, '\0');
    reply[0] = 0xEF; reply[1] = 0xFE; reply[2] = 0x02;  // P1 reply magic
    // MAC at offset 3-8 (any value)
    reply[10] = 20;  // device type byte
    RadioInfo info = RadioDiscovery::parseP1Reply(reply);
    QCOMPARE(info.boardType, HPSDRHW::HermesC10);
}
```

- [ ] **Step 2: Run, confirm FAIL**

Run: `cmake --build build -j$(nproc) --target tst_radio_discovery && ctest --test-dir build -R p1Byte20 -V`
Expected: FAIL — byte 20 falls into default `static_cast<HPSDRHW>(boardByte)` which now would give `HermesC10` *if* enum is 20, but the test is meant to verify the explicit case lands and doesn't go through default.

(Actually the default cast now produces HermesC10 since the enum value is 20. The test still passes via default. To make the test meaningful, also assert that an explicit `case 20:` branch exists. Skip the failing-first if it already passes; document as "verified-pass".)

- [ ] **Step 3: Add explicit case to RadioDiscovery.cpp**

Locate the P1 device-type-byte switch (around line 233). Update the comment line and add the explicit case:

```cpp
    // mapP1DeviceType: 0=Atlas, 1=Hermes, 2=HermesII, 4=Angelia, 5=Orion, 6=HermesLite,
    //                  10=OrionMKII, 20=HermesC10 (ANAN-G2E)  //N1GP G2E added
    quint8 boardByte = static_cast<quint8>(bytes[10]);
    switch (boardByte) {
    case 0:  out.boardType = HPSDRHW::Atlas;      break;
    case 1:  out.boardType = HPSDRHW::Hermes;     break;
    case 2:  out.boardType = HPSDRHW::HermesII;   break;
    case 4:  out.boardType = HPSDRHW::Angelia;    break;
    case 5:  out.boardType = HPSDRHW::Orion;      break;
    case 6:  out.boardType = HPSDRHW::HermesLite; break;  // MI0BOT
    case 10: out.boardType = HPSDRHW::OrionMKII;  break;
    case 20: out.boardType = HPSDRHW::HermesC10;  break;  // From Thetis network.h:425 [v2.10.3.15] //N1GP G2E added (HermesC10)
    default: out.boardType = static_cast<HPSDRHW>(boardByte); break;
    }
```

- [ ] **Step 4: Run test, confirm pass**

Run: `ctest --test-dir build -R p1Byte20 -V`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core/RadioDiscovery.cpp tests/tst_radio_discovery.cpp
git commit -m "feat(discovery): map P1 byte 20 to HermesC10 (ANAN-G2E)

From Thetis network.h:425 [v2.10.3.15] //N1GP G2E added (HermesC10).
Explicit case rather than default fallthrough so the discovery
switch parallels the Thetis mapP1DeviceType structure.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase B — HardwareProfile + Codec Wire-Byte Emission

Goal of Phase B: HermesC10 boards get correct per-board init values populated in HardwareProfile, AND NereusSDR closes the pre-existing wire-byte emission gap for `SetADCSupply` and `LRAudioSwap` (affecting every board, not just G2E).

### Task B1: HardwareProfile — ANAN_G2E init case + verify-table all 14 existing SKUs

**Files:**
- Modify: `src/core/HardwareProfile.cpp` (per-model switch around line 87-195)
- Test: `tests/tst_hardware_profile.cpp` (create if absent, or extend existing)

**Source cite:** `../Thetis/Project Files/Source/Console/clsHardwareSpecific.cs:85-191 [v2.10.3.15]`

**Authoritative table (from design doc §6.3):**

| Board (HPSDRModel) | SetRxADC | SetMKIIBPF | SetADCSupply | LRAudioSwap | Hardware |
|---|---|---|---|---|---|
| HERMES | 1 | 0 | 33 | 1 | Hermes |
| ANAN10 | 1 | 0 | 33 | 1 | Hermes |
| ANAN10E | 1 | 0 | 33 | 1 | HermesII |
| ANAN100 | 1 | 0 | 33 | 1 | Hermes |
| ANAN100B | 1 | 0 | 33 | 1 | HermesII |
| ANAN100D | 2 | 0 | 33 | 0 | Angelia |
| **ANAN_G2E** | **1** | **1** | **33** | **0** | **HermesC10** |
| ANAN200D | 2 | 0 | 50 | 0 | Orion |
| ORIONMKII | 2 | 1 | 50 | 0 | OrionMKII |
| ANAN7000D | 2 | 1 | 50 | 0 | OrionMKII |
| ANAN8000D | 2 | 1 | 50 | 0 | OrionMKII |
| ANAN_G2 | 2 | 1 | 50 | 0 | Saturn |
| ANAN_G2_1K | 2 | 1 | 50 | 0 | Saturn |
| ANVELINAPRO3 | 2 | 1 | 50 | 0 | OrionMKII |
| REDPITAYA | 2 | 0 | 50 | 0 | OrionMKII |

- [ ] **Step 1: Read HardwareProfile.cpp to find the existing per-model switch**

```bash
grep -n "case HPSDRModel" src/core/HardwareProfile.cpp | head -20
```

Expected: switch with one case per HPSDRModel, each setting `adcCount`, `mkiiBpf`, `adcSupplyVoltage`, `lrAudioSwap`, `effectiveBoard`.

- [ ] **Step 2: Write failing test**

Create or extend `tests/tst_hardware_profile.cpp`:

```cpp
struct ExpectedInit {
    HPSDRModel model;
    int adcCount;
    bool mkiiBpf;
    int adcSupplyVoltage;
    bool lrAudioSwap;
    HPSDRHW board;
};

// From Thetis clsHardwareSpecific.cs:85-191 [v2.10.3.15]
static const ExpectedInit kThetisInit[] = {
    {HPSDRModel::HERMES,       1, false, 33, true,  HPSDRHW::Hermes},
    {HPSDRModel::ANAN10,       1, false, 33, true,  HPSDRHW::Hermes},
    {HPSDRModel::ANAN10E,      1, false, 33, true,  HPSDRHW::HermesII},
    {HPSDRModel::ANAN100,      1, false, 33, true,  HPSDRHW::Hermes},
    {HPSDRModel::ANAN100B,     1, false, 33, true,  HPSDRHW::HermesII},
    {HPSDRModel::ANAN100D,     2, false, 33, false, HPSDRHW::Angelia},
    {HPSDRModel::ANAN_G2E,     1, true,  33, false, HPSDRHW::HermesC10},  // //N1GP G2E added
    {HPSDRModel::ANAN200D,     2, false, 50, false, HPSDRHW::Orion},
    {HPSDRModel::ORIONMKII,    2, true,  50, false, HPSDRHW::OrionMKII},
    {HPSDRModel::ANAN7000D,    2, true,  50, false, HPSDRHW::OrionMKII},
    {HPSDRModel::ANAN8000D,    2, true,  50, false, HPSDRHW::OrionMKII},
    {HPSDRModel::ANAN_G2,      2, true,  50, false, HPSDRHW::Saturn},
    {HPSDRModel::ANAN_G2_1K,   2, true,  50, false, HPSDRHW::Saturn},
    {HPSDRModel::ANVELINAPRO3, 2, true,  50, false, HPSDRHW::OrionMKII},
    {HPSDRModel::REDPITAYA,    2, false, 50, false, HPSDRHW::OrionMKII},
};

void TestHardwareProfile::initValuesMatchThetis_data()
{
    QTest::addColumn<HPSDRModel>("model");
    for (const auto& e : kThetisInit) {
        QTest::newRow(displayName(e.model)) << e.model;
    }
}

void TestHardwareProfile::initValuesMatchThetis()
{
    QFETCH(HPSDRModel, model);
    HardwareProfile profile = HardwareProfile::forModel(model);
    const auto& expected = *std::find_if(std::begin(kThetisInit), std::end(kThetisInit),
        [model](const ExpectedInit& e) { return e.model == model; });
    QCOMPARE(profile.adcCount, expected.adcCount);
    QCOMPARE(profile.mkiiBpf, expected.mkiiBpf);
    QCOMPARE(profile.adcSupplyVoltage, expected.adcSupplyVoltage);
    QCOMPARE(profile.lrAudioSwap, expected.lrAudioSwap);
    QCOMPARE(profile.effectiveBoard, expected.board);
}
```

- [ ] **Step 3: Run, confirm FAIL**

Run: `cmake --build build -j$(nproc) --target tst_hardware_profile && ctest --test-dir build -R initValuesMatchThetis -V 2>&1 | tail -30`
Expected: FAIL on ANAN_G2E row (case doesn't exist yet) and potentially on existing rows if NereusSDR's current values diverge.

- [ ] **Step 4: Add ANAN_G2E case to HardwareProfile.cpp**

In the per-model switch (around line 87-195), insert near other modern-class cases:

```cpp
    case HPSDRModel::ANAN_G2E:                                  // From Thetis clsHardwareSpecific.cs:129-135 [v2.10.3.15] //N1GP G2E added
        profile.adcCount         = 1;
        profile.mkiiBpf          = true;
        profile.adcSupplyVoltage = 33;
        profile.lrAudioSwap      = false;
        profile.effectiveBoard   = HPSDRHW::HermesC10;
        break;
```

- [ ] **Step 5: For each existing case, verify against kThetisInit table; fix any divergence**

Re-run the test data-row by data-row. If any existing case fails, that case's HardwareProfile values diverge from Thetis. Fix to match the table with a comment:

```cpp
    case HPSDRModel::<X>:
        profile.adcCount         = N;     // From Thetis clsHardwareSpecific.cs:<line> [v2.10.3.15]
        profile.mkiiBpf          = <0|1>;
        profile.adcSupplyVoltage = <33|50>;
        profile.lrAudioSwap      = <true|false>;
        profile.effectiveBoard   = HPSDRHW::<X>;
        break;
```

- [ ] **Step 6: Run, confirm PASS for all 15 rows**

Run: `ctest --test-dir build -R initValuesMatchThetis -V 2>&1 | tail -30`
Expected: PASS for every data row.

- [ ] **Step 7: Commit**

```bash
git add src/core/HardwareProfile.cpp tests/tst_hardware_profile.cpp
git commit -m "feat(hwprofile): ANAN_G2E init + verify all SKUs against Thetis v2.10.3.15

Per-board HardwareProfile init values now match Thetis
clsHardwareSpecific.cs:85-191 [v2.10.3.15] verbatim — data-driven
test pins all 15 SKUs (Hermes through REDPITAYA + new ANAN_G2E).

ANAN_G2E: adcCount=1, mkiiBpf=true, adcSupplyVoltage=33,
lrAudioSwap=false, effectiveBoard=HermesC10.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task B2: CodecContext fields + buildCodecContext populate

**Files:**
- Modify: `src/core/codec/CodecContext.h` (add 2 fields)
- Modify: `src/core/P1RadioConnection.cpp` (buildCodecContext populate)
- Modify: `src/core/P2RadioConnection.cpp` (buildCodecContext populate)

- [ ] **Step 1: Add fields to CodecContext.h**

Open `src/core/codec/CodecContext.h` and add:

```cpp
    // From Thetis cmaster.SetADCSupply(0, N) — clsHardwareSpecific.cs:85-191 [v2.10.3.15].
    // ADC supply voltage in volts (33 or 50). 0 = use radio firmware default (do not emit).
    int adcSupplyVoltage = 0;

    // From Thetis NetworkIO.LRAudioSwap(N) — clsHardwareSpecific.cs:85-191 [v2.10.3.15].
    // True = swap L/R channels (Hermes-family default); false = no swap (modern boards).
    bool lrAudioSwap = false;
```

- [ ] **Step 2: Populate in P1RadioConnection::buildCodecContext()**

Locate the function (grep for `buildCodecContext` in `P1RadioConnection.cpp`). Add:

```cpp
    ctx.adcSupplyVoltage = m_hardwareProfile.adcSupplyVoltage;  // From Thetis cmaster.SetADCSupply [v2.10.3.15]
    ctx.lrAudioSwap      = m_hardwareProfile.lrAudioSwap;       // From Thetis NetworkIO.LRAudioSwap [v2.10.3.15]
```

- [ ] **Step 3: Same in P2RadioConnection::buildCodecContext()**

Same two lines.

- [ ] **Step 4: Compile**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -10`
Expected: clean build (fields are unused so far; will be consumed in B4/B5).

- [ ] **Step 5: Commit (no behavior change yet — fields plumbed but not emitted)**

```bash
git add src/core/codec/CodecContext.h src/core/P1RadioConnection.cpp src/core/P2RadioConnection.cpp
git commit -m "feat(codec): plumb adcSupplyVoltage + lrAudioSwap from HardwareProfile to CodecContext

Two new CodecContext fields populated from HardwareProfile in
buildCodecContext (P1 + P2). Not yet emitted on the wire — see
Task B4 (LR swap, P1) and Task B5 (ADC supply, P2) for the actual
encode logic.

Closes pre-existing gap where HardwareProfile carried these per-board
values but no codec consumed them. From Thetis
clsHardwareSpecific.cs:85-191 [v2.10.3.15] SetADCSupply/LRAudioSwap
callsites.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task B3: Audit Thetis wire format for SetADCSupply + LRAudioSwap

**Files:** (research-only — no code change)

**Research task — DO NOT skip. Implementer must do this before B4 / B5.**

- [ ] **Step 1: Locate `NetworkIO.LRAudioSwap` in Thetis NetworkIO.cs**

```bash
grep -n "LRAudioSwap\|LR_AUDIO_SWAP\|LRAudio" "/Users/j.j.boyd/Thetis/Project Files/Source/Console/HPSDR/NetworkIO.cs" | head -20
```

Read the method body. Identify:
- The P1 C&C bank/byte position
- Bit position within the byte
- Format (`Convert.ToInt32(value)`, `<<` shifts, etc.)

- [ ] **Step 2: Locate `cmaster.SetADCSupply` source**

```bash
grep -rn "SetADCSupply\|ADC_supply\|adc_supply" "/Users/j.j.boyd/Thetis/Project Files/Source/ChannelMaster/" | head -20
```

Read where the value flows into the P2 outgoing frame. Identify:
- Which command frame carries it (CmdGeneral / CmdHighPriority / CmdRx / dedicated frame)
- Byte position within the frame
- Format/encoding

- [ ] **Step 3: Document findings inline as a comment block in CodecContext.h**

Open `src/core/codec/CodecContext.h` and add after the `lrAudioSwap` field:

```cpp
    // Wire format for the two fields above (audited from Thetis v2.10.3.15):
    //   LRAudioSwap — P1: NetworkIO.cs:<line> emits at C&C bank <N>, byte <M>, bit <K>
    //   ADCSupply   — P2: ChannelMaster/<file>.c emits at CmdGeneral byte <N> (or similar)
    //                 (33 V or 50 V encoded as <raw byte | per-board enum>)
    // STOP-AND-ASK if these don't match what the audit found.
```

Replace the `<placeholders>` with actual findings. If the wire format is more complex than a single byte (e.g., per-board enum lookup), document the full mapping.

- [ ] **Step 4: Commit (audit findings only — pure docs)**

```bash
git add src/core/codec/CodecContext.h
git commit -m "docs(codec): document SetADCSupply + LRAudioSwap wire format per Thetis v2.10.3.15

Audit findings for B4 (LR swap emission) and B5 (ADC supply emission).
Wire byte positions and encoding documented inline in CodecContext.h.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task B4: P1CodecStandard — emit LR audio swap bit

**Files:**
- Modify: `src/core/codec/P1CodecStandard.cpp` (whichever `composeCcForBank()` bank carries the bit per Task B3 audit)
- Test: `tests/tst_codec_wire_bytes.cpp` (new file)

- [ ] **Step 1: Write failing test for LR swap emission**

Create `tests/tst_codec_wire_bytes.cpp`:

```cpp
void TestCodecWireBytes::p1_lrAudioSwap_emitsBit()
{
    P1CodecStandard codec;
    CodecContext ctx{};
    ctx.lrAudioSwap = true;

    // From Thetis NetworkIO.LRAudioSwap [v2.10.3.15] — emits at C&C bank <N>, byte <M>, bit <K>
    QByteArray bank = codec.composeCcForBank(/*bankIndex=*/<N>, ctx);
    QCOMPARE(bank.size(), 5);
    QVERIFY((bank[<M>] >> <K>) & 0x01);  // bit set when lrAudioSwap=true

    ctx.lrAudioSwap = false;
    bank = codec.composeCcForBank(/*bankIndex=*/<N>, ctx);
    QVERIFY(!((bank[<M>] >> <K>) & 0x01));  // bit clear when lrAudioSwap=false
}
```

Replace `<N>`, `<M>`, `<K>` with the actual values from Task B3's audit.

- [ ] **Step 2: Run, confirm FAIL**

Run: `cmake --build build -j$(nproc) --target tst_codec_wire_bytes && ctest --test-dir build -R p1_lrAudioSwap -V`
Expected: FAIL — bit not set.

- [ ] **Step 3: Edit `P1CodecStandard::composeCcForBank()` at bank N**

Locate the bank in `src/core/codec/P1CodecStandard.cpp`. Add the LR swap bit to the appropriate byte:

```cpp
    if (bankIndex == <N>) {
        // ... existing bit assignments ...
        // From Thetis NetworkIO.LRAudioSwap(N) — clsHardwareSpecific.cs:85-191 [v2.10.3.15]
        if (ctx.lrAudioSwap) {
            cc[<M>] |= (1 << <K>);
        }
        // ...
    }
```

- [ ] **Step 4: Run, confirm PASS**

Run: `ctest --test-dir build -R p1_lrAudioSwap -V`
Expected: PASS for both `true` and `false` data points.

- [ ] **Step 5: Commit**

```bash
git add src/core/codec/P1CodecStandard.cpp tests/tst_codec_wire_bytes.cpp
git commit -m "feat(codec/p1): emit LR audio swap bit per Thetis NetworkIO.LRAudioSwap

P1 C&C bank N byte M bit K now reflects ctx.lrAudioSwap. Closes
pre-existing gap where HardwareProfile.lrAudioSwap was populated
per-board (Hermes-family=true, modern=false) but no codec consumed it.

From Thetis NetworkIO.cs:<line> + clsHardwareSpecific.cs:85-191
[v2.10.3.15].

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task B5: P2CodecOrionMkII — emit ADC supply voltage

**Files:**
- Modify: `src/core/codec/P2CodecOrionMkII.cpp` (whichever compose method carries the byte per B3)
- Test: extend `tests/tst_codec_wire_bytes.cpp`

- [ ] **Step 1: Write failing test**

```cpp
void TestCodecWireBytes::p2_adcSupply_emitsByte()
{
    P2CodecOrionMkII codec;
    CodecContext ctx{};
    ctx.adcSupplyVoltage = 33;
    QByteArray cmd = codec.composeCmdGeneral(ctx);  // or whichever frame per B3
    QCOMPARE(cmd.size(), 60);
    QCOMPARE(static_cast<int>(cmd[<N>]), <encoded value for 33V>);  // per B3 audit

    ctx.adcSupplyVoltage = 50;
    cmd = codec.composeCmdGeneral(ctx);
    QCOMPARE(static_cast<int>(cmd[<N>]), <encoded value for 50V>);

    // 0 = sentinel "do not set / use radio default"
    ctx.adcSupplyVoltage = 0;
    cmd = codec.composeCmdGeneral(ctx);
    QCOMPARE(static_cast<int>(cmd[<N>]), <default byte value>);  // unchanged
}
```

- [ ] **Step 2: Run, confirm FAIL**

- [ ] **Step 3: Edit `P2CodecOrionMkII::composeCmdGeneral()` (or appropriate compose method)**

Add:

```cpp
    // From Thetis cmaster.SetADCSupply — clsHardwareSpecific.cs:85-191 [v2.10.3.15]
    if (ctx.adcSupplyVoltage != 0) {
        cmd[<N>] = <encode adc supply per B3 audit>;
    }
```

- [ ] **Step 4: Run, confirm PASS**

- [ ] **Step 5: Commit**

```bash
git add src/core/codec/P2CodecOrionMkII.cpp tests/tst_codec_wire_bytes.cpp
git commit -m "feat(codec/p2): emit ADC supply voltage per Thetis cmaster.SetADCSupply

P2 CmdGeneral byte N now reflects ctx.adcSupplyVoltage. Closes
pre-existing gap where HardwareProfile.adcSupplyVoltage was
populated per-board (33 V for Hermes-class, 50 V for Orion-class)
but no codec consumed it.

From Thetis ChannelMaster/<file>.c + clsHardwareSpecific.cs:85-191
[v2.10.3.15].

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase C — SkuUiProfile + Antenna Button Labels

### Task C1: SkuUiProfile — add EXT1/EXT2 label fields + ANAN_G2E case

**Files:**
- Modify: `src/core/SkuUiProfile.h`
- Modify: `src/core/SkuUiProfile.cpp`
- Test: `tests/tst_sku_ui_profile.cpp` (or extend existing)

**Source cite:** `../Thetis/Project Files/Source/Console/setup.cs:19904-19929 [v2.10.3.15]`

- [ ] **Step 1: Write failing test**

```cpp
void TestSkuUiProfile::g2e_ext2Label_isRxBypass()
{
    auto profile = skuUiProfileFor(HPSDRModel::ANAN_G2E);
    QCOMPARE(profile.ext1OutOnTxLabel, QStringLiteral("Ext 1 on Tx"));
    QCOMPARE(profile.ext2OutOnTxLabel, QStringLiteral("Rx BYPASS on Tx"));
    QCOMPARE(profile.antennaTabLabel, QStringLiteral("Ant/Filters"));
    QCOMPARE(profile.rxOnlyLabels[0], QStringLiteral("BYPS"));
    QCOMPARE(profile.rxOnlyLabels[1], QStringLiteral("EXT1"));
    QCOMPARE(profile.rxOnlyLabels[2], QStringLiteral("XVTR"));
    QVERIFY(profile.hasExt1OutOnTx);
    QVERIFY(profile.hasExt2OutOnTx);
    QVERIFY(!profile.hasRxOutOnTx);
    QVERIFY(!profile.hasRxBypassUi);
}

void TestSkuUiProfile::defaultExt2Label_isExt2OnTx()
{
    // Every other SKU keeps the default
    auto profile = skuUiProfileFor(HPSDRModel::ANAN_G2);
    QCOMPARE(profile.ext1OutOnTxLabel, QStringLiteral("Ext 1 on Tx"));
    QCOMPARE(profile.ext2OutOnTxLabel, QStringLiteral("Ext 2 on Tx"));
}
```

- [ ] **Step 2: Run, confirm FAIL**

Compile error — `ext1OutOnTxLabel` not a member.

- [ ] **Step 3: Add fields to SkuUiProfile.h**

Open `src/core/SkuUiProfile.h` and after the existing fields, add:

```cpp
    // Per-board label overrides for the EXT1/EXT2-on-TX checkboxes in the
    // Antenna control tab. Default to "Ext 1 on Tx" / "Ext 2 on Tx" matching
    // Thetis setup.cs default. Override per SKU when Thetis relabels (e.g.,
    // G2E re-labels chkEXT2OutOnTx to "Rx BYPASS on Tx" at setup.cs:19929 [v2.10.3.15]).
    QString ext1OutOnTxLabel = QStringLiteral("Ext 1 on Tx");
    QString ext2OutOnTxLabel = QStringLiteral("Ext 2 on Tx");
```

- [ ] **Step 4: Add ANAN_G2E case to SkuUiProfile.cpp**

Locate the switch (around line 69-209). Insert ANAN_G2E case in source order:

```cpp
case HPSDRModel::ANAN_G2E:
    // From Thetis setup.cs:19904-19929 [v2.10.3.15] //N1GP G2E added —
    // G2E shares the G2 / 7000D checkbox set with one override:
    // chkEXT2OutOnTx.Text re-labeled to "Rx BYPASS on Tx" at setup.cs:19929.
    p.hasExt1OutOnTx     = true;
    p.hasExt2OutOnTx     = true;
    p.hasRxOutOnTx       = false;
    p.hasRxBypassUi      = false;
    p.rxOnlyLabels       = {QStringLiteral("BYPS"),
                            QStringLiteral("EXT1"),
                            QStringLiteral("XVTR")};
    p.antennaTabLabel    = QStringLiteral("Ant/Filters");
    p.ext1OutOnTxLabel   = QStringLiteral("Ext 1 on Tx");
    p.ext2OutOnTxLabel   = QStringLiteral("Rx BYPASS on Tx");
    break;
```

- [ ] **Step 5: Run, confirm PASS**

```bash
cmake --build build -j$(nproc) --target tst_sku_ui_profile && ctest --test-dir build -R tst_sku_ui_profile -V
```

- [ ] **Step 6: Commit**

```bash
git add src/core/SkuUiProfile.h src/core/SkuUiProfile.cpp tests/tst_sku_ui_profile.cpp
git commit -m "feat(sku): SkuUiProfile EXT label overrides + ANAN_G2E case

Two new SkuUiProfile fields (ext1OutOnTxLabel, ext2OutOnTxLabel)
defaulting to 'Ext 1 on Tx' / 'Ext 2 on Tx'. ANAN_G2E case overrides
ext2OutOnTxLabel to 'Rx BYPASS on Tx' per Thetis setup.cs:19929
[v2.10.3.15] //N1GP G2E added.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task C2: Antenna tab consumer uses new label fields

**Files:**
- Modify: `src/gui/setup/hardware/AntennaAlexAntennaControlTab.cpp` (replace hard-coded "Ext 1 on Tx" / "Ext 2 on Tx" with profile.ext1OutOnTxLabel / .ext2OutOnTxLabel)

- [ ] **Step 1: Find the consumer**

```bash
grep -n "Ext 1 on Tx\|Ext 2 on Tx" src/gui/setup/hardware/*.cpp src/gui/setup/hardware/*.h
```

- [ ] **Step 2: Replace literals with profile field access**

At each match, replace:
```cpp
m_chkExt1OutOnTx->setText(tr("Ext 1 on Tx"));
m_chkExt2OutOnTx->setText(tr("Ext 2 on Tx"));
```

with:
```cpp
m_chkExt1OutOnTx->setText(tr(profile.ext1OutOnTxLabel.toUtf8().constData()));
m_chkExt2OutOnTx->setText(tr(profile.ext2OutOnTxLabel.toUtf8().constData()));
// From Thetis setup.cs:19928-19929 [v2.10.3.15] — per-SKU button label override.
```

(Or whichever idiom matches the existing applyProfile() function in the file.)

- [ ] **Step 3: Compile**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

- [ ] **Step 4: Commit**

```bash
git add src/gui/setup/hardware/AntennaAlexAntennaControlTab.cpp
git commit -m "feat(setup/antenna): consume SkuUiProfile EXT label overrides

Per-SKU EXT1/EXT2-on-TX button text now comes from SkuUiProfile
fields instead of hard-coded literals. Enables G2E's
'Rx BYPASS on Tx' relabel from Task C1 to surface in the UI.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase D — PA path: gains, telemetry, UI gating

### Task D1: PaGainProfile — ANAN_G2E case in kAnan7000dRow group

**Files:**
- Modify: `src/core/PaGainProfile.cpp` (around line 362 — case-group for kAnan7000dRow)
- Test: `tests/tst_pa_gain_profile.cpp`

- [ ] **Step 1: Write failing test**

```cpp
void TestPaGainProfile::ananG2e_usesAnan7000dRow()
{
    // From Thetis clsHardwareSpecific.cs:699-730 [v2.10.3.15] //N1GP G2E added —
    // G2E shares the kAnan7000dRow PA gain table.
    QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2E, Band::B160m), 47.9f);
    QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2E, Band::B20m),  50.9f);
    QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2E, Band::B6m),   44.6f);
    QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2E, Band::VHF0),  63.1f);
    QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2E, Band::VHF13), 63.1f);
}
```

- [ ] **Step 2: Run, confirm FAIL**

```bash
cmake --build build -j$(nproc) --target tst_pa_gain_profile && ctest --test-dir build -R ananG2e_usesAnan7000dRow -V
```

Expected: FAIL — function returns `kPaGainSentinel` (100.0f) for ANAN_G2E.

- [ ] **Step 3: Add ANAN_G2E case to the kAnan7000dRow group**

In `src/core/PaGainProfile.cpp` at the case-group near line 362, change:

```cpp
        // ANAN7000D / ANAN_G2 / ANVELINAPRO3 / REDPITAYA shared row.
        case HPSDRModel::ANAN_G2:
        case HPSDRModel::ANAN7000D:
        // ...
            return lookupHfBand(kAnan7000dRow, band);
```

to:

```cpp
        // ANAN7000D / ANAN_G2 / ANAN_G2E / ANVELINAPRO3 / REDPITAYA shared row.
        // From Thetis clsHardwareSpecific.cs:699-730 [v2.10.3.15] //N1GP G2E added
        case HPSDRModel::ANAN_G2:
        case HPSDRModel::ANAN_G2E:  //N1GP G2E added
        case HPSDRModel::ANAN7000D:
        // ...
            return lookupHfBand(kAnan7000dRow, band);
```

Repeat in the VHF case-group below (same pattern, returns flat 63.1f).

- [ ] **Step 4: Run, confirm PASS**

- [ ] **Step 5: Commit**

```bash
git add src/core/PaGainProfile.cpp tests/tst_pa_gain_profile.cpp
git commit -m "feat(pa-gain): ANAN_G2E uses kAnan7000dRow

From Thetis clsHardwareSpecific.cs:699-730 [v2.10.3.15] //N1GP G2E added.
G2E shares the kAnan7000dRow PA gain table with ANAN_G2/7000D/
ANVELINAPRO3/REDPITAYA tier.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task D2: PaTelemetryScaling — ANAN_G2E fwd + rev triplets

**Files:**
- Modify: `src/core/PaTelemetryScaling.cpp` (around line 80-110)
- Test: `tests/tst_pa_telemetry_scaling.cpp` (create or extend)

- [ ] **Step 1: Write failing test**

```cpp
void TestPaTelemetryScaling::ananG2e_fwdRevTriplets()
{
    // From Thetis console.cs:25007-25015 [v2.10.3.15] //N1GP G2E added (forward power cal)
    auto fwd = fwdTripletFor(HPSDRModel::ANAN_G2E);
    QCOMPARE(fwd.bridgeVolt, 0.15);
    QCOMPARE(fwd.refVoltage, 5.0);
    QCOMPARE(fwd.adcCalOffset, 28);

    // 6m band overrides bridge to 0.7f
    auto fwd6m = fwdTripletForBand(HPSDRModel::ANAN_G2E, Band::B6m);
    QCOMPARE(fwd6m.bridgeVolt, 0.7);

    // From Thetis console.cs:25079-25088 [v2.10.3.15] (RX/reverse power cal)
    auto rev = revTripletFor(HPSDRModel::ANAN_G2E);
    QCOMPARE(rev.bridgeVolt, 0.12);
    QCOMPARE(rev.refVoltage, 5.0);
    QCOMPARE(rev.adcCalOffset, 32);
}
```

- [ ] **Step 2: Run, confirm FAIL**

- [ ] **Step 3: Add ANAN_G2E to forward + reverse case-groups**

In `src/core/PaTelemetryScaling.cpp` fwd-triplet switch:

```cpp
    // From Thetis console.cs:25005-25015 [v2.10.3.15] //N1GP G2E added
    case HPSDRModel::ANAN_G2E:  //N1GP G2E added
    case HPSDRModel::ANAN_G2:
    case HPSDRModel::ANAN_G2_1K:
    case HPSDRModel::ANAN7000D:
    case HPSDRModel::ANVELINAPRO3:
    case HPSDRModel::REDPITAYA:  //DH1KLM
        triplet.bridgeVolt    = (band == Band::B6M) ? 0.7f : 0.15f;
        triplet.refVoltage    = 5.0f;
        triplet.adcCalOffset  = 28;
        return triplet;
```

Same shape for the reverse triplet switch with values 0.12/5.0/32.

- [ ] **Step 4: Run, confirm PASS**

- [ ] **Step 5: Commit**

```bash
git add src/core/PaTelemetryScaling.cpp tests/tst_pa_telemetry_scaling.cpp
git commit -m "feat(pa-telemetry): ANAN_G2E fwd/rev power calibration triplets

From Thetis console.cs:25005-25015 (fwd) and :25079-25088 (rev)
[v2.10.3.15] //N1GP G2E added. G2E joins ANAN_G2/G2_1K/7000D/
ANVELINAPRO3/REDPITAYA tier with bridge_volt=0.15f (0.7f on 6m),
refvoltage=5.0f, adc_cal_offset=28 (fwd) / 32 (rev).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task D3: PaSetupPages — chkAutoPACalibrate visibility per allowsAutoPaCalibrate

**Files:**
- Modify: `src/gui/setup/PaSetupPages.cpp` (`PaGainByBandPage::applyCapabilityVisibility(caps)`)

- [ ] **Step 1: Locate applyCapabilityVisibility**

```bash
grep -n "applyCapabilityVisibility\|m_autoCalibrateCheck" src/gui/setup/PaSetupPages.cpp
```

- [ ] **Step 2: Add visibility gate**

```cpp
void PaGainByBandPage::applyCapabilityVisibility(const BoardCapabilities& caps)
{
    // ... existing gates ...

    // From Thetis setup.cs:19918 [v2.10.3.15] //N1GP G2E added —
    // chkAutoPACalibrate.Visible=false for G2E (auto-cal UI hidden).
    m_autoCalibrateCheck->setVisible(caps.allowsAutoPaCalibrate);
}
```

- [ ] **Step 3: Manual verification (no easy unit test for QWidget visibility without QTest)**

Build, launch app, connect a G2E (or test SKU). Setup → PA Gain by Band → confirm AutoCal checkbox hidden when caps.allowsAutoPaCalibrate=false. (For now, manual; add a QTest::qWaitForWindowExposed test if applet test harness exists.)

- [ ] **Step 4: Commit**

```bash
git add src/gui/setup/PaSetupPages.cpp
git commit -m "feat(setup/pa): gate chkAutoPACalibrate visibility per allowsAutoPaCalibrate

PA Gain by Band page now hides the AutoCal checkbox when
caps.allowsAutoPaCalibrate=false. ANAN_G2E sets this to false per
Thetis setup.cs:19918 [v2.10.3.15] //N1GP G2E added.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task D4: PaSetupPages — NEW chkBypassANANPASettings checkbox

**Files:**
- Modify: `src/gui/setup/PaSetupPages.h` (add member checkbox)
- Modify: `src/gui/setup/PaSetupPages.cpp` (create + lay out + signal-slot wire)
- Modify: `src/models/TransmitModel.h` (add `paSettingsBypass` Q_PROPERTY)
- Modify: `src/models/TransmitModel.cpp` (implement getter/setter/signal/persist)
- Test: extend `tests/tst_transmit_setup_power_pa_page.cpp` (or PaSetupPages test if exists)

- [ ] **Step 1: Add Q_PROPERTY to TransmitModel.h**

In the appropriate section (alongside other PA-related properties):

```cpp
    // Per Thetis setup.cs:19920 [v2.10.3.15] //N1GP G2E added — when checked,
    // PA gain dispatch falls back to bypassPaGainsForBand() (firmware-default
    // PA gains rather than NereusSDR's table). Persisted per-MAC.
    Q_PROPERTY(bool paSettingsBypass READ paSettingsBypass WRITE setPaSettingsBypass NOTIFY paSettingsBypassChanged)
public:
    bool paSettingsBypass() const { return m_paSettingsBypass; }
    void setPaSettingsBypass(bool on);
signals:
    void paSettingsBypassChanged(bool on);
private:
    bool m_paSettingsBypass = false;
```

- [ ] **Step 2: Implement setter in TransmitModel.cpp**

```cpp
void TransmitModel::setPaSettingsBypass(bool on)
{
    if (m_paSettingsBypass == on) return;
    m_paSettingsBypass = on;
    AppSettings::instance().setValue(perMacKey("PaSettingsBypass"),
                                     on ? QStringLiteral("True") : QStringLiteral("False"));
    emit paSettingsBypassChanged(on);
}
```

Plus restoration in the model's restore-from-AppSettings method.

- [ ] **Step 3: Write failing UI test (if PaSetupPages test exists)**

```cpp
void TestPaSetupPages::bypassPaSettingsCheckbox_visibleForG2e()
{
    PaGainByBandPage page;
    BoardCapabilities caps = BoardCapabilities::forBoard(HPSDRHW::HermesC10);
    page.applyCapabilityVisibility(caps);
    QVERIFY(page.bypassPaSettingsCheckbox()->isVisible());

    caps = BoardCapabilities::forBoard(HPSDRHW::OrionMKII);
    page.applyCapabilityVisibility(caps);
    QVERIFY(!page.bypassPaSettingsCheckbox()->isVisible());
}
```

- [ ] **Step 4: Add checkbox to PaGainByBandPage**

In `PaSetupPages.h` private members:

```cpp
    QCheckBox* m_bypassPaSettingsCheck = nullptr;
public:
    QCheckBox* bypassPaSettingsCheckbox() const { return m_bypassPaSettingsCheck; }
```

In `PaSetupPages.cpp` constructor (or setupUi method), add:

```cpp
    // From Thetis setup.cs:19920 [v2.10.3.15] //N1GP G2E added —
    // chkBypassANANPASettings.Visible=true for G2E.
    m_bypassPaSettingsCheck = new QCheckBox(tr("Bypass ANAN PA Settings"));
    m_bypassPaSettingsCheck->setToolTip(tr("Use firmware-default PA gains instead of "
                                            "the PA gain table on this page."));
    layout->addWidget(m_bypassPaSettingsCheck);

    connect(m_bypassPaSettingsCheck, &QCheckBox::toggled,
            this, [](bool on) {
                if (auto* tm = TransmitModel::instance()) {
                    tm->setPaSettingsBypass(on);
                }
            });
```

In `applyCapabilityVisibility(caps)`:

```cpp
    m_bypassPaSettingsCheck->setVisible(caps.showsBypassPaSettingsUi);
```

- [ ] **Step 5: Wire PaGainProfile fallback to honor the bypass flag**

In `src/core/PaGainProfile.cpp` `defaultPaGainsForBand()`:

```cpp
float defaultPaGainsForBand(HPSDRModel model, Band band) noexcept {
    if (auto* tm = TransmitModel::instance(); tm && tm->paSettingsBypass()) {
        return bypassPaGainsForBand(band);
    }
    switch (model) {
        // ... existing cases ...
    }
}
```

(If TransmitModel singleton access isn't available in `noexcept` context, refactor to take the bypass flag as a parameter from the caller — verify which path is used.)

- [ ] **Step 6: Run, confirm PASS**

- [ ] **Step 7: Commit**

```bash
git add src/gui/setup/PaSetupPages.h src/gui/setup/PaSetupPages.cpp src/models/TransmitModel.h src/models/TransmitModel.cpp src/core/PaGainProfile.cpp tests/
git commit -m "feat(setup/pa): chkBypassANANPASettings checkbox + paSettingsBypass model prop

NEW UI from Thetis setup.cs:19920 [v2.10.3.15] //N1GP G2E added.
Checkbox visible only for SKUs with caps.showsBypassPaSettingsUi=true
(ANAN_G2E for now). When checked, PaGainProfile falls back to
bypassPaGainsForBand() (firmware-default PA gains). Persisted per-MAC
via TransmitModel::paSettingsBypass Q_PROPERTY.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task D5: PaValuesPage — gate volts/amps display per HasVolts/HasAmps

**Files:**
- Modify: `src/gui/setup/PaSetupPages.cpp` (`PaValuesPage` setup, around line 689-697)
- Test: extend `tests/tst_pa_values_page.cpp` (if exists)

- [ ] **Step 1: Locate PaValuesPage volts/amps labels**

```bash
grep -n "PaValuesPage\|m_paVolts\|m_paAmps\|MetricLabel" src/gui/setup/PaSetupPages.cpp src/gui/setup/PaSetupPages.h | head -20
```

- [ ] **Step 2: Add visibility gates in applyCapabilityVisibility**

```cpp
void PaValuesPage::applyCapabilityVisibility(const BoardCapabilities& caps)
{
    // From Thetis HasVolts/HasAmps (clsHardwareSpecific.cs:245-264 [v2.10.3.15]) —
    // hide PA voltage/current readouts on boards without on-board telemetry sensors.
    m_paVoltsLabel->setVisible(caps.hasPaVoltsTelemetry);
    m_paAmpsLabel->setVisible(caps.hasPaAmpsTelemetry);
    // ... other gates ...
}
```

- [ ] **Step 3: Manual test (or QTest qWaitForWindowExposed)**

Build, launch, connect a Hermes board → no volts/amps labels. Connect a G2/G2E → labels visible.

- [ ] **Step 4: Commit**

```bash
git add src/gui/setup/PaSetupPages.cpp
git commit -m "feat(setup/pa): gate PA volts/amps display per HasVolts/HasAmps

PaValuesPage hides PA voltage/current MetricLabels on boards without
on-board telemetry sensors. From Thetis HasVolts/HasAmps properties
(clsHardwareSpecific.cs:245-264 [v2.10.3.15]).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task D6: ATT-on-TX UI visibility audit

**Files:**
- Modify: `src/gui/setup/TransmitSetupPages.cpp` (verify or add per-SKU gate)

- [ ] **Step 1: Find current ATT-on-TX UI implementation**

```bash
grep -n "m_attOnTxSpin\|attOnTx\|labelATTOnTX" src/gui/setup/*.cpp src/gui/setup/*.h
```

- [ ] **Step 2: Audit current visibility logic**

Is it universal (visible for all boards) or per-SKU? If universal, no change needed — G2E gets it by default. If per-SKU, ensure ANAN_G2E case sets visible=true.

- [ ] **Step 3: Document finding in spec verification matrix; commit if a change was made.**

If no change: skip commit.

If change made:
```bash
git add src/gui/setup/TransmitSetupPages.cpp
git commit -m "feat(setup/tx): ATT-on-TX UI visibility verified for G2E

Per Thetis setup.cs:19924-19925 [v2.10.3.15] //N1GP G2E added —
labelATTOnTX.Visible=true / udATTOnTX.Visible=true for G2E.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase E — HermesC10 RX-side switches (4 branches)

Each task in Phase E follows the same pattern: grep NereusSDR for the existing family grouping (e.g., `HPSDRHW::Hermes` + `HPSDRHW::HermesII`), add `HPSDRHW::HermesC10` to the case list with the verbatim `//N1GP G2E added (HermesC10)` inline tag preserved.

### Task E1: setAlex1HPF equivalent — add HermesC10 to OrionMKII+Saturn group

**Source cite:** `../Thetis/Project Files/Source/Console/console.cs:6829-6834 [v2.10.3.15]`

```cpp
            if ((HardwareSpecific.Hardware == HPSDRHW.OrionMKII) || (HardwareSpecific.Hardware == HPSDRHW.Saturn)
               || (HardwareSpecific.Hardware == HPSDRHW.HermesC10))  //N1GP G2E added (HermesC10) //DK1HLM
            {
                setBPF1ForOrionIISaturn(freq);
            }
```

- [ ] **Step 1: Find NereusSDR equivalent**

```bash
grep -rn "HPSDRHW::OrionMKII" src/core/codec/ src/core/ src/gui/ | grep -i "saturn\|bpf1\|setAlex1HPF\|hpf"
```

Expected: one or more switch/conditional blocks grouping OrionMKII + Saturn for BPF1 routing. Likely in `P2CodecOrionMkII.cpp`, `P2CodecSaturn.cpp`, or `AlexFilterMap.cpp`.

- [ ] **Step 2: Write failing test**

```cpp
void TestAlexFilterMap::hermesC10_usesBpf1Algorithm()
{
    // From Thetis console.cs:6830 [v2.10.3.15] //N1GP G2E added (HermesC10) //DK1HLM
    // HermesC10 joins OrionMKII + Saturn for BPF1 (MKII-style filter routing).
    QVERIFY(boardUsesBpf1Algorithm(HPSDRHW::HermesC10));
    QVERIFY(boardUsesBpf1Algorithm(HPSDRHW::OrionMKII));
    QVERIFY(boardUsesBpf1Algorithm(HPSDRHW::Saturn));
    QVERIFY(!boardUsesBpf1Algorithm(HPSDRHW::Hermes));
}
```

(Adapt to the actual function name and signature found in Step 1.)

- [ ] **Step 3: Run, confirm FAIL**

- [ ] **Step 4: Add HermesC10 to the group**

```cpp
    if (board == HPSDRHW::OrionMKII || board == HPSDRHW::Saturn
        || board == HPSDRHW::HermesC10)  // From Thetis console.cs:6830 [v2.10.3.15] //N1GP G2E added (HermesC10) //DK1HLM
    {
        return useBpf1Algorithm();
    }
```

- [ ] **Step 5: Run, confirm PASS**

- [ ] **Step 6: Commit**

```bash
git add <file modified> tests/<test>
git commit -m "feat(codec): HermesC10 joins OrionMKII+Saturn BPF1 algorithm group

From Thetis console.cs:6830 [v2.10.3.15] //N1GP G2E added (HermesC10) //DK1HLM —
G2E uses the OrionII/Saturn BPF1 (MKII multi-band filter) algorithm
rather than the legacy Alex1 HPF path.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task E2: RX1 attenuator switch — add HermesC10 to Hermes+HermesII group

**Source cite:** `../Thetis/Project Files/Source/Console/console.cs:8607-8620 [v2.10.3.15]`

```cpp
                    case HPSDRHW.Hermes: // ANAN-10 ANAN-100 Heremes
                    case HPSDRHW.HermesII: // ANAN-10E ANAN-100B HeremesII
                    case HPSDRHW.HermesC10: // ANAN-G2E //N1GP G2E added (HermesC10)
                        switch (tot)
                        ...
```

- [ ] **Step 1: Find NereusSDR equivalent**

```bash
grep -rn "HPSDRHW::Hermes\b" src/core/ src/core/codec/ | grep -v "HermesLite\|HermesC10\|HermesII\|HermesLiteRxOnly"
```

Then look for the one that also groups HermesII for RX1 attenuator-related routing.

- [ ] **Step 2-6: Same shape as Task E1**

Test → fail → add `case HPSDRHW::HermesC10:` to the existing case list → pass → commit.

### Task E3: RX1 output path — add HermesC10 to Hermes (4-ADC) group

**Source cite:** `../Thetis/Project Files/Source/Console/console.cs:8700-8713 [v2.10.3.15]`

- [ ] **Step 1-6: Same pattern as E2.**

### Task E4: VFO split MOX display — add HermesC10 to Hermes+HermesII group

**Source cites:**
- `../Thetis/Project Files/Source/Console/console.cs:32570-32576 [v2.10.3.15]` — VFOASub during split TX
- `../Thetis/Project Files/Source/Console/console.cs:32599-32609 [v2.10.3.15]` — VFOASub PS-state handling

- [ ] **Step 1: Find NereusSDR equivalent**

Likely in `src/gui/widgets/VfoWidget.cpp` or `src/models/SliceModel.cpp` — search for VFO split + MOX gating per HPSDRHW.

```bash
grep -rn "HPSDRHW::Hermes\b\|HPSDRHW::HermesII" src/gui/widgets/ src/models/
```

- [ ] **Step 2-6: Same pattern.** Two case-blocks (32572 and 32604 both need updating with the same case list).

---

## Phase F — ANAN_G2E model-keyed switches (TX path + UI)

### Task F1: TX exciter formula — ANAN_G2E joins OrionMKII group

**Source cite:** `../Thetis/Project Files/Source/Console/console.cs:26001-26013 [v2.10.3.15]`

```cpp
case HPSDRModel.ORIONMKII:
case HPSDRModel.ANAN7000D:
case HPSDRModel.ANAN8000D:
case HPSDRModel.ANAN_G2E: //N1GP G2E added
case HPSDRModel.ANAN_G2:
case HPSDRModel.ANAN_G2_1K:
case HPSDRModel.ANVELINAPRO3:
case HPSDRModel.REDPITAYA: //DH1KLM
    drivepwr = computeOrionMkIIExciterPower();
```

- [ ] **Step 1: Find NereusSDR equivalent of computeExciterPower / OrionMKII formula dispatch**

```bash
grep -rn "computeOrionMkIIExciterPower\|computeExciterPower\|driveExciter\|exciter.*power\|orionMkII.*exciter" src/core/ src/models/ | head -20
```

- [ ] **Step 2-6: Test + add ANAN_G2E case to the group + commit.**

### Task F2: Audio mix states — ANAN_G2E joins HERMES 4-DDC family (USB)

**Source cite:** `../Thetis/Project Files/Source/Console/console.cs:27653-27664 [v2.10.3.15]`

- [ ] **Step 1-6:** Standard pattern.

### Task F3: Audio mix states — ANAN_G2E joins HERMES 2-DDC family (ETH)

**Source cite:** `../Thetis/Project Files/Source/Console/console.cs:27669-27679 [v2.10.3.15]`

- [ ] **Step 1-6:** Standard pattern.

### Task F4: P1 user I/O inhibit bit — ANAN_G2E uniquely groups with 7000D/8000D/REDPITAYA

**Source cite:** `../Thetis/Project Files/Source/Console/console.cs:25859-25865 [v2.10.3.15]`

This is a G2E-unique grouping: G2E goes with 7000D/8000D/REDPITAYA on bit[2] (not with G2/G2_1K which use bit[1]).

```cpp
if (NetworkIO.CurrentRadioProtocol == RadioProtocol.USB)
{
    // protocol 1
    if (HardwareSpecific.Model == HPSDRModel.ANAN_G2E || HardwareSpecific.Model == HPSDRModel.ANAN7000D ||
        HardwareSpecific.Model == HPSDRModel.ANAN8000D || HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM should be in P1  //N1GP G2E added
        inhibit_input = !NetworkIO.getUserI02(); // bit[2] of C1
    else
        inhibit_input = !NetworkIO.getUserI01(); // bit[1] of C1
}
```

- [ ] **Step 1: Find NereusSDR equivalent**

```bash
grep -rn "getUserI01\|getUserI02\|inhibit_input\|userIo.*inhibit" src/core/ | head -20
```

- [ ] **Step 2-6: Standard pattern**, ensuring the exact SKU grouping is preserved (ANAN_G2E with 7000D/8000D/REDPITAYA, NOT with G2/G2_1K).

### Task F5: Preamp combo items — ANAN_G2E gets ANAN-100D preamp settings

**Source cite:** `../Thetis/Project Files/Source/Console/console.cs:40871-40879 [v2.10.3.15]`

- [ ] **Step 1: Find NereusSDR `preampItemsForBoard` or equivalent**

```bash
grep -rn "preampItemsForBoard\|preamp.*items\|comboPreamp\|anan100d_preamp_settings" src/core/ src/gui/
```

- [ ] **Step 2-6: Add ANAN_G2E case returning the ANAN-100D preamp items array.**

### Task F6: TX meter modes — ANAN_G2E joins capable group

**Source cite:** `../Thetis/Project Files/Source/Console/console.cs:14868-14882 [v2.10.3.15]`

```cpp
case HPSDRModel.ANAN100D:
case HPSDRModel.ANAN200D:
case HPSDRModel.ORIONMKII:
case HPSDRModel.ANAN7000D:
case HPSDRModel.ANAN8000D:
case HPSDRModel.ANAN_G2E: //N1GP G2E added
case HPSDRModel.ANAN_G2:
case HPSDRModel.ANAN_G2_1K:
case HPSDRModel.ANVELINAPRO3:
case HPSDRModel.REDPITAYA: //DH1KLM
    if (!comboMeterTXMode.Items.Contains("Ref Pwr"))
        comboMeterTXMode.Items.Insert(1, "Ref Pwr");
    ...
```

- [ ] **Step 1: Find NereusSDR's TX meter mode list builder**

```bash
grep -rn "comboMeterTXMode\|meterTxMode\|tx.*meter.*mode" src/gui/ src/models/
```

- [ ] **Step 2-6: Add ANAN_G2E to the capable group.**

### Task F7: RX2 preamp present FALSE (G2E-unique)

**Source cite:** `../Thetis/Project Files/Source/Console/console.cs:14831-14838 [v2.10.3.15]`

```cpp
case HPSDRModel.ANAN_G2E: //N1GP G2E added
    chkDX.Visible = false;
    _rx2_preamp_present = false;
    break;
```

This is the unique G2E-only deviation: G2E lacks RX2 preamp, unlike ANAN_G2 which has it (true).

- [ ] **Step 1: Find NereusSDR's RX2 preamp-present logic**

The kHermesC10 row has `.preamp = {true, false}` — verify this propagates to RX2 UI gating. Search for where RX2 preamp combo is built/hidden:

```bash
grep -rn "rx2.*preamp\|preamp.*rx2\|caps.preamp\.\|preamp\[1\]" src/gui/ src/core/ src/models/
```

- [ ] **Step 2-6: Test that for HermesC10 caps, the RX2 preamp UI element is not present.**

Likely a QTest of the RxApplet or SetupHardwarePage.

### Task F8: Other G2E inclusion branches

Implementer reference: spec §6.11 table. For each "Needs explicit branch" entry not already covered above (DDC dynamic switching at console.cs:31357, spectrum analyzer use_sa at 53090/53249, etc.), follow the same pattern.

- [ ] **Step 1: Open spec §6.11 table.**
- [ ] **Step 2: For each unaddressed Thetis branch with status "Needs explicit branch":**
  - Find NereusSDR equivalent
  - Add `case HPSDRModel::ANAN_G2E:` (or `case HPSDRHW::HermesC10:`) with inline tag preservation
  - Add a test
  - Commit per branch (or batch related branches into one commit if they're in the same NereusSDR file)

---

## Phase G — Provenance + Verification

### Task G1: THETIS-PROVENANCE.md — bump cite versions

**Files:** Modify `docs/attribution/THETIS-PROVENANCE.md`

- [ ] **Step 1: List files touched by this PR**

```bash
git log --name-only --pretty=format: <first commit of branch>..HEAD | sort -u | grep -v '^$'
```

- [ ] **Step 2: For each row in THETIS-PROVENANCE.md whose file is in the list, bump `[v2.10.3.13]` → `[v2.10.3.15]` and add G2E mention to the "notes" column.**

- [ ] **Step 3: Commit**

```bash
git add docs/attribution/THETIS-PROVENANCE.md
git commit -m "docs(attribution): bump cite versions to v2.10.3.15 for G2E-port files

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task G2: CLAUDE.md — update Thetis version reference

**Files:** Modify `CLAUDE.md`

- [ ] **Step 1: Find the standing `[v2.10.3.13]` reference**

```bash
grep -n "v2.10.3.13\|Thetis version" CLAUDE.md | head -10
```

- [ ] **Step 2: Bump references where appropriate**

The `[v2.10.3.13]` stamps on existing cites stay (they record what those cites were verified against at the time). The standing "current Thetis version" pointer bumps to v2.10.3.15.

- [ ] **Step 3: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: bump CLAUDE.md Thetis reference to v2.10.3.15

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task G3: Bench verification matrix

**Files:** Create `docs/architecture/anan-g2e-verification/README.md`

- [ ] **Step 1: Create the file with the 12-row matrix from spec §7**

```markdown
# ANAN-G2E (HermesC10) Bench Verification Matrix

Status: `Pending bench` (gated on receipt of actual G2E hardware).

| # | Row | Procedure | Pass criterion | Status |
|---|---|---|---|---|
| 1 | Discovery | Power on G2E, launch NereusSDR | ConnectionPanel shows "ANAN-G2E" with board badge "HermesC10" | Pending |
| 2 | RX | Click Connect, set 14.200 MHz USB | Audio heard, FFT renders, RX2 enables and tunes | Pending |
| 3 | RX2 UI gating | Open RxApplet | No RX2 preamp combo; no RX2 stepped-att slider | Pending |
| 4 | PureSignal | Set TX to clean tone, Tools→PureSignal | PSForm opens, calc converges; peak indicator stable | Pending |
| 5 | PA gain table | Setup→PA Gain by Band | Sliders match N1GP defaults from design §3 | Pending |
| 6 | PA telemetry | Setup→PA Values | Volts, amps, temperature labels visible | Pending |
| 7 | Antenna labels | Setup→Antenna Control | RX-only labels read BYPS/EXT1/XVTR; EXT2 button reads "Rx BYPASS on Tx" | Pending |
| 8 | AutoPACalibrate hidden | Setup→PA Gain by Band | chkAutoPACalibrate not visible | Pending |
| 9 | Bypass PA Settings | Setup→PA Gain by Band | chkBypassANANPASettings visible; toggling switches gain dispatch to bypass row | Pending |
| 10 | ATT-on-TX | Setup→Transmit→PA | labelATTOnTX + spinbox render | Pending |
| 11 | MKII BPF on-air | Connect, listen on 20m and 6m | High-band Alex filters engage per band | Pending |
| 12 | Wire capture diff | Wireshark NereusSDR vs Thetis on connect | ADC supply + LR swap bytes match Thetis-emitted values | Pending |
```

- [ ] **Step 2: Commit**

```bash
git add docs/architecture/anan-g2e-verification/README.md
git commit -m "docs(verification): ANAN-G2E 12-row bench matrix (pending hardware)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task G4: Full ctest run

- [ ] **Step 1: Run the entire test suite**

```bash
cmake --build build -j$(nproc) && ctest --test-dir build -j$(nproc) 2>&1 | tail -30
```

Expected: 100% pass.

- [ ] **Step 2: Fix any regressions inline**

If any tests fail that aren't directly related to this PR's work, diagnose and fix (per CLAUDE.md "Failed tests are never ignored" memory rule). Commit fixes with `fix(test): ...` messages.

- [ ] **Step 3: Re-run verifier and pre-commit hooks**

```bash
NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis NEREUS_MI0BOT_DIR=/Users/j.j.boyd/mi0bot-Thetis python3 scripts/verify-inline-tag-preservation.py
```

Expected: `[tag-preservation] OK — no missing tags detected`.

- [ ] **Step 4: Final summary commit (or amend G3)**

If everything is clean, no additional commit needed. If you fixed regressions, those are their own commits already.

---

## Final state at end of plan

- All commits GPG-signed.
- ctest 100% green.
- verify-inline-tag-preservation.py reports 0 missing tags.
- New files: `tests/tst_codec_wire_bytes.cpp` (if not already extant), `docs/architecture/anan-g2e-verification/README.md`, possibly `tests/tst_pa_telemetry_scaling.cpp` and `tests/tst_hardware_profile.cpp` depending on what already exists.
- BoardCapabilities has 13 rows (was 12); 4 new fields populated correctly across all 13.
- HardwareProfile per-board init verified table-driven against Thetis v2.10.3.15 for all 15 SKUs.
- Codec layer emits ADC supply voltage and LR audio swap wire bytes (closes pre-existing gap).
- SkuUiProfile has 2 new label fields; ANAN_G2E case populated.
- 30 console.cs Thetis branches absorbed.
- THETIS-PROVENANCE.md and CLAUDE.md bumped to v2.10.3.15.
- 12-row bench verification matrix exists.
- Ready for `gh pr create` with title `feat(boards): ANAN-G2E (HermesC10) full Thetis-parity port (v2.10.3.15 by N1GP)`.

---

## Self-review notes

1. **Spec coverage:** This plan walks every numbered section in `2026-05-21-anan-g2e-port-design.md` §6 (file-by-file changes). §6.11's 30-branch table is fanned out across Phases E and F. §7 verification is Phase G. §10 acceptance criteria are the "final state" bullets above.

2. **Type consistency:** Functions referenced in tests (`BoardCapabilities::forBoard`, `defaultPaGainsForBand`, `skuUiProfileFor`, `fwdTripletFor`, `parseP1Reply`) all match the names used in existing NereusSDR code (verified via the deep-research Explores).

3. **Placeholder scan:** Phases E and F task descriptions use "Step 1-6: Standard pattern" as shorthand referring back to the fully-detailed Task E1 template. Implementer reading sequentially has the full template in E1; later tasks reference it. This is intentional DRY, not a placeholder. Three places use `<N>`, `<M>`, `<K>` (Task B4) and `<encoded value>` (Task B5) — these are resolved by Task B3's audit and explicitly called out as "Replace `<X>` with the actual values from Task B3's audit." Not a placeholder; a planned dependency.

4. **Scope decomposition:** Phases A-G are sequential; each phase produces working, testable software. Phase A alone gives you HermesC10 discoverable on the wire (radio shows up in ConnectionPanel even if downstream behavior is incomplete). Phase B alone gets the wire bytes emitting. Etc. If JJ wants to split into multiple PRs, the phase boundary is the natural split point — but the spec calls for a single PR per the no-shortcuts directive.
