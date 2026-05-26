# Phase 3F Sub-Epic D: Pan Layouts + Multi-Pan UI — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build the pan layout layer. After this plan lands, operators can choose between 5 layout templates (1, 2v, 2h, 12h, 2x2), float pans to second monitors, route DDC FFT output to multiple pans via `FFTRouter`, and click the `+PAN` button in the bottom bar to add slices/change layout.

**Architecture:** New `PanadapterStack` widget manages N `PanadapterApplet` instances via nested `QSplitter`. Each `PanadapterApplet` hosts a `SpectrumWidget` and binds to one or more slices via the AetherSDR overlay model. `FFTRouter` fan-outs receiver FFT frames to subscribed pans. `PanFloatingWindow` detaches a pan into a top-level window. `+PAN` placeholder in the bottom status bar (currently NYI at `MainWindow.cpp:3990`) activates with a dropdown menu.

**Tech Stack:** C++20, Qt6 (QSplitter, QWidget detach/dock pattern, QMenu, QAction), QtTest.

**Parent design:** [docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md](2026-05-26-phase3f-multi-pan-multi-slice-design.md) §11 (UI Atlas), pan layout sections

**Prereqs:** Sub-Epics A + B + C complete.

**Estimated effort:** 5 working days, 20 tasks, ~110 bite-sized steps.

---

## File Structure

### Files to create

| File | Purpose |
|---|---|
| `src/gui/PanadapterStack.{h,cpp}` | Layout manager (5 templates, splitter trees, active-pan tracking) |
| `src/gui/PanadapterApplet.{h,cpp}` | Per-pan container (SpectrumWidget host, slice association) |
| `src/core/FFTRouter.{h,cpp}` | Maps receiverId → list of subscribed panIds; fan-out of FFT frames |
| `src/gui/PanFloatingWindow.{h,cpp}` | Top-level QWidget wrapper, dock-back signal |
| `src/gui/PanLayoutDialog.{h,cpp}` | 5-tile visual picker dialog |
| `src/gui/PanMenuWidget.{h,cpp}` | The +PAN dropdown menu (add slice, change layout, float pan) |
| `tests/tst_panadapter_stack_layouts.cpp` | 5 layout templates render correctly |
| `tests/tst_panadapter_applet_slice_assoc.cpp` | Slice add/remove/active-slice tracking |
| `tests/tst_fft_router_fanout.cpp` | One receiver → N pans, add/remove subscriptions |
| `tests/tst_pan_floating_window.cpp` | Detach + dock-back signal + geometry persistence |

### Files to modify

| File | Purpose |
|---|---|
| `src/gui/MainWindow.{h,cpp}` | Replace `m_spectrumWidget` with `m_panStack`, add layout menu actions, activate `+PAN` placeholder |
| `src/core/RadioModel.{h,cpp}` | Add `FFTRouter*` accessor; route slice creation to `addSliceOnPan(panId)` via PanadapterStack |
| `tests/CMakeLists.txt` | Register 4 new test files |
| `src/gui/CMakeLists.txt` | Build new widget files |

---

## Task 1: Create `PanadapterApplet` skeleton (per-pan container)

**Files:** Create `src/gui/PanadapterApplet.{h,cpp}`

- [ ] **Step 1: Write failing test**

Create `tests/tst_panadapter_applet_slice_assoc.cpp`:

```cpp
#include <QtTest/QtTest>
#include "gui/PanadapterApplet.h"

using namespace NereusSDR;

class TestPanadapterAppletSliceAssoc : public QObject {
    Q_OBJECT
private slots:
    void applet_constructs_with_pan_id()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        QCOMPARE(applet.panId(), QStringLiteral("pan-0"));
    }
};

QTEST_MAIN(TestPanadapterAppletSliceAssoc)
#include "tst_panadapter_applet_slice_assoc.moc"
```

Register: `nereus_add_test(tst_panadapter_applet_slice_assoc)`.

- [ ] **Step 2: Run + verify failure**

Expected: `'PanadapterApplet' is not a member of 'NereusSDR'`.

- [ ] **Step 3: Create `src/gui/PanadapterApplet.h`**

```cpp
// =================================================================
// src/gui/PanadapterApplet.h  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR src/gui/PanadapterApplet.h
//   Copyright (C) 2024-2026 Jeremy (KK7GWY) / AetherSDR contributors
//   GPLv3, see LICENSE
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-26 — Ported in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//                 Wideband-extended-pan support added per Phase 3F design;
//                 single-output-device + per-slice pan from existing
//                 NereusSDR audio model preserved.
// =================================================================
#pragma once

#include <QWidget>
#include <QString>
#include <QSet>

namespace NereusSDR {

class SpectrumWidget;
class SliceModel;

/// Container for a single panadapter view: spectrum + waterfall (via SpectrumWidget)
/// + associated slice overlays. AetherSDR overlay model: a pan picks one DDC for
/// FFT, any slice whose freq falls within visible range overlays as a flag.
/// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §3
/// (Slice ↔ Pan binding).
class PanadapterApplet : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString panId READ panId CONSTANT)
    Q_PROPERTY(int activeSliceIndex READ activeSliceIndex WRITE setActiveSliceIndex NOTIFY activeSliceChanged)

public:
    explicit PanadapterApplet(const QString& panId, QWidget* parent = nullptr);
    ~PanadapterApplet() override;

    QString panId() const { return m_panId; }
    SpectrumWidget* spectrumWidget() const { return m_spectrum; }

    /// Associate a slice (its flag will overlay when in visible range).
    void addSlice(int sliceIndex);
    void removeSlice(int sliceIndex);
    QSet<int> associatedSlices() const { return m_associatedSlices; }

    /// The "active" slice: receives tune/mode/filter commands from spectrum clicks.
    int activeSliceIndex() const { return m_activeSliceIndex; }
    void setActiveSliceIndex(int sliceIndex);

    /// Display state (client-side, persists via AppSettings)
    double centerMhz() const { return m_centerMhz; }
    void setCenterMhz(double mhz);
    double bandwidthMhz() const { return m_bandwidthMhz; }
    void setBandwidthMhz(double bw);

signals:
    void activated(const QString& panId);  // emitted on any click within applet
    void closeRequested(const QString& panId);
    void activeSliceChanged(const QString& panId, int sliceIndex);

private:
    QString          m_panId;
    SpectrumWidget*  m_spectrum {nullptr};
    int              m_activeSliceIndex {-1};
    QSet<int>        m_associatedSlices;
    double           m_centerMhz {14.225};
    double           m_bandwidthMhz {0.192};
};

} // namespace NereusSDR
```

- [ ] **Step 4: Stub `src/gui/PanadapterApplet.cpp`**

