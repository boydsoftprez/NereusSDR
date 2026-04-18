# Auto AGC-T, Setup AGC/ALC Wiring, and Slider Visual Enhancement

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port Thetis Auto AGC-T (noise-floor-tracking automatic AGC threshold), wire all Setup → AGC/ALC controls to WDSP, and visually enhance the AGC-T slider in both VFO Audio tab and RxApplet.

**Architecture:** NoiseFloorTracker (lerp-based NF estimator) feeds an auto-AGC timer on RadioModel. SliceModel gains 5 new properties. AgcAlcSetupPage controls get enabled and wired. Both AGC-T sliders show green-handle + NF-fill + AUTO-badge when auto is active.

**Tech Stack:** C++20, Qt6 (signals/slots, QTimer, QSlider stylesheet), WDSP AGC API

**Spec:** `docs/superpowers/specs/2026-04-18-auto-agc-t-and-setup-controls-design.md`

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `src/core/NoiseFloorTracker.h` | Create | Lerp-based NF tracker (Thetis display.cs port) |
| `src/core/NoiseFloorTracker.cpp` | Create | Implementation |
| `tests/tst_noise_floor_tracker.cpp` | Create | Unit tests for lerp, fast-attack, isGood |
| `src/models/SliceModel.h` | Modify | Add 5 new AGC properties |
| `src/models/SliceModel.cpp` | Modify | Setters, persistence |
| `src/core/RxChannel.h` | Modify | Add setAgcHangThreshold, setAgcFixedGain |
| `src/core/RxChannel.cpp` | Modify | WDSP wiring for new methods |
| `tests/tst_slice_auto_agc.cpp` | Create | SliceModel auto-AGC property tests |
| `src/models/RadioModel.h` | Modify | Add auto-AGC timer, NoiseFloorTracker* |
| `src/models/RadioModel.cpp` | Modify | Auto-AGC timer tick, wiring |
| `src/gui/MainWindow.cpp` | Modify | Connect FFTEngine → NoiseFloorTracker |
| `src/gui/setup/DspSetupPages.h` | Modify | Add member pointers for AGC controls |
| `src/gui/setup/DspSetupPages.cpp` | Modify | Enable + wire AGC/ALC controls |
| `src/gui/widgets/VfoWidget.h` | Modify | Add m_agcAutoLabel, m_agcInfoLabel |
| `src/gui/widgets/VfoWidget.cpp` | Modify | Auto-mode styling, right-click toggle |
| `src/gui/applets/RxApplet.h` | Modify | Add m_agcAutoLabel, m_agcInfoLabel |
| `src/gui/applets/RxApplet.cpp` | Modify | Auto-mode styling, right-click toggle |
| `tests/CMakeLists.txt` | Modify | Register new tests |

---

### Task 1: NoiseFloorTracker — Test + Implementation

**Files:**
- Create: `src/core/NoiseFloorTracker.h`
- Create: `src/core/NoiseFloorTracker.cpp`
- Create: `tests/tst_noise_floor_tracker.cpp`
- Modify: `tests/CMakeLists.txt`

**Attribution:** This file ports from Thetis `display.cs`. Use the exact `display.cs` header
from the spec § 6 "Byte-for-byte source file headers".

- [ ] **Step 1: Write the failing test**

Create `tests/tst_noise_floor_tracker.cpp` with the full Thetis `display.cs` header block
(byte-for-byte from spec § 6), then:

```cpp
#include <QtTest/QtTest>
#include "core/NoiseFloorTracker.h"

using namespace NereusSDR;

class TestNoiseFloorTracker : public QObject {
    Q_OBJECT

private slots:
    // ── defaults ────────────────────────────────────────────────────
    void defaultsAreCorrect()
    {
        NoiseFloorTracker t;
        QCOMPARE(t.noiseFloor(), -200.0f);    // From Thetis display.cs:4628
        QCOMPARE(t.isGood(), false);
        QCOMPARE(t.attackTimeMs(), 2000.0f);   // From Thetis display.cs:4638
    }

    // ── single feed converges ───────────────────────────────────────
    void singleFeedUpdatesFloor()
    {
        NoiseFloorTracker t;
        QVector<float> bins(512, -90.0f);
        t.feed(bins, 100.0f);  // 100ms frame interval
        // After one feed, lerp moves from -200 toward -90
        QVERIFY(t.noiseFloor() > -200.0f);
        QVERIFY(t.noiseFloor() < -90.0f);
    }

    // ── convergence after many feeds ────────────────────────────────
    void convergesAfterManyFeeds()
    {
        NoiseFloorTracker t;
        QVector<float> bins(512, -95.0f);
        // Feed for 5 seconds (50 frames at 100ms)
        for (int i = 0; i < 50; ++i) {
            t.feed(bins, 100.0f);
        }
        // Should be very close to -95 after 5s with 2s attack
        QVERIFY(qAbs(t.noiseFloor() - (-95.0f)) < 1.0f);
        QCOMPARE(t.isGood(), true);
    }

    // ── fast attack resets immediately ──────────────────────────────
    void fastAttackResetsLerp()
    {
        NoiseFloorTracker t;
        QVector<float> bins(512, -80.0f);
        // Converge to -80
        for (int i = 0; i < 50; ++i) {
            t.feed(bins, 100.0f);
        }
        QVERIFY(qAbs(t.noiseFloor() - (-80.0f)) < 1.0f);

        // Trigger fast attack (simulates retune)
        t.triggerFastAttack();
        QCOMPARE(t.isGood(), false);

        // Feed new floor
        QVector<float> newBins(512, -110.0f);
        t.feed(newBins, 100.0f);
        // Should jump close to -110, not lerp slowly from -80
        QVERIFY(qAbs(t.noiseFloor() - (-110.0f)) < 5.0f);
    }

    // ── isGood becomes true after attack period ─────────────────────
    void isGoodAfterAttackPeriod()
    {
        NoiseFloorTracker t;
        QVector<float> bins(512, -85.0f);
        QCOMPARE(t.isGood(), false);

        // Feed for less than attack time (2000ms) — not yet good
        for (int i = 0; i < 10; ++i) {  // 1000ms
            t.feed(bins, 100.0f);
        }
        QCOMPARE(t.isGood(), false);

        // Feed past attack time
        for (int i = 0; i < 15; ++i) {  // another 1500ms = total 2500ms
            t.feed(bins, 100.0f);
        }
        QCOMPARE(t.isGood(), true);
    }

    // ── empty bins returns without crash ─────────────────────────────
    void emptyBinsNoOp()
    {
        NoiseFloorTracker t;
        QVector<float> empty;
        t.feed(empty, 100.0f);
        QCOMPARE(t.noiseFloor(), -200.0f);
    }
};

QTEST_MAIN(TestNoiseFloorTracker)
#include "tst_noise_floor_tracker.moc"
```

