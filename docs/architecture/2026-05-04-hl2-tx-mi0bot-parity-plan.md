# HL2 TX mi0bot Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port mi0bot-Thetis's complete HL2 TX power/tune model into NereusSDR so that an HL2 user from mi0bot finds identical slider ranges, identical Setup → Transmit "Tune" group layout, identical dB-attenuator semantics, and identical TX-volume-on-the-wire behavior. Closes [#175](https://github.com/boydsoftprez/NereusSDR/issues/175).

**Architecture:** Per-SKU constants in `HpsdrModel.h` drive UI rescaling (sliders, spinboxes, labels) hooked from `RadioModel::currentRadioChanged`. `TransmitModel::setPowerUsingTargetDbm` and `TransmitModel::computeAudioVolume` gain HL2 branches that port mi0bot's sub-step DSP modulation and audio-volume formula. New `m_txPostGenToneMag` property propagates to WDSP via `TxChannel::setPostGenToneMag` thin wrapper around the existing `SetTXAPostGenToneMag` symbol. Persistence is polymorphic on the same AppSettings keys (mi0bot pattern, no migration step).

**Tech Stack:** C++20, Qt6 (`QSlider`, `QDoubleSpinBox`, `QRadioButton`, `QComboBox`, signals/slots), WDSP via existing `TxChannel` wrapper, ctest with the GUI-test pattern from existing `tst_tx_applet_*` files.

**Spec:** `docs/architecture/2026-05-04-hl2-tx-mi0bot-parity-design.md`

**mi0bot tip cited:** `[v2.10.3.13-beta2]` (verified line content unchanged between tag and HEAD `e3375d06`).

---

## Source-first protocol (apply to every task that touches Thetis logic)

Per `CLAUDE.md` source-first porting protocol and memory `feedback_subagent_thetis_source_first`:

1. **READ** the mi0bot source file at the cited line range. Do not write a function body without reading the original.
2. **SHOW** the original code in the commit message: `"Porting from mi0bot-Thetis [file]:[line range] [v2.10.3.13-beta2] — original C# logic:"` then quote.
3. **TRANSLATE** to C++20/Qt6 faithfully. Preserve every `// MI0BOT:` and `//MW0LGE` comment verbatim — these are GPL attribution.
4. **CITE** with `// From mi0bot-Thetis <file>:<line> [v2.10.3.13-beta2]` immediately above each ported block.
5. If you cannot find the source for a piece of logic, **STOP AND ASK** — do not fabricate.

If the line content has changed between `[v2.10.3.13-beta2]` and `[@e3375d06]` for any block you cite, use `[@e3375d06]` instead. Verify each cite at port time:

```bash
git -C ../../../../mi0bot-Thetis show v2.10.3.13-beta2:"Project Files/Source/Console/<file>" | sed -n '<start>,<end>p'
```

---

## Pre-port checklist (Ring 1)

Per `CLAUDE.md` "Pre-port checklist". Run before Task 4 (first source-port task):

```bash
grep -l "src/models/TransmitModel.cpp" docs/attribution/THETIS-PROVENANCE.md
grep -l "src/gui/applets/TxApplet.cpp" docs/attribution/THETIS-PROVENANCE.md
grep -l "src/gui/setup/TransmitSetupPages.cpp" docs/attribution/THETIS-PROVENANCE.md
grep -l "src/core/TxChannel.cpp" docs/attribution/THETIS-PROVENANCE.md
grep -l "src/core/HpsdrModel.h" docs/attribution/THETIS-PROVENANCE.md
```

For each file that returns nothing, the port adds new mi0bot lines and is a **new attribution event** — header + PROVENANCE row required in the same commit. Files that already have provenance just need new inline cites.

---

## File structure

**Modified:**
- `src/core/HpsdrModel.h` — add 9 per-SKU constants + 2 attenuator constants (Task 1).
- `src/core/TxChannel.h`, `src/core/TxChannel.cpp` — add `setPostGenToneMag(double)` wrapper (Task 2).
- `src/models/TransmitModel.h`, `src/models/TransmitModel.cpp` — add `m_txPostGenToneMag` property + signal (Task 3); HL2 sub-step DSP modulation in `setPowerUsingTargetDbm` (Task 4); HL2 audio-volume formula in `computeAudioVolume` (Task 5); polymorphic clamp + cite cleanup (Task 6).
- `src/gui/applets/TxApplet.h`, `src/gui/applets/TxApplet.cpp` — `rescalePowerSlidersForModel` + `updatePowerSliderLabels` helpers + tooltip rewrite (Task 7).
- `src/gui/setup/TransmitSetupPages.h`, `src/gui/setup/TransmitSetupPages.cpp` — full `grpPATune` group port (Task 8); per-band grid `QSpinBox` → `QDoubleSpinBox` + SKU-aware rescale (Task 9).
- `src/models/RadioModel.cpp` — wire `txPostGenToneMagChanged` → `TxChannel::setPostGenToneMag` (Task 10).
- `docs/attribution/THETIS-PROVENANCE.md` — provenance rows for any new attribution events (Task 11).
- `CHANGELOG.md` — entry under "Unreleased" (Task 12).

**New tests (7 files):**
- `tests/tst_hpsdr_model_pa_constants.cpp` (Task 1)
- `tests/tst_tx_channel_post_gen_tone_mag.cpp` (Task 2)
- `tests/tst_transmit_model_tx_post_gen_tone_mag.cpp` (Task 3)
- `tests/tst_transmit_model_hl2_dbm.cpp` (Task 4)
- `tests/tst_transmit_model_hl2_audio_volume.cpp` (Task 5)
- `tests/tst_transmit_model_hl2_persistence.cpp` (Task 6)
- `tests/tst_tx_applet_hl2_slider.cpp` (Task 7)
- `tests/tst_transmit_setup_power_page_hl2.cpp` (Task 8)

(Per Task 9 reuses `tst_transmit_setup_power_pa_page.cpp` — extends, doesn't replace.)

---

## Task 1: Per-SKU constants in HpsdrModel.h

**Files:**
- Test: `tests/tst_hpsdr_model_pa_constants.cpp` (new)
- Modify: `src/core/HpsdrModel.h` (append helpers near `paMaxWattsFor`, ~line 215)
- Modify: `tests/CMakeLists.txt` (register the new test executable)

- [ ] **Step 1.1: Write the failing test**

Create `tests/tst_hpsdr_model_pa_constants.cpp`:

```cpp
// Per-SKU PA constants assertion suite for mi0bot-Thetis HL2 parity.
// Spec: docs/architecture/2026-05-04-hl2-tx-mi0bot-parity-design.md §5.1

#include <QtTest/QtTest>
#include "HpsdrModel.h"

using NereusSDR::HPSDRModel;

class TestHpsdrModelPaConstants : public QObject {
    Q_OBJECT
private slots:
    void hl2_rfPowerSlider() {
        QCOMPARE(NereusSDR::rfPowerSliderMaxFor(HPSDRModel::HERMESLITE),  90);
        QCOMPARE(NereusSDR::rfPowerSliderStepFor(HPSDRModel::HERMESLITE),  6);
    }
    void hl2_tuneSlider() {
        QCOMPARE(NereusSDR::tuneSliderMaxFor(HPSDRModel::HERMESLITE),  99);
        QCOMPARE(NereusSDR::tuneSliderStepFor(HPSDRModel::HERMESLITE),  3);
    }
    void hl2_fixedTuneSpinbox() {
        QCOMPARE(NereusSDR::fixedTuneSpinboxMinFor(HPSDRModel::HERMESLITE), -16.5f);
        QCOMPARE(NereusSDR::fixedTuneSpinboxMaxFor(HPSDRModel::HERMESLITE),   0.0f);
        QCOMPARE(NereusSDR::fixedTuneSpinboxStepFor(HPSDRModel::HERMESLITE), 0.5f);
        QCOMPARE(NereusSDR::fixedTuneSpinboxDecimalsFor(HPSDRModel::HERMESLITE), 1);
        QCOMPARE(QString(NereusSDR::fixedTuneSpinboxSuffixFor(HPSDRModel::HERMESLITE)),
                 QStringLiteral(" dB"));
    }
    void hl2_attenuator() {
        QCOMPARE(NereusSDR::hl2AttenuatorDbPerStep(),  0.5f);
        QCOMPARE(NereusSDR::hl2AttenuatorStepCount(), 16);
    }
    void anan100_default_100W() {
        QCOMPARE(NereusSDR::rfPowerSliderMaxFor(HPSDRModel::ANAN_100), 100);
        QCOMPARE(NereusSDR::rfPowerSliderStepFor(HPSDRModel::ANAN_100), 1);
        QCOMPARE(NereusSDR::tuneSliderMaxFor(HPSDRModel::ANAN_100),    100);
        QCOMPARE(NereusSDR::tuneSliderStepFor(HPSDRModel::ANAN_100),     1);
        QCOMPARE(NereusSDR::fixedTuneSpinboxMinFor(HPSDRModel::ANAN_100),  0.0f);
        QCOMPARE(NereusSDR::fixedTuneSpinboxMaxFor(HPSDRModel::ANAN_100), 100.0f);
        QCOMPARE(QString(NereusSDR::fixedTuneSpinboxSuffixFor(HPSDRModel::ANAN_100)),
                 QStringLiteral(" W"));
    }
    void ananG2_1k_default_100W_slider() {
        // Slider range stays 0-100 on every SKU - the per-band PA gain table
        // does the actual scaling. mi0bot only deviates for HERMESLITE.
        QCOMPARE(NereusSDR::rfPowerSliderMaxFor(HPSDRModel::ANAN_G2_1K), 100);
        QCOMPARE(NereusSDR::tuneSliderMaxFor(HPSDRModel::ANAN_G2_1K),    100);
    }
};

QTEST_MAIN(TestHpsdrModelPaConstants)
#include "tst_hpsdr_model_pa_constants.moc"
```

Register the test in `tests/CMakeLists.txt` following the existing `tst_*` pattern (find an existing entry for `tst_step_attenuator_controller` or similar and clone the structure).

- [ ] **Step 1.2: Run test, verify it fails to build**

```bash
cmake --build build --target tst_hpsdr_model_pa_constants 2>&1 | tail -20
```

Expected: build error, "rfPowerSliderMaxFor was not declared in this scope" or similar — confirms helpers don't exist yet.

- [ ] **Step 1.3: Implement constants**

Append to `src/core/HpsdrModel.h` after the `paMaxWattsFor` block (around line 215):

```cpp
// =============================================================================
// Per-SKU PA UI constants — mi0bot-Thetis HL2 parity
// =============================================================================
//
// HL2 (HERMESLITE) treats RF Power and Tune Power sliders as dB attenuators
// rather than watts targets. Specifically:
//   - RF Power slider: Max=90, step=6 (16-step output attenuator, 0.5 dB/step)
//   - Tune Power slider: Max=99, step=3 (33 sub-steps; 0..51 = DSP audio gain
//     modulation; 52..99 = PA attenuator territory)
//   - Fixed-mode Tune Power spinbox: -16.5..0 dB, 0.5 dB increments
//
// Non-HL2 SKUs keep the canonical Thetis 0..100 unitless drive scale.
//
// From mi0bot-Thetis console.cs:2101-2108 [v2.10.3.13-beta2]
//   ptbPWR.Maximum = 90;        // MI0BOT: Changes for HL2 only having a 16 step output attenuator
//   ptbPWR.LargeChange = 6;
//   ptbPWR.SmallChange = 6;
//   ptbTune.Maximum = 99;
//   ptbTune.LargeChange = 3;
//   ptbTune.SmallChange = 3;
// From mi0bot-Thetis setup.cs:20328-20331 [v2.10.3.13-beta2]
//   udTXTunePower.DecimalPlaces = 1;
//   udTXTunePower.Increment = (decimal)0.5;
//   udTXTunePower.Maximum = (decimal)0;
//   udTXTunePower.Minimum = (decimal)-16.5;

constexpr int rfPowerSliderMaxFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 90 : 100;
}

constexpr int rfPowerSliderStepFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 6 : 1;
}

constexpr int tuneSliderMaxFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 99 : 100;
}

constexpr int tuneSliderStepFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 3 : 1;
}

constexpr float fixedTuneSpinboxMinFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? -16.5f : 0.0f;
}

constexpr float fixedTuneSpinboxMaxFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 0.0f : 100.0f;
}

constexpr float fixedTuneSpinboxStepFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 0.5f : 1.0f;
}

constexpr int fixedTuneSpinboxDecimalsFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 1 : 0;
}

constexpr const char* fixedTuneSpinboxSuffixFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? " dB" : " W";
}

// HL2 attenuator hardware model (used by RF Power label formula at TxApplet
// updatePowerSliderLabels). 16 levels (slider 0/6/12/.../84/90), 0.5 dB per
// half-step → -7.5..0 dB total range.
//
// From mi0bot-Thetis console.cs:29245-29274 [v2.10.3.13-beta2] -
//   formula: lblPWR.Text = "Drive: " + ((round(drv/6.0)/2) - 7.5) + "dB"
constexpr float hl2AttenuatorDbPerStep() noexcept { return 0.5f; }
constexpr int   hl2AttenuatorStepCount() noexcept { return 16; }
```

- [ ] **Step 1.4: Run test, verify it passes**

```bash
cmake --build build --target tst_hpsdr_model_pa_constants && \
  ctest --test-dir build -R tst_hpsdr_model_pa_constants --output-on-failure
```

Expected: 1/1 pass.

- [ ] **Step 1.5: Commit**

```bash
git add src/core/HpsdrModel.h tests/tst_hpsdr_model_pa_constants.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(hpsdr-model): per-SKU PA UI constants for HL2 mi0bot parity

Adds 9 constexpr helpers + 2 attenuator constants in HpsdrModel.h:
rfPowerSliderMaxFor, rfPowerSliderStepFor, tuneSliderMaxFor,
tuneSliderStepFor, fixedTuneSpinboxMin/Max/Step/Decimals/SuffixFor,
hl2AttenuatorDbPerStep, hl2AttenuatorStepCount.

HL2 (HERMESLITE) gets mi0bot's attenuator semantics: RF Power slider
0-90 step 6 (16-step output attenuator, 0.5 dB/step), Tune Power slider
0-99 step 3, Fixed-mode Tune Power spinbox -16.5 to 0 in 0.5 dB
increments. Other SKUs keep the canonical Thetis 0-100 scale.

Cites mi0bot-Thetis console.cs:2101-2108 + setup.cs:20328-20331 +
console.cs:29245-29274 [v2.10.3.13-beta2]. Verified line content
unchanged between v2.10.3.13-beta2 tag and HEAD e3375d06.

Test: tst_hpsdr_model_pa_constants.cpp (5 cases) verifies HL2 +
ANAN-100 + ANAN-G2-1K constant tables.

Spec: docs/architecture/2026-05-04-hl2-tx-mi0bot-parity-design.md §5.1
Plan: docs/architecture/2026-05-04-hl2-tx-mi0bot-parity-plan.md Task 1
Issue: #175

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: TxChannel WDSP wrapper for SetTXAPostGenToneMag

**Files:**
- Test: `tests/tst_tx_channel_post_gen_tone_mag.cpp` (new)
- Modify: `src/core/TxChannel.h` (declare `setPostGenToneMag`)
- Modify: `src/core/TxChannel.cpp` (implement, calls `SetTXAPostGenToneMag`)
- Modify: `tests/CMakeLists.txt`

WDSP function already exists at `third_party/wdsp/src/gen.c:800`:
```c
void SetTXAPostGenToneMag (int channel, double mag);
```

- [ ] **Step 2.1: Write the failing test**

Create `tests/tst_tx_channel_post_gen_tone_mag.cpp`. Mirror the existing pattern from `tst_tx_channel_eq_setters.cpp` or similar. Stub the WDSP call via the existing test harness pattern (most TxChannel tests use a `WDSP_Stub` mode):

```cpp
// TxChannel::setPostGenToneMag wrapper test.
// Spec §5.5; Plan Task 2.

#include <QtTest/QtTest>
#include "TxChannel.h"

class TestTxChannelPostGenToneMag : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        // Existing TxChannel tests run with WDSP stubbed; ensure same here.
        // (The test harness will call SetTXAPostGenToneMag through the stub
        // shim so we exercise the C++ wrapper without booting WDSP.)
    }

    void setPostGenToneMag_clampsAtBounds() {
        NereusSDR::TxChannel ch(/* channel id */ 1);
        ch.setPostGenToneMag(0.5);
        QCOMPARE(ch.postGenToneMag(), 0.5);

        // mi0bot range for HL2: 0.4 .. 0.9999. Wrapper accepts any double 0..1
        // and lets DSP clamp - we just verify round-trip.
        ch.setPostGenToneMag(0.0);
        QCOMPARE(ch.postGenToneMag(), 0.0);

        ch.setPostGenToneMag(1.0);
        QCOMPARE(ch.postGenToneMag(), 1.0);
    }

    void setPostGenToneMag_storesAndRetrieves() {
        NereusSDR::TxChannel ch(1);
        ch.setPostGenToneMag(0.4);
        ch.setPostGenToneMag(0.91);
        QCOMPARE(ch.postGenToneMag(), 0.91);
    }
};

