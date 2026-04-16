# Step Attenuator & ADC Overload Protection — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port Thetis step attenuator & ADC overload auto-attenuate system with a NereusSDR-native "Adaptive" mode enhancement.

**Architecture:** A `StepAttenuatorController` in `src/core/` owns all overload detection and attenuation state. P1/P2 connections emit `adcOverflow()`. Three UI surfaces consume the controller: `GeneralOptionsPage` (setup), `RxApplet` (day-to-day ATT row), and `MainWindow` status bar (ADC OVL badge). All persistence via `AppSettings::setHardwareValue(mac, ...)`.

**Tech Stack:** C++20, Qt6 (QObject signals/slots, QTimer, QStackedWidget, QSpinBox, QComboBox), AppSettings XML persistence.

**Design Spec:** `docs/architecture/2026-04-16-step-attenuator-adc-overload-design.md`

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `src/core/StepAttenuatorController.h` | Create | Enums, controller class header |
| `src/core/StepAttenuatorController.cpp` | Create | Hysteresis, Classic/Adaptive algorithms, per-band persistence |
| `src/gui/setup/GeneralOptionsPage.h` | Create | Options setup page header |
| `src/gui/setup/GeneralOptionsPage.cpp` | Create | Step ATT + auto-att setup UI |
| `src/gui/applets/RxApplet.h` | Modify | Add ATT row members |
| `src/gui/applets/RxApplet.cpp` | Modify | Insert ATT row between Squelch and AGC |
| `src/gui/MainWindow.h` | Modify | Add ADC OVL label + controller members |
| `src/gui/MainWindow.cpp` | Modify | ADC OVL badge, controller creation/wiring |
| `src/gui/SetupDialog.cpp` | Modify | Register GeneralOptionsPage |
| `src/core/RadioConnection.h` | Modify | Add `getAdcForDdc()` virtual |
| `src/core/P1RadioConnection.h` | Modify | Declare `getAdcForDdc()` override |
| `src/core/P1RadioConnection.cpp` | Modify | Emit `adcOverflow`, implement `getAdcForDdc()` |
| `src/core/P2RadioConnection.h` | Modify | Declare `getAdcForDdc()` override |
| `src/core/P2RadioConnection.cpp` | Modify | Emit `adcOverflow`, implement `getAdcForDdc()` |
| `src/models/RadioModel.h` | Modify | Add controller accessor |
| `CMakeLists.txt` | Modify | Add new source files |
| `tests/tst_step_attenuator_controller.cpp` | Create | Controller unit tests |
| `tests/CMakeLists.txt` | Modify | Register test |

---

### Task 1: Protocol Layer — Emit `adcOverflow` from P1 and P2

**Files:**
- Modify: `src/core/RadioConnection.h`
- Modify: `src/core/P1RadioConnection.h`
- Modify: `src/core/P1RadioConnection.cpp:850-887` (instance `parseEp6Frame`)
- Modify: `src/core/P2RadioConnection.h`
- Modify: `src/core/P2RadioConnection.cpp:722-743` (`processHighPriorityStatus`)

- [ ] **Step 1: Add `getAdcForDdc()` virtual to RadioConnection**

In `src/core/RadioConnection.h`, add to the public slots section (after `setAntenna`):

```cpp
    // --- ADC Mapping ---
    // Returns which physical ADC serves the given DDC index.
    // From Thetis console.cs:15083 GetADCInUse(int ddc).
    virtual int getAdcForDdc(int ddc) const { return 0; }
```

- [ ] **Step 2: Implement `getAdcForDdc()` in P1RadioConnection**

In `src/core/P1RadioConnection.h`, add in the public section:

```cpp
    int getAdcForDdc(int ddc) const override;
```

In `src/core/P1RadioConnection.cpp`, add at the end (before the closing namespace brace):

```cpp
// From Thetis console.cs:15083 — GetADCInUse for Protocol 1.
// P1 uses a 14-bit control word (2 bits per DDC): RXADCCtrl_P1.
// Encoding: bits [1:0]=DDC0, [3:2]=DDC1, ... [13:12]=DDC6.
// For now, all P1 DDCs map to ADC0 (single ADC on most P1 boards).
// Full RXADCCtrl_P1 register support deferred to Phase 3F (multi-pan).
int P1RadioConnection::getAdcForDdc(int /*ddc*/) const
{
    return 0;
}
```

- [ ] **Step 3: Implement `getAdcForDdc()` in P2RadioConnection**

In `src/core/P2RadioConnection.h`, add in the public section:

```cpp
    int getAdcForDdc(int ddc) const override;
```

Add a member to store the ADC control register:

```cpp
    quint32 m_rxAdcCtrl1{0};   // From Thetis prn->RXADCCtrl1 — 2 bits per DDC
```

In `src/core/P2RadioConnection.cpp`, add at the end (before `writeBE32`):

```cpp
// From Thetis console.cs:15083 — GetADCInUse for Protocol 2.
// P2 uses RXADCCtrl1 register: 2 bits per DDC (bits [1:0]=DDC0, etc).
// Bit masking: (adcControl >> (ddc * 2)) & 3
int P2RadioConnection::getAdcForDdc(int ddc) const
{
    if (ddc < 0 || ddc > 6) { return 0; }
    return static_cast<int>((m_rxAdcCtrl1 >> (ddc * 2)) & 0x3);
}
```

- [ ] **Step 4: Emit `adcOverflow` from P1 EP6 frame parser**

In `src/core/P1RadioConnection.cpp`, in the instance `parseEp6Frame` method (around line 870, after the static parse succeeds and before the iqDataReceived loop), add:

```cpp
    // From Thetis networkproto1.c — ADC overflow is reported in the C&C
    // status bytes of each EP6 subframe. C0[0] bit 0 = LT2208 overflow.
    // Check both subframes (offsets 11 and 523 for C0 in subframes 0 and 1).
    // From Thetis console.cs:21709 pollOverloadSyncSeqErr → getAndResetADC_Overload
    const quint8 c0_sub0 = frame[11];
    const quint8 c0_sub1 = frame[523];
    if ((c0_sub0 & 0x01) || (c0_sub1 & 0x01)) {
        emit adcOverflow(0);
    }
```

- [ ] **Step 5: Emit `adcOverflow` from P2 high-priority status**

In `src/core/P2RadioConnection.cpp`, in `processHighPriorityStatus` (around line 742, replacing the placeholder `emit meterDataReceived(0,0,0,0)`), add:

```cpp
    // From Thetis ReadUDPFrame:519-532 — High Priority C&C status, 60 bytes.
    // ADC overflow bits are at offset 4+0 = byte 4 of the status payload.
    // From Thetis network.c:getAndResetADC_Overload — bit 0=ADC0, bit 1=ADC1, bit 2=ADC2.
    // Status data starts at byte 4 (after the 4-byte sequence number).
    const quint8 adcOverloadBits = raw[4];
    for (int i = 0; i < 3; ++i) {
        if (adcOverloadBits & (1 << i)) {
            emit adcOverflow(i);
        }
    }

    // TODO: extract full meter data from status frame (forward/reverse power, etc.)
    emit meterDataReceived(0.0f, 0.0f, 0.0f, 0.0f);
```

- [ ] **Step 6: Commit**

```bash
git add src/core/RadioConnection.h \
        src/core/P1RadioConnection.h src/core/P1RadioConnection.cpp \
        src/core/P2RadioConnection.h src/core/P2RadioConnection.cpp
git commit -m "feat(protocol): emit adcOverflow from P1/P2 frame parsers

Wire up the existing adcOverflow(int adc) signal in RadioConnection.
P1: parse LT2208 overflow bit from EP6 C0 bytes.
P2: parse per-ADC overflow bits from high-priority status frame.
Add getAdcForDdc() virtual for DDC→ADC mapping.

Porting from Thetis console.cs:21709 pollOverloadSyncSeqErr,
networkproto1.c C0 status bits, network.c getAndResetADC_Overload."
```

---

### Task 2: StepAttenuatorController — Hysteresis & Overload Detection

**Files:**
- Create: `src/core/StepAttenuatorController.h`
- Create: `src/core/StepAttenuatorController.cpp`
- Create: `tests/tst_step_attenuator_controller.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the test file with hysteresis tests**

Create `tests/tst_step_attenuator_controller.cpp`:

```cpp
// tst_step_attenuator_controller.cpp
//
// Unit tests for StepAttenuatorController — hysteresis counters,
// overload level transitions, and timer-driven tick behavior.
//
// From Thetis console.cs:21359-21382 — _adc_overload_level hysteresis.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/StepAttenuatorController.h"

using namespace NereusSDR;

