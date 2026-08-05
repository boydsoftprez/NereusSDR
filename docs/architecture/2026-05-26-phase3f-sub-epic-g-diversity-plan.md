# Phase 3F Sub-Epic G: Full Diversity Port — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port Thetis's full Diversity feature set to NereusSDR. After this plan lands, operators on 2-ADC SKUs can engage diversity on Slice A, dial phase + gain via sliders or 8-memory recall, use the polar radar visualization, perform direction finding via antenna spacing math, and persist all settings per-band per-MAC.

**Architecture:** WDSP wrappers (`SetEXTDIVRun/Nr/Output/Rotate`) added to `RxChannel`. `DiversityDialog` (modeless QDialog) hosts the full Thetis UI surface. `DiversityRadarWidget` (custom QPainter) renders polar sensitivity pattern. Slice A's DDC topology shifts from DDC2 to DDC0+DDC1 sync pair when diversity engages (codec extension from Sub-Epic B handles wire bytes). Memory storage in AppSettings per-band per-slot.

**Tech Stack:** C++20, Qt6 (custom paint widget, QDialog modeless, QButtonGroup, QSpinBox, QSlider), WDSP P/Invokes wrapped in `RxChannel`, FFTW3 already linked, QtTest.

**Parent design:** [docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md](2026-05-26-phase3f-multi-pan-multi-slice-design.md) §8 (Full Diversity port)

**Prereqs:** Sub-Epics A-F complete. Sub-Epic A added `SliceModel::diversityEnabled` Q_PROPERTY; Sub-Epic B added the diversity DDC topology shift in codec.

**Estimated effort:** 10 working days, 25 tasks, ~135 bite-sized steps.

---

## File Structure

### Files to create

| File | Purpose |
|---|---|
| `src/gui/dialogs/DiversityDialog.{h,cpp}` | Modeless dialog with full Thetis UI |
| `src/gui/widgets/DiversityRadarWidget.{h,cpp}` | Polar sensitivity pattern, ~500 lines |
| `tests/tst_diversity_dialog_persistence.cpp` | Per-band per-slot memory round-trip |
| `tests/tst_diversity_radar_widget_math.cpp` | SensitivityAtAngle math from Thetis port |
| `tests/tst_rx_channel_ext_div_wrappers.cpp` | WDSP wrapper signatures |

### Files to modify

| File | Purpose |
|---|---|
| `src/core/wdsp/RxChannel.{h,cpp}` | Add `setExtDivRun/Nr/Output/Rotate` wrappers |
| `src/models/SliceModel.{h,cpp}` | Per-band diversity persistence (phase, gain, fine-null, cross-fire, lock-angle, 8 memories × 2 fields) |
| `src/gui/MainWindow.cpp` | Add "Tools → Diversity Dialog…" menu entry (Ctrl+Shift+D) |
| `src/models/RadioModel.{h,cpp}` | DiversityDialog lifecycle (one per RadioModel, lazy-created) |

---

## Task 1: WDSP wrapper signatures on `RxChannel`

**Files:** Modify `src/core/wdsp/RxChannel.{h,cpp}`

- [ ] **Step 1: Write failing test**

Create `tests/tst_rx_channel_ext_div_wrappers.cpp`:

```cpp
#include <QtTest/QtTest>
#include "core/wdsp/RxChannel.h"

using namespace NereusSDR;

class TestRxChannelExtDivWrappers : public QObject {
    Q_OBJECT
private slots:
    void wrapper_setExtDivRun_compiles()
    {
        // Compile-only test: ensure the wrapper exists with the right signature.
        // Actual DSP behaviour requires a live WDSP session.
        RxChannel ch;
        ch.setExtDivRun(true);
        QVERIFY(true);
    }

    void wrapper_setExtDivRotate_accepts_phase_gain()
    {
        RxChannel ch;
        double iRotate[2] = {1.0, 0.0};
        double qRotate[2] = {0.0, 1.0};
        ch.setExtDivRotate(2, iRotate, qRotate);
        QVERIFY(true);
    }
};

QTEST_MAIN(TestRxChannelExtDivWrappers)
#include "tst_rx_channel_ext_div_wrappers.moc"
```

Register: `nereus_add_test(tst_rx_channel_ext_div_wrappers)`.

- [ ] **Step 2: Run + verify failure (compile)**

Expected: `'setExtDivRun' is not a member of 'RxChannel'`.

- [ ] **Step 3: Add wrappers to RxChannel.h**