QTEST_MAIN(TestTxChannelPostGenToneMag)
#include "tst_tx_channel_post_gen_tone_mag.moc"
```

Register in `tests/CMakeLists.txt`.

- [ ] **Step 2.2: Run test, verify build failure**

```bash
cmake --build build --target tst_tx_channel_post_gen_tone_mag 2>&1 | tail -10
```

Expected: "no member named 'setPostGenToneMag'".

- [ ] **Step 2.3: Implement wrapper**

Add to `src/core/TxChannel.h` (declaration block near other setters):

```cpp
public:
    // From mi0bot-Thetis console.cs:47666 [v2.10.3.13-beta2]
    // DSP audio-gain modulation parameter for HL2 sub-step tune.
    // Range 0.4..0.9999 on HL2 sub-step path; 1.0 on non-HL2 (no effect).
    // Wrapper for WDSP SetTXAPostGenToneMag (third_party/wdsp/src/gen.c:800).
    void   setPostGenToneMag(double mag);
    double postGenToneMag() const noexcept { return m_postGenToneMag; }

private:
    double m_postGenToneMag{1.0};
```

Add to `src/core/TxChannel.cpp` (near other setters, e.g. after `setLevelerOnOff`):

```cpp
// From mi0bot-Thetis console.cs:47666 [v2.10.3.13-beta2]
// HL2 sub-step DSP audio-gain modulation.
//   if (new_pwr <= 51) {
//       radio.GetDSPTX(0).TXPostGenToneMag = (double)(new_pwr + 40) / 100;
//       new_pwr = 0;
//   } else {
//       radio.GetDSPTX(0).TXPostGenToneMag = 0.9999;
//       new_pwr = (new_pwr - 54) * 2;
//   }
void TxChannel::setPostGenToneMag(double mag)
{
    m_postGenToneMag = mag;
    SetTXAPostGenToneMag(m_channel, mag);
}
```

(The `SetTXAPostGenToneMag` declaration is already pulled in via `wdsp.h` or equivalent header that other setters use — verify the include is present at the top of `TxChannel.cpp`; if not, add the same WDSP header the existing setters use.)

- [ ] **Step 2.4: Run test, verify pass**

```bash
cmake --build build --target tst_tx_channel_post_gen_tone_mag && \
  ctest --test-dir build -R tst_tx_channel_post_gen_tone_mag --output-on-failure
```

Expected: 1/1 pass.

- [ ] **Step 2.5: Commit**

```bash
git add src/core/TxChannel.h src/core/TxChannel.cpp \
        tests/tst_tx_channel_post_gen_tone_mag.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(tx-channel): add setPostGenToneMag WDSP wrapper

Thin C++ wrapper around WDSP SetTXAPostGenToneMag (already present in
third_party/wdsp/src/gen.c:800). Used by TransmitModel HL2 sub-step
DSP audio-gain modulation in setPowerUsingTargetDbm (Task 4).

Cites mi0bot-Thetis console.cs:47666 [v2.10.3.13-beta2].

Test: tst_tx_channel_post_gen_tone_mag.cpp (2 cases) verifies
round-trip and bound storage.

Spec: §5.5; Plan: Task 2; Issue: #175

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: TransmitModel m_txPostGenToneMag property + signal

**Files:**
- Test: `tests/tst_transmit_model_tx_post_gen_tone_mag.cpp` (new)
- Modify: `src/models/TransmitModel.h` (declare property + setter + signal)
- Modify: `src/models/TransmitModel.cpp` (implement setter, emit signal)
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 3.1: Write the failing test**

Create `tests/tst_transmit_model_tx_post_gen_tone_mag.cpp`:

```cpp
// TransmitModel m_txPostGenToneMag property + signal.
// Spec §5.4.3; Plan Task 3.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "TransmitModel.h"

class TestTransmitModelTxPostGenToneMag : public QObject {
    Q_OBJECT
private slots:
    void defaultIs1_0() {
        NereusSDR::TransmitModel m;
        QCOMPARE(m.txPostGenToneMag(), 1.0);
    }

    void setterRoundTrip() {
        NereusSDR::TransmitModel m;
        m.setTxPostGenToneMag(0.5);
        QCOMPARE(m.txPostGenToneMag(), 0.5);
    }

    void setterEmitsSignal() {
        NereusSDR::TransmitModel m;
        QSignalSpy spy(&m, &NereusSDR::TransmitModel::txPostGenToneMagChanged);
        m.setTxPostGenToneMag(0.42);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toDouble(), 0.42);
    }

    void setterDeduplicates() {
        NereusSDR::TransmitModel m;
        m.setTxPostGenToneMag(0.5);
        QSignalSpy spy(&m, &NereusSDR::TransmitModel::txPostGenToneMagChanged);
        m.setTxPostGenToneMag(0.5);  // same value
        QCOMPARE(spy.count(), 0);    // no re-emit
    }
};

QTEST_MAIN(TestTransmitModelTxPostGenToneMag)
#include "tst_transmit_model_tx_post_gen_tone_mag.moc"
```

Register in `tests/CMakeLists.txt`.

- [ ] **Step 3.2: Run test, verify build failure**

```bash
cmake --build build --target tst_transmit_model_tx_post_gen_tone_mag 2>&1 | tail -10
```

Expected: "no member named 'txPostGenToneMag'" or "txPostGenToneMagChanged".

- [ ] **Step 3.3: Implement property**

In `src/models/TransmitModel.h`, in the public block near other DSP property setters (search for `setLevelerOnOff` or similar):

```cpp
public:
    // From mi0bot-Thetis console.cs:47666 [v2.10.3.13-beta2]
    // HL2 sub-step DSP audio-gain modulation parameter. Set by
    // setPowerUsingTargetDbm in HL2 tune-slider mode. Propagates to
    // WDSP via TxChannel::setPostGenToneMag. Range 0.4..0.9999 on HL2
    // sub-step path; 1.0 means "no modulation" (default, used on non-HL2).
    double txPostGenToneMag() const noexcept { return m_txPostGenToneMag; }
    void   setTxPostGenToneMag(double mag);

signals:
    void txPostGenToneMagChanged(double mag);

private:
    double m_txPostGenToneMag{1.0};
```

In `src/models/TransmitModel.cpp`, near other setters:

```cpp
void TransmitModel::setTxPostGenToneMag(double mag)
{
    if (m_txPostGenToneMag == mag) {
        return;  // dedupe; matches NereusSDR setter convention
    }
    m_txPostGenToneMag = mag;
    emit txPostGenToneMagChanged(mag);
}
```

- [ ] **Step 3.4: Run test, verify pass**

```bash
cmake --build build --target tst_transmit_model_tx_post_gen_tone_mag && \
  ctest --test-dir build -R tst_transmit_model_tx_post_gen_tone_mag --output-on-failure
```