- [ ] **Step 2: Create the header**

Create `src/core/NoiseFloorTracker.h` with the exact `display.cs` header block
(byte-for-byte from spec § 6):

```cpp
// [Full display.cs header — see spec § 6]

#pragma once

#include <QVector>

namespace NereusSDR {

// Lerp-based noise floor tracker ported from Thetis display.cs.
// From Thetis v2.10.3.13 display.cs:4628-4693 — noise floor estimation
class NoiseFloorTracker {
public:
    NoiseFloorTracker();

    // Feed a frame of FFT magnitude bins (dBm).
    // frameIntervalMs = time since last feed (typically ~33ms at 30fps).
    // From Thetis v2.10.3.13 display.cs:4633-4636 — bin average + lerp
    void feed(const QVector<float>& binsDbm, float frameIntervalMs);

    // Current smoothed noise floor estimate (dBm).
    // From Thetis v2.10.3.13 display.cs:4664 — NoiseFloorRX1 getter
    float noiseFloor() const noexcept { return m_lerpAverage; }

    // True after at least one full attack period has elapsed since
    // construction or last fast-attack reset.
    // From Thetis v2.10.3.13 display.cs:4654 — IsNoiseFloorGoodRX1
    bool isGood() const noexcept { return m_isGood; }

    // Reset lerp to next bin average (immediate jump).
    // Called on retune (>500 Hz delta), mode change, preamp change.
    // From Thetis v2.10.3.13 display.cs:917-926 — FastAttackNoiseFloorRX1
    void triggerFastAttack();

    // From Thetis v2.10.3.13 display.cs:4638 — m_fAttackTimeInMSForRX1
    float attackTimeMs() const noexcept { return m_attackTimeMs; }
    void setAttackTimeMs(float ms) { m_attackTimeMs = ms; }

private:
    // From Thetis v2.10.3.13 display.cs:4628 — m_fNoiseFloorRX1 = -200
    float m_lerpAverage{-200.0f};

    // From Thetis v2.10.3.13 display.cs:4638 — m_fAttackTimeInMSForRX1 = 2000
    float m_attackTimeMs{2000.0f};

    // From Thetis v2.10.3.13 display.cs:4630 — m_bNoiseFloorGoodRX1
    bool m_isGood{false};

    bool m_fastAttackPending{true};  // first feed acts as fast-attack
    float m_elapsedMs{0.0f};         // time since last fast-attack
};

}  // namespace NereusSDR
```

- [ ] **Step 3: Implement NoiseFloorTracker.cpp**

Create `src/core/NoiseFloorTracker.cpp`:

```cpp
// [NereusSDR header + display.cs header — see spec § 6]

#include "NoiseFloorTracker.h"

#include <cmath>
#include <numeric>

namespace NereusSDR {

NoiseFloorTracker::NoiseFloorTracker() = default;

void NoiseFloorTracker::feed(const QVector<float>& binsDbm, float frameIntervalMs)
{
    if (binsDbm.isEmpty()) {
        return;
    }

    // From Thetis v2.10.3.13 display.cs:4633 — m_fFFTBinAverageRX1
    const float binAverage = std::accumulate(binsDbm.cbegin(), binsDbm.cend(), 0.0f)
                             / static_cast<float>(binsDbm.size());

    if (m_fastAttackPending) {
        // From Thetis v2.10.3.13 display.cs:917-926 — FastAttackNoiseFloorRX1
        // Jump directly to current bin average
        m_lerpAverage = binAverage;
        m_fastAttackPending = false;
        m_elapsedMs = 0.0f;
        m_isGood = false;
        return;
    }

    // From Thetis v2.10.3.13 display.cs:4635-4636 — lerp toward bin average
    const float alpha = 1.0f - std::exp(-frameIntervalMs / m_attackTimeMs);
    m_lerpAverage += alpha * (binAverage - m_lerpAverage);

    m_elapsedMs += frameIntervalMs;

    // From Thetis v2.10.3.13 display.cs:4654 — IsNoiseFloorGoodRX1
    // Good after at least one full attack period
    if (!m_isGood && m_elapsedMs >= m_attackTimeMs) {
        m_isGood = true;
    }
}

void NoiseFloorTracker::triggerFastAttack()
{
    // From Thetis v2.10.3.13 display.cs:917-926 — FastAttackNoiseFloorRX1 = true
    m_fastAttackPending = true;
    m_isGood = false;
}

}  // namespace NereusSDR
```

- [ ] **Step 4: Register test in CMakeLists.txt**

Add to `tests/CMakeLists.txt` after the existing `tst_noise_floor_estimator` line:

```cmake
# ── Auto AGC-T noise floor tracker ──────────────────────────────────
nereus_add_test(tst_noise_floor_tracker)
```

- [ ] **Step 5: Build and run tests**

Run: `cmake --build build --target tst_noise_floor_tracker && ./build/tests/tst_noise_floor_tracker`
Expected: All 5 tests PASS

- [ ] **Step 6: Commit**

```bash
git add src/core/NoiseFloorTracker.h src/core/NoiseFloorTracker.cpp \
        tests/tst_noise_floor_tracker.cpp tests/CMakeLists.txt
git commit -m "feat: NoiseFloorTracker — lerp-based NF estimator from Thetis display.cs"
```

---

### Task 2: SliceModel — New Auto-AGC Properties

**Files:**
- Modify: `src/models/SliceModel.h:153-176,473-500`
- Modify: `src/models/SliceModel.cpp:365-403,701-705`
- Create: `tests/tst_slice_auto_agc.cpp`
- Modify: `tests/CMakeLists.txt`

**Attribution:** Properties from Thetis `console.cs` and `setup.cs`. Use both
headers (byte-for-byte) in the test file.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_slice_auto_agc.cpp` with both `console.cs` and `setup.cs` headers:

```cpp
// [NereusSDR header + console.cs header + setup.cs header — byte-for-byte from spec § 6]

#include <QtTest/QtTest>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceAutoAgc : public QObject {
    Q_OBJECT