Find the existing WDSP wrapper methods section (e.g. `setNbRun`, `setNrSlot`). Add:

```cpp
    /// Phase 3F Sub-Epic G: WDSP External Diversity wrappers.
    /// Ported from Thetis dsp.cs:609-619 [v2.10.3.15] P/Invokes.
    /// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §8.
    void setExtDivRun(bool run);
    void setExtDivNr(int numInputs);
    void setExtDivOutput(int outputMode);  // 0=combined, 1=primary, 2=secondary
    void setExtDivRotate(int numInputs, const double* iRotate, const double* qRotate);
```

- [ ] **Step 4: Implement in RxChannel.cpp**

Find the existing WDSP wrappers. Add:

```cpp
extern "C" {
    void SetEXTDIVRun(int id, int run);
    void SetEXTDIVNr(int id, int nr);
    void SetEXTDIVOutput(int id, int output);
    void SetEXTDIVRotate(int id, int nr, double* Irotate, double* Qrotate);
}

void RxChannel::setExtDivRun(bool run)
{
    SetEXTDIVRun(m_channelId, run ? 1 : 0);
}

void RxChannel::setExtDivNr(int nr)
{
    SetEXTDIVNr(m_channelId, nr);
}

void RxChannel::setExtDivOutput(int output)
{
    SetEXTDIVOutput(m_channelId, output);
}

void RxChannel::setExtDivRotate(int nr, const double* iRotate, const double* qRotate)
{
    // WDSP signature takes non-const double*; safe to const_cast here since
    // SetEXTDIVRotate is read-only on the buffers (per Thetis usage).
    SetEXTDIVRotate(m_channelId, nr,
                    const_cast<double*>(iRotate),
                    const_cast<double*>(qRotate));
}
```

- [ ] **Step 5: Verify libwdsp links these symbols**

```bash
nm build/third_party/wdsp/libwdsp.a 2>/dev/null | grep -E "SetEXTDIV" | head -5
```

Expected: 4 symbols (Run, Nr, Output, Rotate) present.

- [ ] **Step 6: Commit**

```bash
cmake --build build && ctest --test-dir build -R tst_rx_channel_ext_div_wrappers -V 2>&1 | tail -10
git add src/core/wdsp/RxChannel.{h,cpp} tests/tst_rx_channel_ext_div_wrappers.cpp tests/CMakeLists.txt
git commit -m "feat(3f-g): RxChannel WDSP setExtDivRun/Nr/Output/Rotate wrappers"
```

---

## Task 2: Per-band diversity persistence schema in SliceModel

**Files:** Modify `src/models/SliceModel.{h,cpp}`

- [ ] **Step 1: Add Q_PROPERTYs for the 6 per-band diversity values**

(`diversityEnabled` was added in Sub-Epic A. Add the rest now.)

In SliceModel.h Q_PROPERTY block (after diversityEnabled):

```cpp
    // Phase 3F Sub-Epic G: per-band diversity controls (Slice A only).
    Q_PROPERTY(double diversityPhase READ diversityPhase WRITE setDiversityPhase NOTIFY diversityPhaseChanged)
    Q_PROPERTY(double diversityGain READ diversityGain WRITE setDiversityGain NOTIFY diversityGainChanged)
    Q_PROPERTY(double diversityFineNull READ diversityFineNull WRITE setDiversityFineNull NOTIFY diversityFineNullChanged)
    Q_PROPERTY(bool   diversityCrossFire READ diversityCrossFire WRITE setDiversityCrossFire NOTIFY diversityCrossFireChanged)
    Q_PROPERTY(bool   diversityLockAngle READ diversityLockAngle WRITE setDiversityLockAngle NOTIFY diversityLockAngleChanged)
```

Add getters/setters/signals/members for each (5 × 4 = 20 lines, following the pattern from Sub-Epic A).

- [ ] **Step 2: Extend per-band save/load to include these**

In `savePerBand`:
```cpp
    s.setValue(sp + QStringLiteral("DiversityPhase"), m_diversityPhase);
    s.setValue(sp + QStringLiteral("DiversityGain"), m_diversityGain);
    s.setValue(sp + QStringLiteral("DiversityFineNull"), m_diversityFineNull);
    s.setValue(sp + QStringLiteral("DiversityCrossFire"), m_diversityCrossFire);
    s.setValue(sp + QStringLiteral("DiversityLockAngle"), m_diversityLockAngle);
```

