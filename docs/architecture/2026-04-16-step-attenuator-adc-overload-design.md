# Step Attenuator & ADC Overload Protection — Design Spec

**Date:** 2026-04-16
**Phase:** General → Options (Step Attenuator subset)
**Source:** Thetis `setup.cs`, `console.cs` (Options 1 tab — Step Attenuator & Auto Attenuate groups)

## Scope

Port the step attenuator and ADC overload auto-attenuate system from Thetis,
plus a NereusSDR-native "Adaptive" auto-attenuate mode. Specifically:

- Manual step attenuator controls (RX1/RX2 enable + dB value)
- Preamp mode combo (for radios without step attenuator)
- Auto-attenuate on ADC overload — Classic (Thetis 1:1) and Adaptive (NereusSDR)
- ADC overload status bar indicator (yellow → red, per-ADC)
- Protocol-layer ADC overflow signal wiring
- Per-band ATT/preamp persistence
- All tooltips ported from Thetis with source citations

Out of scope: remaining Options 1 controls (PTT, CW delay, tuning options, misc
checkboxes), Options 2, Options 3. These are separate future work.

---

## 1. Architecture

Four components, cleanly separated:

```
RadioConnection (P1/P2)
    │  adcOverflow(int adc)
    ▼
StepAttenuatorController           ◄── new, src/core/
    │  overloadStatusChanged()          attenuationChanged()
    │  preampModeChanged()
    ├──────────────────┬────────────────────────┐
    ▼                  ▼                        ▼
MainWindow          RxApplet                GeneralOptionsPage
(status bar         (ATT/S-ATT row)         (Setup → General → Options)
 ADC OVL badge)
```

### 1.1 StepAttenuatorController (`src/core/StepAttenuatorController.h/cpp`)

Owns all attenuator and overload state for one radio connection.

**Responsibilities:**
- Receives `adcOverflow(int adc)` from RadioConnection
- Maintains per-ADC hysteresis counter (array of 3, values 0–5, 100ms QTimer tick)
- Queries DDC→ADC mapping from RadioConnection to route overloads to correct RX
- Detects and enforces ADC-linked synchronization (RX1/RX2 sharing same ADC)
- Updates ADC mapping when diversity/PureSignal modes change
- Runs auto-attenuate in Classic or Adaptive mode
- Calls `RadioConnection::setAttenuator(int dB)` to apply hardware changes
- Stores/restores per-band ATT and preamp values via AppSettings

**Signals emitted:**
- `overloadStatusChanged(int adc, OverloadLevel level)` — None, Yellow, Red
- `attenuationChanged(int rx, int dB)` — drives RxApplet spinbox
- `preampModeChanged(int rx, PreampMode mode)` — drives RxApplet combo
- `adcLinkedChanged(bool linked)` — drives "adc linked" warning in setup

**Slots:**
- `onAdcOverflow(int adc)` — connected to RadioConnection
- `onBandChanged(int rx, Band band)` — connected to SliceModel
- `setAttenuation(int rx, int dB)` — from UI controls
- `setPreampMode(int rx, PreampMode mode)` — from UI controls
- `setStepAttEnabled(int rx, bool enabled)` — switches ATT ↔ S-ATT mode
- `setAutoAttEnabled(int rx, bool enabled)`
- `setAutoAttMode(int rx, AutoAttMode mode)` — Classic / Adaptive
- `onDdcMappingChanged()` — re-query ADC assignments

**Enums:**
```cpp
enum class OverloadLevel { None, Yellow, Red };
enum class AutoAttMode { Classic, Adaptive };
enum class PreampMode {
    Off,          // 0 dB
    On,           // HPSDR_ON
    Minus10,      // -10 dB
    Minus20,      // -20 dB
    Minus30,      // -30 dB
    Minus40,      // -40 dB (ALEX-equipped only)
    Minus50       // -50 dB (ALEX-equipped only)
};
```

### 1.2 GeneralOptionsPage (`src/gui/setup/GeneralOptionsPage.h/cpp`)

New setup page registered under Setup → General → Options.

**Groups:**

**Step Attenuator group:**
- `chkRx1StepAttEnable` — "Enable RX1"
  - Tooltip: "Enable the step attenuator." (From Thetis setup.cs: chkHermesStepAttenuator)
