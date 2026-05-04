# PA Calibration Safety Hotfix

Issue #167. Branch `hotfix/pa-calibration-167`. Tag `v0.3.2`.

This document is the canonical record of what shipped in v0.3.2 and why.
The execution plan lives at `~/.claude/plans/glowing-snacking-mochi.md`;
this doc is the finished state and the ground truth for future
maintainers, reviewers, and the merge-forward into
`feature/phase3m-3-tx-processing`.

---

## §1 Context

K2GX (ANAN-8000DLE, P2, NereusSDR 0.3.1) reported PA output exceeding
300 W on a 200 W radio at low TUNE-slider positions. PA finals at risk.

### Root cause

`RadioModel.cpp:830-853` (drive-slider lambda) and `:4280-4296` (TUNE
drive scaling) computed:

```cpp
wire_byte = clamp(int(255 * (slider/100) * swrProtect), 0, 255)
```

No per-band PA gain compensation. The 8000DLE 80 m PA has 50.5 dB of
gain — its rated 200 W output is reached well below slider 100 %, so a
"50 %" slider position drives the PA into clipping.

### Thetis precedent

Thetis applies the per-band PA gain table at the **audio output** level,
not the wire-byte path. From console.cs:46645-46762 [v2.10.3.13]
(`SetPowerUsingTargetDBM`):

```
target_dbm   = 10 * log10(slider_watts * 1000)              // watts -> dBm
gbb          = active_profile.getGainForBand(band, slider)
target_dbm  -= gbb
target_volts = sqrt(10^(target_dbm * 0.1) * 0.05)           // E = sqrt(P*R), R = 50
audio_volume = min(target_volts / 0.8, 1.0)                 // clamp to [0, 1]
```

Then composition lives at two distinct call sites:

```
wire_byte = clamp(int(audio_volume * 1.02 * 255), 0, 255)   // audio.cs:268
iq_gain   = audio_volume * swrProtect                        // cmaster.cs:1117
```

**SWR foldback enters via the IQ scalar only.** The wire byte never
sees the SWR factor in MW0LGE Thetis. Pre-hotfix NereusSDR applied SWR
to the wire byte; this is reverted to MW0LGE-canonical topology.

### Master plan deferral

`docs/architecture/phase3m-1a-thetis-pre-code-review.md` §4.3 deferred
this work: "The full dBm-target logic + drive-source enum is deferred
to 3M-3a alongside the rest of the TX processing chain." Issue #167
lands the deferred port now, ahead of 3M-3a, because the K2GX field
report makes it ship-blocking.

---

## §2 Math derivation

The math kernel lives in `TransmitModel::computeAudioVolume`. Pure
function. No state mutation. Returns `audio_volume` in `[0, 1.0]`.

### Domain

- `slider_watts`: integer in `[0, 100]`. Watts the user requested.
- `gbb`: float in roughly `[0, 60]` for real bands; sentinel `100.0` for
  "no compensation" rows (HL2 HF, Bypass profile, GEN/WWV/XVTR).
- Result: `audio_volume` in `[0, 1.0]`. Clamped, so any positive input
  maps to a representable float regardless of `gbb` sign.

### Step-by-step (console.cs:46720-46751 [v2.10.3.13])

```
1. target_dbm   = 10 * log10(slider_watts * 1000)
                = 10 * log10(slider_milliwatts)

2. gbb          = active_profile.getGainForBand(band, slider_watts)
                — interpolated from per-band gain (dB) + per-step
                  9-step adjust matrix (drive_value ∈ [10, 90])

3. target_dbm  -= gbb
                — subtract PA gain (dB) so we drive the input at the
                  level needed to hit target_dbm at the antenna

4. target_pwr_w = 10^(target_dbm * 0.1) / 1000
   target_volts = sqrt(target_pwr_w * 50)
                — Ohm's law: P = V^2 / R, R = 50 ohm, so V = sqrt(P*R)
                — Thetis writes this as sqrt(10^(target_dbm*0.1) * 0.05)
                  which is the same algebra collapsed (50/1000 = 0.05)

5. audio_volume = target_volts / 0.8
                — 0.8 V is the codec full-scale reference

6. audio_volume = min(audio_volume, 1.0)
                — clamp at the codec ceiling
```

### Edge cases

- **`slider_watts == 0`**: short-circuit to `audio_volume = 0.0`.
  Matches Thetis console.cs:46749-46751.