private slots:
    // ── autoAgcEnabled ──────────────────────────────────────────────
    void autoAgcEnabledDefault()
    {
        SliceModel s;
        // From Thetis v2.10.3.13 console.cs:45926 — m_bAutoAGCRX1 default false
        QCOMPARE(s.autoAgcEnabled(), false);
    }

    void autoAgcEnabledSetEmits()
    {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::autoAgcEnabledChanged);
        s.setAutoAgcEnabled(true);
        QCOMPARE(s.autoAgcEnabled(), true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
    }

    void autoAgcEnabledNoOpIfSame()
    {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::autoAgcEnabledChanged);
        s.setAutoAgcEnabled(false);  // same as default
        QCOMPARE(spy.count(), 0);
    }

    // ── autoAgcOffset ───────────────────────────────────────────────
    void autoAgcOffsetDefault()
    {
        SliceModel s;
        // From Thetis v2.10.3.13 setup.designer.cs:38630 — udRX1AutoAGCOffset.Value = 20
        QCOMPARE(s.autoAgcOffset(), 20.0);
    }

    void autoAgcOffsetSetEmits()
    {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::autoAgcOffsetChanged);
        s.setAutoAgcOffset(35.0);
        QCOMPARE(s.autoAgcOffset(), 35.0);
        QCOMPARE(spy.count(), 1);
    }

    // ── agcFixedGain ────────────────────────────────────────────────
    void agcFixedGainDefault()
    {
        SliceModel s;
        // From Thetis v2.10.3.13 setup.designer.cs:39320 — udDSPAGCFixedGaindB default 20
        QCOMPARE(s.agcFixedGain(), 20);
    }

    void agcFixedGainSetEmits()
    {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::agcFixedGainChanged);
        s.setAgcFixedGain(50);
        QCOMPARE(s.agcFixedGain(), 50);
        QCOMPARE(spy.count(), 1);
    }

    // ── agcHangThreshold ────────────────────────────────────────────
    void agcHangThresholdDefault()
    {
        SliceModel s;
        // From Thetis v2.10.3.13 setup.designer.cs:39418 — tbDSPAGCHangThreshold default 0
        QCOMPARE(s.agcHangThreshold(), 0);
    }

    void agcHangThresholdSetEmits()
    {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::agcHangThresholdChanged);
        s.setAgcHangThreshold(50);
        QCOMPARE(s.agcHangThreshold(), 50);
        QCOMPARE(spy.count(), 1);
    }

    // ── agcMaxGain ──────────────────────────────────────────────────
    void agcMaxGainDefault()
    {
        SliceModel s;
        // From Thetis v2.10.3.13 setup.designer.cs:39245 — udDSPAGCMaxGaindB default 90
        QCOMPARE(s.agcMaxGain(), 90);
    }

    void agcMaxGainSetEmits()
    {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::agcMaxGainChanged);
        s.setAgcMaxGain(110);
        QCOMPARE(s.agcMaxGain(), 110);
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestSliceAutoAgc)
#include "tst_slice_auto_agc.moc"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target tst_slice_auto_agc 2>&1 | head -20`
Expected: Compile error — `autoAgcEnabled` not a member of `SliceModel`

- [ ] **Step 3: Add properties to SliceModel.h**

Add Q_PROPERTY declarations near line 176 (after `agcDecay`):

```cpp
Q_PROPERTY(bool autoAgcEnabled READ autoAgcEnabled WRITE setAutoAgcEnabled NOTIFY autoAgcEnabledChanged)
Q_PROPERTY(double autoAgcOffset READ autoAgcOffset WRITE setAutoAgcOffset NOTIFY autoAgcOffsetChanged)
Q_PROPERTY(int agcFixedGain READ agcFixedGain WRITE setAgcFixedGain NOTIFY agcFixedGainChanged)
Q_PROPERTY(int agcHangThreshold READ agcHangThreshold WRITE setAgcHangThreshold NOTIFY agcHangThresholdChanged)
Q_PROPERTY(int agcMaxGain READ agcMaxGain WRITE setAgcMaxGain NOTIFY agcMaxGainChanged)
```

Add accessors near line 317 (after `agcDecay` accessors):

```cpp
// From Thetis v2.10.3.13 console.cs:45926 — m_bAutoAGCRX1
bool autoAgcEnabled() const { return m_autoAgcEnabled; }
void setAutoAgcEnabled(bool on);

// From Thetis v2.10.3.13 console.cs:45913 — m_dAutoAGCOffsetRX1
double autoAgcOffset() const { return m_autoAgcOffset; }
void setAutoAgcOffset(double dB);

// From Thetis v2.10.3.13 setup.designer.cs:39320 — udDSPAGCFixedGaindB
int agcFixedGain() const { return m_agcFixedGain; }
void setAgcFixedGain(int dB);

// From Thetis v2.10.3.13 setup.designer.cs:39418 — tbDSPAGCHangThreshold
int agcHangThreshold() const { return m_agcHangThreshold; }
void setAgcHangThreshold(int val);

// From Thetis v2.10.3.13 setup.designer.cs:39245 — udDSPAGCMaxGaindB
int agcMaxGain() const { return m_agcMaxGain; }
void setAgcMaxGain(int dB);
```

Add signals near line 447 (after `agcDecayChanged`):

```cpp
void autoAgcEnabledChanged(bool on);
void autoAgcOffsetChanged(double dB);
void agcFixedGainChanged(int dB);
void agcHangThresholdChanged(int val);
void agcMaxGainChanged(int dB);
```

Add member variables near line 500 (after `m_agcDecay`):

```cpp
bool   m_autoAgcEnabled{false};   // From Thetis v2.10.3.13 console.cs:45926
double m_autoAgcOffset{20.0};     // From Thetis v2.10.3.13 setup.designer.cs:38630
int    m_agcFixedGain{20};        // From Thetis v2.10.3.13 setup.designer.cs:39320
int    m_agcHangThreshold{0};     // From Thetis v2.10.3.13 setup.designer.cs:39418
int    m_agcMaxGain{90};          // From Thetis v2.10.3.13 setup.designer.cs:39245
```

- [ ] **Step 4: Add setters to SliceModel.cpp**

After the `setAgcDecay` implementation (near line 403), add:

```cpp
void SliceModel::setAutoAgcEnabled(bool on)
{
    if (m_autoAgcEnabled != on) {
        m_autoAgcEnabled = on;
        emit autoAgcEnabledChanged(on);
    }
}

void SliceModel::setAutoAgcOffset(double dB)
{
    if (!qFuzzyCompare(m_autoAgcOffset, dB)) {
        m_autoAgcOffset = dB;
        emit autoAgcOffsetChanged(dB);
    }
}

void SliceModel::setAgcFixedGain(int dB)
{
    if (m_agcFixedGain != dB) {
        m_agcFixedGain = dB;
        emit agcFixedGainChanged(dB);
    }
}

void SliceModel::setAgcHangThreshold(int val)
{
    if (m_agcHangThreshold != val) {
        m_agcHangThreshold = val;
        emit agcHangThresholdChanged(val);
    }
}

void SliceModel::setAgcMaxGain(int dB)
{
    if (m_agcMaxGain != dB) {
        m_agcMaxGain = dB;
        emit agcMaxGainChanged(dB);
    }
}
```