Expected: 4/4 pass.

- [ ] **Step 3.5: Commit**

```bash
git add src/models/TransmitModel.h src/models/TransmitModel.cpp \
        tests/tst_transmit_model_tx_post_gen_tone_mag.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(transmit-model): add txPostGenToneMag property + signal

New property used by setPowerUsingTargetDbm HL2 tune-slider sub-step
DSP modulation (Task 4). Default 1.0 (no modulation, non-HL2 path).
HL2 path writes 0.4..0.9999 per mi0bot formula. Setter dedupes equal
values to match NereusSDR setter convention.

Wired to TxChannel::setPostGenToneMag in Task 10.

Cites mi0bot-Thetis console.cs:47666 [v2.10.3.13-beta2].

Test: tst_transmit_model_tx_post_gen_tone_mag.cpp (4 cases) verifies
default, round-trip, signal emission, and dedupe.

Spec: §5.4.3; Plan: Task 3; Issue: #175

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: TransmitModel HL2 sub-step DSP modulation in setPowerUsingTargetDbm

**Source-first prep**: read the mi0bot block before editing.

```bash
git -C ../../../../mi0bot-Thetis show v2.10.3.13-beta2:"Project Files/Source/Console/console.cs" | sed -n '47660,47680p'
```

Verify the block matches what the spec quotes. If it does NOT match, STOP and re-anchor cite line numbers against the actual tag content; update the spec and this plan task accordingly.

**Files:**
- Test: `tests/tst_transmit_model_hl2_dbm.cpp` (new)
- Modify: `src/models/TransmitModel.cpp` (txMode 1 TUNE_SLIDER branch)
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 4.1: Write the failing test**

Create `tests/tst_transmit_model_hl2_dbm.cpp`:

```cpp
// TransmitModel::setPowerUsingTargetDbm HL2 sub-step DSP modulation.
// Spec §5.4.1; Plan Task 4.
// Reference table from mi0bot-Thetis console.cs:47660-47673 [v2.10.3.13-beta2]:
//   if (new_pwr <= 51) { mag = (new_pwr + 40) / 100; new_pwr = 0; }
//   else              { mag = 0.9999;                new_pwr = (new_pwr - 54) * 2; }

#include <QtTest/QtTest>
#include "TransmitModel.h"
#include "HpsdrModel.h"

using NereusSDR::HPSDRModel;

class TestTransmitModelHl2Dbm : public QObject {
    Q_OBJECT
private slots:
    void hl2_subStep_zero() {
        NereusSDR::TransmitModel m;
        const auto r = m.setPowerUsingTargetDbm(/*tuneSlider=*/0,
                                                /*model=*/HPSDRModel::HERMESLITE,
                                                /*txMode=*/1,
                                                /*driveSrc=*/NereusSDR::DrivePowerSource::TuneSlider,
                                                /*band=*/NereusSDR::Band::B40M);
        QCOMPARE(m.txPostGenToneMag(), 0.40);   // (0 + 40) / 100
        QCOMPARE(r.newPower, 0);
    }

    void hl2_subStep_mid() {
        NereusSDR::TransmitModel m;
        const auto r = m.setPowerUsingTargetDbm(25, HPSDRModel::HERMESLITE, 1,
                                                NereusSDR::DrivePowerSource::TuneSlider,
                                                NereusSDR::Band::B40M);
        QCOMPARE(m.txPostGenToneMag(), 0.65);   // (25 + 40) / 100
        QCOMPARE(r.newPower, 0);
    }

    void hl2_subStep_boundary() {
        NereusSDR::TransmitModel m;
        const auto r = m.setPowerUsingTargetDbm(51, HPSDRModel::HERMESLITE, 1,
                                                NereusSDR::DrivePowerSource::TuneSlider,
                                                NereusSDR::Band::B40M);
        QCOMPARE(m.txPostGenToneMag(), 0.91);   // (51 + 40) / 100
        QCOMPARE(r.newPower, 0);
    }

    void hl2_attenuator_low() {
        NereusSDR::TransmitModel m;
        const auto r = m.setPowerUsingTargetDbm(52, HPSDRModel::HERMESLITE, 1,
                                                NereusSDR::DrivePowerSource::TuneSlider,
                                                NereusSDR::Band::B40M);
        QCOMPARE(m.txPostGenToneMag(), 0.9999);
        QCOMPARE(r.newPower, (52 - 54) * 2);    // -4 (will be clamped downstream)
    }

    void hl2_attenuator_top() {
        NereusSDR::TransmitModel m;
        const auto r = m.setPowerUsingTargetDbm(99, HPSDRModel::HERMESLITE, 1,
                                                NereusSDR::DrivePowerSource::TuneSlider,
                                                NereusSDR::Band::B40M);
        QCOMPARE(m.txPostGenToneMag(), 0.9999);
        QCOMPARE(r.newPower, 90);               // (99 - 54) * 2
    }

    void nonHl2_unchanged() {
        // ANAN-100 should NOT enter the HL2 sub-step path.
        NereusSDR::TransmitModel m;
        const double magBefore = m.txPostGenToneMag();
        const auto r = m.setPowerUsingTargetDbm(50, HPSDRModel::ANAN_100, 1,
                                                NereusSDR::DrivePowerSource::TuneSlider,
                                                NereusSDR::Band::B40M);
        QCOMPARE(m.txPostGenToneMag(), magBefore);  // untouched
        QCOMPARE(r.newPower, 50);                   // straight pass-through
    }
};

QTEST_MAIN(TestTransmitModelHl2Dbm)
#include "tst_transmit_model_hl2_dbm.moc"
```

Note: this test assumes `setPowerUsingTargetDbm` accepts the `model` parameter. The current signature does not — see Step 4.3 below.

Register in `tests/CMakeLists.txt`.

- [ ] **Step 4.2: Run test, verify build failure**

```bash
cmake --build build --target tst_transmit_model_hl2_dbm 2>&1 | tail -10
```

Expected: signature mismatch — `setPowerUsingTargetDbm` does not accept a `model` parameter.

- [ ] **Step 4.3: Extend signature + port HL2 branch**

The current signature at `src/models/TransmitModel.cpp` (around line 760-870) does not accept `HPSDRModel`. Extend it. The model is sourced from `RadioModel::hardwareProfile().model` upstream — pass through.

In `src/models/TransmitModel.h`, update `setPowerUsingTargetDbm` declaration to take `HPSDRModel model` (default-defaulted to `FIRST` so existing callers keep compiling — fix call sites in next subtask).

In `src/models/TransmitModel.cpp`, in the `txMode 1` TUNE_SLIDER case (around line 837-848), replace:

```cpp
        case DrivePowerSource::TuneSlider:
            new_pwr = tunePowerForBand(currentBand);
            break;
```

with:

```cpp
        case DrivePowerSource::TuneSlider:
            new_pwr = tunePowerForBand(currentBand);
            // From mi0bot-Thetis console.cs:47660-47673 [v2.10.3.13-beta2]
            // MI0BOT: As HL2 only has 15 step output attenuator,
            //         reduce the level further
            if (model == HPSDRModel::HERMESLITE) {
                if (result.bConstrain) {
                    new_pwr = std::clamp(new_pwr, 0, 99);
                }
                if (new_pwr <= 51) {
                    setTxPostGenToneMag((new_pwr + 40) / 100.0);
                    new_pwr = 0;
                } else {
                    setTxPostGenToneMag(0.9999);
                    new_pwr = (new_pwr - 54) * 2;
                }
            }
            break;
```

Find every existing caller of `setPowerUsingTargetDbm` (`grep -rn 'setPowerUsingTargetDbm' src/`) and update each to pass the connected radio model. Typical pattern in `RadioModel.cpp`:

```cpp
const auto model = m_currentRadio.hardwareProfile().model;
const auto r = m_transmitModel.setPowerUsingTargetDbm(slider, model, txMode, src, band);
```

If a call site does not have the model in scope, either thread it through or default-default to `FIRST` (which short-circuits the HL2 branch). Add a TODO comment if the latter.

- [ ] **Step 4.4: Run test, verify pass**

```bash
cmake --build build --target tst_transmit_model_hl2_dbm && \
  ctest --test-dir build -R tst_transmit_model_hl2_dbm --output-on-failure
```

Expected: 6/6 pass.

- [ ] **Step 4.5: Verify attribution preservation**

```bash
python3 scripts/verify-inline-tag-preservation.py 2>&1 | tail -5
python3 scripts/verify-inline-cites.py 2>&1 | tail -5
```

Expected: pass. If the verifier flags missing tag preservation, ensure `// MI0BOT:` comment is present verbatim above the ported block.

- [ ] **Step 4.6: Commit**

```bash
git add src/models/TransmitModel.h src/models/TransmitModel.cpp \
        tests/tst_transmit_model_hl2_dbm.cpp tests/CMakeLists.txt \
        $(grep -rl 'setPowerUsingTargetDbm' src/ | xargs git diff --name-only --cached)
git commit -m "$(cat <<'EOF'
feat(transmit-model): port mi0bot HL2 sub-step DSP modulation

setPowerUsingTargetDbm txMode 1 TUNE_SLIDER branch gains an HL2
sub-step path that mirrors mi0bot-Thetis exactly:

  if (new_pwr <= 51) {
      TXPostGenToneMag = (new_pwr + 40) / 100;  // 0.40..0.91 audio gain
      new_pwr = 0;                              // no PA attenuator change
  } else {
      TXPostGenToneMag = 0.9999;                // full audio gain
      new_pwr = (new_pwr - 54) * 2;             // PA attenuator 0..90 step 2
  }

Below slider 51 the radio uses DSP audio-gain modulation to attenuate
finer than the 16-step hardware attenuator can; from 52 to 99 it walks
the attenuator in coarse steps. Together this gives mi0bot's full
33-step Tune slider behavior.

Non-HL2 SKUs unchanged (HPSDRModel guard).

Setter signature gains HPSDRModel model parameter; callers in
RadioModel.cpp updated to thread through hardwareProfile().model.

Cites mi0bot-Thetis console.cs:47660-47673 [v2.10.3.13-beta2]. MI0BOT
attribution comment preserved verbatim.

Test: tst_transmit_model_hl2_dbm.cpp (6 cases) covers slider 0/25/51
sub-step regime, 52/99 attenuator regime, and non-HL2 pass-through.

Pre-commit verifiers (verify-inline-tag-preservation +
verify-inline-cites): clean.

Spec: §5.4.1; Plan: Task 4; Issue: #175

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: TransmitModel HL2 audio-volume formula in computeAudioVolume

**Source-first prep**: read mi0bot block.

```bash
git -C ../../../../mi0bot-Thetis show v2.10.3.13-beta2:"Project Files/Source/Console/console.cs" | sed -n '47770,47785p'
```

**Files:**
- Test: `tests/tst_transmit_model_hl2_audio_volume.cpp` (new)
- Modify: `src/models/TransmitModel.cpp` (`computeAudioVolume` HL2 branch BEFORE the `gbb >= 99.5` sentinel)
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 5.1: Write the failing test**

Create `tests/tst_transmit_model_hl2_audio_volume.cpp`:

```cpp
// TransmitModel::computeAudioVolume HL2 audio-volume formula.
// Spec §5.4.2; Plan Task 5.
// Reference: (hl2Power * (gbb / 100)) / 93.75  clamped to [0, 1]
// From mi0bot-Thetis console.cs:47775-47778 [v2.10.3.13-beta2]

#include <QtTest/QtTest>
#include "TransmitModel.h"
#include "HpsdrModel.h"

class TestTransmitModelHl2AudioVolume : public QObject {
    Q_OBJECT
private slots:
    void hl2_hf_band_at_full() {
        // HF bands have gbb=100 (sentinel) on HL2 per kHermesliteRow.
        // (99 * 100/100) / 93.75 = 99/93.75 = 1.056 -> clamped to 1.0
        NereusSDR::TransmitModel m;
        const double v = m.computeAudioVolume(99,
                                              NereusSDR::Band::B40M,
                                              NereusSDR::HPSDRModel::HERMESLITE);
        QVERIFY(qFuzzyCompare(v, 1.0));
    }