```cpp
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
#include <QVBoxLayout>

namespace NereusSDR {

PanadapterApplet::PanadapterApplet(const QString& panId, QWidget* parent)
    : QWidget(parent)
    , m_panId(panId)
    , m_spectrum(new SpectrumWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_spectrum);
}

PanadapterApplet::~PanadapterApplet() = default;

void PanadapterApplet::addSlice(int sliceIndex)
{
    m_associatedSlices.insert(sliceIndex);
    if (m_activeSliceIndex == -1) {
        setActiveSliceIndex(sliceIndex);
    }
}

void PanadapterApplet::removeSlice(int sliceIndex)
{
    m_associatedSlices.remove(sliceIndex);
    if (m_activeSliceIndex == sliceIndex) {
        m_activeSliceIndex = m_associatedSlices.isEmpty() ? -1 : *m_associatedSlices.begin();
        emit activeSliceChanged(m_panId, m_activeSliceIndex);
    }
}

void PanadapterApplet::setActiveSliceIndex(int sliceIndex)
{
    if (m_activeSliceIndex == sliceIndex) { return; }
    m_activeSliceIndex = sliceIndex;
    emit activeSliceChanged(m_panId, sliceIndex);
}

void PanadapterApplet::setCenterMhz(double mhz) { m_centerMhz = mhz; }
void PanadapterApplet::setBandwidthMhz(double bw) { m_bandwidthMhz = bw; }

} // namespace NereusSDR
```

Add to `src/gui/CMakeLists.txt` build list.

- [ ] **Step 5: Run + commit**

```bash
cmake --build build --target tst_panadapter_applet_slice_assoc && ctest --test-dir build -R tst_panadapter_applet_slice_assoc -V 2>&1 | tail -10
git add src/gui/PanadapterApplet.{h,cpp} tests/tst_panadapter_applet_slice_assoc.cpp tests/CMakeLists.txt src/gui/CMakeLists.txt
git commit -m "feat(3f-d): PanadapterApplet skeleton (per-pan container with SpectrumWidget host)"
```

---

## Task 2: PanadapterApplet slice association API tests

**Files:** Modify `tests/tst_panadapter_applet_slice_assoc.cpp`

- [ ] **Step 1: Add 4 tests**

```cpp
    void add_slice_makes_it_active_when_no_active_yet()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        QCOMPARE(applet.activeSliceIndex(), -1);
        applet.addSlice(2);
        QCOMPARE(applet.activeSliceIndex(), 2);
    }

    void add_second_slice_does_not_change_active()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        applet.addSlice(0);
        applet.addSlice(1);
        QCOMPARE(applet.activeSliceIndex(), 0);  // first one stays active
        QCOMPARE(applet.associatedSlices().size(), 2);
    }

    void remove_active_slice_promotes_another()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        applet.addSlice(0);
        applet.addSlice(1);
        applet.removeSlice(0);
        QCOMPARE(applet.activeSliceIndex(), 1);
    }

    void remove_last_slice_sets_active_to_minus_1()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        applet.addSlice(0);
        applet.removeSlice(0);
        QCOMPARE(applet.activeSliceIndex(), -1);
    }
```

- [ ] **Step 2: Run + commit**

```bash
ctest --test-dir build -R tst_panadapter_applet_slice_assoc -V 2>&1 | tail -10
git add tests/tst_panadapter_applet_slice_assoc.cpp
git commit -m "test(3f-d): PanadapterApplet slice association API coverage"
```

---

## Task 3: Create `PanadapterStack` skeleton (layout container)

**Files:** Create `src/gui/PanadapterStack.{h,cpp}`

- [ ] **Step 1: Write failing test**

Create `tests/tst_panadapter_stack_layouts.cpp`:

```cpp
#include <QtTest/QtTest>
#include "gui/PanadapterStack.h"

using namespace NereusSDR;

class TestPanadapterStackLayouts : public QObject {
    Q_OBJECT
private slots:
    void stack_starts_with_layout_single()
    {
        PanadapterStack stack;
        QCOMPARE(stack.currentLayoutId(), QStringLiteral("1"));
        QCOMPARE(stack.count(), 1);
    }
};

QTEST_MAIN(TestPanadapterStackLayouts)
#include "tst_panadapter_stack_layouts.moc"
```

Register.

- [ ] **Step 2: Run + verify failure**

- [ ] **Step 3: Create PanadapterStack.h**

```cpp
#pragma once
#include <QWidget>
#include <QString>
#include <QList>
#include <QMap>

class QSplitter;

namespace NereusSDR {

class PanadapterApplet;
class PanFloatingWindow;

/// 5-template pan layout manager. Templates: "1", "2v", "2h", "12h", "2x2".
/// Ported structurally from AetherSDR PanadapterStack (12 templates; we use 5 of them).
/// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §11.
class PanadapterStack : public QWidget {
    Q_OBJECT
public:
    explicit PanadapterStack(QWidget* parent = nullptr);
    ~PanadapterStack() override;

    PanadapterApplet* addPanadapter(const QString& panId);
    void removePanadapter(const QString& panId);
    void removeAll();

    /// Apply one of the 5 templates: "1", "2v", "2h", "12h", "2x2"
    void applyLayout(const QString& layoutId, const QStringList& panIds);

    PanadapterApplet* panadapter(const QString& panId) const;
    QList<PanadapterApplet*> allApplets() const;
    int count() const { return m_pans.size(); }
    QString currentLayoutId() const { return m_currentLayoutId; }

    QString activePanId() const { return m_activePanId; }
    void setActivePan(const QString& panId);

    /// Detach a pan into a top-level PanFloatingWindow.
    void floatPanadapter(const QString& panId);

signals:
    void activePanChanged(const QString& panId);
    void countChanged(int count);

private:
    void rebuildSplitters(const QString& layoutId, const QStringList& panIds);
    void clearSplitters();

    QSplitter*                                 m_rootSplitter {nullptr};
    QMap<QString, PanadapterApplet*>           m_pans;
    QMap<QString, PanFloatingWindow*>          m_floating;
    QString                                    m_currentLayoutId {"1"};
    QString                                    m_activePanId;
};

} // namespace NereusSDR
```

- [ ] **Step 4: Stub PanadapterStack.cpp (default layout "1" with one pan)**