- [ ] **Step 5: Add persistence to SliceModel.cpp**

In `saveToSettings` (near line 705, after `AgcDecay`):

```cpp
s.setValue(bp + QStringLiteral("AgcAutoEnabled"), m_autoAgcEnabled ? QStringLiteral("True") : QStringLiteral("False"));
s.setValue(bp + QStringLiteral("AgcAutoOffset"), m_autoAgcOffset);
s.setValue(bp + QStringLiteral("AgcFixedGain"), m_agcFixedGain);
s.setValue(bp + QStringLiteral("AgcHangThreshold"), m_agcHangThreshold);
s.setValue(bp + QStringLiteral("AgcMaxGain"), m_agcMaxGain);
```

In `loadFromSettings`, add matching reads with defaults.

- [ ] **Step 6: Register test and run**

Add to `tests/CMakeLists.txt`:

```cmake
nereus_add_test(tst_slice_auto_agc)
```

Run: `cmake --build build --target tst_slice_auto_agc && ./build/tests/tst_slice_auto_agc`
Expected: All 10 tests PASS

- [ ] **Step 7: Commit**

```bash
git add src/models/SliceModel.h src/models/SliceModel.cpp \
        tests/tst_slice_auto_agc.cpp tests/CMakeLists.txt
git commit -m "feat: SliceModel auto-AGC properties from Thetis console.cs + setup.cs"
```

---

### Task 3: RxChannel — New WDSP Methods

**Files:**
- Modify: `src/core/RxChannel.h:249-279,459-467`
- Modify: `src/core/RxChannel.cpp:374-468`

- [ ] **Step 1: Add setAgcHangThreshold to RxChannel.h**

Near line 279 (after `setAgcDecay`):

```cpp
// From Thetis v2.10.3.13 setup.cs:9081 — tbDSPAGCHangThreshold_Scroll
int agcHangThreshold() const { return m_agcHangThreshold.load(); }
void setAgcHangThreshold(int val);

// From Thetis v2.10.3.13 setup.cs:9001 — udDSPAGCFixedGaindB_ValueChanged
int agcFixedGain() const { return m_agcFixedGain.load(); }
void setAgcFixedGain(int dB);

// From Thetis v2.10.3.13 setup.cs:9011 — udDSPAGCMaxGaindB_ValueChanged
int agcMaxGain() const { return m_agcMaxGain.load(); }
void setAgcMaxGain(int dB);
```

Add atomic members near line 467:

```cpp
std::atomic<int> m_agcHangThreshold{0};   // From Thetis v2.10.3.13 setup.designer.cs:39418
std::atomic<int> m_agcFixedGain{20};      // From Thetis v2.10.3.13 setup.designer.cs:39320
std::atomic<int> m_agcMaxGain{90};        // From Thetis v2.10.3.13 setup.designer.cs:39245
```

- [ ] **Step 2: Implement in RxChannel.cpp**

After the `setAgcDecay` implementation (near line 468):

```cpp
void RxChannel::setAgcHangThreshold(int val)
{
    if (val == m_agcHangThreshold.load()) {
        return;
    }
    m_agcHangThreshold.store(val);
#ifdef HAVE_WDSP
    // From Thetis v2.10.3.13 setup.cs:9081 — SetRXAAGCHangThreshold
    SetRXAAGCHangThreshold(m_channelId, val);
#endif
}

void RxChannel::setAgcFixedGain(int dB)
{
    if (dB == m_agcFixedGain.load()) {
        return;
    }
    m_agcFixedGain.store(dB);
#ifdef HAVE_WDSP
    // From Thetis v2.10.3.13 setup.cs:9001 — SetRXAAGCTop for FIXD mode
    // console.cs:45994-46007 — uses AGCFixedGain when mode == FIXD
    SetRXAAGCFixed(m_channelId, static_cast<double>(dB));
#endif
}

void RxChannel::setAgcMaxGain(int dB)
{
    if (dB == m_agcMaxGain.load()) {
        return;
    }
    m_agcMaxGain.store(dB);
#ifdef HAVE_WDSP
    // From Thetis v2.10.3.13 setup.cs:9011 — SetRXAAGCTop for non-FIXD modes
    SetRXAAGCTop(m_channelId, static_cast<double>(dB));
#endif
}
```

- [ ] **Step 3: Build to verify compilation**

Run: `cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

- [ ] **Step 4: Commit**

```bash
git add src/core/RxChannel.h src/core/RxChannel.cpp
git commit -m "feat: RxChannel AGC hang threshold, fixed gain, max gain — Thetis setup.cs"
```

---

### Task 4: RadioModel — Auto AGC-T Timer + NoiseFloorTracker Wiring

**Files:**
- Modify: `src/models/RadioModel.h:261`
- Modify: `src/models/RadioModel.cpp:768-798`
- Modify: `src/gui/MainWindow.cpp:564-600`

**Attribution:** Ports from both `console.cs` and `display.cs`.

- [ ] **Step 1: Add members to RadioModel.h**

Near line 261 (after `m_syncingAgc`):

```cpp
class NoiseFloorTracker;  // forward declare at top of file