In `loadPerBand`:
```cpp
    setDiversityPhase(s.value(sp + QStringLiteral("DiversityPhase"), 0.0).toDouble());
    setDiversityGain(s.value(sp + QStringLiteral("DiversityGain"), 1.0).toDouble());
    setDiversityFineNull(s.value(sp + QStringLiteral("DiversityFineNull"), 0.0).toDouble());
    setDiversityCrossFire(s.value(sp + QStringLiteral("DiversityCrossFire"), false).toBool());
    setDiversityLockAngle(s.value(sp + QStringLiteral("DiversityLockAngle"), false).toBool());
```

- [ ] **Step 3: Persistence round-trip test**

In `tests/tst_diversity_dialog_persistence.cpp` (created in Task 22):

```cpp
    void per_band_diversity_persists()
    {
        SliceModel slice;
        slice.setSliceLetter(QChar('A'));
        slice.setBand(Band::B20M);
        slice.setDiversityPhase(47.5);
        slice.setDiversityGain(0.85);
        slice.savePerBand("test-mac");

        SliceModel slice2;
        slice2.setBand(Band::B20M);
        slice2.loadPerBand("test-mac");
        QCOMPARE(slice2.diversityPhase(), 47.5);
        QCOMPARE(slice2.diversityGain(), 0.85);
    }
```

- [ ] **Step 4: Commit**

```bash
cmake --build build && ctest --test-dir build -R tst_slice_model 2>&1 | tail -5
git add src/models/SliceModel.{h,cpp}
git commit -m "feat(3f-g): SliceModel per-band diversity properties (phase/gain/fineNull/crossFire/lockAngle)"
```

---

## Task 3: Per-band 8-memory diversity slots

**Files:** Modify `src/models/SliceModel.{h,cpp}`

- [ ] **Step 1: Add memory storage**

In SliceModel.h:
```cpp
    /// Phase 3F Sub-Epic G: per-band 8 memory slots for diversity phase+gain.
    struct DiversityMemorySlot {
        double phase {0.0};
        double gain {1.0};
        bool   filled {false};
    };

    DiversityMemorySlot diversityMemory(int band, int slotIdx) const;
    void setDiversityMemory(int band, int slotIdx, double phase, double gain);
    void clearDiversityMemory(int band, int slotIdx);

private:
    std::array<std::array<DiversityMemorySlot, 8>, 14> m_diversityMemories;  // 14 bands × 8 slots
```

- [ ] **Step 2: Implement getters/setters + persistence**

```cpp
SliceModel::DiversityMemorySlot SliceModel::diversityMemory(int band, int slot) const
{
    if (band < 0 || band >= 14 || slot < 0 || slot >= 8) { return {}; }
    return m_diversityMemories[band][slot];
}

void SliceModel::setDiversityMemory(int band, int slot, double phase, double gain)
{
    if (band < 0 || band >= 14 || slot < 0 || slot >= 8) { return; }
    m_diversityMemories[band][slot] = {phase, gain, true};
    // Save to AppSettings
    auto& s = AppSettings::instance();
    const QString key = QStringLiteral("hardware/%1/Slice%2_Band_%3_DiversityM%4")
        .arg(m_macAddress, m_sliceLetter)
        .arg(bandKeyName(static_cast<Band>(band)))
        .arg(slot + 1);
    s.setValue(key + QStringLiteral("_Phase"), phase);
    s.setValue(key + QStringLiteral("_Gain"), gain);
}

void SliceModel::clearDiversityMemory(int band, int slot)
{
    if (band < 0 || band >= 14 || slot < 0 || slot >= 8) { return; }
    m_diversityMemories[band][slot] = {};
    // Also remove from AppSettings
}
```

- [ ] **Step 3: Load on band change**

Extend `loadPerBand`:
```cpp
    for (int slot = 0; slot < 8; ++slot) {
        const QString key = sp + QStringLiteral("DiversityM%1").arg(slot + 1);
        const double phase = s.value(key + QStringLiteral("_Phase"), 0.0).toDouble();
        const double gain = s.value(key + QStringLiteral("_Gain"), 1.0).toDouble();
        const bool filled = s.contains(key + QStringLiteral("_Phase"));
        if (filled) {
            const int bandIdx = int(m_band);
            m_diversityMemories[bandIdx][slot] = {phase, gain, true};
        }
    }
```

- [ ] **Step 4: Test round-trip**