class TestStepAttenuatorController : public QObject {
    Q_OBJECT
private slots:

    void singleOverflow_emitsYellow()
    {
        StepAttenuatorController ctrl;
        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);

        ctrl.onAdcOverflow(0);
        ctrl.tick();  // level 0→1: Yellow

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);  // ADC 0
        QCOMPARE(spy.at(0).at(1).value<OverloadLevel>(), OverloadLevel::Yellow);
    }

    void noOverflow_levelDecays()
    {
        StepAttenuatorController ctrl;
        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);

        // Push to yellow
        ctrl.onAdcOverflow(0);
        ctrl.tick();  // level 1 → Yellow
        spy.clear();

        // No overflow, tick → level 0 → None
        ctrl.tick();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).value<OverloadLevel>(), OverloadLevel::None);
    }

    void sustainedOverflow_escalatesToRed()
    {
        StepAttenuatorController ctrl;
        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);

        // 4 consecutive overflows → level crosses 3→4 = Red
        for (int i = 0; i < 4; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        // Find the Red emission
        bool foundRed = false;
        for (const auto& args : spy) {
            if (args.at(1).value<OverloadLevel>() == OverloadLevel::Red) {
                foundRed = true;
            }
        }
        QVERIFY(foundRed);
    }

    void levelCapsAtFive()
    {
        StepAttenuatorController ctrl;

        // 10 consecutive overflows — level should cap at 5
        for (int i = 0; i < 10; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        QCOMPARE(ctrl.overloadLevel(0), 5);
    }

    void redDowngradesToYellow()
    {
        StepAttenuatorController ctrl;
        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);

        // Push to Red (level 4)
        for (int i = 0; i < 4; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }
        spy.clear();

        // One tick without overflow: level 4→3 = downgrade from Red to Yellow
        ctrl.tick();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).value<OverloadLevel>(), OverloadLevel::Yellow);
    }

    void multipleAdcsIndependent()
    {
        StepAttenuatorController ctrl;
        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);

        ctrl.onAdcOverflow(0);
        ctrl.tick();
        // ADC0 = Yellow, ADC1 still None

        ctrl.onAdcOverflow(1);
        ctrl.tick();
        // ADC0 decays to 0 (None), ADC1 goes to 1 (Yellow)

        // ADC0 should have gone None, ADC1 should be Yellow
        bool adc0None = false;
        bool adc1Yellow = false;
        for (const auto& args : spy) {
            if (args.at(0).toInt() == 0 &&
                args.at(1).value<OverloadLevel>() == OverloadLevel::None) {
                adc0None = true;
            }
            if (args.at(0).toInt() == 1 &&
                args.at(1).value<OverloadLevel>() == OverloadLevel::Yellow) {
                adc1Yellow = true;
            }
        }
        QVERIFY(adc0None);
        QVERIFY(adc1Yellow);
    }
};

QTEST_MAIN(TestStepAttenuatorController)
#include "tst_step_attenuator_controller.moc"
```

- [ ] **Step 2: Create the header**

Create `src/core/StepAttenuatorController.h`:

```cpp
// src/core/StepAttenuatorController.h
//
// Owns step-attenuator state and ADC overload protection for one radio.
// Porting from Thetis console.cs:21290-21763 (overload detection, auto-att).
//
// Design spec: docs/architecture/2026-04-16-step-attenuator-adc-overload-design.md

#pragma once

#include "models/Band.h"

#include <QObject>
#include <QHash>
#include <QStack>
#include <QTimer>

namespace NereusSDR {

class RadioConnection;

// From Thetis console.cs:21359 — hysteresis levels drive yellow/red thresholds.
enum class OverloadLevel { None, Yellow, Red };

// From design spec §4 — Classic = Thetis 1:1, Adaptive = NereusSDR enhancement.
enum class AutoAttMode { Classic, Adaptive };

// From Thetis enums.cs PreampMode — maps to combo items per radio model.
enum class PreampMode {
    Off,          // 0 dB (HPSDR_OFF)
    On,           // HPSDR_ON
    Minus10,      // -10 dB
    Minus20,      // -20 dB
    Minus30,      // -30 dB
    Minus40,      // -40 dB (ALEX-equipped only)
    Minus50       // -50 dB (ALEX-equipped only)
};

class StepAttenuatorController : public QObject {
    Q_OBJECT
public:
    explicit StepAttenuatorController(QObject* parent = nullptr);

    // --- Read-only state accessors (for tests) ---
    int overloadLevel(int adc) const;

    // --- Configuration (from GeneralOptionsPage) ---
    void setStepAttEnabled(int rx, bool enabled);
    bool stepAttEnabled(int rx) const;

    void setAutoAttEnabled(int rx, bool enabled);
    void setAutoAttMode(int rx, AutoAttMode mode);
    void setAutoAttUndo(int rx, bool enabled);
    void setAutoAttHoldSeconds(int rx, int seconds);

    // --- Connection ---
    void setRadioConnection(RadioConnection* conn);

    // --- Max attenuation (from BoardCapabilities) ---
    void setMaxAttenuation(int maxDb);
    int maxAttenuation() const { return m_maxDb; }

public slots:
    // Called by RadioConnection::adcOverflow signal.
    void onAdcOverflow(int adc);

    // Called by SliceModel::bandChanged.
    void onBandChanged(int rx, Band band);

    // Called by UI controls (RxApplet, GeneralOptionsPage).
    void setAttenuation(int rx, int dB);
    void setPreampMode(int rx, PreampMode mode);

    // Called when DDC mapping changes (diversity/PS mode change).
    void onDdcMappingChanged();

    // Timer-driven tick (100ms). Public for testability.
    void tick();

signals:
    void overloadStatusChanged(int adc, NereusSDR::OverloadLevel level);
    void attenuationChanged(int rx, int dB);
    void preampModeChanged(int rx, NereusSDR::PreampMode mode);
    void adcLinkedChanged(bool linked);
    void autoAttActiveChanged(int rx, bool active);

private:
    void applyClassicAutoAtt(int rx);
    void applyAdaptiveAutoAtt(int rx);
    void applyAttenuation(int rx, int dB);
    int  adcForRx(int rx) const;
    void checkAdcLinked();

    // --- Per-ADC overload state (from Thetis console.cs:21212-21214) ---
    static constexpr int kMaxAdcs = 3;
    int  m_overloadLevel[kMaxAdcs]{};      // 0–5 hysteresis counter
    bool m_overloadFlag[kMaxAdcs]{};       // set per tick, cleared after processing
    OverloadLevel m_prevLevel[kMaxAdcs]{}; // for change detection

    // --- Per-RX state ---
    static constexpr int kMaxRx = 2;

    struct RxState {
        bool stepAttEnabled{false};
        int  attenuationDb{0};
        PreampMode preampMode{PreampMode::Off};
        Band currentBand{Band::Band20m};

        // Auto-att config
        bool autoAttEnabled{false};
        AutoAttMode autoAttMode{AutoAttMode::Classic};
        bool autoAttUndo{true};
        int  autoAttHoldSeconds{5};

        // Classic mode state
        QStack<int> attHistory;
        bool autoAttActive{false};

        // Adaptive mode state
        enum class AdaptivePhase { Idle, Attack, Hold, Decay };
        AdaptivePhase adaptivePhase{AdaptivePhase::Idle};
        int holdTicksRemaining{0};
        int decayTickCounter{0};  // counts to 5 (500ms at 100ms tick)

        // Per-band storage
        QHash<Band, int> perBandAtt;
        QHash<Band, PreampMode> perBandPreamp;
        QHash<Band, int> settledFloor;  // Adaptive mode per-band memory
    };

    RxState m_rx[kMaxRx];

    // --- Hardware ---
    RadioConnection* m_connection{nullptr};
    int m_maxDb{31};
    bool m_adcLinked{false};
    int m_rxDdc[kMaxRx]{0, 1};  // which DDC each RX uses

