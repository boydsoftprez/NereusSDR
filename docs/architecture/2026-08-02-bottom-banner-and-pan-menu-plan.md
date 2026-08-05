# Bottom Banner Cleanup + AetherSDR-Shaped Pan Menu: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Cut the bottom banner from ~1740 px to ~1286 px, replace three
competing responsive systems with one pure-function layout authority, and
replace the text-only `+PAN` menu with AetherSDR's painted thumbnail grid.

**Architecture:** A new `ChromeFoldPlan` holds the fold math as pure
functions over a width table, so it is unit-testable with no widgets.
`ChromeBarController` owns the item table and applies visibility from that
plan in a single pass. Banner items are widened or narrowed only by explicit
content change, never by layout pressure, so a given bar width always yields
exactly one visible set. Part B extends the existing AetherSDR-derived
`PanLayoutDialog` with painted layout thumbnails and per-board capacity
gating, and adds four new layouts to `PanadapterStack`.

**Tech Stack:** C++20, Qt6 (Widgets, Test), CMake + Ninja, `nereus_add_test()`
harness, `AppSettings` (never `QSettings`).

**Design spec:** `docs/architecture/2026-08-02-bottom-banner-and-pan-menu-design.md`

## Global Constraints

* **Design authority is the spec above.** Every decision is recorded in its
  §11 decision log. Do not relitigate a decision; if the spec is wrong, stop
  and ask.
* **Not a Thetis port.** Part A is NereusSDR-original UI. Part B is an
  AetherSDR port and follows §10 of the spec.
* **AetherSDR cite stamp is `[@c6481cb]`** (tag `v26.6.1-512-gc6481cbf`).
  Every new `// From AetherSDR <file>:<line> [@c6481cb]` cite uses that exact
  stamp. Gateware fact cites use `[@8e86a61]`.
* **AetherSDR has no per-file headers.** `HOW-TO-PORT.md` rule 6 applies:
  project-level attribution only. Never fabricate a verbatim block.
* **New NereusSDR-original files** get a `// no-port-check: NereusSDR-original.`
  first line, or the pre-commit `check-new-ports.py` gate fails.
* **GPG-sign every commit.** Never `--no-gpg-sign`.
* **No `Co-Authored-By: Claude` trailer** in any commit message.
* **No em-dash (`—`) in drafted text**: commit messages, design docs, PR
  bodies, and any prose written for a human to read. This does NOT extend to
  UI placeholder glyphs. `QStringLiteral("—")` is this codebase's dominant
  "no reading yet" placeholder, with 61 call sites in `src/` against 16 for
  `"--"`, and new widgets match it. Ruling by JJ / KG4VCF, 2026-08-02.
* **No source cites inside user-visible strings.** Tooltips and labels stay
  plain English; the cite goes in a comment next to the string.
* **`AppSettings`, never `QSettings`.** Boolean values are the strings
  `"True"` / `"False"`.
* **C++ style:** braces on all control flow, no raw `new`/`delete` outside Qt
  parent ownership, `constexpr` not `#define`, members `m_camelCase`,
  constants `kPascalCase`, classes `PascalCase`.
* **Test build is per-target.** Never `cmake --build build` expecting tests;
  test executables are `EXCLUDE_FROM_ALL`. Build one with
  `cmake --build build --target tst_<name>`. Read
  `docs/development/fast-test-loop.md` before running anything broad; the
  full suite is ~32 min and almost nothing needs it.

---

## File Structure

### Created

| File | Responsibility |
| --- | --- |
| `src/gui/chrome/ChromeFoldPlan.h` / `.cpp` | Pure fold math over a width table. No Qt widgets. |
| `src/gui/chrome/ChromeBarController.h` / `.cpp` | Owns the banner item table; applies visibility from a fold plan. |
| `src/gui/widgets/SystemTile.h` / `.cpp` | Merged PA telemetry + CPU two-row tile. |
| `src/gui/widgets/LayoutThumbnail.h` / `.cpp` | Paints one pan-layout geometry preview. |
| `tests/tst_chrome_fold_plan.cpp` | Fold math, monotonicity, no-oscillation. |
| `tests/tst_chrome_bar_controller.cpp` | Visibility application, idempotency. |
| `tests/tst_system_tile.cpp` | Row rules for the merged tile. |
| `tests/tst_layout_thumbnail.cpp` | Cell geometry per layout id. |
| `tests/tst_pan_layout_dialog_gating.cpp` | Per-board hide + footer text. |
| `tests/tst_pan_menu_routing.cpp` | Per-pan add-slice / float targeting. |

### Modified

| File | Change |
| --- | --- |
| `src/gui/MainWindow.cpp` | `buildStatusBar()` rebuilt around the controller; `showPanMenu()` retired; `panIdsForLayout()` extended. |
| `src/gui/MainWindow.h` | Drop the two ladder members; add the controller. |
| `src/gui/widgets/StationBlock.h` / `.cpp` | Second row for model and firmware. |
| `src/gui/widgets/RxDashboard.h` / `.cpp` | Dense row, slice tag, active-slice binding; internal ladder deleted. |
| `src/gui/PanLayoutDialog.h` / `.cpp` | Thumbnail grid, nine layouts, capacity gating. |
| `src/gui/PanadapterStack.cpp` | Four new layouts. |
| `src/gui/PanadapterApplet.cpp` | Per-pan context menu entries. |
| `src/gui/TitleBar.h` / `.cpp` | Single-row UTC clock. |
| `tests/CMakeLists.txt` | Register the six new tests. |
| `docs/attribution/aethersdr-contributor-index.md` | Row 270 status update. |

---

# Phase A: The banner

Phase A is independently shippable. It does not touch any Part B file.

## Task A1: Fold math as a pure function

**Files:**
- Create: `src/gui/chrome/ChromeFoldPlan.h`, `src/gui/chrome/ChromeFoldPlan.cpp`
- Test: `tests/tst_chrome_fold_plan.cpp`
- Modify: `tests/CMakeLists.txt`, `CMakeLists.txt` (source list)

**Interfaces:**
- Consumes: nothing.
- Produces: `ChromeFoldEntry { int rung; int widthPx; QString label; }` and
  `ChromeFoldPlan::requiredWidth(items, foldThroughRung) -> int`,
  `ChromeFoldPlan::planFold(items, barWidthPx) -> int`,
  `ChromeFoldPlan::foldedLabels(items, foldThroughRung) -> QStringList`.
  Task A2 consumes all three.

**Rung numbering** (spec §6, one integer per fold step, same rung folds together):

| Rung | Item |
| ---: | --- |
| 0 | never folds |
| 1 | system tile |
| 2 | TGXL chip |
| 3 | CAT and TCI (both entries carry rung 3) |
| 4 | CH chain tags |
| 5 | SQL pill |
| 6 | APF pill |
| 7 | NB pill |
| 8 | NR pill |
| 9 | AGC pill |
| 10 | placeholder row |

- [ ] **Step 1: Write the failing test**

Create `tests/tst_chrome_fold_plan.cpp`:

```cpp
// no-port-check: NereusSDR-original. No upstream port.
#include <QtTest/QtTest>
#include "gui/chrome/ChromeFoldPlan.h"

using namespace NereusSDR;

namespace {
QVector<ChromeFoldEntry> sampleTable()
{
    return {
        {0, 100, QStringLiteral("anchor")},
        {1,  60, QStringLiteral("System")},
        {2,  62, QStringLiteral("TGXL")},
        {3,  60, QStringLiteral("CAT")},
        {3,  60, QStringLiteral("TCI")},
        {10, 122, QStringLiteral("Placeholders")},
    };
}
} // namespace

class TstChromeFoldPlan : public QObject {
    Q_OBJECT
private slots:
    void requiredWidthCountsGapsAndPadding() {
        // 6 entries: 464 px of content + 5 gaps * 6 px + 12 px padding.
        QCOMPARE(ChromeFoldPlan::requiredWidth(sampleTable(), 0), 506);
    }

    void foldingRungOneRemovesThatEntry() {
        // Drop the 60 px system tile and one 6 px gap.
        QCOMPARE(ChromeFoldPlan::requiredWidth(sampleTable(), 1), 440);
    }

    void sameRungFoldsTogether() {
        // Rung 3 holds CAT and TCI; folding through 3 removes both.
        const int through2 = ChromeFoldPlan::requiredWidth(sampleTable(), 2);
        const int through3 = ChromeFoldPlan::requiredWidth(sampleTable(), 3);
        QCOMPARE(through2 - through3, 132);  // 60 + 60 + two 6 px gaps
    }

    void rungZeroNeverFolds() {
        const QVector<ChromeFoldEntry> t = sampleTable();
        // Even folding through every rung leaves the anchor.
        QCOMPARE(ChromeFoldPlan::requiredWidth(t, 99), 112);  // 100 + 12 padding
    }

    void planFoldReturnsZeroWhenEverythingFits() {
        QCOMPARE(ChromeFoldPlan::planFold(sampleTable(), 2000), 0);
    }

    void planFoldStopsAtFirstFittingRung() {
        // 440 px fits exactly once rung 1 is folded, so rung 2 must not fire.
        QCOMPARE(ChromeFoldPlan::planFold(sampleTable(), 440), 1);
    }

    void planFoldIsMonotonic() {
        const QVector<ChromeFoldEntry> t = sampleTable();
        int prev = 0;
        for (int w = 2000; w >= 100; --w) {
            const int rung = ChromeFoldPlan::planFold(t, w);
            QVERIFY2(rung >= prev,
                     qPrintable(QStringLiteral("rung went backwards at w=%1").arg(w)));
            prev = rung;
        }
    }

    void planFoldIsDeterministic() {
        const QVector<ChromeFoldEntry> t = sampleTable();
        for (int w = 100; w <= 2000; ++w) {
            QCOMPARE(ChromeFoldPlan::planFold(t, w), ChromeFoldPlan::planFold(t, w));
        }
    }

    void foldedLabelsListsWhatWent() {
        const QStringList got = ChromeFoldPlan::foldedLabels(sampleTable(), 3);
        QCOMPARE(got, (QStringList{QStringLiteral("System"),
                                   QStringLiteral("TGXL"),
                                   QStringLiteral("CAT"),
                                   QStringLiteral("TCI")}));
    }
};
QTEST_MAIN(TstChromeFoldPlan)
#include "tst_chrome_fold_plan.moc"
```

- [ ] **Step 2: Register the test and run it to verify it fails**

Add to `tests/CMakeLists.txt`, near the other GUI-widget registrations:

```cmake
nereus_add_test(tst_chrome_fold_plan)
```

Run:

```bash
cmake --build build --target tst_chrome_fold_plan
```

Expected: FAIL to compile, `gui/chrome/ChromeFoldPlan.h` not found.

- [ ] **Step 3: Write the header**

Create `src/gui/chrome/ChromeFoldPlan.h`:

```cpp
// no-port-check: NereusSDR-original. No upstream port. Pure fold math
// for the bottom-banner layout authority; see
// docs/architecture/2026-08-02-bottom-banner-and-pan-menu-design.md §5.

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace NereusSDR {

/// One banner item's contribution to the width budget.
///
/// rung 0 never folds. Rungs 1..N fold in ascending order; entries sharing
/// a rung fold together (CAT and TCI, per design §6).
struct ChromeFoldEntry {
    int     rung{0};
    int     widthPx{0};
    QString label;
};

/// Fold decisions as pure functions over a width table.
///
/// Deliberately free of QWidget so the whole ladder is testable without a
/// GUI, and so no decision can read a live sizeHint that a previous fold
/// step just changed. That feedback path is what made the three systems
/// this replaces oscillate.
class ChromeFoldPlan {
public:
    /// Layout spacing, mirroring buildStatusBar()'s QHBoxLayout.
    static constexpr int kGapPx = 6;
    /// Left plus right content margin, mirroring setContentsMargins(6,0,6,0).
    static constexpr int kPadPx = 12;

    /// Width the bar needs with every rung up to and including
    /// foldThroughRung hidden. Pass 0 for "nothing folded".
    static int requiredWidth(const QVector<ChromeFoldEntry>& items,
                             int foldThroughRung);

    /// Lowest rung that makes the bar fit in barWidthPx, or 0 if it already
    /// fits. Returns the highest rung present when nothing makes it fit.
    static int planFold(const QVector<ChromeFoldEntry>& items,
                        int barWidthPx);

    /// Labels of every entry hidden at foldThroughRung, in rung order.
    static QStringList foldedLabels(const QVector<ChromeFoldEntry>& items,
                                    int foldThroughRung);
};

} // namespace NereusSDR
```

- [ ] **Step 4: Write the implementation**

Create `src/gui/chrome/ChromeFoldPlan.cpp`:

```cpp
// no-port-check: NereusSDR-original. No upstream port.

// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/chrome/ChromeFoldPlan.h"

#include <algorithm>

namespace NereusSDR {

int ChromeFoldPlan::requiredWidth(const QVector<ChromeFoldEntry>& items,
                                  int foldThroughRung)
{
    int content = 0;
    int visible = 0;
    for (const ChromeFoldEntry& e : items) {
        if (e.rung != 0 && e.rung <= foldThroughRung) {
            continue;
        }
        content += e.widthPx;
        ++visible;
    }
    if (visible == 0) {
        return kPadPx;
    }
    return content + kGapPx * (visible - 1) + kPadPx;
}

int ChromeFoldPlan::planFold(const QVector<ChromeFoldEntry>& items,
                             int barWidthPx)
{
    int maxRung = 0;
    for (const ChromeFoldEntry& e : items) {
        maxRung = std::max(maxRung, e.rung);
    }
    for (int rung = 0; rung <= maxRung; ++rung) {
        if (requiredWidth(items, rung) <= barWidthPx) {
            return rung;
        }
    }
    return maxRung;
}

QStringList ChromeFoldPlan::foldedLabels(const QVector<ChromeFoldEntry>& items,
                                         int foldThroughRung)
{
    QVector<ChromeFoldEntry> hidden;
    for (const ChromeFoldEntry& e : items) {
        if (e.rung != 0 && e.rung <= foldThroughRung) {
            hidden.append(e);
        }
    }
    std::stable_sort(hidden.begin(), hidden.end(),
                     [](const ChromeFoldEntry& a, const ChromeFoldEntry& b) {
                         return a.rung < b.rung;
                     });
    QStringList out;
    out.reserve(hidden.size());
    for (const ChromeFoldEntry& e : hidden) {
        if (!e.label.isEmpty()) {
            out << e.label;
        }
    }
    return out;
}

} // namespace NereusSDR
```

- [ ] **Step 5: Add the sources to the build**

In the root `CMakeLists.txt`, find the `NereusSDRObjs` source list (search for
`src/gui/widgets/MetricLabel.cpp`) and add alongside it:

```cmake
    src/gui/chrome/ChromeFoldPlan.cpp
```

- [ ] **Step 6: Run the test to verify it passes**

```bash
cmake --build build --target tst_chrome_fold_plan && ctest --test-dir build -R tst_chrome_fold_plan --output-on-failure
```

Expected: PASS, 9 of 9.

- [ ] **Step 7: Commit**

```bash
git add src/gui/chrome/ChromeFoldPlan.h src/gui/chrome/ChromeFoldPlan.cpp \
        tests/tst_chrome_fold_plan.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -S -m "feat(chrome): pure fold math for the banner layout authority

The three responsive systems in the banner each measure, mutate
themselves, then measure again, so each needed a hysteresis patch and
the unpoliced left section still overlapped. ChromeFoldPlan makes the
decision a pure function of bar width over a cached table, which is
what makes flicker impossible rather than damped.

No widgets, so the whole ladder is testable without a GUI. The
monotonicity and determinism tests are the regression gate: sweeping
every width from 2000 down to 100 must never see a rung go backwards
and must never see two calls at one width disagree."
```

---

## Task A2: The controller that applies the plan

**Files:**
- Create: `src/gui/chrome/ChromeBarController.h`, `src/gui/chrome/ChromeBarController.cpp`
- Test: `tests/tst_chrome_bar_controller.cpp`
- Modify: `tests/CMakeLists.txt`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `ChromeFoldEntry`, `ChromeFoldPlan::planFold`,
  `ChromeFoldPlan::foldedLabels` from Task A1.
- Produces: `ChromeBarController::addItem(QWidget* widget, QWidget* separator, int rung, const QString& label)`,
  `::setNaturalWidth(QWidget*, int)`, `::relayout(int barWidthPx)`,
  `::foldedLabels() const -> QStringList`, signal
  `foldStateChanged(const QStringList&)`. Task A8 consumes all of them.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_chrome_bar_controller.cpp`:

```cpp
// no-port-check: NereusSDR-original. No upstream port.
#include <QtTest/QtTest>
#include <QLabel>
#include <QSignalSpy>
#include <QWidget>
#include "gui/chrome/ChromeBarController.h"

using namespace NereusSDR;

class TstChromeBarController : public QObject {
    Q_OBJECT
private:
    QWidget* host{nullptr};

    QLabel* makeItem(int widthPx) {
        auto* l = new QLabel(host);
        l->setFixedWidth(widthPx);
        return l;
    }

private slots:
    void init() {
        host = new QWidget;
        host->resize(2000, 46);
    }

    void cleanup() {
        delete host;
        host = nullptr;
    }