    void hl2_hf_band_at_zero() {
        NereusSDR::TransmitModel m;
        const double v = m.computeAudioVolume(0,
                                              NereusSDR::Band::B40M,
                                              NereusSDR::HPSDRModel::HERMESLITE);
        QVERIFY(qFuzzyCompare(v, 0.0));
    }

    void hl2_hf_band_at_mid() {
        // (50 * 100/100) / 93.75 = 0.5333
        NereusSDR::TransmitModel m;
        const double v = m.computeAudioVolume(50,
                                              NereusSDR::Band::B40M,
                                              NereusSDR::HPSDRModel::HERMESLITE);
        QVERIFY(qAbs(v - (50.0 / 93.75)) < 1e-9);
    }

    void hl2_6m_at_full() {
        // 6m: gbb=38.8 on HL2.
        // (99 * 38.8/100) / 93.75 = 38.412 / 93.75 = 0.4097
        NereusSDR::TransmitModel m;
        const double v = m.computeAudioVolume(99,
                                              NereusSDR::Band::B6M,
                                              NereusSDR::HPSDRModel::HERMESLITE);
        QVERIFY(qAbs(v - ((99.0 * 0.388) / 93.75)) < 1e-6);
    }

    void nonHl2_unchanged_path() {
        // ANAN-100 should NOT use the HL2 formula. The existing path is
        // unchanged - assert the result differs from the HL2 formula.
        NereusSDR::TransmitModel m;
        const double v_hl2  = (50.0 * 100.0 / 100.0) / 93.75;
        const double v_real = m.computeAudioVolume(50,
                                                   NereusSDR::Band::B40M,
                                                   NereusSDR::HPSDRModel::ANAN_100);
        QVERIFY(qAbs(v_real - v_hl2) > 1e-6);
    }
};

QTEST_MAIN(TestTransmitModelHl2AudioVolume)
#include "tst_transmit_model_hl2_audio_volume.moc"
```

Register in `tests/CMakeLists.txt`.

- [ ] **Step 5.2: Run test, verify build failure or wrong values**

```bash
cmake --build build --target tst_transmit_model_hl2_audio_volume 2>&1 | tail -10
ctest --test-dir build -R tst_transmit_model_hl2_audio_volume --output-on-failure
```

Expected: signature mismatch (model param missing) OR wrong result on HL2 cases.

- [ ] **Step 5.3: Implement HL2 branch**

In `src/models/TransmitModel.cpp`, in `computeAudioVolume` (around line 660), find the existing `gbb >= 99.5` sentinel and place the HL2 branch BEFORE it:

```cpp
double TransmitModel::computeAudioVolume(int sliderWatts,
                                         Band band,
                                         HPSDRModel model) const
{
    // ... existing setup, profile lookup ...
    const float gbb = profile.getGainForBand(band, sliderWatts);

    // From mi0bot-Thetis console.cs:47775-47778 [v2.10.3.13-beta2]
    // MI0BOT: We want to jump in steps of 16 but getting 6.
    //         Drive value is 0-255 but only top 4 bits used.
    //         Need to correct for multiplication of 1.02 in
    //         Radio volume / Formula - 1/((16/6)/(255/1.02))
    //
    // Branch order: HL2 path BEFORE gbb sentinel. On HL2 HF bands gbb=100
    // (sentinel value), but mi0bot uses (hl2Power * gbb/100) / 93.75
    // directly - the sentinel was a NereusSDR-original fallback for radios
    // with no PA-gain compensation; mi0bot has explicit HL2 math.
    if (model == HPSDRModel::HERMESLITE) {
        const double hl2Power = static_cast<double>(sliderWatts);
        return std::clamp((hl2Power * (gbb / 100.0)) / 93.75, 0.0, 1.0);
    }

    if (gbb >= 99.5f) {
        const double linear = static_cast<double>(sliderWatts) / 100.0;
        return std::clamp(linear, 0.0, 1.0);
    }

    // ... existing non-HL2 dBm/target_volts formula ...
}
```

Update the declaration in `src/models/TransmitModel.h` to add `HPSDRModel model` parameter (default-default to `FIRST` for safety on non-HL2 callers if any).

Update existing callers (`grep -rn 'computeAudioVolume' src/`) to pass `model` from `hardwareProfile().model`.

- [ ] **Step 5.4: Run test, verify pass**

```bash
cmake --build build --target tst_transmit_model_hl2_audio_volume && \
  ctest --test-dir build -R tst_transmit_model_hl2_audio_volume --output-on-failure
```

Expected: 5/5 pass.

- [ ] **Step 5.5: Verify attribution**

```bash
python3 scripts/verify-inline-tag-preservation.py 2>&1 | tail -3
python3 scripts/verify-inline-cites.py 2>&1 | tail -3
```

- [ ] **Step 5.6: Commit**

```bash
git add src/models/TransmitModel.h src/models/TransmitModel.cpp \
        tests/tst_transmit_model_hl2_audio_volume.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(transmit-model): port mi0bot HL2 audio-volume formula

computeAudioVolume gains an HL2 branch before the gbb >= 99.5 sentinel:

    if (model == HERMESLITE) {
        return clamp((hl2Power * (gbb / 100)) / 93.75, 0, 1);
    }

Branch order matters: on HL2 HF bands gbb=100 (sentinel value), but
mi0bot uses the formula directly - the sentinel was a NereusSDR-original
linear fallback for radios with no PA-gain compensation; mi0bot has
explicit HL2 math. Placing the HL2 branch before the sentinel ensures
HL2 HF bands actually go through mi0bot's formula instead of falling
into the legacy linear path.

mi0bot's own comment notes the divisor 93.75 has a known empirical
discrepancy with their derived value (~95.6 from
1/((16/6)/(255/1.02))). Copying verbatim per source-first; the
discrepancy is upstream-known and not a NereusSDR issue.

Cites mi0bot-Thetis console.cs:47775-47778 [v2.10.3.13-beta2]. MI0BOT
attribution comments (4 lines) preserved verbatim.

Test: tst_transmit_model_hl2_audio_volume.cpp (5 cases) covers HF
bands at zero/mid/full, 6m at full, and non-HL2 path divergence.

Spec: §5.4.2; Plan: Task 5; Issue: #175

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: TransmitModel polymorphic clamp + cite cleanup

**Files:**
- Test: `tests/tst_transmit_model_hl2_persistence.cpp` (new)
- Modify: `src/models/TransmitModel.cpp` (`setTunePowerForBand` clamp polymorphic; cite cleanup at line 475)
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 6.1: Write the failing test**

Create `tests/tst_transmit_model_hl2_persistence.cpp`:

```cpp
// TransmitModel HL2 polymorphic clamp + persistence semantic.
// Spec §6; Plan Task 6.

#include <QtTest/QtTest>
#include "TransmitModel.h"
#include "HpsdrModel.h"

class TestTransmitModelHl2Persistence : public QObject {
    Q_OBJECT
private slots:
    void hl2_clamp_to_99() {
        // On HL2, per-band tune power clamps [0, 99] (mi0bot encoding).
        NereusSDR::TransmitModel m;
        m.setHpsdrModel(NereusSDR::HPSDRModel::HERMESLITE);
        m.setTunePowerForBand(NereusSDR::Band::B40M, 150);
        QCOMPARE(m.tunePowerForBand(NereusSDR::Band::B40M), 99);
    }

    void nonHl2_clamp_to_100() {
        // On non-HL2, per-band tune power clamps [0, 100] (existing behavior).
        NereusSDR::TransmitModel m;
        m.setHpsdrModel(NereusSDR::HPSDRModel::ANAN_100);
        m.setTunePowerForBand(NereusSDR::Band::B40M, 150);
        QCOMPARE(m.tunePowerForBand(NereusSDR::Band::B40M), 100);
    }

    void hl2_negative_clamps_to_zero() {
        NereusSDR::TransmitModel m;
        m.setHpsdrModel(NereusSDR::HPSDRModel::HERMESLITE);
        m.setTunePowerForBand(NereusSDR::Band::B40M, -10);
        QCOMPARE(m.tunePowerForBand(NereusSDR::Band::B40M), 0);
    }

    void hl2_dB_to_int_round_trip() {
        // Per-band stored value is int 0..99 on HL2.
        // Conversion at UI boundary: dB-to-int = (33 + dB*2) * 3
        // Inverse:                   int-to-dB = (value/3 - 33) / 2

        // dB = 0     -> int = (33 + 0) * 3 = 99
        // dB = -16.5 -> int = (33 + -33) * 3 = 0
        // dB = -8.0  -> int = (33 + -16) * 3 = 51

        // Test the inverse: int = 99 -> dB = (99/3 - 33)/2 = (33-33)/2 = 0
        const double dB_99 = (99.0/3.0 - 33.0) / 2.0;
        QVERIFY(qFuzzyCompare(dB_99 + 1.0, 0.0 + 1.0));   // qFuzzyCompare-around-zero idiom

        const double dB_0 = (0.0/3.0 - 33.0) / 2.0;
        QVERIFY(qFuzzyCompare(dB_0, -16.5));

        const double dB_51 = (51.0/3.0 - 33.0) / 2.0;
        QVERIFY(qFuzzyCompare(dB_51, -8.0));
    }
};

QTEST_MAIN(TestTransmitModelHl2Persistence)
#include "tst_transmit_model_hl2_persistence.moc"
```

Note: `setHpsdrModel(HPSDRModel)` — verify TransmitModel has this setter. If not, the test stub uses an alternative way to inject the model (likely via constructor or member). Adapt as needed.

Register in `tests/CMakeLists.txt`.

- [ ] **Step 6.2: Run test, verify build/runtime failure**

```bash
cmake --build build --target tst_transmit_model_hl2_persistence 2>&1 | tail -10
ctest --test-dir build -R tst_transmit_model_hl2_persistence --output-on-failure
```

Expected: HL2 case fails clamp ceiling (returns 100, expected 99).

- [ ] **Step 6.3: Implement polymorphic clamp + cite cleanup**

In `src/models/TransmitModel.cpp`, find `setTunePowerForBand` (around line 475). Replace with:

```cpp
// NereusSDR-original: per-band tune-power memory.
// Thetis (both ramdor and mi0bot) stores a single global tune_power; we
// extend it to per-band so the operator does not have to readjust on
// band change. Mirrors NereusSDR's existing per-band power_by_band[]
// pattern.
//
// Clamp range polymorphs on the connected radio model:
//   HERMESLITE: [0, 99] (mi0bot Tune slider scale, 33 sub-steps)
//   others:     [0, 100] (canonical Thetis 0-100 watts target)
void TransmitModel::setTunePowerForBand(Band band, int value)
{
    const int idx = static_cast<int>(band);
    if (idx < 0 || idx >= kBandCount) {
        return;
    }
    const int hi = (m_hpsdrModel == HPSDRModel::HERMESLITE) ? 99 : 100;
    const int clamped = std::clamp(value, 0, hi);
    if (m_tunePowerByBand[static_cast<std::size_t>(idx)] == clamped) {
        return;
    }
    m_tunePowerByBand[static_cast<std::size_t>(idx)] = clamped;

    // ... existing emit + persist logic ...
}
```

Verify there is an `m_hpsdrModel` field in TransmitModel. If not, add one with a setter, and ensure `RadioModel` updates it via `currentRadioChanged`. (Typical pattern: TransmitModel learns the model on `setHpsdrModel(HPSDRModel)`.)

- [ ] **Step 6.4: Run test, verify pass**

```bash
cmake --build build --target tst_transmit_model_hl2_persistence && \
  ctest --test-dir build -R tst_transmit_model_hl2_persistence --output-on-failure
```

Expected: 4/4 pass.

- [ ] **Step 6.5: Commit**

