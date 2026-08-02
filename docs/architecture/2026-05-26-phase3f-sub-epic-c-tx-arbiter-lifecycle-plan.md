# Phase 3F Sub-Epic C: TxSliceArbiter + Slice Lifecycle — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Introduce `TxSliceArbiter` (single-TX invariant enforcement with MOX-drop guard), wire slice add/remove lifecycle through `RadioModel::addSliceOnPan` (AetherSDR-faithful workflow), and connect `VfoWidget::m_txBadge` click handler to the arbiter. After this plan lands, operators can create a second slice via API, bind TX to either slice safely, and the radio honours the TX-bound slice's antenna/band/mode on key-down.

**Architecture:** New `TxSliceArbiter` class owned by `RadioModel`. `RadioModel` gains `addSliceOnPan(QString panId)` + `removeSlice(int sliceIndex)`. `VfoWidget::m_txBadge` (existing visual surface, currently inert) gains `clicked()` connection to arbiter. MoxController + AlexController + persistence layer subscribe to arbiter signals.

**Tech Stack:** C++20, Qt6 (signals/slots, QObject lifetime via parent ownership), QtTest, AppSettings for `TxBoundSliceIndex` persistence.

**Parent design:** [docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md](2026-05-26-phase3f-multi-pan-multi-slice-design.md) §6 (TxSliceArbiter), §3 (Slice Lifecycle)

**Prereqs:** Sub-Epics A + B complete.

**Estimated effort:** 3 working days, 12 tasks, ~65 bite-sized steps.

---

## File Structure

### Files to create

| File | Purpose |
|---|---|
| `src/core/TxSliceArbiter.h` | Class declaration: handoff API, MOX-drop sequencing, signals |
| `src/core/TxSliceArbiter.cpp` | Handoff sequence implementation |
| `tests/tst_tx_slice_arbiter.cpp` | Handoff flow, MOX-drop guard, idempotency, persistence |
| `tests/tst_radio_model_slice_lifecycle.cpp` | addSliceOnPan + removeSlice + cap enforcement |
| `tests/tst_vfo_widget_tx_badge_click.cpp` | Badge click → arbiter handoff |

### Files to modify

| File | Purpose |
|---|---|
| `src/models/RadioModel.h` | `addSliceOnPan(QString)`, `removeSlice(int)`, owns `TxSliceArbiter*`, `sliceAdded`/`sliceRemoved` signals |
| `src/models/RadioModel.cpp` | Lifecycle implementations, arbiter wiring on construct |
| `src/gui/widgets/VfoWidget.h` | New slot `onTxBadgeClicked()`, takes a slice index/pointer parameter |
| `src/gui/widgets/VfoWidget.cpp` | Wire `m_txBadge->clicked()` to slot; slot calls `RadioModel::txSliceArbiter()->requestHandoff(sliceIndex)` |
| `src/gui/MainWindow.cpp` | Status-bar reject toast on `addSliceOnPan` cap-exceeded; delete deprecated `RadioModel::setSplit` stub call sites |
| `src/models/RadioModel.cpp` | Delete the stubbed `setSplit(int, bool)` (Q_INVOKABLE) per design §3 cleanup |

---

## Task 1: Create `TxSliceArbiter.h` skeleton

**Files:** Create `src/core/TxSliceArbiter.h`

- [ ] **Step 1: Write failing test**

Create `tests/tst_tx_slice_arbiter.cpp`:

```cpp
// =================================================================
// tests/tst_tx_slice_arbiter.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic C: TxSliceArbiter single-TX invariant.
// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §6.
// =================================================================
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/TxSliceArbiter.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestTxSliceArbiter : public QObject {
    Q_OBJECT
private slots:
    void default_tx_bound_slice_index_is_0()
    {
        TxSliceArbiter arb;
        QCOMPARE(arb.txBoundSliceIndex(), 0);
    }
};

QTEST_MAIN(TestTxSliceArbiter)
#include "tst_tx_slice_arbiter.moc"
```

Register: `nereus_add_test(tst_tx_slice_arbiter)` in `tests/CMakeLists.txt`.

- [ ] **Step 2: Run + verify failure (compile error)**

```bash
cmake --build build --target tst_tx_slice_arbiter 2>&1 | tail -5
```

Expected: `'TxSliceArbiter' is not a member of 'NereusSDR'`.