    void wideBarShowsEverything() {
        ChromeBarController c;
        QLabel* anchor = makeItem(100);
        QLabel* sys    = makeItem(60);
        c.addItem(anchor, nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));
        c.relayout(2000);
        QVERIFY(!anchor->isHidden());
        QVERIFY(!sys->isHidden());
        QVERIFY(c.foldedLabels().isEmpty());
    }

    void narrowBarFoldsRungOne() {
        ChromeBarController c;
        QLabel* anchor = makeItem(100);
        QLabel* sys    = makeItem(60);
        c.addItem(anchor, nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));
        c.relayout(120);
        QVERIFY(!anchor->isHidden());
        QVERIFY(sys->isHidden());
        QCOMPARE(c.foldedLabels(), QStringList{QStringLiteral("System")});
    }

    void separatorFoldsWithItsItem() {
        ChromeBarController c;
        QLabel* sys = makeItem(60);
        QLabel* sep = makeItem(14);
        c.addItem(makeItem(100), nullptr, 0, QString());
        c.addItem(sys, sep, 1, QStringLiteral("System"));
        c.relayout(120);
        QVERIFY(sys->isHidden());
        QVERIFY(sep->isHidden());
    }

    void relayoutIsIdempotent() {
        ChromeBarController c;
        QLabel* sys = makeItem(60);
        c.addItem(makeItem(100), nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));
        c.relayout(120);
        const bool first = sys->isHidden();
        c.relayout(120);
        QCOMPARE(sys->isHidden(), first);
    }

    void noOscillationAcrossNeighbouringWidths() {
        ChromeBarController c;
        QLabel* sys = makeItem(60);
        c.addItem(makeItem(100), nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));
        for (int w = 100; w <= 400; ++w) {
            c.relayout(w);
            const bool cold = sys->isHidden();
            c.relayout(w + 1);
            c.relayout(w);
            QCOMPARE(sys->isHidden(), cold);
        }
    }

    void foldStateChangedOnlyFiresOnChange() {
        ChromeBarController c;
        c.addItem(makeItem(100), nullptr, 0, QString());
        c.addItem(makeItem(60), nullptr, 1, QStringLiteral("System"));
        QSignalSpy spy(&c, &ChromeBarController::foldStateChanged);
        c.relayout(120);
        QCOMPARE(spy.count(), 1);
        c.relayout(120);
        QCOMPARE(spy.count(), 1);
        c.relayout(2000);
        QCOMPARE(spy.count(), 2);
    }

    void setNaturalWidthChangesTheDecision() {
        ChromeBarController c;
        QLabel* sys = makeItem(60);
        c.addItem(makeItem(100), nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));
        c.relayout(180);
        QVERIFY(!sys->isHidden());
        c.setNaturalWidth(sys, 200);
        c.relayout(180);
        QVERIFY(sys->isHidden());
    }
};
QTEST_MAIN(TstChromeBarController)
#include "tst_chrome_bar_controller.moc"
```

- [ ] **Step 2: Register and run to verify it fails**

Add `nereus_add_test(tst_chrome_bar_controller)` to `tests/CMakeLists.txt`.

```bash
cmake --build build --target tst_chrome_bar_controller
```

Expected: FAIL to compile, header not found.

- [ ] **Step 3: Write the header**

Create `src/gui/chrome/ChromeBarController.h`:

```cpp
// no-port-check: NereusSDR-original. No upstream port. Single layout
// authority for the bottom banner; replaces RxDashboard's internal
// ladder and MainWindow::reapplyRightStripDropPriority. See
// docs/architecture/2026-08-02-bottom-banner-and-pan-menu-design.md §5.

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVector>

#include "gui/chrome/ChromeFoldPlan.h"

class QWidget;

namespace NereusSDR {

/// Owns the banner's item table and applies one fold decision per resize.
///
/// Invariants this class exists to hold (design §5.1):
///   1. Nothing shrinks. An item is at its natural width or hidden.
///   2. Widths are cached, never re-measured mid-decision.
///   3. One pass per relayout. No second look, no feedback.
class ChromeBarController : public QObject {
    Q_OBJECT

public:
    explicit ChromeBarController(QObject* parent = nullptr);

    /// Register a banner item. rung 0 never folds. separator may be null;
    /// when present it is hidden and shown with its item so the dot run
    /// never dangles. Natural width is taken from the widget's sizeHint at
    /// registration; override it later with setNaturalWidth.
    void addItem(QWidget* widget, QWidget* separator, int rung,
                 const QString& overflowLabel);

    /// Update a cached width after a content change (a new PA reading with
    /// more digits, a longer radio name). Explicit, because an implicit
    /// re-measure is the feedback path this design removes.
    void setNaturalWidth(QWidget* widget, int px);

    /// Apply the fold decision for this bar width. Idempotent.
    void relayout(int barWidthPx);

    /// Labels of everything currently folded, in rung order.
    QStringList foldedLabels() const { return m_foldedLabels; }

    /// Rung currently folded through. 0 means nothing is folded.
    int foldedThroughRung() const noexcept { return m_foldedThrough; }

signals:
    /// Emitted only when the folded set actually changes, so OverflowChip
    /// is not rewritten on every resize event.
    void foldStateChanged(const QStringList& foldedLabels);

private:
    struct Registered {
        QWidget* widget{nullptr};
        QWidget* separator{nullptr};
        int      rung{0};
        int      naturalWidth{0};
        QString  label;
    };

    QVector<ChromeFoldEntry> buildTable() const;

    QVector<Registered>    m_items;
    QHash<QWidget*, int>   m_indexByWidget;
    QStringList            m_foldedLabels;
    int                    m_foldedThrough{-1};
};

} // namespace NereusSDR
```

- [ ] **Step 4: Write the implementation**

Create `src/gui/chrome/ChromeBarController.cpp`:

```cpp
// no-port-check: NereusSDR-original. No upstream port.

// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/chrome/ChromeBarController.h"

#include <QWidget>

namespace NereusSDR {

ChromeBarController::ChromeBarController(QObject* parent)
    : QObject(parent)
{
}

void ChromeBarController::addItem(QWidget* widget, QWidget* separator,
                                  int rung, const QString& overflowLabel)
{
    if (!widget) {
        return;
    }
    Registered r;
    r.widget       = widget;
    r.separator    = separator;
    r.rung         = rung;
    r.naturalWidth = widget->sizeHint().width();
    r.label        = overflowLabel;
    if (separator) {
        r.naturalWidth += separator->sizeHint().width() + ChromeFoldPlan::kGapPx;
    }
    m_indexByWidget.insert(widget, m_items.size());
    m_items.append(r);
    // A new item invalidates the last decision.
    m_foldedThrough = -1;
}

void ChromeBarController::setNaturalWidth(QWidget* widget, int px)
{
    const auto it = m_indexByWidget.constFind(widget);
    if (it == m_indexByWidget.constEnd()) {
        return;
    }
    Registered& r = m_items[*it];
    if (r.naturalWidth == px) {
        return;
    }
    r.naturalWidth = px;
    // Content width moved, so the previous decision no longer applies.
    m_foldedThrough = -1;
}

QVector<ChromeFoldEntry> ChromeBarController::buildTable() const
{
    QVector<ChromeFoldEntry> table;
    table.reserve(m_items.size());
    for (const Registered& r : m_items) {
        table.append(ChromeFoldEntry{r.rung, r.naturalWidth, r.label});
    }
    return table;
}

void ChromeBarController::relayout(int barWidthPx)
{
    const QVector<ChromeFoldEntry> table = buildTable();
    const int rung = ChromeFoldPlan::planFold(table, barWidthPx);
    if (rung == m_foldedThrough) {
        return;
    }
    m_foldedThrough = rung;

    for (const Registered& r : m_items) {
        const bool hide = (r.rung != 0 && r.rung <= rung);
        r.widget->setVisible(!hide);
        if (r.separator) {
            r.separator->setVisible(!hide);
        }
    }

    const QStringList labels = ChromeFoldPlan::foldedLabels(table, rung);
    if (labels != m_foldedLabels) {
        m_foldedLabels = labels;
        emit foldStateChanged(m_foldedLabels);
    }
}

} // namespace NereusSDR
```

- [ ] **Step 5: Add the source to the build**

In the root `CMakeLists.txt`, next to the `ChromeFoldPlan.cpp` entry:

```cmake
    src/gui/chrome/ChromeBarController.cpp
```

- [ ] **Step 6: Run the test to verify it passes**

```bash
cmake --build build --target tst_chrome_bar_controller && ctest --test-dir build -R tst_chrome_bar_controller --output-on-failure
```

Expected: PASS, 7 of 7.

- [ ] **Step 7: Commit**

```bash
git add src/gui/chrome/ChromeBarController.h src/gui/chrome/ChromeBarController.cpp \
        tests/tst_chrome_bar_controller.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -S -m "feat(chrome): ChromeBarController applies one fold decision per resize

Owns the banner item table and turns ChromeFoldPlan's decision into
setVisible calls. Separators hide with their item so the dot run never
dangles, and foldStateChanged fires only on an actual change so
OverflowChip is not rewritten on every resize event.

Width updates are explicit via setNaturalWidth rather than an implicit
re-measure. That is the point: an implicit re-measure is the feedback
path that made the systems this replaces oscillate."
```

---

## Task A3: SystemTile, merging PA telemetry with CPU

**Files:**
- Create: `src/gui/widgets/SystemTile.h`, `src/gui/widgets/SystemTile.cpp`
- Test: `tests/tst_system_tile.cpp`
- Modify: `tests/CMakeLists.txt`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `MetricLabel` (`src/gui/widgets/MetricLabel.h`), existing:
  `MetricLabel(const QString& label, const QString& value, QWidget*)`,
  `setLabel(const QString&)`, `setValue(const QString&)`, `label()`, `value()`.
- Produces: `SystemTile::setPaVolts(double)`, `::setPaTempCelsius(double)`,
  `::clearPaVolts()`, `::clearPaTemp()`, `::setCpuPercent(double)`,
  `::setPaLabel(const QString&)`, `::paRowText() const -> QString`,
  `::cpuRowText() const -> QString`, `::hasPaRow() const -> bool`,
  signal `paTempClicked()`. Task A8 consumes all of them.

**Behaviour rules (design §4.3):**
- Row 1 is PA telemetry: volts on MKII-class, temperature on HL2, both when
  a board publishes both, hidden when it publishes neither.
- Row 2 is CPU, always present.
- Temperature formatting goes through `PaTempUnitNotifier::format`.
- Clicking row 1 emits `paTempClicked()` only while row 1 carries temperature.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_system_tile.cpp`:

```cpp
// no-port-check: NereusSDR-original. No upstream port.
#include <QtTest/QtTest>
#include "gui/widgets/SystemTile.h"

using namespace NereusSDR;

class TstSystemTile : public QObject {
    Q_OBJECT
private slots:
    void cpuOnlyWhenBoardHasNoPaTelemetry() {
        SystemTile t;
        t.setCpuPercent(19.0);
        QVERIFY(!t.hasPaRow());
        QCOMPARE(t.cpuRowText(), QStringLiteral("19%"));
    }

    void voltsOnlyOnMkiiClass() {
        SystemTile t;
        t.setPaVolts(13.8);
        t.setCpuPercent(19.0);
        QVERIFY(t.hasPaRow());
        QCOMPARE(t.paRowText(), QStringLiteral("13.8V"));
    }

    void tempOnlyOnHl2() {
        SystemTile t;
        t.setPaTempCelsius(42.5);
        t.setCpuPercent(19.0);
        QVERIFY(t.hasPaRow());
        QVERIFY(t.paRowText().contains(QStringLiteral("42.5")));
    }

    void bothReadingsShareRowOne() {
        SystemTile t;
        t.setPaVolts(13.8);
        t.setPaTempCelsius(42.5);
        QVERIFY(t.hasPaRow());
        QVERIFY2(t.paRowText().contains(QStringLiteral("13.8V")),
                 qPrintable(t.paRowText()));
        QVERIFY2(t.paRowText().contains(QStringLiteral("42.5")),
                 qPrintable(t.paRowText()));
    }

    void cpuSurvivesWhenBothPaReadingsArrive() {
        SystemTile t;
        t.setCpuPercent(19.0);
        t.setPaVolts(13.8);
        t.setPaTempCelsius(42.5);
        QCOMPARE(t.cpuRowText(), QStringLiteral("19%"));
    }

    void clearingBothHidesRowOne() {
        SystemTile t;
        t.setPaVolts(13.8);
        t.setPaTempCelsius(42.5);
        t.clearPaVolts();
        t.clearPaTemp();
        QVERIFY(!t.hasPaRow());
    }

    void paLabelIsSettableForG2ePsu() {
        SystemTile t;
        t.setPaLabel(QStringLiteral("PSU"));
        t.setPaVolts(13.4);
        QCOMPARE(t.paLabel(), QStringLiteral("PSU"));
    }
};
QTEST_MAIN(TstSystemTile)
#include "tst_system_tile.moc"
```

- [ ] **Step 2: Register and run to verify it fails**

Add `nereus_add_test(tst_system_tile)` to `tests/CMakeLists.txt`.

```bash
cmake --build build --target tst_system_tile
```

Expected: FAIL to compile, `gui/widgets/SystemTile.h` not found.

- [ ] **Step 3: Write the header**

Create `src/gui/widgets/SystemTile.h`:

```cpp
// no-port-check: NereusSDR-original. No upstream port. Merges the PA
// telemetry stack with the CPU metric into one two-row tile; see
// docs/architecture/2026-08-02-bottom-banner-and-pan-menu-design.md §4.3.

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QWidget>

namespace NereusSDR {

class MetricLabel;

/// Two-row banner tile: PA telemetry over CPU.
///
/// The PA stack it replaces was already two rows, but the rows were
/// mutually exclusive in practice: volts fills only on MKII-class boards,
/// temperature only on HL2. CPU moves into the row that was always empty,
/// which reclaims roughly 100 px including a separator.
class SystemTile : public QWidget {
    Q_OBJECT

public:
    explicit SystemTile(QWidget* parent = nullptr);

    void setPaVolts(double volts);
    void clearPaVolts();
    void setPaTempCelsius(double celsius);
    void clearPaTemp();
    void setCpuPercent(double percent);

    /// "PA" on most boards, "PSU" on the ANAN-G2E supply_volts path.
    void setPaLabel(const QString& label);
    QString paLabel() const;

    /// Row-one value text, empty when the row is hidden.
    QString paRowText() const;
    /// Row-two value text.
    QString cpuRowText() const;
    /// False when the board publishes neither volts nor temperature.
    bool hasPaRow() const noexcept { return m_hasVolts || m_hasTemp; }

    /// Re-render row one from cached values. Call on a unit toggle.
    void refreshPaRow();

signals:
    /// Emitted on a click while row one is carrying temperature.
    void paTempClicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    MetricLabel* m_paRow{nullptr};
    MetricLabel* m_cpuRow{nullptr};

    bool   m_hasVolts{false};
    bool   m_hasTemp{false};
    double m_volts{0.0};
    double m_celsius{0.0};
};

} // namespace NereusSDR
```

- [ ] **Step 4: Write the implementation**

Create `src/gui/widgets/SystemTile.cpp`:

```cpp
// no-port-check: NereusSDR-original. No upstream port.

// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/widgets/SystemTile.h"

// The notifier class is PaTempUnitNotifier, but it lives in PaTempUnit.h
// alongside the PaTempUnit enum. There is no PaTempUnitNotifier.h.
#include "core/PaTempUnit.h"
#include "gui/widgets/MetricLabel.h"

#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

namespace NereusSDR {

SystemTile::SystemTile(QWidget* parent)
    : QWidget(parent)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    m_paRow = new MetricLabel(QStringLiteral("PA"), QString(), this);
    m_paRow->setVisible(false);
    vbox->addWidget(m_paRow);

    m_cpuRow = new MetricLabel(QStringLiteral("CPU"), QStringLiteral("—"), this);
    vbox->addWidget(m_cpuRow);

    // MetricLabel's child QLabels would otherwise eat the press before it
    // reaches mousePressEvent. Same treatment the old PA-T row needed.
    for (QLabel* child : findChildren<QLabel*>()) {
        child->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
}

void SystemTile::setPaVolts(double volts)
{
    m_volts    = volts;
    m_hasVolts = true;
    refreshPaRow();
}

void SystemTile::clearPaVolts()
{
    m_hasVolts = false;
    refreshPaRow();
}

void SystemTile::setPaTempCelsius(double celsius)
{
    m_celsius = celsius;
    m_hasTemp = true;
    refreshPaRow();
}

void SystemTile::clearPaTemp()
{
    m_hasTemp = false;
    refreshPaRow();
}

void SystemTile::setCpuPercent(double percent)
{
    m_cpuRow->setValue(QString::asprintf("%.0f%%", percent));
}

void SystemTile::setPaLabel(const QString& label)
{
    m_paRow->setLabel(label);
}

QString SystemTile::paLabel() const
{
    return m_paRow->label();
}

QString SystemTile::paRowText() const
{
    return m_paRow->value();
}

QString SystemTile::cpuRowText() const
{
    return m_cpuRow->value();
}

void SystemTile::refreshPaRow()
{
    if (!hasPaRow()) {
        m_paRow->setValue(QString());
        m_paRow->setVisible(false);
        setToolTip(QString());
        return;
    }

    QString text;
    if (m_hasVolts) {
        text = QString::asprintf("%.1fV", m_volts);
    }
    if (m_hasTemp) {
        // Both readings share row one rather than evicting CPU (design §4.3).
        if (!text.isEmpty()) {
            text += QLatin1Char(' ');
        }
        text += PaTempUnitNotifier::format(m_celsius);
    }
    m_paRow->setValue(text);
    m_paRow->setVisible(true);

    setCursor(m_hasTemp ? Qt::PointingHandCursor : Qt::ArrowCursor);
    setToolTip(m_hasTemp ? tr("Click to toggle °C / °F") : QString());
}

void SystemTile::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_hasTemp) {
        emit paTempClicked();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

} // namespace NereusSDR
```