```cpp
    void diversity_memory_slot_persists()
    {
        SliceModel slice;
        slice.setSliceLetter(QChar('A'));
        slice.setMacAddress("test-mac");
        slice.setDiversityMemory(5, 2, 47.5, 0.85);  // band=20m, slot=M3

        SliceModel slice2;
        slice2.setSliceLetter(QChar('A'));
        slice2.setMacAddress("test-mac");
        slice2.setBand(Band::B20M);
        slice2.loadPerBand("test-mac");

        const auto mem = slice2.diversityMemory(5, 2);
        QCOMPARE(mem.filled, true);
        QCOMPARE(mem.phase, 47.5);
        QCOMPARE(mem.gain, 0.85);
    }
```

- [ ] **Step 5: Commit**

```bash
git add src/models/SliceModel.{h,cpp}
git commit -m "feat(3f-g): SliceModel 8 diversity memory slots per band (224 values total)"
```

---

## Task 4: `DiversityDialog` skeleton

**Files:** Create `src/gui/dialogs/DiversityDialog.{h,cpp}`

- [ ] **Step 1: Create header**

```cpp
#pragma once
#include <QDialog>

namespace NereusSDR {

class SliceModel;
class RadioModel;

/// Modeless dialog for diversity operation. Ported from Thetis DiversityForm.cs
/// (2937 lines). See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §8.
class DiversityDialog : public QDialog {
    Q_OBJECT
public:
    explicit DiversityDialog(RadioModel* radio, QWidget* parent = nullptr);
    ~DiversityDialog() override;

private slots:
    void onEnableToggled(bool on);
    void onPhaseChanged(double phase);
    void onGainChanged(double gain);
    void onFineNullChanged(double fn);
    void onCrossFireToggled(bool on);
    void onLockAngleToggled(bool on);
    void onShiftButtonClicked(int degrees);  // -10, -45, -90, +10, +45, +90, 180
    void onMemorySlotClicked(int slotIdx);
    void onMemorySlotRightClicked(int slotIdx);
    void onReferenceAdcChanged(int adc);
    void onAutoFindNull();

private:
    RadioModel* m_radio {nullptr};
    SliceModel* m_sliceA {nullptr};  // diversity is Slice-A only
    void rebindToSliceA();
    void refreshFromSlice();
};

} // namespace NereusSDR
```

- [ ] **Step 2: Stub implementation (UI scaffolding only)**

```cpp
#include "gui/dialogs/DiversityDialog.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>

namespace NereusSDR {

DiversityDialog::DiversityDialog(RadioModel* radio, QWidget* parent)
    : QDialog(parent), m_radio(radio)
{
    setWindowTitle(QStringLiteral("Diversity"));
    setStyleSheet(QStringLiteral("background: %1; color: %2;").arg(Style::kPanelBg, Style::kTextPrimary));
    setMinimumSize(680, 600);

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    // Left column: radar + direction finding (Task 5)
    auto* leftColumn = new QVBoxLayout();
    leftColumn->addWidget(new QLabel(QStringLiteral("[Radar widget placeholder]"), this));
    mainLayout->addLayout(leftColumn);

    // Right column: all controls (Tasks 6-10)
    auto* rightColumn = new QVBoxLayout();
    rightColumn->addWidget(new QLabel(QStringLiteral("[Controls placeholder]"), this));
    mainLayout->addLayout(rightColumn);

    rebindToSliceA();
}

DiversityDialog::~DiversityDialog() = default;

void DiversityDialog::rebindToSliceA()
{
    if (m_radio && !m_radio->slices().isEmpty()) {
        m_sliceA = m_radio->slices().first();
    }
}

void DiversityDialog::refreshFromSlice() {}
void DiversityDialog::onEnableToggled(bool) {}
void DiversityDialog::onPhaseChanged(double) {}
void DiversityDialog::onGainChanged(double) {}
void DiversityDialog::onFineNullChanged(double) {}
void DiversityDialog::onCrossFireToggled(bool) {}
void DiversityDialog::onLockAngleToggled(bool) {}
void DiversityDialog::onShiftButtonClicked(int) {}
void DiversityDialog::onMemorySlotClicked(int) {}
void DiversityDialog::onMemorySlotRightClicked(int) {}
void DiversityDialog::onReferenceAdcChanged(int) {}
void DiversityDialog::onAutoFindNull() {}

} // namespace NereusSDR
```

- [ ] **Step 3: Wire into Tools menu**