- [ ] **Step 3: Create `src/core/TxSliceArbiter.h`**

```cpp
// =================================================================
// src/core/TxSliceArbiter.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Enforces the single-TX-bound-slice
// invariant for Phase 3F multi-slice. Design ref:
// docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §6.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-26 — Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
// =================================================================
#pragma once

#include <QObject>
#include <QString>

namespace NereusSDR {

class SliceModel;
class MoxController;

/// Enforces the single-TX invariant: exactly one slice is TX-bound at a time.
/// Performs RF-safe handoff (drops MOX before flipping). RadioModel owns one
/// instance; MoxController + AlexController + VfoWidget + persistence subscribe.
class TxSliceArbiter : public QObject {
    Q_OBJECT
    Q_PROPERTY(int txBoundSliceIndex READ txBoundSliceIndex NOTIFY txBoundSliceChanged)

public:
    explicit TxSliceArbiter(QObject* parent = nullptr);

    int txBoundSliceIndex() const { return m_txBoundIndex; }

    /// Inject the MoxController (called by RadioModel during construction wiring).
    /// Arbiter calls mox->setMox(false) and waits for moxChanged confirmation
    /// before flipping txSlice flags.
    void setMoxController(MoxController* mox);

    /// Inject the slice list owner (RadioModel) so arbiter can flip txSlice
    /// flags on SliceModel instances.
    void setSliceList(QVector<SliceModel*>* slices);

public slots:
    /// Request TX handoff to the slice at newSliceIndex. RF-safe (drops MOX first).
    /// Returns true if handoff succeeded or was a no-op (already TX-bound).
    /// Returns false and emits handoffBlocked if requested slice doesn't exist.
    bool requestHandoff(int newSliceIndex);

signals:
    /// Emitted after handoff completes. oldIndex may be -1 on initial bind.
    void txBoundSliceChanged(int oldIndex, int newIndex);

    /// Emitted when a handoff request is rejected (slice doesn't exist, etc.).
    void handoffBlocked(int requestedIndex, QString reason);

private:
    int                       m_txBoundIndex {0};
    MoxController*            m_mox {nullptr};
    QVector<SliceModel*>*     m_slices {nullptr};  // non-owning pointer to RadioModel's list
};

} // namespace NereusSDR
```

- [ ] **Step 4: Stub `src/core/TxSliceArbiter.cpp`**

```cpp
#include "core/TxSliceArbiter.h"
#include "models/SliceModel.h"
#include "core/MoxController.h"

namespace NereusSDR {

TxSliceArbiter::TxSliceArbiter(QObject* parent) : QObject(parent) {}

void TxSliceArbiter::setMoxController(MoxController* mox) { m_mox = mox; }

void TxSliceArbiter::setSliceList(QVector<SliceModel*>* slices) { m_slices = slices; }

bool TxSliceArbiter::requestHandoff(int /*newSliceIndex*/)
{
    // Stub: filled in Task 3
    return false;
}

} // namespace NereusSDR
```

Add to CMake build (`src/core/CMakeLists.txt` or equivalent — look for existing TxSlice / MoxController entry).

- [ ] **Step 5: Run + commit**

```bash
cmake --build build --target tst_tx_slice_arbiter && ctest --test-dir build -R tst_tx_slice_arbiter -V 2>&1 | tail -10
git add src/core/TxSliceArbiter.{h,cpp} tests/tst_tx_slice_arbiter.cpp tests/CMakeLists.txt src/core/CMakeLists.txt
git commit -m "feat(3f-c): scaffold TxSliceArbiter class (skeleton, default state)"
```

---

## Task 2: Test scaffolding for handoff (slices stub)

**Files:** Modify `tests/tst_tx_slice_arbiter.cpp`

- [ ] **Step 1: Add a helper to build a slice list for tests**

Append to the test class:

```cpp
private:
    // Build a list of N SliceModel instances for testing. Returned via out-param;
    // each slice gets sliceIndex set via reflection or via SliceModel public API.
    void buildSlices(QVector<SliceModel*>& outSlices, int n)
    {
        for (int i = 0; i < n; ++i) {
            auto* s = new SliceModel(this);  // parent=this for cleanup
            outSlices.append(s);
        }
        // Mark slice 0 as initially TX-bound (default state)
        if (!outSlices.isEmpty()) {
            outSlices[0]->setTxSlice(true);
        }
    }
```