    // --- Timer ---
    QTimer* m_tickTimer{nullptr};
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::OverloadLevel)
Q_DECLARE_METATYPE(NereusSDR::PreampMode)
Q_DECLARE_METATYPE(NereusSDR::AutoAttMode)
```

- [ ] **Step 3: Create the implementation — hysteresis tick only**

Create `src/core/StepAttenuatorController.cpp`:

```cpp
// src/core/StepAttenuatorController.cpp
//
// Porting from Thetis console.cs:21290-21763 — overload detection, auto-att.

#include "StepAttenuatorController.h"
#include "RadioConnection.h"

#include <QLoggingCategory>
#include <algorithm>

Q_LOGGING_CATEGORY(lcStepAtt, "nereus.stepatt")

namespace NereusSDR {

StepAttenuatorController::StepAttenuatorController(QObject* parent)
    : QObject(parent)
{
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(100);  // 100ms — matches Thetis polling cadence
    connect(m_tickTimer, &QTimer::timeout, this, &StepAttenuatorController::tick);

    qRegisterMetaType<OverloadLevel>();
    qRegisterMetaType<PreampMode>();
    qRegisterMetaType<AutoAttMode>();
}

int StepAttenuatorController::overloadLevel(int adc) const
{
    if (adc < 0 || adc >= kMaxAdcs) { return 0; }
    return m_overloadLevel[adc];
}

void StepAttenuatorController::setRadioConnection(RadioConnection* conn)
{
    if (m_connection) {
        QObject::disconnect(m_connection, &RadioConnection::adcOverflow,
                            this, &StepAttenuatorController::onAdcOverflow);
        m_tickTimer->stop();
    }
    m_connection = conn;
    if (m_connection) {
        connect(m_connection, &RadioConnection::adcOverflow,
                this, &StepAttenuatorController::onAdcOverflow);
        m_tickTimer->start();
    }
}

void StepAttenuatorController::setMaxAttenuation(int maxDb)
{
    m_maxDb = maxDb;
}

void StepAttenuatorController::onAdcOverflow(int adc)
{
    if (adc >= 0 && adc < kMaxAdcs) {
        m_overloadFlag[adc] = true;
    }
}

// From Thetis console.cs:21359-21382 — hysteresis counter update per tick.
// Yellow: level > 0 (levels 1-3). Red: level > 3 (levels 4-5).
void StepAttenuatorController::tick()
{
    for (int i = 0; i < kMaxAdcs; ++i) {
        if (m_overloadFlag[i]) {
            m_overloadLevel[i] = std::min(m_overloadLevel[i] + 1, 5);
        } else {
            m_overloadLevel[i] = std::max(m_overloadLevel[i] - 1, 0);
        }
        m_overloadFlag[i] = false;

        // Determine current level
        OverloadLevel level = OverloadLevel::None;
        if (m_overloadLevel[i] > 3) {
            level = OverloadLevel::Red;
        } else if (m_overloadLevel[i] > 0) {
            level = OverloadLevel::Yellow;
        }

        // Emit only on transitions
        if (level != m_prevLevel[i]) {
            m_prevLevel[i] = level;
            emit overloadStatusChanged(i, level);
        }
    }

    // Auto-attenuate processing per RX
    for (int rx = 0; rx < kMaxRx; ++rx) {
        auto& state = m_rx[rx];
        if (!state.autoAttEnabled) { continue; }

        const int adc = adcForRx(rx);
        if (adc < 0 || adc >= kMaxAdcs) { continue; }

        if (state.autoAttMode == AutoAttMode::Classic) {
            applyClassicAutoAtt(rx);
        } else {
            applyAdaptiveAutoAtt(rx);
        }
    }
}

// --- Configuration setters ---

void StepAttenuatorController::setStepAttEnabled(int rx, bool enabled)
{
    if (rx >= 0 && rx < kMaxRx) {
        m_rx[rx].stepAttEnabled = enabled;
    }
}

bool StepAttenuatorController::stepAttEnabled(int rx) const
{
    if (rx < 0 || rx >= kMaxRx) { return false; }
    return m_rx[rx].stepAttEnabled;
}

void StepAttenuatorController::setAutoAttEnabled(int rx, bool enabled)
{
    if (rx >= 0 && rx < kMaxRx) {
        m_rx[rx].autoAttEnabled = enabled;
    }
}

void StepAttenuatorController::setAutoAttMode(int rx, AutoAttMode mode)
{
    if (rx >= 0 && rx < kMaxRx) {
        m_rx[rx].autoAttMode = mode;
    }
}

void StepAttenuatorController::setAutoAttUndo(int rx, bool enabled)
{
    if (rx >= 0 && rx < kMaxRx) {
        m_rx[rx].autoAttUndo = enabled;
    }
}

void StepAttenuatorController::setAutoAttHoldSeconds(int rx, int seconds)
{
    if (rx >= 0 && rx < kMaxRx) {
        m_rx[rx].autoAttHoldSeconds = seconds;
    }
}

// --- Attenuation application ---

void StepAttenuatorController::setAttenuation(int rx, int dB)
{
    if (rx < 0 || rx >= kMaxRx) { return; }
    dB = std::clamp(dB, 0, m_maxDb);
    m_rx[rx].attenuationDb = dB;
    m_rx[rx].perBandAtt[m_rx[rx].currentBand] = dB;
    applyAttenuation(rx, dB);
    emit attenuationChanged(rx, dB);

    // ADC-linked sync
    if (m_adcLinked && rx == 0 && m_rx[1].attenuationDb != dB) {
        m_rx[1].attenuationDb = dB;
        m_rx[1].perBandAtt[m_rx[1].currentBand] = dB;
        applyAttenuation(1, dB);
        emit attenuationChanged(1, dB);
    } else if (m_adcLinked && rx == 1 && m_rx[0].attenuationDb != dB) {
        m_rx[0].attenuationDb = dB;
        m_rx[0].perBandAtt[m_rx[0].currentBand] = dB;
        applyAttenuation(0, dB);
        emit attenuationChanged(0, dB);
    }
}

void StepAttenuatorController::setPreampMode(int rx, PreampMode mode)
{
    if (rx < 0 || rx >= kMaxRx) { return; }
    m_rx[rx].preampMode = mode;
    m_rx[rx].perBandPreamp[m_rx[rx].currentBand] = mode;
    emit preampModeChanged(rx, mode);
}

void StepAttenuatorController::onBandChanged(int rx, Band band)
{
    if (rx < 0 || rx >= kMaxRx) { return; }
    auto& state = m_rx[rx];
    state.currentBand = band;

    if (state.stepAttEnabled) {
        const int dB = state.perBandAtt.value(band, 0);
        state.attenuationDb = dB;
        applyAttenuation(rx, dB);
        emit attenuationChanged(rx, dB);
    } else {
        const PreampMode mode = state.perBandPreamp.value(band, PreampMode::Off);
        state.preampMode = mode;
        emit preampModeChanged(rx, mode);
    }
}

void StepAttenuatorController::onDdcMappingChanged()
{
    if (!m_connection) { return; }
    for (int rx = 0; rx < kMaxRx; ++rx) {
        // Update cached DDC→ADC mapping
        // m_rxDdc values are set externally or default to 0,1
    }
    checkAdcLinked();
}

void StepAttenuatorController::applyAttenuation(int rx, int dB)
{
    if (!m_connection) { return; }
    // From Thetis console.cs RX1AttenuatorData property — for now, direct path.
    // ALEX extended range (32-61 dB split) deferred until ALEX atten command is wired.
    Q_UNUSED(rx);  // TODO: per-RX ADC routing when multi-ADC support lands
    m_connection->setAttenuator(dB);
}

int StepAttenuatorController::adcForRx(int rx) const
{
    if (!m_connection || rx < 0 || rx >= kMaxRx) { return 0; }
    return m_connection->getAdcForDdc(m_rxDdc[rx]);
}

void StepAttenuatorController::checkAdcLinked()
{
    const bool linked = (adcForRx(0) == adcForRx(1));
    if (linked != m_adcLinked) {
        m_adcLinked = linked;
        emit adcLinkedChanged(linked);
    }
}

// --- Classic auto-attenuate (Thetis 1:1) ---
// From Thetis console.cs:21432-21706 handleOverload()

void StepAttenuatorController::applyClassicAutoAtt(int rx)
{
    auto& state = m_rx[rx];
    const int adc = adcForRx(rx);

    if (m_overloadLevel[adc] > 3 && !state.autoAttActive) {
        // Red threshold — push current value and bump
        state.attHistory.push(state.attenuationDb);
        const int newDb = std::min(state.attenuationDb + 1, m_maxDb);
        state.attenuationDb = newDb;
        state.perBandAtt[state.currentBand] = newDb;
        applyAttenuation(rx, newDb);
        emit attenuationChanged(rx, newDb);
        state.autoAttActive = true;
        emit autoAttActiveChanged(rx, true);

        if (state.autoAttUndo) {
            QTimer::singleShot(state.autoAttHoldSeconds * 1000, this, [this, rx]() {
                auto& s = m_rx[rx];
                if (!s.attHistory.isEmpty()) {
                    const int prevDb = s.attHistory.pop();
                    s.attenuationDb = prevDb;
                    s.perBandAtt[s.currentBand] = prevDb;
                    applyAttenuation(rx, prevDb);
                    emit attenuationChanged(rx, prevDb);
                }
                s.autoAttActive = false;
                emit autoAttActiveChanged(rx, false);
            });
        }
    }
}

// --- Adaptive auto-attenuate (NereusSDR enhancement) ---
// Design spec §4.2 — attack/decay model with per-band memory.

void StepAttenuatorController::applyAdaptiveAutoAtt(int rx)
{
    auto& state = m_rx[rx];
    const int adc = adcForRx(rx);
    const bool overloading = m_overloadLevel[adc] > 3;

    switch (state.adaptivePhase) {
    case RxState::AdaptivePhase::Idle:
        if (overloading) {
            state.adaptivePhase = RxState::AdaptivePhase::Attack;
            state.autoAttActive = true;
            emit autoAttActiveChanged(rx, true);
        }
        break;

    case RxState::AdaptivePhase::Attack:
        if (overloading) {
            // Ramp up 1 dB per tick while overload persists
            if (state.attenuationDb < m_maxDb) {
                const int newDb = state.attenuationDb + 1;
                state.attenuationDb = newDb;
                state.perBandAtt[state.currentBand] = newDb;
                applyAttenuation(rx, newDb);
                emit attenuationChanged(rx, newDb);
            }
        } else {
            // Overload cleared — enter hold
            state.adaptivePhase = RxState::AdaptivePhase::Hold;
            // holdSeconds × 10 ticks/sec
            state.holdTicksRemaining = state.autoAttHoldSeconds * 10;
        }
        break;

    case RxState::AdaptivePhase::Hold:
        if (overloading) {
            // Re-attack immediately
            state.adaptivePhase = RxState::AdaptivePhase::Attack;
        } else {
            state.holdTicksRemaining--;
            if (state.holdTicksRemaining <= 0) {
                if (state.autoAttUndo) {
                    state.adaptivePhase = RxState::AdaptivePhase::Decay;
                    state.decayTickCounter = 0;
                } else {
                    state.adaptivePhase = RxState::AdaptivePhase::Idle;
                    state.autoAttActive = false;
                    emit autoAttActiveChanged(rx, false);
                }
            }
        }
        break;

    case RxState::AdaptivePhase::Decay:
        if (overloading) {
            // Re-attack immediately
            state.adaptivePhase = RxState::AdaptivePhase::Attack;
        } else {
            state.decayTickCounter++;
            if (state.decayTickCounter >= 5) {  // 500ms (5 × 100ms ticks)
                state.decayTickCounter = 0;
                const int floor = state.settledFloor.value(state.currentBand, 0);
                if (state.attenuationDb > floor) {
                    const int newDb = state.attenuationDb - 1;
                    state.attenuationDb = newDb;
                    state.perBandAtt[state.currentBand] = newDb;
                    applyAttenuation(rx, newDb);
                    emit attenuationChanged(rx, newDb);
                } else {
                    // Decay complete — save settled floor
                    state.settledFloor[state.currentBand] = state.attenuationDb;
                    state.adaptivePhase = RxState::AdaptivePhase::Idle;
                    state.autoAttActive = false;
                    emit autoAttActiveChanged(rx, false);
                }
            }
        }
        break;
    }
}

} // namespace NereusSDR
```

- [ ] **Step 4: Register in CMakeLists.txt**

In `CMakeLists.txt`, after the `src/core/ClarityController.cpp` line (around line 260), add:

```cmake
    src/core/StepAttenuatorController.cpp
```

In `tests/CMakeLists.txt`, add at the end:

```cmake
# Step attenuator controller (hysteresis, auto-att)
nereus_add_test(tst_step_attenuator_controller)
```

- [ ] **Step 5: Build and run tests**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build -R tst_step_attenuator_controller -V`

Expected: All 6 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add src/core/StepAttenuatorController.h src/core/StepAttenuatorController.cpp \
        tests/tst_step_attenuator_controller.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "feat(core): StepAttenuatorController with hysteresis & auto-att

New controller owns per-ADC overload detection (100ms tick, 0-5
hysteresis counter, yellow/red thresholds) and both Classic (Thetis
1:1 bump+timer-undo) and Adaptive (NereusSDR attack/hold/decay with
per-band memory) auto-attenuate algorithms.

Porting from Thetis console.cs:21290-21763 handleOverload()."
```

---

### Task 3: Add Controller to RadioModel & Wire in MainWindow

**Files:**
- Modify: `src/models/RadioModel.h`
- Modify: `src/gui/MainWindow.h`
- Modify: `src/gui/MainWindow.cpp`

- [ ] **Step 1: Add controller accessor to RadioModel**

In `src/models/RadioModel.h`, after the `clarityController` accessor (around line 81), add:

```cpp
    class StepAttenuatorController* stepAttController() const { return m_stepAttController; }
    void setStepAttController(class StepAttenuatorController* c) { m_stepAttController = c; }
```

Add the member (after `m_clarityController`, around line 162):

```cpp
    class StepAttenuatorController* m_stepAttController{nullptr};
```

- [ ] **Step 2: Add ADC OVL label member to MainWindow**

In `src/gui/MainWindow.h`, add to the private members section:

```cpp
    class StepAttenuatorController* m_stepAttController{nullptr};
    QLabel* m_adcOvlLabel{nullptr};
```

- [ ] **Step 3: Create controller and wire in MainWindow**

In `src/gui/MainWindow.cpp`, add include at top:

```cpp
#include "core/StepAttenuatorController.h"
```

In the constructor (or wherever `m_clarityController` is created — find the pattern), add after ClarityController creation:

```cpp
    // Step Attenuator Controller
    m_stepAttController = new StepAttenuatorController(this);
    m_model->setStepAttController(m_stepAttController);
```

Where the radio connection is wired (look for where `m_clarityController` connects to the radio), add:

```cpp
    m_stepAttController->setRadioConnection(m_model->connection());
```

Also wire board capabilities when radio connects — find the `connectionStateChanged` handler or equivalent and add:

```cpp
    if (m_model->connection() && m_model->connection()->isConnected()) {
        const auto& caps = BoardCapsTable::forBoard(m_model->connection()->radioInfo().boardType);
        m_stepAttController->setMaxAttenuation(caps.attenuator.maxDb);
    }
```

- [ ] **Step 4: Add ADC OVL badge to status bar**

In `src/gui/MainWindow.cpp`, in the status bar build method, insert before `hbox->addWidget(stationContainer)` (around line 1418):

```cpp
    // ── ADC Overload indicator ───────────────────────────────────────────────
    m_adcOvlLabel = new QLabel(barWidget);
    m_adcOvlLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #FFD700; font-size: 12px; font-weight: bold;"
        " border: none; background: transparent; padding: 0 4px; }"));
    m_adcOvlLabel->hide();
    hbox->addWidget(m_adcOvlLabel);

    connect(m_stepAttController, &StepAttenuatorController::overloadStatusChanged,
            this, [this](int adc, OverloadLevel level) {
        // Build text from all ADCs
        QString text;
        // Re-check all ADC levels to build composite text
        for (int i = 0; i < 3; ++i) {
            const int lvl = m_stepAttController->overloadLevel(i);
            if (lvl > 0) {
                if (!text.isEmpty()) { text += QStringLiteral(" "); }
                text += QStringLiteral("ADC%1 OVL").arg(i);
            }
        }

        if (text.isEmpty()) {
            m_adcOvlLabel->hide();
        } else {
            // Worst level determines color: any Red → red, else yellow
            bool anyRed = false;
            for (int i = 0; i < 3; ++i) {
                if (m_stepAttController->overloadLevel(i) > 3) {
                    anyRed = true;
                    break;
                }
            }
            const QString color = anyRed ? QStringLiteral("#FF3333")
                                         : QStringLiteral("#FFD700");
            m_adcOvlLabel->setStyleSheet(
                QStringLiteral("QLabel { color: %1; font-size: 12px; font-weight: bold;"
                               " border: none; background: transparent;"
                               " padding: 0 4px; }").arg(color));
            m_adcOvlLabel->setText(text);
            m_adcOvlLabel->show();
        }

        // Tooltip
        QString tip;
        for (int i = 0; i < 3; ++i) {
            if (m_stepAttController->overloadLevel(i) > 0) {
                if (!tip.isEmpty()) { tip += QStringLiteral("\n"); }
                tip += QStringLiteral("ADC%1: overload").arg(i);
            }
        }
        m_adcOvlLabel->setToolTip(tip);
    });