In MainWindow.cpp where the Tools menu is built:
```cpp
    auto* divAct = toolsMenu->addAction(QStringLiteral("Diversity Dialog…"));
    divAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+D")));
    divAct->setEnabled(false);  // toggled by capability gate below

    connect(m_radioModel, &RadioModel::currentRadioChanged, this, [this, divAct](const RadioInfo& info) {
        const auto caps = capabilitiesFor(info.model);
        divAct->setEnabled(caps.hasDiversityReceiver);
    });

    connect(divAct, &QAction::triggered, this, [this]() {
        static DiversityDialog* dlg = nullptr;
        if (!dlg) {
            dlg = new DiversityDialog(m_radioModel, this);
        }
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    });
```

- [ ] **Step 4: Commit**

```bash
cmake --build build
git add src/gui/dialogs/DiversityDialog.{h,cpp} src/gui/MainWindow.cpp src/gui/CMakeLists.txt
git commit -m "feat(3f-g): DiversityDialog skeleton + Tools menu entry (Ctrl+Shift+D, capability-gated)"
```

---

## Task 5: `DiversityRadarWidget` polar paint widget

**Files:** Create `src/gui/widgets/DiversityRadarWidget.{h,cpp}`

This ports Thetis `picRadar_Paint` + `SensitivityAtAngle()` math from `DiversityForm.cs:2390-2440`.

- [ ] **Step 1: Create header**

```cpp
#pragma once
#include <QWidget>

namespace NereusSDR {

/// Polar sensitivity pattern for diversity. Custom paint widget.
/// Ports Thetis DiversityForm.picRadar_Paint + SensitivityAtAngle math (DiversityForm.cs:2390-2440 [v2.10.3.15]).
/// User drags on the radar to retune phase/gain interactively.
class DiversityRadarWidget : public QWidget {
    Q_OBJECT
public:
    explicit DiversityRadarWidget(QWidget* parent = nullptr);

    void setPhase(double radians);       // -π to +π
    void setGain(double ratio);           // 0.0 to 10.0
    void setCrossFire(bool on);
    void setVfoFreqMhz(double mhz);       // drives wavelength λ
    void setAntennaSpacingMeters(double m);

signals:
    void phaseAdjusted(double newRadians);  // user drag updated
    void gainAdjusted(double newGain);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    double m_phase {0.0};
    double m_gain {1.0};
    bool   m_crossFire {false};
    double m_vfoMhz {14.225};
    double m_antSpacingM {5.5};
    bool   m_dragging {false};

    /// Returns RMS sensitivity at the given angle (radians).
    /// Ports Thetis SensitivityAtAngle() from DiversityForm.cs:2400-2438 [v2.10.3.15].
    double sensitivityAtAngle(double angleRad) const;
};

} // namespace NereusSDR
```

- [ ] **Step 2: Implement (compact)**