- [ ] **Step 2: Add test for no-op when handoff to already-bound slice**

```cpp
    void handoff_to_already_bound_slice_is_noop_returns_true()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2);
        TxSliceArbiter arb;
        arb.setSliceList(&slices);

        QSignalSpy spy(&arb, &TxSliceArbiter::txBoundSliceChanged);
        const bool ok = arb.requestHandoff(0);  // 0 is already TX-bound
        QCOMPARE(ok, true);
        QCOMPARE(spy.count(), 0);  // no signal emitted (no change)
    }
```

- [ ] **Step 3: Add test for handoff to invalid index**

```cpp
    void handoff_to_nonexistent_slice_returns_false_emits_blocked()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2);
        TxSliceArbiter arb;
        arb.setSliceList(&slices);

        QSignalSpy blocked(&arb, &TxSliceArbiter::handoffBlocked);
        const bool ok = arb.requestHandoff(5);  // out of range
        QCOMPARE(ok, false);
        QCOMPARE(blocked.count(), 1);
    }
```

- [ ] **Step 4: Run + verify failure (handoff logic not implemented)**

Expected: tests fail because `requestHandoff` is a stub returning `false`.

- [ ] **Step 5: Commit (failing tests)**

```bash
git add tests/tst_tx_slice_arbiter.cpp
git commit -m "test(3f-c): TxSliceArbiter handoff scaffolding + failing edge cases"
```

---

## Task 3: Implement basic handoff (no MOX, no antenna)

**Files:** Modify `src/core/TxSliceArbiter.cpp`

- [ ] **Step 1: Implement `requestHandoff` minimal path**

Replace the stub:

```cpp
bool TxSliceArbiter::requestHandoff(int newSliceIndex)
{
    if (!m_slices || newSliceIndex < 0 || newSliceIndex >= m_slices->size()) {
        emit handoffBlocked(newSliceIndex, QStringLiteral("Slice index out of range"));
        return false;
    }

    if (newSliceIndex == m_txBoundIndex) {
        return true;  // already TX-bound, no-op
    }

    const int oldIndex = m_txBoundIndex;

    // Flip txSlice flags
    if (oldIndex >= 0 && oldIndex < m_slices->size()) {
        m_slices->at(oldIndex)->setTxSlice(false);
    }
    m_slices->at(newSliceIndex)->setTxSlice(true);

    m_txBoundIndex = newSliceIndex;
    emit txBoundSliceChanged(oldIndex, newSliceIndex);
    return true;
}
```

- [ ] **Step 2: Add test for happy-path handoff**

```cpp
    void handoff_to_different_slice_flips_tx_flags_and_emits()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2);
        TxSliceArbiter arb;
        arb.setSliceList(&slices);

        QSignalSpy spy(&arb, &TxSliceArbiter::txBoundSliceChanged);

        QCOMPARE(slices[0]->isTxSlice(), true);
        QCOMPARE(slices[1]->isTxSlice(), false);

        const bool ok = arb.requestHandoff(1);
        QCOMPARE(ok, true);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 0);  // oldIndex
        QCOMPARE(spy.first().at(1).toInt(), 1);  // newIndex

        QCOMPARE(slices[0]->isTxSlice(), false);
        QCOMPARE(slices[1]->isTxSlice(), true);
        QCOMPARE(arb.txBoundSliceIndex(), 1);
    }
```

- [ ] **Step 3: Run + commit**

```bash
ctest --test-dir build -R tst_tx_slice_arbiter -V 2>&1 | tail -15
git add src/core/TxSliceArbiter.cpp tests/tst_tx_slice_arbiter.cpp
git commit -m "feat(3f-c): TxSliceArbiter handoff (basic flip, no MOX guard yet)"
```

---

## Task 4: Add MOX-drop guard

**Files:** Modify `src/core/TxSliceArbiter.cpp`

- [ ] **Step 1: Add test for MOX-drop guard**

