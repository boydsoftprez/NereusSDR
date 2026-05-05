# HL2 TX Power / Tune Power UI + DSP — mi0bot Parity Design

**Issue**: [#175](https://github.com/boydsoftprez/NereusSDR/issues/175) (Chris W4ORS — HL2 RF Power and Tune Power scales show 0-100 W instead of 0-5 W)

**Author**: J.J. Boyd ~ KG4VCF (with Claude assist)
**Date**: 2026-05-04
**Status**: Design — pending JJ review

---

## 1. Overview

NereusSDR currently treats the TxApplet RF Power and Tune Power sliders, the per-band Tune Power grid in Setup → Transmit → Power, and the underlying TX power/tune DSP path as if every connected radio were a 100 W class radio. On HL2 (5 W max output) the visual scales mislead users, and the DSP path skips mi0bot's HL2-specific sub-step modulation and audio-volume formula entirely.

This design ports the **complete mi0bot-Thetis HL2 TX power/tune model** into NereusSDR. After this change, an HL2 user coming from mi0bot finds:

- Sliders with mi0bot's HL2 ranges and step sizes
- Setup → Transmit "Tune" group box (`grpPATune`) faithfully replicating mi0bot's layout
- The Tune Power spinbox in dB (-16.5..0 in 0.5 dB steps) on HL2
- The same TX-volume-on-the-wire behavior as mi0bot — sub-step DSP modulation in the 0-51 slider range, attenuator-step quantization in the 52-99 slider range, mi0bot's `(hl2Power * gbb / 100) / 93.75` audio-volume formula

PR #178 (the v0.3.2 PA-cal hotfix) already corrected the **meter** display (gauge tops out at 0-6 W on HL2). This design closes the remaining gap on **input controls** and **DSP behavior**.

## 2. Goals

- Full UX parity with mi0bot-Thetis on HL2 for the TX power/tune surfaces.
- Byte-for-byte matching DSP behavior on HL2 (slider value → wire byte → audio scalar).
- Zero regression on non-HL2 SKUs (ANAN-100, ANAN-G2, Hermes, etc.).
- All ports carry mi0bot attribution per `docs/attribution/HOW-TO-PORT.md`.
- Test coverage that catches regression at the math level (without HL2 hardware).

## 3. Non-goals

- HL2 ATT/filter safety audit (separate, deferred from PR #152).
- HL2 PA gain calibration UI port (`udHermesLitePAGain<band>` is a separate AppSettings migration concern; NereusSDR's `PaGainProfile.cpp:380-381` already encodes the HL2 row).
- 2-tone test power on HL2 (covered indirectly via the `setPowerUsingTargetDbm` shared kernel; no dedicated 2-tone HL2 work in this PR).
- ATT-on-TX scaffolding activation (still gated on PureSignal in Phase 3M-4).
- The mi0bot 93.75 audio-volume divisor question (their own comment notes ~95.625 derived; we copy 93.75 verbatim with the comment preserved — divergence from upstream needs separate maintainer approval).

## 4. Source-first context

Per memory rule `feedback_mi0bot_authoritative_for_hl2`: **mi0bot-Thetis is authoritative for HL2 features**. Where ramdor/Thetis and mi0bot disagree on HL2, mi0bot wins.

Verified by deep research dispatch (this session, 2026-05-04):
- Every HL2-specific power/tune behavior in this design originates in mi0bot.
- ramdor/Thetis has zero HL2 awareness in `console.cs`, `setup.cs`, or `clsHardwareSpecific.cs`.
- mi0bot tip used for cites: **`@e3375d06`**.

Per memory rule `feedback_thetis_userland_parity`: **match Thetis Setup IA / density / group-box layout 1:1; spin when it serves the user**. NereusSDR's per-band Tune Power grid is the maintainer-approved spin (gives per-band memory beyond what Thetis offers).

## 5. Component design

### 5.1 — `src/core/HpsdrModel.h` (per-SKU constants)

Hoist mi0bot's inline magic numbers into named `constexpr` per-SKU lookup tables. Single source of truth for slider ranges, attenuator math, and tune spinbox bounds.

```cpp
// New helpers — siblings of paMaxWattsFor()

// RF Power slider (ptbPWR equivalent) - main UI on the right
constexpr int   rfPowerSliderMaxFor(HPSDRModel m) noexcept;     // HL2: 90, others: 100
constexpr int   rfPowerSliderStepFor(HPSDRModel m) noexcept;    // HL2: 6,  others: 1

// Tune Power slider (ptbTune equivalent) - main UI on the right
constexpr int   tuneSliderMaxFor(HPSDRModel m) noexcept;        // HL2: 99, others: 100
constexpr int   tuneSliderStepFor(HPSDRModel m) noexcept;       // HL2: 3,  others: 1

// Fixed-mode tune spinbox (udTXTunePower equivalent) - Setup → Transmit → Tune group
constexpr float fixedTuneSpinboxMinFor(HPSDRModel m) noexcept;  // HL2: -16.5, others: 0
constexpr float fixedTuneSpinboxMaxFor(HPSDRModel m) noexcept;  // HL2: 0,    others: 100
constexpr float fixedTuneSpinboxStepFor(HPSDRModel m) noexcept; // HL2: 0.5,  others: 1
constexpr int   fixedTuneSpinboxDecimalsFor(HPSDRModel m) noexcept; // HL2: 1, others: 0
constexpr const char* fixedTuneSpinboxSuffixFor(HPSDRModel m) noexcept; // HL2: " dB", others: " W"

// HL2 attenuator hardware model (used by RF Power label formula)
constexpr float hl2AttenuatorDbPerStep() noexcept { return 0.5f; }
constexpr int   hl2AttenuatorStepCount() noexcept { return 16; } // 0, 1, ..., 15 levels (slider 0/6/12/.../90)
```

**Inline cites required** at every constant: `// From mi0bot-Thetis console.cs:2101-2108 [@e3375d06]` etc.

### 5.2 — `src/gui/applets/TxApplet.{h,cpp}`

Add a new helper `rescalePowerSlidersForModel(HPSDRModel)` next to existing `rescaleFwdGaugeForModel`:

```cpp
void TxApplet::rescalePowerSlidersForModel(HPSDRModel m) {
    const QSignalBlocker rfBlock(m_rfPowerSlider);
    const QSignalBlocker tunBlock(m_tunePwrSlider);

    // Per-SKU slider ranges from HpsdrModel.h
    m_rfPowerSlider->setRange(0, rfPowerSliderMaxFor(m));
    m_rfPowerSlider->setSingleStep(rfPowerSliderStepFor(m));
    m_rfPowerSlider->setPageStep(rfPowerSliderStepFor(m));

    m_tunePwrSlider->setRange(0, tuneSliderMaxFor(m));
    m_tunePwrSlider->setSingleStep(tuneSliderStepFor(m));
    m_tunePwrSlider->setPageStep(tuneSliderStepFor(m));

    // Tooltip rewrite — Thetis-faithful on every SKU.
    // From Thetis console.resx [v2.10.3.15]
    m_rfPowerSlider->setToolTip(QStringLiteral(
        "Transmit Drive — relative value, not watts unless the PA profile "
        "is configured with MAX watts @ 100%"));
    m_tunePwrSlider->setToolTip(QStringLiteral(
        "Tune and/or 2Tone Drive — relative value, not watts unless the "
        "PA profile is configured with MAX watts @ 100%"));

    updatePowerSliderLabels();   // refresh m_rfPowerValue, m_tunePwrValue
}
```

Wired from the existing `currentRadioChanged` lambda alongside the gauge rescale call.

**Label formatting** (`updatePowerSliderLabels` helper).

Trigger sources (all wired in `wireControls`):
- `m_rfPowerSlider::valueChanged` → label refresh.
- `m_tunePwrSlider::valueChanged` → label refresh.
- `RadioModel::currentRadioChanged` → label refresh (formula per-SKU may have changed).
- `rescalePowerSlidersForModel` → final call at end-of-function.

```cpp
void TxApplet::updatePowerSliderLabels() {
    const auto m = m_model->hardwareProfile().model;
    const int  rfVal  = m_rfPowerSlider->value();
    const int  tunVal = m_tunePwrSlider->value();

    if (m == HPSDRModel::HERMESLITE) {
        // From mi0bot-Thetis console.cs:29245-29274 [@e3375d06]
        // formula: (round(drv/6.0)/2) - 7.5  →  -7.5..0 dB in 0.5 dB steps
        const float rfDb  = (std::round(rfVal  / 6.0f) / 2.0f) - 7.5f;
        m_rfPowerValue->setText(QString::number(rfDb, 'f', 1));
        // Tune slider on HL2: dB display from 0..99 slider -> -16.5..0 dB
        const float tunDb = (tunVal / 3.0f - 33.0f) / 2.0f;
        m_tunePwrValue->setText(QString::number(tunDb, 'f', 1));
    } else {
        m_rfPowerValue->setText(QString::number(rfVal));
        m_tunePwrValue->setText(QString::number(tunVal));
    }
}
```

### 5.3 — `src/gui/setup/TransmitSetupPages.cpp` (PowerPage — `grpPATune` port)

**New method** `buildTuneGroup()` — replicates mi0bot's `grpPATune` "Tune" group box. Placed at the top of PowerPage, above the existing per-band grid.

Layout:
```
┌─────────────────────────────────────────────┐
│ Tune                                        │
│  Drive Source:                              │
│   ○ Fixed                                   │
│   ● Tune Slider     (default)               │
│   ○ Drive Slider                            │
│                                             │
│  TX TUN Meter:  [Forward Power      ▼]      │
│                                             │
│  Fixed Tune Power:  [    0  ▲▼]  W (or dB)  │
└─────────────────────────────────────────────┘
```

Three radio buttons bound to `TransmitModel::setTuneDrivePowerSource(DrivePowerSource)`. Pattern already exists in `TestTwoTonePage.cpp` — copy that pattern.

TX TUN Meter combo: drives `TransmitModel::setTuneTxMeter(MeterTXMode)`. Options match mi0bot enum.

Fixed Tune Power spinbox:
- Bound to `TransmitModel::m_tunePower` (existing `FixedTunePower` AppSettings key).
- On HL2: `setRange(-16.5, 0)`, `setSingleStep(0.5)`, `setDecimals(1)`, suffix `" dB"`.
- On non-HL2: `setRange(0, 100)`, `setSingleStep(1)`, `setDecimals(0)`, suffix `" W"`.
- Reconfigures on `currentRadioChanged`.
- **Disabled** when drive source != Fixed (greyed out).

**Per-band grid** (`buildTunePowerGroup`, NereusSDR-original): kept visible on every SKU. On HL2, each spinbox flips to dB display.

Widget type change: convert `m_tunePwrSpins[14]` from `QSpinBox` to `QDoubleSpinBox` so decimals can vary per SKU. On non-HL2 this is functionally identical to today (decimals=0, integer steps).

- HL2: `setRange(-16.5, 0)`, `setSingleStep(0.5)`, `setDecimals(1)`, suffix `" dB"`. Display value = `(stored_int / 3.0 - 33.0) / 2.0`. On user edit, store back as `(33 + dB*2)*3` (rounded to int). Storage stays as int 0-99 (mi0bot encoding) under the same `tunePower_by_band/<band>` AppSettings key — semantic polymorphs by SKU.
- Non-HL2: `setRange(0, 100)`, `setSingleStep(1)`, `setDecimals(0)`, suffix `" W"`. Display == stored.
- Reconfigures on `currentRadioChanged`.

### 5.4 — `src/models/TransmitModel.{h,cpp}`

#### 5.4.1 — `setPowerUsingTargetDbm` HL2 sub-step modulation (txMode 1 TUNE_SLIDER)

Port mi0bot `console.cs:47660-47673 [@e3375d06]` verbatim:

```cpp
case 1:  // tune mode
    switch (m_tuneDrivePowerSource) {
        case DrivePowerSource::DriveSlider:
            new_pwr = m_power;
            break;
        case DrivePowerSource::TuneSlider:
            new_pwr = tunePowerForBand(currentBand);
            // From mi0bot-Thetis console.cs:47660-47673 [@e3375d06]
            // MI0BOT: As HL2 only has 15 step output attenuator,
            //         reduce the level further
            if (model == HPSDRModel::HERMESLITE) {
                if (result.bConstrain) {
                    new_pwr = std::clamp(new_pwr, 0, 99);
                }
                if (new_pwr <= 51) {
                    m_txPostGenToneMag = (new_pwr + 40) / 100.0;
                    new_pwr = 0;
                } else {
                    m_txPostGenToneMag = 0.9999;
                    new_pwr = (new_pwr - 54) * 2;
                }
                emit txPostGenToneMagChanged(m_txPostGenToneMag);
            }
            break;
        case DrivePowerSource::Fixed:
            new_pwr = m_tunePower;
            result.bConstrain = false;
            break;
    }
    break;
```

#### 5.4.2 — `computeAudioVolume` HL2 audio-volume formula

Port mi0bot `console.cs:47775-47778 [@e3375d06]` verbatim. **Branch order matters**: the HL2 path must run BEFORE the existing `gbb >= 99.5` sentinel-fallback short-circuit. Otherwise HF HL2 bands (where `gbb=100.0f` per `kHermesliteRow`) keep using the legacy linear `slider/100` path and never see mi0bot's formula.

```cpp
double TransmitModel::computeAudioVolume(int sliderWatts, Band band, ...) {
    const float gbb = profile.getGainForBand(band, sliderWatts);

    // From mi0bot-Thetis console.cs:47775-47778 [@e3375d06]
    // MI0BOT: We want to jump in steps of 16 but getting 6.
    //         Drive value is 0-255 but only top 4 bits used.
    //         Need to correct for multiplication of 1.02 in
    //         Radio volume / Formula - 1/((16/6)/(255/1.02))
    //
    // BRANCH ORDER: HL2 path before gbb sentinel. On HL2 HF bands gbb=100
    // (sentinel), but mi0bot uses (hl2Power * gbb/100) / 93.75 directly —
    // the sentinel path was a NereusSDR-original fallback for radios that
    // had no PA-gain compensation; mi0bot has explicit HL2 math.
    if (model == HPSDRModel::HERMESLITE) {
        const double hl2Power = static_cast<double>(sliderWatts);
        return std::clamp((hl2Power * (gbb / 100.0)) / 93.75, 0.0, 1.0);
    }

    // gbb >= 99.5 sentinel for non-HL2 radios with no PA-gain compensation
    if (gbb >= 99.5f) {
        const double linear = static_cast<double>(sliderWatts) / 100.0;
        return std::clamp(linear, 0.0, 1.0);
    }

    // ... existing non-HL2 dBm/target_volts formula ...
}
```

`sliderWatts` here is the un-mutated slider value (pre-dBm-math). Matches mi0bot's `hl2Power = (double)new_pwr` snapshot at `console.cs:47731` — captured **before** lines 47733+ mutate `new_pwr`.

#### 5.4.3 — New property `m_txPostGenToneMag`

```cpp
// TransmitModel.h
double txPostGenToneMag() const noexcept { return m_txPostGenToneMag; }
void setTxPostGenToneMag(double mag);    // for testing / direct setpoint
Q_SIGNAL void txPostGenToneMagChanged(double mag);

private:
    // From mi0bot-Thetis console.cs:47666 [@e3375d06]
    // DSP audio-gain modulation parameter for HL2 sub-step tune.
    // Range 0.4..0.9999 on HL2 sub-step path; 1.0 unused on non-HL2.
    double m_txPostGenToneMag{1.0};
```

The `txPostGenToneMagChanged` signal must propagate to the WDSP TX channel via the existing `TxChannel` wrapper. WDSP API call: `SetTXAPostGenToneMag(channel, mag)` (or equivalent — verify exact name during impl).

#### 5.4.4 — Inline cite cleanup

`TransmitModel.cpp:475` (the `setTunePowerForBand` cite to `console.cs:12094`) is a lie — that line in Thetis declares nothing of the kind. The per-band tune power array is fully NereusSDR-original. Replace cite with a NereusSDR-original tag:

```cpp
// NereusSDR-original: per-band tune-power memory.
// Thetis stores a single global tune_power; we extend it to per-band so
// the operator does not have to reset on band change.
void TransmitModel::setTunePowerForBand(Band band, int value) {
    // ... clamp logic ...
}
```

**Clamp range** flips polymorphically by SKU:
- HL2: clamp to `[0, 99]` (slider range, mi0bot encoding).
- Non-HL2: clamp to `[0, 100]` (existing behavior).

### 5.5 — WDSP wrapper (`src/core/TxChannel.{h,cpp}` + WDSP)

Add `TxChannel::setPostGenToneMag(double)` thin wrapper around the WDSP setter. The WDSP function exists; the C# P/Invoke at `dsp.cs` declares it. Existing TxChannel pattern (10 EQ + 5 Lev + 7 ALC setters from 3M-3a-i) shows the boilerplate.

Wire the model signal `txPostGenToneMagChanged` → channel setter through the existing `RadioModel` glue path.

## 6. Persistence

**Mi0bot pattern**: same AppSettings key, polymorphic semantic by SKU. No migration step. Matches mi0bot exactly per `feedback_thetis_parity_over_local_optimization`.

| Key | Storage | HL2 semantic | Non-HL2 semantic |
|-----|---------|--------------|------------------|
| `FixedTunePower` | int | 0-99 (mi0bot dB-encoded) | 0-100 (watts) |
| `tunePower_by_band/<band>` | int | 0-99 (mi0bot dB-encoded) | 0-100 (watts) |
| `TuneDrivePowerSource` | int | enum DrivePowerSource | enum DrivePowerSource |
| `TuneTxMeter` | int | enum MeterTXMode | enum MeterTXMode |
| `m_txPostGenToneMag` | NOT persisted | runtime-only | unused (always 1.0) |

**Migration risk for HL2 users upgrading from v0.3.x to v0.3.3+**: existing per-band stored value of, e.g., `5` (intent: "5 W") will be reinterpreted as int 0-99 (intent: ~`-15.8 dB`). Equivalent to mi0bot's behavior when you flip the model selector to HERMESLITE in Setup. Acceptable — mi0bot itself does no migration. Document in changelog.

## 7. Tests

### 7.1 — `tests/tst_hpsdr_model_pa_constants.cpp` (NEW)

Per-SKU constants assertion suite. For HERMESLITE, ANAN_100, ANAN_G2_1K, HERMES_II:
- `rfPowerSliderMaxFor` returns expected value.
- `rfPowerSliderStepFor` returns expected value.
- Same for tune slider, fixed-mode spinbox bounds, decimals, suffix string.

### 7.2 — `tests/tst_tx_applet_hl2_slider.cpp` (NEW)

Headless construction guard required (TxApplet pattern). Validates:
- After `rescalePowerSlidersForModel(HPSDRModel::HERMESLITE)`, `m_rfPowerSlider->maximum() == 90`, single/page step == 6.
- After `rescalePowerSlidersForModel(HPSDRModel::ANAN_100)`, max == 100, step == 1.
- Radio swap HL2 → ANAN-100 → HL2 leaves slider in expected end state.
- RF Power label on HL2 with slider at 60 reads `"-2.5"` (formula sanity check).
- RF Power label on HL2 with slider at 90 reads `"0.0"`.
- Non-HL2 label is bare integer.

### 7.3 — `tests/tst_transmit_setup_power_page_hl2.cpp` (NEW)

- `grpPATune` group box exists with 3 radios + meter combo + Fixed spinbox.
- On HL2: Fixed spinbox range `-16.5..0`, decimals 1, suffix `" dB"`.
- On non-HL2: Fixed spinbox range `0..100`, decimals 0, suffix `" W"`.
- Per-band grid: 14 spinboxes, all reflect SKU-aware range/suffix.
- Radio swap reconfigures all controls.
- Drive-source radio toggle persists to `TransmitModel::tuneDrivePowerSource`.

### 7.4 — `tests/tst_transmit_model_hl2_dbm.cpp` (NEW)

`setPowerUsingTargetDbm(txMode=1, tuneSource=TuneSlider, model=HERMESLITE)`:
- Sweep slider 0..99 in steps of 3.
- Assert at each step: `m_txPostGenToneMag` and resulting `new_pwr` match mi0bot formulas:
  - 0 ≤ slider ≤ 51 → `mag = (slider + 40) / 100`, `new_pwr = 0`
  - 52 ≤ slider ≤ 99 → `mag = 0.9999`, `new_pwr = (slider - 54) * 2`
- Cross-check with non-HL2 path (no mutation, `new_pwr = slider`, `mag` untouched).

### 7.5 — `tests/tst_transmit_model_hl2_audio_volume.cpp` (NEW)

`computeAudioVolume(sliderWatts, band, model=HERMESLITE)`:
- Reference table of (sliderWatts, gbb, expected_volume) values manually computed from `(hl2Power * (gbb/100)) / 93.75` clamped to [0, 1].
- Cross-check non-HL2 path uses the existing `target_volts / 0.8` formula.

### 7.6 — `tests/tst_transmit_model_hl2_persistence.cpp` (NEW)

- Save HL2-mode value; load; assert round-trip.
- dB-to-int and int-to-dB conversions: `(33 + dB*2)*3` and inverse `(value/3 - 33)/2` match mi0bot at boundary points (-16.5 → 0, 0 → 99, -7.5 → 49.5/50 — note half-step rounding behavior).

### 7.7 — `tests/tst_tx_channel_post_gen_tone_mag.cpp` (NEW)

- `TxChannel::setPostGenToneMag(0.5)` calls underlying WDSP setter without crash.
- Mock or stub WDSP if needed (existing `txgain_stub.c` pattern from PR #178).

## 8. Attribution

Every ported function gets:

1. **Verbatim file header** copied from mi0bot source per `docs/attribution/HOW-TO-PORT.md`. For files that already have headers (TransmitModel.cpp), append a `// --- From mi0bot-Thetis console.cs ---` section per multi-file attribution rule.
2. **Inline cite** on every ported block: `// From mi0bot-Thetis <file>:<line> [@e3375d06]`.
3. **Verbatim `// MI0BOT:` comments** preserved from upstream. Run `scripts/verify-inline-tag-preservation.py` before commit.
4. **PROVENANCE table updates** in `docs/attribution/THETIS-PROVENANCE.md` for any newly-ported NereusSDR file. Run `scripts/check-new-ports.py` before commit.

Author tags expected from mi0bot HL2 sections (verify against `docs/attribution/thetis-author-tags.json`):
- `// MI0BOT:` (canonical mi0bot tag)
- Any `//MW0LGE` or other Samphire-era tags inside ported regions

## 9. Bench validation

**This PR cannot ship to release without an HL2 bench pass.** DSP changes drive the actual signal on the wire to the radio. Tests cover the math; the bench confirms the math drives the radio correctly.

**Required bench checklist**:
- [ ] HL2 connected, dummy load, calibrated RF wattmeter (or scope into 50Ω).
- [ ] TUN button on, drive source = TuneSlider.
- [ ] Slider 0: zero output (DSP gain mod = 0.4, attenuator at min).
- [ ] Slider 25: low audible carrier, measured output ~0.05-0.1 W.
- [ ] Slider 51: still in DSP-mod regime (mag = 0.91), measured output ~0.4 W.
- [ ] Slider 52: regime crossover. Verify no audible glitch / power spike.
- [ ] Slider 60: attenuator regime starts. Output ~1-2 W.
- [ ] Slider 99: full power, ~5 W out.
- [ ] Drive source = Fixed, spinbox -7.5 dB: output should match RF Pwr slider at 60.
- [ ] SWR meter reads ~1.0 throughout (catches the rev-power scaling bug from #178 staying fixed).
- [ ] Drive source = DriveSlider with RF Pwr 60: matches Fixed -7.5 dB.

**Bench coordinator candidates**:
- JJ (if HL2 + bench gear available).
- Chris W4ORS (issue reporter; may be willing to bench).
- Steve (HL2 user from PR #178 acknowledgments).

If no bench coordinator confirmed, this PR lands merged-but-not-shipped on `feature/hl2-mi0bot-parity` until bench passes.

## 10. Known limitations / open questions

### 10.1 — mi0bot's own divergences

- **93.75 audio-volume divisor**: mi0bot's comment derives ~95.625 from `1/((16/6)/(255/1.02))` but uses 93.75. We copy 93.75 verbatim. If a bench shows the divergence is audible, file a follow-up issue — but do not deviate from upstream without maintainer approval per `feedback_source_first_exceptions`.
- **HL2 → non-HL2 radio swap reset gap in mi0bot**: mi0bot's `comboRadioModel_SelectedIndexChanged` only sets `udTXTunePower` properties on HERMESLITE branch; non-HL2 branches don't reset. NereusSDR's `currentRadioChanged` hook closes this gap (we explicitly reconfigure on every model change).
- **`limitTunePower_by_band[]` LimitValue mechanism**: mi0bot's per-band soft cap defaults to 100 on every band, applied via `ptbTune.LimitValue = limitTunePower_by_band[band]`. On HL2 with `ptbTune.Maximum = 99`, the cap is effectively above slider max — no effect. We do NOT port this to NereusSDR; if user demand emerges, file separate.

### 10.2 — Open questions for JJ

1. **WDSP `SetTXAPostGenToneMag` exact name**: research subagent noted the C# P/Invoke declares it but did not name the exact symbol. Implementer must verify against `Project Files/Source/Console/dsp.cs` and `third_party/wdsp/src/`.
2. **HL2 16-vs-15 step naming**: mi0bot comments say "16 step output attenuator" but the slider gives 15 active steps above zero. Cosmetic; impacts only `hl2AttenuatorStepCount()` constant. Defaulting to 16 for code clarity (zero-step counts).
3. **Per-band Tune Power grid on HL2 — UX call**: keeping it visible per design above (NereusSDR-original feature). Alternative: hide on HL2 to match mi0bot exactly. Going with keep-visible per `feedback_thetis_userland_parity` "spin when it serves the user".

## 11. Out of scope

- HL2 PA gain calibration UI (`udHermesLitePAGain<band>`) — already encoded in `PaGainProfile.cpp:380-381` as `kHermesliteRow`. No UI surfacing in this PR.
- HL2 ATT/filter safety audit (deferred from PR #152).
- HL2 PureSignal feedback DDC handling (Phase 3M-4).
- 2-tone test on HL2 (covered indirectly via shared `setPowerUsingTargetDbm` kernel; no dedicated UI work).
- Migrating existing AppSettings values (no migration; mi0bot pattern is polymorphic semantic on same key).

## 12. Files touched (summary)

**New**:
- `tests/tst_hpsdr_model_pa_constants.cpp`
- `tests/tst_tx_applet_hl2_slider.cpp`
- `tests/tst_transmit_setup_power_page_hl2.cpp`
- `tests/tst_transmit_model_hl2_dbm.cpp`
- `tests/tst_transmit_model_hl2_audio_volume.cpp`
- `tests/tst_transmit_model_hl2_persistence.cpp`
- `tests/tst_tx_channel_post_gen_tone_mag.cpp`

**Modified**:
- `src/core/HpsdrModel.h` — new per-SKU constants (~9 helpers + 2 attenuator constants).
- `src/gui/applets/TxApplet.{h,cpp}` — `rescalePowerSlidersForModel` + `updatePowerSliderLabels` helpers, `currentRadioChanged` hook expansion, tooltip rewrite.
- `src/gui/setup/TransmitSetupPages.{h,cpp}` — new `buildTuneGroup()` (full `grpPATune` port: 3 radios + meter combo + Fixed spinbox), per-band grid SKU-aware rescale.
- `src/models/TransmitModel.{h,cpp}` — `setPowerUsingTargetDbm` HL2 sub-step block, `computeAudioVolume` HL2 formula, new `m_txPostGenToneMag` property + signal, `setTunePowerForBand` clamp polymorphic by SKU, inline cite cleanup.
- `src/core/TxChannel.{h,cpp}` — `setPostGenToneMag` thin wrapper.
- `docs/attribution/THETIS-PROVENANCE.md` — provenance row updates for any newly-ported file.
- `CHANGELOG.md` — entry under "Unreleased / next patch" section.

**Estimated**: 13 files modified, 7 new test files. Should land in 1 PR.

## 13. Acceptance criteria

- All 7 new test executables pass.
- Existing test suite stays green (current ~325/325).
- All 5 attribution verifiers clean (`verify-thetis-headers`, `verify-inline-tag-preservation`, `verify-inline-cites`, `verify-provenance-sync`, `check-new-ports`).
- All commits GPG-signed.
- Bench checklist complete on HL2 hardware (separate sign-off, see §9).
- Issue #175 closed.

---

## Sign-off

- [ ] JJ design approval
- [ ] Spec self-review (Claude)
- [ ] Bench coordinator identified
- [ ] Implementation plan written (writing-plans skill)