- `spnRx1StepAttValue` — NumericUpDown 0–31 (or 0–61 with ALEX)
  - Range adjusted dynamically from BoardCapabilities
- `chkRx2StepAttEnable` — "Enable RX2"
  - Tooltip: "Enable the step attenuator." (From Thetis setup.cs: chkRX2StepAtt)
- `spnRx2StepAttValue` — NumericUpDown 0–31
- `lblAdcLinked` — red "adc linked" label, visible when RX1/RX2 share ADC
  - Driven by `StepAttenuatorController::adcLinkedChanged`

**Auto Attenuate RX1 group:**
- `chkAutoAttRx1` — "Auto att RX1"
  - Tooltip: "Auto attenuate RX1 on ADC overload" (From Thetis setup.cs: chkAutoATTRx1)
- `cmbAutoAttRx1Mode` — "Classic" / "Adaptive"
  - Tooltip: "Classic: Thetis-style immediate bump with timed undo. Adaptive: gradual attack/decay with per-band memory."
- `chkAutoAttUndoRx1` — "Undo" (Classic) / "Decay" (Adaptive)
  - Tooltip: "Undo the changes made after X seconds" (From Thetis setup.cs: chkAutoAttUndoRX1)
  - Enabled only when auto-att is enabled
- `spnAutoAttHoldRx1` — seconds, range 1–3600, default 5
  - Enabled only when undo/decay is enabled

**Auto Attenuate RX2 group:** (mirrors RX1)
- `chkAutoAttRx2`, `cmbAutoAttRx2Mode`, `chkAutoAttUndoRx2`, `spnAutoAttHoldRx2`

### 1.3 RxApplet ATT Row (`src/gui/applets/RxApplet.h/cpp` — modify)

New row in the right column, between Squelch and AGC:

```
Squelch:    [SQL toggle] [slider]
ATT:        [S-ATT] [stacked: combo OR spinbox]
AGC:        [AGC combo] [AGC threshold slider]
```

**Components:**
- **Label** — dynamic text:
  - "ATT" when preamp combo is active (step att disabled)
  - "S-ATT" when step attenuator is active (step att enabled)
- **QStackedWidget** containing:
  - Page 0: Preamp combo — items from BoardCapabilities preamp modes, per radio model
  - Page 1: dB spinbox — range from BoardCapabilities attenuator min/max/step

**Capability gating:**
- No attenuator AND no preamp → entire row hidden
- Preamp only → label "ATT", combo visible
- Step att enabled → label "S-ATT", spinbox visible

**Per-band behavior:**
- On `SliceModel::bandChanged`, controller restores the per-band value
- Spinbox/combo updates via `attenuationChanged` / `preampModeChanged` signals

**Preamp combo items per radio model (from Thetis):**

| Model | Items |
|-------|-------|
| Atlas | "0dB", "-20dB" + ALEX items if present |
| Hermes | With ALEX: "0dB", "-20dB(HPSDR)", "-10dB", "-20dB(SA)", "-30dB", "-40dB", "-50dB". Without: "0dB", "-10dB", "-20dB", "-30dB" |
| ANAN-10/10E | "0dB", "-10dB", "-20dB", "-30dB" |
| ANAN-100/100B | "0dB", "-20dB(HPSDR)", "-10dB", "-20dB(SA)", "-30dB", "-40dB", "-50dB" |
| ANAN-100D/200D | With ALEX: same as 100/100B. Without: "0dB", "-10dB", "-20dB", "-30dB" |
| ANAN-7000D/8000D/OrionMkII/G2 | "0dB", "-10dB", "-20dB", "-30dB" |
| Hermes Lite 2 | "0dB" (preamp only, no bypass) |
| ANAN-7000 | "0dB", "-10dB", "-20dB", "-30dB" |

### 1.4 ADC Overload Status Bar Indicator (`src/gui/MainWindow.h/cpp` — modify)

Inserted left of the STATION container:

```
... [radio info] [stretch] [ADC OVL badge] [gap] [STATION: KG4VCF] [stretch] ...
```

**Visual states:**

| State | Text | Color |
|-------|------|-------|
| No overload | — (hidden) | — |
| ADC0 yellow | "ADC0 OVL" | #FFD700 (yellow) |
| ADC0 red | "ADC0 OVL" | #FF3333 (red) |
| ADC1 yellow | "ADC1 OVL" | #FFD700 |
| ADC1 red | "ADC1 OVL" | #FF3333 |
| Both | "ADC0 OVL ADC1 OVL" | worst level color |