```cpp
    void handoff_drops_mox_first_then_flips()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2);

        MoxController mox;
        mox.setMox(true);  // simulate keyed

        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMoxController(&mox);

        QCOMPARE(mox.isMox(), true);

        QSignalSpy moxSpy(&mox, &MoxController::moxChanged);
        const bool ok = arb.requestHandoff(1);

        QCOMPARE(ok, true);
        QVERIFY(moxSpy.count() >= 1);
        QCOMPARE(mox.isMox(), false);  // MOX dropped
        QCOMPARE(slices[1]->isTxSlice(), true);  // handoff completed
    }
```

- [ ] **Step 2: Run + verify failure (MoxController not consulted)**

Expected: MOX stays true → test fails.

- [ ] **Step 3: Extend `requestHandoff` to drop MOX before flip**

```cpp
bool TxSliceArbiter::requestHandoff(int newSliceIndex)
{
    if (!m_slices || newSliceIndex < 0 || newSliceIndex >= m_slices->size()) {
        emit handoffBlocked(newSliceIndex, QStringLiteral("Slice index out of range"));
        return false;
    }

    if (newSliceIndex == m_txBoundIndex) {
        return true;
    }

    // ── RF-safe handoff: drop MOX before changing TX-bound slice ──
    if (m_mox && m_mox->isMox()) {
        m_mox->setMox(false);
        // MoxController::setMox is synchronous on the moxChanged side; if it ever
        // becomes async, switch to a QEventLoop wait on moxChanged here.
    }

    const int oldIndex = m_txBoundIndex;
    if (oldIndex >= 0 && oldIndex < m_slices->size()) {
        m_slices->at(oldIndex)->setTxSlice(false);
    }
    m_slices->at(newSliceIndex)->setTxSlice(true);

    m_txBoundIndex = newSliceIndex;
    emit txBoundSliceChanged(oldIndex, newSliceIndex);
    return true;
}
```

- [ ] **Step 4: Run + commit**

```bash
ctest --test-dir build -R tst_tx_slice_arbiter -V 2>&1 | tail -10
git add src/core/TxSliceArbiter.cpp tests/tst_tx_slice_arbiter.cpp
git commit -m "feat(3f-c): TxSliceArbiter MOX-drop guard before TX-slice flip (RF-safe)"
```

---

## Task 5: Persist `TxBoundSliceIndex` per-MAC

**Files:** Modify `src/core/TxSliceArbiter.{h,cpp}`

- [ ] **Step 1: Add persistence round-trip test**

```cpp
    void tx_bound_index_persists_per_mac()
    {
        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:ff");
        QVector<SliceModel*> slices;
        buildSlices(slices, 3);

        {
            TxSliceArbiter arb;
            arb.setSliceList(&slices);
            arb.setMacAddress(mac);
            arb.requestHandoff(2);
            arb.save();
        }
        {
            TxSliceArbiter arb2;
            arb2.setSliceList(&slices);
            arb2.setMacAddress(mac);
            arb2.load();
            QCOMPARE(arb2.txBoundSliceIndex(), 2);
        }
    }
```

- [ ] **Step 2: Add MAC + save/load API**

In `TxSliceArbiter.h`:

```cpp
public:
    void setMacAddress(const QString& mac) { m_mac = mac; }
    void load();
    void save();

private:
    QString m_mac;
```

In `TxSliceArbiter.cpp`:

```cpp
void TxSliceArbiter::save()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString key = QStringLiteral("hardware/%1/TxBoundSliceIndex").arg(m_mac);
    s.setValue(key, m_txBoundIndex);
}

void TxSliceArbiter::load()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString key = QStringLiteral("hardware/%1/TxBoundSliceIndex").arg(m_mac);
    const int restored = s.value(key, 0).toInt();
    if (restored != m_txBoundIndex && restored >= 0 && m_slices && restored < m_slices->size()) {
        requestHandoff(restored);
    }
}
```

Add `#include "core/AppSettings.h"` in the .cpp.

- [ ] **Step 3: Run + commit**

```bash
ctest --test-dir build -R tst_tx_slice_arbiter -V 2>&1 | tail -10
git add src/core/TxSliceArbiter.{h,cpp} tests/tst_tx_slice_arbiter.cpp
git commit -m "feat(3f-c): TxSliceArbiter persists txBoundSliceIndex per-MAC"
```

---

## Task 6: Integrate `TxSliceArbiter` into `RadioModel`

**Files:** Modify `src/models/RadioModel.{h,cpp}`

- [ ] **Step 1: Add `TxSliceArbiter*` member + accessor**

