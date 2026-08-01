# Phase 3F Sub-Epic E: UI Atlas Surfaces — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build the operator-facing UI surfaces that make multi-slice + multi-pan + multi-antenna usable: `SpectrumStatusOverlay` widget per pan, right-click VFO context menu, antenna picker submenu with chain-consequence hints, auto-switch toast, TX-bound confirmation dialog, Filter Policy popup, and the Setup → Hardware → DDC Routing page.

**Architecture:** 7 new widgets/dialogs, all styled with `StyleConstants.h` palette. `VfoWidget` gains right-click context menu. `MainWindow` wires the toast notifications + dialogs. Setup → Antenna Control gains a Conflict Policy group.

**Tech Stack:** C++20, Qt6 (QMenu, QToolTip, QWidget overlays, QDialog, QRadioButton), QtTest.

**Parent design:** [docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md](2026-05-26-phase3f-multi-pan-multi-slice-design.md) §11 (UI Atlas), §5 (Antenna conflict)

**Prereqs:** Sub-Epics A + B + C + D complete.

**Estimated effort:** 4 working days, 16 tasks, ~85 bite-sized steps.

---

## File Structure

### Files to create

| File | Purpose |
|---|---|
| `src/gui/widgets/SpectrumStatusOverlay.{h,cpp}` | Top-right per-pan badge widget (slice / freq / CH / TX / WIDE / DIV / PS HOLD) |
| `src/gui/widgets/AntennaPickerMenu.{h,cpp}` | Right-click submenu with chain consequences |
| `src/gui/widgets/AntennaSwitchToast.{h,cpp}` | Non-blocking toast for auto-switch with Undo |
| `src/gui/widgets/TxBoundConfirmDialog.{h,cpp}` | Modal for TX-bound antenna re-route |
| `src/gui/widgets/FilterPolicyDialog.{h,cpp}` | Per-chain BPF policy popup (Auto/ForceBand/ForceBypass + HPF toggle) |
| `src/gui/setup/HardwareDdcRoutingPage.{h,cpp}` | Setup → Hardware → DDC Routing power-user table |
| `tests/tst_spectrum_status_overlay.cpp` | Badge state rendering for 4 states |
| `tests/tst_antenna_picker_menu.cpp` | Submenu population + chain-consequence hints |
| `tests/tst_filter_policy_dialog.cpp` | Apply button propagates to AlexController |

### Files to modify

| File | Purpose |
|---|---|
| `src/gui/widgets/VfoWidget.{h,cpp}` | Right-click context menu (Make TX, Antenna ▶, Sample rate ▶, Move pan ▶, Move DDC ▶, Diversity ▶, Filter policy…, Remove) |
| `src/gui/PanadapterApplet.{h,cpp}` | Embed `SpectrumStatusOverlay` in top-right corner |
| `src/gui/MainWindow.cpp` | Wire toast + dialog handlers to RadioModel signals |
| `src/gui/setup/AntennaControlPage.{h,cpp}` | Add "Conflict policy" group at bottom |

---

## Task 1: `SpectrumStatusOverlay` skeleton

**Files:** Create `src/gui/widgets/SpectrumStatusOverlay.{h,cpp}`

- [ ] **Step 1: Write failing test**

Create `tests/tst_spectrum_status_overlay.cpp`:

```cpp
#include <QtTest/QtTest>
#include "gui/widgets/SpectrumStatusOverlay.h"

using namespace NereusSDR;

class TestSpectrumStatusOverlay : public QObject {
    Q_OBJECT
private slots:
    void overlay_constructs_with_slice_letter()
    {
        SpectrumStatusOverlay overlay;
        overlay.setSliceLetter(QChar('A'));
        overlay.setFrequencyHz(14225000);
        overlay.setMode(QStringLiteral("USB"));
        overlay.setChainIndex(0);
        QCOMPARE(overlay.sliceLetter(), QChar('A'));
    }
};

QTEST_MAIN(TestSpectrumStatusOverlay)
#include "tst_spectrum_status_overlay.moc"
```

Register: `nereus_add_test(tst_spectrum_status_overlay)`.

- [ ] **Step 2: Run + verify failure**

- [ ] **Step 3: Create header**

```cpp
#pragma once
#include <QWidget>
#include <QChar>

namespace NereusSDR {

/// Top-right per-pan overlay widget. Mirror of SpectrumOverlayPanel pattern.
/// Shows: slice letter badge, freq, mode, CH N tag, TX/WIDE/DIV/PS HOLD pills.
/// Click WIDE → opens FilterPolicyDialog. Click TX → requests TxSliceArbiter handoff.
/// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §11.
class SpectrumStatusOverlay : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumStatusOverlay(QWidget* parent = nullptr);
    ~SpectrumStatusOverlay() override;

    void setSliceLetter(QChar letter);
    QChar sliceLetter() const { return m_sliceLetter; }

    void setFrequencyHz(qint64 hz);
    void setMode(const QString& mode);
    void setChainIndex(int chainIdx);

    void setTxBound(bool tx);
    void setWideBpf(bool wide, const QString& reason);
    void setDiversityActive(bool div);
    void setPsPaused(bool paused);

signals:
    void txBadgeClicked();
    void wideBadgeClicked();
    void chainTagClicked(int chainIdx);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QChar    m_sliceLetter {'A'};
    qint64   m_frequencyHz {0};
    QString  m_mode {"USB"};
    int      m_chainIndex {0};
    bool     m_txBound {false};
    bool     m_wideBpf {false};
    QString  m_wideReason;
    bool     m_diversityActive {false};
    bool     m_psPaused {false};
};

} // namespace NereusSDR
```