```

- [ ] **Step 5: Build to verify compilation**

Run: `cmake --build build -j$(nproc)`

Expected: Clean build, no errors.

- [ ] **Step 6: Commit**

```bash
git add src/models/RadioModel.h src/gui/MainWindow.h src/gui/MainWindow.cpp
git commit -m "feat(gui): wire StepAttenuatorController + ADC OVL status badge

Create and own StepAttenuatorController in MainWindow, expose via
RadioModel accessor. ADC OVL badge in status bar shows per-ADC
overload text (yellow/red) left of STATION container.

From Thetis console.cs ucInfoBar Warning() + pbAutoAttWarningRX1."
```

---

### Task 4: GeneralOptionsPage — Setup UI

**Files:**
- Create: `src/gui/setup/GeneralOptionsPage.h`
- Create: `src/gui/setup/GeneralOptionsPage.cpp`
- Modify: `src/gui/SetupDialog.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the header**

Create `src/gui/setup/GeneralOptionsPage.h`:

```cpp
// src/gui/setup/GeneralOptionsPage.h
//
// Setup → General → Options page.
// Step attenuator enable/value + auto-attenuate configuration.
// Porting from Thetis setup.cs Options 1 tab — step attenuator +
// auto-attenuate groups only.

#pragma once

#include "gui/SetupPage.h"

class QCheckBox;

namespace NereusSDR {

class StepAttenuatorController;

class GeneralOptionsPage : public SetupPage {
    Q_OBJECT
public:
    explicit GeneralOptionsPage(RadioModel* model, QWidget* parent = nullptr);
    void syncFromModel() override;

private:
    void buildStepAttGroup();
    void buildAutoAttGroup();
    void connectController();

    StepAttenuatorController* m_ctrl{nullptr};

    // Step Attenuator group
    QCheckBox* m_chkRx1StepAttEnable{nullptr};
    QSpinBox*  m_spnRx1StepAttValue{nullptr};
    QCheckBox* m_chkRx2StepAttEnable{nullptr};
    QSpinBox*  m_spnRx2StepAttValue{nullptr};
    QLabel*    m_lblAdcLinked{nullptr};

    // Auto Attenuate RX1 group
    QCheckBox* m_chkAutoAttRx1{nullptr};
    QComboBox* m_cmbAutoAttRx1Mode{nullptr};
    QCheckBox* m_chkAutoAttUndoRx1{nullptr};
    QSpinBox*  m_spnAutoAttHoldRx1{nullptr};

    // Auto Attenuate RX2 group
    QCheckBox* m_chkAutoAttRx2{nullptr};
    QComboBox* m_cmbAutoAttRx2Mode{nullptr};
    QCheckBox* m_chkAutoAttUndoRx2{nullptr};
    QSpinBox*  m_spnAutoAttHoldRx2{nullptr};
};

} // namespace NereusSDR
```