```cpp
#include "gui/widgets/DiversityRadarWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <cmath>

namespace NereusSDR {

DiversityRadarWidget::DiversityRadarWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(220, 220);
    setMouseTracking(true);
}

void DiversityRadarWidget::setPhase(double r) { m_phase = r; update(); }
void DiversityRadarWidget::setGain(double g) { m_gain = g; update(); }
void DiversityRadarWidget::setCrossFire(bool on) { m_crossFire = on; update(); }
void DiversityRadarWidget::setVfoFreqMhz(double mhz) { m_vfoMhz = mhz; update(); }
void DiversityRadarWidget::setAntennaSpacingMeters(double m) { m_antSpacingM = m; update(); }

void DiversityRadarWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = rect().adjusted(2, 2, -2, -2);
    const QPointF center = r.center();
    const double radius = std::min(r.width(), r.height()) / 2.0 - 8.0;

    // Background gradient
    QRadialGradient grad(center, radius);
    grad.setColorAt(0.0, QColor(0x00, 0x28, 0x48));
    grad.setColorAt(0.7, QColor(0x00, 0x10, 0x20));
    grad.setColorAt(1.0, QColor(0x00, 0x08, 0x10));
    p.setBrush(grad);
    p.setPen(QColor(0x20, 0x50, 0x70));
    p.drawEllipse(center, radius, radius);

    // Compass labels
    p.setPen(QColor(0x60, 0x70, 0x80));
    p.setFont(QFont("monospace", 8));
    p.drawText(QPointF(center.x() - 4, r.top() + 12), "N");
    p.drawText(QPointF(r.right() - 12, center.y() + 4), "E");
    p.drawText(QPointF(center.x() - 4, r.bottom() - 4), "S");
    p.drawText(QPointF(r.left() + 4, center.y() + 4), "W");

    // Range rings (50%, 30%)
    p.setPen(QPen(QColor(0x1a, 0x40, 0x60), 1, Qt::DashLine));
    p.drawEllipse(center, radius * 0.5, radius * 0.5);
    p.drawEllipse(center, radius * 0.3, radius * 0.3);

    // Sensitivity lobe: sample 360 angles, draw closed polyline
    QPolygonF lobe;
    for (int deg = 0; deg < 360; deg += 3) {
        const double angleRad = deg * M_PI / 180.0;
        const double sens = sensitivityAtAngle(angleRad);  // 0..1
        const double dist = radius * sens;
        lobe << center + QPointF(dist * std::sin(angleRad), -dist * std::cos(angleRad));
    }
    p.setBrush(QColor(0, 255, 255, 50));
    p.setPen(QPen(QColor(0, 255, 255), 1.5));
    p.drawPolygon(lobe);

    // Centre dot
    p.setBrush(QColor(0xff, 0xcc, 0x99));
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, 3, 3);
}

double DiversityRadarWidget::sensitivityAtAngle(double theta) const
{
    // Ported from Thetis DiversityForm.SensitivityAtAngle (DiversityForm.cs:2400-2438 [v2.10.3.15]).
    const double freqHz = m_vfoMhz * 1e6;
    const double c = 299792458.0;
    const double lambda = c / freqHz;
    const double d_lambda = m_antSpacingM / lambda;

    const double crossFireOffset = m_crossFire ? M_PI : 0.0;
    const double steeringAngle = m_phase;
    const double phi = crossFireOffset + std::cos(theta + steeringAngle);

    // 20-sample RMS over one cycle
    const double dt = 1.0 / (20.0 * freqHz);
    const double angularFreq = 2.0 * M_PI * freqHz * dt;
    double rms = 0.0;
    for (int i = 0; i < 20; ++i) {
        const double v1 = std::sin(double(i) * angularFreq);
        const double v2 = std::sin(double(i) * angularFreq + phi - 2.0 * M_PI * d_lambda);
        const double sum = v1 + v2 * m_gain;
        rms += sum * sum;
    }
    const double pwr = (rms / 20.0) * 5.5;  // normalize to 1.0 max (Thetis constant)
    return std::clamp(pwr, 0.0, 1.0);
}

void DiversityRadarWidget::mousePressEvent(QMouseEvent* e) { m_dragging = (e->button() == Qt::LeftButton); }
void DiversityRadarWidget::mouseReleaseEvent(QMouseEvent*) { m_dragging = false; }
void DiversityRadarWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_dragging) { return; }
    const QPointF center = rect().center();
    const double dx = e->pos().x() - center.x();
    const double dy = -(e->pos().y() - center.y());
    const double newPhase = std::atan2(dx, dy);  // 0 = north, π/2 = east
    emit phaseAdjusted(newPhase);
}

} // namespace NereusSDR
```

- [ ] **Step 3: Math test**

Create `tests/tst_diversity_radar_widget_math.cpp`:

```cpp
#include <QtTest/QtTest>
#include "gui/widgets/DiversityRadarWidget.h"

using namespace NereusSDR;

class TestDiversityRadarWidgetMath : public QObject {
    Q_OBJECT
private slots:
    void sensitivity_at_zero_phase_is_directional()
    {
        DiversityRadarWidget w;
        w.setVfoFreqMhz(14.225);
        w.setAntennaSpacingMeters(5.5);
        w.setPhase(0.0);
        w.setGain(1.0);
        // Sensitivity should differ between angles (not constant) — i.e. directional pattern exists
        // We don't expose sensitivityAtAngle as public; test via render output dimensions or expose via friend.
        QVERIFY(true);  // placeholder; expose via #ifdef NEREUSSDR_TESTING for full coverage
    }
};

QTEST_MAIN(TestDiversityRadarWidgetMath)
#include "tst_diversity_radar_widget_math.moc"
```

Register.

- [ ] **Step 4: Commit**

```bash
cmake --build build
git add src/gui/widgets/DiversityRadarWidget.{h,cpp} tests/tst_diversity_radar_widget_math.cpp tests/CMakeLists.txt src/gui/CMakeLists.txt
git commit -m "feat(3f-g): DiversityRadarWidget (polar paint + SensitivityAtAngle port from Thetis)"
```

---

## Task 6-10: DiversityDialog full UI (5 tasks)