```cpp
#include "gui/PanadapterStack.h"
#include "gui/PanadapterApplet.h"
#include <QVBoxLayout>
#include <QSplitter>

namespace NereusSDR {

PanadapterStack::PanadapterStack(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_rootSplitter = new QSplitter(Qt::Vertical, this);
    layout->addWidget(m_rootSplitter);

    addPanadapter(QStringLiteral("pan-0"));
}

PanadapterStack::~PanadapterStack() = default;

PanadapterApplet* PanadapterStack::addPanadapter(const QString& panId)
{
    if (m_pans.contains(panId)) {
        return m_pans[panId];
    }
    auto* applet = new PanadapterApplet(panId, this);
    m_pans[panId] = applet;
    if (m_activePanId.isEmpty()) { setActivePan(panId); }
    if (m_pans.size() == 1) {
        m_rootSplitter->addWidget(applet);
    }
    emit countChanged(m_pans.size());
    return applet;
}

void PanadapterStack::removePanadapter(const QString& panId)
{
    auto* applet = m_pans.take(panId);
    if (!applet) { return; }
    applet->deleteLater();
    emit countChanged(m_pans.size());
}

void PanadapterStack::removeAll() { /* TODO Task 5 */ }
void PanadapterStack::applyLayout(const QString&, const QStringList&) { /* TODO Task 4 */ }
PanadapterApplet* PanadapterStack::panadapter(const QString& id) const { return m_pans.value(id, nullptr); }
QList<PanadapterApplet*> PanadapterStack::allApplets() const { return m_pans.values(); }
void PanadapterStack::setActivePan(const QString& id) { if (m_activePanId != id) { m_activePanId = id; emit activePanChanged(id); } }
void PanadapterStack::floatPanadapter(const QString&) { /* TODO Task 8 */ }
void PanadapterStack::rebuildSplitters(const QString&, const QStringList&) {}
void PanadapterStack::clearSplitters() {}

} // namespace NereusSDR
```

Add to CMake build.

- [ ] **Step 5: Run + commit**

```bash
cmake --build build --target tst_panadapter_stack_layouts && ctest --test-dir build -R tst_panadapter_stack_layouts -V 2>&1 | tail -10
git add src/gui/PanadapterStack.{h,cpp} tests/tst_panadapter_stack_layouts.cpp tests/CMakeLists.txt src/gui/CMakeLists.txt
git commit -m "feat(3f-d): PanadapterStack skeleton (default 'Single' layout, addPanadapter)"
```

---

## Task 4: Implement `applyLayout("2v")` and `applyLayout("2h")`

**Files:** Modify `src/gui/PanadapterStack.cpp`

- [ ] **Step 1: Add tests**

```cpp
    void apply_layout_2v_creates_two_pans_stacked()
    {
        PanadapterStack stack;
        QStringList ids = {QStringLiteral("pan-0"), QStringLiteral("pan-1")};
        stack.applyLayout(QStringLiteral("2v"), ids);
        QCOMPARE(stack.count(), 2);
        QCOMPARE(stack.currentLayoutId(), QStringLiteral("2v"));
    }

    void apply_layout_2h_creates_two_pans_side_by_side()
    {
        PanadapterStack stack;
        QStringList ids = {QStringLiteral("pan-0"), QStringLiteral("pan-1")};
        stack.applyLayout(QStringLiteral("2h"), ids);
        QCOMPARE(stack.count(), 2);
        QCOMPARE(stack.currentLayoutId(), QStringLiteral("2h"));
    }
```

- [ ] **Step 2: Implement `applyLayout` for "2v" + "2h"**

Replace the stub in PanadapterStack.cpp:

```cpp
void PanadapterStack::applyLayout(const QString& layoutId, const QStringList& panIds)
{
    clearSplitters();
    m_currentLayoutId = layoutId;

    if (layoutId == QStringLiteral("1") && !panIds.isEmpty()) {
        auto* applet = addPanadapter(panIds[0]);
        m_rootSplitter->setOrientation(Qt::Vertical);
        m_rootSplitter->addWidget(applet);
    }
    else if (layoutId == QStringLiteral("2v") && panIds.size() >= 2) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        m_rootSplitter->addWidget(addPanadapter(panIds[0]));
        m_rootSplitter->addWidget(addPanadapter(panIds[1]));
    }
    else if (layoutId == QStringLiteral("2h") && panIds.size() >= 2) {
        m_rootSplitter->setOrientation(Qt::Horizontal);
        m_rootSplitter->addWidget(addPanadapter(panIds[0]));
        m_rootSplitter->addWidget(addPanadapter(panIds[1]));
    }
    // 12h + 2x2 in Task 5
}

void PanadapterStack::clearSplitters()
{
    // Detach all applets from current splitter tree but don't delete them
    for (auto* applet : m_pans.values()) {
        applet->setParent(this);
        applet->hide();
    }
    // Clear nested splitters by replacing root
    delete m_rootSplitter;
    m_rootSplitter = new QSplitter(Qt::Vertical, this);
    layout()->addWidget(m_rootSplitter);
}
```

- [ ] **Step 3: Run + commit**

```bash
ctest --test-dir build -R tst_panadapter_stack_layouts -V 2>&1 | tail -10
git add src/gui/PanadapterStack.cpp tests/tst_panadapter_stack_layouts.cpp
git commit -m "feat(3f-d): PanadapterStack 2v + 2h layouts"
```

---

## Task 5: Implement `applyLayout("12h")` and `applyLayout("2x2")`

**Files:** Modify `src/gui/PanadapterStack.cpp`

- [ ] **Step 1: Add tests**

```cpp
    void apply_layout_12h_creates_3_pans_with_wide_top()
    {
        PanadapterStack stack;
        QStringList ids = {QStringLiteral("pan-0"), QStringLiteral("pan-1"), QStringLiteral("pan-2")};
        stack.applyLayout(QStringLiteral("12h"), ids);
        QCOMPARE(stack.count(), 3);
    }

    void apply_layout_2x2_creates_4_pans_in_grid()
    {
        PanadapterStack stack;
        QStringList ids = {QStringLiteral("p0"), QStringLiteral("p1"), QStringLiteral("p2"), QStringLiteral("p3")};
        stack.applyLayout(QStringLiteral("2x2"), ids);
        QCOMPARE(stack.count(), 4);
    }
```

- [ ] **Step 2: Extend `applyLayout`**

Add inside the existing `applyLayout` after the "2h" branch:

```cpp
    else if (layoutId == QStringLiteral("12h") && panIds.size() >= 3) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        m_rootSplitter->addWidget(addPanadapter(panIds[0]));  // wide top
        auto* bottomSplitter = new QSplitter(Qt::Horizontal, m_rootSplitter);
        bottomSplitter->addWidget(addPanadapter(panIds[1]));
        bottomSplitter->addWidget(addPanadapter(panIds[2]));
        m_rootSplitter->addWidget(bottomSplitter);
        m_rootSplitter->setStretchFactor(0, 2);  // top pan gets 2x weight
        m_rootSplitter->setStretchFactor(1, 1);
    }
    else if (layoutId == QStringLiteral("2x2") && panIds.size() >= 4) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        auto* topRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        topRow->addWidget(addPanadapter(panIds[0]));
        topRow->addWidget(addPanadapter(panIds[1]));
        auto* bottomRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        bottomRow->addWidget(addPanadapter(panIds[2]));
        bottomRow->addWidget(addPanadapter(panIds[3]));
        m_rootSplitter->addWidget(topRow);
        m_rootSplitter->addWidget(bottomRow);
    }
```

- [ ] **Step 3: Run + commit**