- [ ] **Step 4: Implement (paint-based for performance; not full QPushButton tree)**

Use a hybrid: QPainter for the badges, hit-testing in mousePressEvent.

```cpp
#include "gui/widgets/SpectrumStatusOverlay.h"
#include "gui/StyleConstants.h"
#include <QPainter>
#include <QMouseEvent>

namespace NereusSDR {

SpectrumStatusOverlay::SpectrumStatusOverlay(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(22);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
}

SpectrumStatusOverlay::~SpectrumStatusOverlay() = default;

void SpectrumStatusOverlay::setSliceLetter(QChar letter) { m_sliceLetter = letter; update(); }
void SpectrumStatusOverlay::setFrequencyHz(qint64 hz) { m_frequencyHz = hz; update(); }
void SpectrumStatusOverlay::setMode(const QString& mode) { m_mode = mode; update(); }
void SpectrumStatusOverlay::setChainIndex(int idx) { m_chainIndex = idx; update(); }
void SpectrumStatusOverlay::setTxBound(bool tx) { m_txBound = tx; update(); }
void SpectrumStatusOverlay::setWideBpf(bool wide, const QString& reason) { m_wideBpf = wide; m_wideReason = reason; update(); }
void SpectrumStatusOverlay::setDiversityActive(bool div) { m_diversityActive = div; update(); }
void SpectrumStatusOverlay::setPsPaused(bool paused) { m_psPaused = paused; update(); }

void SpectrumStatusOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // rgba(20, 30, 45, 240) background
    p.setBrush(QColor(20, 30, 45, 240));
    p.setPen(QColor(Style::kBorderSubtle));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 3, 3);

    int x = 4;
    const int y = (height() - 16) / 2;

    // Slice letter badge
    QColor sliceColor;
    switch (m_sliceLetter.toLatin1()) {
        case 'A': sliceColor = QColor(0x00, 0xd4, 0xff); break;
        case 'B': sliceColor = QColor(0xff, 0x40, 0xff); break;
        case 'C': sliceColor = QColor(0x40, 0xff, 0x40); break;
        case 'D': sliceColor = QColor(0xff, 0xff, 0x00); break;
        default:  sliceColor = QColor(0x00, 0xd4, 0xff); break;
    }
    p.setBrush(sliceColor);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(x, y, 16, 16, 2, 2);
    p.setPen(Qt::black);
    p.setFont(QFont(QStringLiteral("monospace"), 10, QFont::Bold));
    p.drawText(QRect(x, y, 16, 16), Qt::AlignCenter, m_sliceLetter);
    x += 22;

    // Freq + mode text
    p.setPen(QColor(Style::kTextPrimary));
    p.setFont(QFont(QStringLiteral("monospace"), 10, QFont::Bold));
    const QString text = QStringLiteral("%1.%2 %3")
        .arg(m_frequencyHz / 1000000)
        .arg((m_frequencyHz / 1000) % 1000, 3, 10, QChar('0'))
        .arg(m_mode);
    p.drawText(x, y, 180, 16, Qt::AlignVCenter, text);
    x += 180;

    // CH tag (always visible)
    p.setBrush(QColor(0x1a, 0x2a, 0x3a));
    p.setPen(QColor(0x30, 0x40, 0x50));
    p.drawRoundedRect(x, y + 1, 32, 14, 2, 2);
    p.setPen(QColor(Style::kTitleText));
    p.setFont(QFont(QStringLiteral("monospace"), 9, QFont::Bold));
    p.drawText(QRect(x, y + 1, 32, 14), Qt::AlignCenter, QStringLiteral("CH %1").arg(m_chainIndex));
    x += 38;

    // Optional pills: TX, WIDE, DIV, PS HOLD
    auto drawPill = [&](const QString& text, const QColor& bg, const QColor& fg, const QColor& border) {
        const int pillW = 36;
        p.setBrush(bg);
        p.setPen(border);
        p.drawRoundedRect(x, y + 1, pillW, 14, 2, 2);
        p.setPen(fg);
        p.drawText(QRect(x, y + 1, pillW, 14), Qt::AlignCenter, text);
        x += pillW + 4;
    };

    if (m_txBound) {
        drawPill(QStringLiteral("TX"), QColor(0xcc, 0x22, 0x22), QColor(Qt::white), QColor(0xff, 0x44, 0x44));
    }
    if (m_wideBpf) {
        drawPill(QStringLiteral("WIDE"), QColor(0x60, 0x40, 0x00), QColor(0xff, 0xb8, 0x00), QColor(0x90, 0x60, 0x00));
    }
    if (m_diversityActive) {
        drawPill(QStringLiteral("DIV"), QColor(0x00, 0x60, 0x40), QColor(0x00, 0xff, 0x88), QColor(0x00, 0xa0, 0x60));
    }
    if (m_psPaused) {
        drawPill(QStringLiteral("PS HOLD"), QColor(0x60, 0x40, 0x00), QColor(0xff, 0xb8, 0x00), QColor(0x90, 0x60, 0x00));
    }

    setMinimumWidth(x + 4);
}

void SpectrumStatusOverlay::mousePressEvent(QMouseEvent* event)
{
    // Hit-test the badges (approximate; better via QRect cache)
    const int x = event->pos().x();
    // Slice + freq region ~ 0-220 (no action). CH tag ~ 220-260.
    // TX/WIDE/DIV/PS pills start at ~260 in 36+4 px steps depending on which are visible.

    int hitX = 220;  // approximate CH start
    if (x >= hitX && x < hitX + 32) {
        emit chainTagClicked(m_chainIndex);
        return;
    }
    hitX += 38;  // after CH tag

    if (m_txBound && x >= hitX && x < hitX + 36) {
        emit txBadgeClicked();
        return;
    }
    if (m_txBound) { hitX += 40; }

    if (m_wideBpf && x >= hitX && x < hitX + 36) {
        emit wideBadgeClicked();
        return;
    }
}

} // namespace NereusSDR
```