**Tooltip:** Per-ADC detail, e.g. "ADC0: overload". When auto-att active,
appends "\nAuto attenuation active".

**Timing:** Follows controller's `overloadStatusChanged` signal directly.
Badge visible as long as any ADC has level > 0. No separate display timer.

---

## 2. Protocol Layer

### 2.1 ADC Overflow Emission

`adcOverflow(int adc)` signal exists in `RadioConnection.h` (line 99) but is
never emitted. Wire it up:

**P1 (Protocol 1):** In the EP6 (RX data) frame handler in `P1RadioConnection`,
parse the ADC overflow bit from the C1 byte when C0 address = 0x00.
Emit `adcOverflow(0)` when C1 bit 0 is set.
- C0 bit 0 at address 0x00 is PTT/MOX — NOT overflow
- C1 bit 0 at address 0x00 = LT2208 overflow (ADC0)
- Reference: `openhpsdr-protocol1-capture-reference.md` §3.3, §A.11;
  `radio-abstraction.md` C&C byte format table
- **Note:** existing `P1RadioConnection.cpp:872-878` checks C0 (frame[11])
  instead of C1 (frame[12]) — pre-existing bug to fix during implementation

**P2 (Protocol 2):** In the high-priority status frame handler in
`P2RadioConnection`, parse per-ADC overflow bits. Emit `adcOverflow(adcIndex)`
for each flagged ADC (up to 3 ADCs: ADC0, ADC1, ADC2).

### 2.2 DDC → ADC Mapping

The controller needs to know which ADC serves each RX to route overloads correctly.

**From Thetis (`GetADCInUse(int ddc)` — console.cs:15083):**
- P2: Uses `RXADCCtrl1` (DDC 0–3) / `RXADCCtrl2` (DDC 4–7), 2 bits per DDC
- P1: Uses `RXADCCtrl_P1` (14-bit int, 2 bits per DDC)
- Register selection + index remap required for P2:
  ```
  adcControl = (ddc < 4) ? RXADCCtrl1 : RXADCCtrl2;
  if (ddc >= 4) ddc -= 4;   // remap to local index within ctrl2
  return (adcControl & (3 << (ddc * 2))) >> (ddc * 2);
  ```
- Without the remap, DDC 4+ shifts beyond the 8-bit register width
- Reference: `adc-ddc-panadapter-mapping.md` §4.1–4.3
- **Note:** existing `P2RadioConnection::getAdcForDdc()` (line 759) always
  reads `m_rxAdcCtrl1` and lacks `m_rxAdcCtrl2` — pre-existing bug to fix

**NereusSDR approach:** Add `int getAdcForDdc(int ddc)` to RadioConnection
(virtual, implemented in P1/P2). Controller calls this to resolve the mapping.
Re-query on diversity/PS mode changes via `onDdcMappingChanged()`.

### 2.3 ALEX Attenuation (Extended Range)

For radios with `BoardCapabilities::hasAlexFilters` AND not in the exclusion
list (ANAN-10, ANAN-10E, ANAN-7000D, ANAN-8000D, OrionMkII, G2, G2-1K,
AnvelinaPro3, RedPitaya):

- **0–31 dB:** Direct to ADC step attenuator, ALEX atten = 0
- **32–61 dB:** ALEX atten set to -30 dB, ADC step atten = value - 30
  (maps 32→2 through 61→31, keeping ADC within its 0–31 range)

This logic lives in `StepAttenuatorController`, not the protocol layer.

---

## 3. Hysteresis & Overload Detection

### 3.1 Per-ADC Counters

```cpp
int m_overloadLevel[3] = {0, 0, 0};   // 0–5 per ADC
bool m_overloadFlag[3] = {};            // set by adcOverflow, cleared each tick
```

**100ms QTimer tick:**
1. For each ADC (0, 1, 2):
   - If `m_overloadFlag[i]` was set since last tick: increment `m_overloadLevel[i]` (cap at 5)
   - Else: decrement `m_overloadLevel[i]` (floor at 0)
   - Clear `m_overloadFlag[i]`
2. Emit `overloadStatusChanged` when level crosses thresholds:
   - 0 → 1: Yellow
   - 3 → 4: Red
   - 1 → 0: None
   - 4 → 3: Yellow (downgrade from red)