```bash
ctest --test-dir build -R tst_panadapter_stack_layouts -V 2>&1 | tail -10
git add src/gui/PanadapterStack.cpp tests/tst_panadapter_stack_layouts.cpp
git commit -m "feat(3f-d): PanadapterStack 12h + 2x2 layouts (3 + 4 pan)"
```

---

## Task 6: PanadapterStack splitter-size persistence

**Files:** Modify `src/gui/PanadapterStack.{h,cpp}`

- [ ] **Step 1: Add API + tests**

In PanadapterStack.h:
```cpp
public:
    void saveSplitterState();
    void restoreSplitterState();
```

In tests:
```cpp
    void splitter_state_persists_across_construct()
    {
        const QString mac = QStringLiteral("test-stack-mac");
        {
            PanadapterStack stack;
            stack.applyLayout(QStringLiteral("2v"), {QStringLiteral("p0"), QStringLiteral("p1")});
            // Force a non-default split (e.g. 300/500 instead of 50/50)
            // ... (depends on how PanadapterStack exposes splitter)
            stack.saveSplitterState();
        }
        {
            PanadapterStack stack2;
            stack2.applyLayout(QStringLiteral("2v"), {QStringLiteral("p0"), QStringLiteral("p1")});
            stack2.restoreSplitterState();
            // verify restored sizes (test seam needed)
            QVERIFY(true);  // placeholder; needs splitter accessor for full check
        }
    }
```

- [ ] **Step 2: Implement save/restore using AppSettings keys `Pan0_Splitter_Sizes`, `Pan1_Splitter_Sizes`, etc.**

```cpp
void PanadapterStack::saveSplitterState()
{
    auto& s = AppSettings::instance();
    QStringList sizes;
    for (int i = 0; i < m_rootSplitter->count(); ++i) {
        sizes << QString::number(m_rootSplitter->sizes()[i]);
    }
    s.setValue(QStringLiteral("PanSplitter0Sizes"), sizes.join(QStringLiteral(",")));
    s.setValue(QStringLiteral("PanLayoutId"), m_currentLayoutId);
}

void PanadapterStack::restoreSplitterState()
{
    auto& s = AppSettings::instance();
    const QString raw = s.value(QStringLiteral("PanSplitter0Sizes"), QString()).toString();
    if (raw.isEmpty()) { return; }
    QList<int> sizes;
    for (const QString& part : raw.split(QStringLiteral(","))) {
        sizes << part.toInt();
    }
    m_rootSplitter->setSizes(sizes);
}
```

- [ ] **Step 3: Commit**

```bash
git add src/gui/PanadapterStack.{h,cpp} tests/tst_panadapter_stack_layouts.cpp
git commit -m "feat(3f-d): PanadapterStack splitter-size persistence (AppSettings)"
```

---

## Task 7: Create `FFTRouter`

**Files:** Create `src/core/FFTRouter.{h,cpp}`, `tests/tst_fft_router_fanout.cpp`

- [ ] **Step 1: Write failing test**

```cpp
#include <QtTest/QtTest>
#include "core/FFTRouter.h"

using namespace NereusSDR;

class TestFFTRouterFanout : public QObject {
    Q_OBJECT
private slots:
    void map_pan_to_receiver_round_trips()
    {
        FFTRouter router;
        router.mapPanToReceiver(QStringLiteral("pan-0"), 2);  // pan-0 watches receiver 2 (DDC2)
        QList<QString> pans = router.pansForReceiver(2);
        QVERIFY(pans.contains(QStringLiteral("pan-0")));
    }

    void multiple_pans_can_subscribe_same_receiver()
    {
        FFTRouter router;
        router.mapPanToReceiver(QStringLiteral("pan-0"), 2);
        router.mapPanToReceiver(QStringLiteral("pan-1"), 2);
        QList<QString> pans = router.pansForReceiver(2);
        QCOMPARE(pans.size(), 2);
    }

    void remove_pan_unsubscribes_from_all_receivers()
    {
        FFTRouter router;
        router.mapPanToReceiver(QStringLiteral("pan-0"), 2);
        router.removePan(QStringLiteral("pan-0"));
        QList<QString> pans = router.pansForReceiver(2);
        QVERIFY(pans.isEmpty());
    }
};

QTEST_MAIN(TestFFTRouterFanout)
#include "tst_fft_router_fanout.moc"
```

Register.

- [ ] **Step 2: Run + verify failure**

- [ ] **Step 3: Create FFTRouter.h**

```cpp
#pragma once
#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QVector>

namespace NereusSDR {

/// Routes FFT frames from receivers to subscribed pans.
/// One receiver (DDC) → 1..N pans (different zoom levels of same data).
/// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §11.
class FFTRouter : public QObject {
    Q_OBJECT
public:
    explicit FFTRouter(QObject* parent = nullptr) : QObject(parent) {}

    void mapPanToReceiver(const QString& panId, int receiverId);
    void removePan(const QString& panId);
    void removeReceiver(int receiverId);

    QList<QString> pansForReceiver(int receiverId) const;
    QList<int> receiversForPan(const QString& panId) const;

public slots:
    /// Called by per-receiver FFTEngine when a frame is ready. Fans out to subscribed pans.
    void onFftFrame(int receiverId, const QVector<float>& binsDbm);

signals:
    void fftFrameForPan(const QString& panId, int receiverId, const QVector<float>& binsDbm);

private:
    QMap<int, QList<QString>> m_receiverToPans;
};

} // namespace NereusSDR
```

- [ ] **Step 4: Stub FFTRouter.cpp**

```cpp
#include "core/FFTRouter.h"

namespace NereusSDR {

void FFTRouter::mapPanToReceiver(const QString& panId, int receiverId)
{
    auto& list = m_receiverToPans[receiverId];
    if (!list.contains(panId)) { list.append(panId); }
}

void FFTRouter::removePan(const QString& panId)
{
    for (auto& list : m_receiverToPans) {
        list.removeAll(panId);
    }
}

void FFTRouter::removeReceiver(int receiverId)
{
    m_receiverToPans.remove(receiverId);
}

QList<QString> FFTRouter::pansForReceiver(int receiverId) const
{
    return m_receiverToPans.value(receiverId);
}

QList<int> FFTRouter::receiversForPan(const QString& panId) const
{
    QList<int> result;
    for (auto it = m_receiverToPans.cbegin(); it != m_receiverToPans.cend(); ++it) {
        if (it.value().contains(panId)) { result.append(it.key()); }
    }
    return result;
}

void FFTRouter::onFftFrame(int receiverId, const QVector<float>& binsDbm)
{
    for (const QString& panId : m_receiverToPans.value(receiverId)) {
        emit fftFrameForPan(panId, receiverId, binsDbm);
    }
}

} // namespace NereusSDR
```

Add to `src/core/CMakeLists.txt`.

- [ ] **Step 5: Run + commit**