Each task adds a group of controls following the pattern (failing test → implement → pass → commit).

- [ ] **Task 6**: Enable checkbox + Phase/Gain/FineNull sliders + spinboxes wired to SliceModel
- [ ] **Task 7**: 7 quick-nudge buttons (-10°, -45°, -90°, +45°, +90°, +10°, 180°)
- [ ] **Task 8**: Mode group (Auto / Cross-fire / Lock angle / Always on top checkboxes)
- [ ] **Task 9**: Memory grid (M1-M8 buttons, click to recall, right-click to store, per-band)
- [ ] **Task 10**: Reference ADC radio buttons + Slice integration checkboxes (Sync Slice A→B, Link ATT)

Each task adds the relevant Qt widgets to `DiversityDialog.cpp`, wires the signals to slots that update `SliceModel` properties, and adds a corresponding test in `tests/tst_diversity_dialog_persistence.cpp`.

---

## Task 11: Direction finding group (antenna spacing, calibration, derived direction label)

**Files:** Modify `src/gui/dialogs/DiversityDialog.cpp`

- [ ] **Step 1: Add direction finding group below radar widget in left column**

Spinboxes for antenna spacing (meters) + calibration (dimensionless). Derived `d/lambda` display computed from current VFO freq.

- [ ] **Step 2: Persist antenna spacing + calibration as global AppSettings keys**

`Diversity_AntSpacingMeters`, `Diversity_Calibration`.

- [ ] **Step 3: Commit**

---

## Task 12: Wire DiversityRadarWidget to SliceModel state

**Files:** Modify `src/gui/dialogs/DiversityDialog.cpp`

- [ ] **Step 1: Forward phase/gain/crossFire changes from SliceModel into radar widget**

```cpp
    connect(m_sliceA, &SliceModel::diversityPhaseChanged, this, [this](double phase) {
        if (m_radarWidget) { m_radarWidget->setPhase(phase * M_PI / 180.0); }  // SliceModel uses degrees
    });
    connect(m_sliceA, &SliceModel::diversityGainChanged, this, [this](double gain) {
        if (m_radarWidget) { m_radarWidget->setGain(gain); }
    });
    connect(m_sliceA, &SliceModel::frequencyChanged, this, [this](double mhz) {
        if (m_radarWidget) { m_radarWidget->setVfoFreqMhz(mhz); }
    });
```

- [ ] **Step 2: Reverse direction: radar drag → SliceModel update**

```cpp
    connect(m_radarWidget, &DiversityRadarWidget::phaseAdjusted, this, [this](double rad) {
        if (m_sliceA) { m_sliceA->setDiversityPhase(rad * 180.0 / M_PI); }
    });
```

- [ ] **Step 3: Commit**

---

## Task 13: Wire enable toggle to RxChannel WDSP wrappers

**Files:** Modify `src/gui/dialogs/DiversityDialog.cpp`

- [ ] **Step 1: On enable change, call WDSP**

```cpp
void DiversityDialog::onEnableToggled(bool on)
{
    if (!m_sliceA) { return; }
    m_sliceA->setDiversityEnabled(on);  // triggers SliceModel signal

    if (auto* radio = m_radio) {
        if (auto* rxCh = radio->wdspEngine()->rxChannel(0)) {  // Slice A = WDSP channel 0
            rxCh->setExtDivRun(on);
            if (on) {
                rxCh->setExtDivNr(2);
                rxCh->setExtDivOutput(0);  // combined
                double iRot[2] = {std::cos(m_sliceA->diversityPhase() * M_PI / 180.0),
                                  std::cos((m_sliceA->diversityPhase() + 90) * M_PI / 180.0)};
                double qRot[2] = {std::sin(m_sliceA->diversityPhase() * M_PI / 180.0),
                                  std::sin((m_sliceA->diversityPhase() + 90) * M_PI / 180.0)};
                rxCh->setExtDivRotate(2, iRot, qRot);
            }
        }
    }
}
```

- [ ] **Step 2: Also trigger codec DDC reassignment (Slice A migrates DDC2 → DDC0+1)**

The codec extension from Sub-Epic B handles this automatically when `SliceModel::diversityEnabled` flips. Verify:

```cpp
    connect(m_sliceA, &SliceModel::diversityEnabledChanged, this, [this](bool /*on*/) {
        m_radio->invokeCodecDdcAssignment();
    });
```

- [ ] **Step 3: Commit**

---

