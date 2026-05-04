# Phase 3M-3a-iii Design: DEXP/VOX Full Parameter Port

**Status:** Approved (brainstorm 2026-05-03), ready for `superpowers:writing-plans`.
**Author:** J.J. Boyd (KG4VCF), AI-assisted (Anthropic Claude Code, Opus 4.7).
**Date:** 2026-05-03.
**Upstream stamps captured this session:**
- Thetis `v2.10.3.13` (`@501e3f51`)
- mi0bot-Thetis `v2.10.3.13-beta2` (`@c26a8a4`)
- AetherSDR `@0cd4559`
**Master design alignment:** Implements §7.3 of `phase3m-tx-epic-master-design.md`, with one scope refinement (TX AMSQ moved to 3M-3b after Thetis archaeology found Thetis itself does not wire `SetTXAAMSQ*`; see §17 Deferred / Out of Scope).

---

## 1. Goal

Complete the WDSP DEXP downward-expander port begun in 3M-1b, exposing all 14 Thetis DEXP setters (3 already shipped, 11 to add) plus the live VOX/DEXP peak-meter visualization that Thetis presents on its main console. Wire the existing un-wired PhoneCwApplet VOX + DEXP stubs to real model state, drop the duplicate VOX surface from TxApplet, and add a Thetis-faithful Setup → Transmit → DEXP/VOX page (`tpDSPVOXDE` equivalent) for the full parameter set.

The user-visible result: an operator with VOX engaged sees their live mic peak as a green strip, the threshold as a red marker, and can tweak Threshold + Hold from the applet; full envelope tuning (Attack / Release / DetTau / Hyst / ExpRatio / side-channel filter / look-ahead) lives one right-click away in the Setup page.

## 2. Scope

### In scope