- [ ] **Step 5: Add the source to the build**

In the root `CMakeLists.txt`, alongside `src/gui/widgets/MetricLabel.cpp`:

```cmake
    src/gui/widgets/SystemTile.cpp
```

- [ ] **Step 6: Run the test to verify it passes**

```bash
cmake --build build --target tst_system_tile && ctest --test-dir build -R tst_system_tile --output-on-failure
```

Expected: PASS, 7 of 7.

If `MetricLabel::label()` or `::value()` turn out not to be public accessors,
add them as `const noexcept` getters returning `m_label` / `m_value`;
`tests/tst_metric_label.cpp` already calls both, so they exist.

- [ ] **Step 7: Commit**

```bash
git add src/gui/widgets/SystemTile.h src/gui/widgets/SystemTile.cpp \
        tests/tst_system_tile.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -S -m "feat(chrome): SystemTile merges PA telemetry with CPU

The PA stack was already two rows, but the rows are mutually exclusive
in practice: volts fills only on MKII-class boards, temperature only on
HL2, so one row is always empty on any given radio. CPU moves into it.

Two tiles plus a separator at about 160 px becomes one tile at about
60 px. A board publishing both readings puts both in row one rather
than evicting CPU, and a board publishing neither shows a CPU-only tile
instead of an empty one."
```

---

## Task A4: StationBlock second row

**Files:**
- Modify: `src/gui/widgets/StationBlock.h`, `src/gui/widgets/StationBlock.cpp`
- Test: `tests/tst_station_block.cpp` (create if absent)
- Modify: `tests/CMakeLists.txt` (only if creating the test file)

**Interfaces:**
- Consumes: existing `StationBlock::setRadioName(const QString&)`,
  `::radioName()`, `::isConnectedAppearance()`, signals `clicked()` and
  `contextMenuRequested(const QPoint&)`.
- Produces: `StationBlock::setHardwareLine(const QString& model, const QString& firmware)`,
  `::hardwareLine() const -> QString`. Task A8 consumes both.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_station_block.cpp` (or append these slots if the file
already exists):

```cpp
// no-port-check: NereusSDR-original. No upstream port.
#include <QtTest/QtTest>
#include "gui/widgets/StationBlock.h"

using namespace NereusSDR;

class TstStationBlock : public QObject {
    Q_OBJECT
private slots:
    void hardwareLineJoinsModelAndFirmware() {
        StationBlock b;
        b.setHardwareLine(QStringLiteral("ANAN-G2"), QStringLiteral("v27"));
        QCOMPARE(b.hardwareLine(), QStringLiteral("ANAN-G2 · v27"));
    }

    void modelAloneOmitsTheSeparator() {
        StationBlock b;
        b.setHardwareLine(QStringLiteral("ANAN-G2"), QString());
        QCOMPARE(b.hardwareLine(), QStringLiteral("ANAN-G2"));
    }

    void firmwareAloneOmitsTheSeparator() {
        StationBlock b;
        b.setHardwareLine(QString(), QStringLiteral("v27"));
        QCOMPARE(b.hardwareLine(), QStringLiteral("v27"));
    }

    void bothEmptyGivesAnEmptyLine() {
        StationBlock b;
        b.setHardwareLine(QString(), QString());
        QCOMPARE(b.hardwareLine(), QString());
    }

    void clearingTheNameAlsoClearsTheHardwareLine() {
        StationBlock b;
        b.setRadioName(QStringLiteral("Nereus G2"));
        b.setHardwareLine(QStringLiteral("ANAN-G2"), QStringLiteral("v27"));
        b.setRadioName(QString());
        QCOMPARE(b.hardwareLine(), QString());
        QVERIFY(!b.isConnectedAppearance());
    }
};
QTEST_MAIN(TstStationBlock)
#include "tst_station_block.moc"
```

- [ ] **Step 2: Register if new, then run to verify it fails**

If you created the file, add `nereus_add_test(tst_station_block)` to
`tests/CMakeLists.txt`.

```bash
cmake --build build --target tst_station_block
```

Expected: FAIL to compile, `setHardwareLine` not a member.

- [ ] **Step 3: Extend the header**

In `src/gui/widgets/StationBlock.h`, add to the public section after
`isConnectedAppearance()`:

```cpp
    // Second row: "ANAN-G2 · v27". Either part may be empty; the middle dot
    // appears only when both are present. Cleared automatically whenever the
    // radio name is cleared, so the disconnected placeholder never shows a
    // stale board name.
    void setHardwareLine(const QString& model, const QString& firmware);
    QString hardwareLine() const noexcept { return m_hardwareLine; }
```

And to the private section:

```cpp
    QString m_hardwareLine;
    QLabel* m_hardwareLabel{nullptr};
```

- [ ] **Step 4: Extend the implementation**

In `src/gui/widgets/StationBlock.cpp`, the constructor currently builds a
single `m_label`. Wrap it in a `QVBoxLayout` with the new row beneath:

```cpp
    m_hardwareLabel = new QLabel(this);
    m_hardwareLabel->setAlignment(Qt::AlignCenter);
    m_hardwareLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-size: 10px; background: transparent; }"));
    m_hardwareLabel->setVisible(false);
    // Presses must reach StationBlock::mousePressEvent, not stop here.
    m_hardwareLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
```

Add the method:

```cpp
void StationBlock::setHardwareLine(const QString& model, const QString& firmware)
{
    QString line = model;
    if (!model.isEmpty() && !firmware.isEmpty()) {
        line += QStringLiteral(" · ");
    }
    line += firmware;

    if (m_hardwareLine == line) {
        return;
    }
    m_hardwareLine = line;
    m_hardwareLabel->setText(line);
    m_hardwareLabel->setVisible(!line.isEmpty());
}
```

In `setRadioName`, when the name is cleared, clear the hardware row too:

```cpp
    if (name.isEmpty()) {
        setHardwareLine(QString(), QString());
    }
```

Place that after `m_radioName = name;` and before `applyStyle();`.

- [ ] **Step 5: Run the test to verify it passes**

```bash
cmake --build build --target tst_station_block && ctest --test-dir build -R tst_station_block --output-on-failure
```

Expected: PASS, 5 of 5.

- [ ] **Step 6: Commit**

```bash
git add src/gui/widgets/StationBlock.h src/gui/widgets/StationBlock.cpp \
        tests/tst_station_block.cpp tests/CMakeLists.txt
git commit -S -m "feat(chrome): StationBlock carries model and firmware on a second row

Radio identity rendered twice: a stacked model-over-firmware pair in the
banner's left section with no click affordance, and the name in the
centred StationBlock. The left pair sat in the section with no drop
policy, so its width changes were what shoved its neighbours.

One identity surface now, already the ConnectionPanel click target, and
still between two flex spacers so it stays centred and grows
symmetrically. The hardware line clears with the name so the
disconnected placeholder never shows a stale board."
```

---

## Task A5: RxDashboard dense row, slice tag, active-slice binding

**Files:**
- Modify: `src/gui/widgets/RxDashboard.h`, `src/gui/widgets/RxDashboard.cpp`
- Modify: `src/gui/MainWindow.cpp:2823-2838` (the slice-0 bind)
- Test: `tests/tst_rx_dashboard.cpp` (exists; add slots)

**Interfaces:**
- Consumes: `SliceModel` signals `dspModeChanged(DSPMode)`,
  `filterChanged(int, int)`, `agcModeChanged(AGCMode)`,
  `activeNrChanged(NrSlot)`, `nbModeChanged(NbMode)`,
  `apfEnabledChanged(bool)`, `ssqlEnabledChanged(bool)`.
  `RadioModel::activeSlice()` and `RadioModel::sliceById(int)`.
- Produces: `RxDashboard::setSliceLetter(QChar)`,
  `::sliceLetter() const -> QChar`,
  `::badgeForRung(int rung) const -> StatusBadge*` used by Task A8 to
  register rungs 5 through 9. Rung mapping: 5 SQL, 6 APF, 7 NB, 8 NR, 9 AGC.

**What is deleted:** `reapplyDropPriority()`, `m_droppedBadges`,
`m_inReapplyDropPriority`, `m_lastDecisionBudget`, `m_settled`, and
`resizeEvent`. The dashboard no longer makes layout decisions; the
controller folds its badges individually.

- [ ] **Step 1: Write the failing test**

Append to `tests/tst_rx_dashboard.cpp`:

```cpp
    void sliceLetterRoundTrips() {
        RxDashboard d;
        d.setSliceLetter(QLatin1Char('B'));
        QCOMPARE(d.sliceLetter(), QLatin1Char('B'));
    }

    void badgeForRungMapsTheLadder() {
        RxDashboard d;
        QVERIFY(d.badgeForRung(5) != nullptr);   // SQL
        QVERIFY(d.badgeForRung(6) != nullptr);   // APF
        QVERIFY(d.badgeForRung(7) != nullptr);   // NB
        QVERIFY(d.badgeForRung(8) != nullptr);   // NR
        QVERIFY(d.badgeForRung(9) != nullptr);   // AGC
    }

    void modeAndFilterAreNotOnTheLadder() {
        RxDashboard d;
        // Rungs 0 through 4 belong to other banner items; the dashboard
        // must not claim mode or filter, which never fold.
        for (int rung = 0; rung <= 4; ++rung) {
            QCOMPARE(d.badgeForRung(rung), nullptr);
        }
        for (int rung = 10; rung <= 12; ++rung) {
            QCOMPARE(d.badgeForRung(rung), nullptr);
        }
    }

    void rebindingSwitchesTheObservedSlice() {
        SliceModel a(0);
        SliceModel b(1);
        RxDashboard d;
        d.bindSlice(&a);
        QCOMPARE(d.slice(), &a);
        d.bindSlice(&b);
        QCOMPARE(d.slice(), &b);
        // The old slice must no longer drive the badges.
        a.setDspMode(NereusSDR::DSPMode::CWU);
        QVERIFY(!d.modeText().contains(QStringLiteral("CW")));
    }
```

Add `QString modeText() const;` to `RxDashboard`'s public API returning the
mode badge's current label, so the last assertion has something to read.

- [ ] **Step 2: Run to verify it fails**

```bash
cmake --build build --target tst_rx_dashboard
```

Expected: FAIL to compile, `setSliceLetter` not a member.

- [ ] **Step 3: Rewrite the widget layout**

In `src/gui/widgets/RxDashboard.h`, replace the three `BadgePair` members and
the ladder state with:

```cpp
    /// Slice this dashboard is describing. Prepended to the row so a
    /// multi-pan operator can tell which slice the readings belong to.
    void setSliceLetter(QChar letter);
    QChar sliceLetter() const noexcept { return m_sliceLetter; }

    /// Mode badge text, for tests and for the overflow tooltip.
    QString modeText() const;

    /// Badge that folds at this rung, or nullptr if the rung is not ours.
    /// 5 SQL, 6 APF, 7 NB, 8 NR, 9 AGC. Mode and filter never fold.
    StatusBadge* badgeForRung(int rung) const;
```

Private members become:

```cpp
    QChar        m_sliceLetter{QLatin1Char('A')};
    QLabel*      m_sliceTag{nullptr};
    SliceModel*  m_slice{nullptr};
    StatusBadge* m_modeBadge{nullptr};
    StatusBadge* m_filterBadge{nullptr};
    StatusBadge* m_agcBadge{nullptr};
    StatusBadge* m_nrBadge{nullptr};
    StatusBadge* m_nbBadge{nullptr};
    StatusBadge* m_apfBadge{nullptr};
    StatusBadge* m_sqlBadge{nullptr};
```

Delete: `m_droppedBadges`, `m_inReapplyDropPriority`, `m_lastDecisionBudget`,
`m_settled`, `m_modeFilterPair`, `m_agcNrPair`, `m_nbApfPair`,
`reapplyDropPriority()`, and the `resizeEvent` override.

In `buildUi()`, lay the badges out in a single `QHBoxLayout` in this order,
with `setSpacing(4)` and no `BadgePair` wrappers:

```
[slice tag] [mode] [filter] | [AGC] | [NR] [NB] [APF] [SQL]
```

Style the slice tag:

```cpp
    m_sliceTag = new QLabel(QString(m_sliceLetter), this);
    m_sliceTag->setStyleSheet(QStringLiteral(
        "QLabel { color: #0a0a14; background: %1; border-radius: 3px;"
        " padding: 1px 5px; font-weight: bold; font-size: 11px; }")
        .arg(Style::kAccent));
```

- [ ] **Step 4: Implement the new accessors**

```cpp
void RxDashboard::setSliceLetter(QChar letter)
{
    if (m_sliceLetter == letter) {
        return;
    }
    m_sliceLetter = letter;
    if (m_sliceTag) {
        m_sliceTag->setText(QString(letter));
    }
}

QString RxDashboard::modeText() const
{
    return m_modeBadge ? m_modeBadge->label() : QString();
}

StatusBadge* RxDashboard::badgeForRung(int rung) const
{
    switch (rung) {
    case 5:  return m_sqlBadge;
    case 6:  return m_apfBadge;
    case 7:  return m_nbBadge;
    case 8:  return m_nrBadge;
    case 9:  return m_agcBadge;
    default: return nullptr;
    }
}
```

In `bindSlice`, disconnect the previous slice before connecting the new one so
a rebind does not leave the old slice driving the badges:

```cpp
void RxDashboard::bindSlice(SliceModel* slice)
{
    if (m_slice == slice) {
        return;
    }
    if (m_slice) {
        disconnect(m_slice, nullptr, this, nullptr);
    }
    m_slice = slice;
    if (!m_slice) {
        return;
    }
    // ... existing connect() calls, unchanged ...
    // Seed every badge from the new slice so the row is correct before the
    // first signal arrives.
    onModeChanged(static_cast<int>(m_slice->dspMode()));
    onFilterChanged(m_slice->filterLow(), m_slice->filterHigh());
    onAgcChanged(static_cast<int>(m_slice->agcMode()));
    onNrChanged(static_cast<int>(m_slice->activeNr()));
    onNbChanged(static_cast<int>(m_slice->nbMode()));
    onApfChanged(m_slice->apfEnabled());
    onSsqlChanged(m_slice->ssqlEnabled());
}
```

- [ ] **Step 5: Rebind on active-slice change**

In `src/gui/MainWindow.cpp`, replace the slice-0-only bind at lines 2823-2838:

```cpp
    // The dashboard follows the ACTIVE slice, not slice 0. It was pinned to
    // id 0 and never rebound, so after multi-pan landed (#312) an operator
    // working Slice B was shown Slice A's mode, filter, AGC and NR as
    // current. See design §4.2.
    auto rebindDashboard = [this]() {
        if (!m_rxDashboard || !m_radioModel) { return; }
        SliceModel* s = m_radioModel->activeSlice();
        if (!s) { return; }
        m_rxDashboard->bindSlice(s);
        // Use SliceModel::sliceLetter(), do NOT derive the letter here.
        // It is already derived from sliceIndex() upstream. It previously
        // returned a stored member defaulting to 'A', so every slice
        // reported 'A' and three call sites mislabelled their slices; see
        // the comment at SliceModel.h:503. Deriving locally would
        // reintroduce a second source of truth for the same fact.
        m_rxDashboard->setSliceLetter(s->sliceLetter());
    };
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [rebindDashboard](int) { rebindDashboard(); });
    connect(m_radioModel, &RadioModel::activeSliceChanged, this,
            [rebindDashboard]() { rebindDashboard(); });
    rebindDashboard();
```

If `RadioModel` exposes no `activeSliceChanged` signal, grep for the signal it
emits when the active slice changes and use that name; do not invent one.

- [ ] **Step 6: Run the test to verify it passes**

```bash
cmake --build build --target tst_rx_dashboard && ctest --test-dir build -R tst_rx_dashboard --output-on-failure
```

Expected: PASS, including the pre-existing slots.

- [ ] **Step 7: Commit**

```bash
git add src/gui/widgets/RxDashboard.h src/gui/widgets/RxDashboard.cpp \
        src/gui/MainWindow.cpp tests/tst_rx_dashboard.cpp
git commit -S -m "fix(chrome): RxDashboard follows the active slice, and goes dense

Two changes to the same widget.

The binding was a correctness bug. bindSlice ran on sliceAdded only when
sliceId == 0 and was never rebound, so since multi-pan landed in #312 an
operator working Slice B saw Slice A's mode, filter, AGC and noise
reduction presented as current. It now follows the active slice and
carries a slice-letter tag so the reading is unambiguous.