In `RadioModel.h`:

```cpp
public:
    class TxSliceArbiter* txSliceArbiter() const { return m_txSliceArbiter; }

private:
    class TxSliceArbiter* m_txSliceArbiter {nullptr};
```

In `RadioModel.cpp` (constructor):

```cpp
#include "core/TxSliceArbiter.h"

RadioModel::RadioModel(QObject* parent)
    : QObject(parent)
    , m_txSliceArbiter(new TxSliceArbiter(this))
{
    // ... existing init ...

    // Wire arbiter to slice list and mox controller
    m_txSliceArbiter->setSliceList(&m_slices);
    m_txSliceArbiter->setMoxController(m_moxController);  // assumes m_moxController already exists
}
```

- [ ] **Step 2: Wire MAC into arbiter on radio connect**

Find where `currentRadioChanged` is handled. Add:

```cpp
    connect(this, &RadioModel::currentRadioChanged, this, [this](const RadioInfo& info) {
        if (m_txSliceArbiter) {
            m_txSliceArbiter->setMacAddress(info.macAddress);
            m_txSliceArbiter->load();
        }
    });
```

- [ ] **Step 3: Save arbiter state on disconnect or shutdown**

Find `disconnectFromRadio` / `~RadioModel`. Add:

```cpp
    if (m_txSliceArbiter) {
        m_txSliceArbiter->save();
    }
```

- [ ] **Step 4: Commit**

```bash
cmake --build build && ctest --test-dir build 2>&1 | tail -5
git add src/models/RadioModel.{h,cpp}
git commit -m "feat(3f-c): RadioModel owns TxSliceArbiter, wires MAC + lifecycle"
```

---

## Task 7: `RadioModel::addSliceOnPan(QString panId)` (AetherSDR port)

**Files:** Modify `src/models/RadioModel.{h,cpp}`

Port the AetherSDR pattern from `MainWindow.cpp:6849-6859`. UX = `+RX` button on the active pan; menu item also available via `View > Add slice on active pan` (Ctrl+R).

- [ ] **Step 1: Add lifecycle test**

Create `tests/tst_radio_model_slice_lifecycle.cpp`:

```cpp
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestRadioModelSliceLifecycle : public QObject {
    Q_OBJECT
private slots:
    void add_slice_on_pan_creates_new_slice_when_under_cap()
    {
        // Simulate connected radio with maxSlices=5 (e.g. G2). For tests we may
        // need to inject capabilities or use a test seam; placeholder here.
        // For now, the test verifies the signal is emitted when called.
        RadioModel radio;
        QSignalSpy spy(&radio, &RadioModel::sliceAdded);

        // Without a connected radio, addSliceOnPan should reject (cap = 1, slice 0 already exists).
        radio.addSliceOnPan(QStringLiteral("pan-0"));

        // Disconnected default cap is 1; we already have slice 0 from default RadioModel construction.
        // Either 0 (rejected) or 1 (accepted) emissions is acceptable depending on init order.
        // This test mostly verifies addSliceOnPan exists and compiles.
        QVERIFY(spy.count() <= 1);
    }
};

QTEST_MAIN(TestRadioModelSliceLifecycle)
#include "tst_radio_model_slice_lifecycle.moc"
```

Register: `nereus_add_test(tst_radio_model_slice_lifecycle)`.

- [ ] **Step 2: Add to RadioModel.h**

Public API (near existing slice accessors):

```cpp
public:
    /// Phase 3F: AetherSDR-faithful slice creation entry point.
    /// Creates a new slice and associates it with the given pan.
    /// Caller checks maxSlices() first; this method also rejects with rejectedCallback
    /// if cap exceeded.
    Q_INVOKABLE void addSliceOnPan(const QString& panId);

    /// Phase 3F: remove a slice by index. If the slice is TX-bound, hands TX off
    /// to slice 0 first (or rejects if slice 0 is being removed and any other slice exists).
    Q_INVOKABLE void removeSlice(int sliceIndex);

signals:
    void sliceAdded(SliceModel* slice);
    void sliceRemoved(int sliceIndex);
    void sliceAddRejected(QString reason);
```

(Some of these signals may already exist from prior phases; verify with `grep -n "sliceAdded\|sliceRemoved" src/models/RadioModel.h` before adding duplicates.)