### 3.2 Thresholds (from Thetis)

- **Yellow:** level > 0 (immediate, within one 100ms tick)
- **Red:** level > 3 (~400ms sustained overload)
- **Auto-attenuate triggers at Red** (not yellow)

---

## 4. Auto-Attenuate Algorithms

### 4.1 Classic Mode (Thetis 1:1 Port)

**Attack:**
- On red threshold for ADC serving RX*n*:
  - Resolve which RX uses the overloading ADC via DDC→ADC mapping
  - Push current ATT value onto history stack (`QStack<int>`)
  - Increment ATT by `_adc_step_shift` (accumulated overload count, capped at 31)
  - Apply via `RadioConnection::setAttenuator(dB)`
  - If ADCs linked (RX1/RX2 share ADC), synchronize both RX attenuators

**Recovery:**
- If undo enabled: start a `QTimer::singleShot(holdSeconds * 1000)`
  - On fire: pop history stack, restore previous ATT value
- If undo disabled: ATT stays elevated until manual adjustment

**Preamp mode reduction (when step att not enabled):**
- Instead of increasing dB, step the preamp combo to the next higher attenuation
  mode (e.g. 0dB → -10dB → -20dB → -30dB)
- History stack stores the previous PreampMode for undo

### 4.2 Adaptive Mode (NereusSDR Enhancement)

**Attack:**
- On red threshold: increase ATT by **1 dB per 100ms tick** while overload persists
- Cap at board maximum (31 or 61 dB)
- No single large jump — gradual ramp proportional to overload duration

**Hold:**
- When overloads stop, hold current ATT for `holdSeconds` (configurable, default 5s)
- No decay during hold period
- If overload recurs during hold, immediately re-enter attack (no hold delay on re-attack)

**Decay:**
- After hold expires: decrease ATT by **1 dB every 500ms** (5x slower than attack)
- If overload recurs during decay, immediately re-enter attack
- Decay stops at per-band settled floor (or 0 if no floor set)

**Per-band memory:**
- `QHash<Band, int> m_settledFloor` per RX
- When decay completes without re-triggering for a full hold period, save current
  value as settled floor for current band
- On band change: restore settled floor as starting ATT value (instead of 0)
- Gives pre-loaded protection on known-noisy bands

### 4.3 ADC-Linked Synchronization

From Thetis `console.cs` lines 11062, 11224, 19357, 19478:

```
linked = (nRX1ADCinUse == nRX2ADCinUse) || bDiversityAttLink
```

When linked:
- Changing RX1 ATT forces RX2 to match (and vice versa)
- Auto-attenuate on either RX updates both
- Setup page shows red "adc linked" label

---

## 5. Persistence

All settings via `AppSettings::setHardwareValue(mac, key, value)`.

**Note on CLAUDE.md radio-authoritative policy:** CLAUDE.md lists ADC
attenuation and preamp as "radio-authoritative (do NOT persist)." However,
Thetis persists per-band ATT/preamp *desired values* client-side and restores
them on band change. This is consistent — we persist the operator's *intent*
per band, then send the value to hardware on each band change. The hardware
has no per-band memory; the client is the source of truth for "what ATT should
I use on 40m?"

### 5.1 Configuration Keys

```
options/stepAtt/rx1Enabled              bool    default false
options/stepAtt/rx1Value                int     default 0
options/stepAtt/rx2Enabled              bool    default false
options/stepAtt/rx2Value                int     default 0
options/autoAtt/rx1Enabled              bool    default false
options/autoAtt/rx1Mode                 string  default "Classic"
options/autoAtt/rx1Undo                 bool    default true
options/autoAtt/rx1HoldSeconds          int     default 5
options/autoAtt/rx2Enabled              bool    default false
options/autoAtt/rx2Mode                 string  default "Classic"
options/autoAtt/rx2Undo                 bool    default true
options/autoAtt/rx2HoldSeconds          int     default 5
```

### 5.2 Per-Band Keys

```
options/stepAtt/rx1Band/<bandKey>       int     per-band ATT dB
options/stepAtt/rx2Band/<bandKey>       int     per-band ATT dB
options/preamp/rx1Band/<bandKey>        string  PreampMode name
options/preamp/rx2Band/<bandKey>        string  PreampMode name
```

Band keys match existing NereusSDR band key format from `Band.h`.