The dense row is the layout half: seven values in one row without
per-badge borders instead of three stacked bordered pairs, 268 px down
to 186. The internal three-stage ladder and its settled-flag deadband
are deleted; ChromeBarController folds the badges individually at rungs
5 through 9, and mode and filter never fold."
```

---

## Task A6: Reserved safety slots

**Files:**
- Modify: `src/gui/MainWindow.cpp` (`buildStatusBar()` safety section)
- Modify: `src/gui/MainWindow.h`
- Test: `tests/tst_mainwindow_status_bar_safety.cpp` (exists; add slots)

**Interfaces:**
- Consumes: `StatusBadge::setLabel`, `::setVariant`, `::setSvgIcon`,
  `StatusBadge::Variant::{On, Off, Tx}`, `AdcOverloadBadge::setAdcs`,
  `::setVariant`.
- Produces: `MainWindow::m_safetyGroup` (a `QWidget*` holding four
  fixed-width slots), consumed by Task A8 which registers it at rung 0.

**Rule (design §4.5):** each slot is permanently allocated at 50 px. Only the
badge inside changes appearance. An inactive slot dims its badge to 14%
opacity rather than hiding it, so no geometry moves when an alarm fires.

- [ ] **Step 1: Write the failing test**

Append to `tests/tst_mainwindow_status_bar_safety.cpp`:

```cpp
    void safetySlotsHoldGeometryWhenAnAlarmFires() {
        MainWindow w;
        w.resize(1512, 900);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        auto* txBadge = w.findChild<StatusBadge*>(QStringLiteral("txStatusBadge"));
        QVERIFY(txBadge);
        const QPoint before = txBadge->mapTo(&w, QPoint(0, 0));

        auto* ovl = w.findChild<AdcOverloadBadge*>(QStringLiteral("adcOvlBadge"));
        QVERIFY(ovl);
        ovl->setAdcs(QStringLiteral("0"));
        ovl->setVariant(AdcOverloadBadge::Variant::Tx);
        QCoreApplication::processEvents();

        const QPoint after = txBadge->mapTo(&w, QPoint(0, 0));
        QCOMPARE(after, before);
    }

    void everySafetySlotIsFixedWidth() {
        MainWindow w;
        auto* group = w.findChild<QWidget*>(QStringLiteral("safetyGroup"));
        QVERIFY(group);
        const QList<QWidget*> slots =
            group->findChildren<QWidget*>(QStringLiteral("safetySlot"),
                                          Qt::FindDirectChildrenOnly);
        QCOMPARE(slots.size(), 4);
        for (QWidget* s : slots) {
            // Assert the CONSTRAINT, not the laid-out geometry. Qt does not
            // lay out an unshown window, so width() would read the default
            // 100 here and fail for a reason that has nothing to do with
            // the fix. setFixedWidth pins both bounds, so this is the
            // property the reserved-slot design actually depends on.
            QCOMPARE(s->minimumWidth(), 50);
            QCOMPARE(s->maximumWidth(), 50);
        }
    }
```

- [ ] **Step 2: Run to verify it fails**

```bash
cmake --build build --target tst_mainwindow_status_bar_safety
```

Expected: FAIL, `safetyGroup` not found.

- [ ] **Step 3: Build the group**

In `src/gui/MainWindow.cpp`, replace the loose sequence that currently runs
from `m_txInhibitLabel` (`:6578`) through the TX badge separator (`:6705`)
with a single group. Each badge keeps its existing objectName so the existing
tests and the wiring below still find it.

```cpp
    // ── Reserved safety slots (design §4.5) ──────────────────────────────
    // Every slot is permanently allocated. Only the badge inside changes.
    // The old code inserted the overload badge BETWEEN the PA and TX badges
    // and made it visible on overload, so TX slid sideways at the exact
    // moment something went wrong. Reserving the slot fixes that: an alarm
    // now lights up in a pixel the operator has already learned.
    m_safetyGroup = new QWidget(barWidget);
    m_safetyGroup->setObjectName(QStringLiteral("safetyGroup"));
    m_safetyGroup->setStyleSheet(QStringLiteral(
        "QWidget#safetyGroup { border-left: 1px solid #203040; }"));
    auto* safetyRow = new QHBoxLayout(m_safetyGroup);
    safetyRow->setContentsMargins(8, 0, 0, 0);
    safetyRow->setSpacing(6);

    auto addSlot = [&](QWidget* badge) {
        auto* slot = new QWidget(m_safetyGroup);
        slot->setObjectName(QStringLiteral("safetySlot"));
        slot->setFixedWidth(50);
        auto* sl = new QHBoxLayout(slot);
        sl->setContentsMargins(0, 0, 0, 0);
        sl->addWidget(badge);
        badge->setParent(slot);
        safetyRow->addWidget(slot);
    };

    addSlot(m_txInhibitLabel);
    addSlot(m_paStatusBadge);
    addSlot(m_adcOvlBadge);
    addSlot(m_txStatusBadge);
    hbox->addWidget(m_safetyGroup);
```

Shorten `m_txInhibitLabel`'s text to `INH` so it fits 50 px, and move the
explanation into the tooltip, which already reads
"External TX Inhibit asserted — TX is blocked". Replace that em-dash:

```cpp
    m_txInhibitLabel->setText(QStringLiteral("INH"));
    m_txInhibitLabel->setToolTip(tr("External TX Inhibit asserted. TX is blocked."));
```

- [ ] **Step 4: Dim instead of hide**

Every `setVisible(false)` on a safety badge becomes a dim. Add near the group:

```cpp
    // Inactive slots dim rather than collapse, so geometry never moves.
    auto dimBadge = [](QWidget* w, bool active) {
        auto* fx = qobject_cast<QGraphicsOpacityEffect*>(w->graphicsEffect());
        if (!fx) {
            fx = new QGraphicsOpacityEffect(w);
            w->setGraphicsEffect(fx);
        }
        fx->setOpacity(active ? 1.0 : 0.14);
    };
```

Replace, at these sites:
- `m_txInhibitLabel->setVisible(false)` (`:6584`) with `dimBadge(m_txInhibitLabel, false)`
- `m_adcOvlBadge->setVisible(false)` (`:6623`) with `dimBadge(m_adcOvlBadge, false)`
- inside the ADC hide timer (`:6638`) with `dimBadge(m_adcOvlBadge, false)`
- inside the overload handler (`:6680`) with `dimBadge(m_adcOvlBadge, true)`

Delete `m_adcOvlSep` and its `setVisible` calls entirely; the group's
border-left replaces every separator here. Add `#include <QGraphicsOpacityEffect>`.

The four `reapplyRightStripDropPriority(/*force=*/true)` calls inside these
handlers (`:6642`, `:6686`) are removed in Task A8; leave them for now so the
tree still builds.

- [ ] **Step 5: Declare the member**

In `src/gui/MainWindow.h`, next to `m_txStatusBadge`:

```cpp
    // Reserved safety slot group. Registered at rung 0 (never folds).
    QWidget* m_safetyGroup{nullptr};
```

- [ ] **Step 6: Run the test to verify it passes**

```bash
cmake --build build --target tst_mainwindow_status_bar_safety && ctest --test-dir build -R tst_mainwindow_status_bar_safety --output-on-failure
```

Expected: PASS, including the pre-existing slots.

- [ ] **Step 7: Commit**

```bash
git add src/gui/MainWindow.cpp src/gui/MainWindow.h \
        tests/tst_mainwindow_status_bar_safety.cpp
git commit -S -m "fix(chrome): reserve the safety slots so TX stops moving

AdcOverloadBadge was inserted between the PA and TX badges and made
visible on overload, which widened the run and slid the TX indicator
sideways at the exact moment something went wrong. TX INHIBIT did the
same.

Four permanently allocated 50 px slots now. Only the badge inside
changes appearance, and an inactive slot dims to 14% rather than
collapsing, so an alarm lights up in a pixel the operator has already
learned and nothing else on the bar moves. The group carries the 1 px
section rule the 2026-04-30 chrome spec specified and the
implementation never used."
```

---

## Task A7: UTC clock moves to the TitleBar

**Files:**
- Modify: `src/gui/TitleBar.h`, `src/gui/TitleBar.cpp`
- Modify: `src/gui/MainWindow.cpp` (delete the banner clock block at `:6727-6766`)
- Test: `tests/tst_title_bar_clock.cpp` (create)
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: existing `TitleBar` constructor and its `m_hbox` layout.
- Produces: `TitleBar::utcText() const -> QString`. Task A8 needs nothing
  from this task; it is independent.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_title_bar_clock.cpp`:

```cpp
// no-port-check: NereusSDR-original. No upstream port.
#include <QtTest/QtTest>
#include <QRegularExpression>
#include "gui/TitleBar.h"

using namespace NereusSDR;

class TstTitleBarClock : public QObject {
    Q_OBJECT
private slots:
    void utcTextIsPopulatedBeforeTheFirstTick() {
        TitleBar t;
        QVERIFY(!t.utcText().isEmpty());
    }

    void utcTextMatchesTheSingleRowFormat() {
        TitleBar t;
        const QRegularExpression re(
            QStringLiteral("^[0-2][0-9]:[0-5][0-9]:[0-5][0-9] UTC$"));
        QVERIFY2(re.match(t.utcText()).hasMatch(), qPrintable(t.utcText()));
    }

    void utcTextHasNoDateRow() {
        TitleBar t;
        QVERIFY(!t.utcText().contains(QLatin1Char('-')));
    }
};
QTEST_MAIN(TstTitleBarClock)
#include "tst_title_bar_clock.moc"
```

- [ ] **Step 2: Register and run to verify it fails**

Add `nereus_add_test(tst_title_bar_clock)` to `tests/CMakeLists.txt`.

```bash
cmake --build build --target tst_title_bar_clock
```

Expected: FAIL to compile, `utcText` not a member.

- [ ] **Step 3: Add the clock to TitleBar**

In `src/gui/TitleBar.h` public section:

```cpp
    /// Single-row UTC, "23:59:59 UTC". Time lives where every OS puts it,
    /// which also frees the banner's bottom-right corner for alarms alone.
    QString utcText() const;
```

Private:

```cpp
    QLabel* m_utcLabel{nullptr};
    QTimer* m_utcTimer{nullptr};
```

In `src/gui/TitleBar.cpp`, after the `m_master` widget is added around line
423 and before the trailing `addSpacing(6)`:

```cpp
    m_utcLabel = new QLabel(this);
    m_utcLabel->setToolTip(tr("UTC time"));
    m_utcLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #8aa8c0; font-size: 11px;"
        " font-family: 'SF Mono', Menlo, monospace; }"));
    m_hbox->addWidget(m_utcLabel);
    m_hbox->addSpacing(10);

    auto tickUtc = [this]() {
        m_utcLabel->setText(QDateTime::currentDateTimeUtc()
                                .toString(QStringLiteral("hh:mm:ss UTC")));
    };
    tickUtc();  // populate before the first timer fire
    m_utcTimer = new QTimer(this);
    connect(m_utcTimer, &QTimer::timeout, this, tickUtc);
    m_utcTimer->start(1000);
```

And:

```cpp
QString TitleBar::utcText() const
{
    return m_utcLabel ? m_utcLabel->text() : QString();
}
```

Add `#include <QDateTime>` and `#include <QTimer>` if absent.

- [ ] **Step 4: Delete the banner clock**

In `src/gui/MainWindow.cpp`, delete the whole braced block at lines 6727-6766
that builds `m_timeWidget`, `m_utcTimeLabel`, `localDateLabel` and
`m_clockTimer`. Remove `m_timeWidget`, `m_utcTimeLabel` and `m_clockTimer`
from `MainWindow.h`. Remove the `Clock` entry from the `priorityGroups` table
at `:7801`; Task A8 deletes that whole function, but leaving a dangling
`m_timeWidget` reference now would not compile.

- [ ] **Step 5: Run the test to verify it passes**

```bash
cmake --build build --target tst_title_bar_clock && ctest --test-dir build -R tst_title_bar_clock --output-on-failure
```

Expected: PASS, 3 of 3.

- [ ] **Step 6: Commit**

```bash
git add src/gui/TitleBar.h src/gui/TitleBar.cpp src/gui/MainWindow.h \
        src/gui/MainWindow.cpp tests/tst_title_bar_clock.cpp tests/CMakeLists.txt
git commit -S -m "feat(chrome): move UTC to the title bar, single row

Placement before pixels. Every desktop OS puts time top-right, so
bottom-right costs a cross-window eye movement, and more importantly it
squats in the one corner that should belong to alarms alone.

The date and local-time row is dropped: the date is already known and
local time sits in the menu bar six inches away. UTC is the one fact
the OS clock does not give an operator for logging, so it is the one
that stays. 130 px leaves the banner."
```

---

## Task A8: Wire the controller in, delete both old ladders

**Files:**
- Create: `src/gui/chrome/ChromeBarItems.h`, `src/gui/chrome/ChromeBarItems.cpp`
- Modify: `src/gui/MainWindow.cpp` (`buildStatusBar()`, `resizeEvent`), `src/gui/MainWindow.h`
- Test: `tests/tst_chrome_bar_items.cpp` (create)
- Modify: `tests/CMakeLists.txt`, `CMakeLists.txt`

**Why the item list is extracted.** `MainWindow` cannot be constructed in a
test in this harness. Doing so starts real `RadioDiscovery` UDP broadcasts on
the LAN, spends roughly 9 s building the auto-opened `ConnectionPanel`, and
SIGABRTs the binary on teardown with `SpectrumThread` still running. This is
documented independently in `tests/tst_mainwindow_status_bar_safety.cpp:29-38`,
`tests/tst_pan_active_slice_sync.cpp` and `tests/tst_pan_badge_click_wiring.cpp`,
and was reconfirmed empirically during Task A6 by building and running it.

So the fold ladder's composition moves into a free function that takes the
widgets as a plain struct. `buildStatusBar()` fills the struct and calls it;
the test fills the struct with sized stand-in widgets and calls the same
function. The regression sweep therefore exercises the real rung assignments
rather than a hand-copied table that could silently drift.

**Interfaces:**
- Consumes: everything produced by Tasks A1 through A7.
- Produces: nothing downstream. This closes Phase A.

**What is deleted:** `reapplyRightStripDropPriority()` and its declaration,
`m_rightStripLastBudget`, `m_rightStripSettled`, `m_chain1IndicatorWidget`
drop handling, and all four `reapplyRightStripDropPriority(/*force=*/true)`
call sites at `:6473`, `:6642`, `:6686`, `:7127`.

- [ ] **Step 1: Write the failing test**

`MainWindow` cannot be constructed in this harness. Doing so starts real
`RadioDiscovery` UDP broadcasts on the LAN, spends roughly 9 s building the
auto-opened `ConnectionPanel`, and SIGABRTs the binary on teardown with
`SpectrumThread` still running. This is documented independently in
`tests/tst_mainwindow_status_bar_safety.cpp:29-38`,
`tests/tst_pan_active_slice_sync.cpp` and `tests/tst_pan_badge_click_wiring.cpp`,
and was reconfirmed empirically during Task A6.

So the ladder's composition is extracted into a free function taking a plain
struct of widgets. `buildStatusBar()` fills the struct and calls it; the test
fills it with sized stand-ins and calls the same function. The sweep then
exercises the real rung assignments rather than a hand-copied table that could
drift. Making `MainWindow` testable is the better long-term fix and is tracked
separately; it needs test-only seams in the constructor plus a fix for the
`SpectrumThread` teardown ordering, both out of scope here.

Create `tests/tst_chrome_bar_items.cpp`:

```cpp
// no-port-check: NereusSDR-original. No upstream port.
#include <QtTest/QtTest>
#include <QLabel>
#include <QWidget>

#include "gui/chrome/ChromeBarController.h"
#include "gui/chrome/ChromeBarItems.h"

using namespace NereusSDR;

class TstChromeBarItems : public QObject {
    Q_OBJECT

private:
    QWidget* host{nullptr};

    QLabel* w(int px) {
        auto* l = new QLabel(host);
        l->setFixedWidth(px);
        return l;
    }

    // Natural widths from the design doc's section 4.6 ledger.
    ChromeBarWidgets makeWidgets() {
        ChromeBarWidgets g;
        g.panButton        = w(48);
        g.panelToggle      = w(18);
        g.placeholderGroup = w(132);
        g.placeholderSep   = w(14);
        g.chain0           = w(52);
        g.rxDashRow        = w(96);
        g.psaIndicator     = w(66);
        g.stationBlock     = w(168);
        g.catIndicator     = w(60);
        g.catSep           = w(14);
        g.tciIndicator     = w(60);
        g.tciSep           = w(14);
        g.tgxlChip         = w(62);
        g.systemTile       = w(60);
        g.systemTileSep    = w(14);
        g.safetyGroup      = w(200);
        for (int r = 5; r <= 9; ++r) { g.pillByRung[r] = w(22); }
        return g;
    }

private slots:
    void init()    { host = new QWidget; }
    void cleanup() { delete host; host = nullptr; }

    void everythingShowsWhenWide() {
        ChromeBarController c;
        ChromeBarWidgets g = makeWidgets();
        registerChromeBarItems(c, g);
        c.relayout(2400);
        QCOMPARE(c.foldedThroughRung(), 0);
        QVERIFY(c.foldedLabels().isEmpty());
    }

    void foldRungNeverGoesBackwardsAsWidthShrinks() {
        ChromeBarController c;
        ChromeBarWidgets g = makeWidgets();
        registerChromeBarItems(c, g);
        int prev = 0;
        for (int width = 2400; width >= 300; --width) {
            c.relayout(width);
            const int rung = c.foldedThroughRung();
            QVERIFY2(rung >= prev,
                     qPrintable(QStringLiteral("rung %1 < %2 at width %3")
                                    .arg(rung).arg(prev).arg(width)));
            prev = rung;
        }
        QVERIFY2(prev > 0, "nothing ever folded; fixture widths are wrong");
    }

    void oneWidthAlwaysYieldsOneLayout() {
        ChromeBarController c;
        ChromeBarWidgets g = makeWidgets();
        registerChromeBarItems(c, g);
        for (int width = 300; width <= 2400; width += 7) {
            c.relayout(width);
            const int cold = c.foldedThroughRung();
            c.relayout(width + 1);
            c.relayout(width - 1);
            c.relayout(width);
            QCOMPARE(c.foldedThroughRung(), cold);
        }
    }

    void neverFoldingItemsSurviveEveryWidth() {
        ChromeBarController c;
        ChromeBarWidgets g = makeWidgets();
        registerChromeBarItems(c, g);
        const QList<QWidget*> mustSurvive = {
            g.panButton, g.panelToggle, g.stationBlock, g.safetyGroup
        };
        for (int width = 2400; width >= 300; width -= 3) {
            c.relayout(width);
            for (QWidget* keep : mustSurvive) {
                QVERIFY2(!keep->isHidden(),
                         qPrintable(QStringLiteral("never-fold item folded at %1")
                                        .arg(width)));
            }
        }
    }

    void ladderFoldsInTheDesignedOrder() {
        ChromeBarController c;
        ChromeBarWidgets g = makeWidgets();
        registerChromeBarItems(c, g);
        // Design section 6: system tile, then TGXL, then the CAT/TCI pair,
        // then the chain tags, then the RX pills, then the placeholders.
        const QList<QWidget*> order = {
            g.systemTile, g.tgxlChip, g.catIndicator, g.chain0,
            g.pillByRung[5], g.placeholderGroup
        };
        QList<int> foldWidth;
        for (QWidget* item : order) {
            int found = -1;
            for (int width = 2400; width >= 200; --width) {
                c.relayout(width);
                if (item->isHidden()) { found = width; break; }
            }
            QVERIFY2(found > 0, "an item on the ladder never folded");
            foldWidth << found;
        }
        for (int i = 1; i < foldWidth.size(); ++i) {
            QVERIFY2(foldWidth[i] <= foldWidth[i - 1],
                     qPrintable(QStringLiteral("ladder out of order at index %1")
                                    .arg(i)));
        }
    }

    void catAndTciFoldTogether() {
        ChromeBarController c;
        ChromeBarWidgets g = makeWidgets();
        registerChromeBarItems(c, g);
        for (int width = 2400; width >= 300; --width) {
            c.relayout(width);
            QCOMPARE(g.catIndicator->isHidden(), g.tciIndicator->isHidden());
        }
    }

    void nullWidgetsAreSkippedNotCrashed() {
        ChromeBarController c;
        ChromeBarWidgets g = makeWidgets();
        g.chain1 = nullptr;   // single-ADC SKU
        g.tgxlChip = nullptr; // no tuner present
        registerChromeBarItems(c, g);
        c.relayout(1512);
        QVERIFY(!g.panButton->isHidden());
    }
};
QTEST_MAIN(TstChromeBarItems)
#include "tst_chrome_bar_items.moc"
```

- [ ] **Step 2: Register and run to verify it fails**

Add `nereus_add_test(tst_chrome_bar_items)` to `tests/CMakeLists.txt`.

```bash
cmake --build build --target tst_chrome_bar_items
```

Expected: FAIL to compile, `gui/chrome/ChromeBarItems.h` not found.

- [ ] **Step 2b: Write the extracted registration unit**

Create `src/gui/chrome/ChromeBarItems.h`:

```cpp
// no-port-check: NereusSDR-original. No upstream port. The banner's fold
// ladder composition, extracted from buildStatusBar so it can be tested
// without constructing MainWindow.

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>

class QWidget;

namespace NereusSDR {

class ChromeBarController;

/// Every widget the banner registers. Any member may be null; registration
/// skips nulls, which is how single-ADC SKUs omit chain 1.
struct ChromeBarWidgets {
    QWidget* panButton{nullptr};
    QWidget* panelToggle{nullptr};
    QWidget* stationBlock{nullptr};
    QWidget* safetyGroup{nullptr};
    QWidget* psaIndicator{nullptr};

    QWidget* systemTile{nullptr};
    QWidget* systemTileSep{nullptr};
    QWidget* tgxlChip{nullptr};
    QWidget* catIndicator{nullptr};
    QWidget* catSep{nullptr};
    QWidget* tciIndicator{nullptr};
    QWidget* tciSep{nullptr};
    QWidget* chain0{nullptr};
    QWidget* chain1{nullptr};
    QWidget* rxDashRow{nullptr};
    QWidget* placeholderGroup{nullptr};
    QWidget* placeholderSep{nullptr};

    /// Rung to pill widget, rungs 5..9 only (SQL, APF, NB, NR, AGC).
    /// Mode and filter never fold and are deliberately absent.
    QHash<int, QWidget*> pillByRung;
};

/// Single source of truth for which banner item folds at which rung.
/// Rung 0 never folds. See design doc section 6.
void registerChromeBarItems(ChromeBarController& controller,
                            const ChromeBarWidgets& widgets);

} // namespace NereusSDR
```

Create `src/gui/chrome/ChromeBarItems.cpp`:

```cpp
// no-port-check: NereusSDR-original. No upstream port.

// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/chrome/ChromeBarItems.h"

#include "gui/chrome/ChromeBarController.h"

#include <QCoreApplication>
#include <QWidget>

namespace NereusSDR {

namespace {
/// Skip-on-null so callers need no per-item guard.
void add(ChromeBarController& c, QWidget* widget, QWidget* sep, int rung,
         const QString& label)
{
    if (!widget) { return; }
    c.addItem(widget, sep, rung, label);
}
} // namespace

void registerChromeBarItems(ChromeBarController& c, const ChromeBarWidgets& w)
{
    // Rung 0: nothing else reaches these. +PAN and the panel toggle have no
    // menu equivalent at all; the safety slots have no menu, dialog or
    // applet. Reachability audit: design doc section 3.4.
    add(c, w.panButton,    nullptr, 0, QString());
    add(c, w.panelToggle,  nullptr, 0, QString());
    add(c, w.stationBlock, nullptr, 0, QString());
    add(c, w.safetyGroup,  nullptr, 0, QString());
    add(c, w.psaIndicator, nullptr, 0, QString());

    add(c, w.systemTile, w.systemTileSep, 1,
        QCoreApplication::translate("ChromeBar", "PA / CPU"));
    add(c, w.tgxlChip, nullptr, 2,
        QCoreApplication::translate("ChromeBar", "TGXL"));

    // CAT and TCI share rung 3 so they fold as a pair, avoiding a
    // "TCI but no CAT" half-state.
    add(c, w.catIndicator, w.catSep, 3,
        QCoreApplication::translate("ChromeBar", "CAT"));
    add(c, w.tciIndicator, w.tciSep, 3,
        QCoreApplication::translate("ChromeBar", "TCI"));

    // Both chain tags share rung 4. Chain 1 is null on single-ADC SKUs.
    add(c, w.chain0, nullptr, 4,
        QCoreApplication::translate("ChromeBar", "CH"));
    add(c, w.chain1, nullptr, 4, QString());

    // Rungs 5..9, one pill each, right to left.
    static const char* const kPillNames[] = {"SQL", "APF", "NB", "NR", "AGC"};
    for (int rung = 5; rung <= 9; ++rung) {
        add(c, w.pillByRung.value(rung), nullptr, rung,
            QCoreApplication::translate("ChromeBar", kPillNames[rung - 5]));
    }

    // Rung 10, last resort: placeholders fold only after every live
    // reading has already gone.
    add(c, w.placeholderGroup, w.placeholderSep, 10,
        QCoreApplication::translate("ChromeBar", "TNF / CWX / DVK / FDX"));
}

} // namespace NereusSDR
```

Add both new sources to the root `CMakeLists.txt` beside the existing
`src/gui/chrome/` entries.

- [ ] **Step 3: Register every item**

At the end of `buildStatusBar()`, before `sb->addWidget(barWidget, 1)`:

```cpp
    // ── Layout authority (design §5) ─────────────────────────────────────
    // One controller replaces RxDashboard's internal ladder, this file's
    // reapplyRightStripDropPriority, and Qt's squeeze in the left section.
    // The rung assignments live in registerChromeBarItems so they can be
    // tested without constructing MainWindow; do not inline them here.
    m_chromeBar = new ChromeBarController(this);

    ChromeBarWidgets bar;
    bar.panButton        = panBtn;
    bar.panelToggle      = panelToggleLabel;
    bar.stationBlock     = m_stationBlock;
    bar.safetyGroup      = m_safetyGroup;
    bar.psaIndicator     = m_psaIndicator;
    bar.systemTile       = m_systemTile;
    bar.systemTileSep    = m_systemTileSep;
    bar.tgxlChip         = m_tgxlChip;
    bar.catIndicator     = m_catIndicator;
    bar.catSep           = m_catSep;
    bar.tciIndicator     = m_tciIndicator;
    bar.tciSep           = m_tciSep;
    bar.chain0           = m_chain0IndicatorWidget;
    bar.chain1           = m_chain1IndicatorWidget;  // null on 1-ADC SKUs
    bar.rxDashRow        = m_rxDashboard;
    bar.placeholderGroup = m_placeholderGroup;
    bar.placeholderSep   = m_placeholderSep;
    for (int rung = 5; rung <= 9; ++rung) {
        bar.pillByRung[rung] = m_rxDashboard->badgeForRung(rung);
    }

    registerChromeBarItems(*m_chromeBar, bar);

    connect(m_chromeBar, &ChromeBarController::foldStateChanged,
            m_overflowChip, &OverflowChip::setDroppedItems);
```

Two supporting edits earlier in `buildStatusBar()`.

Capture CH 0's widget, which is currently added inline and never stored:

```cpp
    auto* chain0Widget = makeChainIndicator(0);
    hbox->addWidget(chain0Widget);
    m_chain0IndicatorWidget = chain0Widget;
```

Group the band-stack dots and the four placeholder labels into one widget so
rung 10 folds them together, replacing the five separate `hbox->addWidget`
calls at `:6110` and `:6186-6215`:

```cpp
    // Rung 10, last resort. Grouped so the ladder folds them as one unit
    // rather than dribbling them out one label at a time.
    m_placeholderGroup = new QWidget(barWidget);
    auto* phRow = new QHBoxLayout(m_placeholderGroup);
    phRow->setContentsMargins(0, 0, 0, 0);
    phRow->setSpacing(6);
    phRow->addWidget(bandStackLabel);
    phRow->addWidget(m_tnfLabel);
    phRow->addWidget(cwxLabel);
    phRow->addWidget(dvkLabel);
    phRow->addWidget(fdxLabel);
    hbox->addWidget(m_placeholderGroup);
```

Reparent each label to `m_placeholderGroup` at construction so the group owns
them. Task B4 gives `panBtn` its `addPanButton` objectName; if Phase B has not
landed yet, add `panBtn->setObjectName(QStringLiteral("addPanButton"))` here so
the integration test can find it.

- [ ] **Step 4: Drive it from resizeEvent**

Replace the `reapplyRightStripDropPriority()` call at `:8075` in
`MainWindow::resizeEvent` with:

```cpp
    if (m_chromeBar && m_chromeBarWidget) {
        m_chromeBar->relayout(m_chromeBarWidget->width());
    }
```

Content-change sites call `setNaturalWidth` then `relayout` instead of
`reapplyRightStripDropPriority(/*force=*/true)`. For the SystemTile:

```cpp
    m_chromeBar->setNaturalWidth(m_systemTile, m_systemTile->sizeHint().width());
    m_chromeBar->relayout(m_chromeBarWidget->width());
```

- [ ] **Step 4b: Retire two comments that now assert the opposite of the code**

Found during Task A5's review, deferred to here because this task rewrites
the same block.

`MainWindow.cpp` around lines 8846-8849 says the dashboard "is always bound
to slice(0) ... No per-connect rebind needed". Task A5 made that false.
Delete or correct it.

`MainWindow.cpp` around lines 6249-6250 says "Bound to slice(0)", and the
call a few lines below it still reads `m_rxDashboard->bindSlice(slices.at(0))`.
That call is dead in practice: `buildStatusBar()` runs during construction,
and `RadioModel::addSlice` is only ever invoked from the connect flow, so
`slices()` is provably empty there. Delete both the comment and the call;
Task A5's `rebindDashboard` lambda is the only binding path now.

Do NOT touch the comment near line 9152. An earlier note of mine wrongly
flagged it; its claim is still accurate.

- [ ] **Step 5: Delete the old ladder**

Delete from `src/gui/MainWindow.cpp`: the whole body of
`reapplyRightStripDropPriority` (`:7749-7866`). Delete from
`src/gui/MainWindow.h`: its declaration, `m_rightStripLastBudget`,
`m_rightStripSettled`. Add:

```cpp
    // Single layout authority for the banner. Replaces the two ladders and
    // their hysteresis deadbands; see design §5.2.
    ChromeBarController* m_chromeBar{nullptr};
    SystemTile*          m_systemTile{nullptr};
    QLabel*              m_systemTileSep{nullptr};
    QWidget*             m_placeholderGroup{nullptr};
    QWidget*             m_chain0IndicatorWidget{nullptr};
```

Delete `m_paStackWidget`, `m_paVoltLabel`, `m_paTempLabel`,
`m_paVoltLabelSep`, `m_cpuMetric`, `m_cpuMetricSep` and `m_adcOvlSep`, and
repoint their signal handlers at `m_systemTile`:

```cpp
    connect(conn, &RadioConnection::userAdc0Changed, this, [this](float v) {
        const auto model = m_radioModel->hardwareProfile().model;
        if (model == HPSDRModel::ANAN_G2E) { return; }
        const bool is8000D = (model == HPSDRModel::ANAN8000D);
        const bool showVolts = !is8000D ||
            AppSettings::instance().value(
                QStringLiteral("HardwareAnan8000DleShowVoltsAmps"),
                QStringLiteral("True")).toString() == QStringLiteral("True");
        if (!showVolts) { return; }
        m_systemTile->setPaLabel(QStringLiteral("PA"));
        m_systemTile->setPaVolts(static_cast<double>(v));
    });
    connect(conn, &RadioConnection::supplyVoltsChanged, this, [this](float v) {
        if (m_radioModel->hardwareProfile().model != HPSDRModel::ANAN_G2E) { return; }
        m_systemTile->setPaLabel(QStringLiteral("PSU"));
        m_systemTile->setPaVolts(static_cast<double>(v));
    });
    connect(&m_radioModel->radioStatus(), &RadioStatus::paTemperatureChanged,
            this, [this](double celsius) {
        m_systemTile->setPaTempCelsius(celsius);
    });
    connect(&PaTempUnitNotifier::instance(), &PaTempUnitNotifier::unitChanged,
            this, [this](PaTempUnit) { m_systemTile->refreshPaRow(); });
    // There is no toggle(); flip explicitly, matching the existing
    // isPaTempToggle handler in eventFilter (MainWindow.cpp:8099-8103).
    connect(m_systemTile, &SystemTile::paTempClicked, this, []() {
        const PaTempUnit cur = PaTempUnitNotifier::currentUnit();
        PaTempUnitNotifier::setUnit(cur == PaTempUnit::Celsius
                                        ? PaTempUnit::Fahrenheit
                                        : PaTempUnit::Celsius);
    });
```

On disconnect, clear both PA readings:

```cpp
        if (s != ConnectionState::Connected) {
            m_systemTile->clearPaVolts();
            m_systemTile->clearPaTemp();
        }
```

The CPU timer's body becomes `m_systemTile->setCpuPercent(m_cpuSmoothedPct);`
and the right-click menu policy moves from `m_cpuMetric` to `m_systemTile`.

Also delete `RxDashboard::resizeEvent` if Task A5 left it, and confirm nothing
still references `m_timeWidget` after Task A7.

- [ ] **Step 5b: Verify A6's deferred finding is actually resolved**

Task A6's review found that `reapplyRightStripDropPriority()` still listed
`m_paStatusBadge` in its `priorityGroups` table and called `setVisible()` on
it. Once A6 nested that badge inside a fixed-width slot, two problems
followed: the badge could still be hidden outright by an ordinary resize,
outside the new dim mechanism entirely; and hiding it saved no width at all,
because `requiredWidth()` walks only direct children of the outer `hbox` and
`m_safetyGroup`'s sizeHint is pinned at 4 x 50 px regardless of what is
visible inside it. The ladder therefore over-dropped CAT, TCI, PA telemetry
and the clock chasing a saving that never materialised.

A6 was forbidden from touching that function because this task deletes it.
Confirm the deletion actually resolved it. All three must hold:

```bash
# 1. The function and its table are gone.
grep -n "reapplyRightStripDropPriority\|priorityGroups" src/gui/MainWindow.cpp src/gui/MainWindow.h
# Expected: no output.

# 2. No safety badge is setVisible()'d anywhere outside its own dim helper.
grep -n "m_paStatusBadge->setVisible\|m_txStatusBadge->setVisible\|m_adcOvlBadge->setVisible\|m_txInhibitLabel->setVisible" src/gui/MainWindow.cpp
# Expected: no output. Dimming is the only mechanism.

# 3. The controller measures m_safetyGroup once, at rung 0.
grep -n "safetyGroup" src/gui/MainWindow.cpp
```

If any of the first two produce output, stop and report it rather than
working around it.

Also retire the stale mirror in `tests/tst_mainwindow_status_bar_safety.cpp`
around lines 80-100: `txInhibitLabel_hiddenByDefault` still asserts the full
`"TX INHIBIT"` text, `setVisible(false)`, and an em-dash tooltip, none of
which `buildStatusBar()` does any more. It passes because it is a
self-contained mirror, so it documents a superseded state with nothing
flagging it. Update it to `INH`, dimming, and the current tooltip.

- [ ] **Step 6: Run the extracted-registration test**

```bash
cmake --build build --target tst_chrome_bar_items && ctest --test-dir build -R tst_chrome_bar_items --output-on-failure
```

Expected: PASS, 4 of 4.

- [ ] **Step 7: Run the chrome subsystem before committing**

```bash
ctest --test-dir build -R "chrome|status_bar|rx_dashboard|station_block|system_tile|title_bar" --output-on-failure
```

Expected: all PASS. If a pre-existing test fails, diagnose and fix it here;
do not carry it forward.

- [ ] **Step 8: Commit**

```bash
git add src/gui/MainWindow.cpp src/gui/MainWindow.h \
        src/gui/chrome/ChromeBarItems.h src/gui/chrome/ChromeBarItems.cpp \
        tests/tst_chrome_bar_items.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -S -m "refactor(chrome): one layout authority, both old ladders deleted

buildStatusBar now registers every banner item with ChromeBarController
and resizeEvent calls relayout once. reapplyRightStripDropPriority and
its 30 px deadband are gone, RxDashboard's internal ladder and its
settled flag are gone, and nothing in the bar shrinks below its natural
width any more, which removes the overlap in the left section outright.

The banner needs about 1286 px where it needed 1740, so on a 1512
logical-point display it fits with roughly 226 px of slack and folds
nothing in normal use.

The integration test sweeps 1512 down to 800 and asserts the fold rung
never goes backwards, that TX never folds, and that +PAN and the panel
toggle never fold. That sweep is the regression gate the old code could
not pass."
```

---

# Phase B: The pan menu

Phase B is independently shippable and touches no Phase A file except
`MainWindow.cpp`. It can land before or after Phase A.

## Task B1: Four new layouts in PanadapterStack

**Files:**
- Modify: `src/gui/PanadapterStack.cpp`
- Modify: `src/gui/MainWindow.cpp:8609-8621` (`panIdsForLayout`)
- Test: `tests/tst_panadapter_stack_layouts.cpp` (exists; add slots)

**Interfaces:**
- Consumes: `PanadapterStack::applyLayout(const QString& layoutId, const QStringList& panIds)`,
  `::addPanadapter(const QString& id) -> PanadapterApplet*`, `::count()`,
  `::currentLayoutId()`, member `m_rootSplitter`.
- Produces: layout ids `"2h1"`, `"3v"`, `"4v"`, `"3h2"` accepted by
  `applyLayout`, and `MainWindow::panIdsForLayout` returning the right pan
  count for each. Task B3 relies on all four ids existing.

**Geometry (design §8.3):**

| Id | Rows | Pans |
| --- | --- | ---: |
| `2h1` | `A\|B` over `C` | 3 |
| `3v` | `A` over `B` over `C` | 3 |
| `4v` | `A` over `B` over `C` over `D` | 4 |
| `3h2` | `A\|B\|C` over `D\|E` | 5 |

- [ ] **Step 1: Write the failing test**

Append to `tests/tst_panadapter_stack_layouts.cpp`:

```cpp
    void layout2h1BuildsThreePans() {
        PanadapterStack s;
        s.applyLayout(QStringLiteral("2h1"),
                      {QStringLiteral("p0"), QStringLiteral("p1"),
                       QStringLiteral("p2")});
        QCOMPARE(s.count(), 3);
        QCOMPARE(s.currentLayoutId(), QStringLiteral("2h1"));
    }

    void layout3vBuildsThreePans() {
        PanadapterStack s;
        s.applyLayout(QStringLiteral("3v"),
                      {QStringLiteral("p0"), QStringLiteral("p1"),
                       QStringLiteral("p2")});
        QCOMPARE(s.count(), 3);
        QCOMPARE(s.currentLayoutId(), QStringLiteral("3v"));
    }

    void layout4vBuildsFourPans() {
        PanadapterStack s;
        s.applyLayout(QStringLiteral("4v"),
                      {QStringLiteral("p0"), QStringLiteral("p1"),
                       QStringLiteral("p2"), QStringLiteral("p3")});
        QCOMPARE(s.count(), 4);
    }

    void layout3h2BuildsFivePans() {
        PanadapterStack s;
        s.applyLayout(QStringLiteral("3h2"),
                      {QStringLiteral("p0"), QStringLiteral("p1"),
                       QStringLiteral("p2"), QStringLiteral("p3"),
                       QStringLiteral("p4")});
        QCOMPARE(s.count(), 5);
    }

    void newLayoutsIgnoreShortIdLists() {
        PanadapterStack s;
        s.applyLayout(QStringLiteral("3h2"),
                      {QStringLiteral("p0"), QStringLiteral("p1")});
        // Guard clause declines rather than building a partial layout.
        QVERIFY(s.count() != 5);
    }

    void panIdsForLayoutCountsMatchGeometry() {
        QCOMPARE(MainWindow::panIdsForLayout(QStringLiteral("2h1")).size(), 3);
        QCOMPARE(MainWindow::panIdsForLayout(QStringLiteral("3v")).size(), 3);
        QCOMPARE(MainWindow::panIdsForLayout(QStringLiteral("4v")).size(), 4);
        QCOMPARE(MainWindow::panIdsForLayout(QStringLiteral("3h2")).size(), 5);
        // Regression guard on the existing five.
        QCOMPARE(MainWindow::panIdsForLayout(QStringLiteral("1")).size(), 1);
        QCOMPARE(MainWindow::panIdsForLayout(QStringLiteral("2v")).size(), 2);
        QCOMPARE(MainWindow::panIdsForLayout(QStringLiteral("2h")).size(), 2);
        QCOMPARE(MainWindow::panIdsForLayout(QStringLiteral("12h")).size(), 3);
        QCOMPARE(MainWindow::panIdsForLayout(QStringLiteral("2x2")).size(), 4);
    }
```

Add `#include "gui/MainWindow.h"` to the test's includes.

- [ ] **Step 2: Run to verify it fails**

```bash
cmake --build build --target tst_panadapter_stack_layouts && ctest --test-dir build -R tst_panadapter_stack_layouts --output-on-failure
```

Expected: FAIL, counts wrong for the four new ids.

- [ ] **Step 3: Rewrite panIdsForLayout as a table**

Replace `src/gui/MainWindow.cpp:8609-8621` with:

```cpp
QStringList MainWindow::panIdsForLayout(const QString& layoutId)
{
    // One table, so a new layout is a one-line addition here and a branch in
    // PanadapterStack::applyLayout, rather than a chain of ternaries that
    // silently defaults new ids to 2. Counts match design §8.3.
    static const QHash<QString, int> kPanCount = {
        {QStringLiteral("1"),   1},
        {QStringLiteral("2v"),  2},
        {QStringLiteral("2h"),  2},
        {QStringLiteral("2h1"), 3},
        {QStringLiteral("12h"), 3},
        {QStringLiteral("3v"),  3},
        {QStringLiteral("2x2"), 4},
        {QStringLiteral("4v"),  4},
        {QStringLiteral("3h2"), 5},
    };
    const int needed = kPanCount.value(layoutId, 1);
    QStringList ids;
    ids.reserve(needed);
    for (int i = 0; i < needed; ++i) {
        ids << QStringLiteral("pan-%1").arg(i);
    }
    return ids;
}
```

The default changes from 2 to 1: an unknown id should degrade to Single, not
silently build two pans.

- [ ] **Step 4: Add the four layout branches**

In `src/gui/PanadapterStack.cpp`, after the `2x2` branch, add:

```cpp
    else if (layoutId == QStringLiteral("2h1") && panIds.size() >= 3) {
        m_rootSplitter->setOrientation(Qt::Vertical);

        auto* topRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        auto* tl = addPanadapter(panIds[0]);
        auto* tr = addPanadapter(panIds[1]);
        topRow->addWidget(tl);
        topRow->addWidget(tr);
        tl->show();
        tr->show();
        m_rootSplitter->addWidget(topRow);

        auto* bottom = addPanadapter(panIds[2]);
        m_rootSplitter->addWidget(bottom);
        bottom->show();
    }
    else if (layoutId == QStringLiteral("3v") && panIds.size() >= 3) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        for (int i = 0; i < 3; ++i) {
            auto* p = addPanadapter(panIds[i]);
            m_rootSplitter->addWidget(p);
            p->show();
        }
    }
    else if (layoutId == QStringLiteral("4v") && panIds.size() >= 4) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        for (int i = 0; i < 4; ++i) {
            auto* p = addPanadapter(panIds[i]);
            m_rootSplitter->addWidget(p);
            p->show();
        }
    }
    else if (layoutId == QStringLiteral("3h2") && panIds.size() >= 5) {
        m_rootSplitter->setOrientation(Qt::Vertical);

        auto* topRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        for (int i = 0; i < 3; ++i) {
            auto* p = addPanadapter(panIds[i]);
            topRow->addWidget(p);
            p->show();
        }
        m_rootSplitter->addWidget(topRow);

        auto* bottomRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        for (int i = 3; i < 5; ++i) {
            auto* p = addPanadapter(panIds[i]);
            bottomRow->addWidget(p);
            p->show();
        }
        m_rootSplitter->addWidget(bottomRow);
    }
```

- [ ] **Step 5: Run the test to verify it passes**

```bash
cmake --build build --target tst_panadapter_stack_layouts && ctest --test-dir build -R tst_panadapter_stack_layouts --output-on-failure
```

Expected: PASS, including the pre-existing slots.

- [ ] **Step 6: Commit**

```bash
git add src/gui/PanadapterStack.cpp src/gui/MainWindow.cpp \
        tests/tst_panadapter_stack_layouts.cpp
git commit -S -m "feat(pan): add the 2h1, 3v, 4v and 3h2 layouts

Nine layouts now, which is every arrangement reachable inside the
five-slice cap BoardCapabilities enforces on every supported board. The
layout picker in the next commit needs all nine to exist before it can
show them.

panIdsForLayout becomes a table. The ternary chain it replaces defaulted
any unrecognised id to two pans, so adding a layout without touching it
would silently have built the wrong pan count. The default is now
Single, which degrades safely.

3h2 is the only layout that exercises the full five-slice cap, which
makes it the highest-value bench case for DDC allocation."
```

---

## Task B2: LayoutThumbnail

**Files:**
- Create: `src/gui/widgets/LayoutThumbnail.h`, `src/gui/widgets/LayoutThumbnail.cpp`
- Test: `tests/tst_layout_thumbnail.cpp`
- Modify: `tests/CMakeLists.txt`, `CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: `PanLayoutGeometry { QString id; QString label; int panCount; QVector<int> rows; }`,
  `kPanLayouts` (a `QVector<PanLayoutGeometry>` of all nine, Single first),
  and `LayoutThumbnail(const PanLayoutGeometry&, bool isCurrent, bool enabled, QWidget*)`
  with `::cellRects() const -> QVector<QRect>`. Task B3 consumes all of them.

**Port note.** Structure comes from AetherSDR `PanLayoutDialog.cpp`'s
`LayoutThumbnail` at `[@c6481cb]`. Fixed size 120 x 90, pad 4, gap 3, current
fill `#00607a` with a 2 px `#00b4d8` border, cell fill `#2a5a8a`, cell text
`#c8d8e8`, disabled fills `#101018` / `#1a1a2a` / `#40404f`.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_layout_thumbnail.cpp`:

```cpp
// no-port-check: AetherSDR-derived. Structure from AetherSDR
// PanLayoutDialog.cpp LayoutThumbnail [@c6481cb]; registered in
// docs/attribution/aethersdr-contributor-index.md.
#include <QtTest/QtTest>
#include "gui/widgets/LayoutThumbnail.h"

using namespace NereusSDR;

namespace {
const PanLayoutGeometry& byId(const QString& id) {
    for (const PanLayoutGeometry& g : kPanLayouts) {
        if (g.id == id) { return g; }
    }
    Q_ASSERT(false);
    return kPanLayouts.first();
}
} // namespace

class TstLayoutThumbnail : public QObject {
    Q_OBJECT
private slots:
    void nineLayoutsShip() {
        QCOMPARE(kPanLayouts.size(), 9);
    }

    void singleComesFirst() {
        QCOMPARE(kPanLayouts.first().id, QStringLiteral("1"));
    }

    void noLayoutExceedsFivePans() {
        for (const PanLayoutGeometry& g : kPanLayouts) {
            QVERIFY2(g.panCount <= 5, qPrintable(g.id));
        }
    }

    void panCountMatchesRowSum() {
        for (const PanLayoutGeometry& g : kPanLayouts) {
            int total = 0;
            for (int cols : g.rows) { total += cols; }
            QCOMPARE(total, g.panCount);
        }
    }

    void cellRectsMatchTheGeometry() {
        LayoutThumbnail t(byId(QStringLiteral("3h2")), false, true);
        QCOMPARE(t.cellRects().size(), 5);
    }

    void cellRectsDoNotOverlap() {
        LayoutThumbnail t(byId(QStringLiteral("2x2")), false, true);
        const QVector<QRect> r = t.cellRects();
        QCOMPARE(r.size(), 4);
        for (int i = 0; i < r.size(); ++i) {
            for (int j = i + 1; j < r.size(); ++j) {
                QVERIFY2(!r[i].intersects(r[j]),
                         qPrintable(QStringLiteral("cell %1 overlaps %2")
                                        .arg(i).arg(j)));
            }
        }
    }

    void cellRectsStayInsideTheWidget() {
        for (const PanLayoutGeometry& g : kPanLayouts) {
            LayoutThumbnail t(g, false, true);
            for (const QRect& r : t.cellRects()) {
                QVERIFY2(t.rect().contains(r), qPrintable(g.id));
            }
        }
    }

    void fixedSizeMatchesAetherSdr() {
        LayoutThumbnail t(byId(QStringLiteral("1")), false, true);
        QCOMPARE(t.size(), QSize(120, 90));
    }
};
QTEST_MAIN(TstLayoutThumbnail)
#include "tst_layout_thumbnail.moc"
```

- [ ] **Step 2: Register and run to verify it fails**

Add `nereus_add_test(tst_layout_thumbnail)` to `tests/CMakeLists.txt`.

```bash
cmake --build build --target tst_layout_thumbnail
```

Expected: FAIL to compile, header not found.

- [ ] **Step 3: Write the header**

Create `src/gui/widgets/LayoutThumbnail.h`:

```cpp
// no-port-check: AetherSDR-derived NereusSDR file. Painted pan-layout
// preview tile, structurally from AetherSDR PanLayoutDialog.cpp
// LayoutThumbnail [@c6481cb]. Registered in
// docs/attribution/aethersdr-contributor-index.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/widgets/LayoutThumbnail.h  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR PanLayoutDialog.cpp [@c6481cb].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// AetherSDR carries no per-file headers, so this is a project-level
// citation per docs/attribution/HOW-TO-PORT.md rule 6.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Bottom-banner + pan-menu epic.
//                                    Nine layouts rather than
//                                    AetherSDR's twelve; see design
//                                    §8.3. AI-assisted transformation
//                                    via Anthropic Claude Code.
// =================================================================
#pragma once

#include <QRect>
#include <QString>
#include <QVector>
#include <QWidget>

namespace NereusSDR {

/// One pan-layout arrangement. rows holds the column count per row, so
/// {2, 1} is two pans over one.
struct PanLayoutGeometry {
    QString      id;
    QString      label;
    int          panCount{0};
    QVector<int> rows;
};

/// Every layout NereusSDR ships, Single first.
///
/// AetherSDR ships twelve, up to eight pans. The three largest are omitted
/// because BoardCapabilities caps maxSlices at five on every supported
/// board, so those tiles could never light up. That is a client-side
/// allocation limit and not a hardware one: the gateware does eight
/// receivers (n1gp-Anvelina_PROIII Orion.v:958 [@8e86a61], NR = 8).
extern const QVector<PanLayoutGeometry> kPanLayouts;

/// Paints one layout's cell geometry with lettered pans, so an operator
/// recognises the arrangement instead of decoding an id string.
class LayoutThumbnail : public QWidget {
    Q_OBJECT

public:
    LayoutThumbnail(const PanLayoutGeometry& layout, bool isCurrent,
                    bool enabled, QWidget* parent = nullptr);

    /// Cell rectangles in paint order, A first. Exposed for testing.
    QVector<QRect> cellRects() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    PanLayoutGeometry m_layout;
    bool              m_current{false};
    bool              m_enabled{true};
};

} // namespace NereusSDR
```

- [ ] **Step 4: Write the implementation**

Create `src/gui/widgets/LayoutThumbnail.cpp` with the same header block, then:

```cpp
#include "gui/widgets/LayoutThumbnail.h"