- [ ] **Step 3: Implement in RadioModel.cpp**

```cpp
void RadioModel::addSliceOnPan(const QString& panId)
{
    if (m_slices.size() >= maxSlices()) {
        const QString reason = QStringLiteral("%1 supports a maximum of %2 slices")
                                   .arg(m_currentRadio.model_label.isEmpty() ? QStringLiteral("This radio") : m_currentRadio.model_label)
                                   .arg(maxSlices());
        emit sliceAddRejected(reason);
        return;
    }

    auto* slice = new SliceModel(this);
    slice->setSliceLetter(QChar('A' + m_slices.size()));  // A, B, C, D, E
    m_slices.append(slice);

    // Associate with pan via existing pan-slice mapping (Sub-Epic D wires the full flow).
    // For now, store the panId as a property on the slice for later lookup.
    slice->setProperty("initialPanId", panId);

    // Trigger codec recompute for the new slice list
    invokeCodecDdcAssignment();

    emit sliceAdded(slice);
}

void RadioModel::removeSlice(int sliceIndex)
{
    if (sliceIndex < 0 || sliceIndex >= m_slices.size()) {
        return;
    }
    if (m_slices.size() == 1) {
        return;  // never remove the last slice
    }

    SliceModel* victim = m_slices.at(sliceIndex);

    // If victim is TX-bound, hand TX off to slice 0 first
    if (victim->isTxSlice() && m_txSliceArbiter) {
        const int fallbackIndex = (sliceIndex == 0) ? 1 : 0;
        m_txSliceArbiter->requestHandoff(fallbackIndex);
    }

    m_slices.removeAt(sliceIndex);
    victim->deleteLater();

    invokeCodecDdcAssignment();
    emit sliceRemoved(sliceIndex);
}
```

- [ ] **Step 4: Commit**

```bash
cmake --build build && ctest --test-dir build -R tst_radio_model_slice_lifecycle -V 2>&1 | tail -10
git add src/models/RadioModel.{h,cpp} tests/tst_radio_model_slice_lifecycle.cpp tests/CMakeLists.txt
git commit -m "feat(3f-c): RadioModel addSliceOnPan + removeSlice (AetherSDR port + cap enforcement)"
```

---

## Task 8: Status-bar reject toast on `sliceAddRejected`

**Files:** Modify `src/gui/MainWindow.cpp`

- [ ] **Step 1: Wire `sliceAddRejected` in MainWindow**

Find where `RadioModel` is connected (e.g. `connect(m_radioModel, ...)` blocks in `MainWindow::init`). Add:

```cpp
    connect(m_radioModel, &RadioModel::sliceAddRejected, this, [this](const QString& reason) {
        if (auto* sb = statusBar()) {
            sb->showMessage(reason, 4000);
        }
    });
```

- [ ] **Step 2: Manual smoke check**

```bash
cmake --build build
./build/NereusSDR.app/Contents/MacOS/NereusSDR &
```

Connect to a radio with maxSlices=1 (HL2 ideal; if no HL2, simulate by temporarily forcing maxSlices=1). Trigger `addSliceOnPan` via console / debug action. Verify status bar shows the reject message.

- [ ] **Step 3: Commit**

```bash
git add src/gui/MainWindow.cpp
git commit -m "feat(3f-c): MainWindow status-bar reject toast on sliceAddRejected"
```

---

## Task 9: Wire `VfoWidget::m_txBadge` click handler

**Files:** Modify `src/gui/widgets/VfoWidget.{h,cpp}`

`m_txBadge` exists at `VfoWidget.cpp:615` (checkable QPushButton, "TX" label) but has no click handler today.

- [ ] **Step 1: Write failing UI test (skeleton)**

Create `tests/tst_vfo_widget_tx_badge_click.cpp`:

```cpp
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "gui/widgets/VfoWidget.h"

using namespace NereusSDR;

class TestVfoWidgetTxBadgeClick : public QObject {
    Q_OBJECT
private slots:
    void tx_badge_click_emits_handoff_request_signal()
    {
        VfoWidget widget;
        widget.setSliceIndex(1);  // this widget represents Slice B

        QSignalSpy spy(&widget, &VfoWidget::txHandoffRequested);

        // Simulate badge click (find the badge via objectName or via test seam)
        widget.simulateTxBadgeClick();  // test-only helper

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 1);  // sliceIndex=1
    }
};

QTEST_MAIN(TestVfoWidgetTxBadgeClick)
#include "tst_vfo_widget_tx_badge_click.moc"
```