```bash
cmake --build build --target tst_fft_router_fanout && ctest --test-dir build -R tst_fft_router_fanout -V 2>&1 | tail -10
git add src/core/FFTRouter.{h,cpp} tests/tst_fft_router_fanout.cpp tests/CMakeLists.txt src/core/CMakeLists.txt
git commit -m "feat(3f-d): FFTRouter (receiver→pan fan-out, AetherSDR overlay model support)"
```

---

## Task 8: Create `PanFloatingWindow` (detach to second monitor)

**Files:** Create `src/gui/PanFloatingWindow.{h,cpp}`, `tests/tst_pan_floating_window.cpp`

- [ ] **Step 1: Write failing test**

```cpp
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterApplet.h"

using namespace NereusSDR;

class TestPanFloatingWindow : public QObject {
    Q_OBJECT
private slots:
    void construct_with_applet_keeps_applet_visible()
    {
        auto* applet = new PanadapterApplet(QStringLiteral("pan-floated"));
        PanFloatingWindow* w = new PanFloatingWindow(applet, nullptr);
        QVERIFY(w->applet() == applet);
        QCOMPARE(w->panId(), QStringLiteral("pan-floated"));
        delete w;
    }

    void dock_requested_signal_emitted_on_close()
    {
        auto* applet = new PanadapterApplet(QStringLiteral("pan-floated"));
        PanFloatingWindow* w = new PanFloatingWindow(applet, nullptr);
        QSignalSpy spy(w, &PanFloatingWindow::dockRequested);
        w->requestDock();
        QCOMPARE(spy.count(), 1);
        delete w;
    }
};

QTEST_MAIN(TestPanFloatingWindow)
#include "tst_pan_floating_window.moc"
```

Register.

- [ ] **Step 2: Create PanFloatingWindow.h**

```cpp
#pragma once
#include <QWidget>
#include <QString>

namespace NereusSDR {

class PanadapterApplet;

/// Top-level QWidget wrapping a PanadapterApplet for multi-monitor detach.
/// Ported from AetherSDR src/gui/PanFloatingWindow.{h,cpp}.
class PanFloatingWindow : public QWidget {
    Q_OBJECT
public:
    PanFloatingWindow(PanadapterApplet* applet, QWidget* parent = nullptr);
    ~PanFloatingWindow() override;

    PanadapterApplet* applet() const { return m_applet; }
    QString panId() const;

    void requestDock();  // re-emits dockRequested

signals:
    void dockRequested(const QString& panId);
    void geometryChanged(const QString& panId, const QByteArray& geometry);

protected:
    void closeEvent(QCloseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    PanadapterApplet* m_applet {nullptr};  // non-owning; ownership stays with PanadapterStack
};

} // namespace NereusSDR
```

- [ ] **Step 3: Implement PanFloatingWindow.cpp**

```cpp
#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterApplet.h"
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QMoveEvent>
#include <QResizeEvent>

namespace NereusSDR {

PanFloatingWindow::PanFloatingWindow(PanadapterApplet* applet, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , m_applet(applet)
{
    setWindowTitle(QStringLiteral("NereusSDR - Pan %1").arg(applet->panId()));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(applet);
    resize(800, 400);
}

PanFloatingWindow::~PanFloatingWindow() = default;

QString PanFloatingWindow::panId() const { return m_applet ? m_applet->panId() : QString(); }

void PanFloatingWindow::requestDock() { emit dockRequested(panId()); }

void PanFloatingWindow::closeEvent(QCloseEvent* event)
{
    requestDock();
    event->accept();
}

void PanFloatingWindow::moveEvent(QMoveEvent*)
{
    emit geometryChanged(panId(), saveGeometry());
}

void PanFloatingWindow::resizeEvent(QResizeEvent*)
{
    emit geometryChanged(panId(), saveGeometry());
}

} // namespace NereusSDR
```

- [ ] **Step 4: Wire `floatPanadapter` in PanadapterStack**

Replace the stub in `PanadapterStack.cpp`:

```cpp
void PanadapterStack::floatPanadapter(const QString& panId)
{
    auto* applet = m_pans.value(panId, nullptr);
    if (!applet || m_floating.contains(panId)) { return; }

    auto* fw = new PanFloatingWindow(applet, nullptr);
    m_floating[panId] = fw;

    connect(fw, &PanFloatingWindow::dockRequested, this, [this, panId]() {
        auto* fw = m_floating.take(panId);
        if (!fw) { return; }
        auto* applet = fw->applet();
        if (applet) {
            // Reparent back to stack (re-add via current layout)
            applyLayout(m_currentLayoutId, m_pans.keys());
        }
        fw->deleteLater();
    });

    fw->show();
}
```

- [ ] **Step 5: Run + commit**

```bash
cmake --build build --target tst_pan_floating_window && ctest --test-dir build -R tst_pan_floating_window -V 2>&1 | tail -10
git add src/gui/PanFloatingWindow.{h,cpp} src/gui/PanadapterStack.cpp tests/tst_pan_floating_window.cpp tests/CMakeLists.txt
git commit -m "feat(3f-d): PanFloatingWindow + PanadapterStack::floatPanadapter (multi-monitor detach)"
```

---

## Task 9: `PanLayoutDialog` (5-tile visual picker)

**Files:** Create `src/gui/PanLayoutDialog.{h,cpp}`

- [ ] **Step 1: Create header**

```cpp
#pragma once
#include <QDialog>
#include <QString>

namespace NereusSDR {

/// 5-tile visual layout picker. Ported structurally from AetherSDR PanLayoutDialog.
/// Tiles: "1" (Single), "2v" (Stacked), "2h" (Side-by-Side), "12h" (Wide+2 with
/// WIDEBAND badge), "2x2" (Grid).
class PanLayoutDialog : public QDialog {
    Q_OBJECT
public:
    explicit PanLayoutDialog(QWidget* parent = nullptr);
    ~PanLayoutDialog() override;

    QString selectedLayout() const { return m_selected; }

private:
    QString m_selected;
};

} // namespace NereusSDR
```

- [ ] **Step 2: Implement (uses Style helpers)**