// From Thetis v2.10.3.13 console.cs:46057 — tmrAutoAGC (500ms interval)
QTimer* m_autoAgcTimer{nullptr};
NoiseFloorTracker* m_noiseFloorTracker{nullptr};
```

- [ ] **Step 2: Add auto-AGC timer setup in RadioModel.cpp**

In constructor or `wireSliceSignals()`, after the existing AGC wiring (~line 798):

```cpp
// ── Auto AGC-T timer ────────────────────────────────────────────────
// From Thetis v2.10.3.13 console.cs:46057 — tmrAutoAGC_Tick, 500ms interval
m_autoAgcTimer = new QTimer(this);
m_autoAgcTimer->setInterval(500);
connect(m_autoAgcTimer, &QTimer::timeout, this, [this]() {
    SliceModel* slice = m_activeSlice;
    if (!slice || !slice->autoAgcEnabled()) {
        return;
    }
    // From Thetis v2.10.3.13 console.cs:46059 — guard: skip if not connected or MOX
    if (!m_connection || !m_connection->isStreaming()) {
        return;
    }
    if (!m_noiseFloorTracker || !m_noiseFloorTracker->isGood()) {
        return;
    }

    // From Thetis v2.10.3.13 console.cs:46107-46115
    const double noiseFloor = static_cast<double>(m_noiseFloorTracker->noiseFloor());
    const double threshold = noiseFloor + slice->autoAgcOffset();

    // From Thetis v2.10.3.13 console.cs:45960-46008 — setAGCThresholdPoint
    const double clamped = std::clamp(threshold, -160.0, 2.0);
    const int threshInt = static_cast<int>(std::round(clamped));

    // Set threshold (this triggers the existing agcThresholdChanged wiring
    // which calls RxChannel::setAgcThreshold + readback + RF gain sync)
    if (slice->agcThreshold() != threshInt) {
        slice->setAgcThreshold(threshInt);
    }
});
m_autoAgcTimer->start();
```

- [ ] **Step 3: Wire auto-disable on manual slider drag**

In the existing `agcThresholdChanged` connection (~line 768), add auto-disable
before the WDSP call:

```cpp
connect(slice, &SliceModel::agcThresholdChanged, this, [this](int dBu) {
    if (m_syncingAgc) { return; }

    // From Thetis v2.10.3.13 console.cs:49129-49130 — manual drag disables auto
    SliceModel* s = m_activeSlice;
    if (s && s->autoAgcEnabled()) {
        // Only disable if this wasn't triggered by the auto timer itself
        // (the timer sets m_syncingAgc before calling setAgcThreshold — 
        // but actually it doesn't, it calls setAgcThreshold directly.
        // So we need to distinguish. Use a flag.)
        s->setAutoAgcEnabled(false);
    }

    RxChannel* rxCh = m_wdspEngine->rxChannel(0);
    // ... rest of existing code unchanged ...
});
```

**Important:** The auto timer must set `m_syncingAgc = true` before calling
`slice->setAgcThreshold()` to prevent the connection from disabling auto mode.
Update the timer lambda:

```cpp
// In the timer lambda, wrap the setAgcThreshold call:
m_syncingAgc = true;
slice->setAgcThreshold(threshInt);
m_syncingAgc = false;
```

- [ ] **Step 4: Wire new properties to WDSP**

After the existing AGC wiring section (~line 798), add connections for the new
properties:

```cpp
// From Thetis v2.10.3.13 setup.cs:9081 — hang threshold
connect(slice, &SliceModel::agcHangThresholdChanged, this, [this](int val) {
    RxChannel* rxCh = m_wdspEngine->rxChannel(0);
    if (rxCh) {
        rxCh->setAgcHangThreshold(val);
    }
    scheduleSettingsSave();
});

// From Thetis v2.10.3.13 setup.cs:9001 — fixed gain
connect(slice, &SliceModel::agcFixedGainChanged, this, [this](int dB) {
    RxChannel* rxCh = m_wdspEngine->rxChannel(0);
    if (rxCh) {
        rxCh->setAgcFixedGain(dB);
    }
    scheduleSettingsSave();
});

// From Thetis v2.10.3.13 setup.cs:9011 — max gain
connect(slice, &SliceModel::agcMaxGainChanged, this, [this](int dB) {
    RxChannel* rxCh = m_wdspEngine->rxChannel(0);
    if (rxCh) {
        rxCh->setAgcMaxGain(dB);
    }
    scheduleSettingsSave();
});
```

- [ ] **Step 5: Connect FFTEngine → NoiseFloorTracker in MainWindow.cpp**

In `MainWindow.cpp`, near line 597 (after the ClarityController FFT connection):

```cpp
// ── NoiseFloorTracker for Auto AGC-T ────────────────────────────────
#include "core/NoiseFloorTracker.h"

auto* nfTracker = new NoiseFloorTracker;  // owned by RadioModel
m_radioModel->setNoiseFloorTracker(nfTracker);

connect(m_fftEngine, &FFTEngine::fftReady,
        this, [nfTracker](int /*rxId*/, const QVector<float>& binsDbm) {
    // Approximate frame interval from FFT rate (~30fps = ~33ms)
    static constexpr float kFrameIntervalMs = 33.0f;
    nfTracker->feed(binsDbm, kFrameIntervalMs);
});

// Fast-attack triggers
// From Thetis v2.10.3.13 display.cs:905 — freq change > 500 Hz
connect(m_radioModel->activeSlice(), &SliceModel::frequencyChanged,
        this, [nfTracker](double /*hz*/) {
    // Simplified: trigger on every retune. Thetis checks delta > 500 Hz
    // but retunes are infrequent enough that always triggering is fine.
    nfTracker->triggerFastAttack();
});
connect(m_radioModel->activeSlice(), &SliceModel::modeChanged,
        this, [nfTracker]() {
    nfTracker->triggerFastAttack();
});
```

Add `setNoiseFloorTracker(NoiseFloorTracker* t)` accessor to `RadioModel.h`.

- [ ] **Step 6: Build and verify**

Run: `cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

- [ ] **Step 7: Commit**

```bash
git add src/models/RadioModel.h src/models/RadioModel.cpp \
        src/gui/MainWindow.cpp
git commit -m "feat: Auto AGC-T timer + NoiseFloorTracker wiring — Thetis console.cs:46057"
```

---

### Task 5: Setup → AGC/ALC Control Wiring

**Files:**
- Modify: `src/gui/setup/DspSetupPages.h:90-94`
- Modify: `src/gui/setup/DspSetupPages.cpp:35-74`

**Attribution:** Ports from `setup.cs`. Use exact header.

- [ ] **Step 1: Add member pointers to DspSetupPages.h**

Replace the stub class declaration:

```cpp
class AgcAlcSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit AgcAlcSetupPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void updateCustomGating(AGCMode mode);

    QComboBox*  m_agcModeCombo{nullptr};
    QSpinBox*   m_agcAttack{nullptr};
    QSpinBox*   m_agcDecay{nullptr};
    QSpinBox*   m_agcHang{nullptr};
    QSlider*    m_agcSlope{nullptr};
    QSpinBox*   m_agcMaxGain{nullptr};
    QSpinBox*   m_agcFixedGain{nullptr};
    QSlider*    m_agcHangThresh{nullptr};
    QCheckBox*  m_autoAgcChk{nullptr};
    QSpinBox*   m_autoAgcOffset{nullptr};
};
```

- [ ] **Step 2: Rewrite AgcAlcSetupPage constructor**