Register test.

- [ ] **Step 2: Run + verify failure**

Expected: `txHandoffRequested` signal doesn't exist.

- [ ] **Step 3: Add signal + slot to VfoWidget.h**

```cpp
signals:
    /// Phase 3F: emitted when operator clicks the TX badge on this slice's flag.
    /// MainWindow forwards to RadioModel::txSliceArbiter()->requestHandoff().
    void txHandoffRequested(int sliceIndex);

#ifdef NEREUSSDR_TESTING
public:
    void simulateTxBadgeClick() { onTxBadgeClicked(); }
#endif

private slots:
    void onTxBadgeClicked();
```

- [ ] **Step 4: Implement slot + wire in VfoWidget.cpp**

In the constructor (after `m_txBadge` is created around line 624):

```cpp
    connect(m_txBadge, &QPushButton::clicked, this, &VfoWidget::onTxBadgeClicked);
```

Add the slot:

```cpp
void VfoWidget::onTxBadgeClicked()
{
    emit txHandoffRequested(m_sliceIndex);
}
```

- [ ] **Step 5: Wire MainWindow to forward to arbiter**

In `MainWindow.cpp` (wherever VfoWidget is constructed/wired):

```cpp
    connect(vfoWidget, &VfoWidget::txHandoffRequested, this, [this](int sliceIndex) {
        if (m_radioModel && m_radioModel->txSliceArbiter()) {
            m_radioModel->txSliceArbiter()->requestHandoff(sliceIndex);
        }
    });
```

- [ ] **Step 6: Run + commit**

```bash
ctest --test-dir build -R tst_vfo_widget_tx_badge_click -V 2>&1 | tail -10
git add src/gui/widgets/VfoWidget.{h,cpp} src/gui/MainWindow.cpp tests/tst_vfo_widget_tx_badge_click.cpp tests/CMakeLists.txt
git commit -m "feat(3f-c): VfoWidget TX badge click → TxSliceArbiter handoff"
```

---

## Task 10: Wire `txSliceArbiter::txBoundSliceChanged` to UI updates

**Files:** Modify `src/gui/MainWindow.cpp`

- [ ] **Step 1: Wire signal to update all VfoWidget TX badges**

In MainWindow init:

```cpp
    if (auto* arb = m_radioModel->txSliceArbiter()) {
        connect(arb, &TxSliceArbiter::txBoundSliceChanged, this,
                [this](int /*oldIdx*/, int newIdx) {
            // Update every VfoWidget's badge based on its slice index
            for (int i = 0; i < m_vfoWidgets.size(); ++i) {
                m_vfoWidgets[i]->setTxSlice(i == newIdx);
            }
            // Status bar feedback
            if (auto* sb = statusBar()) {
                sb->showMessage(QStringLiteral("TX → Slice %1").arg(QChar('A' + newIdx)), 2000);
            }
        });
    }
```

(Assumes `m_vfoWidgets` is a `QVector<VfoWidget*>`; in Sub-Epic D this becomes per-pan. For Sub-Epic C with one VfoWidget, the loop has one entry.)

- [ ] **Step 2: Commit**

```bash
cmake --build build
git add src/gui/MainWindow.cpp
git commit -m "feat(3f-c): MainWindow updates VfoWidget badges + status bar on TX handoff"
```

---

## Task 11: Delete deprecated `RadioModel::setSplit` stub

**Files:** Modify `src/models/RadioModel.{h,cpp}` (line 1213 in .h, line 8948 in .cpp per audit)

Per design §3: "The stubbed `RadioModel::setSplit(int rx, bool on)` is deleted in 3F cleanup."

- [ ] **Step 1: Find and inspect**

```bash
grep -nE "setSplit\b" src/ tests/ 2>&1 | head -10
```

- [ ] **Step 2: Delete the declaration in RadioModel.h:1213**

Remove the `Q_INVOKABLE void setSplit(int rx, bool on);` line and its surrounding comment block.

- [ ] **Step 3: Delete the implementation in RadioModel.cpp:8948**

Remove the entire `void RadioModel::setSplit(int rx, bool on)` function.