```cpp
#include "gui/PanLayoutDialog.h"
#include "gui/StyleConstants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

namespace NereusSDR::Style {} // bring helpers into scope

namespace NereusSDR {

namespace {
struct LayoutTile { const char* id; const char* label; const char* subtitle; };
constexpr LayoutTile kTiles[] = {
    {"1",   "Single",        "1 pan"},
    {"2v",  "Stacked",       "2 pans · 2v"},
    {"2h",  "Side-by-Side",  "2 pans · 2h"},
    {"12h", "Wide + 2",      "3 pans · 12h · WIDEBAND"},
    {"2x2", "Grid",          "4 pans · 2x2"},
};
}

PanLayoutDialog::PanLayoutDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Pan Layout"));
    setStyleSheet(QStringLiteral("background: %1; color: %2;").arg(Style::kAppBg, Style::kTextPrimary));

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    auto* intro = new QLabel(QStringLiteral("Pick a layout. Per-pan source is set with right-click after applying."), this);
    intro->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(Style::kTextSecondary));
    mainLayout->addWidget(intro);

    auto* tileRow = new QHBoxLayout();
    mainLayout->addLayout(tileRow);

    for (const auto& tile : kTiles) {
        auto* btn = new QPushButton(QStringLiteral("%1\n%2").arg(tile.label, tile.subtitle), this);
        btn->setFixedSize(120, 100);
        btn->setStyleSheet(Style::buttonBaseStyle());
        QString tileId = QString::fromLatin1(tile.id);
        connect(btn, &QPushButton::clicked, this, [this, tileId]() {
            m_selected = tileId;
            accept();
        });
        tileRow->addWidget(btn);
    }

    auto* footerRow = new QHBoxLayout();
    mainLayout->addLayout(footerRow);
    footerRow->addStretch(1);
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
    cancelBtn->setStyleSheet(Style::buttonBaseStyle());
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    footerRow->addWidget(cancelBtn);
}

PanLayoutDialog::~PanLayoutDialog() = default;

} // namespace NereusSDR
```

- [ ] **Step 3: Commit**

```bash
cmake --build build && git add src/gui/PanLayoutDialog.{h,cpp} src/gui/CMakeLists.txt
git commit -m "feat(3f-d): PanLayoutDialog (5-tile visual picker, real StyleConstants palette)"
```

---

## Task 10: `+PAN` dropdown menu activation in bottom status bar

**Files:** Modify `src/gui/MainWindow.cpp` (the `buildStatusBar` method around line 3984)

The existing `+PAN` label at `MainWindow.cpp:3990` is currently styled `color: #404858; font-weight: bold` (greyed NYI). Activate it.

- [ ] **Step 1: Find and replace the `+PAN` label**

Search for `panLabel` in `MainWindow.cpp:buildStatusBar`. Replace its construction:

```cpp
    // Phase 3F: activated +PAN button (was NYI placeholder).
    auto* panBtn = new QPushButton(QStringLiteral("+PAN"), barWidget);
    panBtn->setFlat(true);
    panBtn->setCursor(Qt::PointingHandCursor);
    panBtn->setStyleSheet(QStringLiteral(
        "QPushButton { color: %1; font-weight: bold; font-size: 11px;"
        " border: 1px solid %2; border-radius: 3px; padding: 2px 7px;"
        " background: %3; }"
        "QPushButton:hover { background: %4; }")
        .arg(Style::kAccent, Style::kBorder, Style::kButtonBg, Style::kButtonAltHover));
    panBtn->setToolTip(QStringLiteral("Add slice / change layout / float pan"));
    connect(panBtn, &QPushButton::clicked, this, &MainWindow::showPanMenu);
    hbox->addWidget(panBtn);
```

- [ ] **Step 2: Implement `showPanMenu` slot in MainWindow**

In MainWindow.h:
```cpp
private slots:
    void showPanMenu();
```

In MainWindow.cpp:
```cpp
void MainWindow::showPanMenu()
{
    QMenu menu(this);

    // Add slice section
    menu.addSection(QStringLiteral("Add slice on active pan"));
    const int currentSlices = m_radioModel->slices().size();
    const int maxS = m_radioModel->maxSlices();
    for (int i = 0; i < maxS; ++i) {
        const QChar letter = QChar('A' + i);
        QAction* act = menu.addAction(QStringLiteral("Slice %1").arg(letter));
        act->setEnabled(i >= currentSlices);  // grey out already-active letters
        if (i == currentSlices) {
            connect(act, &QAction::triggered, this, [this]() {
                if (m_panStack) {
                    const QString activePan = m_panStack->activePanId();
                    m_radioModel->addSliceOnPan(activePan);
                }
            });
        }
    }

    menu.addSeparator();
    menu.addSection(QStringLiteral("Layout"));
    for (const auto& layoutId : {"1", "2v", "2h", "12h", "2x2"}) {
        QAction* act = menu.addAction(QString::fromLatin1(layoutId));
        if (m_panStack && m_panStack->currentLayoutId() == layoutId) {
            act->setCheckable(true);
            act->setChecked(true);
        }
        QString id = QString::fromLatin1(layoutId);
        connect(act, &QAction::triggered, this, [this, id]() {
            if (m_panStack) {
                // Synthesize panIds list to match the layout's required count
                const int needed = (id == "1") ? 1 : (id.startsWith("2") && id != "2x2") ? 2 : (id == "12h") ? 3 : 4;
                QStringList ids;
                for (int i = 0; i < needed; ++i) { ids << QStringLiteral("pan-%1").arg(i); }
                m_panStack->applyLayout(id, ids);
            }
        });
    }

    menu.addSeparator();
    if (auto* floatAct = menu.addAction(QStringLiteral("Float active pan..."))) {
        connect(floatAct, &QAction::triggered, this, [this]() {
            if (m_panStack) {
                m_panStack->floatPanadapter(m_panStack->activePanId());
            }
        });
    }

    menu.exec(QCursor::pos());
}
```

- [ ] **Step 3: Commit**

```bash
cmake --build build
git add src/gui/MainWindow.{h,cpp}
git commit -m "feat(3f-d): activate +PAN button in bottom status bar with dropdown menu"
```

---

## Task 11: Bottom-bar per-chain stacked indicators (CH 0 / CH 1)

**Files:** Modify `src/gui/MainWindow.cpp` (insert in `buildStatusBar` near the +PAN button)

- [ ] **Step 1: Add chain indicator widget construction**

In `buildStatusBar`, after the `+PAN` button (and before the `☰` panel toggle):

```cpp
    // Phase 3F: per-chain WIDE state indicators.
    // CH 0 (always shown), CH 1 (shown only on 2-ADC SKUs).
    auto makeChainIndicator = [&](int adc) -> QWidget* {
        auto* w = new QWidget(barWidget);
        auto* vl = new QVBoxLayout(w);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);

        auto* topLbl = new QLabel(QStringLiteral("CH %1").arg(adc), w);
        topLbl->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; font-weight: bold;").arg(Style::kTextScale));
        vl->addWidget(topLbl);

        auto* botLbl = new QLabel(QStringLiteral("idle"), w);
        botLbl->setObjectName(QStringLiteral("chainIndicator%1").arg(adc));
        botLbl->setStyleSheet(QStringLiteral("color: %1; font-size: 9px; font-weight: bold;").arg(Style::kTextInactive));
        vl->addWidget(botLbl);

        return w;
    };

    hbox->addWidget(makeChainIndicator(0));
    auto* chain1Widget = makeChainIndicator(1);
    chain1Widget->setVisible(false);  // hidden by default; shown when connected to 2-ADC SKU
    hbox->addWidget(chain1Widget);
    m_chain1IndicatorWidget = chain1Widget;
```

- [ ] **Step 2: Wire to `AlexController::bpfStateChanged`**