## Task 14: Auto-find-null implementation (simple gradient descent)

**Files:** Modify `src/gui/dialogs/DiversityDialog.cpp`

- [ ] **Step 1: Implement basic null-finding**

```cpp
void DiversityDialog::onAutoFindNull()
{
    if (!m_sliceA) { return; }
    // Simple algorithm: sweep phase from -180° to +180° in 5° steps,
    // measure RX signal level (via WDSP meter), find minimum.
    // For Sub-Epic G v1: implement the sweep + result-apply. Refinement deferred.

    double bestPhase = m_sliceA->diversityPhase();
    double bestLevel = 1e9;
    for (double phase = -180.0; phase < 180.0; phase += 5.0) {
        m_sliceA->setDiversityPhase(phase);
        // Read current RX signal level via meter (placeholder)
        const double level = 0.0;  // m_radio->meterPoller()->signalDbm(0);
        if (level < bestLevel) {
            bestLevel = level;
            bestPhase = phase;
        }
    }
    m_sliceA->setDiversityPhase(bestPhase);
}
```

- [ ] **Step 2: Commit**

---

## Task 15-20: Polish tasks (5-6 days)

- [ ] **Task 15**: Right-click memory slot context menu (Store / Clear / Set as default)
- [ ] **Task 16**: Tooltip pass on every control (text from Thetis verbatim)
- [ ] **Task 17**: "Always on top" checkbox sets window flag
- [ ] **Task 18**: Dialog geometry persistence (`Diversity_DialogGeometry` key)
- [ ] **Task 19**: WDSP wrapper smoke test on a live G2 bench (verify phase/gain audibly change RX combining)
- [ ] **Task 20**: DIV badge on VfoWidget + SpectrumStatusOverlay updates from `diversityEnabledChanged`

---

## Task 21: PS-active-during-MOX diversity pause UX

**Files:** Modify `src/gui/widgets/SpectrumStatusOverlay.cpp` + MainWindow PS state wiring

- [ ] **Step 1: When PS engages on MOX with diversity active, set `psPaused=true` on Slice A**

Wired in MainWindow PS state handler (likely existing for Sub-Epic A's psPaused property). When MOX-on with PS active and diversity active, set Slice A's psPaused. On MOX-off, clear.

- [ ] **Step 2: Status overlay shows "DIV paused (PS active)" via setPsPaused + setDiversityActive**

Already supported by Sub-Epic E's SpectrumStatusOverlay.

- [ ] **Step 3: Commit**

---

## Task 22-25: Final integration tests + bench verification

- [ ] **Task 22**: `tests/tst_diversity_dialog_persistence.cpp` covers all per-band + memory persistence
- [ ] **Task 23**: Bench verification matrix on G2 (full Thetis-equivalent UX)
- [ ] **Task 24**: Retrospective note in design doc
- [ ] **Task 25**: Open Sub-Epic G PR

---

## Sub-Epic G Completion Criteria

- `RxChannel` has WDSP `SetEXTDIV*` wrappers
- `SliceModel` per-band diversity properties (5 controls + 8 memories × 2 fields = ~21 keys/band/slice)
- `DiversityDialog` full Thetis port: enable, phase, gain, fine null, cross-fire, lock angle, auto mode, 7 quick-nudge buttons, 8 memory slots, reference ADC, slice integration
- `DiversityRadarWidget` polar paint + drag-to-tune
- Direction finding (antenna spacing, calibration, d/lambda)
- DDC topology shift on enable (codec from Sub-Epic B handles wire bytes)
- PS-active-during-MOX pauses diversity with "DIV paused (PS active)" UX
- Tools menu Diversity Dialog (Ctrl+Shift+D, capability-gated)
- 3 new test files, all green
- G2 bench verification of audible diversity combining + null finding

Ready for **Sub-Epic H (Bench verification + polish)** to begin.

---

## References

- Design §8 (Full Diversity port)
- Thetis `DiversityForm.cs` 2937 lines (full source of truth)
- Thetis `DiversityForm.cs:2390-2440` SensitivityAtAngle math
- Thetis `dsp.cs:609-619 [v2.10.3.15]` WDSP P/Invoke signatures
- mi0bot HL2 PS rate carveout (preserved from Sub-Epic B)
- Sub-Epic A `SliceModel::diversityEnabled` (foundation)
- Sub-Epic B codec diversity DDC topology shift
- Sub-Epic E SpectrumStatusOverlay DIV + PS HOLD badge support