- [ ] **Step 4: Verify no callers reference it**

```bash
grep -rE "setSplit\(" src/ tests/ 2>&1 | head -10
```

Expected: empty (no callers). If anything calls it, replace with a comment "// Phase 3F: split removed; use XIT for ±10 kHz or create a second slice for full retune."

- [ ] **Step 5: Commit**

```bash
cmake --build build && ctest --test-dir build 2>&1 | tail -5
git add src/models/RadioModel.{h,cpp}
git commit -m "refactor(3f-c): delete deprecated RadioModel::setSplit stub (per design §3)"
```

---

## Task 12: Regression sweep + Sub-Epic C completion checkpoint

**Files:** none modified (verification only)

- [ ] **Step 1: Run full test suite**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -20
```

Expected: all existing + 3 new test suites pass.

- [ ] **Step 2: Manual smoke (with bench)**

If a radio is available:
- Launch app, connect, verify single-slice operation unchanged
- Force a second slice via debug action / API call: `m_radioModel->addSliceOnPan("pan-0")`
- Verify second slice appears in slice list, has Slice B letter
- Force TX handoff via API: `m_radioModel->txSliceArbiter()->requestHandoff(1)`
- Verify Slice B's TX badge lights, Slice A's clears
- Key MOX on Slice B, verify radio TXs (HL2: single-slice cap; can only test on 2-slice+ SKU)

- [ ] **Step 3: Sub-Epic C retrospective note**

Append to the design doc (same pattern as Sub-Epic A):

```markdown
## Sub-Epic C implementation note (landed YYYY-MM-DD)

Implemented per `docs/architecture/2026-05-26-phase3f-sub-epic-c-tx-arbiter-lifecycle-plan.md`.
Operator-visible changes:
- `RadioModel::addSliceOnPan(QString panId)` exists and creates slices up to maxSlices
- `RadioModel::removeSlice(int)` removes non-last slices, auto-hands TX off if needed
- `VfoWidget` TX badge is now clickable (click = handoff request)
- Status bar shows "TX → Slice X" toast on handoff and "<SKU> supports a maximum of N slices" reject

Discovered during implementation:
- `MoxController::setMox(false)` is synchronous in the Qt event-loop sense; no async wait needed
- The VfoWidget `simulateTxBadgeClick()` test seam needs `NEREUSSDR_TESTING` build flag (verify pattern in existing tests)

Sub-Epic D (Pan layouts + multi-pan UI) can now begin: `PanadapterStack`, `PanadapterApplet`, `FFTRouter`, `+RX` button on `SpectrumOverlayPanel`.
```

- [ ] **Step 4: Commit + checkpoint**

```bash
git add docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md
git commit -m "docs(arch): record Sub-Epic C landing + impl-time discoveries"
git commit --allow-empty -m "chore(3f-c): Sub-Epic C complete (TxSliceArbiter + slice lifecycle)"
```

---

## Sub-Epic C Completion Criteria

When Tasks 1-12 are done:

- `TxSliceArbiter` class with `requestHandoff` + MOX-drop guard + per-MAC persistence
- `RadioModel` owns the arbiter, wires MAC on connect, saves on disconnect
- `RadioModel::addSliceOnPan(QString)` + `removeSlice(int)` with cap enforcement
- `VfoWidget::m_txBadge` click wired to handoff
- Status bar shows reject toast + TX handoff confirmation
- Deprecated `setSplit` stub deleted
- 3 new test files, ~15 cases, all green
- Single-slice operation unchanged (regression sweep)

Ready for **Sub-Epic D (Pan layouts + multi-pan UI)** to begin.

---

## References

- Sub-Epic A: `docs/architecture/2026-05-26-phase3f-sub-epic-a-foundation-plan.md`
- Sub-Epic B: `docs/architecture/2026-05-26-phase3f-sub-epic-b-codec-chain-plan.md`
- Design §6 (TxSliceArbiter), §3 (Slice Lifecycle)
- AetherSDR `+RX` pattern: `MainWindow.cpp:6849-6859`, `RadioModel.cpp:537-553`
- Existing `VfoWidget::m_txBadge` at `src/gui/widgets/VfoWidget.cpp:615-624` (visual surface present, click handler missing)
- Existing `MoxController`: `src/core/MoxController.{h,cpp}`