#include <QFont>
#include <QPainter>

namespace NereusSDR {

// Nine layouts, Single first. AetherSDR orders Single last; NereusSDR leads
// with it because it is where an operator starts and returns.
const QVector<PanLayoutGeometry> kPanLayouts = {
    {QStringLiteral("1"),   QStringLiteral("Single"),        1, {1}},
    {QStringLiteral("2v"),  QStringLiteral("A / B"),         2, {1, 1}},
    {QStringLiteral("2h"),  QStringLiteral("A | B"),         2, {2}},
    {QStringLiteral("12h"), QStringLiteral("A / B|C"),       3, {1, 2}},
    {QStringLiteral("2h1"), QStringLiteral("A|B / C"),       3, {2, 1}},
    {QStringLiteral("3v"),  QStringLiteral("A / B / C"),     3, {1, 1, 1}},
    {QStringLiteral("2x2"), QStringLiteral("A|B / C|D"),     4, {2, 2}},
    {QStringLiteral("4v"),  QStringLiteral("A/B/C/D"),       4, {1, 1, 1, 1}},
    {QStringLiteral("3h2"), QStringLiteral("A|B|C / D|E"),   5, {3, 2}},
};

namespace {
// From AetherSDR PanLayoutDialog.cpp:43,53-56 [@c6481cb].
constexpr int kThumbW = 120;
constexpr int kThumbH = 90;
constexpr int kPad    = 4;
constexpr int kGap    = 3;
constexpr char kLetters[] = "ABCDEFGH";
} // namespace

LayoutThumbnail::LayoutThumbnail(const PanLayoutGeometry& layout,
                                 bool isCurrent, bool enabled,
                                 QWidget* parent)
    : QWidget(parent), m_layout(layout), m_current(isCurrent),
      m_enabled(enabled)
{
    setFixedSize(kThumbW, kThumbH);
    setCursor(enabled ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
}

QVector<QRect> LayoutThumbnail::cellRects() const
{
    QVector<QRect> out;
    const int w = width()  - kPad * 2;
    const int h = height() - kPad * 2;
    const int totalRows = m_layout.rows.size();
    if (totalRows == 0) {
        return out;
    }
    const int rowH = (h - kGap * (totalRows - 1)) / totalRows;

    for (int r = 0; r < totalRows; ++r) {
        const int cols = m_layout.rows[r];
        if (cols <= 0) {
            continue;
        }
        const int colW = (w - kGap * (cols - 1)) / cols;
        const int y    = kPad + r * (rowH + kGap);
        for (int c = 0; c < cols; ++c) {
            out.append(QRect(kPad + c * (colW + kGap), y, colW, rowH));
        }
    }
    return out;
}

void LayoutThumbnail::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QColor bg = m_current ? QColor(0x00, 0x60, 0x7a) : QColor(0x1a, 0x2a, 0x3a);
    if (!m_enabled) { bg = QColor(0x10, 0x10, 0x18); }
    p.fillRect(rect(), bg);

    QColor border = m_current ? QColor(0x00, 0xb4, 0xd8) : QColor(0x30, 0x40, 0x50);
    if (!m_enabled) { border = QColor(0x20, 0x20, 0x30); }
    p.setPen(QPen(border, m_current ? 2 : 1));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);

    const QColor cellColor = m_enabled ? QColor(0x2a, 0x5a, 0x8a)
                                       : QColor(0x1a, 0x1a, 0x2a);
    const QColor textColor = m_enabled ? QColor(0xc8, 0xd8, 0xe8)
                                       : QColor(0x40, 0x40, 0x50);

    const QVector<QRect> cells = cellRects();
    for (int i = 0; i < cells.size(); ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(cellColor);
        p.drawRoundedRect(cells[i], 3, 3);

        if (i < static_cast<int>(sizeof(kLetters) - 1)) {
            p.setPen(textColor);
            p.setFont(QFont(QStringLiteral("sans-serif"), 14, QFont::Bold));
            p.drawText(cells[i], Qt::AlignCenter, QString(QLatin1Char(kLetters[i])));
        }
    }
}

} // namespace NereusSDR
```

- [ ] **Step 5: Add the source to the build**

In the root `CMakeLists.txt`, alongside the other `src/gui/widgets/` entries:

```cmake
    src/gui/widgets/LayoutThumbnail.cpp
```

- [ ] **Step 6: Run the test to verify it passes**

```bash
cmake --build build --target tst_layout_thumbnail && ctest --test-dir build -R tst_layout_thumbnail --output-on-failure
```

Expected: PASS, 8 of 8.

- [ ] **Step 7: Commit**

```bash
git add src/gui/widgets/LayoutThumbnail.h src/gui/widgets/LayoutThumbnail.cpp \
        tests/tst_layout_thumbnail.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -S -m "feat(pan): painted layout thumbnails, ported from AetherSDR

Structure from AetherSDR PanLayoutDialog.cpp LayoutThumbnail at
@c6481cb: 120 by 90, pad 4, gap 3, current tile filled 00607a with a
2 px 00b4d8 border, lettered cells. An operator recognises A / B|C;
nobody recognises the string 12h, which is what our picker shows today.

Nine layouts rather than AetherSDR's twelve. The three largest need six,
seven and eight pans, and BoardCapabilities caps maxSlices at five on
every board we support, so those tiles could never light up. That is a
client-side allocation limit and not a hardware one: Orion.v:958
@8e86a61 sets NR = 8.

Single leads the list. AetherSDR puts it last, which reads wrong for the
layout an operator starts from and returns to."
```

---

## Task B3: Rebuild PanLayoutDialog around the grid

**Files:**
- Modify: `src/gui/PanLayoutDialog.h`, `src/gui/PanLayoutDialog.cpp`
- Test: `tests/tst_pan_layout_dialog_gating.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `kPanLayouts`, `PanLayoutGeometry`, `LayoutThumbnail` from Task B2.
- Produces: `PanLayoutDialog(int maxSlices, const QString& currentLayoutId, const QString& boardName, QWidget* parent)`,
  `::selectedLayout() const -> QString`, `::visibleLayoutIds() const -> QStringList`,
  `::footerText() const -> QString`. Task B4 consumes the constructor and
  `selectedLayout()`.

**Gating rule (design §8.4):** a layout with `panCount > maxSlices` is
**hidden**, not greyed, and a footer line names how many went and why.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_pan_layout_dialog_gating.cpp`:

```cpp
// no-port-check: AetherSDR-derived. See PanLayoutDialog.h.
#include <QtTest/QtTest>
#include "gui/PanLayoutDialog.h"

using namespace NereusSDR;

class TstPanLayoutDialogGating : public QObject {
    Q_OBJECT
private slots:
    void fiveSliceBoardSeesEverything() {
        PanLayoutDialog d(5, QStringLiteral("1"), QStringLiteral("ANAN-G2"));
        QCOMPARE(d.visibleLayoutIds().size(), 9);
        QVERIFY(d.footerText().isEmpty());
    }

    void fourSliceBoardHidesTheFivePanLayout() {
        PanLayoutDialog d(4, QStringLiteral("1"), QStringLiteral("Hermes"));
        QCOMPARE(d.visibleLayoutIds().size(), 8);
        QVERIFY(!d.visibleLayoutIds().contains(QStringLiteral("3h2")));
        QVERIFY(d.footerText().contains(QStringLiteral("Hermes")));
        QVERIFY(d.footerText().contains(QStringLiteral("1 layout")));
    }

    void threeSliceBoardHidesThePanCountsAboveThree() {
        PanLayoutDialog d(3, QStringLiteral("1"), QStringLiteral("Metis"));
        const QStringList ids = d.visibleLayoutIds();
        QCOMPARE(ids.size(), 6);
        QVERIFY(!ids.contains(QStringLiteral("2x2")));
        QVERIFY(!ids.contains(QStringLiteral("4v")));
        QVERIFY(!ids.contains(QStringLiteral("3h2")));
        QVERIFY(d.footerText().contains(QStringLiteral("3 layouts")));
    }

    void twoSliceBoardKeepsOnlyTheOneAndTwoPanLayouts() {
        PanLayoutDialog d(2, QStringLiteral("1"), QStringLiteral("Hermes II"));
        const QStringList ids = d.visibleLayoutIds();
        QCOMPARE(ids, (QStringList{QStringLiteral("1"),
                                   QStringLiteral("2v"),
                                   QStringLiteral("2h")}));
    }

    void singleAlwaysSurvives() {
        PanLayoutDialog d(1, QStringLiteral("1"), QStringLiteral("Tiny"));
        QVERIFY(d.visibleLayoutIds().contains(QStringLiteral("1")));
    }

    void footerNamesTheBoardNotTheApp() {
        PanLayoutDialog d(2, QStringLiteral("1"), QStringLiteral("Hermes II"));
        // "this radio does not have that", never "this app does not have that".
        QVERIFY(d.footerText().contains(QStringLiteral("Hermes II")));
        QVERIFY(d.footerText().contains(QStringLiteral("radio")));
    }

    void selectedLayoutIsEmptyBeforeAnyChoice() {
        PanLayoutDialog d(5, QStringLiteral("1"), QStringLiteral("ANAN-G2"));
        QVERIFY(d.selectedLayout().isEmpty());
    }
};
QTEST_MAIN(TstPanLayoutDialogGating)
#include "tst_pan_layout_dialog_gating.moc"
```

- [ ] **Step 2: Register and run to verify it fails**

Add `nereus_add_test(tst_pan_layout_dialog_gating)` to `tests/CMakeLists.txt`.

```bash
cmake --build build --target tst_pan_layout_dialog_gating
```

Expected: FAIL to compile, the three-argument constructor does not exist.

- [ ] **Step 3: Rewrite the header**

`src/gui/PanLayoutDialog.h` keeps its existing attribution block, with the
cite stamp bumped from `@0cd4559` to `@c6481cb` and a new modification-history
line. The class becomes:

```cpp
class PanLayoutDialog : public QDialog {
    Q_OBJECT

public:
    /// maxSlices comes from BoardCapabilities for the connected radio.
    /// boardName appears in the footer when layouts are hidden.
    PanLayoutDialog(int maxSlices, const QString& currentLayoutId,
                    const QString& boardName, QWidget* parent = nullptr);
    ~PanLayoutDialog() override;

    QString selectedLayout() const { return m_selected; }

    /// Layout ids actually shown, in grid order. For tests.
    QStringList visibleLayoutIds() const { return m_visibleIds; }

    /// Footer sentence, empty when nothing was hidden.
    QString footerText() const { return m_footerText; }

private:
    void buildUi(int maxSlices, const QString& currentLayoutId,
                 const QString& boardName);

    QString     m_selected;
    QStringList m_visibleIds;
    QString     m_footerText;
};
```

- [ ] **Step 4: Rewrite the implementation**

Replace the body of `src/gui/PanLayoutDialog.cpp` below the header block:

```cpp
#include "gui/PanLayoutDialog.h"

#include "gui/StyleConstants.h"
#include "gui/widgets/LayoutThumbnail.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {
// From AetherSDR PanLayoutDialog.cpp:138 [@c6481cb].
constexpr int kMaxCols = 3;
} // namespace

PanLayoutDialog::PanLayoutDialog(int maxSlices, const QString& currentLayoutId,
                                 const QString& boardName, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Panadapter Layout"));
    setStyleSheet(QStringLiteral("background: %1; color: %2;")
                      .arg(Style::kAppBg, Style::kTextPrimary));
    buildUi(maxSlices, currentLayoutId, boardName);
}

PanLayoutDialog::~PanLayoutDialog() = default;

void PanLayoutDialog::buildUi(int maxSlices, const QString& currentLayoutId,
                              const QString& boardName)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(16, 16, 16, 16);
    vbox->setSpacing(8);

    auto* title = new QLabel(tr("Choose panadapter layout"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 13px; font-weight: bold; }")
        .arg(Style::kTextSecondary));
    vbox->addWidget(title);

    auto* grid = new QGridLayout();
    grid->setSpacing(8);
    vbox->addLayout(grid);

    int col = 0;
    int row = 0;
    int hidden = 0;

    for (const PanLayoutGeometry& g : kPanLayouts) {
        // Hide rather than grey. A tile that can never be clicked is noise,
        // and greying nine down to three on a 2-slice board makes the dialog
        // look broken. AetherSDR greys; see design §8.4 for the divergence.
        if (g.panCount > maxSlices) {
            ++hidden;
            continue;
        }
        m_visibleIds << g.id;

        auto* btn = new QPushButton(this);
        btn->setFixedSize(130, 118);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; border: none; }"
            "QPushButton:hover { background: rgba(0, 180, 216, 30);"
            " border: 1px solid %1; border-radius: 4px; }")
            .arg(Style::kAccent));

        auto* btnLayout = new QVBoxLayout(btn);
        btnLayout->setContentsMargins(4, 4, 4, 2);
        btnLayout->setSpacing(2);
        btnLayout->addWidget(
            new LayoutThumbnail(g, g.id == currentLayoutId, true, btn),
            0, Qt::AlignCenter);

        auto* caption = new QLabel(
            tr("%1 (%2 pan%3)").arg(g.label)
                               .arg(g.panCount)
                               .arg(g.panCount > 1 ? QStringLiteral("s")
                                                   : QString()),
            btn);
        caption->setAlignment(Qt::AlignCenter);
        caption->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
        btnLayout->addWidget(caption);

        const QString layoutId = g.id;
        connect(btn, &QPushButton::clicked, this, [this, layoutId]() {
            m_selected = layoutId;
            accept();
        });

        grid->addWidget(btn, row, col);
        if (++col >= kMaxCols) {
            col = 0;
            ++row;
        }
    }

    if (hidden > 0) {
        // Names the radio, never the app: "this radio does not have that".
        m_footerText = tr("%1 allots %2 slices. %n layout(s) need a radio "
                          "with more.", "", hidden)
                           .arg(boardName)
                           .arg(maxSlices);
        auto* footer = new QLabel(m_footerText, this);
        footer->setAlignment(Qt::AlignCenter);
        footer->setWordWrap(true);
        footer->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }").arg(Style::kTextScale));
        vbox->addWidget(footer);
    }

    auto* footerRow = new QHBoxLayout();
    vbox->addLayout(footerRow);
    footerRow->addStretch(1);
    auto* cancelBtn = new QPushButton(tr("Cancel"), this);
    cancelBtn->setStyleSheet(Style::buttonBaseStyle());
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    footerRow->addWidget(cancelBtn);
    footerRow->addStretch(1);
}

} // namespace NereusSDR
```

If `tr(..., "", n)` plural handling proves awkward in the test assertions,
build the sentence explicitly instead:

```cpp
        const QString layoutWord = (hidden == 1) ? tr("1 layout needs")
                                                 : tr("%1 layouts need").arg(hidden);
        m_footerText = tr("%1 allots %2 slices. %3 a radio with more.")
                           .arg(boardName).arg(maxSlices).arg(layoutWord);
```

The tests assert on `"1 layout"` and `"3 layouts"`, so the explicit form is
the safer choice.

- [ ] **Step 5: Fix the existing call site**

`View > Pan Layout…` at `src/gui/MainWindow.cpp:5157` constructs
`PanLayoutDialog` with the old signature. Update it to pass the live values:

```cpp
        const int maxSlices = m_radioModel ? m_radioModel->maxSlices() : 1;
        const QString boardName = m_radioModel
            ? m_radioModel->currentRadio().name : QString();
        PanLayoutDialog dlg(maxSlices,
                            m_panStack ? m_panStack->currentLayoutId()
                                       : QStringLiteral("1"),
                            boardName, this);
        if (dlg.exec() == QDialog::Accepted && !dlg.selectedLayout().isEmpty()) {
            applyPanLayout(dlg.selectedLayout());
        }
```

- [ ] **Step 6: Run the test to verify it passes**

```bash
cmake --build build --target tst_pan_layout_dialog_gating && ctest --test-dir build -R tst_pan_layout_dialog_gating --output-on-failure
```

Expected: PASS, 7 of 7.

- [ ] **Step 7: Commit**

```bash
git add src/gui/PanLayoutDialog.h src/gui/PanLayoutDialog.cpp \
        src/gui/MainWindow.cpp tests/tst_pan_layout_dialog_gating.cpp \
        tests/CMakeLists.txt
git commit -S -m "feat(pan): thumbnail grid replaces the five text tiles

PanLayoutDialog was five text tiles reachable only from View, showing
layout ids as bare strings. It is now AetherSDR's three-column painted
grid at @c6481cb, nine layouts, with the current one highlighted.

Unavailable layouts are hidden rather than greyed, with a footer naming
the board and the count. AetherSDR greys and says nothing, because it is
relaying a capacity number from an API it does not own. We allocate DDCs
locally, so we can write the sentence that turns \"this app does not
have that\" into \"this radio does not have that\". Greying nine tiles
down to three on a Hermes II would just look broken.