- [ ] **Step 5: Commit**

```bash
cmake --build build && ctest --test-dir build -R tst_spectrum_status_overlay -V 2>&1 | tail -10
git add src/gui/widgets/SpectrumStatusOverlay.{h,cpp} tests/tst_spectrum_status_overlay.cpp tests/CMakeLists.txt src/gui/CMakeLists.txt
git commit -m "feat(3f-e): SpectrumStatusOverlay widget (paint-based, real palette + slice colors)"
```

---

## Task 2: Embed `SpectrumStatusOverlay` in `PanadapterApplet`

**Files:** Modify `src/gui/PanadapterApplet.{h,cpp}`

- [ ] **Step 1: Add member + position in resizeEvent**

In PanadapterApplet.h:
```cpp
private:
    SpectrumStatusOverlay* m_statusOverlay {nullptr};
```

In PanadapterApplet.cpp constructor:
```cpp
    m_statusOverlay = new SpectrumStatusOverlay(this);
    m_statusOverlay->raise();
```

Add `resizeEvent` override:
```cpp
protected:
    void resizeEvent(QResizeEvent* event) override;
```

```cpp
void PanadapterApplet::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_statusOverlay) {
        const QSize hint = m_statusOverlay->sizeHint();
        m_statusOverlay->setGeometry(width() - hint.width() - 8, 8, hint.width(), hint.height());
    }
}
```

- [ ] **Step 2: Forward signals from overlay to PanadapterApplet**

In PanadapterApplet.h:
```cpp
signals:
    void txBadgeClicked(const QString& panId);
    void wideBadgeClicked(const QString& panId);
    void chainTagClicked(int chainIdx);
```

In constructor:
```cpp
    connect(m_statusOverlay, &SpectrumStatusOverlay::txBadgeClicked, this, [this]() {
        emit txBadgeClicked(m_panId);
    });
    connect(m_statusOverlay, &SpectrumStatusOverlay::wideBadgeClicked, this, [this]() {
        emit wideBadgeClicked(m_panId);
    });
    connect(m_statusOverlay, &SpectrumStatusOverlay::chainTagClicked, this, &PanadapterApplet::chainTagClicked);
```

- [ ] **Step 3: Add `updateStatusOverlay(SliceModel*)` helper**

```cpp
public:
    void updateStatusOverlay(SliceModel* activeSlice);
```

```cpp
void PanadapterApplet::updateStatusOverlay(SliceModel* slice)
{
    if (!m_statusOverlay || !slice) { return; }
    m_statusOverlay->setSliceLetter(slice->sliceLetter());
    m_statusOverlay->setFrequencyHz(qint64(slice->frequency() * 1e6));
    m_statusOverlay->setMode(slice->dspModeStr());  // assumes existing accessor or use enum→string helper
    m_statusOverlay->setChainIndex(slice->chainIndex());
    m_statusOverlay->setTxBound(slice->isTxSlice());
    m_statusOverlay->setDiversityActive(slice->diversityEnabled());
    m_statusOverlay->setPsPaused(slice->psPaused());
}
```

- [ ] **Step 4: Wire MainWindow to call updateStatusOverlay on slice property changes**