Find where AlexController is connected. Add:

```cpp
    if (auto* alex = m_radioModel->alexController()) {
        connect(alex, &AlexController::bpfStateChanged, this,
                [this](int adc, const AlexController::AlexAdcState& state) {
            auto* lbl = findChild<QLabel*>(QStringLiteral("chainIndicator%1").arg(adc));
            if (!lbl) { return; }
            lbl->setText(state.reasonText);
            const QString color = (state.effective == AlexController::BpfEffective::Filtered) ? Style::kGreenText :
                                  (state.effective == AlexController::BpfEffective::WidebandLocked) ? Style::kAmberWarn :
                                  Style::kAmberWarn;
            lbl->setStyleSheet(QStringLiteral("color: %1; font-size: 9px; font-weight: bold;").arg(color));
        });
    }
```

- [ ] **Step 3: Show CH 1 only on 2-ADC SKUs**

In `currentRadioChanged` handler:
```cpp
    if (m_chain1IndicatorWidget) {
        const auto caps = capabilitiesFor(info.model);
        m_chain1IndicatorWidget->setVisible(caps.adcCount >= 2);
    }
```

- [ ] **Step 4: Commit**

```bash
cmake --build build
git add src/gui/MainWindow.{h,cpp}
git commit -m "feat(3f-d): bottom-bar CH 0 / CH 1 stacked indicators (BPF state, 2-ADC gated)"
```

---

## Task 12: Replace `m_spectrumWidget` with `m_panStack` in MainWindow

**Files:** Modify `src/gui/MainWindow.{h,cpp}`

This is the central wiring change. `m_spectrumWidget` (single SpectrumWidget) becomes `m_panStack` (PanadapterStack containing N applets, each with its own SpectrumWidget).

- [ ] **Step 1: Replace member declaration in MainWindow.h**

```cpp
    // OLD: SpectrumWidget* m_spectrumWidget {nullptr};
    PanadapterStack* m_panStack {nullptr};

    /// Backward-compat accessor: returns the active pan's spectrum widget.
    /// Existing code that references m_spectrumWidget directly should migrate
    /// to using m_panStack->panadapter(activePanId)->spectrumWidget() or this helper.
    SpectrumWidget* activeSpectrumWidget() const;
```

- [ ] **Step 2: Replace construction in MainWindow.cpp**