Accepted cost: the grid reflows per radio, so tile positions are not
stable across radios on a multi-radio bench."
```

---

## Task B4: The +PAN icon, wired to the grid

**Files:**
- Modify: `src/gui/MainWindow.cpp` (`buildStatusBar()` `+PAN` block, `showPanMenu()`)
- Modify: `src/gui/MainWindow.h`
- Test: `tests/tst_pan_menu_routing.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `PanLayoutDialog(int, const QString&, const QString&, QWidget*)`
  and `::selectedLayout()` from Task B3; `MainWindow::applyPanLayout(const QString&)`.
- Produces: `MainWindow::showPanLayoutDialog()` replacing `showPanMenu()`.
  Task B5 does not depend on it.

- [ ] **Step 1: Write the failing test**

**Same pre-flight defect as Task A8's test:** the three `MainWindow w;`
constructions below start real radio discovery and will `SIGABRT` the test
binary on teardown (`SpectrumThread` still running). See the note at the
top of Task A8's Step 1 for the evidence; find another way to reach
`addPanButton` (a standalone host widget, or a narrower accessor) before
using this pattern.

Create `tests/tst_pan_menu_routing.cpp`:

```cpp
// no-port-check: AetherSDR-derived behaviour; see PanLayoutDialog.h.
#include <QtTest/QtTest>
#include <QLabel>
#include "gui/MainWindow.h"

using namespace NereusSDR;

class TstPanMenuRouting : public QObject {
    Q_OBJECT
private slots:
    void panButtonIsAnIconNotText() {
        MainWindow w;
        auto* btn = w.findChild<QLabel*>(QStringLiteral("addPanButton"));
        QVERIFY(btn);
        QVERIFY2(btn->text().isEmpty(), qPrintable(btn->text()));
        QVERIFY(!btn->pixmap().isNull());
    }

    void panButtonIsDimWhenDisconnected() {
        MainWindow w;
        auto* btn = w.findChild<QLabel*>(QStringLiteral("addPanButton"));
        QVERIFY(btn);
        // Disconnected at construction; the affordance must read unavailable
        // before the click, not silently no-op after it.
        QVERIFY(!btn->isEnabled());
        QVERIFY(btn->toolTip().contains(QStringLiteral("Connect a radio")));
    }

    void panButtonTooltipCarriesNoSourceCite() {
        MainWindow w;
        auto* btn = w.findChild<QLabel*>(QStringLiteral("addPanButton"));
        QVERIFY(btn);
        QVERIFY(!btn->toolTip().contains(QStringLiteral("AetherSDR")));
        QVERIFY(!btn->toolTip().contains(QLatin1Char('@')));
    }
};
QTEST_MAIN(TstPanMenuRouting)
#include "tst_pan_menu_routing.moc"
```

- [ ] **Step 2: Register and run to verify it fails**

Add `nereus_add_test(tst_pan_menu_routing)` to `tests/CMakeLists.txt`.

```bash
cmake --build build --target tst_pan_menu_routing
```

Expected: FAIL, `addPanButton` is a `QPushButton` with text.

- [ ] **Step 3: Replace the text pill with the drawn icon**

In `src/gui/MainWindow.cpp`, replace the `panBtn` block at `:6118-6130`:

```cpp
    // +PAN icon. From AetherSDR MainWindow.cpp:4368-4396 [@c6481cb]: a jagged
    // spectrum polyline with a plus in the upper right. An icon reads as a
    // control where a text pill reads as a label, and the trace says what
    // kind of thing it adds.
    auto* panBtn = new QLabel(barWidget);
    panBtn->setObjectName(QStringLiteral("addPanButton"));
    panBtn->setAccessibleName(tr("Add panadapter"));
    {
        QPixmap pm(36, 28);
        pm.fill(Qt::transparent);
        QPainter pp(&pm);
        pp.setRenderHint(QPainter::Antialiasing);
        const QColor stroke(255, 255, 255, 210);
        pp.setPen(QPen(stroke, 1.6));
        const QPointF pts[] = {
            { 0, 22}, { 1, 21}, { 2, 22}, { 3, 19}, { 4, 22},
            { 5, 21}, { 6, 18}, { 7, 12}, { 8, 17}, { 9, 22},
            {10, 21}, {11, 22}, {12, 16}, {13, 22},
            {14, 21}, {15, 19}, {16, 22},
            {17, 20}, {18, 12}, {19,  4}, {20, 11}, {21, 21},
            {22, 22}, {23, 21}, {24, 17}, {25, 22},
            {26, 21}, {27, 22}, {28, 18}, {29, 22}, {30, 22}
        };
        pp.drawPolyline(pts, sizeof(pts) / sizeof(pts[0]));
        pp.setPen(QPen(stroke, 2.2));
        pp.drawLine(30, 4, 30, 14);
        pp.drawLine(25, 9, 35, 9);
    }
    panBtn->setPixmap(pm);
    panBtn->setCursor(Qt::PointingHandCursor);
    panBtn->installEventFilter(this);
    panBtn->setProperty("isAddPanButton", true);
    hbox->addWidget(panBtn);
    m_addPanButton = panBtn;
    updateAddPanButtonState();
```

Move the `QPixmap pm` declaration above the brace so `setPixmap` can see it.

- [ ] **Step 4: Add the dim-when-disconnected state**

```cpp
void MainWindow::updateAddPanButtonState()
{
    if (!m_addPanButton) { return; }
    const bool connected = m_radioModel && m_radioModel->isConnected();
    m_addPanButton->setEnabled(connected);
    // AetherSDR's gate is a silent early return, which reads as a dead click.
    // Show the state before the click instead (design §8.2).
    auto* fx = qobject_cast<QGraphicsOpacityEffect*>(
        m_addPanButton->graphicsEffect());
    if (!fx) {
        fx = new QGraphicsOpacityEffect(m_addPanButton);
        m_addPanButton->setGraphicsEffect(fx);
    }
    fx->setOpacity(connected ? 1.0 : 0.35);
    m_addPanButton->setToolTip(connected
        ? tr("Change panadapter layout")
        : tr("Connect a radio to change pan layout"));
}
```

Call it from the existing `connectionStateChanged` handler, and declare it in
`MainWindow.h` alongside `QLabel* m_addPanButton{nullptr};`.

- [ ] **Step 5: Replace showPanMenu with the dialog**

Delete the body of `showPanMenu()` (`:8714-8773`) and replace with:

```cpp
void MainWindow::showPanLayoutDialog()
{
    if (!m_radioModel || !m_radioModel->isConnected()) {
        return;
    }
    const int maxSlices = m_radioModel->maxSlices();
    const QString boardName = m_radioModel->currentRadio().name;
    PanLayoutDialog dlg(maxSlices,
                        m_panStack ? m_panStack->currentLayoutId()
                                   : QStringLiteral("1"),
                        boardName, this);
    if (dlg.exec() == QDialog::Accepted && !dlg.selectedLayout().isEmpty()) {
        applyPanLayout(dlg.selectedLayout());
    }
}
```

Rename the declaration in `MainWindow.h` and route the click through
`eventFilter`, next to the existing `isPanelToggle` handling:

```cpp
    if (watched->property("isAddPanButton").toBool()
        && event->type() == QEvent::MouseButtonPress) {
        showPanLayoutDialog();
        return true;
    }
```

- [ ] **Step 6: Run the test to verify it passes**

```bash
cmake --build build --target tst_pan_menu_routing && ctest --test-dir build -R tst_pan_menu_routing --output-on-failure
```

Expected: PASS, 3 of 3.

- [ ] **Step 7: Commit**

```bash
git add src/gui/MainWindow.cpp src/gui/MainWindow.h \
        tests/tst_pan_menu_routing.cpp tests/CMakeLists.txt
git commit -S -m "feat(pan): +PAN becomes a drawn icon opening the layout grid

The spectrum-trace-plus-plus pixmap is from AetherSDR MainWindow.cpp
:4368-4396 @c6481cb, traced from the same point list. An icon reads as
a control; the text pill it replaces reads as a label.

showPanMenu is retired. Its layout section listed ids as bare strings
and is superseded by the thumbnail grid; its two per-pan actions move in
the next commit.

One divergence from AetherSDR: their connection gate is a silent early
return, which reads as a dead click. Ours dims the icon and says
\"Connect a radio to change pan layout\" in the tooltip, so the state is
visible before the click rather than after it."
```

---

## Task B5: Per-pan add-slice and float

**Files:**
- Modify: `src/gui/PanadapterApplet.cpp`, `src/gui/PanadapterApplet.h`
- Test: `tests/tst_pan_menu_routing.cpp` (extend)

**Interfaces:**
- Consumes: `PanadapterApplet::panId() const -> QString`,
  `RadioModel::addSliceOnPan(const QString& panId)`,
  `PanadapterStack::floatPanadapter(const QString& panId)`.
- Produces: `PanadapterApplet` signals `addSliceRequested(const QString& panId)`
  and `floatRequested(const QString& panId)`, both carrying the applet's own
  pan id. Nothing downstream consumes them.

**Rule (design §8.5):** these two actions resolved through
`m_panStack->activePanId()` from a button sitting nowhere near any pan. A
control drawn on a pan targets *that* pan.

- [ ] **Step 1: Write the failing test**

Append to `tests/tst_pan_menu_routing.cpp`:

```cpp
    void perPanMenuCarriesItsOwnPanId() {
        PanadapterApplet a(QStringLiteral("pan-2"));
        QSignalSpy addSpy(&a, &PanadapterApplet::addSliceRequested);
        QSignalSpy floatSpy(&a, &PanadapterApplet::floatRequested);

        a.emitAddSliceForTest();
        a.emitFloatForTest();

        QCOMPARE(addSpy.count(), 1);
        QCOMPARE(addSpy.at(0).at(0).toString(), QStringLiteral("pan-2"));
        QCOMPARE(floatSpy.count(), 1);
        QCOMPARE(floatSpy.at(0).at(0).toString(), QStringLiteral("pan-2"));
    }

    void twoPansEmitDifferentIds() {
        PanadapterApplet a(QStringLiteral("pan-0"));
        PanadapterApplet b(QStringLiteral("pan-1"));
        QSignalSpy spyA(&a, &PanadapterApplet::addSliceRequested);
        QSignalSpy spyB(&b, &PanadapterApplet::addSliceRequested);

        b.emitAddSliceForTest();

        QCOMPARE(spyA.count(), 0);
        QCOMPARE(spyB.count(), 1);
        QCOMPARE(spyB.at(0).at(0).toString(), QStringLiteral("pan-1"));
    }
```

Add `#include "gui/PanadapterApplet.h"` and `#include <QSignalSpy>`.

- [ ] **Step 2: Run to verify it fails**

```bash
cmake --build build --target tst_pan_menu_routing
```

Expected: FAIL to compile, signals do not exist.

- [ ] **Step 3: Add the signals and menu entries**

In `src/gui/PanadapterApplet.h`:

```cpp
signals:
    /// Both carry THIS applet's pan id. Never routed through an
    /// active-pan lookup: a control drawn on a pan targets that pan.
    void addSliceRequested(const QString& panId);
    void floatRequested(const QString& panId);

public:
    // Test seams so the routing can be asserted without synthesising a
    // context-menu event.
    void emitAddSliceForTest() { emit addSliceRequested(panId()); }
    void emitFloatForTest()    { emit floatRequested(panId()); }
```

In the applet's existing context-menu builder in `PanadapterApplet.cpp`, add
at the top of the menu:

```cpp
    menu.addAction(tr("Add slice on this pan"), this, [this]() {
        emit addSliceRequested(panId());
    });
    menu.addAction(tr("Float this pan"), this, [this]() {
        emit floatRequested(panId());
    });
    menu.addSeparator();
```

If `PanadapterApplet` has no context menu yet, add
`setContextMenuPolicy(Qt::CustomContextMenu)` in the constructor and a
`customContextMenuRequested` handler that builds and execs the menu.

- [ ] **Step 4: Wire them in MainWindow**

Wherever `PanadapterApplet` instances are wired (search `wirePanadapter`):

```cpp
    connect(applet, &PanadapterApplet::addSliceRequested,
            this, [this](const QString& panId) {
        if (m_radioModel) { m_radioModel->addSliceOnPan(panId); }
    });
    connect(applet, &PanadapterApplet::floatRequested,
            this, [this](const QString& panId) {
        if (m_panStack) { m_panStack->floatPanadapter(panId); }
    });
```

- [ ] **Step 5: Run the test to verify it passes**

```bash
cmake --build build --target tst_pan_menu_routing && ctest --test-dir build -R tst_pan_menu_routing --output-on-failure
```

Expected: PASS, 5 of 5.

- [ ] **Step 6: Commit**

```bash
git add src/gui/PanadapterApplet.h src/gui/PanadapterApplet.cpp \
        src/gui/MainWindow.cpp tests/tst_pan_menu_routing.cpp
git commit -S -m "fix(pan): add-slice and float target their own pan

Both actions resolved through m_panStack->activePanId() from the +PAN
button, which sits nowhere near any pan, so \"add slice on active pan\"
meant whichever pan happened to be active when you reached the far side
of the window. They now live on the pan's own right-click menu and carry
that applet's pan id.

That retires the last activePanId() routing from the +PAN affordance and
leaves it doing exactly one job. The test asserts two applets emit
different ids, which is the property the active-pan lookup could not
give."
```

---

## Task B6: Attribution closeout

**Files:**
- Modify: `src/gui/PanLayoutDialog.h`, `src/gui/PanLayoutDialog.cpp` (cite stamp)
- Modify: `docs/attribution/aethersdr-contributor-index.md:270`

**Interfaces:** none. This task produces no code.

- [ ] **Step 1: Bump the PanLayoutDialog cite stamp**

In both `src/gui/PanLayoutDialog.h` and `.cpp`, change every `[@0cd4559]` to
`[@c6481cb]`, and append to the modification history:

```
//   2026-08-02  J.J. Boyd / KG4VCF  Re-ported against AetherSDR
//                                    @c6481cb: painted LayoutThumbnail
//                                    grid replaces the five text tiles.
//                                    Nine layouts, not twelve; hide
//                                    rather than grey. See design §8.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
```

- [ ] **Step 2: Update the contributor index**

In `docs/attribution/aethersdr-contributor-index.md`, row 270 currently reads
`(pending Phase 3F)`. Change that cell to:

```
`v0.5.3` @c6481cb
```

and extend the file list in that row to include
`LayoutThumbnail.{h,cpp}`.

- [ ] **Step 3: Run the attribution gates**

```bash
python3 scripts/verify-thetis-headers.py --all-kinds
python3 scripts/check-new-ports.py
python3 scripts/verify-inline-cites.py
```

Expected: all pass. `check-new-ports.py` in particular must report every new
file as attributed or skip-marked; if it flags `ChromeFoldPlan`,
`ChromeBarController` or `SystemTile`, their `no-port-check` marker is missing
or malformed.

- [ ] **Step 4: Run the full suite once**

Per `docs/development/fast-test-loop.md`, this is the single full run:

```bash
cmake --build build --target all_tests && ctest --test-dir build --output-on-failure
```

Expected: all PASS. Any failure, including one that looks unrelated, gets
diagnosed here rather than carried forward.

- [ ] **Step 5: Commit**

```bash
git add src/gui/PanLayoutDialog.h src/gui/PanLayoutDialog.cpp \
        docs/attribution/aethersdr-contributor-index.md
git commit -S -m "docs(attribution): bump PanLayoutDialog to AetherSDR @c6481cb

The file carried a port citation against @0cd4559 from the Phase 3F
five-tile version. This epic re-ports it against @c6481cb, so the stamp
moves and LayoutThumbnail joins the file list.

The contributor-index row for the multi-pan UI leaves its (pending Phase
3F) state. AetherSDR carries no per-file headers, so this stays a
project-level citation per HOW-TO-PORT.md rule 6; there is no verbatim
block to copy and none is fabricated."
```

---

## Self-review

**Spec coverage.** Every design section maps to a task:

| Spec | Task |
| --- | --- |
| §4.1 identity merge | A4, wired in A8 |
| §4.2 pills densified, slice-bound | A5 |
| §4.3 PA + CPU merge | A3, wired in A8 |
| §4.4 clock relocation | A7 |
| §4.5 reserved safety slots | A6 |
| §5 single layout authority | A1, A2, A8 |
| §6 fold ladder | A1 rung table, A8 registration |
| §7 degenerate cases | A3 tests, A8 disconnect handling |
| §8.1 drawn icon | B4 |
| §8.2 thumbnail grid, connection gate | B3, B4 |
| §8.3 nine layouts | B1, B2 |
| §8.4 hide plus footer | B3 |
| §8.5 per-pan routing | B5 |
| §9.1 unit tests | A1, A2, A8, B3 |
| §10 attribution | B6 |

**Known gaps, deliberate.** Spec §9.2 bench steps are operator work and are
not tasks. Spec §12 open questions (revisiting `maxSlices`, tile ordering,
modal versus popup) are explicitly out of scope.

**Type consistency.** `ChromeFoldEntry` fields (`rung`, `widthPx`, `label`)
are used identically in A1 and A2. `ChromeBarController::addItem` has one
signature across A2 and A8. `PanLayoutGeometry` fields (`id`, `label`,
`panCount`, `rows`) are identical in B2 and B3. `PanLayoutDialog`'s
four-argument constructor is used identically in B3 step 5 and B4 step 5.
`SystemTile`'s setters in A3 match the call sites in A8.

**Rung numbering** is defined once in A1 and referenced by A5 (`badgeForRung`
maps 5 through 9) and A8 (registration). No other task assigns rungs.