```bash
git add src/models/TransmitModel.h src/models/TransmitModel.cpp \
        tests/tst_transmit_model_hl2_persistence.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
fix(transmit-model): polymorphic per-band tune-power clamp + cite cleanup

setTunePowerForBand clamp ceiling now polymorphs by SKU:
  HERMESLITE: [0, 99]   (mi0bot Tune slider scale, 33 sub-steps)
  others:     [0, 100]  (canonical Thetis 0-100 target)

Same AppSettings key tunePower_by_band/<band>; semantics flip per-SKU.
Matches mi0bot pattern - same key, polymorphic interpretation, no
migration step. HL2 user upgrading from prior NereusSDR sees existing
stored values reinterpreted as int 0-99 (mi0bot encoding) -> dB at UI
boundary; documented in changelog.

Cite cleanup: previous comment cited Thetis console.cs:12094 for
tunePower_by_band[] but that array does not exist in either ramdor or
mi0bot. The per-band tune-power array is fully NereusSDR-original.
Replaced with a NereusSDR-original tag.

Test: tst_transmit_model_hl2_persistence.cpp (4 cases) covers HL2
clamp to 99, non-HL2 clamp to 100, negative-clamps-to-zero, and the
dB-to-int conversion formula round-trip.

Spec: §6; Plan: Task 6; Issue: #175

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: TxApplet rescalePowerSlidersForModel + updatePowerSliderLabels + tooltip

**Files:**
- Test: `tests/tst_tx_applet_hl2_slider.cpp` (new)
- Modify: `src/gui/applets/TxApplet.h`, `src/gui/applets/TxApplet.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 7.1: Write the failing test**

Create `tests/tst_tx_applet_hl2_slider.cpp`:

```cpp
// TxApplet HL2 slider rescale + label formula + tooltip.
// Spec §5.2; Plan Task 7.

#include <QtTest/QtTest>
#include <QApplication>
#include "TxApplet.h"
#include "RadioModel.h"
#include "HpsdrModel.h"

class TestTxAppletHl2Slider : public QObject {
    Q_OBJECT

    QApplication* m_app{nullptr};
private slots:
    void initTestCase() {
        int argc = 1;
        static char arg0[] = "tst";
        static char* argv[] = {arg0};
        if (!QApplication::instance()) {
            m_app = new QApplication(argc, argv);
        }
    }

    void hl2_rfSlider_rescales() {
        NereusSDR::RadioModel rm;
        NereusSDR::TxApplet ta(&rm);
        ta.rescalePowerSlidersForModel(NereusSDR::HPSDRModel::HERMESLITE);
        QCOMPARE(ta.rfPowerSlider()->maximum(),  90);
        QCOMPARE(ta.rfPowerSlider()->singleStep(), 6);
        QCOMPARE(ta.rfPowerSlider()->pageStep(),   6);
    }

    void hl2_tuneSlider_rescales() {
        NereusSDR::RadioModel rm;
        NereusSDR::TxApplet ta(&rm);
        ta.rescalePowerSlidersForModel(NereusSDR::HPSDRModel::HERMESLITE);
        QCOMPARE(ta.tunePowerSlider()->maximum(),  99);
        QCOMPARE(ta.tunePowerSlider()->singleStep(), 3);
    }

    void anan100_returnsToDefault() {
        NereusSDR::RadioModel rm;
        NereusSDR::TxApplet ta(&rm);
        ta.rescalePowerSlidersForModel(NereusSDR::HPSDRModel::HERMESLITE);
        ta.rescalePowerSlidersForModel(NereusSDR::HPSDRModel::ANAN_100);
        QCOMPARE(ta.rfPowerSlider()->maximum(),  100);
        QCOMPARE(ta.rfPowerSlider()->singleStep(), 1);
        QCOMPARE(ta.tunePowerSlider()->maximum(), 100);
    }

    void hl2_label_formula() {
        // (round(60/6.0)/2) - 7.5 = (10/2) - 7.5 = 5 - 7.5 = -2.5
        NereusSDR::RadioModel rm;
        NereusSDR::TxApplet ta(&rm);
        ta.rescalePowerSlidersForModel(NereusSDR::HPSDRModel::HERMESLITE);
        ta.rfPowerSlider()->setValue(60);
        ta.updatePowerSliderLabels();
        QCOMPARE(ta.rfPowerLabel()->text(), QStringLiteral("-2.5"));
    }

    void hl2_label_at_max() {
        NereusSDR::RadioModel rm;
        NereusSDR::TxApplet ta(&rm);
        ta.rescalePowerSlidersForModel(NereusSDR::HPSDRModel::HERMESLITE);
        ta.rfPowerSlider()->setValue(90);
        ta.updatePowerSliderLabels();
        QCOMPARE(ta.rfPowerLabel()->text(), QStringLiteral("0.0"));
    }

    void nonHl2_label_is_int() {
        NereusSDR::RadioModel rm;
        NereusSDR::TxApplet ta(&rm);
        ta.rescalePowerSlidersForModel(NereusSDR::HPSDRModel::ANAN_100);
        ta.rfPowerSlider()->setValue(75);
        ta.updatePowerSliderLabels();
        QCOMPARE(ta.rfPowerLabel()->text(), QStringLiteral("75"));
    }

    void tooltip_thetis_faithful() {
        NereusSDR::RadioModel rm;
        NereusSDR::TxApplet ta(&rm);
        ta.rescalePowerSlidersForModel(NereusSDR::HPSDRModel::HERMESLITE);
        QVERIFY(ta.rfPowerSlider()->toolTip().contains("relative value"));
        QVERIFY(ta.rfPowerSlider()->toolTip().contains("MAX watts"));
    }
};

QTEST_MAIN(TestTxAppletHl2Slider)
#include "tst_tx_applet_hl2_slider.moc"
```

Note: this test exposes accessors `rfPowerSlider()`, `tunePowerSlider()`, `rfPowerLabel()` — these may need to be added as test-only accessors in TxApplet.h (`#ifdef NEREUSSDR_TESTING` guard or just public accessors per existing pattern). Check existing tests for the project convention.

Register in `tests/CMakeLists.txt`.

- [ ] **Step 7.2: Run test, verify failure**

```bash
cmake --build build --target tst_tx_applet_hl2_slider 2>&1 | tail -10
```

Expected: build error — `rescalePowerSlidersForModel` not declared, or runtime fail on label format.

- [ ] **Step 7.3: Implement helpers**

In `src/gui/applets/TxApplet.h` (private section), declare:

```cpp
public:
    // Test accessors (existing pattern - check project convention).
    QSlider* rfPowerSlider()    const noexcept { return m_rfPowerSlider; }
    QSlider* tunePowerSlider()  const noexcept { return m_tunePwrSlider; }
    QLabel*  rfPowerLabel()     const noexcept { return m_rfPowerValue; }
    QLabel*  tunePowerLabel()   const noexcept { return m_tunePwrValue; }

    // mi0bot-Thetis HL2 parity helpers.
    void rescalePowerSlidersForModel(HPSDRModel m);
    void updatePowerSliderLabels();
```

In `src/gui/applets/TxApplet.cpp`, add the helpers (place near `rescaleFwdGaugeForModel` around line 1315):

```cpp
// From mi0bot-Thetis console.cs:2098-2108 [v2.10.3.13-beta2]
// Slider Maximum/Step adjusts per radio SKU. HL2 gets 16-step output
// attenuator (Maximum=90, step=6) on RF Power, 33-step Tune (Maximum=99,
// step=3). Other SKUs keep canonical 0-100 step 1.
void TxApplet::rescalePowerSlidersForModel(HPSDRModel model)
{
    const QSignalBlocker rfBlock(m_rfPowerSlider);
    const QSignalBlocker tunBlock(m_tunePwrSlider);

    m_rfPowerSlider->setRange(0, rfPowerSliderMaxFor(model));
    m_rfPowerSlider->setSingleStep(rfPowerSliderStepFor(model));
    m_rfPowerSlider->setPageStep(rfPowerSliderStepFor(model));

    m_tunePwrSlider->setRange(0, tuneSliderMaxFor(model));
    m_tunePwrSlider->setSingleStep(tuneSliderStepFor(model));
    m_tunePwrSlider->setPageStep(tuneSliderStepFor(model));

    // Tooltip - Thetis-faithful on every SKU.
    // From Thetis console.resx [v2.10.3.15] (ramdor) and
    // mi0bot-Thetis console.resx [v2.10.3.13-beta2].
    m_rfPowerSlider->setToolTip(QStringLiteral(
        "Transmit Drive — relative value, not watts unless the PA profile "
        "is configured with MAX watts @ 100%"));
    m_tunePwrSlider->setToolTip(QStringLiteral(
        "Tune and/or 2Tone Drive — relative value, not watts unless the "
        "PA profile is configured with MAX watts @ 100%"));

    updatePowerSliderLabels();
}

// From mi0bot-Thetis console.cs:29245-29274 [v2.10.3.13-beta2]
// HL2 RF Power label formula:
//   lblPWR.Text = "Drive: " + ((round(drv/6.0)/2) - 7.5) + "dB"
// HL2 Tune slider label formula (slider 0..99 -> dB -16.5..0):
//   dB = (slider/3 - 33) / 2  (inverse of the persistence formula)
void TxApplet::updatePowerSliderLabels()
{
    const auto model = m_model->hardwareProfile().model;
    const int  rfVal  = m_rfPowerSlider->value();
    const int  tunVal = m_tunePwrSlider->value();

    if (model == HPSDRModel::HERMESLITE) {
        const float rfDb  = (std::round(rfVal  / 6.0f) / 2.0f) - 7.5f;
        const float tunDb = (tunVal / 3.0f - 33.0f) / 2.0f;
        m_rfPowerValue ->setText(QString::number(rfDb,  'f', 1));
        m_tunePwrValue ->setText(QString::number(tunDb, 'f', 1));
    } else {
        m_rfPowerValue ->setText(QString::number(rfVal));
        m_tunePwrValue ->setText(QString::number(tunVal));
    }
}
```

Hook from the existing `wireControls()` `currentRadioChanged` lambda. Find the existing call to `rescaleFwdGaugeForModel(...)` (around line 785-792) and add `rescalePowerSlidersForModel(...)` right after it. Also add `valueChanged` connections for both sliders pointing at `updatePowerSliderLabels`:

```cpp
connect(m_rfPowerSlider, &QSlider::valueChanged, this, [this](int) {
    updatePowerSliderLabels();
});
connect(m_tunePwrSlider, &QSlider::valueChanged, this, [this](int) {
    updatePowerSliderLabels();
});
```

Remove the obsolete `rfSlider->setToolTip(QStringLiteral("RF output power (0-100 W)"));` at line 286 (replaced by the helper above) and the equivalent tune slider tooltip.

- [ ] **Step 7.4: Run test, verify pass**

```bash
cmake --build build --target tst_tx_applet_hl2_slider && \
  ctest --test-dir build -R tst_tx_applet_hl2_slider --output-on-failure
```

Expected: 7/7 pass.

- [ ] **Step 7.5: Commit**