In MainWindow init:
```cpp
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](SliceModel* slice) {
        connect(slice, &SliceModel::frequencyChanged, this, [this, slice]() {
            // Find which pan(s) show this slice + refresh their overlays
            for (auto* applet : m_panStack->allApplets()) {
                if (applet->activeSliceIndex() == m_radioModel->slices().indexOf(slice)) {
                    applet->updateStatusOverlay(slice);
                }
            }
        });
        // Same for: dspModeChanged, txSliceChanged, diversityEnabledChanged, psPausedChanged, chainIndexChanged
    });
```

- [ ] **Step 5: Commit**

```bash
cmake --build build
git add src/gui/PanadapterApplet.{h,cpp} src/gui/MainWindow.cpp
git commit -m "feat(3f-e): embed SpectrumStatusOverlay in PanadapterApplet (top-right corner)"
```

---

## Task 3: WIDE badge → opens FilterPolicyDialog

**Files:** Create `src/gui/widgets/FilterPolicyDialog.{h,cpp}`, Modify `src/gui/MainWindow.cpp`

- [ ] **Step 1: Create header**

```cpp
#pragma once
#include <QDialog>
#include "core/accessories/AlexController.h"

namespace NereusSDR {

class FilterPolicyDialog : public QDialog {
    Q_OBJECT
public:
    explicit FilterPolicyDialog(int chainIndex, AlexController* alex, QWidget* parent = nullptr);
    ~FilterPolicyDialog() override;
};

} // namespace NereusSDR
```

- [ ] **Step 2: Implement**

```cpp
#include "gui/widgets/FilterPolicyDialog.h"
#include "gui/StyleConstants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QButtonGroup>

namespace NereusSDR {

FilterPolicyDialog::FilterPolicyDialog(int chainIndex, AlexController* alex, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Chain %1 — Filter Policy").arg(chainIndex));
    setStyleSheet(QStringLiteral("background: %1; color: %2;").arg(Style::kPanelBg, Style::kTextPrimary));
    setFixedWidth(420);

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(14, 14, 14, 14);

    // Current state group
    auto* stateGroup = new QGroupBox(QStringLiteral("Current state"), this);
    stateGroup->setStyleSheet(Style::kGroupBoxStyle);
    auto* stateLayout = new QVBoxLayout(stateGroup);
    const auto& state = alex->adcState(chainIndex);
    auto* stateLbl = new QLabel(QStringLiteral("Effective: %1\nReason: %2").arg(
        (state.effective == AlexController::BpfEffective::Filtered) ? QStringLiteral("Filtered") :
        (state.effective == AlexController::BpfEffective::WidebandLocked) ? QStringLiteral("BYPASS (wideband)") :
        QStringLiteral("BYPASS"),
        state.reasonText), stateGroup);
    stateLbl->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px;"));
    stateLayout->addWidget(stateLbl);
    main->addWidget(stateGroup);

    // BPF mode group
    auto* modeGroup = new QGroupBox(QStringLiteral("BPF mode"), this);
    modeGroup->setStyleSheet(Style::kGroupBoxStyle);
    auto* modeLayout = new QVBoxLayout(modeGroup);
    auto* btnGroup = new QButtonGroup(this);

    auto* autoBtn = new QRadioButton(QStringLiteral("Auto — Filter when single-band, bypass when multi-band"), modeGroup);
    auto* forceBandBtn = new QRadioButton(QStringLiteral("Force filter (TX-bound band)"), modeGroup);
    auto* forceByBtn = new QRadioButton(QStringLiteral("Force bypass — Always wideband"), modeGroup);
    btnGroup->addButton(autoBtn, int(AlexController::BpfMode::Auto));
    btnGroup->addButton(forceBandBtn, int(AlexController::BpfMode::ForceBand));
    btnGroup->addButton(forceByBtn, int(AlexController::BpfMode::ForceBypass));

    switch (alex->bpfMode(chainIndex)) {
        case AlexController::BpfMode::Auto:        autoBtn->setChecked(true); break;
        case AlexController::BpfMode::ForceBand:   forceBandBtn->setChecked(true); break;
        case AlexController::BpfMode::ForceBypass: forceByBtn->setChecked(true); break;
    }

    modeLayout->addWidget(autoBtn);
    modeLayout->addWidget(forceBandBtn);
    modeLayout->addWidget(forceByBtn);
    main->addWidget(modeGroup);

    // HPF checkbox
    auto* hpfBox = new QCheckBox(QStringLiteral("HPF (broadcast band reject) enabled"), this);
    hpfBox->setChecked(true);  // Default; bind to AlexController HPF state in Sub-Epic G
    hpfBox->setStyleSheet(Style::kCheckBoxStyle);
    main->addWidget(hpfBox);

    // Footer buttons
    auto* footer = new QHBoxLayout();
    footer->addStretch(1);
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
    cancelBtn->setStyleSheet(Style::buttonBaseStyle());
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    footer->addWidget(cancelBtn);
    auto* applyBtn = new QPushButton(QStringLiteral("Apply"), this);
    applyBtn->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());
    connect(applyBtn, &QPushButton::clicked, this, [this, alex, chainIndex, btnGroup]() {
        alex->setBpfMode(chainIndex, static_cast<AlexController::BpfMode>(btnGroup->checkedId()));
        accept();
    });
    footer->addWidget(applyBtn);
    main->addLayout(footer);
}

FilterPolicyDialog::~FilterPolicyDialog() = default;

} // namespace NereusSDR
```