Replace the entire constructor in `DspSetupPages.cpp` (lines 35-74). Remove
`disableGroup(agcGrp)`. Wire each control to SliceModel via `model()->activeSlice()`:

```cpp
AgcAlcSetupPage::AgcAlcSetupPage(RadioModel* radio, QWidget* parent)
    : SetupPage("AGC/ALC", radio, parent)
{
    SliceModel* slice = radio->activeSlice();

    // ── RX1 AGC ─────────────────────────────────────────────────────
    QGroupBox* agcGrp = addSection("RX1 AGC");
    QVBoxLayout* agcLay = qobject_cast<QVBoxLayout*>(agcGrp->layout());

    // Mode combo — From Thetis v2.10.3.13 setup.cs:8996
    m_agcModeCombo = new QComboBox;
    m_agcModeCombo->addItems({"Off", "Long", "Slow", "Med", "Fast", "Custom"});
    m_agcModeCombo->setCurrentIndex(static_cast<int>(slice->agcMode()));
    addLabeledCombo(agcLay, "Mode", m_agcModeCombo);
    connect(m_agcModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, slice](int idx) {
        auto mode = static_cast<AGCMode>(idx);
        slice->setAgcMode(mode);
        updateCustomGating(mode);
    });

    // Attack — From Thetis v2.10.3.13 setup.cs:9054
    m_agcAttack = new QSpinBox;
    m_agcAttack->setRange(1, 1000);
    m_agcAttack->setSuffix(" ms");
    m_agcAttack->setValue(slice->agcAttack());
    addLabeledSpinner(agcLay, "Attack", m_agcAttack);
    connect(m_agcAttack, QOverload<int>::of(&QSpinBox::valueChanged),
            slice, &SliceModel::setAgcAttack);

    // Decay — From Thetis v2.10.3.13 setup.cs:9040 (disabled unless Custom)
    m_agcDecay = new QSpinBox;
    m_agcDecay->setRange(1, 5000);
    m_agcDecay->setSuffix(" ms");
    m_agcDecay->setValue(slice->agcDecay());
    addLabeledSpinner(agcLay, "Decay", m_agcDecay);
    connect(m_agcDecay, QOverload<int>::of(&QSpinBox::valueChanged),
            slice, &SliceModel::setAgcDecay);

    // Hang — From Thetis v2.10.3.13 setup.cs:9068 (disabled unless Custom, min 10)
    m_agcHang = new QSpinBox;
    m_agcHang->setRange(10, 5000);  // From Thetis v2.10.3.13 setup.designer.cs:39290 min 10
    m_agcHang->setSuffix(" ms");
    m_agcHang->setValue(slice->agcHang());
    addLabeledSpinner(agcLay, "Hang", m_agcHang);
    connect(m_agcHang, QOverload<int>::of(&QSpinBox::valueChanged),
            slice, &SliceModel::setAgcHang);

    // Slope — From Thetis v2.10.3.13 setup.cs:9054 — UI 0-20, WDSP receives ×10
    m_agcSlope = new QSlider(Qt::Horizontal);
    m_agcSlope->setRange(0, 20);  // From Thetis v2.10.3.13 setup.designer.cs:39380
    m_agcSlope->setValue(slice->agcSlope() / 10);  // stored as ×10 in SliceModel
    addLabeledSlider(agcLay, "Slope", m_agcSlope);
    connect(m_agcSlope, &QSlider::valueChanged, this, [slice](int val) {
        // From Thetis v2.10.3.13 setup.cs:9054 — RXAGCSlope = 10 * (int)(udDSPAGCSlope.Value)
        slice->setAgcSlope(val * 10);
    });

    // Max Gain — From Thetis v2.10.3.13 setup.cs:9011
    m_agcMaxGain = new QSpinBox;
    m_agcMaxGain->setRange(-20, 120);
    m_agcMaxGain->setSuffix(" dB");
    m_agcMaxGain->setValue(slice->agcMaxGain());
    addLabeledSpinner(agcLay, "Max Gain", m_agcMaxGain);
    connect(m_agcMaxGain, QOverload<int>::of(&QSpinBox::valueChanged),
            slice, &SliceModel::setAgcMaxGain);

    // Fixed Gain — From Thetis v2.10.3.13 setup.cs:9001
    m_agcFixedGain = new QSpinBox;
    m_agcFixedGain->setRange(-20, 120);
    m_agcFixedGain->setSuffix(" dB");
    m_agcFixedGain->setValue(slice->agcFixedGain());
    addLabeledSpinner(agcLay, "Fixed Gain", m_agcFixedGain);
    connect(m_agcFixedGain, QOverload<int>::of(&QSpinBox::valueChanged),
            slice, &SliceModel::setAgcFixedGain);

    // Hang Threshold — From Thetis v2.10.3.13 setup.cs:9081
    m_agcHangThresh = new QSlider(Qt::Horizontal);
    m_agcHangThresh->setRange(0, 100);
    m_agcHangThresh->setValue(slice->agcHangThreshold());
    addLabeledSlider(agcLay, "Hang Threshold", m_agcHangThresh);
    connect(m_agcHangThresh, &QSlider::valueChanged,
            slice, &SliceModel::setAgcHangThreshold);

    // Custom-mode gating — From Thetis v2.10.3.13 setup.cs:5046-5076
    updateCustomGating(slice->agcMode());

    // ── Auto AGC ────────────────────────────────────────────────────
    QGroupBox* autoGrp = addSection("Auto AGC");
    QVBoxLayout* autoLay = qobject_cast<QVBoxLayout*>(autoGrp->layout());

    // From Thetis v2.10.3.13 setup.designer.cs:38586 — chkAutoAGCRX1
    m_autoAgcChk = new QCheckBox("Auto AGC RX1");
    m_autoAgcChk->setChecked(slice->autoAgcEnabled());
    autoLay->addWidget(m_autoAgcChk);
    connect(m_autoAgcChk, &QCheckBox::toggled,
            slice, &SliceModel::setAutoAgcEnabled);

    // From Thetis v2.10.3.13 setup.designer.cs:38630 — udRX1AutoAGCOffset
    m_autoAgcOffset = new QSpinBox;
    m_autoAgcOffset->setRange(-60, 60);
    m_autoAgcOffset->setSuffix(" dB");
    m_autoAgcOffset->setValue(static_cast<int>(slice->autoAgcOffset()));
    addLabeledSpinner(autoLay, "± Offset", m_autoAgcOffset);
    connect(m_autoAgcOffset, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [slice](int val) {
        slice->setAutoAgcOffset(static_cast<double>(val));
    });

    // ── ALC (TX — disabled, deferred to Phase 3M) ───────────────────
    QGroupBox* alcGrp = addSection("ALC");
    QVBoxLayout* alcLay = qobject_cast<QVBoxLayout*>(alcGrp->layout());
    auto* alcDecay = new QSpinBox;
    alcDecay->setRange(1, 5000);
    alcDecay->setSuffix(" ms");
    addLabeledSpinner(alcLay, "Decay", alcDecay);
    auto* alcMaxGain = new QSpinBox;
    alcMaxGain->setRange(0, 120);
    alcMaxGain->setSuffix(" dB");
    addLabeledSpinner(alcLay, "Max Gain", alcMaxGain);
    disableGroup(alcGrp);

    // ── Leveler (TX — disabled, deferred to Phase 3M) ────────────────
    QGroupBox* levGrp = addSection("Leveler");
    QVBoxLayout* levLay = qobject_cast<QVBoxLayout*>(levGrp->layout());
    auto* levEnable = new QPushButton("Enable");
    addLabeledToggle(levLay, "Enable", levEnable);
    auto* levThresh = new QSpinBox;
    levThresh->setRange(-20, 20);
    levThresh->setSuffix(" dB");
    addLabeledSpinner(levLay, "Threshold", levThresh);
    auto* levDecay2 = new QSpinBox;
    levDecay2->setRange(1, 5000);
    levDecay2->setSuffix(" ms");
    addLabeledSpinner(levLay, "Decay", levDecay2);
    disableGroup(levGrp);
}

void AgcAlcSetupPage::updateCustomGating(AGCMode mode)
{
    // From Thetis v2.10.3.13 setup.cs:5046-5076 — CustomRXAGCEnabled
    const bool custom = (mode == AGCMode::Custom);
    m_agcDecay->setEnabled(custom);
    m_agcHang->setEnabled(custom);
}
```