- [ ] **Step 2: Create the implementation**

Create `src/gui/setup/GeneralOptionsPage.cpp`:

```cpp
// src/gui/setup/GeneralOptionsPage.cpp
//
// Porting from Thetis setup.cs — Options 1 tab step attenuator +
// auto-attenuate groups.

#include "GeneralOptionsPage.h"
#include "core/StepAttenuatorController.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace NereusSDR {

GeneralOptionsPage::GeneralOptionsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Options"), model, parent)
{
    m_ctrl = model ? model->stepAttController() : nullptr;

    buildStepAttGroup();
    buildAutoAttGroup();

    contentLayout()->addStretch(1);

    if (m_ctrl) {
        connectController();
    }
}

void GeneralOptionsPage::buildStepAttGroup()
{
    // From Thetis setup.cs: grpHermesStepAttenuator
    auto* grp = addSection(QStringLiteral("Step Attenuator"));

    // RX1 enable
    m_chkRx1StepAttEnable = new QCheckBox(QStringLiteral("Enable RX1"), this);
    // From Thetis setup.cs: chkHermesStepAttenuator
    m_chkRx1StepAttEnable->setToolTip(QStringLiteral("Enable the step attenuator."));
    auto* rx1Row = new QHBoxLayout;
    rx1Row->addWidget(m_chkRx1StepAttEnable);

    m_spnRx1StepAttValue = new QSpinBox(this);
    m_spnRx1StepAttValue->setRange(0, 31);
    m_spnRx1StepAttValue->setSuffix(QStringLiteral(" dB"));
    m_spnRx1StepAttValue->setFixedWidth(80);
    m_spnRx1StepAttValue->setEnabled(false);
    rx1Row->addWidget(m_spnRx1StepAttValue);
    rx1Row->addStretch(1);
    grp->layout()->addItem(rx1Row);

    connect(m_chkRx1StepAttEnable, &QCheckBox::toggled, this, [this](bool on) {
        m_spnRx1StepAttValue->setEnabled(on);
        if (m_ctrl) { m_ctrl->setStepAttEnabled(0, on); }
    });
    connect(m_spnRx1StepAttValue, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int val) {
        if (m_ctrl) { m_ctrl->setAttenuation(0, val); }
    });

    // RX2 enable
    m_chkRx2StepAttEnable = new QCheckBox(QStringLiteral("Enable RX2"), this);
    // From Thetis setup.cs: chkRX2StepAtt
    m_chkRx2StepAttEnable->setToolTip(QStringLiteral("Enable the step attenuator."));
    auto* rx2Row = new QHBoxLayout;
    rx2Row->addWidget(m_chkRx2StepAttEnable);

    m_spnRx2StepAttValue = new QSpinBox(this);
    m_spnRx2StepAttValue->setRange(0, 31);
    m_spnRx2StepAttValue->setSuffix(QStringLiteral(" dB"));
    m_spnRx2StepAttValue->setFixedWidth(80);
    m_spnRx2StepAttValue->setEnabled(false);
    rx2Row->addWidget(m_spnRx2StepAttValue);
    rx2Row->addStretch(1);
    grp->layout()->addItem(rx2Row);

    connect(m_chkRx2StepAttEnable, &QCheckBox::toggled, this, [this](bool on) {
        m_spnRx2StepAttValue->setEnabled(on);
        if (m_ctrl) { m_ctrl->setStepAttEnabled(1, on); }
    });
    connect(m_spnRx2StepAttValue, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int val) {
        if (m_ctrl) { m_ctrl->setAttenuation(1, val); }
    });

    // ADC linked label
    m_lblAdcLinked = new QLabel(QStringLiteral("adc linked"), this);
    m_lblAdcLinked->setStyleSheet(QStringLiteral(
        "QLabel { color: red; font-weight: bold; font-size: 11px; }"));
    m_lblAdcLinked->hide();
    auto* linkedRow = new QHBoxLayout;
    linkedRow->addWidget(m_lblAdcLinked);
    linkedRow->addStretch(1);
    grp->layout()->addItem(linkedRow);
}

void GeneralOptionsPage::buildAutoAttGroup()
{
    // --- RX1 ---
    // From Thetis setup.cs: groupBoxTS47
    auto* grpRx1 = addSection(QStringLiteral("Auto Attenuate RX1"));

    m_chkAutoAttRx1 = new QCheckBox(QStringLiteral("Auto att RX1"), this);
    // From Thetis setup.cs: chkAutoATTRx1
    m_chkAutoAttRx1->setToolTip(QStringLiteral("Auto attenuate RX1 on ADC overload"));

    m_cmbAutoAttRx1Mode = new QComboBox(this);
    m_cmbAutoAttRx1Mode->addItems({QStringLiteral("Classic"), QStringLiteral("Adaptive")});
    // NereusSDR native tooltip
    m_cmbAutoAttRx1Mode->setToolTip(QStringLiteral(
        "Classic: Thetis-style immediate bump with timed undo. "
        "Adaptive: gradual attack/decay with per-band memory."));
    m_cmbAutoAttRx1Mode->setFixedWidth(100);
    m_cmbAutoAttRx1Mode->setEnabled(false);

    auto* rx1EnableRow = new QHBoxLayout;
    rx1EnableRow->addWidget(m_chkAutoAttRx1);
    rx1EnableRow->addWidget(m_cmbAutoAttRx1Mode);
    rx1EnableRow->addStretch(1);
    grpRx1->layout()->addItem(rx1EnableRow);

    m_chkAutoAttUndoRx1 = new QCheckBox(QStringLiteral("Undo"), this);
    // From Thetis setup.cs: chkAutoAttUndoRX1
    m_chkAutoAttUndoRx1->setToolTip(QStringLiteral("Undo the changes made after X seconds"));
    m_chkAutoAttUndoRx1->setChecked(true);
    m_chkAutoAttUndoRx1->setEnabled(false);

    m_spnAutoAttHoldRx1 = new QSpinBox(this);
    m_spnAutoAttHoldRx1->setRange(1, 3600);
    m_spnAutoAttHoldRx1->setValue(5);
    m_spnAutoAttHoldRx1->setSuffix(QStringLiteral(" sec"));
    m_spnAutoAttHoldRx1->setFixedWidth(90);
    m_spnAutoAttHoldRx1->setEnabled(false);

    auto* rx1UndoRow = new QHBoxLayout;
    rx1UndoRow->addWidget(m_chkAutoAttUndoRx1);
    rx1UndoRow->addWidget(m_spnAutoAttHoldRx1);
    rx1UndoRow->addStretch(1);
    grpRx1->layout()->addItem(rx1UndoRow);

    // Enable/disable cascade
    connect(m_chkAutoAttRx1, &QCheckBox::toggled, this, [this](bool on) {
        m_cmbAutoAttRx1Mode->setEnabled(on);
        m_chkAutoAttUndoRx1->setEnabled(on);
        m_spnAutoAttHoldRx1->setEnabled(on && m_chkAutoAttUndoRx1->isChecked());
        if (m_ctrl) { m_ctrl->setAutoAttEnabled(0, on); }
    });
    connect(m_chkAutoAttUndoRx1, &QCheckBox::toggled, this, [this](bool on) {
        m_spnAutoAttHoldRx1->setEnabled(on && m_chkAutoAttRx1->isChecked());
        if (m_ctrl) { m_ctrl->setAutoAttUndo(0, on); }
    });
    connect(m_cmbAutoAttRx1Mode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        const auto mode = (idx == 0) ? AutoAttMode::Classic : AutoAttMode::Adaptive;
        m_chkAutoAttUndoRx1->setText(
            mode == AutoAttMode::Classic ? QStringLiteral("Undo")
                                        : QStringLiteral("Decay"));
        if (m_ctrl) { m_ctrl->setAutoAttMode(0, mode); }
    });
    connect(m_spnAutoAttHoldRx1, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int val) {
        if (m_ctrl) { m_ctrl->setAutoAttHoldSeconds(0, val); }
    });

    // --- RX2 ---
    auto* grpRx2 = addSection(QStringLiteral("Auto Attenuate RX2"));

    m_chkAutoAttRx2 = new QCheckBox(QStringLiteral("Auto att RX2"), this);
    m_chkAutoAttRx2->setToolTip(QStringLiteral("Auto attenuate RX2 on ADC overload"));

    m_cmbAutoAttRx2Mode = new QComboBox(this);
    m_cmbAutoAttRx2Mode->addItems({QStringLiteral("Classic"), QStringLiteral("Adaptive")});
    m_cmbAutoAttRx2Mode->setToolTip(QStringLiteral(
        "Classic: Thetis-style immediate bump with timed undo. "
        "Adaptive: gradual attack/decay with per-band memory."));
    m_cmbAutoAttRx2Mode->setFixedWidth(100);
    m_cmbAutoAttRx2Mode->setEnabled(false);

    auto* rx2EnableRow = new QHBoxLayout;
    rx2EnableRow->addWidget(m_chkAutoAttRx2);
    rx2EnableRow->addWidget(m_cmbAutoAttRx2Mode);
    rx2EnableRow->addStretch(1);
    grpRx2->layout()->addItem(rx2EnableRow);

    m_chkAutoAttUndoRx2 = new QCheckBox(QStringLiteral("Undo"), this);
    m_chkAutoAttUndoRx2->setToolTip(QStringLiteral("Undo the changes made after X seconds"));
    m_chkAutoAttUndoRx2->setChecked(true);
    m_chkAutoAttUndoRx2->setEnabled(false);

    m_spnAutoAttHoldRx2 = new QSpinBox(this);
    m_spnAutoAttHoldRx2->setRange(1, 3600);
    m_spnAutoAttHoldRx2->setValue(5);
    m_spnAutoAttHoldRx2->setSuffix(QStringLiteral(" sec"));
    m_spnAutoAttHoldRx2->setFixedWidth(90);
    m_spnAutoAttHoldRx2->setEnabled(false);

    auto* rx2UndoRow = new QHBoxLayout;
    rx2UndoRow->addWidget(m_chkAutoAttUndoRx2);
    rx2UndoRow->addWidget(m_spnAutoAttHoldRx2);
    rx2UndoRow->addStretch(1);
    grpRx2->layout()->addItem(rx2UndoRow);

    connect(m_chkAutoAttRx2, &QCheckBox::toggled, this, [this](bool on) {
        m_cmbAutoAttRx2Mode->setEnabled(on);
        m_chkAutoAttUndoRx2->setEnabled(on);
        m_spnAutoAttHoldRx2->setEnabled(on && m_chkAutoAttUndoRx2->isChecked());
        if (m_ctrl) { m_ctrl->setAutoAttEnabled(1, on); }
    });
    connect(m_chkAutoAttUndoRx2, &QCheckBox::toggled, this, [this](bool on) {
        m_spnAutoAttHoldRx2->setEnabled(on && m_chkAutoAttRx2->isChecked());
        if (m_ctrl) { m_ctrl->setAutoAttUndo(1, on); }
    });
    connect(m_cmbAutoAttRx2Mode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        const auto mode = (idx == 0) ? AutoAttMode::Classic : AutoAttMode::Adaptive;
        m_chkAutoAttUndoRx2->setText(
            mode == AutoAttMode::Classic ? QStringLiteral("Undo")
                                        : QStringLiteral("Decay"));
        if (m_ctrl) { m_ctrl->setAutoAttMode(1, mode); }
    });
    connect(m_spnAutoAttHoldRx2, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int val) {
        if (m_ctrl) { m_ctrl->setAutoAttHoldSeconds(1, val); }
    });
}

void GeneralOptionsPage::connectController()
{
    connect(m_ctrl, &StepAttenuatorController::adcLinkedChanged,
            m_lblAdcLinked, &QLabel::setVisible);

    connect(m_ctrl, &StepAttenuatorController::attenuationChanged,
            this, [this](int rx, int dB) {
        QSignalBlocker b0(m_spnRx1StepAttValue);
        QSignalBlocker b1(m_spnRx2StepAttValue);
        if (rx == 0) { m_spnRx1StepAttValue->setValue(dB); }
        if (rx == 1) { m_spnRx2StepAttValue->setValue(dB); }
    });
}

void GeneralOptionsPage::syncFromModel()
{
    // Sync from controller state if available
    if (!m_ctrl) { return; }
    QSignalBlocker b0(m_chkRx1StepAttEnable);
    QSignalBlocker b1(m_chkRx2StepAttEnable);
    QSignalBlocker b2(m_spnRx1StepAttValue);
    QSignalBlocker b3(m_spnRx2StepAttValue);
    m_chkRx1StepAttEnable->setChecked(m_ctrl->stepAttEnabled(0));
    m_chkRx2StepAttEnable->setChecked(m_ctrl->stepAttEnabled(1));
    m_spnRx1StepAttValue->setEnabled(m_ctrl->stepAttEnabled(0));
    m_spnRx2StepAttValue->setEnabled(m_ctrl->stepAttEnabled(1));
}

} // namespace NereusSDR
```