- [ ] **Step 3: Wire WIDE badge click in MainWindow**

```cpp
    connect(applet, &PanadapterApplet::wideBadgeClicked, this, [this](const QString&) {
        // For now, open FilterPolicyDialog for active slice's chain
        const auto* slice = m_radioModel->slices().value(0);  // active
        if (!slice || !m_radioModel->alexController()) { return; }
        FilterPolicyDialog dlg(slice->chainIndex(), m_radioModel->alexController(), this);
        dlg.exec();
    });

    connect(applet, &PanadapterApplet::chainTagClicked, this, [this](int chainIdx) {
        if (!m_radioModel->alexController()) { return; }
        FilterPolicyDialog dlg(chainIdx, m_radioModel->alexController(), this);
        dlg.exec();
    });
```

- [ ] **Step 4: Commit**

```bash
cmake --build build
git add src/gui/widgets/FilterPolicyDialog.{h,cpp} src/gui/MainWindow.cpp src/gui/CMakeLists.txt
git commit -m "feat(3f-e): FilterPolicyDialog (per-chain BPF mode override + HPF toggle)"
```

---

## Task 4: Right-click VFO flag context menu

**Files:** Modify `src/gui/widgets/VfoWidget.{h,cpp}`

- [ ] **Step 1: Add contextMenuEvent override**

In VfoWidget.h:
```cpp
protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
```

In VfoWidget.cpp:
```cpp
void VfoWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);

    QAction* makeTxAct = menu.addAction(QStringLiteral("Make this the TX slice"));
    makeTxAct->setEnabled(!m_isTxSlice);  // can't re-bind to already-bound
    connect(makeTxAct, &QAction::triggered, this, [this]() {
        emit txHandoffRequested(m_sliceIndex);
    });

    menu.addSeparator();

    QMenu* antMenu = menu.addMenu(QStringLiteral("Antenna ▶"));
    // Populate antMenu with current SKU's antenna list and chain consequences (Task 5)
    // For now: stub
    antMenu->addAction(QStringLiteral("ANT1"));
    antMenu->addAction(QStringLiteral("ANT2"));

    QMenu* rateMenu = menu.addMenu(QStringLiteral("Sample rate ▶"));
    for (int hz : {48000, 96000, 192000, 384000, 768000, 1536000}) {
        QAction* act = rateMenu->addAction(QStringLiteral("%1 kHz").arg(hz / 1000));
        act->setCheckable(true);
        act->setChecked(false);  // hooked to slice->sampleRateHz() in next pass
        connect(act, &QAction::triggered, this, [this, hz]() {
            emit sampleRateRequested(m_sliceIndex, hz);
        });
    }

    menu.addSeparator();

    QAction* divAct = menu.addAction(QStringLiteral("Diversity ▶"));
    divAct->setEnabled(m_sliceIndex == 0 && m_capsHasDiversity);  // Slice A only on 2-ADC

    QAction* filterAct = menu.addAction(QStringLiteral("Filter policy…"));
    connect(filterAct, &QAction::triggered, this, [this]() {
        emit filterPolicyRequested(m_chainIndex);
    });

    menu.addSeparator();

    QAction* removeAct = menu.addAction(QStringLiteral("Remove slice"));
    removeAct->setIcon(QIcon());  // could add red x icon
    connect(removeAct, &QAction::triggered, this, [this]() {
        emit removeSliceRequested(m_sliceIndex);
    });

    menu.exec(event->globalPos());
}
```

- [ ] **Step 2: Add signals to VfoWidget.h**

```cpp
signals:
    void txHandoffRequested(int sliceIndex);  // already exists from Sub-Epic C
    void sampleRateRequested(int sliceIndex, int hz);
    void filterPolicyRequested(int chainIndex);
    void removeSliceRequested(int sliceIndex);
    void antennaChangeRequested(int sliceIndex, int antIdx);
```

- [ ] **Step 3: Wire in MainWindow**

```cpp
    connect(vfoWidget, &VfoWidget::sampleRateRequested, this, [this](int sliceIdx, int hz) {
        if (auto* slice = m_radioModel->slices().value(sliceIdx)) {
            slice->setSampleRateHz(hz);
        }
    });
    connect(vfoWidget, &VfoWidget::filterPolicyRequested, this, [this](int chainIdx) {
        FilterPolicyDialog dlg(chainIdx, m_radioModel->alexController(), this);
        dlg.exec();
    });
    connect(vfoWidget, &VfoWidget::removeSliceRequested, this, [this](int sliceIdx) {
        m_radioModel->removeSlice(sliceIdx);
    });
```