- [ ] **Step 3: Add `#include <QCheckBox>` to DspSetupPages.cpp**

- [ ] **Step 4: Fix Slope range in existing code**

The existing slope slider has range 0-10, needs to be 0-20. This is handled by
the rewrite above.

- [ ] **Step 5: Build and verify**

Run: `cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

- [ ] **Step 6: Commit**

```bash
git add src/gui/setup/DspSetupPages.h src/gui/setup/DspSetupPages.cpp
git commit -m "feat: wire Setup AGC/ALC controls to SliceModel — Thetis setup.cs"
```

---

### Task 6: VfoWidget AGC-T Slider Visual Enhancement

**Files:**
- Modify: `src/gui/widgets/VfoWidget.h:504-512`
- Modify: `src/gui/widgets/VfoWidget.cpp:811-839`

- [ ] **Step 1: Add new members to VfoWidget.h**

Near line 512 (after `m_agcTLabel`):

```cpp
QLabel*  m_agcAutoLabel{nullptr};   // "AUTO" badge
QLabel*  m_agcInfoLabel{nullptr};   // "NF -62 dB · offset +20 · right-click to disable"
QWidget* m_agcNfFill{nullptr};      // dark green NF fill overlay
bool     m_autoAgcActive{false};
float    m_noiseFloorDbm{-200.0f};
```

Add signals:

```cpp
void autoAgcToggled(bool on);
```

- [ ] **Step 2: Modify buildAudioTab AGC-T row**

In `VfoWidget.cpp`, replace the AGC-T slider row (lines 811-839) with the
enhanced version that includes auto-mode visuals:

```cpp
// 6. AGC threshold slider row — enhanced with auto-AGC visuals
// From Thetis v2.10.3.13 console.cs:45977 — agc_thresh_point, range -160..0
{
    m_agcTContainer = new QWidget(audioWidget);
    auto* containerLayout = new QVBoxLayout(m_agcTContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(1);

    auto* row = new QHBoxLayout;
    m_agcTLabelWidget = new QLabel(QStringLiteral("AGC-T"), m_agcTContainer);
    m_agcTLabelWidget->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
    m_agcTLabelWidget->setFixedWidth(40);
    row->addWidget(m_agcTLabelWidget);

    m_agcTSlider = new QSlider(Qt::Horizontal, m_agcTContainer);
    m_agcTSlider->setRange(-160, 0);
    m_agcTSlider->setSingleStep(1);
    m_agcTSlider->setValue(-20);
    m_agcTSlider->setInvertedAppearance(true);
    m_agcTSlider->setStyleSheet(
        QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                        "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
    m_agcTSlider->setToolTip(QStringLiteral("AGC Max Gain - Operates similarly to traditional RF Gain. Right click to toggle AUTO based on noise floor."));
    m_agcTSlider->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_agcTSlider, &QWidget::customContextMenuRequested, this, [this]() {
        // From Thetis v2.10.3.13 console.cs:46119-46122 — ptbRF_Click toggle
        emit autoAgcToggled(!m_autoAgcActive);
    });
    row->addWidget(m_agcTSlider);

    m_agcTLabel = new QLabel(QStringLiteral("-20"), m_agcTContainer);
    m_agcTLabel->setStyleSheet(QStringLiteral("color: #c8d8e8; font-size: 11px;"));
    m_agcTLabel->setFixedWidth(32);
    m_agcTLabel->setAlignment(Qt::AlignRight);
    row->addWidget(m_agcTLabel);

    // AUTO badge — hidden by default
    m_agcAutoLabel = new QLabel(QStringLiteral("AUTO"), m_agcTContainer);
    m_agcAutoLabel->setStyleSheet(
        QStringLiteral("background: #1a2a1a; border: 1px solid #adff2f;"
                        "color: #adff2f; font-size: 7px; padding: 0 3px; border-radius: 2px;"));
    m_agcAutoLabel->setFixedHeight(14);
    m_agcAutoLabel->hide();
    row->addWidget(m_agcAutoLabel);

    containerLayout->addLayout(row);

    // Info sub-line — hidden by default
    m_agcInfoLabel = new QLabel(m_agcTContainer);
    m_agcInfoLabel->setStyleSheet(QStringLiteral("color: #33aa33; font-size: 7px; padding: 0 2px;"));
    m_agcInfoLabel->hide();
    containerLayout->addWidget(m_agcInfoLabel);

    connect(m_agcTSlider, &QSlider::valueChanged, this, [this](int val) {
        m_agcTLabel->setText(QString::number(val));
        if (!m_updatingFromModel) {
            emit agcThresholdChanged(val);
        }
    });

    audioLayout->addWidget(m_agcTContainer);
}
```

- [ ] **Step 3: Add updateAgcAutoVisuals method**

```cpp
void VfoWidget::updateAgcAutoVisuals(bool autoOn, float noiseFloorDbm, double offset)
{
    m_autoAgcActive = autoOn;
    m_noiseFloorDbm = noiseFloorDbm;

    if (autoOn) {
        // Green handle + NF fill
        m_agcTSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: qlineargradient("
                            "x1:0,y1:0,x2:1,y2:0, stop:0 #0a1a0a, stop:%1 #1a3a1a, "
                            "stop:%2 #1a2a3a, stop:1 #1a2a3a); height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #adff2f; width: 12px;"
                            " margin: -3px 0; border-radius: 6px; }")
                .arg(qBound(0.0, (noiseFloorDbm + 160.0) / 160.0, 1.0), 0, 'f', 3)
                .arg(qBound(0.0, (noiseFloorDbm + 160.0) / 160.0 + 0.01, 1.0), 0, 'f', 3));
        m_agcTLabelWidget->setStyleSheet(QStringLiteral("color: #adff2f; font-size: 11px; font-weight: bold;"));
        m_agcTContainer->setStyleSheet(QStringLiteral("border: 1px solid rgba(173,255,47,0.25); border-radius: 3px; background: #0a100a;"));
        m_agcAutoLabel->show();
        m_agcInfoLabel->setText(
            QStringLiteral("NF %1 dB \u00b7 offset +%2 \u00b7 right-click to disable")
                .arg(static_cast<int>(noiseFloorDbm))
                .arg(static_cast<int>(offset)));
        m_agcInfoLabel->show();
    } else {
        // Cyan handle, plain groove
        m_agcTSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        m_agcTLabelWidget->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
        m_agcTContainer->setStyleSheet(QString());
        m_agcAutoLabel->hide();
        m_agcInfoLabel->hide();
    }
}
```

- [ ] **Step 4: Build and verify**

Run: `cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