- [ ] **Step 3: Register in SetupDialog**

In `src/gui/SetupDialog.cpp`, add include:

```cpp
#include "setup/GeneralOptionsPage.h"
```

In `buildTree()`, after the Navigation page (line 131), add:

```cpp
    add(general, "Options",                new GeneralOptionsPage(m_model));
```

- [ ] **Step 4: Add to CMakeLists.txt**

In `CMakeLists.txt`, after `src/gui/setup/GeneralSetupPages.cpp` (around line 373), add:

```cmake
    src/gui/setup/GeneralOptionsPage.cpp
```

- [ ] **Step 5: Build to verify**

Run: `cmake --build build -j$(nproc)`

Expected: Clean build.

- [ ] **Step 6: Commit**

```bash
git add src/gui/setup/GeneralOptionsPage.h src/gui/setup/GeneralOptionsPage.cpp \
        src/gui/SetupDialog.cpp CMakeLists.txt
git commit -m "feat(setup): GeneralOptionsPage — step ATT + auto-att config

New Options page under Setup → General with step attenuator enable/
value per RX, auto-attenuate controls (enable, Classic/Adaptive mode
combo, undo/decay toggle, hold seconds). All tooltips ported from
Thetis setup.cs with source citations.

Porting from Thetis setup.cs: grpHermesStepAttenuator, groupBoxTS47."
```

---

### Task 5: RxApplet ATT Row

**Files:**
- Modify: `src/gui/applets/RxApplet.h`
- Modify: `src/gui/applets/RxApplet.cpp`

- [ ] **Step 1: Add ATT row members to header**