```bash
git add src/gui/applets/TxApplet.h src/gui/applets/TxApplet.cpp \
        tests/tst_tx_applet_hl2_slider.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(tx-applet): HL2 slider rescale + dB label + Thetis tooltips

New helpers rescalePowerSlidersForModel and updatePowerSliderLabels
hooked from RadioModel::currentRadioChanged. On HL2:

  - RF Power slider: Max=90, step=6
  - Tune Power slider: Max=99, step=3
  - RF Power label: -X.X dB via mi0bot formula
                    (round(drv/6.0)/2) - 7.5
  - Tune Power label: -X.X dB via inverse persistence formula
                      (slider/3 - 33) / 2

On non-HL2 SKUs sliders return to canonical 0-100 step 1, labels
display the bare integer value.

Tooltips on both sliders rewritten across all SKUs to Thetis-faithful
wording: "Transmit Drive - relative value, not watts unless the PA
profile is configured with MAX watts @ 100%". Replaces the previous
"RF output power (0-100 W)" wording which contradicts upstream
semantics on every SKU.

Cites mi0bot-Thetis console.cs:2098-2108 (slider Max/step) +
console.cs:29245-29274 (label formula) + console.resx (tooltip text)
[v2.10.3.13-beta2].

Test: tst_tx_applet_hl2_slider.cpp (7 cases) covers HL2 rescale,
SKU-swap-back, label formula at multiple slider positions, non-HL2
integer labels, and tooltip wording.

Spec: §5.2; Plan: Task 7; Issue: #175

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: TransmitSetupPages PowerPage `grpPATune` group port

**Source-first prep:**

```bash
git -C ../../../../mi0bot-Thetis show v2.10.3.13-beta2:"Project Files/Source/Console/setup.designer.cs" | sed -n '47874,47900p'
git -C ../../../../mi0bot-Thetis show v2.10.3.13-beta2:"Project Files/Source/Console/setup.cs" | sed -n '20320,20335p'
```

**Files:**
- Test: `tests/tst_transmit_setup_power_page_hl2.cpp` (new)
- Modify: `src/gui/setup/TransmitSetupPages.h` (declare `buildTuneGroup`, members for new controls)
- Modify: `src/gui/setup/TransmitSetupPages.cpp` (implement `buildTuneGroup`, hook radio-change)
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 8.1: Write the failing test**

Create `tests/tst_transmit_setup_power_page_hl2.cpp`:

```cpp
// TransmitSetupPages PowerPage grpPATune group port.
// Spec §5.3; Plan Task 8.

#include <QtTest/QtTest>
#include <QApplication>
#include "TransmitSetupPages.h"
#include "RadioModel.h"
#include "HpsdrModel.h"

class TestTransmitSetupPowerPageHl2 : public QObject {
    Q_OBJECT
    QApplication* m_app{nullptr};
private slots:
    void initTestCase() {
        int argc = 1;
        static char arg0[] = "tst";
        static char* argv[] = {arg0};
        if (!QApplication::instance()) {
            m_app = new QApplication(argc, argv);
        }
    }

    void tuneGroup_exists() {
        NereusSDR::RadioModel rm;
        NereusSDR::PowerPage page(&rm);
        QGroupBox* g = page.findChild<QGroupBox*>(QStringLiteral("grpPATune"));
        QVERIFY(g != nullptr);
        QCOMPARE(g->title(), QStringLiteral("Tune"));
    }

    void tuneGroup_hasThreeRadios() {
        NereusSDR::RadioModel rm;
        NereusSDR::PowerPage page(&rm);
        QVERIFY(page.findChild<QRadioButton*>(QStringLiteral("radUseFixedDriveTune")));
        QVERIFY(page.findChild<QRadioButton*>(QStringLiteral("radUseDriveSliderTune")));
        QVERIFY(page.findChild<QRadioButton*>(QStringLiteral("radUseTuneSliderTune")));
    }

    void tuneGroup_hasMeterCombo() {
        NereusSDR::RadioModel rm;
        NereusSDR::PowerPage page(&rm);
        QVERIFY(page.findChild<QComboBox*>(QStringLiteral("comboTXTUNMeter")));
    }

    void tuneGroup_fixedSpinbox_hl2() {
        NereusSDR::RadioModel rm;
        NereusSDR::PowerPage page(&rm);
        page.applyHpsdrModel(NereusSDR::HPSDRModel::HERMESLITE);
        auto* sb = page.findChild<QDoubleSpinBox*>(QStringLiteral("udTXTunePower"));
        QVERIFY(sb != nullptr);
        QCOMPARE(sb->minimum(), -16.5);
        QCOMPARE(sb->maximum(),   0.0);
        QCOMPARE(sb->singleStep(), 0.5);
        QCOMPARE(sb->decimals(),    1);
        QCOMPARE(sb->suffix(), QStringLiteral(" dB"));
    }

    void tuneGroup_fixedSpinbox_anan100() {
        NereusSDR::RadioModel rm;
        NereusSDR::PowerPage page(&rm);
        page.applyHpsdrModel(NereusSDR::HPSDRModel::ANAN_100);
        auto* sb = page.findChild<QDoubleSpinBox*>(QStringLiteral("udTXTunePower"));
        QCOMPARE(sb->minimum(),    0.0);
        QCOMPARE(sb->maximum(),  100.0);
        QCOMPARE(sb->singleStep(), 1.0);
        QCOMPARE(sb->decimals(),     0);
        QCOMPARE(sb->suffix(), QStringLiteral(" W"));
    }

    void tuneGroup_radioToggle_persists() {
        NereusSDR::RadioModel rm;
        NereusSDR::PowerPage page(&rm);
        auto* radFixed = page.findChild<QRadioButton*>(QStringLiteral("radUseFixedDriveTune"));
        radFixed->setChecked(true);
        QCOMPARE(rm.transmitModel().tuneDrivePowerSource(),
                 NereusSDR::DrivePowerSource::Fixed);
    }

    void tuneGroup_fixedSpinbox_disabledWhenNotFixed() {
        NereusSDR::RadioModel rm;
        NereusSDR::PowerPage page(&rm);
        auto* radTune  = page.findChild<QRadioButton*>(QStringLiteral("radUseTuneSliderTune"));
        radTune->setChecked(true);
        auto* sb = page.findChild<QDoubleSpinBox*>(QStringLiteral("udTXTunePower"));
        QVERIFY(!sb->isEnabled());
    }
};

QTEST_MAIN(TestTransmitSetupPowerPageHl2)
#include "tst_transmit_setup_power_page_hl2.moc"
```

Register in `tests/CMakeLists.txt`.

- [ ] **Step 8.2: Run test, verify failure**

```bash
cmake --build build --target tst_transmit_setup_power_page_hl2 2>&1 | tail -10
```

Expected: build error or runtime test failure (group box doesn't exist yet).

- [ ] **Step 8.3: Implement `buildTuneGroup`**

In `src/gui/setup/TransmitSetupPages.h`, add declarations:

```cpp
public:
    void applyHpsdrModel(HPSDRModel m);

private:
    void buildTuneGroup();
    void onTuneDriveSourceChanged(DrivePowerSource src);

    QGroupBox*       m_grpPATune{nullptr};
    QRadioButton*    m_radFixedDrive{nullptr};
    QRadioButton*    m_radDriveSlider{nullptr};
    QRadioButton*    m_radTuneSlider{nullptr};
    QComboBox*       m_comboTxTunMeter{nullptr};
    QDoubleSpinBox*  m_fixedTunePwrSpin{nullptr};
```

In `src/gui/setup/TransmitSetupPages.cpp`, add the `buildTuneGroup` method. Place it ABOVE `buildTunePowerGroup` so the layout order is "Tune group, then per-band grid":

```cpp
// From mi0bot-Thetis setup.designer.cs:47874-47930 [v2.10.3.13-beta2]
// "Tune" group box on the Setup -> Transmit page. Mirrors mi0bot's
// grpPATune layout: 3 drive-source radios + TX TUN Meter combo + Fixed-
// mode tune-power spinbox. The spinbox is only enabled when the Fixed
// radio is selected.
//
// On HL2 the spinbox bounds flip to mi0bot's dB-attenuation range
// (-16.5..0 dB, 0.5 step) per setup.cs:20328-20331 [v2.10.3.13-beta2].
void PowerPage::buildTuneGroup()
{
    m_grpPATune = new QGroupBox(QStringLiteral("Tune"), this);
    m_grpPATune->setObjectName(QStringLiteral("grpPATune"));

    auto* form = new QFormLayout(m_grpPATune);
    form->setSpacing(4);

    // Drive source radio buttons
    auto* btnGroup = new QButtonGroup(m_grpPATune);
    m_radFixedDrive  = new QRadioButton(QStringLiteral("Fixed"),       m_grpPATune);
    m_radFixedDrive ->setObjectName(QStringLiteral("radUseFixedDriveTune"));
    m_radDriveSlider = new QRadioButton(QStringLiteral("Drive Slider"), m_grpPATune);
    m_radDriveSlider->setObjectName(QStringLiteral("radUseDriveSliderTune"));
    m_radTuneSlider  = new QRadioButton(QStringLiteral("Tune Slider"),  m_grpPATune);
    m_radTuneSlider ->setObjectName(QStringLiteral("radUseTuneSliderTune"));
    btnGroup->addButton(m_radFixedDrive,  static_cast<int>(DrivePowerSource::Fixed));
    btnGroup->addButton(m_radDriveSlider, static_cast<int>(DrivePowerSource::DriveSlider));
    btnGroup->addButton(m_radTuneSlider,  static_cast<int>(DrivePowerSource::TuneSlider));
    m_radTuneSlider->setChecked(true);  // default per mi0bot

    auto* radioBox = new QVBoxLayout;
    radioBox->setSpacing(2);
    radioBox->addWidget(m_radFixedDrive);
    radioBox->addWidget(m_radTuneSlider);
    radioBox->addWidget(m_radDriveSlider);
    form->addRow(QStringLiteral("Drive Source:"), radioBox);

    // TX TUN Meter combo
    m_comboTxTunMeter = new QComboBox(m_grpPATune);
    m_comboTxTunMeter->setObjectName(QStringLiteral("comboTXTUNMeter"));
    m_comboTxTunMeter->addItem(QStringLiteral("Forward Power"));
    m_comboTxTunMeter->addItem(QStringLiteral("Reverse Power"));
    m_comboTxTunMeter->addItem(QStringLiteral("SWR"));
    m_comboTxTunMeter->addItem(QStringLiteral("Mic"));
    m_comboTxTunMeter->addItem(QStringLiteral("ALC"));
    form->addRow(QStringLiteral("TX TUN Meter:"), m_comboTxTunMeter);

    // Fixed-mode tune-power spinbox - bounds set in applyHpsdrModel.
    m_fixedTunePwrSpin = new QDoubleSpinBox(m_grpPATune);
    m_fixedTunePwrSpin->setObjectName(QStringLiteral("udTXTunePower"));
    form->addRow(QStringLiteral("Fixed Tune Power:"), m_fixedTunePwrSpin);

    // Wire model <-> radios
    if (model()) {
        TransmitModel& tx = model()->transmitModel();
        connect(btnGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked),
                this, [this, &tx, btnGroup](QAbstractButton* btn) {
            const int id = btnGroup->id(btn);
            tx.setTuneDrivePowerSource(static_cast<DrivePowerSource>(id));
            onTuneDriveSourceChanged(static_cast<DrivePowerSource>(id));
        });
        // Initial state from model
        onTuneDriveSourceChanged(tx.tuneDrivePowerSource());
    }

    contentLayout()->addWidget(m_grpPATune);
}

void PowerPage::onTuneDriveSourceChanged(DrivePowerSource src)
{
    m_fixedTunePwrSpin->setEnabled(src == DrivePowerSource::Fixed);
}

// applyHpsdrModel is also called for the per-band grid in Task 9.
// In Task 8 we only handle the Fixed-mode spinbox bounds.
void PowerPage::applyHpsdrModel(HPSDRModel m)
{
    if (!m_fixedTunePwrSpin) {
        return;
    }
    m_fixedTunePwrSpin->setRange(fixedTuneSpinboxMinFor(m),
                                 fixedTuneSpinboxMaxFor(m));
    m_fixedTunePwrSpin->setSingleStep(fixedTuneSpinboxStepFor(m));
    m_fixedTunePwrSpin->setDecimals(fixedTuneSpinboxDecimalsFor(m));
    m_fixedTunePwrSpin->setSuffix(QString::fromLatin1(fixedTuneSpinboxSuffixFor(m)));
}
```

In the `PowerPage` constructor, add `buildTuneGroup()` BEFORE `buildTunePowerGroup()`. Hook to `RadioModel::currentRadioChanged` in the constructor (or wherever existing currentRadioChanged hooks live):

```cpp
if (model()) {
    connect(model(), &RadioModel::currentRadioChanged, this,
            [this](const NereusSDR::RadioInfo&) {
        applyHpsdrModel(model()->hardwareProfile().model);
    });
    applyHpsdrModel(model()->hardwareProfile().model);
}
```

- [ ] **Step 8.4: Run test, verify pass**

```bash
cmake --build build --target tst_transmit_setup_power_page_hl2 && \
  ctest --test-dir build -R tst_transmit_setup_power_page_hl2 --output-on-failure