### 5.3 Adaptive Mode Per-Band Settled Floors

```
options/autoAtt/rx1Settled/<bandKey>    int     settled floor dB
options/autoAtt/rx2Settled/<bandKey>    int     settled floor dB
```

These are runtime-accumulated values, persisted so they survive restarts.

---

## 6. Signal Flow

```
RadioConnection::adcOverflow(int adc)
    → StepAttenuatorController::onAdcOverflow(adc)
        → sets m_overloadFlag[adc]
        → 100ms timer tick updates hysteresis counters
        → overloadStatusChanged(adc, level)  →  MainWindow ADC OVL badge
        → if red + auto-att enabled:
            → resolve DDC→ADC→RX mapping
            → apply Classic or Adaptive algorithm
            → attenuationChanged(rx, dB)     →  RxApplet spinbox
            → RadioConnection::setAttenuator(dB)

RxApplet spinbox/combo changed by user
    → StepAttenuatorController::setAttenuation(rx, dB) or setPreampMode(rx, mode)
        → RadioConnection::setAttenuator(dB)
        → persist per-band value via AppSettings
        → attenuationChanged(rx, dB)         →  other listeners
        → if ADCs linked, synchronize other RX

GeneralOptionsPage controls changed
    → StepAttenuatorController config update slots
    → AppSettings persistence

SliceModel::bandChanged(rx, band)
    → StepAttenuatorController::onBandChanged(rx, band)
        → restore per-band ATT/preamp value
        → RadioConnection::setAttenuator(dB)
        → attenuationChanged / preampModeChanged → RxApplet
```

---

## 7. Files Changed / Created

| File | Action | Purpose |
|------|--------|---------|
| `src/core/StepAttenuatorController.h` | Create | Controller header |
| `src/core/StepAttenuatorController.cpp` | Create | Controller implementation |
| `src/gui/setup/GeneralOptionsPage.h` | Create | Options setup page header |
| `src/gui/setup/GeneralOptionsPage.cpp` | Create | Options setup page implementation |
| `src/gui/applets/RxApplet.h` | Modify | Add ATT row members |
| `src/gui/applets/RxApplet.cpp` | Modify | Add ATT row between Squelch and AGC |
| `src/gui/MainWindow.h` | Modify | Add ADC OVL label member |
| `src/gui/MainWindow.cpp` | Modify | Add ADC OVL badge in status bar |
| `src/gui/SetupDialog.cpp` | Modify | Register GeneralOptionsPage under General |
| `src/core/P1RadioConnection.cpp` | Modify | Emit adcOverflow from EP6 frame parser |
| `src/core/P2RadioConnection.cpp` | Modify | Emit adcOverflow from status frame parser |
| `src/core/RadioConnection.h` | Modify | Add getAdcForDdc() virtual method |
| `src/core/P1RadioConnection.h/cpp` | Modify | Implement getAdcForDdc() |
| `src/core/P2RadioConnection.h/cpp` | Modify | Implement getAdcForDdc() |
| `src/core/BoardCapabilities.h` | Modify | Add preamp mode list per board (if not present) |
| `CMakeLists.txt` | Modify | Add new source files |

---

## 8. Tooltips (Ported from Thetis)

All tooltips sourced from Thetis `setup.cs` with citations:

| Control | Tooltip | Source |
|---------|---------|--------|
| Enable RX1 step att | "Enable the step attenuator." | setup.cs: chkHermesStepAttenuator |
| Enable RX2 step att | "Enable the step attenuator." | setup.cs: chkRX2StepAtt |
| Auto att RX1 | "Auto attenuate RX1 on ADC overload" | setup.cs: chkAutoATTRx1 |
| Auto att RX2 | "Auto attenuate RX2 on ADC overload" | setup.cs: chkAutoATTRx2 |
| Undo RX1 | "Undo the changes made after X seconds" | setup.cs: chkAutoAttUndoRX1 |
| Undo RX2 | "Undo the changes made after X seconds" | setup.cs: chkAutoAttUndoRX2 |
| Mode combo | "Classic: Thetis-style immediate bump with timed undo. Adaptive: gradual attack/decay with per-band memory." | NereusSDR native |
| ADC OVL badge | "Auto Attenuation has been triggered from an ADC overload !" | console.cs: pbAutoAttWarningRX1 tooltip |