- [ ] **Step 5: Commit**

```bash
git add src/gui/widgets/VfoWidget.h src/gui/widgets/VfoWidget.cpp
git commit -m "feat: VfoWidget AGC-T auto-mode visuals — green handle, NF fill, AUTO badge"
```

---

### Task 7: RxApplet AGC-T Slider Visual Enhancement

**Files:**
- Modify: `src/gui/applets/RxApplet.h:245-246`
- Modify: `src/gui/applets/RxApplet.cpp:400-441`

Identical treatment to VfoWidget Task 6. Same auto-mode visuals: green handle,
NF fill, AUTO badge, info sub-line, right-click toggle.

- [ ] **Step 1: Add members to RxApplet.h**

Near line 246 (after `m_agcTSlider`):

```cpp
QLabel*  m_agcAutoLabel{nullptr};
QLabel*  m_agcInfoLabel{nullptr};
QWidget* m_agcTContainer{nullptr};
QLabel*  m_agcTLabelWidget{nullptr};
```

- [ ] **Step 2: Modify RxApplet AGC-T row construction**

Apply the same pattern as Task 6 Step 2 to the RxApplet's AGC-T slider
construction (~lines 425-441). Add right-click toggle, AUTO badge, info sub-line.

- [ ] **Step 3: Add updateAgcAutoVisuals method to RxApplet**

Same implementation as VfoWidget's `updateAgcAutoVisuals` from Task 6 Step 3.

- [ ] **Step 4: Build and verify**

Run: `cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

- [ ] **Step 5: Commit**

```bash
git add src/gui/applets/RxApplet.h src/gui/applets/RxApplet.cpp
git commit -m "feat: RxApplet AGC-T auto-mode visuals — matches VfoWidget treatment"
```

---

### Task 8: Wire Visual Updates from RadioModel

**Files:**
- Modify: `src/gui/MainWindow.cpp`

- [ ] **Step 1: Connect auto-AGC state to both widgets**

In MainWindow, wire SliceModel auto-AGC signals to VfoWidget and RxApplet:

```cpp
// Auto AGC visual sync — update both AGC-T slider widgets
connect(slice, &SliceModel::autoAgcEnabledChanged, this, [this](bool on) {
    float nf = m_radioModel->noiseFloorTracker()
             ? m_radioModel->noiseFloorTracker()->noiseFloor() : -200.0f;
    double offset = m_radioModel->activeSlice()->autoAgcOffset();
    m_vfoWidget->updateAgcAutoVisuals(on, nf, offset);
    // Update RxApplet similarly if accessible
});

// Periodic NF visual update (piggyback on auto-AGC timer or separate 500ms timer)
connect(m_radioModel->autoAgcTimer(), &QTimer::timeout, this, [this]() {
    SliceModel* s = m_radioModel->activeSlice();
    if (s && s->autoAgcEnabled() && m_radioModel->noiseFloorTracker()) {
        float nf = m_radioModel->noiseFloorTracker()->noiseFloor();
        m_vfoWidget->updateAgcAutoVisuals(true, nf, s->autoAgcOffset());
    }
});

// Wire right-click toggle from VfoWidget back to SliceModel
connect(m_vfoWidget, &VfoWidget::autoAgcToggled,
        slice, &SliceModel::setAutoAgcEnabled);
```

- [ ] **Step 2: Build and run the app**

Run: `cmake --build build && ./build/NereusSDR`
Expected: App launches, AGC-T slider visible in Audio tab. Right-click toggles
auto mode visuals (green handle, AUTO badge, info line appear/disappear).

- [ ] **Step 3: Commit**

```bash
git add src/gui/MainWindow.cpp
git commit -m "feat: wire auto-AGC visuals to VfoWidget + RxApplet from RadioModel"
```

---

### Task 9: Full Integration Test

- [ ] **Step 1: Run all existing tests**

Run: `cmake --build build && cd build && ctest --output-on-failure`
Expected: All tests PASS, no regressions

- [ ] **Step 2: Manual verification with radio**

1. Connect to radio, verify AGC-T slider works as before (manual mode)
2. Right-click AGC-T slider → verify AUTO badge appears, handle turns green
3. Verify NF info line shows noise floor and offset values
4. Move AGC-T slider manually → verify auto disables (cyan handle returns)
5. Open Setup → DSP → AGC/ALC → verify all controls are enabled and functional
6. Check Auto AGC checkbox in Setup → verify VFO tab slider updates
7. Change AGC mode to Custom → verify Decay and Hang spinners enable

- [ ] **Step 3: Final commit if any fixups needed**

```bash
git add -A && git commit -m "fix: integration fixups for auto AGC-T"
```