```

Expected: 7/7 pass.

- [ ] **Step 8.5: Verify attribution**

```bash
python3 scripts/verify-inline-cites.py 2>&1 | tail -3
```

- [ ] **Step 8.6: Commit**

```bash
git add src/gui/setup/TransmitSetupPages.h src/gui/setup/TransmitSetupPages.cpp \
        tests/tst_transmit_setup_power_page_hl2.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(setup-pages): port mi0bot grpPATune Tune group to PowerPage

Setup -> Transmit -> Power gains a new "Tune" group box mirroring
mi0bot-Thetis's grpPATune layout exactly:

  - 3 drive-source radio buttons (Fixed / Tune Slider / Drive Slider)
  - TX TUN Meter combo (Forward Power / Reverse Power / SWR / Mic / ALC)
  - Fixed-mode tune-power spinbox

The Fixed spinbox bounds polymorph by SKU per mi0bot:
  HL2:        -16.5..0 dB, 0.5 dB step, 1 decimal,  " dB" suffix
  others:     0..100 W,    1 W step,    0 decimals, " W" suffix

Spinbox is enabled only when "Fixed" radio is selected. Default radio
selection is "Tune Slider" (matches mi0bot).

Reconfigures on RadioModel::currentRadioChanged so SKU swaps reapply
the right bounds without restart.

Cites mi0bot-Thetis setup.designer.cs:47874-47930 (group layout) +
setup.cs:20328-20331 (HL2 spinbox bounds) [v2.10.3.13-beta2].

Test: tst_transmit_setup_power_page_hl2.cpp (7 cases) covers group
existence, radio buttons, meter combo, spinbox bounds on HL2 vs
ANAN-100, radio toggle persisting to model, and Fixed-disabled
behavior.

Spec: §5.3; Plan: Task 8; Issue: #175

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: Per-band Tune Power grid SKU-aware (QSpinBox → QDoubleSpinBox)

**Files:**
- Modify: `src/gui/setup/TransmitSetupPages.h` (`m_tunePwrSpins[14]` flips to `QDoubleSpinBox*`)
- Modify: `src/gui/setup/TransmitSetupPages.cpp` (`buildTunePowerGroup` widget conversion + apply in `applyHpsdrModel`)
- Modify: `tests/tst_transmit_setup_power_pa_page.cpp` (existing test; update widget cast + add HL2 cases)

- [ ] **Step 9.1: Update existing test for widget type change + add HL2 cases**

Find the existing `tst_transmit_setup_power_pa_page.cpp` cases that do `qobject_cast<QSpinBox*>` on the per-band spinboxes (around lines 139-171). Flip to `QDoubleSpinBox*`. Add new cases for HL2:

```cpp
void perBandSpinboxes_hl2_dB() {
    NereusSDR::RadioModel rm;
    NereusSDR::PowerPage page(&rm);
    page.applyHpsdrModel(NereusSDR::HPSDRModel::HERMESLITE);
    for (int i = 0; i < kBandCount; ++i) {
        const auto label = NereusSDR::bandLabel(static_cast<NereusSDR::Band>(i));
        const auto name  = QStringLiteral("spinTunePwr_") + label;
        auto* sb = page.findChild<QDoubleSpinBox*>(name);
        QVERIFY2(sb != nullptr, qPrintable(QStringLiteral("missing %1").arg(name)));
        QCOMPARE(sb->minimum(),   -16.5);
        QCOMPARE(sb->maximum(),     0.0);
        QCOMPARE(sb->singleStep(),  0.5);
        QCOMPARE(sb->decimals(),      1);
        QCOMPARE(sb->suffix(),     QStringLiteral(" dB"));
    }
}

void perBandSpinboxes_anan100_W() {
    NereusSDR::RadioModel rm;
    NereusSDR::PowerPage page(&rm);
    page.applyHpsdrModel(NereusSDR::HPSDRModel::ANAN_100);
    auto* sb = page.findChild<QDoubleSpinBox*>(QStringLiteral("spinTunePwr_40m"));
    QCOMPARE(sb->minimum(),   0.0);
    QCOMPARE(sb->maximum(), 100.0);
    QCOMPARE(sb->decimals(),    0);
    QCOMPARE(sb->suffix(), QStringLiteral(" W"));
}
```

- [ ] **Step 9.2: Run test, verify failure**

```bash
cmake --build build --target tst_transmit_setup_power_pa_page 2>&1 | tail -10
```

Expected: build fail (`m_tunePwrSpins` is `QSpinBox*` not `QDoubleSpinBox*`).

- [ ] **Step 9.3: Convert widget type + apply per-SKU in `applyHpsdrModel`**

In `src/gui/setup/TransmitSetupPages.h`:

```cpp
private:
    std::array<QDoubleSpinBox*, kBandCount> m_tunePwrSpins{};   // was QSpinBox*
```

In `src/gui/setup/TransmitSetupPages.cpp`, in `buildTunePowerGroup` (around line 280), flip the spinbox construction:

```cpp
auto* spin = new QDoubleSpinBox(group);
spin->setObjectName(QStringLiteral("spinTunePwr_") + label);
m_tunePwrSpins[i] = spin;
// Initial range/decimals/suffix set by applyHpsdrModel.
```

Remove the hardcoded `setRange(0, 100)`, `setSuffix(" W")`, and `setToolTip("0–100 W")` calls — these all move into `applyHpsdrModel`.

Extend `applyHpsdrModel` (added in Task 8) to also reconfigure the per-band grid:

```cpp
void PowerPage::applyHpsdrModel(HPSDRModel m)
{
    // Fixed-mode spinbox (Task 8).
    if (m_fixedTunePwrSpin) {
        m_fixedTunePwrSpin->setRange(fixedTuneSpinboxMinFor(m),
                                     fixedTuneSpinboxMaxFor(m));
        m_fixedTunePwrSpin->setSingleStep(fixedTuneSpinboxStepFor(m));
        m_fixedTunePwrSpin->setDecimals(fixedTuneSpinboxDecimalsFor(m));
        m_fixedTunePwrSpin->setSuffix(QString::fromLatin1(fixedTuneSpinboxSuffixFor(m)));
    }

    // Per-band grid (Task 9). Same per-SKU rules; storage is int 0-99
    // (HL2) / 0-100 (others). UI displays dB on HL2, W elsewhere.
    for (auto* spin : m_tunePwrSpins) {
        if (!spin) continue;
        spin->setRange(fixedTuneSpinboxMinFor(m),
                       fixedTuneSpinboxMaxFor(m));
        spin->setSingleStep(fixedTuneSpinboxStepFor(m));
        spin->setDecimals(fixedTuneSpinboxDecimalsFor(m));
        spin->setSuffix(QString::fromLatin1(fixedTuneSpinboxSuffixFor(m)));
    }
}
```

Update the existing per-band binding logic in `buildTunePowerGroup`. Currently:

```cpp
spin->setValue(tx.tunePowerForBand(band));
connect(spin, &QSpinBox::valueChanged, this, [this, band](int val) {
    model()->transmitModel().setTunePowerForBand(band, val);
});
connect(&tx, &TransmitModel::tunePowerByBandChanged,
        spin, [band, spin](Band changedBand, int watts) {
    if (changedBand != band) return;
    QSignalBlocker b(spin);
    spin->setValue(watts);
});
```

Replace with SKU-aware conversion. The model stores int 0-99 (HL2) / 0-100 (others); the UI displays dB on HL2, W elsewhere:

```cpp
auto displayFromStored = [this](int stored) -> double {
    if (model() && model()->hardwareProfile().model == HPSDRModel::HERMESLITE) {
        return (stored / 3.0 - 33.0) / 2.0;   // int 0..99 -> dB -16.5..0
    }
    return static_cast<double>(stored);       // int 0..100 -> W
};

auto storedFromDisplay = [this](double display) -> int {
    if (model() && model()->hardwareProfile().model == HPSDRModel::HERMESLITE) {
        return static_cast<int>(std::round((33 + display * 2) * 3));  // dB -> int
    }
    return static_cast<int>(std::round(display));
};

{
    QSignalBlocker b(spin);
    spin->setValue(displayFromStored(tx.tunePowerForBand(band)));
}
connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
        this, [this, band, storedFromDisplay](double display) {
    model()->transmitModel().setTunePowerForBand(band, storedFromDisplay(display));
});
connect(&tx, &TransmitModel::tunePowerByBandChanged, spin,
        [this, band, spin, displayFromStored](Band changedBand, int storedValue) {
    if (changedBand != band) return;
    QSignalBlocker b(spin);
    spin->setValue(displayFromStored(storedValue));
});
```

Also add a refresh-all-bands call in `applyHpsdrModel` so the displayed values update when the SKU changes:

```cpp
// At end of applyHpsdrModel, after setting bounds, refresh displayed values
// so HL2 -> ANAN-100 swap or vice versa repaints the spinboxes.
if (model()) {
    TransmitModel& tx = model()->transmitModel();
    for (int i = 0; i < kBandCount; ++i) {
        auto* spin = m_tunePwrSpins[i];
        if (!spin) continue;
        QSignalBlocker b(spin);
        spin->setValue(displayFromStored(tx.tunePowerForBand(static_cast<Band>(i))));
    }
}
```

(Hoist `displayFromStored` to a private member function to share between `buildTunePowerGroup` and `applyHpsdrModel`.)

- [ ] **Step 9.4: Run test, verify pass**

```bash
cmake --build build --target tst_transmit_setup_power_pa_page && \
  ctest --test-dir build -R tst_transmit_setup_power_pa_page --output-on-failure
```

Expected: all existing cases plus new HL2 cases pass.

- [ ] **Step 9.5: Commit**

```bash
git add src/gui/setup/TransmitSetupPages.h src/gui/setup/TransmitSetupPages.cpp \
        tests/tst_transmit_setup_power_pa_page.cpp
git commit -m "$(cat <<'EOF'
feat(setup-pages): per-band Tune Power grid SKU-aware (HL2 dB / others W)

m_tunePwrSpins[14] flips from QSpinBox* to QDoubleSpinBox* so
decimals can vary per SKU. On HL2:
  - range -16.5..0 dB, 0.5 step, 1 decimal, " dB" suffix
  - displayed value = (stored / 3.0 - 33.0) / 2.0
  - on user edit, stored = round((33 + dB*2) * 3)

Storage stays as int under the existing tunePower_by_band/<band>
AppSettings key - same key, polymorphic semantic by SKU (mi0bot
pattern, no migration step).

On non-HL2 SKUs, behavior is functionally identical to today
(decimals=0, integer steps, " W" suffix, value == stored).

Reconfigures on RadioModel::currentRadioChanged via applyHpsdrModel
(extended from Task 8). All 14 spinboxes refresh displayed values on
SKU swap.

NereusSDR-original feature: Thetis (both ramdor and mi0bot) only has
a single global udTXTunePower. The 14 per-band spinboxes are NereusSDR-
extended UX. Tagged as such inline.

Test: tst_transmit_setup_power_pa_page.cpp gains 2 new cases for HL2
dB display + ANAN-100 W display, plus widget cast updates (QSpinBox ->
QDoubleSpinBox) on existing cases.

Spec: §5.3 (per-band grid section); Plan: Task 9; Issue: #175

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: Wire `txPostGenToneMagChanged` from TransmitModel to TxChannel

**Files:**
- Modify: `src/models/RadioModel.cpp` (signal-slot wire-up alongside existing TX channel glue)

- [ ] **Step 10.1: Identify the existing model→channel wiring path**

Find where `TransmitModel` setter signals already wire to `TxChannel` setters. Pattern from Task 3M-3a-i (commit history shows 13 setter wire-ups). Typical location: `RadioModel.cpp` constructor or an `attachTxChannel` helper.

```bash
grep -n 'connect(.*&TransmitModel::.*Changed.*TxChannel' src/models/RadioModel.cpp
```

Find ~10-15 existing connect statements like:

```cpp
connect(&m_transmitModel, &TransmitModel::eq10ChangedX, this, [this](double v) {
    if (m_txChannel) m_txChannel->setEqGain(0, v);
});
```

- [ ] **Step 10.2: Add the new wire**

Insert:

```cpp
connect(&m_transmitModel, &TransmitModel::txPostGenToneMagChanged,
        this, [this](double mag) {
    if (m_txChannel) m_txChannel->setPostGenToneMag(mag);
});
```

- [ ] **Step 10.3: Verify with full ctest**

```bash
cmake --build build && ctest --test-dir build --output-on-failure 2>&1 | tail -20
```

Expected: all tests still green. There's no dedicated test for this wire-up — it's covered indirectly by Task 4's HL2 dbm test (which writes `m_txPostGenToneMag` and would propagate through this connect to a real TxChannel in production).

- [ ] **Step 10.4: Commit**

```bash
git add src/models/RadioModel.cpp
git commit -m "$(cat <<'EOF'
feat(radio-model): wire txPostGenToneMagChanged to TxChannel