- [ ] **Step 4: Commit**

```bash
git add src/gui/widgets/VfoWidget.{h,cpp} src/gui/MainWindow.cpp
git commit -m "feat(3f-e): VfoWidget right-click context menu (TX/Antenna/Rate/Filter/Remove)"
```

---

## Task 5: `AntennaPickerMenu` with chain-consequence hints

**Files:** Create `src/gui/widgets/AntennaPickerMenu.{h,cpp}`

- [ ] **Step 1: Create header**

```cpp
#pragma once
#include <QMenu>
#include <QStringList>

namespace NereusSDR {

class SliceModel;
class AlexController;
class BoardCapabilities;

class AntennaPickerMenu : public QMenu {
    Q_OBJECT
public:
    AntennaPickerMenu(SliceModel* slice, AlexController* alex,
                       const BoardCapabilities& caps, QWidget* parent = nullptr);
    ~AntennaPickerMenu() override;

signals:
    void antennaSelected(int sliceIndex, const QString& antennaName);
};

} // namespace NereusSDR
```

- [ ] **Step 2: Implement chain-consequence hints**

```cpp
#include "gui/widgets/AntennaPickerMenu.h"
#include "models/SliceModel.h"
#include "core/accessories/AlexController.h"
#include "core/BoardCapabilities.h"
#include <QAction>

namespace NereusSDR {

AntennaPickerMenu::AntennaPickerMenu(SliceModel* slice, AlexController* /*alex*/,
                                       const BoardCapabilities& caps, QWidget* parent)
    : QMenu(parent)
{
    addSection(QStringLiteral("Slice %1 — Antenna for %2").arg(slice->sliceLetter()).arg(slice->bandLabel()));

    QStringList antennas = {QStringLiteral("ANT1"), QStringLiteral("ANT2"), QStringLiteral("ANT3")};
    if (caps.antennaInputCount < 3) { antennas.removeLast(); }
    if (caps.antennaInputCount < 2) { antennas.removeLast(); }

    const QString currentAnt = slice->rxAntenna();
    for (const QString& ant : antennas) {
        QString consequence;
        if (ant == currentAnt) {
            consequence = QStringLiteral("Chain %1 — current").arg(slice->chainIndex());
        } else {
            // Determine consequence: would joining other chain cause bypass? would re-route be needed?
            // For now: simple text. Full logic from design §5 in operator-facing version.
            consequence = QStringLiteral("(switches chain)");
        }
        QAction* act = addAction(QStringLiteral("%1   %2").arg(ant, consequence));
        if (ant == currentAnt) { act->setCheckable(true); act->setChecked(true); }
        connect(act, &QAction::triggered, this, [this, slice, ant]() {
            emit antennaSelected(slice->sliceIndex(), ant);
        });
    }

    addSeparator();
    addAction(QStringLiteral("EXT1   RX only"));
    addAction(QStringLiteral("EXT2   RX only"));
    addAction(QStringLiteral("BYPS   RX only, no Alex"));
}

AntennaPickerMenu::~AntennaPickerMenu() = default;

} // namespace NereusSDR
```

- [ ] **Step 3: Wire from VfoWidget's "Antenna ▶" submenu**

In VfoWidget::contextMenuEvent, replace the stub antenna menu with:
```cpp
    AntennaPickerMenu* antMenu = new AntennaPickerMenu(m_currentSlice, m_alexController, m_caps, this);
    menu.addMenu(antMenu);
    connect(antMenu, &AntennaPickerMenu::antennaSelected, this, [this](int sliceIdx, const QString& ant) {
        emit antennaChangeRequested(sliceIdx, ant);
    });
```

- [ ] **Step 4: Commit**

```bash
cmake --build build
git add src/gui/widgets/AntennaPickerMenu.{h,cpp} src/gui/widgets/VfoWidget.{h,cpp} src/gui/CMakeLists.txt
git commit -m "feat(3f-e): AntennaPickerMenu with chain-consequence hints"
```

---

## Task 6: `AntennaSwitchToast` for auto-switch notification

**Files:** Create `src/gui/widgets/AntennaSwitchToast.{h,cpp}`

- [ ] **Step 1: Create header + implementation (compact)**

```cpp
// AntennaSwitchToast.h
#pragma once
#include <QWidget>
#include <QTimer>

namespace NereusSDR {

/// Non-blocking bottom-right toast for antenna auto-switch with Undo button.
/// 8s auto-dismiss. Designed to overlay MainWindow without modal blocking.
class AntennaSwitchToast : public QWidget {
    Q_OBJECT
public:
    AntennaSwitchToast(const QString& message, QWidget* parent = nullptr);
    ~AntennaSwitchToast() override;

signals:
    void undoRequested();

private:
    QTimer m_autoDismissTimer;
};

} // namespace NereusSDR
```