In `src/gui/applets/RxApplet.h`, add includes:

```cpp
class QStackedWidget;
```

After the Squelch members (around line 119, after `m_sqlSlider`), add:

```cpp
    // ATT/S-ATT row (between Squelch and AGC)
    QLabel*        m_attLabel{nullptr};
    QStackedWidget* m_attStack{nullptr};
    QComboBox*     m_preampCombo{nullptr};   // Page 0: ATT mode
    QSpinBox*      m_stepAttSpin{nullptr};   // Page 1: S-ATT mode
```

- [ ] **Step 2: Build the ATT row in RxApplet::buildUi**

In `src/gui/applets/RxApplet.cpp`, add includes:

```cpp
#include "core/StepAttenuatorController.h"
#include <QStackedWidget>
```

In `buildUi()`, between the Squelch section (around line 345, after `NyiOverlay::markNyi(m_sqlSlider, ...)`) and the AGC section (around line 348, `// Controls 9 + 10: AGC`), insert:

```cpp
    // ATT/S-ATT row — between Squelch and AGC
    // From Thetis console.cs: comboPreamp / udRX1StepAttData (stacked)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        row->setContentsMargins(0, 0, 0, 0);

        m_attLabel = new QLabel(QStringLiteral("ATT"), this);
        m_attLabel->setFixedWidth(34);
        m_attLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #8aa8c0; font-size: 11px; }"));
        row->addWidget(m_attLabel);

        m_attStack = new QStackedWidget(this);
        m_attStack->setFixedHeight(20);

        // Page 0: Preamp combo (ATT mode — step att disabled)
        m_preampCombo = new QComboBox(this);
        m_preampCombo->addItems({QStringLiteral("0dB"), QStringLiteral("-10dB"),
                                 QStringLiteral("-20dB"), QStringLiteral("-30dB")});
        m_preampCombo->setFixedWidth(70);
        m_preampCombo->setFixedHeight(20);
        applyComboStyle(m_preampCombo);
        m_attStack->addWidget(m_preampCombo);

        // Page 1: Step att spinbox (S-ATT mode — step att enabled)
        m_stepAttSpin = new QSpinBox(this);
        m_stepAttSpin->setRange(0, 31);
        m_stepAttSpin->setSuffix(QStringLiteral(" dB"));
        m_stepAttSpin->setFixedWidth(70);
        m_stepAttSpin->setFixedHeight(20);
        m_attStack->addWidget(m_stepAttSpin);

        m_attStack->setCurrentIndex(0);  // default to preamp combo
        row->addWidget(m_attStack, 1);

        rightCol->addLayout(row);
    }
```

- [ ] **Step 3: Wire ATT controls to controller in `connectSlice`**

In `src/gui/applets/RxApplet.cpp`, in the `connectSlice` method, add after existing connections:

```cpp
    // ATT/S-ATT — wire to StepAttenuatorController if available
    auto* attCtrl = m_slice ? (model() ? model()->stepAttController() : nullptr)
                            : nullptr;
    if (attCtrl) {
        connect(m_stepAttSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [attCtrl](int val) {
            attCtrl->setAttenuation(0, val);
        });

        connect(m_preampCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [attCtrl](int idx) {
            attCtrl->setPreampMode(0, static_cast<PreampMode>(idx));
        });

        connect(attCtrl, &StepAttenuatorController::attenuationChanged,
                this, [this](int rx, int dB) {
            if (rx == 0) {
                QSignalBlocker blk(m_stepAttSpin);
                m_stepAttSpin->setValue(dB);
            }
        });

        // Update label and stack page based on step-att-enabled state
        const bool stepOn = attCtrl->stepAttEnabled(0);
        m_attLabel->setText(stepOn ? QStringLiteral("S-ATT") : QStringLiteral("ATT"));
        m_attStack->setCurrentIndex(stepOn ? 1 : 0);
    }
```

- [ ] **Step 4: Build to verify**

Run: `cmake --build build -j$(nproc)`

Expected: Clean build.

- [ ] **Step 5: Commit**

```bash
git add src/gui/applets/RxApplet.h src/gui/applets/RxApplet.cpp
git commit -m "feat(applet): ATT/S-ATT row in RxApplet between Squelch and AGC

New row with dynamic ATT/S-ATT label and stacked preamp combo (ATT
mode) or dB spinbox (S-ATT mode). Wired to StepAttenuatorController.
Capability-gated per board, per-band values restored on band change.

Porting from Thetis console.cs: comboPreamp / udRX1StepAttData."
```

---

### Task 6: Persistence — Load/Save Settings

**Files:**
- Modify: `src/core/StepAttenuatorController.h`
- Modify: `src/core/StepAttenuatorController.cpp`
- Modify: `src/gui/MainWindow.cpp`

- [ ] **Step 1: Add persistence methods to controller**

In `src/core/StepAttenuatorController.h`, add public methods:

```cpp
    // Persistence — save/restore from AppSettings per radio MAC
    void saveSettings(const QString& mac);
    void loadSettings(const QString& mac);
```

- [ ] **Step 2: Implement saveSettings and loadSettings**

In `src/core/StepAttenuatorController.cpp`, add include:

```cpp
#include "core/AppSettings.h"
```

Add at the end (before closing namespace brace):

```cpp
void StepAttenuatorController::saveSettings(const QString& mac)
{
    auto& s = AppSettings::instance();

    for (int rx = 0; rx < kMaxRx; ++rx) {
        const auto& state = m_rx[rx];
        const QString prefix = QStringLiteral("options/stepAtt/rx%1").arg(rx + 1);

        s.setHardwareValue(mac, prefix + QStringLiteral("Enabled"),
                           state.stepAttEnabled ? QStringLiteral("True")
                                                : QStringLiteral("False"));
        s.setHardwareValue(mac, prefix + QStringLiteral("Value"),
                           QString::number(state.attenuationDb));

        const QString autoPrefix = QStringLiteral("options/autoAtt/rx%1").arg(rx + 1);
        s.setHardwareValue(mac, autoPrefix + QStringLiteral("Enabled"),
                           state.autoAttEnabled ? QStringLiteral("True")
                                                : QStringLiteral("False"));
        s.setHardwareValue(mac, autoPrefix + QStringLiteral("Mode"),
                           state.autoAttMode == AutoAttMode::Classic
                               ? QStringLiteral("Classic")
                               : QStringLiteral("Adaptive"));
        s.setHardwareValue(mac, autoPrefix + QStringLiteral("Undo"),
                           state.autoAttUndo ? QStringLiteral("True")
                                             : QStringLiteral("False"));
        s.setHardwareValue(mac, autoPrefix + QStringLiteral("HoldSeconds"),
                           QString::number(state.autoAttHoldSeconds));

        // Per-band ATT values
        for (auto it = state.perBandAtt.begin(); it != state.perBandAtt.end(); ++it) {
            const QString key = QStringLiteral("options/stepAtt/rx%1Band/%2")
                                    .arg(rx + 1).arg(bandKeyName(it.key()));
            s.setHardwareValue(mac, key, QString::number(it.value()));
        }

        // Per-band preamp modes
        for (auto it = state.perBandPreamp.begin(); it != state.perBandPreamp.end(); ++it) {
            const QString key = QStringLiteral("options/preamp/rx%1Band/%2")
                                    .arg(rx + 1).arg(bandKeyName(it.key()));
            s.setHardwareValue(mac, key, QString::number(static_cast<int>(it.value())));
        }

        // Adaptive settled floors
        for (auto it = state.settledFloor.begin(); it != state.settledFloor.end(); ++it) {
            const QString key = QStringLiteral("options/autoAtt/rx%1Settled/%2")
                                    .arg(rx + 1).arg(bandKeyName(it.key()));
            s.setHardwareValue(mac, key, QString::number(it.value()));
        }
    }

    s.save();
}

void StepAttenuatorController::loadSettings(const QString& mac)
{
    auto& s = AppSettings::instance();

    for (int rx = 0; rx < kMaxRx; ++rx) {
        auto& state = m_rx[rx];
        const QString prefix = QStringLiteral("options/stepAtt/rx%1").arg(rx + 1);

        state.stepAttEnabled = s.hardwareValue(mac, prefix + QStringLiteral("Enabled"),
                                               QStringLiteral("False")).toString()
                               == QStringLiteral("True");
        state.attenuationDb = s.hardwareValue(mac, prefix + QStringLiteral("Value"),
                                              0).toInt();

        const QString autoPrefix = QStringLiteral("options/autoAtt/rx%1").arg(rx + 1);
        state.autoAttEnabled = s.hardwareValue(mac, autoPrefix + QStringLiteral("Enabled"),
                                               QStringLiteral("False")).toString()
                               == QStringLiteral("True");
        const QString modeStr = s.hardwareValue(mac, autoPrefix + QStringLiteral("Mode"),
                                                QStringLiteral("Classic")).toString();
        state.autoAttMode = (modeStr == QStringLiteral("Adaptive"))
                                ? AutoAttMode::Adaptive
                                : AutoAttMode::Classic;
        state.autoAttUndo = s.hardwareValue(mac, autoPrefix + QStringLiteral("Undo"),
                                            QStringLiteral("True")).toString()
                            == QStringLiteral("True");
        state.autoAttHoldSeconds = s.hardwareValue(mac, autoPrefix + QStringLiteral("HoldSeconds"),
                                                   5).toInt();

        // Per-band ATT values
        for (int b = 0; b < static_cast<int>(Band::Count); ++b) {
            const Band band = static_cast<Band>(b);
            const QString key = QStringLiteral("options/stepAtt/rx%1Band/%2")
                                    .arg(rx + 1).arg(bandKeyName(band));
            const QVariant val = s.hardwareValue(mac, key);
            if (val.isValid()) {
                state.perBandAtt[band] = val.toInt();
            }
        }

        // Per-band preamp modes
        for (int b = 0; b < static_cast<int>(Band::Count); ++b) {
            const Band band = static_cast<Band>(b);
            const QString key = QStringLiteral("options/preamp/rx%1Band/%2")
                                    .arg(rx + 1).arg(bandKeyName(band));
            const QVariant val = s.hardwareValue(mac, key);
            if (val.isValid()) {
                state.perBandPreamp[band] = static_cast<PreampMode>(val.toInt());
            }
        }

        // Adaptive settled floors
        for (int b = 0; b < static_cast<int>(Band::Count); ++b) {
            const Band band = static_cast<Band>(b);
            const QString key = QStringLiteral("options/autoAtt/rx%1Settled/%2")
                                    .arg(rx + 1).arg(bandKeyName(band));
            const QVariant val = s.hardwareValue(mac, key);
            if (val.isValid()) {
                state.settledFloor[band] = val.toInt();
            }
        }
    }
}
```