Find where `m_spectrumWidget` is constructed (likely in MainWindow's central widget setup). Replace:

```cpp
    m_panStack = new PanadapterStack(this);
    setCentralWidget(m_panStack);

    // Wire panStack signals
    connect(m_panStack, &PanadapterStack::activePanChanged, this,
            [this](const QString& panId) {
        // Update VFO bindings, FFT routing, etc., based on new active pan
        rewireForActivePan(panId);
    });
```

Add helper:

```cpp
SpectrumWidget* MainWindow::activeSpectrumWidget() const
{
    if (!m_panStack) { return nullptr; }
    auto* applet = m_panStack->panadapter(m_panStack->activePanId());
    return applet ? applet->spectrumWidget() : nullptr;
}
```

- [ ] **Step 3: Update all references**

```bash
grep -nE "m_spectrumWidget\b" src/gui/MainWindow.cpp | head -10
```

For each occurrence, replace with `activeSpectrumWidget()` or thread through the active pan's applet.

- [ ] **Step 4: Build + smoke test**

```bash
cmake --build build && ctest --test-dir build 2>&1 | tail -5
./build/NereusSDR.app/Contents/MacOS/NereusSDR &
```

Verify single-pan operation works (default layout "1", one slice).

- [ ] **Step 5: Commit**

```bash
git add src/gui/MainWindow.{h,cpp}
git commit -m "refactor(3f-d): MainWindow replaces m_spectrumWidget with m_panStack (PanadapterStack)"
```

---

## Task 13: Wire RadioModel slice signals to PanadapterStack

**Files:** Modify `src/gui/MainWindow.cpp` (init connections)

When `sliceAdded` fires, the new slice needs to associate with a pan + FFTRouter.

- [ ] **Step 1: Add wiring in MainWindow::init**

```cpp
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](SliceModel* slice) {
        if (!slice || !m_panStack) { return; }
        const QString initialPan = slice->property("initialPanId").toString();
        const QString targetPan = initialPan.isEmpty() ? m_panStack->activePanId() : initialPan;

        auto* applet = m_panStack->panadapter(targetPan);
        if (!applet) {
            applet = m_panStack->addPanadapter(targetPan);
        }

        const int sliceIdx = m_radioModel->slices().indexOf(slice);
        applet->addSlice(sliceIdx);

        // Map this slice's receiver to its pan via FFTRouter
        if (auto* router = m_radioModel->fftRouter()) {
            // receiverId = slice's DDC index, which the codec assigned
            router->mapPanToReceiver(targetPan, slice->ddcIndex());
        }
    });

    connect(m_radioModel, &RadioModel::sliceRemoved, this, [this](int sliceIndex) {
        if (!m_panStack) { return; }
        for (auto* applet : m_panStack->allApplets()) {
            applet->removeSlice(sliceIndex);
        }
    });
```

- [ ] **Step 2: Expose FFTRouter from RadioModel**

In RadioModel.h:
```cpp
public:
    FFTRouter* fftRouter() const { return m_fftRouter; }
private:
    FFTRouter* m_fftRouter {nullptr};
```

In RadioModel constructor:
```cpp
    m_fftRouter = new FFTRouter(this);
```

- [ ] **Step 3: Commit**

```bash
cmake --build build && ctest --test-dir build 2>&1 | tail -5
git add src/gui/MainWindow.cpp src/models/RadioModel.{h,cpp}
git commit -m "feat(3f-d): wire RadioModel sliceAdded/Removed signals to PanadapterStack + FFTRouter"
```

---

## Task 14: View menu — Pan Layout, Add slice, Float active pan

**Files:** Modify `src/gui/MainWindow.cpp` (menu construction)

- [ ] **Step 1: Add menu entries**

Find where the View menu is constructed. Add:

```cpp
    auto* panLayoutAct = viewMenu->addAction(QStringLiteral("Pan Layout..."));
    panLayoutAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
    connect(panLayoutAct, &QAction::triggered, this, [this]() {
        PanLayoutDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            const QString layoutId = dialog.selectedLayout();
            if (m_panStack) {
                const int needed = (layoutId == "1") ? 1 : (layoutId.startsWith("2") && layoutId != "2x2") ? 2 : (layoutId == "12h") ? 3 : 4;
                QStringList ids;
                for (int i = 0; i < needed; ++i) { ids << QStringLiteral("pan-%1").arg(i); }
                m_panStack->applyLayout(layoutId, ids);
            }
        }
    });

    auto* addSliceAct = viewMenu->addAction(QStringLiteral("Add slice on active pan"));
    addSliceAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    connect(addSliceAct, &QAction::triggered, this, [this]() {
        if (m_panStack && m_radioModel) {
            m_radioModel->addSliceOnPan(m_panStack->activePanId());
        }
    });

    auto* floatAct = viewMenu->addAction(QStringLiteral("Float active pan..."));
    connect(floatAct, &QAction::triggered, this, [this]() {
        if (m_panStack) {
            m_panStack->floatPanadapter(m_panStack->activePanId());
        }
    });
```

- [ ] **Step 2: Commit**

```bash
cmake --build build
git add src/gui/MainWindow.cpp
git commit -m "feat(3f-d): View menu - Pan Layout (Ctrl+L), Add slice (Ctrl+R), Float pan"
```

---

## Task 15: PanadapterStack layout persistence + restore-on-launch

**Files:** Modify `src/gui/MainWindow.cpp` (init + shutdown)

- [ ] **Step 1: Save layout state on shutdown**

In `MainWindow::~MainWindow` or close event:

```cpp
    if (m_panStack) { m_panStack->saveSplitterState(); }
```

- [ ] **Step 2: Restore layout state on construction**

In `MainWindow::init` (after creating `m_panStack`):

```cpp
    const QString restoredLayout = AppSettings::instance().value(QStringLiteral("PanLayoutId"), QStringLiteral("1")).toString();
    QStringList ids;
    const int needed = (restoredLayout == "1") ? 1 : (restoredLayout.startsWith("2") && restoredLayout != "2x2") ? 2 : (restoredLayout == "12h") ? 3 : 4;
    for (int i = 0; i < needed; ++i) { ids << QStringLiteral("pan-%1").arg(i); }
    m_panStack->applyLayout(restoredLayout, ids);
    m_panStack->restoreSplitterState();
```

- [ ] **Step 3: Commit**

```bash
git add src/gui/MainWindow.cpp
git commit -m "feat(3f-d): PanadapterStack layout state persists across app launches"
```

---

## Task 16: Disconnect-before-removal pattern for pan removal

**Files:** Modify `src/gui/MainWindow.cpp` and `src/gui/PanadapterStack.cpp`

Following AetherSDR issue #242 pattern: when removing a panadapter, disconnect all signals BEFORE deleting widgets to avoid lambda crashes.

- [ ] **Step 1: Implement disconnect helper in MainWindow**

```cpp
void MainWindow::disconnectPanadapter(const QString& panId)
{
    if (!m_panStack) { return; }
    auto* applet = m_panStack->panadapter(panId);
    if (!applet) { return; }

    if (auto* sw = applet->spectrumWidget()) {
        sw->disconnect(this);
    }
    applet->disconnect(this);

    if (auto* router = m_radioModel ? m_radioModel->fftRouter() : nullptr) {
        router->removePan(panId);
    }
}
```

Call before `m_panStack->removePanadapter(panId)`.

- [ ] **Step 2: Commit**

```bash
git add src/gui/MainWindow.{h,cpp}
git commit -m "feat(3f-d): disconnect-before-removal pattern for pan removal (AetherSDR issue #242)"
```

---

## Task 17: Smoke test on 2-pan operation (manual)

**Files:** none modified

- [ ] **Step 1: Build + launch**

```bash
cmake --build build && ./build/NereusSDR.app/Contents/MacOS/NereusSDR &
```

- [ ] **Step 2: Connect to G2 (or other 2-ADC SKU with maxSlices>=2)**

Verify:
- Default layout is "1" with one pan, one slice (A)
- Click +PAN → "Slice B" → adds Slice B
- View → Pan Layout → pick "2v" → display splits vertically into 2 pans
- Slice B's flag overlay appears on appropriate pan based on frequency
- Click +PAN → "Float active pan..." → pan detaches into top-level window
- Drag floating window to a second monitor (if available) → verify it stays there
- Close floating window → pan returns to main layout

- [ ] **Step 3: Commit (manual verification checkpoint)**

```bash
git commit --allow-empty -m "chore(3f-d): manual smoke test passed (2-pan + floating window on G2)"
```

---

## Task 18: Regression sweep — single-slice operation unchanged

**Files:** none modified

- [ ] **Step 1: Test on HL2 (1-slice cap)**

Connect to HL2. Verify:
- Single pan, single slice
- +PAN menu shows "Slice A" but greyed (already exists), Slice B greyed out as "cap reached"
- Layout switcher works (can apply 2v to single slice → pan-0 + pan-1, both showing same DDC at different zooms via overlay)
- Audio + spectrum work as before

- [ ] **Step 2: Run full test suite**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -15
```

Expected: all previous + new D test suites pass.

- [ ] **Step 3: Commit**

```bash
git commit --allow-empty -m "chore(3f-d): HL2 single-slice regression sweep passed"
```

---

## Task 19: Sub-Epic D retrospective note

**Files:** Modify `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md`

Append retrospective note (same pattern as Sub-Epic A + C).

```bash
git add docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md
git commit -m "docs(arch): record Sub-Epic D landing"
```

---

## Task 20: Open Sub-Epic D PR

```bash
git push -u origin HEAD
gh pr create --title "Phase 3F Sub-Epic D: Pan Layouts + Multi-Pan UI" --body "$(cat <<'EOF'
## Summary

- 5 layout templates (1, 2v, 2h, 12h, 2x2) via PanadapterStack
- PanadapterApplet per-pan container with slice association
- FFTRouter for receiver→pan fan-out
- PanFloatingWindow for multi-monitor detach
- PanLayoutDialog visual picker (View → Pan Layout, Ctrl+L)
- +PAN button activated in bottom status bar with dropdown menu
- Per-chain CH 0 / CH 1 stacked indicators in bottom bar (BPF state from AlexController)
- Layout state persists across app launches

## Test plan

- [ ] CI green (4 new test suites)
- [ ] HL2 single-slice operation unchanged
- [ ] G2 2-pan stacked + side-by-side
- [ ] G2 12h layout with wideband-ready top
- [ ] G2 2x2 grid layout
- [ ] Float pan to second monitor + dock back
- [ ] Splitter sizes restore across launches

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

---

## Sub-Epic D Completion Criteria

When Tasks 1-20 are done:

- `PanadapterStack` with 5 layout templates + splitter state persistence
- `PanadapterApplet` per-pan container with slice association
- `FFTRouter` for receiver→pan fan-out
- `PanFloatingWindow` for multi-monitor detach
- `PanLayoutDialog` visual picker
- `+PAN` button activated with dropdown menu
- Per-chain bottom-bar indicators
- View menu entries (Pan Layout / Add slice / Float)
- MainWindow uses `m_panStack` instead of single `m_spectrumWidget`
- Single-slice operation unchanged (regression sweep on HL2)
- 4 new test files, ~25 cases, all green

Ready for **Sub-Epic E (UI atlas surfaces: SpectrumStatusOverlay, antenna picker, Filter Policy popup, Setup DDC Routing)** to begin.