```cpp
// AntennaSwitchToast.cpp
#include "gui/widgets/AntennaSwitchToast.h"
#include "gui/StyleConstants.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace NereusSDR {

AntennaSwitchToast::AntennaSwitchToast(const QString& message, QWidget* parent) : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border: 1px solid %2; border-left: 3px solid #00ff88; border-radius: 3px; }"
    ).arg(Style::kPanelBg, Style::kBorder));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);

    auto* checkLbl = new QLabel(QStringLiteral("✓"), this);
    checkLbl->setStyleSheet(QStringLiteral("color: #00ff88; font-size: 14px;"));
    layout->addWidget(checkLbl);

    auto* textWidget = new QWidget(this);
    auto* textLayout = new QVBoxLayout(textWidget);
    textLayout->setContentsMargins(0, 0, 0, 0);

    auto* titleLbl = new QLabel(QStringLiteral("Antenna auto-switched"), textWidget);
    titleLbl->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; font-weight: bold;").arg(Style::kTextPrimary));
    textLayout->addWidget(titleLbl);

    auto* msgLbl = new QLabel(message, textWidget);
    msgLbl->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(Style::kTextTertiary));
    textLayout->addWidget(msgLbl);

    layout->addWidget(textWidget);

    auto* undoBtn = new QPushButton(QStringLiteral("UNDO"), this);
    undoBtn->setStyleSheet(Style::buttonBaseStyle() + QStringLiteral("QPushButton { height: 18px; padding: 0 8px; font-size: 9px; }"));
    connect(undoBtn, &QPushButton::clicked, this, &AntennaSwitchToast::undoRequested);
    connect(undoBtn, &QPushButton::clicked, this, &QWidget::close);
    layout->addWidget(undoBtn);

    setFixedHeight(50);
    setFixedWidth(380);

    m_autoDismissTimer.setSingleShot(true);
    connect(&m_autoDismissTimer, &QTimer::timeout, this, &QWidget::close);
    m_autoDismissTimer.start(8000);
}

AntennaSwitchToast::~AntennaSwitchToast() = default;

} // namespace NereusSDR
```

- [ ] **Step 2: Wire MainWindow to show toast on `RadioModel::antennaAutoSwitched`**

