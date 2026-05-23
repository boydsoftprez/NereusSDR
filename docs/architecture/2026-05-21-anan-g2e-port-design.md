# ANAN-G2E (HermesC10) Port — Design

**Date:** 2026-05-21 (revised same day per Thetis-parity directive)
**Status:** Approved
**Author:** JJ Boyd (KG4VCF) with Claude Opus 4.7
**Upstream:** Thetis [v2.10.3.15](https://github.com/ramdor/Thetis/releases/tag/v2.10.3.15), tag named literally `g2e`, all changes by Rick **N1GP** (annotated `//N1GP G2E added`).

---

## 1. Overview

Apache Labs released the ANAN-G2E (formerly named ANAN-G1; same hardware, renamed by N1GP). Thetis v2.10.3.15 added the model. This spec ports it into NereusSDR **at full Thetis parity** — every Thetis branch that mentions G2E (either via `HPSDRModel.ANAN_G2E` or `HPSDRHW.HermesC10`) gets a NereusSDR equivalent. No deferrals; no "we'll do it later."

Where Thetis behavior reveals a NereusSDR-architectural gap that affects more than just G2E (e.g., `cmaster.SetADCSupply` and `NetworkIO.LRAudioSwap` are stored in `HardwareProfile` but never emitted), the gap is closed in this PR for *all* boards, not just HermesC10.

## 2. Source-first lineage

Every port cite carries `[v2.10.3.15]`. Every `//N1GP G2E added` inline tag is preserved verbatim per CLAUDE.md inline-comment preservation rule.

Thetis files containing G2E changes (paths under `/Users/j.j.boyd/Thetis/Project Files/Source/`):

| File | What N1GP added |
|---|---|
| `ChannelMaster/network.h:425` | `HermesC10 = 20` (board enum, wire byte) |
| `ChannelMaster/network.h:446` | `HPSDRModel_ANAN_G2E = 16` (model enum) |
| `Console/enums.cs` (2 lines) | C# mirrors of the two enums |
| `Console/clsHardwareSpecific.cs:129-135` | Model init: `SetRxADC(1)`, `SetMKIIBPF(1)`, `SetADCSupply(0,33)`, `LRAudioSwap(0)`, `Hardware=HPSDRHW.HermesC10` |
| `Console/clsHardwareSpecific.cs:245-254` | `HasVolts` property — G2E joins ANAN-7000D / 8000D / ANVELINAPRO3 / G2 / G2_1K / REDPITAYA |
| `Console/clsHardwareSpecific.cs:255-264` | `HasAmps` property — same SKU group |
| `Console/clsHardwareSpecific.cs:357-358, 385-386` | Bidirectional model ↔ string conversion ("ANAN-G2E") |
| `Console/clsHardwareSpecific.cs:699-730` | PA gain table entry (shares row with ANAN-G2 / ANAN-7000D / ANVELINAPRO3 / REDPITAYA) |
| `Console/clsHardwareSpecific.cs:790-803` | `HasSteppedAttenuation(rx==2)` returns false for G2E — grouped with HERMES / ANAN10 / 10E / 100 / 100B (single-DDC family) |
| `Console/setup.cs:6340` | BPF panel visibility include — G2E gets BPF UI |
| `Console/setup.cs:6424-6437` | OtherHW tab insertion |
| `Console/setup.cs:15810-15824, 15869-15883` | RX1/RX2 attenuator max — G2E in exclusion list so max stays at 31 dB |
| `Console/setup.cs:19904-19929` | PA UI config block — Alex forced on, Apollo disabled, auto-cal hidden, bypass-PA-settings visible, RX-only antenna labels `BYPS/EXT1/XVTR`, EXT2 button re-labeled "Rx BYPASS on Tx", ATT-on-TX visible |
| `Console/setup.cs:20537-20548, 20710-20754` | ADC reset / ADC visibility (single-ADC: only ADC0) |
| `Console/setup.cs:23746-23762` | 25 PA-gain persistence keys (`udANAN_G2EPAGain<HF>` × 11, `udANAN_G2EPAGainVHF<n>` × 14) |
| `Console/setup.designer.cs:8572` | "ANAN-G2E" entry in radio model combo |
| `Console/console.cs` (32 branches) | RX/TX path, DDC routing, exciter formula, meter modes, BPF/HPF, attenuator, audio mix, etc. (full mapping in §6.11) |
| `Console/cmaster.cs:594-606, 685-707, 807-826, 885-907` | ChannelMaster DDC routing arrays (G2E reuses HERMES routing) |
| `Console/Andromeda/Andromeda.cs:1235, 2446, 2496, 2591` | Andromeda G2 *console panel* gating clauses — orthogonal to G2E radio model (see §6.13) |
| `Console/ReleaseNotes.txt:23, 42` | Rename + add entries |

## 3. G2E hardware spec

From `clsHardwareSpecific.cs:129-135 [v2.10.3.15]` (model init) and downstream console.cs branches:

| Capability | Value | Thetis cite |
|---|---|---|
| ADC count | 1 | `SetRxADC(1)` |
| Max RX | 4 (RX1, RX2, RX3 diversity, RX4 PureSignal feedback) | `console.cs:8388 P1_rxcount=4 nddc=4` |
| RX1 preamp | Present | (HERMES-class default; no override) |
| RX2 preamp | **Not present** | `console.cs:14835 _rx2_preamp_present = false` |
| RX1 stepped attenuator | Present (0–31 dB) | (HERMES-class default) |
| RX2 stepped attenuator | **Not present** | `clsHardwareSpecific.cs:790-803 HasSteppedAttenuation(rx==2)==false` |
| RX1/RX2 attenuator max | 31 dB (NOT 61) | G2E in exclusion list at `setup.cs:15810-15824 + :15869-15883` |
| Alex / Alex-2 | Both on | `SetMKIIBPF(1)` + `chkAlexPresent.Checked=true` at setup.cs:19905 |
| Antenna inputs | 3 (Ant1/Ant2/Ant3 + RX-only Bypass/Ext1/XVTR) | inherited from HERMES class |
| L/R audio swap | Off | `LRAudioSwap(0)` |
| MKII BPF | On | `SetMKIIBPF(1)` |
| ADC supply | 33 V | `SetADCSupply(0, 33)` |
| PureSignal | Enabled | RX4 dedicated as feedback DDC; `psDefaultPeak=0.2899` (P2 default), `psSampleRate=192000` |
| Diversity RX | **No** | only 1 ADC |
| TX exciter formula | Orion MKII | `console.cs:26006 computeOrionMkIIExciterPower()` |
| TX PA power cal | bridge=0.15f (0.7f on 6 m), refvolt=5.0f, adc_cal_offset=28 | `console.cs:25007-25015` |
| RX PA power cal | bridge=0.12f, refvolt=5.0f, adc_cal_offset=32 | `console.cs:25081-25088` |
| PA gain (HF, per band) | 47.9 / 50.5 / 50.8 / 50.8 / 50.9 / 50.9 / 50.5 / 47.0 / 47.9 / 46.5 / 44.6 (160 / 80 / 60 / 40 / 30 / 20 / 17 / 15 / 12 / 10 / 6 m) | `clsHardwareSpecific.cs:699-730` (shared with ANAN-G2 / ANAN-7000D / ANVELINAPRO3 / REDPITAYA) |
| PA gain (VHF flat) | 63.1 | same row |
| Preamp combo items | ANAN-100D preamp values | `console.cs:40874` |
| Apollo | No | (G2-class boards don't use Apollo) |
| Ganymede 500 W PA capable | **No** | not an Andromeda-console SKU |
| `HasVolts` | **Yes** | `clsHardwareSpecific.cs:245-254` |
| `HasAmps` | **Yes** | `clsHardwareSpecific.cs:255-264` |
| Auto-PA-Calibrate UI | **Hidden** | `setup.cs:19918 chkAutoPACalibrate.Visible=false` |
| "Bypass ANAN PA Settings" UI | **Visible** | `setup.cs:19920 chkBypassANANPASettings.Visible=true` |
| ATT-on-TX UI | **Visible** | `setup.cs:19924-19925 labelATTOnTX.Visible=true / udATTOnTX.Visible=true` |
| EXT2-on-TX button label | **"Rx BYPASS on Tx"** (re-labeled) | `setup.cs:19929 chkEXT2OutOnTx.Text="Rx BYPASS on Tx"` |
| EXT1-on-TX button label | "Ext 1 on Tx" (default) | `setup.cs:19928 chkEXT1OutOnTx.Text="Ext 1 on Tx"` |
| Audio mix states (P1 USB) | 4 & 5 DDC family | `console.cs:27655 SetAAudioMixStates(RX1+RX1S+RX2+MON, ...)` |
| Audio mix states (P2 ETH) | 2-DDC family | `console.cs:27674` (HERMES grouping) |
| Spectrum analyzer mode (use_sa) | Yes | `console.cs:53090 / 53249` |
| P1 user I/O inhibit bit | **bit[2]** of C1 (newer position) | `console.cs:25862` — G2E grouped with 7000D / 8000D / REDPITAYA, NOT with G2 / G2_1K |

### 3.1 Two-layer SKU dispatch (Thetis pattern, mirrored)

Thetis dispatches G2E behavior via **two distinct enum keys**. NereusSDR must mirror both:

**`HPSDRModel.ANAN_G2E` switches** (28 branches in console.cs + clsHardwareSpecific + setup.cs + cmaster.cs) — TX path, PA, model-level UI gating, init. G2E groups with G2 / G2_1K / 7000D / 8000D / OrionMKII / ANVELINAPRO3 / REDPITAYA for most modern-capable features.

**`HPSDRHW.HermesC10` switches** (4 branches in console.cs) — RX-side wire routing. G2E groups with **Hermes / HermesII** (NOT G2), revealing that G2E uses Hermes-era RX silicon under a MKII BPF wrap:

- `console.cs:6830 setAlex1HPF` — uses OrionII/Saturn BPF1 algorithm (HermesC10 joins OrionMKII + Saturn here)
- `console.cs:8612` RX1 attenuator switch — 3-attenuator family (HermesC10 joins Hermes + HermesII)
- `console.cs:8705` RX1 output path — 4-ADC routing (HermesC10 joins Hermes)
- `console.cs:32572, 32604` VFO sub display during split MOX — legacy split (HermesC10 joins Hermes + HermesII)

Interpretation: **G2E is HERMES-architecture RX silicon (single-ADC, 3-step att, VFO channels 0/1) + OrionMKII-class TX/PA/exciter + MKII BPF filter bank**. The `kHermesC10` capability row encodes this; the per-NereusSDR-file changes in §6 carry it into every relevant switch.

## 4. The HPSDRHW value-20 collision and resolution (Option A, approved)

Thetis assigns `HermesC10 = 20`. NereusSDR's [HpsdrModel.h:132](../../src/core/HpsdrModel.h:132) already used `Andromeda = 20` as a reserved NereusSDR-native slot for a future Apache Labs Andromeda-console radio.

**Andromeda audit verified:** 15 reference points, all in tests/docs/comment cites; zero in `boardForModel()`, `RadioDiscovery.cpp`, `SkuUiProfile`, or string-keyed AppSettings. No radio has ever been wire-detected as Andromeda. Moving its enum value is safe.

**Resolution:** `Andromeda` → 21. `HermesC10` takes 20, matching Thetis byte-for-byte. Migration: none required.

## 5. Enum value assignments (locked)

| Symbol | New value | Cite |
|---|---|---|
| `HPSDRHW::HermesC10` | 20 | `network.h:425 [v2.10.3.15]` |
| `HPSDRHW::Andromeda` | 21 | NereusSDR-native; moved from 20 |
| `HPSDRModel::ANAN_G2E` | 16 | `network.h:446 [v2.10.3.15]` |
| `HPSDRModel::LAST` | 17 | sentinel bump (was 16) |

## 6. File-by-file changes

### 6.1 [src/core/HpsdrModel.h](../../src/core/HpsdrModel.h)

- HPSDRHW enum: add `HermesC10 = 20`, move `Andromeda` to 21. Cite `// From Thetis network.h:425 [v2.10.3.15] //N1GP G2E added (HermesC10)`.
- HPSDRModel enum: add `ANAN_G2E = 16`, bump `LAST = 17`. Cite `// From Thetis network.h:446 [v2.10.3.15] //N1GP G2E added`.
- `boardForModel()`: add `case HPSDRModel::ANAN_G2E: return HPSDRHW::HermesC10;`.
- `displayName()`: add `case HPSDRModel::ANAN_G2E: return "ANAN-G2E";`.
- `boardCodeName()`: add `case HPSDRHW::HermesC10: return "HermesC10";`.

### 6.2 [src/core/BoardCapabilities.h](../../src/core/BoardCapabilities.h) + [.cpp](../../src/core/BoardCapabilities.cpp)

#### 6.2.1 Add two new capability fields to the struct

Source: Thetis `clsHardwareSpecific.cs:245-264 [v2.10.3.15]`. Add to `BoardCapabilities`:

```cpp
bool hasPaVoltsTelemetry = false;  // From Thetis HasVolts (clsHardwareSpecific.cs:245-254 [v2.10.3.15])
bool hasPaAmpsTelemetry  = false;  // From Thetis HasAmps  (clsHardwareSpecific.cs:255-264 [v2.10.3.15])
```

Set to **true** for these existing rows: `kOrionMKII` (7000DLE/8000DLE), `kSaturn` (ANAN-G2/G2_1K), plus the new `kHermesC10` (G2E), and (if/when added) ANVELINAPRO3 and REDPITAYA. Set to **false** for: `kAtlas`, `kHermes`, `kHermesII`, `kAngelia`, `kOrion`, `kHermesLite`, `kHermesLiteRxOnly`, `kSaturnMKII` (Thetis groups Saturn only, not SaturnMKII), `kAndromeda` (NereusSDR-native; defer mapping until real hardware), `kUnknown`.

For each existing row, the field-add carries a verbatim cite: `// HasVolts/HasAmps per Thetis clsHardwareSpecific.cs:245-264 [v2.10.3.15]`.

#### 6.2.2 New row: `kHermesC10`

Inserted near `kOrionMKII` for source-order grouping:

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
    .adcCount         = 1,                                  // SetRxADC(1) [v2.10.3.15]
    .maxReceivers     = 4,
    .sampleRates      = {48000, 96000, 192000, 0, 0, 0},
    .maxSampleRate    = 192000,
    .attenuator       = {0, 31, 1, true, 0x1F, 0x20, false}, // RX1 only; RX2 returns false from HasSteppedAttenuation (clsHardwareSpecific.cs:790-803)
    .preamp           = {true, false},                       // RX1 preamp present; RX2 preamp explicitly absent (console.cs:14835)
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasAlex2         = true,                                // SetMKIIBPF(1) (clsHardwareSpecific.cs:131)
    .hasRxBypassRelay = true,
    .rxOnlyAntennaCount = 3,
    .hasPureSignal    = true,                                // P1_rxcount=4 nddc=4 (console.cs:8388)
    .psDefaultPeak    = 0.2899,                              // P2 default
    .psSampleRate     = 192000,                              // cmaster.cs:424 ps_rate=192000
    .hasDiversityReceiver = false,                           // 1 ADC
    .hasStepAttenuatorCal = true,                            // RX1 stepped att present and calibratable
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = false,                               // chkApolloPresent.Enabled=false (setup.cs:19908)
    .hasAlex          = true,                                // chkAlexPresent.Checked=true (setup.cs:19905)
    .hasPennyLane     = true,
    .canDriveGanymede = false,                               // not an Andromeda console family
    .hasPaVoltsTelemetry = true,                             // HasVolts true (clsHardwareSpecific.cs:251)
    .hasPaAmpsTelemetry  = true,                             // HasAmps  true (clsHardwareSpecific.cs:261)
    .minFirmwareVersion = 0,
    .knownGoodFirmware  = 0,
    .p2PreampPerAdc   = false,                               // 1 ADC; per-ADC control not meaningful
    .displayName      = "ANAN-G2E",
    .sourceCitation   = "network.h:425, clsHardwareSpecific.cs:129-135 / 245-264 / 699-730 / 790-803, "
                        "console.cs:8388/14835/25007 [v2.10.3.15]; N1GP G2E added",
};
```

Register in `kTable` (becomes 13 entries).

### 6.3 [src/core/HardwareProfile.cpp](../../src/core/HardwareProfile.cpp) + [.h](../../src/core/HardwareProfile.h)

Per-board init values, mirroring Thetis `clsHardwareSpecific.cs:85-191 [v2.10.3.15]`. NereusSDR's HardwareProfile already carries `mkiiBpf`, `adcSupplyVoltage`, `lrAudioSwap` fields populated per model (HardwareProfile.cpp:87-195). Two actions:

1. **Add HermesC10 / ANAN_G2E case** with the verbatim Thetis values:
   ```cpp
   case HPSDRModel::ANAN_G2E:                                // From Thetis clsHardwareSpecific.cs:129-135 [v2.10.3.15] //N1GP G2E added
       profile.adcCount         = 1;
       profile.mkiiBpf          = true;                       // SetMKIIBPF(1)
       profile.adcSupplyVoltage = 33;                         // SetADCSupply(0, 33)
       profile.lrAudioSwap      = false;                      // LRAudioSwap(0)
       profile.effectiveBoard   = HPSDRHW::HermesC10;
       break;
   ```

2. **Verify every existing case matches Thetis** — cross-check the full table from `clsHardwareSpecific.cs:87-191 [v2.10.3.15]`:

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

Where NereusSDR's existing HardwareProfile values diverge from this table, fix to match Thetis (with cite `[v2.10.3.15]`). Where they already match, no change; verify by test (§6.14).

### 6.4 [src/core/codec/CodecContext.h](../../src/core/codec/CodecContext.h) + emission audit

#### 6.4.1 `mkiiBpf` (verify only — already wired)

`P2CodecOrionMkII::buildAlex0()` ([P2CodecOrionMkII.cpp:439-449](../../src/core/codec/P2CodecOrionMkII.cpp:439)) already consumes `ctx.mkiiBpf` to split RX-out routing between bit 11 (EXT2/TX) and bit 14 (RX-only during RX). Verify HermesC10 gets `mkiiBpf=true` propagated through `buildCodecContext()` ([P2RadioConnection.cpp:1881](../../src/core/P2RadioConnection.cpp:1881) `ctx.mkiiBpf = m_hardwareProfile.mkiiBpf`). No code change beyond §6.3.

#### 6.4.2 `adcSupplyVoltage` — close the gap

`HardwareProfile.adcSupplyVoltage` is populated per model but never emitted on the wire. Per Thetis `cmaster.SetADCSupply(0, supplyVolts)` ([clsHardwareSpecific.cs:131](#) for G2E; the call resolves into the ChannelMaster DLL's outgoing P2 frame). Action:

1. **Audit the wire byte.** Open `/Users/j.j.boyd/Thetis/Project Files/Source/ChannelMaster/` for the C source that consumes `SetADCSupply` and emits the wire byte. Identify the byte position in the P2 CmdGeneral or per-RX command frame.
2. **Add `int adcSupplyVoltage = 0;` to `CodecContext`** with default 0 = "do not set / use radio default".
3. **Populate from HardwareProfile** in `buildCodecContext()` for both P1 (`P1RadioConnection`) and P2 (`P2RadioConnection`).
4. **Emit** from the appropriate codec method — likely `P2CodecOrionMkII::composeCmdGeneral()` (per-connection one-shot via PA-control byte) or a new dedicated init frame. P1 emission may not be needed if ChannelMaster only invokes SetADCSupply for P2 boards (verify in step 1).
5. **Cite** every change: `// From Thetis cmaster.SetADCSupply(0, N) — clsHardwareSpecific.cs:<line> [v2.10.3.15]`.

#### 6.4.3 `lrAudioSwap` — close the gap

`HardwareProfile.lrAudioSwap` is populated per model but never emitted. Per Thetis `NetworkIO.LRAudioSwap(0 or 1)`. Action mirrors §6.4.2:

1. **Audit the wire byte.** `NetworkIO.cs` defines `LRAudioSwap` — identify its P1 C&C register / bank / bit position.
2. **Add `bool lrAudioSwap = false;` to `CodecContext`**.
3. **Populate from HardwareProfile** in P1 buildCodecContext (P1 only — `NetworkIO.*` is P1 namespace).
4. **Emit** from the appropriate P1 codec bank (likely `P1CodecStandard::composeCcForBank()` for whichever bank carries the swap bit).
5. **Cite**: `// From Thetis NetworkIO.LRAudioSwap(N) — clsHardwareSpecific.cs:<line> [v2.10.3.15]`.

#### 6.4.4 SetMKIIBPF P1 emission

Thetis sends `SetMKIIBPF(0 or 1)` for *both* P1 and P2 boards (per the table in §6.3, Hermes/HermesII boards send 0). NereusSDR's P2 codec consumes `ctx.mkiiBpf` but P1 codec emission is not visible in the audit. Action:

1. **Audit `NetworkIO.SetMKIIBPF`** to find the P1 C&C register/bit.
2. If P1 emission is needed (i.e., MKII BPF actually affects a P1 wire byte and not just a host-side config), wire it through `P1CodecStandard::composeCcForBank()` for the appropriate bank.
3. If MKII BPF is purely a runtime/host-side flag with no P1 wire impact, document the finding in HardwareProfile.h's `mkiiBpf` field comment and skip P1 emission.

### 6.5 [src/core/SkuUiProfile.h](../../src/core/SkuUiProfile.h) + [.cpp](../../src/core/SkuUiProfile.cpp)

#### 6.5.1 Extend struct with EXT button label overrides

Per Thetis `setup.cs:19928-19929 [v2.10.3.15]`, G2E re-labels `chkEXT2OutOnTx.Text` to "Rx BYPASS on Tx". The default for other SKUs is "Ext 2 on Tx" (and "Ext 1 on Tx" for EXT1). Extend `SkuUiProfile`:

```cpp
QString ext1OutOnTxLabel = QStringLiteral("Ext 1 on Tx");  // From Thetis setup.cs default
QString ext2OutOnTxLabel = QStringLiteral("Ext 2 on Tx");  // From Thetis setup.cs default
```

Every existing SKU case keeps the defaults (no behavior change). The ANAN_G2E case sets `ext2OutOnTxLabel` explicitly.

#### 6.5.2 Add G2E case

```cpp
case HPSDRModel::ANAN_G2E:
    // Thetis setup.cs:19904-19929 [v2.10.3.15] //N1GP G2E added
    p.hasExt1OutOnTx     = true;
    p.hasExt2OutOnTx     = true;
    p.hasRxOutOnTx       = false;
    p.hasRxBypassUi      = false;
    p.rxOnlyLabels       = {QStringLiteral("BYPS"),
                            QStringLiteral("EXT1"),
                            QStringLiteral("XVTR")};
    p.antennaTabLabel    = QStringLiteral("Ant/Filters");
    p.ext1OutOnTxLabel   = QStringLiteral("Ext 1 on Tx");                // setup.cs:19928
    p.ext2OutOnTxLabel   = QStringLiteral("Rx BYPASS on Tx");            // setup.cs:19929
    break;
```

#### 6.5.3 Wire consumer

Find the button widget that renders `chkEXT2OutOnTx` (per Explore audit: `src/gui/setup/hardware/AntennaAlexAntennaControlTab.cpp`) and replace the hard-coded label literal with `profile.ext2OutOnTxLabel`. Same for EXT1.

### 6.6 [src/core/RadioDiscovery.cpp:233](../../src/core/RadioDiscovery.cpp:233)

```cpp
// mapP1DeviceType: 0=Atlas, 1=Hermes, 2=HermesII, 4=Angelia, 5=Orion, 6=HermesLite,
//                  10=OrionMKII, 20=HermesC10 (ANAN-G2E)  //N1GP G2E added
case 20: out.boardType = HPSDRHW::HermesC10; break;  // From Thetis network.h:425 [v2.10.3.15] //N1GP G2E added
```

P2 path (`static_cast<HPSDRHW>(byte)`) needs no change — byte 20 will produce HermesC10 once the enum value lands.

### 6.7 [src/core/PaGainProfile.cpp](../../src/core/PaGainProfile.cpp)

Add ANAN_G2E case at the existing `kAnan7000dRow` group (line ~365):

```cpp
// ANAN7000D / ANAN_G2 / ANAN_G2E / ANVELINAPRO3 / REDPITAYA shared row.
// From Thetis clsHardwareSpecific.cs:699-730 [v2.10.3.15] //N1GP G2E added
case HPSDRModel::ANAN_G2E:
case HPSDRModel::ANAN_G2:
case HPSDRModel::ANAN7000D:
// (etc — verify existing case-grouping style)
    return lookupHfBand(kAnan7000dRow, band);
```

VHF rows: same group returns flat 63.1f per `clsHardwareSpecific.cs:730 [v2.10.3.15]`.

### 6.8 [src/core/PaTelemetryScaling.cpp](../../src/core/PaTelemetryScaling.cpp)

Per Explore audit, this file holds a per-model `PaFwdTriplet` (bridge_volt, refvoltage, adc_cal_offset) table. Add ANAN_G2E entries for both forward and reverse (RX-cal) paths:

```cpp
// From Thetis console.cs:25005-25015 [v2.10.3.15] //N1GP G2E added (forward power cal)
case HPSDRModel::ANAN_G2E:
case HPSDRModel::ANAN_G2:
case HPSDRModel::ANAN_G2_1K:
case HPSDRModel::ANAN7000D:
case HPSDRModel::ANVELINAPRO3:
case HPSDRModel::REDPITAYA:
    triplet.bridgeVolt    = (band == Band::B6M) ? 0.7f : 0.15f;
    triplet.refVoltage    = 5.0f;
    triplet.adcCalOffset  = 28;
    return triplet;
```

```cpp
// From Thetis console.cs:25079-25088 [v2.10.3.15] //N1GP G2E added (reverse/RX power cal)
case HPSDRModel::ANAN_G2E:
case HPSDRModel::ANAN_G2:
case HPSDRModel::ANAN_G2_1K:
case HPSDRModel::ANAN7000D:
case HPSDRModel::ANVELINAPRO3:
case HPSDRModel::REDPITAYA:
    triplet.bridgeVolt    = 0.12f;
    triplet.refVoltage    = 5.0f;
    triplet.adcCalOffset  = 32;
    return triplet;
```

(Adapt to actual function signatures in the file. Preserve existing case-grouping style.)

### 6.9 [src/gui/setup/PaSetupPages.cpp](../../src/gui/setup/PaSetupPages.cpp) — per-SKU PA UI gating

Per Thetis `setup.cs:19918-19920 [v2.10.3.15]` G2E hides `chkAutoPACalibrate` and shows `chkBypassANANPASettings`.

#### 6.9.1 chkAutoPACalibrate visibility

`PaSetupPages.cpp:605` already has the AutoCal checkbox. Extend `PaGainByBandPage::applyCapabilityVisibility(caps)` so the checkbox honors a new capability flag:

```cpp
// From Thetis setup.cs:19918 [v2.10.3.15] //N1GP G2E added — G2E hides chkAutoPACalibrate
m_autoCalibrateCheck->setVisible(caps.allowsAutoPaCalibrate);
```

Add `bool allowsAutoPaCalibrate = true;` to `BoardCapabilities` (default true), set to **false** for ANAN_G2E only. Per Thetis `setup.cs` global gating, AnvelinaPro3 and REDPITAYA may also hide the auto-cal UI — audit and set false for those rows too (preserving Thetis parity).

#### 6.9.2 chkBypassANANPASettings — NEW UI

NereusSDR has no equivalent today (Explore audit confirmed: "GAP — not implemented"). Per Thetis `setup.cs:19920 [v2.10.3.15] chkBypassANANPASettings.Visible=true`, G2E shows a "Bypass ANAN PA Settings" checkbox that lets the user fall back to firmware-default PA gains rather than NereusSDR's table.

Action:
1. **Add checkbox** to `PaGainByBandPage` near the AutoCal checkbox.
2. **Add `bool showsBypassPaSettingsUi = false;` to `BoardCapabilities`**, set true for ANAN_G2E + every other Thetis SKU where `chkBypassANANPASettings.Visible=true` (audit setup.cs for the full list — likely G2E only at v2.10.3.15, but verify).
3. **Wire visibility** via `applyCapabilityVisibility(caps)`.
4. **Wire toggle** to a new `TransmitModel::paSettingsBypass` Q_PROPERTY (bool, persisted per-MAC, default false). When true, the PA gain dispatch falls back to a `bypassPaGainsForBand(band)` fallthrough (which already exists in PaGainProfile.cpp:424 returning the HPSDRModel::FIRST row).
5. **Cite** each change with `// From Thetis setup.cs:19920 [v2.10.3.15] //N1GP G2E added`.

#### 6.9.3 labelATTOnTX / udATTOnTX visibility

NereusSDR's TransmitSetupPowerPaPage has `m_attOnTxSpin` ([tst_transmit_setup_power_pa_page.cpp](../../tests/tst_transmit_setup_power_pa_page.cpp)). Per Thetis `setup.cs:19924-19925 labelATTOnTX.Visible=true / udATTOnTX.Visible=true`, G2E shows the ATT-on-TX UI. Most modern SKUs do too. Audit the existing visibility gate; if it's universal in NereusSDR, no change needed. If it's per-SKU, ensure ANAN_G2E case sets `visible=true`.

### 6.10 PA telemetry display — gate per HasVolts/HasAmps

[PaSetupPages.h:689-697](../../src/gui/setup/PaSetupPages.h:689) `PaValuesPage` renders six MetricLabel children including PA voltage, PA current, PA temperature, ADC overflow, drive. Per Thetis `HasVolts` / `HasAmps` properties, the voltage/current MetricLabels should be hidden on boards lacking telemetry.

Action:
1. **Wire visibility** to the new capability flags:
   ```cpp
   m_paVoltsLabel->setVisible(caps.hasPaVoltsTelemetry);
   m_paAmpsLabel->setVisible(caps.hasPaAmpsTelemetry);
   ```
2. **Same gating** in any bottom-banner / status-bar telemetry display that shows PA volts or amps (audit `src/gui/MainWindow.cpp` and the bottom banner widgets).
3. **Cite** with `// From Thetis HasVolts / HasAmps (clsHardwareSpecific.cs:245-264 [v2.10.3.15])`.

### 6.11 console.cs branch port matrix

Each Thetis branch must land somewhere in NereusSDR. **Already covered** = the branch's behavior is dispatched by an existing capability flag in `kHermesC10` (or by codec selection); no new code beyond §6.1-6.10. **Needs explicit branch** = the implementer must add `case HPSDRModel::ANAN_G2E:` or `case HPSDRHW::HermesC10:` to a specific NereusSDR switch.

| Thetis branch | Behavior | NereusSDR landing | Status |
|---|---|---|---|
| `console.cs:6830 setAlex1HPF` | use OrionII/Saturn BPF1 algorithm | Codec layer — wherever NereusSDR groups `HPSDRHW::OrionMKII + HPSDRHW::Saturn` for BPF1 algorithm. Likely `P2CodecOrionMkII::buildAlex1()` or `P2CodecSaturn::buildAlex1()`. Add `HermesC10` to the group. | **Needs explicit branch** |
| `console.cs:8388 P1_rxcount=4 nddc=4` | 4-DDC, RX4 = PS feedback | `caps.maxReceivers=4`, `caps.hasPureSignal=true` already in kHermesC10 row | Covered |
| `console.cs:8612 RX1 attenuator tot switch` | 3-attenuator family routing | Codec / RadioConnection — wherever NereusSDR groups `HPSDRHW::Hermes + HermesII`. Add `HermesC10`. | **Needs explicit branch** |
| `console.cs:8705 RX1 output path tot switch` | 4-ADC routing (Hermes family) | Same — group with Hermes | **Needs explicit branch** |
| `console.cs:10037 RX1 Preamp legacy cal exclusion` | exclude G2E from old HPSDR_MINUS10 path | Already-modern preamp combo from kHermesC10 row (preamp combo items per §6.11 row below) | Covered |
| `console.cs:11012 RX1 step att max 61` | G2E excluded → max stays 31 | `caps.attenuator.maxDb=31` in kHermesC10 row | Covered |
| `console.cs:11038 RX1 step att application exclusion` | exclude G2E from legacy Alex att path | ADC-based step att path used (NereusSDR `StepAttenuatorController`) | Covered |
| `console.cs:11182 RX2 step att max 61` | G2E excluded → max stays 31 | RX2 has no stepped att at all (`HasSteppedAttenuation(2)==false`); UI hidden via caps | Covered |
| `console.cs:14835 RX2 preamp present FALSE` | G2E uniquely lacks RX2 preamp | `caps.preamp = {true, false}` + UI hides RX2 preamp combo | Covered (verify UI gates) |
| `console.cs:14873 TX meter modes Ref Pwr/SWR` | G2E joins capable group | Audit NereusSDR meter modes — likely a per-SKU TX meter list. If hardcoded, add G2E. | **Needs explicit branch** |
| `console.cs:15414 RX1 DDS freq (Hermes family)` | use VFOfreq(0,...) only | Codec — P1 codec emits per-DDC freq. Already correct since G2E adcCount=1 + maxReceivers=4 routes only DDC0/DDC1 | Covered |
| `console.cs:15449 RX2 DDS freq (Hermes family)` | use VFOfreq(1,...) | Same — DDC1 used for RX2 in P1 codec | Covered |
| `console.cs:19315 RX1 Alex att legacy exclusion` | exclude G2E | Same as :11038 | Covered |
| `console.cs:19491 RX2 step att command (positive)` | G2E joins capable group | Inverse of :11182; G2E has no RX2 step att, so this branch is moot. Verify NereusSDR's RX2 step att UI hides for `kHermesC10`. | Covered |
| `console.cs:22547 TX spectrum markers w/ Alex` | G2E joins capable group | `caps.hasAlex=true` already triggers TX spectrum markers in NereusSDR's SpectrumWidget | Covered |
| `console.cs:25007 TX power bridge cal` | G2E gets 0.15f / 5.0f / 28 | PaTelemetryScaling.cpp G2E case (§6.8) | **Covered via §6.8** |
| `console.cs:25081 RX power bridge cal` | G2E gets 0.12f / 5.0f / 32 | PaTelemetryScaling.cpp G2E case (§6.8) | **Covered via §6.8** |
| `console.cs:25807 RX2 preamp restore on band change` | G2E in capable group | Moot (G2E has no RX2 preamp). Verify the restore handler skips when `caps.preamp.present[1]==false`. | Covered |
| `console.cs:25833 RX2 preamp save on band change` | G2E in capable group | Same as :25807 | Covered |
| `console.cs:25862 P1 user I/O inhibit bit selection` | G2E reads bit[2] of C1 (newer position), grouped with 7000D/8000D/REDPITAYA | Codec / RadioConnection P1 path. Add `case HPSDRModel::ANAN_G2E:` (or `case HPSDRHW::HermesC10:`) to the bit-2 group. | **Needs explicit branch** |
| `console.cs:26006 TX exciter formula (OrionMKII)` | G2E uses computeOrionMkIIExciterPower | Audit NereusSDR's TX exciter computation — if it's per-SKU dispatched, add G2E to the OrionMKII group. If it's caps-flag driven, add a flag or set existing flag. | **Needs explicit branch** |
| `console.cs:27655 audio mix states (P1 USB 4+DDC)` | G2E joins HERMES + 4-DDC group | NereusSDR codec / ChannelMaster equivalent (cmaster.SetAAudioMixStates). If NereusSDR has a per-SKU audio-mix-state switch, add G2E. | **Needs explicit branch** (or verify covered by adcCount/maxReceivers) |
| `console.cs:27674 audio mix states (P2 ETH 2-DDC)` | G2E joins HERMES + 2-DDC group | Same — audit per-protocol audio mix dispatch | **Needs explicit branch** |
| `console.cs:31357 DDC dynamic switching on RX2 enable` | G2E in capable group | Codec / RadioConnection — RX2 enable triggers UpdateDDCs equivalent. If gated per-SKU, add G2E. Likely already handled via maxReceivers=4. | Covered (verify) |
| `console.cs:32572 VFO sub display split (HermesC10)` | G2E in Hermes display family | Per-board UI logic. Add `HPSDRHW::HermesC10` to the Hermes + HermesII group. | **Needs explicit branch** |
| `console.cs:32604 VFO sub display PS state (HermesC10)` | Same | Same — same Hermes group | **Needs explicit branch** |
| `console.cs:40874 RX1 preamp combo items (ANAN-100D)` | G2E joins ANAN-100D preamp group | `BoardCapsTable::preampItemsForBoard()` — add ANAN-100D items for HermesC10 | **Needs explicit branch** in preampItemsForBoard |
| `console.cs:53028 max_att 61 vs 31 (negative)` | G2E excluded → 31 | Same as :11012 | Covered |
| `console.cs:53090 RX1 spectrum analyzer mode` | G2E uses SA mode | Audit NereusSDR — if SA mode is per-SKU, add G2E. May already be implicit via caps.hasAlex2. | **Needs explicit branch** (or verify covered) |
| `console.cs:53249 RX2 spectrum analyzer mode` | Same | Same | **Needs explicit branch** (or verify covered) |

**Implementer task per each "Needs explicit branch" row:** grep NereusSDR for the family grouping (e.g., `HPSDRHW::Hermes` + `HPSDRHW::HermesII`), add `HermesC10` to the case list, preserve `//N1GP G2E added (HermesC10)` cite verbatim per the inline-comment preservation rule.

### 6.12 [src/core/cmaster](../../src/core/) — DDC routing arrays

Thetis `cmaster.cs:594-606, 685-707, 807-826, 885-907 [v2.10.3.15]` adds G2E to four DDC routing arrays (G2E reuses the HERMES / ANAN10 / ANAN100 4-DDC route maps with no G2E-specific values). NereusSDR's equivalent ChannelMaster routing is in the P1/P2 codec layer. For each of the four Thetis array touchpoints, verify NereusSDR's P1 codec emits the correct DDC routing when `caps.adcCount=1` + `caps.maxReceivers=4` (HermesC10's row). If a per-SKU switch exists, add `HPSDRModel::ANAN_G2E` to the HERMES grouping with cite.

### 6.13 Andromeda.cs gating clauses (orthogonal, NOT a G2E port item)

Thetis `Andromeda.cs:1235, 2446, 2496, 2591 [v2.10.3.15]` defines `AndromedaG2Enabled` — a user-toggleable runtime flag indicating the Andromeda G2 *console panel* (a separate front-panel hardware accessory) is attached. This flag gates encoder/pushbutton handlers; it does NOT key on `HPSDRModel.ANAN_G2E`.

**This PR does not port Andromeda console panel support.** The Andromeda G2 console is a separate physical accessory unrelated to the G2E radio model. Porting it requires a UI toggle, persistent state, and three handler integrations — a separate parity work item, tracked as a follow-up (not blocking G2E radio support).

### 6.14 Tests

#### Updates / pins

- [tests/tst_board_capabilities.cpp](../../tests/tst_board_capabilities.cpp):
  - Pin `HPSDRHW::Andromeda == 21`.
  - Pin `HPSDRHW::HermesC10 == 20`.
  - Pin `HPSDRModel::ANAN_G2E == 16`.
  - Update existing `case HPSDRModel.ANAN_G1: //N1GP G1 added` comment cite to `//N1GP G2E added` with `setup.cs:19904-19929 [v2.10.3.15]`.

#### New cases

- Discovery: P1 byte 20 → `HermesC10`.
- `BoardCapabilities::forBoard(HermesC10)` returns `kHermesC10` with: `hasPureSignal=true`, `hasDiversityReceiver=false`, `hasAlex2=true`, `canDriveGanymede=false`, `hasPaVoltsTelemetry=true`, `hasPaAmpsTelemetry=true`, `preamp={true, false}`, `attenuator.maxDb=31`, `adcCount=1`, `maxReceivers=4`.
- `boardForModel(ANAN_G2E) == HermesC10`; `displayName(ANAN_G2E) == "ANAN-G2E"`; `boardCodeName(HermesC10) == "HermesC10"`.
- `defaultPaGainsForBand(ANAN_G2E, Band::B20m) == 50.9f` and `Band::VHF0 == 63.1f` (smoke check).
- `PaTelemetryScaling::forwardTriplet(ANAN_G2E, Band::B20m).bridgeVolt == 0.15f`; `.bridgeVolt for B6M == 0.7f`; `.adcCalOffset == 28`.
- `PaTelemetryScaling::reverseTriplet(ANAN_G2E).bridgeVolt == 0.12f`; `.adcCalOffset == 32`.
- `skuUiProfileFor(ANAN_G2E).rxOnlyLabels == {"BYPS","EXT1","XVTR"}`.
- `skuUiProfileFor(ANAN_G2E).ext2OutOnTxLabel == "Rx BYPASS on Tx"`.
- HardwareProfile for ANAN_G2E: `mkiiBpf==true`, `adcSupplyVoltage==33`, `lrAudioSwap==false`, `effectiveBoard==HermesC10`.
- All HasVolts/HasAmps caps assertions for the 6 boards Thetis marks as `true` (G2/G2_1K/G2E/7000D/8000D/ANVELINAPRO3/REDPITAYA — where each row exists in NereusSDR).
- Capability `allowsAutoPaCalibrate==false` for ANAN_G2E.
- Capability `showsBypassPaSettingsUi==true` for ANAN_G2E.

#### Wire-byte emission tests (new)

- `P1CodecStandard::composeCcForBank()` emits the expected LR-swap bit when `ctx.lrAudioSwap` is set.
- `P2CodecOrionMkII::composeCmdGeneral()` (or whichever frame carries it) emits the expected ADC supply byte when `ctx.adcSupplyVoltage` is set.
- `P2CodecOrionMkII::buildAlex0()` continues to route bit 11/14 correctly for `ctx.mkiiBpf==true` (regression).

### 6.15 Provenance updates

[docs/attribution/THETIS-PROVENANCE.md](../../docs/attribution/THETIS-PROVENANCE.md):

- Bump cite version from `[v2.10.3.13]` to `[v2.10.3.15]` on every row whose file is touched by this PR (HpsdrModel.h, BoardCapabilities.{h,cpp}, HardwareProfile.{h,cpp}, SkuUiProfile.{h,cpp}, RadioDiscovery.cpp, PaGainProfile.cpp, PaTelemetryScaling.cpp, P1CodecStandard.cpp, P2CodecOrionMkII.cpp, CodecContext.h, PaSetupPages.cpp).
- Add "ANAN-G2E" to the "notes" column.

[CLAUDE.md](../../CLAUDE.md): bump the standing Thetis-version reference from `v2.10.3.13` to `v2.10.3.15`. Note in the "Modification history" of touched files that this is the first port that stamps `[v2.10.3.15]`.

## 7. Verification

### Unit (lands in this PR)

- ctest green across the touched suites.
- All enum value pins (21/20/16).
- Discovery byte 20 → HermesC10 round-trip.
- Every capability row assertion in §6.14.
- HardwareProfile per-board init values table from §6.3.
- Wire-byte emission tests from §6.14.

### Bench (deferred — gated on user receiving G2E hardware)

New file: `docs/architecture/anan-g2e-verification/README.md`. Twelve rows:

1. **Discovery** — radio appears in ConnectionPanel, displayName="ANAN-G2E", board badge="HermesC10".
2. **RX** — connect, hear audio, FFT renders, RX2 enables and tunes (Hermes-family VFO 0/1 routing).
3. **RX2 UI gating** — no RX2 preamp combo, no RX2 stepped-att slider visible on RxApplet.
4. **PureSignal** — PSForm opens, calc converges on a clean signal, peak indicator stable.
5. **PA gain table** — Setup → PA tab shows N1GP defaults; sliders match table in §3.
6. **PA telemetry display** — Setup → PA Values page shows volts + amps + temperature + drive (HasVolts=true, HasAmps=true gate).
7. **Antenna routing** — Ant/Filters tab shows BYPS/EXT1/XVTR labels; EXT2 button reads "Rx BYPASS on Tx"; AlexController routes 1/2/3 → wire correctly; band changes refresh routing.
8. **Auto-PA-Calibrate UI hidden** — `chkAutoPACalibrate` is not visible on G2E PA setup page.
9. **Bypass ANAN PA Settings UI visible** — `chkBypassANANPASettings` checkbox is visible; toggling it switches PA gain dispatch to the bypass row.
10. **ATT-on-TX UI visible** — `labelATTOnTX` + `udATTOnTX` render in the Transmit setup PA page.
11. **MKII BPF wire-routed correctly** — `ctx.mkiiBpf=true` flows through P2CodecOrionMkII::buildAlex0(); on-air, high-band Alex filters engage as expected.
12. **ADC supply + LR swap emission** — verify the new CodecContext fields emit correctly on the wire (Wireshark capture against actual G2E hardware comparing NereusSDR frame to Thetis baseline).

Matrix status: `Pending bench`.

## 8. PR shape

Single PR, unlabeled (not a numbered phase — a Thetis-parity board addition). Title:

> `feat(boards): ANAN-G2E (HermesC10) full Thetis-parity port (v2.10.3.15 by N1GP)`

Estimated diff: **~700-900 lines added, ~30-50 modified** across the following files:

- Enum + capability: `HpsdrModel.h`, `BoardCapabilities.{h,cpp}` (+ HasVolts/HasAmps fields applied to 13 existing rows + new HermesC10 row), `SkuUiProfile.{h,cpp}` (+ EXT1/EXT2 label fields)
- Hardware profile: `HardwareProfile.{cpp,h}` (HermesC10 case + verify-table)
- Codec layer: `CodecContext.h` (+ adcSupplyVoltage + lrAudioSwap fields), `P1CodecStandard.cpp` (LR-swap emission), `P2CodecOrionMkII.cpp` (ADC-supply emission), `P2CodecSaturn.cpp` (verify no regression), `P1RadioConnection.cpp` + `P2RadioConnection.cpp` (buildCodecContext populate)
- Discovery + PA + tests: `RadioDiscovery.cpp`, `PaGainProfile.cpp`, `PaTelemetryScaling.cpp`
- UI: `PaSetupPages.cpp` (per-SKU AutoCal hide + BypassANANPASettings checkbox), `AntennaAlexAntennaControlTab.cpp` (EXT button label override consumer), `PaValuesPage` (HasVolts/HasAmps gating)
- Console.cs branch ports: every "Needs explicit branch" row in §6.11 — likely scattered across `P1CodecStandard.cpp`, `P2CodecOrionMkII.cpp`, `RadioConnection.cpp`, `MainWindow.cpp`, applet files
- Tests: `tst_board_capabilities.cpp`, `tst_hardware_profile.cpp` (new or existing), `tst_pa_telemetry_scaling.cpp`, `tst_sku_ui_profile.cpp`, plus new wire-byte emission tests
- Docs: `THETIS-PROVENANCE.md`, `CLAUDE.md`, new `anan-g2e-verification/README.md`

Branches that touch multiple board rows (HasVolts/HasAmps fields, HardwareProfile verification) inevitably broaden the diff. Per the no-shortcuts directive, multi-board fixups land in this PR rather than being split.

## 9. Out of scope (deliberate, narrow)

- **Andromeda value-200 cleanup** — long-term NereusSDR namespace hygiene; separate follow-up.
- **G2-1.8 ("ANAN-G2 1.8") follow-on** — Thetis hasn't added a separate entry.
- **Andromeda G2 console panel parity** — `AndromedaG2Enabled` / `AndromedaCATEnabled` runtime flags are a separate accessory feature (front-panel hardware, not radio). Tracked as its own parity work item. See §6.13.
- **Bench verification on the actual G2E** — requires hardware. The 12-row matrix exists with status `Pending bench`.
- **Per-board PA telemetry calibration constants beyond the bridge/refvolt/offset triplet** — covered when those triplets land via §6.8; deeper per-band overrides are caps-row data and not part of this port.

## 10. Acceptance criteria

- All enum value pin tests pass (21 / 20 / 16).
- ctest green on every touched suite.
- `gh pr ready` (clean diff, GPG-signed commits).
- Every existing Thetis cite in touched files is verified against v2.10.3.15 (and bumped if it changed; preserved if it didn't).
- Every new `// From Thetis` cite carries `[v2.10.3.15]`.
- Every `//N1GP G2E added` (and `//N1GP G2E added (HermesC10)`, `//N1GP G2E added //DK1HLM`) tag preserved verbatim per the inline-comment preservation rule.
- THETIS-PROVENANCE.md rows reflect `[v2.10.3.15]` for every touched file.
- HasVolts/HasAmps capability flags populated for all 13 existing BoardCapabilities rows per the Thetis grouping (true for G2/G2_1K/7000D/8000D/ANVELINAPRO3/REDPITAYA + new HermesC10).
- ADC supply voltage + LR audio swap wire bytes emit correctly for every board (not just G2E) per the table in §6.3.
- All "Needs explicit branch" rows in §6.11 have NereusSDR-side code added with the `HermesC10` or `ANAN_G2E` case.
- Andromeda's relocation comment in HpsdrModel.h documents the move and references this design doc.
- A future Thetis sync (`git -C ../Thetis pull`) does not require any rework of the kHermesC10 row.
- Bench verification matrix exists at `docs/architecture/anan-g2e-verification/README.md` with rows marked `Pending bench`.

## 11. References

- Thetis v2.10.3.15: `/Users/j.j.boyd/Thetis`, tag `v2.10.3.15`, commit `3759d096`.
- Upstream commits of interest: `8cef87c2 G1 modifications by Rick N1GP`, `3759d096 v2.10.3.15 - g2e`.
- ANAN-G2E ReleaseNotes: `ReleaseNotes.txt:23` (rename), `:42` (model add).
- Per-board init switch (authoritative): `clsHardwareSpecific.cs:85-191 [v2.10.3.15]`.
- HasVolts/HasAmps properties: `clsHardwareSpecific.cs:245-264 [v2.10.3.15]`.
- Full PA UI block for G2E: `setup.cs:19904-19929 [v2.10.3.15]`.
- 32-branch G2E console.cs map: §6.11 of this doc.
- Prior NereusSDR work informing the row shape: [antenna-routing-design.md](antenna-routing-design.md) §4.3 (SkuUiProfile), [phase3p-i-b-rxonly-xvtr-sku-plan.md](phase3p-i-b-rxonly-xvtr-sku-plan.md) (RX-only labels), [phase3m-0-pa-safety-foundation-plan.md](phase3m-0-pa-safety-foundation-plan.md) (Andromeda + canDriveGanymede rationale).