Closes the model -> channel glue for the new HL2 sub-step DSP
modulation property added in Task 3.

When TransmitModel::setPowerUsingTargetDbm runs the HL2 sub-step path
(Task 4) it writes m_txPostGenToneMag, emits the signal, and this
connect routes the value into TxChannel::setPostGenToneMag (Task 2)
which calls WDSP SetTXAPostGenToneMag.

No new test - covered indirectly by Task 4 HL2 dbm test asserting the
property write side, plus full ctest for end-to-end build sanity.

Spec: §5.4.3, §5.5; Plan: Task 10; Issue: #175

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 11: Attribution sweep + PROVENANCE updates

**Files:**
- Modify: `docs/attribution/THETIS-PROVENANCE.md` (rows for any new attribution events)
- Run: all 5 attribution verifiers

- [ ] **Step 11.1: Run the pre-port checklist now in reverse — confirm every touched file's provenance is clean**

```bash
for f in src/core/HpsdrModel.h \
         src/core/TxChannel.h src/core/TxChannel.cpp \
         src/models/TransmitModel.h src/models/TransmitModel.cpp \
         src/models/RadioModel.cpp \
         src/gui/applets/TxApplet.h src/gui/applets/TxApplet.cpp \
         src/gui/setup/TransmitSetupPages.h src/gui/setup/TransmitSetupPages.cpp; do
    echo "=== $f ==="
    grep -l "$f" docs/attribution/THETIS-PROVENANCE.md || echo "  NEW PROVENANCE EVENT"
done
```

For each "NEW PROVENANCE EVENT" file, add a row to `docs/attribution/THETIS-PROVENANCE.md` per the format used by existing entries. Cite `mi0bot-Thetis [v2.10.3.13-beta2]` as the upstream source.

- [ ] **Step 11.2: Run all 5 attribution verifiers**

```bash
python3 scripts/verify-thetis-headers.py
python3 scripts/verify-inline-tag-preservation.py
python3 scripts/verify-inline-cites.py
python3 scripts/verify-provenance-sync.py
python3 scripts/check-new-ports.py
```

Expected: all clean. If any fail, fix the specific issue (missing `// MI0BOT:` tag, missing `[v...]` stamp on a cite, missing PROVENANCE row, etc.).

- [ ] **Step 11.3: Run the corpus drift refresh (if any new authors detected)**

```bash
python3 scripts/discover-thetis-author-tags.py
git diff docs/attribution/thetis-author-tags.json | head -20
```

If the corpus changed, commit the update.

- [ ] **Step 11.4: Commit**

```bash
git add docs/attribution/THETIS-PROVENANCE.md \
        docs/attribution/thetis-author-tags.json
git commit -m "$(cat <<'EOF'
chore(attribution): provenance + author corpus updates for HL2 mi0bot port

Adds PROVENANCE rows for files newly carrying mi0bot-Thetis attribution:
HpsdrModel.h, TxChannel.{h,cpp}, TransmitModel.{h,cpp},
TxApplet.{h,cpp}, TransmitSetupPages.{h,cpp}.

Refreshes scripts/discover-thetis-author-tags.py corpus to capture any
new MI0BOT-tagged commits introduced by this PR.

All 5 attribution verifiers (verify-thetis-headers, verify-inline-tag-
preservation, verify-inline-cites, verify-provenance-sync, check-new-
ports) clean.

Spec: §8; Plan: Task 11; Issue: #175

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 12: Final integration — full ctest, CHANGELOG, GPG verify, PR prep

- [ ] **Step 12.1: Run full ctest suite**

```bash
cmake --build build && ctest --test-dir build --output-on-failure 2>&1 | tail -30
```

Expected: all tests green. If anything fails that's not from this PR's new tests, fix — do not skip. Per memory `feedback_no_ci_unblock_shortcuts`: never stub a test return value to bypass CI.

- [ ] **Step 12.2: Verify all commits are GPG-signed**

```bash
git log --show-signature --format="%h %s" origin/main..HEAD 2>&1 | grep -E "^[a-f0-9]{7}|gpg:" | head -50
```

Expected: every commit shows `gpg: Good signature from "JJ Boyd <kg4vcf@gmail.com>"`. If any commit is unsigned, recommit (per memory `feedback_gpg_sign_commits`, never `--no-gpg-sign`).

- [ ] **Step 12.3: Update CHANGELOG.md**

Add to `CHANGELOG.md` under the next-release "Unreleased" section:

```markdown
### Fixed
- **HL2 TX UI parity with mi0bot-Thetis (#175)**: RF Power and Tune Power
  sliders, the Tune Power per-band grid, and the new Setup → Transmit
  "Tune" group all now respect HL2's dB-attenuator semantics (sliders
  0-90 / 0-99 with mi0bot's step sizes; spinboxes -16.5 to 0 dB in 0.5 dB
  increments). Underlying TX path also gains mi0bot's HL2 sub-step DSP
  modulation in `setPowerUsingTargetDbm` and the
  `(hl2Power * gbb / 100) / 93.75` audio-volume formula. HL2 users
  upgrading: existing per-band Tune Power values reinterpret as int 0-99
  (mi0bot encoding) - no migration required. Non-HL2 SKUs unchanged.
- Tooltip wording on RF Power and Tune Power sliders rewritten across
  every SKU to match Thetis upstream: "Transmit Drive — relative value,
  not watts unless the PA profile is configured with MAX watts @ 100%".
```

- [ ] **Step 12.4: Commit changelog + final verification**

```bash
git add CHANGELOG.md
git commit -m "$(cat <<'EOF'
docs(changelog): HL2 TX mi0bot parity entry for #175

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 12.5: Push branch, prepare PR description**

```bash
git push -u origin claude/peaceful-khayyam-779567 2>&1 | tail -5
```

Then prepare the PR description (do NOT use `gh pr create` until JJ green-lights). Skeleton:

```markdown
## Summary

Closes #175 (Chris W4ORS — HL2 RF Power and Tune Power scales show 0-100 W
instead of 0-5 W on HL2). PR #178 (v0.3.2 PA-cal hotfix) corrected the
meter side; this PR closes the input-control + DSP gap with full mi0bot-
Thetis HL2 parity.

After this PR, an HL2 user from mi0bot finds:
- RF Power slider 0-90 step 6 (16-step output attenuator, 0.5 dB/step)
- Tune Power slider 0-99 step 3 (33 sub-steps; 0-51 DSP audio gain
  modulation, 52-99 PA attenuator territory)
- Setup → Transmit → "Tune" group box (3 drive-source radios + meter
  combo + Fixed-mode spinbox) mirroring mi0bot's `grpPATune` exactly
- Fixed-mode and per-band Tune Power spinboxes -16.5..0 dB in 0.5 dB
  increments
- TX volume on the wire matching mi0bot's HL2 sub-step DSP modulation
  and `(hl2Power * gbb / 100) / 93.75` audio-volume formula

Non-HL2 SKUs: visual tooltip alignment with Thetis upstream wording, no
behavior change otherwise.

## Source

Per memory `feedback_mi0bot_authoritative_for_hl2`. Cites
mi0bot-Thetis `[v2.10.3.13-beta2]`. All `// MI0BOT:` attribution
comments preserved verbatim. PROVENANCE updated.

## Test plan

- [x] 7 new ctests + 1 extended ctest covering constants, WDSP wrapper,
      property+signal, HL2 sub-step DSP formula table, HL2 audio-volume
      formula table, polymorphic clamp, slider rescale, setup page
      layout, per-band grid SKU-aware (~30 cases total).
- [x] Full ctest suite green locally.
- [x] All 5 attribution verifiers clean.
- [x] All commits GPG-signed.
- [ ] **Bench verification on HL2 hardware** (see spec §9 checklist):
      - [ ] Slider 0..51 sub-step regime: TXPostGenToneMag scales 0.4..0.91,
            no PA attenuator change, output 0..~0.4 W.
      - [ ] Slider 52..99 attenuator regime: full audio gain, attenuator
            walks 0/2/4/.../90, output ~1..5 W.
      - [ ] Slider 99: 5 W exactly into dummy load (calibrated wattmeter).
      - [ ] Drive source = Fixed -7.5 dB matches RF Pwr slider at 60.
      - [ ] SWR meter reads ~1.0 throughout (no rev-power scaling regression).

Bench coordinator: TBD (JJ / Chris / Steve).

## Risk

DSP changes drive the actual signal on the wire. Tests cover the math;
the bench confirms the math drives the radio correctly. Cannot ship to
release without bench pass.

J.J. Boyd ~ KG4VCF
```

---

## Self-review

After writing this plan, I checked it against the spec section-by-section:

**Spec coverage:**
- §5.1 (HpsdrModel constants) → Task 1 ✓
- §5.2 (TxApplet rescale + label + tooltip) → Task 7 ✓
- §5.3 (TransmitSetupPages grpPATune + per-band grid) → Tasks 8 + 9 ✓
- §5.4.1 (sub-step DSP modulation) → Task 4 ✓
- §5.4.2 (audio-volume formula) → Task 5 ✓
- §5.4.3 (m_txPostGenToneMag property) → Task 3 ✓
- §5.4.4 (cite cleanup) → Task 6 (folded with polymorphic clamp) ✓
- §5.5 (TxChannel WDSP wrapper) → Task 2 ✓
- §6 (persistence) → Task 6 ✓
- §7 (tests) → covered across Tasks 1-9 ✓
- §8 (attribution) → Task 11 ✓
- §9 (bench) → Task 12 PR description references the spec checklist ✓

**Type consistency:**
- `paMaxWattsFor` (existing) → spec uses `rfPowerSliderMaxFor` etc. (new). Different names; no collision.
- `txPostGenToneMag` / `setTxPostGenToneMag` / `txPostGenToneMagChanged` consistent across Tasks 3, 4, 10.
- `setPostGenToneMag` (TxChannel) consistent in Tasks 2 + 10.
- `applyHpsdrModel` consistent across Tasks 8 + 9.

**Placeholder scan:** none of "TBD", "TODO", "implement later", "fill in details", "similar to Task N", "add appropriate error handling" appear in any task body. Code blocks present in every code step.

**Open question carry-overs from spec §10.2:**
- `SetTXAPostGenToneMag` exact name → resolved (verified: `third_party/wdsp/src/gen.c:800`).
- HL2 16-vs-15 step naming → resolved (chose 16, includes zero step, baked into Task 1).
- Per-band grid on HL2 → resolved (kept visible per spec).

**Bench coordination from spec §9:** noted as a TBD in Task 12 PR description. Not blocking the implementation; only blocking ship.

Plan ready for execution.