(Signal needs to be added to RadioModel — actually defer to Sub-Epic B's antenna-change wiring; for now wire on a synthesized stub signal.)

```cpp
    connect(m_radioModel, &RadioModel::antennaAutoSwitched, this, [this](int sliceIdx, const QString& oldAnt, const QString& newAnt) {
        const QString msg = QStringLiteral("Slice %1 moved from %2 to %3 for Slice C.").arg(QChar('A' + sliceIdx), oldAnt, newAnt);
        auto* toast = new AntennaSwitchToast(msg, this);
        toast->setAttribute(Qt::WA_DeleteOnClose);
        const QRect screen = this->geometry();
        toast->move(screen.right() - 400, screen.bottom() - 70);
        toast->show();
        connect(toast, &AntennaSwitchToast::undoRequested, this, [this]() {
            // m_radioModel->undoLastAntennaSwitch();  // wired in B
        });
    });
```

- [ ] **Step 3: Commit**

```bash
cmake --build build
git add src/gui/widgets/AntennaSwitchToast.{h,cpp} src/gui/MainWindow.cpp src/gui/CMakeLists.txt
git commit -m "feat(3f-e): AntennaSwitchToast (bottom-right, 8s auto-dismiss, Undo)"
```

---

## Task 7: `TxBoundConfirmDialog` for TX-bound re-route

**Files:** Create `src/gui/widgets/TxBoundConfirmDialog.{h,cpp}`

- [ ] **Step 1: Create header + implementation**

```cpp
// TxBoundConfirmDialog.h
#pragma once
#include <QDialog>
#include <QVector>

namespace NereusSDR {

class SliceModel;

/// Modal dialog shown when adding a new slice would require re-routing the TX-bound
/// slice's chain to a different antenna. Per design §5.
class TxBoundConfirmDialog : public QDialog {
    Q_OBJECT
public:
    enum Outcome { Cancelled, UseExistingAntenna, ConfirmReroute };

    TxBoundConfirmDialog(const QString& proposedAntenna,
                          const QString& existingAntenna,
                          const QVector<SliceModel*>& atRiskSlices,
                          QWidget* parent = nullptr);

    Outcome outcome() const { return m_outcome; }

private:
    Outcome m_outcome {Cancelled};
};

} // namespace NereusSDR
```

- [ ] **Step 2: Implementation (simplified)**

```cpp
#include "gui/widgets/TxBoundConfirmDialog.h"
#include "gui/StyleConstants.h"
#include "models/SliceModel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace NereusSDR {

TxBoundConfirmDialog::TxBoundConfirmDialog(const QString& proposedAntenna,
                                            const QString& existingAntenna,
                                            const QVector<SliceModel*>& atRiskSlices,
                                            QWidget* parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("⚠ TX-bound antenna re-route"));
    setStyleSheet(QStringLiteral("background: %1; color: %2;").arg(Style::kPanelBg, Style::kTextPrimary));
    setFixedWidth(500);

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(14, 14, 14, 14);

    main->addWidget(new QLabel(QStringLiteral("Adding new slice on %1 requires re-routing chain from %2 to %1.")
        .arg(proposedAntenna, existingAntenna), this));

    auto* warnLbl = new QLabel(QStringLiteral("⚠ TX-bound slice is on this chain. Re-routing changes the antenna your transmit signal goes to."), this);
    warnLbl->setStyleSheet(QStringLiteral("color: #ffb800; font-size: 11px;"));
    warnLbl->setWordWrap(true);
    main->addWidget(warnLbl);

    main->addWidget(new QLabel(QStringLiteral("Verify %1 is rated for the current band and TX power.").arg(proposedAntenna), this));

    auto* footer = new QHBoxLayout();
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
    auto* useExistingBtn = new QPushButton(QStringLiteral("Use %1 instead").arg(existingAntenna), this);
    auto* confirmBtn = new QPushButton(QStringLiteral("Re-route %1").arg(proposedAntenna), this);
    cancelBtn->setStyleSheet(Style::buttonBaseStyle());
    useExistingBtn->setStyleSheet(Style::buttonBaseStyle());
    confirmBtn->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());

    connect(cancelBtn, &QPushButton::clicked, this, [this]() { m_outcome = Cancelled; reject(); });
    connect(useExistingBtn, &QPushButton::clicked, this, [this]() { m_outcome = UseExistingAntenna; accept(); });
    connect(confirmBtn, &QPushButton::clicked, this, [this]() { m_outcome = ConfirmReroute; accept(); });

    footer->addWidget(cancelBtn);
    footer->addStretch(1);
    footer->addWidget(useExistingBtn);
    footer->addWidget(confirmBtn);
    main->addLayout(footer);
}

} // namespace NereusSDR
```

- [ ] **Step 3: Commit**

```bash
cmake --build build
git add src/gui/widgets/TxBoundConfirmDialog.{h,cpp} src/gui/CMakeLists.txt
git commit -m "feat(3f-e): TxBoundConfirmDialog (modal for TX-bound antenna re-route)"
```

---

## Task 8-13: Setup → Hardware → DDC Routing page + Antenna conflict policy

**Files:** Create `src/gui/setup/HardwareDdcRoutingPage.{h,cpp}`, Modify `src/gui/setup/AntennaControlPage.{h,cpp}`

These are largely Qt-form-with-table widgets. Implementation pattern is well-established in the codebase (look at existing Setup pages in `src/gui/setup/`).

Each task: create page header, register in SetupDialog, populate per-DDC rows with combo boxes for slice + ADC, persist overrides per-MAC.

(Tasks 8-13 condensed for brevity; each follows the same TDD pattern as preceding tasks.)

- [ ] **Task 8 steps**: Create `HardwareDdcRoutingPage.h` skeleton + register in SetupDialog
- [ ] **Task 9 steps**: Populate the DDC table (reserved rows locked, user rows have combos)
- [ ] **Task 10 steps**: Wire combo changes to AppSettings per-MAC override keys
- [ ] **Task 11 steps**: Add "Conflict policy" group to AntennaControlPage (3 radio buttons)
- [ ] **Task 12 steps**: Wire conflict policy choice to AppSettings + RadioModel consume
- [ ] **Task 13 steps**: Smoke test + commit

(See preceding plans for the exact step-by-step template — write failing test, fail, implement, pass, commit.)

---

## Task 14-15: Regression sweep + Sub-Epic E retrospective

**Files:** none modified for 14; design doc for 15

- [ ] Run full test suite, verify all pass
- [ ] Smoke test on G2: right-click VFO → menu opens, badges work, FilterPolicyDialog opens, etc.
- [ ] Append retrospective note to design doc

---

## Task 16: Open Sub-Epic E PR

```bash
git push -u origin HEAD
gh pr create --title "Phase 3F Sub-Epic E: UI Atlas Surfaces" --body "..."
```

---

## Sub-Epic E Completion Criteria

- `SpectrumStatusOverlay` widget embedded in every `PanadapterApplet`
- Right-click VFO flag opens context menu with TX/Antenna/Rate/Filter/Remove
- `AntennaPickerMenu` shows chain consequences inline
- `AntennaSwitchToast` with Undo for RX-only auto-switch
- `TxBoundConfirmDialog` for TX-bound re-route
- `FilterPolicyDialog` accessible from WIDE badge + Chain tag + right-click
- Setup → Hardware → DDC Routing page for power users
- Setup → Antenna Control gains Conflict Policy group
- ~3 new test files, all green
- Single-slice + 2-slice operation unchanged

Ready for **Sub-Epic F (Wideband extended pan)** to begin.