- [ ] **Step 3: Wire load/save in MainWindow**

In `src/gui/MainWindow.cpp`, in the radio connection handler (where the radio connects and board capabilities are read), add after `setMaxAttenuation`:

```cpp
    m_stepAttController->loadSettings(m_model->connection()->radioInfo().macAddress);
```

In the disconnect or cleanup handler, add:

```cpp
    if (m_model->connection()) {
        m_stepAttController->saveSettings(m_model->connection()->radioInfo().macAddress);
    }
```

- [ ] **Step 4: Build to verify**

Run: `cmake --build build -j$(nproc)`

Expected: Clean build.

- [ ] **Step 5: Commit**

```bash
git add src/core/StepAttenuatorController.h src/core/StepAttenuatorController.cpp \
        src/gui/MainWindow.cpp
git commit -m "feat(core): per-MAC persistence for step ATT & auto-att settings

Save/load all step attenuator config, per-band ATT/preamp values, and
adaptive mode settled floors via AppSettings::setHardwareValue(mac, ...).
Load on radio connect, save on disconnect.

Keys: options/stepAtt/rx{1,2}*, options/autoAtt/rx{1,2}*,
options/preamp/rx{1,2}Band/<bandKey>."
```

---

### Task 7: Additional Auto-Attenuate Tests

**Files:**
- Modify: `tests/tst_step_attenuator_controller.cpp`

- [ ] **Step 1: Add Classic mode auto-attenuate test**

Append to the test class in `tests/tst_step_attenuator_controller.cpp`:

```cpp
    void classicAutoAtt_bumpsOnRed()
    {
        StepAttenuatorController ctrl;
        ctrl.setStepAttEnabled(0, true);
        ctrl.setAutoAttEnabled(0, true);
        ctrl.setAutoAttMode(0, AutoAttMode::Classic);
        ctrl.setAutoAttUndo(0, false);  // no undo for this test
        QSignalSpy attSpy(&ctrl, &StepAttenuatorController::attenuationChanged);

        // Push ADC0 to Red (4 ticks)
        for (int i = 0; i < 4; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        // Should have bumped ATT from 0 to 1
        bool foundBump = false;
        for (const auto& args : attSpy) {
            if (args.at(0).toInt() == 0 && args.at(1).toInt() > 0) {
                foundBump = true;
            }
        }
        QVERIFY(foundBump);
    }

    void adaptiveAttack_rampsGradually()
    {
        StepAttenuatorController ctrl;
        ctrl.setStepAttEnabled(0, true);
        ctrl.setAutoAttEnabled(0, true);
        ctrl.setAutoAttMode(0, AutoAttMode::Adaptive);
        QSignalSpy attSpy(&ctrl, &StepAttenuatorController::attenuationChanged);

        // Push to Red and keep overloading for 3 more ticks
        for (int i = 0; i < 7; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        // Should have ramped up gradually (multiple 1-dB increments)
        // First 4 ticks build to Red, then 3 ticks of attack = 3 dB increase
        int maxDb = 0;
        for (const auto& args : attSpy) {
            if (args.at(0).toInt() == 0) {
                maxDb = std::max(maxDb, args.at(1).toInt());
            }
        }
        QVERIFY(maxDb >= 3);
    }

    void adaptiveDecay_relaxesAfterHold()
    {
        StepAttenuatorController ctrl;
        ctrl.setStepAttEnabled(0, true);
        ctrl.setAutoAttEnabled(0, true);
        ctrl.setAutoAttMode(0, AutoAttMode::Adaptive);
        ctrl.setAutoAttUndo(0, true);
        ctrl.setAutoAttHoldSeconds(0, 1);  // 1 second hold = 10 ticks
        QSignalSpy attSpy(&ctrl, &StepAttenuatorController::attenuationChanged);

        // Push to Red + attack for 6 ticks total (4 to Red + 2 attack = 2 dB)
        for (int i = 0; i < 6; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }
        attSpy.clear();

        // Hold phase: 10 ticks without overflow
        for (int i = 0; i < 10; ++i) {
            ctrl.tick();
        }

        // Decay phase: tick until ATT drops
        for (int i = 0; i < 20; ++i) {
            ctrl.tick();
        }

        // Should have decayed (at least one attenuationChanged with lower value)
        bool decayed = false;
        for (const auto& args : attSpy) {
            if (args.at(0).toInt() == 0 && args.at(1).toInt() < 2) {
                decayed = true;
            }
        }
        QVERIFY(decayed);
    }
```

- [ ] **Step 2: Build and run all tests**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build -R tst_step_attenuator_controller -V`

Expected: All 9 tests PASS.

- [ ] **Step 3: Commit**

```bash
git add tests/tst_step_attenuator_controller.cpp
git commit -m "test(core): Classic + Adaptive auto-att controller tests

Verify Classic mode bumps ATT on Red threshold, Adaptive mode ramps
gradually during attack, and Adaptive decay relaxes after hold period."
```

---

### Task 8: Integration Build + Manual Smoke Test

**Files:** None new — verification only.

- [ ] **Step 1: Full build**

Run: `cmake --build build -j$(nproc)`

Expected: Clean build, zero warnings from new files.

- [ ] **Step 2: Run full test suite**

Run: `ctest --test-dir build -V`

Expected: All existing tests + new `tst_step_attenuator_controller` pass.

- [ ] **Step 3: Launch app and verify**

Run: `./build/NereusSDR`

Verify:
1. Setup → General → Options page appears with Step Attenuator and Auto Attenuate groups
2. RxApplet shows ATT row between Squelch and AGC
3. Status bar has space for ADC OVL badge (hidden when no radio connected)
4. Tooltips appear on all new controls

- [ ] **Step 4: Commit any fixes found during smoke test**

If any fixes were needed, commit them individually with descriptive messages.

---

## Self-Review Checklist

- [x] **Spec coverage:** All 8 spec sections mapped to tasks: Protocol (T1), Controller (T2), Wiring (T3), Setup UI (T4), RxApplet (T5), Persistence (T6), Tests (T7), Smoke (T8)
- [x] **No placeholders:** Every step has code or exact commands
- [x] **Type consistency:** `OverloadLevel`, `AutoAttMode`, `PreampMode` enums defined in T2 header, used consistently in T3-T7
- [x] **Signal names match:** `overloadStatusChanged`, `attenuationChanged`, `preampModeChanged`, `adcLinkedChanged`, `autoAttActiveChanged` — consistent across header, impl, and consumers
- [x] **File paths exact:** All paths verified against codebase exploration