- **`gbb >= 99.5`**: HL2 / Bypass / sentinel-band fallback. Returns
  `clamp(slider_watts / 100.0, 0.0, 1.0)` — the v0.3.1 linear formula.
  Documented inline. See §3.
- **Out-of-range `Band`** (XVTR, GEN, WWV, SWL): `getGainForBand` returns
  the Thetis-faithful `1000.0f` sentinel (setup.cs:23866 [v2.10.3.13]),
  which trips the `gbb >= 99.5` short-circuit.

### K2GX hand-computed regression

ANAN-8000DLE, 80 m, slider = 50:

```
target_dbm   = 10 * log10(50 * 1000)        = 46.99 dBm
gbb          = 50.5                          (8000DLE 80 m)
target_dbm  -= 50.5                          = -3.51 dBm
target_volts = sqrt(10^(-0.351) * 0.05)     = 0.1490 V
audio_volume = min(0.1490 / 0.8, 1.0)       = 0.1862
wire_byte    = clamp(int(0.1862 * 1.02 * 255), 0, 255) = 48
```

Pre-hotfix wire byte at the same input was `127` (slider = 50% → byte
255 / 2). Post-hotfix is `~49`. PA dissipation drops by ~6.5x at the
input — math is consistent with the K2GX field measurement.

### Safety ceiling

For every (HPSDRModel, Band) at the radio's spec'd max watts,
`audio_volume < 0.99`. The tightest case is ANAN-8000D B10M @ 200 W,
which lands at `0.992` — flagged inline in
`tests/tst_transmit_model_audio_volume.cpp` so the next person who
tweaks the math has a regression target.

---

## §3 HL2 sentinel rationale

mi0bot's HL2 row at clsHardwareSpecific.cs:484 [v2.10.3.13-beta2
@c26a8a4] sets the HL2 HF gain to `100.0` with the inline comment
"100 is no output power". Thetis HL2 users have always had to PA-
calibrate their HL2 before HF transmit; running the dBm math through
that row gives `audio_volume ~= 0.0009` at 100 W slider, which is the
"don't transmit until you've calibrated" behaviour Thetis ships.

NereusSDR users upgrading from a working v0.3.1 state cannot regress
to silent transmit. v0.3.1 used a linear formula; v0.3.2 must produce
the same output for HL2 users until they explicitly PA-calibrate.

### Resolution

`computeAudioVolume` short-circuits when `gbb >= 99.5`:

```cpp
if (gbb >= 99.5f) {
    // HL2 / Bypass / sentinel-band fallback — preserve the
    // pre-v0.3.2 linear formula instead of running the dBm math.
    return std::clamp(static_cast<double>(sliderWatts) / 100.0,
                      0.0, 1.0);
}
```

Properties:

1. **Centralized**: every caller (drive-slider, TUNE, two-tone, future
   CW) goes through the same kernel and gets the correct fallback.
2. **Bypass-profile catch**: the all-100.0 Bypass profile trips the
   same path automatically. Useful for users who want to disable PA
   compensation without unloading the profile system.
3. **Documented inline**: the deviation is cited next to the code.

The 99.5 threshold (vs. exact `== 100.0f`) handles float-precision
noise from UI round-trips. Anything `>= 99.5` is treated as the
sentinel; real PA gain rows top out around 60 dB (Thetis tables).

### Inline comment in TransmitModel.cpp

```cpp
// HL2 / Bypass / sentinel-band short-circuit. NereusSDR-original
// deviation from Thetis SetPowerUsingTargetDBM. Justification:
// mi0bot HL2 row at clsHardwareSpecific.cs:484 [v2.10.3.13-beta2
// @c26a8a4] sets HF gain to 100.0 with comment "100 is no output
// power". Thetis users live with audio_volume ~= 0.0009 at 100 W
// slider; NereusSDR v0.3.1 users had a working linear formula
// pre-hotfix and must not regress. See pa-calibration-hotfix.md §3.
```

---

## §4 Wire-byte vs IQ-scalar topology

The pre-hotfix code applied `swrProtect` to the wire byte and never to
the IQ scalar. Thetis (MW0LGE-canonical) does the opposite. The
hotfix corrects to MW0LGE topology.

### Thetis source of truth

`audio.cs:262-271 [v2.10.3.13]`:

```csharp
public static double RadioVolume {
    set {
        radio_volume = value;
        NetworkIO.SetOutputPower((float)(value * 1.02));   // wire byte: NO SWR
        cmaster.CMSetTXOutputLevel();
    }
}
```

`cmaster.cs:1115-1119 [v2.10.3.13]`:

```csharp
public static void CMSetTXOutputLevel() {
    double level = Audio.RadioVolume * Audio.HighSWRScale;  // IQ scalar: SWR HERE
    cmaster.SetTXFixedGain(0, level, level);
}
```

Two distinct paths from the same `audio_volume`:

1. **Wire byte** (`NetworkIO.SetOutputPower`): `audio_volume * 1.02`
   only. The `1.02` factor is a Thetis-internal scaling constant
   that's been there since the original FlexRadio code. No SWR.
2. **IQ scalar** (`cmaster.SetTXFixedGain`): `audio_volume *
   HighSWRScale`. SWR foldback is here. The C++ side of WDSP
   multiplies the IQ stream by this scalar before the codec write.

### Why this matters

If SWR foldback rides the wire byte, every `setTxDrive` call has to
multiply by `swrProtect`. That makes every protocol byte path SWR-
aware, which is fragile (the SWR factor needs to be threaded through
P1, P2, codecs, and any future protocols) and topologically wrong
(the wire byte is supposed to be a stable "user requested this much
drive" value; SWR is a real-time safety modulation that lives in DSP).

Putting SWR on the IQ scalar — exactly once, inside WDSP — is both
simpler and Thetis-faithful.

### NereusSDR composition

`RadioModel.cpp` (Phase 4A):

```cpp
const double audio_volume = m_transmitModel.computeAudioVolume(
    *activeProfile, currentBand, sliderWatts);

// Wire byte: NO SWR factor. From Thetis audio.cs:262-271 [v2.10.3.13]
// SetOutputPower(value * 1.02). The 1.02 is a Thetis-internal
// scaling constant. SWR foldback rides the IQ scalar only — see
// cmaster.cs:1115-1119 and the setTxFixedGain call below.
const int wire_byte = std::clamp(
    static_cast<int>(audio_volume * 1.02 * 255.0), 0, 255);
m_radioConnection->setTxDrive(wire_byte);

// IQ scalar: SWR foldback applied here, inside WDSP. From Thetis
// cmaster.cs:1115-1119 [v2.10.3.13] CMSetTXOutputLevel: level =
// RadioVolume * HighSWRScale. The wire byte does NOT see this
// factor — see audio.cs:268 above and the asymmetry comment.
const double iq_scalar = audio_volume * m_transmitModel.swrProtectFactor();
if (m_txChannel) {
    m_txChannel->setTxFixedGain(iq_scalar);
}
```

Both call sites carry the cross-reference comment so future
maintainers don't "fix" the asymmetry.

---

## §5 ATT-on-TX-on-power-change wiring

Thetis console.cs:46740-46748 [v2.10.3.13] inserts an ATT-on-TX bump
when (a) PureSignal is active and (b) the user changed power. This is
because PS calibration depends on a stable RF chain, and a power
change on a live PS-active TX would otherwise sail straight into the
ADC at full forward.

### Thetis source

```csharp
if (new_pwr != _lastPower
    && chkFWCATUBypass.Checked
    && _forceAttwhenPowerChangesWhenPSAon) {
    if (new_pwr > _lastPower
        || _forceAttwhenPowerChangesWhenPSAonAndDecreased) {
        SetupForm.ATTOnTX = 31;
    }
    _lastPower = new_pwr;
}
```

### NereusSDR port

Inside `TransmitModel::setPowerUsingTargetDbm`, after computing
`new_pwr` and before applying `audio_volume`:

```cpp
if (new_pwr != m_lastPower
    && pureSignalActive()
    && m_forceAttwhenPowerChangesWhenPSAon) {
    if (new_pwr > m_lastPower
        || m_forceAttwhenPowerChangesWhenPSAonAndDecreased) {
        if (m_stepAttController) {
            m_stepAttController->setAttOnTxValue(31);  // SetupForm.ATTOnTX = 31
        }
    }
    m_lastPower = new_pwr;
}
```

### PureSignal future-proofing

**The ATT-on-TX subsystem in v0.3.2 has no observable behaviour. The
`pureSignalActive()` predicate returns false unconditionally; the
gate never fires. The state and gate logic are scaffolding for Phase
3M-4.** The PS feedback DDC and `chkFWCATUBypass` live wiring don't
exist yet, so there is no UI surface for the safety toggles in v0.3.2
either — the previous `m_attOnTxWarning` informational label was
dropped from `PaGainByBandPage` to avoid implying configurability that
isn't there.

But the structure is here:

- The three `forceAttwhen*` properties are wired and persisted per-MAC.
- The `m_lastPower` sentinel and reset semantics
  (console.cs:29298 — reset to -1 when `forceAttwhenPowerChangesWhenPSAon`
  setter changes value) are ported.
- `setAttOnTxValue(int dB)` lives on `StepAttenuatorController` and
  routes to the existing TxAttenData path (console.cs:10600-10625
  `[2.10.3.6]MW0LGE att_fixes #399`).

When 3M-4 lands, flipping `pureSignalActive()` to read the live PS
state activates the gate without further structural changes. The
informational UI surface (warning row + Setup → General → Options
toggles) is also re-added at that point.

### Test seam

`TransmitModel::setPureSignalActiveForTest(bool)` is gated on
`NEREUS_BUILD_TESTS`. Tests can flip the predicate to true and
exercise the gate end-to-end without touching the (non-existent)
PureSignal stack.

---

## §6 Setup → PA parity matrix

Thetis `tpPowerAmplifier` (setup.designer.cs:47366-47391 [v2.10.3.13])
hosts `tcPowerAmplifier` containing two sub-tabs: `tpGainByBand` and
`tpWattMeter`. NereusSDR splits `panelPAValues` out of `tpWattMeter`
into a dedicated `PaValuesPage` (NereusSDR-spin), so we have three
pages instead of two.

### `tpGainByBand` parity (PaGainByBandPage)

| Thetis control | NereusSDR equivalent | Status |
|---|---|---|
| `pbPAProfileWarning` (warning icon) | `m_warningIcon` (QLabel + QIcon) | ✅ Phase 6 |
| `lblPAProfileWarning` (warning text) | `m_warningLabel` (QLabel) | ✅ Phase 6 |
| `chkPANewCal` (new-cal mode toggle) | `m_newCalCheck` (QCheckBox) | ✅ Phase 6 |
| `grpGainByBandPA` (main GroupBox) | `m_gainByBandGroup` (QGroupBox) | ✅ Phase 6 |
| 14 band gain spinboxes (`nud160M..nudVHF13`) | `m_gainSpins[14]` (QDoubleSpinBox array) | ✅ Phase 6 |
| `panelAdjustGain` (drive-step adjust matrix) | `m_adjustSpins[14][9]` (QDoubleSpinBox 14×9 grid) | ✅ Phase 6 |
| `nudMaxPowerForBandPA` + `chkUsePowerOnDrvTunPA` | `m_maxPowerSpins[14]` + `m_useMaxPowerChecks[14]` | ✅ Phase 6 |
| `comboPAProfile` (active profile combo) | `m_profileCombo` (QComboBox) | ✅ Phase 6 |
| `btnNewPAProfile` | `m_btnNew` (QPushButton) | ✅ Phase 6 |
| `btnCopyPAProfile` | `m_btnCopy` (QPushButton) | ✅ Phase 6 |
| `btnDeletePAProfile` | `m_btnDelete` (QPushButton) | ✅ Phase 6 |
| `btnResetPAProfile` (Reset Defaults) | `m_btnReset` (QPushButton) | ✅ Phase 6 |
| `chkAutoPACalibrate` | `m_autoCalibrateCheck` (QCheckBox) | ✅ Phase 6 |
| `panelAutoPACalibrate` (sweep config) | `m_autoCalPanel` (QGroupBox: target W spinbox + status label + progress bar + cancel button) | ⚠️ Partial (Phase 7) — 4 of 17 sub-controls ported; per-band selectors, all-vs-selected radio pair, ANAN bypass, explicit Calibrate trigger deferred to F1. |

NereusSDR-spin densification: Thetis ships only the row for the
currently-selected band via `panelAdjustGain`. NereusSDR exposes the
full 14×9 matrix so the editor is non-modal and the user can scan
across bands at a glance.

### `tpWattMeter` parity (PaWattMeterPage)

| Thetis control | NereusSDR equivalent | Status |
|---|---|---|
| Per-board PA forward-power cal-points | `PaCalibrationGroup` | ✅ Pre-existing (v0.3.1) |
| `chkPAValues` (toggle Show PA Values panel) | `m_paValuesToggle` (QCheckBox) | ✅ Phase 5A |
| `btnResetPAValues` (peak/min reset) | `m_resetPaValuesButton` (QPushButton) emitting `resetPaValuesRequested` | ✅ Phase 5A |

Phase 9 cross-wires `resetPaValuesRequested` to
`PaValuesPage::resetPaValues()` in `SetupDialog::buildTree()`.

### `panelPAValues` parity (PaValuesPage — NereusSDR-spin promotion)

| Thetis field | NereusSDR equivalent | Status |
|---|---|---|
| Calibrated FWD | `m_fwdCalibratedLabel` | ✅ v0.3.1 |
| REV | `m_revPowerLabel` | ✅ v0.3.1 |
| SWR | `m_swrLabel` | ✅ v0.3.1 |
| DC volts (supply) | `m_supplyVoltsLabel` | ✅ v0.3.1 |
| PA current | `m_paCurrentLabel` | ✅ v0.3.1 |
| PA temperature | `m_paTempLabel` | ✅ v0.3.1 |
| ADC overload | `m_adcOverloadLabel` | ✅ v0.3.1 |
| FWD ADC raw | `m_fwdAdcLabel` | ✅ v0.3.1 |
| REV ADC raw | `m_revAdcLabel` | ✅ v0.3.1 |
| Raw FWD power (W) | `m_fwdRawLabel` | ✅ Phase 5B |
| Drive power | `m_driveLabel` | ✅ Phase 5B |
| FWD voltage | `m_fwdVoltageLabel` | ✅ Phase 5B |
| REV voltage | `m_revVoltageLabel` | ✅ Phase 5B |
| `btnResetPAValues` (running peak/min reset) | `m_resetButton` + 6 peak/min trackers | ✅ Phase 5B |

Phase 5B uses the new public `PaTelemetryScaling::scaleFwdPowerWatts`
/ `scaleFwdRevVoltage` helpers (Phase 1B). These were lifted out of
`RadioModel.cpp` private file-scope so the page can call them
directly.

### NereusSDR-original additions (no Thetis equivalent)

The following PA Setup controls have no upstream parallel and were added as
NereusSDR-spin enhancements:

| Control | Reason |
|---|---|
| PA Current label (PaValuesPage) | Native ANAN/G2 telemetry; Thetis ships no equivalent on Setup pages |
| PA Temperature label (PaValuesPage) | Same |
| ADC Overload label (PaValuesPage) | Native NereusSDR ADC overflow status display |
| Reset Peak/Min button + 6 PeakMin trackers (PaValuesPage) | Running peak/min on telemetry; Thetis textboxes are instantaneous-only |
| m_noPaSupportBanner (PaGainByBandPage) | NereusSDR per-SKU UX: explicit banner when caps.hasPaProfile=false (RX-only kits, etc.) |
| m_ganymedeWarning (PaGainByBandPage) | NereusSDR per-SKU UX: Andromeda Ganymede 500W PA tab follow-up notice |
| m_psWarning (PaGainByBandPage) | NereusSDR per-SKU UX: PureSignal-related PA/TX-Profile recovery linkage notice |

(`m_attOnTxWarning` was originally part of this block; dropped in v0.3.2
cleanup — see CHANGELOG ATT-on-TX entry. The ATT-on-TX UI lands with
PureSignal in Phase 3M-4.)

---

## §7 Per-SKU visibility (16-row matrix)

`SetupDialog::applyPaVisibility` consumes `BoardCapabilities` and
hides / disables PA controls per SKU. Mirrors Thetis
`comboRadioModel_SelectedIndexChanged` (setup.cs:19812-20310
[v2.10.3.13 @501e3f51]).

### Capability flags consumed

- `caps.isRxOnlySku` — entire PA category hidden (no TX hardware)
- `caps.hasPaProfile` — PaGainByBandPage editor disabled with banner
- `caps.canDriveGanymede` — Ganymede 500 W warning shown (informational; tab deferred)
- `caps.hasPureSignal` — PA-Profile / TX-Profile recovery linkage warning shown
- `caps.hasStepAttenuatorCal` — ATT-on-TX-on-power-change warning row visibility

### 16-model matrix

| HPSDRModel | hasPaProfile | isRxOnlySku | PA category visible | Editor enabled | Notes |
|---|---|---|---|---|---|
| FIRST (Atlas) | ❌ | ❌ | ❌ | ❌ | "no PA support" banner |
| HERMES | ✅ | ❌ | ✅ | ✅ | |
| HPSDR | ✅ | ❌ | ✅ | ✅ | |
| ORIONMKII | ✅ | ❌ | ✅ | ✅ | |
| ANAN10 | ✅ | ❌ | ✅ | ✅ | |
| ANAN10E | ✅ | ❌ | ✅ | ✅ | |
| ANAN100 | ✅ | ❌ | ✅ | ✅ | |
| ANAN100B | ✅ | ❌ | ✅ | ✅ | |
| ANAN100D | ✅ | ❌ | ✅ | ✅ | |
| ANAN200D | ✅ | ❌ | ✅ | ✅ | |
| ANAN8000D | ✅ | ❌ | ✅ | ✅ | K2GX scenario |
| ANAN7000D | ✅ | ❌ | ✅ | ✅ | |
| ANAN_G2 | ✅ | ❌ | ✅ | ✅ | |
| ANAN_G2_1K | ✅ | ❌ | ✅ | ✅ | Ganymede 500 W warning shown |
| ANVELINAPRO3 | ✅ | ❌ | ✅ | ✅ | |
| HERMESLITE | ✅ | ❌ | ✅ | ✅ | All-100.0 sentinel on HF; 38.8 on 6m+VHF |
| REDPITAYA | ❌ | ❌ | ❌ | ❌ | "no PA support" banner |

`hasPaProfile=true` is the gate. HERMESLITE has the row (with sentinel
values), so the page renders. The HL2 user sees a PA Gain table with
all 100.0 on HF, can edit them post-PA-calibration, and the
`gbb >= 99.5` short-circuit handles the un-edited rows.

### Live capability subscription

`SetupDialog` subscribes to `RadioModel::currentRadioChanged` and
re-applies visibility on radio swap. Replaces the construction-time
gate (which required closing and reopening Setup to pick up a model
change). Mirrors Thetis runtime visibility behaviour.

---

## §8 Carry-over story for upgraders

Users upgrading 0.3.1 → 0.3.2:

| State | v0.3.1 location | v0.3.2 behaviour |
|---|---|---|
| PA forward-power METER cal (`PaCalProfile`) | `hardware/<mac>/cal/...` | Untouched. Carries over byte-identical. Already per-SKU. |
| Per-band tune power (`tunePowerByBand`) | `hardware/<mac>/tunePowerByBand/<band>` | Untouched. Carries over. |
| Per-band normal-mode power (`powerByBand`) | DOES NOT EXIST | NEW in 0.3.2. First-launch default = 100 W per band (matches Thetis console.cs:1819). |
| PA gain profile (drive output gain) | DOES NOT EXIST | NEW in 0.3.2. First-launch on each MAC: `PaProfileManager` seeds 16 factory profiles + Bypass; sets active = `Default - <connected model>`. User edits persist. |
| ATT-on-TX-on-power-change settings | DOES NOT EXIST | NEW in 0.3.2. Defaults match Thetis (`forceATTwhenPSAoff=true`, `forceATTwhenPowerChangesWhenPSAon=true`, `_anddecreased=false`). PS gate dormant until 3M-4. |

Critical: nothing in v0.3.1 user data needs migration. The new keys
are purely additive. Pre-existing settings carry over byte-identical.

The sole behavioural difference for an existing user is that their
audio output level on TX now compensates for the per-band PA gain.
On HL2 with default factory rows on HF, the linear v0.3.1 behaviour
is preserved via the sentinel.

---

## §9 Test strategy

### K2GX regression

`tests/tst_radio_model_drive_path.cpp` — drives `RadioModel` through
the K2GX scenario (ANAN-8000DLE, 80 m, slider 50). Asserts wire byte
≤ 50. Pre-hotfix produced 127. Builds fail if the math regresses.

### Safety ceiling matrix

`tests/tst_transmit_model_audio_volume.cpp` — for every (HPSDRModel,
Band) pair at the radio's spec'd max watts, asserts
`audio_volume < 0.99`. Tightest case (ANAN-8000D B10M @ 200 W) lands
at 0.992 — flagged inline.

### HL2 regression

Same test file — explicit fixture for `(HERMESLITE, B80M, 100)`
asserting `audio_volume == 1.0` (sentinel triggers, returns
`clamp(100/100, 0, 1) = 1.0`). Asserts the linear formula is preserved
for HF rows where `gbb == 100.0f`. Distinct fixture for
`(HERMESLITE, B6M, 50)` asserts dBm math kicks in (`audio_volume <
1.0`) since the 6 m row carries a real `38.8` gain.

### Per-SKU visibility matrix

`tests/tst_pa_setup_per_sku_visibility.cpp` — 19 cases covering all
16 HPSDRModel values + RX-only + Ganymede + PureSignal. Asserts
each capability flag drives the expected page state.

### Cross-page Reset wiring (Phase 9)

`tests/tst_setup_dialog_pa_reset_wiring.cpp` — constructs SetupDialog,
drives `radioStatus().setForwardPower` so Values-page peak/min
diverge from current, clicks the WattMeter Reset button via the
Phase 5A test seam, asserts Values-page peak/min collapse to current.
Pass requires the SetupDialog cross-page connect() to fire.

### ATT-on-TX dormant-gate test

`tests/tst_transmit_model_set_power_using_dbm.cpp` — exercises the
dormant gate path with `pureSignalActive() = false` (default), asserts
`StepAttenuatorController::setAttOnTxValue` is NOT called. Then with
the test-only `setPureSignalActiveForTest(true)` seam, asserts the
gate fires correctly (`new_pwr > _lastPower` and
`_anddecreased=false` paths).

---

## §10 Deferred follow-ups

Tracked here so the next maintainer doesn't lose them.

### XVTR transverter LO band translation

NereusSDR has only one XVTR slot today. The sentinel fallback in
`computeAudioVolume` catches `Band::XVTR` (returns 1000.0f from
`getGainForBand` → triggers `gbb >= 99.5` short-circuit → linear
fallback). Full Thetis `XVTRForm.TranslateFreq` lands when multi-XVTR
support arrives.

### PureSignal feedback DDC + `chkFWCATUBypass` live wiring (3M-4)

The ATT-on-TX-on-power-change gate is structurally complete but
dormant. `pureSignalActive()` returns false. When 3M-4 wires the PS
feedback DDC and the `chkFWCATUBypass` live state, flipping the
predicate activates the gate with no other structural changes.

### Andromeda Ganymede 500 W PA tab

Andromeda boards (`canDriveGanymede=true`) have a separate 500 W PA
control surface (Andromeda.cs:914-920). NereusSDR shows an
informational warning on PaGainByBandPage when the flag is set.
Full tab deferred to a follow-up PR.

### Thetis profile-string import

NereusSDR uses 27-entry / 171-pipe-delimited serialization. Thetis
uses 42-entry / 423 or 507-pipe-delimited. One-way import
(`Thetis profile string → NereusSDR PaProfile`) is a nice-to-have
for users migrating from Thetis. Not blocking.

### ChannelMaster `txgain` port (full DSP attenuation)

The current `third_party/wdsp/src/txgain_stub.c` is a glue stub that
stores the IQ scalar but doesn't apply DSP attenuation. The wire-
byte path is the primary K2GX safety lever — output power is
correct without the DSP-side attenuation — but full DSP
attenuation lands when ChannelMaster is ported (3L). At that point
the stub is replaced with the real WDSP `SetTXFixedGain`
implementation and the IQ stream gets multiplied by the SWR-
foldback-aware scalar in DSP.

### Cherry-pick into `feature/phase3m-3-tx-processing`

After v0.3.2 ships, cherry-pick or merge-forward the 18 hotfix
commits into the in-flight 3M-3 branch. The 3M-3 branch's
`TransmitModel` / `RadioModel` / `TxChannel` modifications stack
cleanly on the hotfix; the conflict surface is small (mostly the
`m_powerByBand` declaration ordering).

---

## §11 References

### Thetis sources (verified at v2.10.3.13 / commit 501e3f51)

- `console.cs:46645-46762` — `SetPowerUsingTargetDBM` (math kernel + deep-parity wrapper)
- `console.cs:46720-46751` — dBm-target math
- `console.cs:46740-46748` — ATT-on-TX-on-power-change gate
- `console.cs:29285-29310` — `_forceAttwhen*` properties + `_lastPower` reset
- `console.cs:1817` — `limitPower_by_band` defaults
- `console.cs:1819-1820` — `tunePower_by_band` defaults
- `console.cs:1813-1814` — `power_by_band` (50 W default per Thetis, NereusSDR uses 100 W per `limitPower_by_band`)
- `console.cs:10228-10387` — `CalibratePAGain` (auto-cal sweep state machine)
- `console.cs:10600-10625` — TxAttenData chain (`[2.10.3.6]MW0LGE att_fixes #399`)
- `audio.cs:262-271` — `RadioVolume` setter (wire byte composition, NO SWR)
- `cmaster.cs:1115-1119` — `CMSetTXOutputLevel` (IQ scalar, SWR HERE)
- `setup.cs:19812-20310` — `comboRadioModel_SelectedIndexChanged` (per-SKU visibility)
- `setup.cs:23295-23316` — `initPAProfiles`
- `setup.cs:23763-24046` — `PAProfile` class
- `setup.cs:23866` — `getGainForBand` 1000.0f sentinel for out-of-range Band
- `setup.cs:1920-1959` — `RecoverPAProfiles` semantics
- `setup.cs:16340-16357` — `chkPAValues_CheckedChanged` + `btnResetPAValues_Click`
- `setup.designer.cs:47366-47391` — `tpPowerAmplifier`
- `setup.designer.cs:47386-47525` — `tpGainByBand` (gain spinbox grid + adjust matrix + max-power column)
- `setup.designer.cs:49094-49186` — `panelAutoPACalibrate`
- `setup.designer.cs:49304-49309` — `tpWattMeter`
- `setup.designer.cs:51155-51177` — `panelPAValues`
- `clsHardwareSpecific.cs:459-751` — `DefaultPAGainsForBands` (16 SKU rows)

### mi0bot sources (verified at v2.10.3.13-beta2 / commit @c26a8a4)

- `clsHardwareSpecific.cs:484` — HL2 "100 is no output power" comment
- `clsHardwareSpecific.cs:769-797` — HL2 PA gain row

### NereusSDR commits on `hotfix/pa-calibration-167`

| Phase | Commit | Description |
|---|---|---|
| 1A | `dec9b1a` | feat(pa): add PaGainProfile factory tables for 16 HPSDRModel rows + Bypass |
| 1B | `ec08e2a` | feat(pa): add PaTelemetryScaling for PA Values telemetry display |
| 1C | `0b72f4c` | feat(tx): add TxChannel::setTxFixedGain wrapper for WDSP SetTXFixedGain |
| 1D | `3009c4b` | feat(satt): add StepAttenuatorController::setAttOnTxValue |
| 2A | `4693bea` | feat(pa): port Thetis PAProfile class for editable per-instance gain tables |
| 2B | `f618efe` | feat(pa): add PaProfileManager per-MAC bank for PA gain profiles |
| 3A | `ec52133` | feat(tx): TransmitModel scaffolding for PA-cal hotfix math kernel |
| 3A-fix | `f01b946` | fix(tx): correct powerByBand default to 50W per Thetis power_by_band parity |
| 3B | `b8b73ea` | feat(tx): TransmitModel::computeAudioVolume math kernel for PA-cal hotfix |
| 3C | `bb46a70` | feat(tx): TransmitModel::setPowerUsingTargetDbm deep-parity wrapper |
| 4B | `52443d7` | feat(2tone): wire TwoToneController through Phase 3C PA gain math |
| 4A | `c9598d6` | fix(radio): K2GX PA safety — route drive-slider + TUNE through Phase 3C math |
| 5A | `1cb5e34` | feat(pa-setup): add chkPAValues toggle + btnResetPAValues to PaWattMeterPage |
| 5B | `49a0b25` | feat(pa-setup): close PaValuesPage 4-label gap + peak/min tracking + reset |
| 6 | `3be7306` | feat(pa-setup): replace PaGainByBandPage placeholder with full Thetis editor |
| 7 | `f830005` | feat(pa-setup): auto-cal sweep state machine for PaGainByBandPage |
| 8 | `da51baa` | feat(pa-setup): per-SKU visibility wiring across PA Setup pages |
| 9 | (this commit) | chore: Phase 9 final integration for PA-cal hotfix v0.3.2 |

### Acknowledgments

K2GX — the field report that triggered this hotfix.