- 11 new TxChannel WDSP wrappers for the remaining DEXP setters (envelope, ratios, side-channel filter, audio look-ahead).
- 2 new TxChannel meter readers for live peak data (`GetDEXPPeakSignal` for VOX, `CalculateTXMeter(MIC)` for DEXP).
- TransmitModel schema additions: `dexpEnabled` plus 11 numeric DEXP properties, all with per-MAC AppSettings persistence and `voxGainScalar`-style change signals.
- MicProfileManager bundle additions for all new keys (estimated +14 keys, taking baseline from 94 to ~108).
- PhoneCwApplet wiring: VOX + DEXP rows promoted from un-wired stubs to fully-bound controls; new `DexpPeakMeter` widget; 100 ms polling timer for live data.
- TxApplet cleanup: remove the duplicate VOX button + popup wiring; delete `VoxSettingsPopup.{h,cpp}`.
- New Setup page `DexpVoxPage` (Setup → Transmit → DEXP/VOX) with two group boxes matching Thetis `tpDSPVOXDE`: `grpDEXPVOX` (8 numeric controls + 2 enables) and `grpDEXPLookAhead` (1 enable + 1 numeric).
- Right-click on either applet button emits `openSetupRequested("Transmit", "DEXP/VOX")` signal that MainWindow already routes to the Setup dialog (existing pattern from `SpeechProcessorPage`).
- Doc-refresh commit (already partially staged in working tree): CLAUDE.md current-phase paragraph, master design §7.2 (ParametricEq complete), master design §7.3 (this section's spec to be revised).

### Out of scope (deferred)

- TX AMSQ (`SetTXAAMSQRun/MutedGain/Threshold`). Thetis does not wire these in stock, and per the source-first directive ("if Thetis has it we want it... we are porting this application") we do not invent the surface. Folded into 3M-3b alongside other AM-mode TX work.
- (Originally listed: SCF UI. **Pulled in-scope mid-port 2026-05-03** after Batch B agent's source-first read found `grpSCF` on tpDSPVOXDE. See §3.2 grpSCF row, §17, and Task 14 in the implementation plan.)
- Anti-VOX deep UI (multi-source selector, `cmaster.CMSetAntiVoxSourceWhat`). Already deferred to 3M-3c per master design §7.5.
- Pre-emphasis (`emph.c` family). Already deferred to 3M-3b per master design §7.4.

## 3. Source-of-Truth Mapping

Every Thetis cite in this document uses `[v2.10.3.13]`. The tag is 7 commits behind current Thetis HEAD (`@501e3f51`); none of those 7 commits touched DEXP/VOX based on `git -C ../Thetis log v2.10.3.13..@501e3f51 -- '*amsq*' '*dexp*' setup.cs setup.Designer.cs console.cs cmaster.cs` returning empty. If that changes during the port, refresh the stamp to `[@501e3f51]` and document the diff.

### 3.1 WDSP setters (`cmaster.cs` DllImports)

| WDSP setter | C# import line | Range / units | Thetis default | Already wrapped? |
|---|---|---|---|---|
| `SetDEXPRun` | `cmaster.cs:166-167` | `bool` | unchecked | NO (new) |
| `SetDEXPRunVox` | `cmaster.cs:199-200` | `bool` | unchecked | YES (3M-1b: `setVoxRun`) |
| `SetDEXPDetectorTau` | `cmaster.cs:169-170` | `double` seconds (ms/1000) | 20 ms | NO (new) |
| `SetDEXPAttackTime` | `cmaster.cs:172-173` | `double` seconds | 2 ms | NO (new) |
| `SetDEXPReleaseTime` | `cmaster.cs:175-176` | `double` seconds | 100 ms | NO (new) |
| `SetDEXPHoldTime` | `cmaster.cs:178-179` | `double` seconds | 500 ms | YES (3M-1b: `setVoxHangTime`) |
| `SetDEXPExpansionRatio` | `cmaster.cs:181-182` | `double` linear (`Math.Pow(10, dB/20)`) | 1.0 dB raw → 1.122 linear | NO (new) |
| `SetDEXPHysteresisRatio` | `cmaster.cs:184-185` | `double` linear (`Math.Pow(10, dB/20)`) | 2.0 dB raw → 1.259 linear | NO (new) |
| `SetDEXPAttackThreshold` | `cmaster.cs:187-188` | `double` linear (`Math.Pow(10, dB/20)`) | -20 dB raw → 0.1 linear | YES (3M-1b: `setVoxAttackThreshold`) |
| `SetDEXPLowCut` | `cmaster.cs:190-191` | `double` Hz | 500 (udSCFLowCut.Value, line 45240) | NO (new) |
| `SetDEXPHighCut` | `cmaster.cs:193-194` | `double` Hz | 1500 (udSCFHighCut.Value, line 45210) | NO (new) |
| `SetDEXPRunSideChannelFilter` | `cmaster.cs:196-197` | `bool` | **CHECKED** (chkSCFEnable.Checked, line 45250) | NO (new) |
| `SetDEXPRunAudioDelay` | `cmaster.cs:202-203` | `bool` | **CHECKED** (Thetis default true) | NO (new) |
| `SetDEXPAudioDelay` | `cmaster.cs:205-206` | `double` seconds (ms/1000) | 60 ms | NO (new) |

11 new wrappers + 2 already shipped + 1 reused (`SetDEXPHoldTime` reused for both VOX hang and DEXP hold; same WDSP param) = 14 setters covered. The dB→linear conversions use `Math.Pow(10, dB/20)` and live in `setup.cs` value-changed handlers, not in `cmaster.cs`. NereusSDR will replicate the conversion on the model→TxChannel boundary so wrapper signatures stay in physical units (dB / ms).

### 3.2 Setup form ranges (Thetis `setup.Designer.cs` → `tpDSPVOXDE`)

`grpDEXPVOX` group title: `"VOX / DEXP"` (`setup.Designer.cs:44843`).

| Control | Range | Default | Step | Decimals | Thetis label | Setter |
|---|---|---|---|---|---|---|
| `chkVOXEnable` | bool | unchecked | | | "Enable VOX" | `SetDEXPRunVox` |
| `chkDEXPEnable` | bool | unchecked | | | "DEXP" | `SetDEXPRun` |
| `udDEXPThreshold` | -80.0..0.0 dB | -20.0 dB | 1 | 1 | "Threshold (dBV)" | `CMSetTXAVoxThresh` |
| `udDEXPHysteresisRatio` | 0.0..10.0 dB | 2.0 dB | 0.1 | 1 | "Hyst.Ratio (dB)" | `SetDEXPHysteresisRatio` |
| `udDEXPExpansionRatio` | 0.0..30.0 dB | 1.0 dB | 0.1 | 1 | "Exp. Ratio (dB)" | `SetDEXPExpansionRatio` |
| `udDEXPAttack` | 2..100 ms | 2 ms | 1 | 0 | "Attack (ms)" | `SetDEXPAttackTime` |
| `udDEXPHold` | 1..2000 ms | 500 ms | 10 | 0 | "Hold (ms)" | `SetDEXPHoldTime` |
| `udDEXPRelease` | 2..1000 ms | 100 ms | 1 | 0 | "Release (ms)" | `SetDEXPReleaseTime` |
| `udDEXPDetTau` | 1..100 ms | 20 ms | 1 | 0 | "DetTau (ms)" | `SetDEXPDetectorTau` |

`grpDEXPLookAhead` group title: `"Audio LookAhead"` (`setup.Designer.cs:44763`).

| Control | Range | Default | Step | Thetis label | Setter |
|---|---|---|---|---|---|
| `chkDEXPLookAheadEnable` | bool | **CHECKED** | | "Enable" | `SetDEXPRunAudioDelay` |
| `udDEXPLookAhead` | 10..999 ms | 60 ms | 1 | "Look Ahead (ms)" | `SetDEXPAudioDelay` |

`grpSCF` group title: `"Side-Channel Trigger Filter"` (`setup.Designer.cs:45165`).

**Scope correction (2026-05-03 mid-implementation):** the original spec wrongly claimed Thetis had no SCF Setup UI. Source-first read by the Batch B implementer agent surfaced the actual `grpSCF` group on `tpDSPVOXDE` (third group, alongside grpDEXPVOX + grpDEXPLookAhead). Per the source-first directive ("if Thetis has it we want it"), SCF is in scope.

| Control | Range | Default | Step | Thetis label | Setter |
|---|---|---|---|---|---|
| `chkSCFEnable` | bool | **CHECKED** | | "Enable" | `SetDEXPRunSideChannelFilter` |
| `udSCFLowCut` | 100..10000 Hz | 500 Hz | 10 | "Low Cut (Hz)" | `SetDEXPLowCut` |
| `udSCFHighCut` | 100..10000 Hz | 1500 Hz | 10 | "High Cut (Hz)" | `SetDEXPHighCut` |

### 3.3 Main console controls (Thetis `console.cs` → `panelModeSpecificPhone`)

| Control | What it is | Wired to | Cite |
|---|---|---|---|
| `chkVOX` | VOX enable toggle | `SetupForm.VOXEnable` (= `chkVOXEnable.Checked`) | `console.cs:28843-28862` |
| `ptbVOX` | Threshold slider (-80..0 dB, default -20) | `SetupForm.VOXSens` (= `udDEXPThreshold.Value`) | `console.cs:28938-28947`, `setup.Designer.cs:6018-6019` |
| `picVOX` | Live peak meter using `cmaster.GetDEXPPeakSignal` | repaints every paint cycle | `console.cs:28949-28960` |
| `lblVOXVal` | Numeric value label for `ptbVOX` | text from `ptbVOX.Value.ToString()` | `console.cs:28940` |
| `chkNoiseGate` | DEXP enable (Thetis labels it "Noise Gate") | `SetupForm.NoiseGateEnabled` (= `chkDEXPEnable.Checked`) + `SetGeneralSetting(0, OtherButtonId.DEXP, ...)` | `console.cs:28874-28882` |
| `ptbNoiseGate` | DEXP threshold slider (DECORATIVE in stock Thetis) | scroll handler updates label only; never pushed to WDSP | `console.cs:28962-28970` |
| `picNoiseGate` | Live peak meter using `WDSP.CalculateTXMeter(MIC)`, 100 ms polling | repaints from `UpdateNoiseGate` async loop | `console.cs:25346-25359`, `:28972-28981` |

The `ptbNoiseGate` decorative-only behavior is faithfully replicated. See §6.2 for the inline-comment treatment.

## 4. TxChannel WDSP Wrappers

All wrappers follow the existing 3M-1a/b pattern: physical-unit signature (dB or ms), `m_*Last` cache for idempotent guards, NaN-initialized to ensure first call always fires, range clamp at the wrapper boundary, byte-for-byte Thetis cite in the Doxygen header.

### 4.1 New DEXP setters (11 functions)

```cpp
// All wrappers live in src/core/TxChannel.h (declarations) and TxChannel.cpp (impls).
// Each declaration carries:
//   /// One-line purpose.
//   ///
//   /// From Thetis cmaster.cs:NNN-MMM [v2.10.3.13] - Set<Name> DLL import.
//   /// WDSP impl: wdsp/dexp.c (function name).
//   /// Thetis call-site: setup.cs:NNN [v2.10.3.13] - <handler name>.
//   /// Range: <lo>..<hi> <unit>.  Default: <thetis default>.

void setDexpRun(bool run);                          // SetDEXPRun
void setDexpDetectorTau(double tauMs);              // SetDEXPDetectorTau (ms/1000 -> seconds)
void setDexpAttackTime(double attackMs);            // SetDEXPAttackTime
void setDexpReleaseTime(double releaseMs);          // SetDEXPReleaseTime
void setDexpExpansionRatio(double ratioDb);         // SetDEXPExpansionRatio (dB -> std::pow(10, dB/20.0))
void setDexpHysteresisRatio(double ratioDb);        // SetDEXPHysteresisRatio (dB -> std::pow(10, -dB/20.0); NEGATIVE exponent per setup.cs:18924 + wdsp/dexp.c:533-534)
void setDexpLowCut(double hz);                      // SetDEXPLowCut
void setDexpHighCut(double hz);                     // SetDEXPHighCut
void setDexpRunSideChannelFilter(bool run);         // SetDEXPRunSideChannelFilter
void setDexpRunAudioDelay(bool run);                // SetDEXPRunAudioDelay
void setDexpAudioDelay(double delayMs);             // SetDEXPAudioDelay (ms/1000 -> seconds)
```

Test accessors (`lastDexp*ForTest()`) follow the 3M-1a pattern. Ranges and clamps applied per §3.2 table.

### 4.2 New meter readers (2 functions)

```cpp
/// Live VOX peak signal in linear amplitude (0.0..1.0).
/// From Thetis cmaster.cs:163-164 [v2.10.3.13] - GetDEXPPeakSignal DLL import.
/// WDSP impl: wdsp/dexp.c (GetDEXPPeakSignal).
/// Caller (console.cs:28952): cmaster.GetDEXPPeakSignal(0, &audio_peak);
double getDexpPeakSignal() const;

/// Live TX mic-meter reading in dB (negative; 0 dB = full scale).
/// From Thetis WDSP.cs - CalculateTXMeter(int channel, MeterType type).
/// Caller (console.cs:25353): WDSP.CalculateTXMeter(1, WDSP.MeterType.MIC);
/// Returns the negative of the WDSP value (Thetis: noise_gate_data = -CalculateTXMeter + 3.0f offset).
double getTxMicMeterDb() const;
```

### 4.3 Reuse from 3M-1b (no changes)

```cpp
void setVoxRun(bool run);                           // SetDEXPRunVox - keep existing
void setVoxAttackThreshold(double thresh);          // SetDEXPAttackThreshold - keep existing
void setVoxHangTime(double seconds);                // SetDEXPHoldTime - keep existing
```

The 3M-1b naming (`setVox*`) stays for backward compatibility with persisted AppSettings keys. New naming (`setDexp*`) is for the new wrappers only. Comments in the header document the param-overlap (e.g., `setVoxAttackThreshold` IS `SetDEXPAttackThreshold`; both VOX and DEXP read the same threshold value).

### 4.4 Side-channel filter wrappers (no UI)

`setDexpLowCut`, `setDexpHighCut`, `setDexpRunSideChannelFilter` are added for full WDSP coverage but call sites in NereusSDR are limited to `RadioModel` push-on-connect only (so values stay at our chosen ship defaults, which match Thetis WDSP defaults: low-cut and high-cut not engaged, SideChannelFilter off). No model property surfaces these to users.

## 5. TransmitModel Schema

### 5.1 New properties

All properties follow the existing `voxThresholdDb` / `voxHangTimeMs` 3M-1b pattern: Q_PROPERTY-style getter/setter, `m_*` member, `*Changed(value)` signal, AppSettings auto-persist via `persistOne()`, range-clamped in setter, idempotent guard.

```cpp
// src/models/TransmitModel.h additions
bool   dexpEnabled() const noexcept;                            // SetDEXPRun
double dexpDetectorTauMs() const noexcept;                      // 1..100, default 20
double dexpAttackTimeMs() const noexcept;                       // 2..100, default 2
double dexpReleaseTimeMs() const noexcept;                      // 2..1000, default 100
double dexpExpansionRatioDb() const noexcept;                   // 0.0..30.0, default 1.0
double dexpHysteresisRatioDb() const noexcept;                  // 0.0..10.0, default 2.0
bool   dexpLookAheadEnabled() const noexcept;                   // default TRUE (Thetis default)
double dexpLookAheadMs() const noexcept;                        // 10..999, default 60
double dexpLowCutHz() const noexcept;                           // (no UI; ships at WDSP default)
double dexpHighCutHz() const noexcept;                          // (no UI)
bool   dexpSideChannelFilterEnabled() const noexcept;           // (no UI)
```

### 5.2 Persistence policy

| Property | Persists across launches? | Reason |
|---|---|---|
| `voxEnabled` | NO (existing carve-out) | 3M-1b safety: VOX always loads OFF to prevent waking up keying on background noise |
| `dexpEnabled` | YES | DEXP cannot accidentally key TX (it only gates audio that is already keyed); no safety hazard. Matches Thetis behavior (Thetis persists via DB.SaveVarsDictionary) |
| `dexpLookAheadEnabled` | YES | Default ON per Thetis; matches Thetis-faithful policy |
| All numeric DEXP params | YES | Operator-tuned settings should survive restart |
| `dexpLowCutHz` / `HighCutHz` / `SideChannelFilterEnabled` | YES | Persist at WDSP defaults; no user surface but values stick if a future PR exposes them |

AppSettings keys use the per-MAC prefix already established by 3M-1b: `radio/<mac>/tx/dexp.<key>`. Following the existing `radio/<mac>/tx/vox.<key>` pattern.

### 5.3 Signals + `RadioModel` routing

Following 3M-1b lines 707-708 (`voxGainScalarChanged → MoxController::setVoxGainScalar`), every new DEXP signal gets a `RadioModel` connect to the appropriate downstream consumer:

```cpp
// In RadioModel ctor (src/models/RadioModel.cpp):
connect(&m_transmitModel, &TransmitModel::dexpEnabledChanged,
        m_txChannel.get(), &TxChannel::setDexpRun);
connect(&m_transmitModel, &TransmitModel::dexpDetectorTauMsChanged,
        m_txChannel.get(), &TxChannel::setDexpDetectorTau);
// ... 9 more connects, one per new property
```

Push-on-connect: when the radio connects, `RadioModel::pushTxStateToWdsp()` calls every `set*` on TxChannel from the current model state. New DEXP properties join that list.

## 6. UI: PhoneCwApplet wiring

### 6.1 Existing stub state (current)

PhoneCwApplet already has the VOX + DEXP rows laid out in `buildPhonePage()`:
- Row 10 (line 415-461): `VOX [ON]` button + level slider + delay slider + 2 inset value labels.
- Row 11 (line 463-490): `[DEXP]` button + level slider + 1 inset value label.
- Both rows have **zero `connect()` calls** in `wireControls()`. They are pure stubs from the 3-UI-Skeleton phase.
- Header member declarations: `m_voxBtn`, `m_voxSlider`, `m_voxLvlLabel`, `m_voxDlySlider`, `m_voxDlyLabel`, `m_dexpBtn`, `m_dexpSlider`, `m_dexpLabel`.

### 6.2 Proposed wiring (delta from stub)

- Row 10 layout reshape: stack `VOX [ON]` button + (Threshold slider above thin peak meter strip) + value label + (Hold slider) + value label.
  - Replace `m_voxLvlLabel` "level" naming with "Threshold (dB)" (no Thetis cite in user string per `feedback_no_cites_in_user_strings`).
  - Replace `m_voxDlySlider` "delay" naming with "Hold (ms)" matching Thetis `lblDEXPHold.Text = "Hold (ms)"` ([setup.Designer.cs:45118](https://github.com/ramdor/Thetis)).
  - New `DexpPeakMeter* m_voxPeakMeter` member, placed in a `slider-stack` vertical layout with `m_voxSlider`.
  - `wireControls()` adds: button toggled → `TransmitModel::setVoxEnabled`; threshold slider valueChanged → `TransmitModel::setVoxThresholdDb`; hold slider valueChanged → `TransmitModel::setVoxHangTimeMs`. Bidirectional sync via `QSignalBlocker` in `syncFromModel()`.
  - Right-click on `m_voxBtn` (set `setContextMenuPolicy(Qt::CustomContextMenu)`) emits `openSetupRequested("Transmit", "DEXP/VOX")` signal which MainWindow forwards to SetupDialog (existing pattern from `SpeechProcessorPage`).

- Row 11 reshape: stack `[DEXP] [ON]` + (Threshold slider above peak meter strip) + value label.
  - `m_dexpSlider` is the **decorative** ptbNoiseGate equivalent. Inline comment must read:

    ```cpp
    // From Thetis console.cs:28962-28970 [v2.10.3.13] - ptbNoiseGate_Scroll.
    // FAITHFUL Thetis quirk: in stock Thetis the scroll handler ONLY updates
    // the label and grabs focus.  The slider value is NEVER pushed to WDSP.
    // It is read by picNoiseGate_Paint (console.cs:28974-28980) to draw the
    // threshold marker on the live mic peak meter.  We replicate that exact
    // behavior - the slider value updates m_dexpThresholdMarkerDb (a UI-only
    // value), is read by DexpPeakMeter to draw the marker line, and is
    // optionally persisted in AppSettings as a UI-state key.  Do NOT wire
    // this slider to TxChannel::setDexpAttackThreshold or any WDSP setter.
    ```

  - New `DexpPeakMeter* m_dexpPeakMeter` member.
  - Right-click on `m_dexpBtn` emits same `openSetupRequested` signal.

- Remove header member declarations for unused stub controls (none; all members get wired or repurposed in this PR. See §7 for what gets removed from TxApplet).

- Add a 100 ms `QTimer m_dexpMeterTimer` driving both peak meters. See §10.

### 6.3 New `openSetupRequested` signal

PhoneCwApplet adds:

```cpp
signals:
    /// Emitted when right-click on VOX or DEXP button requests jump to
    /// Setup -> Transmit -> DEXP/VOX page.  MainWindow connects this to
    /// SetupDialog::selectPage("Transmit", "DEXP/VOX").  Mirrors the
    /// existing SpeechProcessorPage::openSetupRequested signal pattern
    /// (TransmitSetupPages.h:201).
    void openSetupRequested(const QString& category, const QString& page);
```

MainWindow wires this in the existing applet-signal-connection block.

## 7. UI: TxApplet cleanup

Files / methods removed entirely:

- `src/gui/widgets/VoxSettingsPopup.h`
- `src/gui/widgets/VoxSettingsPopup.cpp`
- `TxApplet::m_voxBtn` member
- `TxApplet::showVoxSettingsPopup(const QPoint&)` method
- The VOX-button construction block in `TxApplet::buildUI()` (line 365-395, "Section 4b").
- The two `connect()` calls wiring `m_voxBtn` (lines 905-919).
- The `m_voxBtn` sync code in `TxApplet::syncFromModel()` (lines 1156-1158).
- `#include "gui/widgets/VoxSettingsPopup.h"` (line 155).

After cleanup, TxApplet's right-pane row sequence is: TUNE+MOX, MON, Mon Vol, LEV/EQ/CFC, gauges. The `[VOX]` button is gone from TxApplet entirely; the wired surface lives on PhoneCwApplet.

`CMakeLists.txt` removes the two VoxSettingsPopup source/header lines.

## 8. UI: Setup → Transmit → DEXP/VOX page

New `class DexpVoxPage : public SetupPage` in `src/gui/setup/TransmitSetupPages.{h,cpp}` (alongside the existing `SpeechProcessorPage`, `CfcSetupPage`, etc.).

### 8.1 Layout

Mirror Thetis `tpDSPVOXDE` 1:1 per the source-first directive. Two QGroupBox containers in a horizontal layout:

```
+-------------------------------------------+--------------------+
| VOX / DEXP                                | Audio LookAhead    |
| [x] Enable VOX        [x] DEXP            | [x] Enable         |
|                                           |  Look Ahead: [ ms] |
|  Attack:  [    ms]    Threshold: [ dBV]   |                    |
|  Hold:    [    ms]    Hyst:      [ dB]    |                    |
|  Release: [    ms]    ExpRatio:  [ dB]    |                    |
|  DetTau:  [    ms]                        |                    |
+-------------------------------------------+--------------------+
```

Group box titles match Thetis verbatim: `"VOX / DEXP"` and `"Audio LookAhead"`.
Spinbox labels match Thetis verbatim: `"Threshold (dBV)"`, `"Hyst.Ratio (dB)"`, `"Exp. Ratio (dB)"`, `"Attack (ms)"`, `"Hold (ms)"`, `"Release (ms)"`, `"DetTau (ms)"`, `"Look Ahead (ms)"`.

### 8.2 Controls and binding

| Widget | Binding | Range / step / decimals |
|---|---|---|
| `m_chkVOXEnable` (QCheckBox) | `TransmitModel::voxEnabled` | bool |
| `m_chkDEXPEnable` (QCheckBox) | `TransmitModel::dexpEnabled` | bool |
| `m_udDEXPThreshold` (QDoubleSpinBox) | `TransmitModel::voxThresholdDb` (existing) | -80.0..0.0, step 1, 1 decimal |
| `m_udDEXPHysteresisRatio` (QDoubleSpinBox) | `TransmitModel::dexpHysteresisRatioDb` | 0.0..10.0, step 0.1, 1 decimal |
| `m_udDEXPExpansionRatio` (QDoubleSpinBox) | `TransmitModel::dexpExpansionRatioDb` | 0.0..30.0, step 0.1, 1 decimal |
| `m_udDEXPAttack` (QSpinBox) | `TransmitModel::dexpAttackTimeMs` | 2..100, step 1 |
| `m_udDEXPHold` (QSpinBox) | `TransmitModel::voxHangTimeMs` (existing) | 1..2000, step 10 |
| `m_udDEXPRelease` (QSpinBox) | `TransmitModel::dexpReleaseTimeMs` | 2..1000, step 1 |
| `m_udDEXPDetTau` (QSpinBox) | `TransmitModel::dexpDetectorTauMs` | 1..100, step 1 |
| `m_chkDEXPLookAheadEnable` (QCheckBox) | `TransmitModel::dexpLookAheadEnabled` | bool, **default true** |
| `m_udDEXPLookAhead` (QSpinBox) | `TransmitModel::dexpLookAheadMs` | 10..999, step 1 |

Bidirectional sync uses the existing SetupPage pattern: `QSignalBlocker` on inbound model-changed signals; spinbox `valueChanged` calls `TransmitModel::set*`.

### 8.3 SetupDialog integration

`SetupDialog::buildPageTree()` adds a new leaf `"DEXP/VOX"` under category `"Transmit"`. The cross-link signal from PhoneCwApplet and SpeechProcessorPage routes through the existing `SetupDialog::selectPage(category, leaf)` API.

`SpeechProcessorPage::m_amSqDexpStatusLabel` (the existing stub at TransmitSetupPages.h:232) gets bound to live DEXP run-state for status display.

## 9. UI: DexpPeakMeter widget

New widget `src/gui/widgets/DexpPeakMeter.{h,cpp}`. NereusSDR-original (Thetis renders via Win32 Graphics PaintEvent; Qt6 needs a fresh implementation).

### 9.1 Public API

```cpp
class DexpPeakMeter : public QWidget {
    Q_OBJECT
public:
    explicit DexpPeakMeter(QWidget* parent = nullptr);

    /// Set the live signal level (linear 0.0..1.0 for VOX,
    /// or dB for DEXP - the source widget owns the meaning).
    /// Caller's responsibility to map domain to widget's scale.
    void setSignalLevel(double normalized01);

    /// Set the threshold marker position (0.0..1.0).
    void setThresholdMarker(double normalized01);

    QSize sizeHint() const override { return QSize(100, 4); }
    QSize minimumSizeHint() const override { return QSize(40, 4); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    double m_signal01    = 0.0;
    double m_threshold01 = 0.5;
};
```

### 9.2 Paint behavior

Following Thetis `picVOX_Paint` (`console.cs:28949-28960`) and `picNoiseGate_Paint` (`console.cs:28972-28981`):

- Background: `Style::kBgDarkest` (matches existing 4 px slider track color).
- Signal fill: lime-green (`#4cd048`) horizontal bar from left edge to `m_signal01 * width()`.
- Threshold marker: 1 px red (`#ff3a3a`) vertical line at `m_threshold01 * width()`.
- Above-threshold red overlay: from threshold position to signal position, draw the segment in red instead of green, matching `picVOX_Paint` lines 28958-28959.

### 9.3 Caller responsibility

PhoneCwApplet maps:
- VOX peak: `getDexpPeakSignal()` returns linear amplitude; convert via `20 * log10(peak)` clamped to -80..0 dB then map to 0..1.
- VOX threshold: `voxThresholdDb` already in -80..0 dB; map directly.
- DEXP peak: `getTxMicMeterDb()` returns dB (typically -160..0 range); map to 0..1 via `(dB + 160) / 160` matching Thetis (`console.cs:28974`).
- DEXP threshold: `m_dexpThresholdMarkerDb` (UI-only, see §6.2 inline-comment) using same `(dB + 160) / 160` mapping.

## 10. Peak meter polling pipeline

Following Thetis `UpdateNoiseGate` and `UpdateVOX` async loops (`console.cs:25347+`), but adapted to Qt6 patterns.

### 10.1 PhoneCwApplet timer

```cpp
// In PhoneCwApplet ctor:
m_dexpMeterTimer = new QTimer(this);
m_dexpMeterTimer->setInterval(100);   // matches Thetis Task.Delay(100)
connect(m_dexpMeterTimer, &QTimer::timeout, this, &PhoneCwApplet::pollDexpMeters);
m_dexpMeterTimer->start();

// pollDexpMeters() reads TxChannel meter wrappers, updates DexpPeakMeter
// widgets via setSignalLevel() / setThresholdMarker().  Cheap repaints (4 px tall).
```

The timer ticks unconditionally (on or off MOX). When not transmitting, `getDexpPeakSignal()` returns ~0 and `getTxMicMeterDb()` returns the noise floor; the meters show silent state. This matches Thetis behavior (the green bar dips to zero when `_mox == false`).

### 10.2 TxChannel meter implementation

Thread safety: `getDexpPeakSignal()` and `getTxMicMeterDb()` are READ-ONLY DLL imports that safely return immediately; both are documented as callable from non-DSP threads in WDSP. We invoke them directly from the GUI thread on the timer tick. No mutex needed.

### 10.3 Idle cost

Qt timer at 100 ms = 10 Hz. Each tick does 2 DLL imports + 2 small widget repaints. Negligible CPU. Matches Thetis cost.

## 11. Data flow

### 11.1 Threshold control: applet -> WDSP

```
[PhoneCwApplet VOX threshold slider drag]
    -> QSlider::valueChanged(int) lambda
       -> TransmitModel::setVoxThresholdDb(double dB)
          -> persistOne("vox.thresholdDb", dB)         (AppSettings)
          -> emit voxThresholdDbChanged(dB)
             -> TxChannel::setVoxAttackThreshold(linear)  (RadioModel auto-connect)
                -> SetDEXPAttackThreshold(0, linear)      (WDSP)

[Setup -> DEXP/VOX page udDEXPThreshold spinbox]
    -> Same path via TransmitModel::setVoxThresholdDb
       (Setup spinbox bidirectional with model just like applet slider)
```

Both UI surfaces edit the same model property. No feedback loops because `QSignalBlocker` guards in `syncFromModel()` prevent echoes.

### 11.2 Mic profile bundle round-trip

```
[Active profile changed]
    -> MicProfileManager::applyProfile(name)
       -> Read JSON blob "vox.thresholdDb", "dexp.attackTimeMs", ... (14 keys)
       -> TransmitModel::setVoxThresholdDb(...) ; setDexpAttackTimeMs(...) ; ...
          (model setters fire signals -> WDSP push as in §11.1)

[User adjusts slider with profile active]
    -> TransmitModel::voxThresholdDbChanged
       -> MicProfileManager::onModelChanged() (existing live-capture pattern from 3M-3a-ii G.2)
          -> Update in-memory profile blob; mark profile dirty
```

### 11.3 Peak meter polling

```
[QTimer m_dexpMeterTimer (100 ms)]
    -> PhoneCwApplet::pollDexpMeters()
       -> double voxPeak = m_radioModel->txChannel()->getDexpPeakSignal()
       -> double dexpDb  = m_radioModel->txChannel()->getTxMicMeterDb()
       -> m_voxPeakMeter->setSignalLevel(voxPeakNorm)
       -> m_dexpPeakMeter->setSignalLevel(dexpDbNorm)
       -> repaints (4 px tall)
```

## 12. Persistence + safety carve-outs

| Setting | Persist? | Default on first launch | Safety reason |
|---|---|---|---|
| `voxEnabled` | NO | OFF | 3M-1b carve-out: VOX must always start OFF |
| `dexpEnabled` | YES | OFF | DEXP gates audio, cannot accidentally key TX |
| `voxThresholdDb` | YES | -20 dB (Thetis default) | |
| `voxHangTimeMs` | YES | 500 ms (Thetis default) | |
| `voxGainScalar` | YES (existing) | 1.0 | NereusSDR-spin from 3M-1b, kept |
| `dexpDetectorTauMs` | YES | 20 ms | |
| `dexpAttackTimeMs` | YES | 2 ms | |
| `dexpReleaseTimeMs` | YES | 100 ms | |
| `dexpExpansionRatioDb` | YES | 1.0 dB | |
| `dexpHysteresisRatioDb` | YES | 2.0 dB | |
| `dexpLookAheadEnabled` | YES | **TRUE** (Thetis default) | |
| `dexpLookAheadMs` | YES | 60 ms | |
| Side-channel filter trio | YES | (WDSP defaults) | No UI; persists at unchanged values |

`dexpThresholdMarkerDb` (the UI-only marker for the decorative slider): persisted under `ui/phoneApplet/dexpMarkerDb`. Default -50 dB.

## 13. Source-first subagent dispatch plan

Per the new `feedback_subagent_thetis_source_first.md` rule, every implementer dispatch in this PR carries the Thetis context block + per-task explicit cites + STOP-AND-ASK clause + license/attribution preservation rules.

### 13.1 Dispatch batches (4 implementer agents)

Batches grouped to amortize Thetis-source context loads (each agent reads 2-3 Thetis files cold; grouping related wrappers reduces total reads).

**Batch A: TxChannel WDSP envelope/timing wrappers** (4 setters)
- `setDexpDetectorTau` from `cmaster.cs:169-170 [v2.10.3.13]`
- `setDexpAttackTime` from `cmaster.cs:172-173 [v2.10.3.13]`
- `setDexpReleaseTime` from `cmaster.cs:175-176 [v2.10.3.13]`
- `setDexpRun` from `cmaster.cs:166-167 [v2.10.3.13]`
- Each: READ cmaster.cs DllImport, READ wdsp/dexp.c impl for signature, READ setup.cs:18896-18920 for value-change handler conversion patterns, SHOW each in commit message before TRANSLATE.
- Plus tests `tst_tx_channel_dexp_envelope`.

**Batch B: TxChannel WDSP gate/ratio + side-channel + look-ahead wrappers** (7 setters)
- `setDexpExpansionRatio` from `cmaster.cs:181-182 [v2.10.3.13]` (with Math.Pow conversion)
- `setDexpHysteresisRatio` from `cmaster.cs:184-185 [v2.10.3.13]` (with Math.Pow conversion)
- `setDexpLowCut` from `cmaster.cs:190-191 [v2.10.3.13]`
- `setDexpHighCut` from `cmaster.cs:193-194 [v2.10.3.13]`
- `setDexpRunSideChannelFilter` from `cmaster.cs:196-197 [v2.10.3.13]`
- `setDexpRunAudioDelay` from `cmaster.cs:202-203 [v2.10.3.13]`
- `setDexpAudioDelay` from `cmaster.cs:205-206 [v2.10.3.13]`
- Plus 2 meter readers: `getDexpPeakSignal` (`cmaster.cs:163-164`), `getTxMicMeterDb` (WDSP.cs CalculateTXMeter).
- Tests: `tst_tx_channel_dexp_gate`, `tst_tx_channel_dexp_meters`.

**Batch C: TransmitModel + MicProfileManager + RadioModel routing**
- 11 new properties on TransmitModel with persistence + signals + clamps.
- MicProfileManager bundle additions for new keys (estimated baseline 94 -> 108).
- RadioModel auto-connect block additions (one per new property -> TxChannel).
- Tests: `tst_transmit_model_dexp`, `tst_mic_profile_dexp_round_trip`.

**Batch D: UI (PhoneCwApplet wiring + Setup page + DexpPeakMeter widget + TxApplet cleanup)**
- DexpPeakMeter widget creation + paint test.
- PhoneCwApplet VOX/DEXP rows wired (threshold/hold sliders, peak meter integration, right-click signal, polling timer).
- New `DexpVoxPage` SetupPage subclass with **14** controls in **3** group boxes matching Thetis 1:1: grpDEXPVOX (8 controls + 2 enables), grpDEXPLookAhead (1 enable + 1 numeric), grpSCF (1 enable + 2 numerics; added per scope correction §3.2).
- TxApplet cleanup (delete VoxSettingsPopup, drop button + popup wiring).
- SetupDialog tree leaf addition.
- Tests: `tst_dexp_peak_meter`, `tst_dexp_vox_setup_page`, `tst_phone_cw_applet_vox_dexp_wiring`.

### 13.2 Each dispatch prompt includes verbatim:

```
Thetis path: /Users/j.j.boyd/Thetis/Project Files/Source/
Thetis stamp: v2.10.3.13 (@501e3f51, 7 commits past tag, none touch DEXP)
Protocol: READ -> SHOW -> TRANSLATE per CLAUDE.md.

Per-task cite table:
  <wrapper name>:
    READ cmaster.cs:NNN-MMM [v2.10.3.13] for the DllImport
    READ wdsp/dexp.c <function name> for the C signature/range/defaults
    READ setup.cs:NNN [v2.10.3.13] for the value-change handler (unit conversion)
  Show each in commit message before translating.

STOP AND ASK if you cannot locate a Thetis source for any value, range,
default, behavior, or attribution.  Do not invent.  Do not infer.

License preservation: byte-for-byte headers if creating a new .cpp/.h.
Inline tag preservation: every //DH1KLM, //MW0LGE, //W2PA tag within +/-5
lines of a cited Thetis source line must be preserved verbatim within
+/-10 lines of the corresponding port line.

Self-review checklist: source quoted in commit; tags preserved; version
stamp on every cite; constants traceable; per-pre-commit-hook expectations.
```

## 14. PR shape and commit plan

Branch: continue on the current `claude/recursing-shirley-8706c2` worktree branch (rename to `feature/phase3m-3a-iii-dexp-vox` before opening the PR).

Estimated commit count: ~14-16 GPG-signed commits.

| # | Commit | Files |
|---|---|---|
| 1 | docs: refresh CLAUDE.md current-phase + master design §7.2/§7.3 + (optional) HANDOFF cleanup | CLAUDE.md, master design, HANDOFF*.md |
| 2 | docs: add 3M-3a-iii design + plan | this file + writing-plans output |
| 3 | feat(tx-channel): DEXP envelope/timing wrappers (Batch A, 4 setters) | TxChannel.{h,cpp} |
| 4 | test(tx-channel): tst_tx_channel_dexp_envelope | new test |
| 5 | feat(tx-channel): DEXP gate/ratio/side-ch/look-ahead wrappers (Batch B, 7 setters) | TxChannel.{h,cpp} |
| 6 | feat(tx-channel): DEXP peak signal + TX mic meter readers | TxChannel.{h,cpp} |
| 7 | test(tx-channel): tst_tx_channel_dexp_gate + dexp_meters | new tests |
| 8 | feat(transmit-model): 11 new DEXP properties + persistence | TransmitModel.{h,cpp} |
| 9 | feat(profile): MicProfileManager bundle DEXP keys | MicProfileManager.{h,cpp} |
| 10 | feat(model): RadioModel TM->TxChannel routing for DEXP setters | RadioModel.cpp |
| 11 | test(transmit-model): tst_transmit_model_dexp + tst_mic_profile_dexp_round_trip | new tests |
| 12 | feat(widget): DexpPeakMeter widget + paint test | new widget |
| 13 | feat(setup): DexpVoxPage with grpDEXPVOX + grpDEXPLookAhead groups | TransmitSetupPages.{h,cpp}, SetupDialog |
| 14 | feat(applet): PhoneCwApplet wires VOX + DEXP stubs to model + peak meters + right-click->Setup | PhoneCwApplet.{h,cpp} |
| 15 | refactor(tx-applet): drop duplicate VOX button, delete VoxSettingsPopup | TxApplet.{h,cpp}, VoxSettingsPopup.{h,cpp} (delete), CMakeLists.txt |
| 16 | test(applet): tst_phone_cw_applet_vox_dexp_wiring + tst_dexp_vox_setup_page | new tests |

Tests run after each commit per `feedback_minimize_test_invocations` (TDD: only the new test, plus pre-commit hooks). Full ctest sweep at PR open.

## 15. Testing plan

New ctest executables:

| Test | Verifies |
|---|---|
| `tst_tx_channel_dexp_envelope` | 4 envelope setters: clamping, idempotency, ms->seconds conversion |
| `tst_tx_channel_dexp_gate` | 7 gate/ratio/filter/look-ahead setters: clamping, idempotency, dB->linear conversion for ratio setters |
| `tst_tx_channel_dexp_meters` | 2 meter readers: callable without crash, return sensible values when DSP not running |
| `tst_transmit_model_dexp` | 11 new TM properties: getter/setter, change signal emission, persistence round-trip via AppSettings |
| `tst_mic_profile_dexp_round_trip` | MicProfileManager bundles + restores all 14 new keys, baseline 94 -> 108 |
| `tst_dexp_peak_meter` | Widget paint: signal bar width, threshold marker position, above-threshold red overlay |
| `tst_dexp_vox_setup_page` | Setup page builds with 11 controls + 2 group boxes, bidirectional sync with TM |
| `tst_phone_cw_applet_vox_dexp_wiring` | Applet rows wire to TM properties; right-click emits openSetupRequested |

All existing tests must continue to pass.

## 16. Bench verification matrix (manual, ANAN-G2)

To be executed by JJ after PR merges, before tagging release. Pattern matches 3M-3a-i / 3M-3a-ii "land then bench."

1. Connect to ANAN-G2, select Phone mode SSB, activate VOX [ON].
2. Verify peak meter shows live mic level when speaking, idle when silent.
3. Adjust Threshold slider; verify red marker on peak meter moves.
4. Speak: verify VOX keys TX when peak crosses red marker.
5. Verify Hold slider extends post-speech keying period.
6. Right-click VOX [ON]; verify Setup -> DEXP/VOX page opens.
7. Adjust Attack / Release / DetTau / Hyst / ExpRatio in Setup; verify changes affect VOX behavior audibly.
8. Toggle DEXP [ON] (audio noise gate); verify silence between syllables gets gated.
9. Adjust DEXP threshold marker (decorative slider); verify red marker moves on its peak meter but threshold behavior unchanged (matches Thetis quirk).
10. Toggle Audio LookAhead in Setup off then on; verify VOX timing changes (with look-ahead, VOX engages before first syllable; without, slight clipping at start).
11. Save current settings as a mic profile; switch profile; switch back; verify all DEXP/VOX settings restore.
12. Restart NereusSDR; verify all DEXP settings persist EXCEPT `voxEnabled` (must load OFF).

## 17. Deferred / out of scope

| Item | Why deferred | Where it lands |
|---|---|---|
| TX AMSQ (`SetTXAAMSQRun/MutedGain/Threshold`) | Thetis does not wire any of these (grep confirms zero hits across cmaster.cs/dsp.cs/radio.cs/setup.cs); per source-first directive, we do not invent UI Thetis lacks | 3M-3b (mode-specific TX, AM/SAM/DSB context) |
| Pre-emphasis | Already deferred from 3M-3a-ii per master design §7.4 | 3M-3b |
| Anti-VOX deep UI (multi-source selector) | Already deferred per master design §7.5 | 3M-3c |
| ~~Side-channel filter UI~~ | **PULLED IN-SCOPE 2026-05-03 mid-implementation:** Thetis DOES ship grpSCF on tpDSPVOXDE (setup.Designer.cs:45157+ [v2.10.3.13]). Original spec wrongly claimed otherwise; Batch B agent caught this via source-first read. Now bound in Task 14. | This PR |
| HANDOFF.md / HANDOFF-PLAN4.md cleanup | Not strictly required for 3M-3a-iii; folded into the doc-refresh commit if JJ confirms | Doc-refresh commit (this PR) |

## 18. Risks and open items

| Risk | Mitigation |
|---|---|
| Decorative ptbNoiseGate slider confuses users (drags do nothing audible) | Inline source comment explains the Thetis quirk; tooltip on the slider says "Threshold marker (display)" not "Threshold". Alternative is to wire it to the same `voxThresholdDb` as the VOX slider, but that creates two-sliders-edit-one-value confusion. Decision: faithful Thetis port, document the quirk |
| `voxThresholdDb` is shared between VOX and DEXP at the WDSP level (single `SetDEXPAttackThreshold`); naming in the model implies VOX-only | Keep existing `voxThresholdDb` model property name (3M-1b backward-compat for AppSettings keys). Comment in TransmitModel.h documents that this single param controls both VOX-keying and DEXP-gating thresholds in WDSP |
| Bench-verifying 14 settings interaction is finicky; some DEXP timing combinations may sound bad | Defaults track Thetis exactly; if Thetis defaults sound good there, they should sound good here. Bench matrix §16 walks through |
| Peak meter timer at 100 Hz could add idle CPU on slow systems | 100 ms is exactly the Thetis cadence; if it shows up in profiles we can debounce when not transmitting |
| MicProfileManager bundle key count growing ~108 (from 94); test baseline updates needed | `tst_mic_profile_round_trip` baseline gets updated in the same commit that adds the keys |

---

**End of design.**
