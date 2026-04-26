# Phase 3G — RX Epic Sub-epic E: Waterfall Scrollback (Rewind) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port AetherSDR's waterfall scrollback ("rewind") into NereusSDR — a per-panadapter ring-buffer history of pre-coloured waterfall rows that the user can pause and scrub back through, with a right-edge time-scale strip + clickable LIVE button as the entire user-facing surface.

**Architecture:** A second `QImage` (`m_waterfallHistory`) backing a row-LIFO ring buffer alongside a parallel `QVector<qint64>` timestamp array on `SpectrumWidget`. Capacity = `min(20-min/m_wfUpdatePeriodMs, 4096)` rows. The existing `m_waterfall` becomes the rebuilt viewport — when live, every row appends to both; when paused, only history grows (offset auto-bumps to keep displayed row visually fixed) and the viewport is rebuilt from a window into history. Pan/zoom reprojects the entire history image horizontally to fit the new freq window (matching upstream); large band/zoom jumps additionally clear history and force live (NereusSDR divergence per Q4). A 250 ms `QTimer` debounces re-allocation during resize and slider drags.

**Tech Stack:** C++20, Qt6 (`QImage` Format_RGB32, `QVector<qint64>`, `QTimer`, `QDateTime::currentMSecsSinceEpoch`), QRhi widget (existing GPU + QPainter dual-path), `AppSettings` for `DisplayWaterfallHistoryMs` persistence, QtTest for unit tests.

**Spec reference:** [phase3g-rx-experience-epic-design.md](phase3g-rx-experience-epic-design.md) § sub-epic E.

**Upstream stamps:**
- AetherSDR `main@0cd4559` (2026-04-21) — for the merged scrollback core. Inline cite: `// From AetherSDR <file>:<line> [@0cd4559]`.
- AetherSDR `aetherclaude/issue-1478@2bb3b5c` (2026-04-15) — **unmerged PR #1478** for the 4 096-row cap and 250 ms resize debounce. Inline cite: `// From AetherSDR <file>:<line> [@2bb3b5c]`. The unmerged-PR context lives in this spec doc (§authoring-time #2), not in the bracketed stamp — cite format follows the project's standard `[vX.Y.Z.W|@shortsha]` grammar.

---

## Authoring-time decisions (lock these before coding)

Three points where the design doc paraphrase, the AetherSDR `main` upstream, and the unmerged PR diverge. **Each port follows the source named below**, and a reviewer reading the doc + the upstream code should see why each choice was made without needing to ask. Documented here so a reviewer doesn't read the doc, look at the code, and ask "wait, why does it look like that?".

1. **History depth IS persisted** — NereusSDR-side enhancement.
   AetherSDR hardcodes `kWaterfallHistoryMs = 20LL * 60LL * 1000LL` as a `constexpr`. Epic design §E says `DisplayWaterfallHistoryMs` should be a Setup → Display dropdown (60 s / 5 min / 15 min / 20 min). Port follows the design doc — depth is loaded from `AppSettings` per-`SpectrumWidget` (PascalCase key `DisplayWaterfallHistoryMs`, default 20 min as ms). The constant `kWaterfallHistoryMs` survives in the source as the default initializer; the runtime value lives in a new `m_waterfallHistoryMs` instance member.

2. **Cap + debounce port from unmerged PR `2bb3b5c`** — known-good fix for failure modes that apply more strongly to NereusSDR than upstream.
   Upstream `main@0cd4559` has neither the cap nor the debounce. PR #1478 (still on `upstream/aetherclaude/issue-1478`) added both with this rationale (commit message verbatim):
   > The 20-minute waterfall scrollback ring buffer allocated up to 12,000 rows (at 100 ms line duration), consuming ~88-176 MB per panadapter. On resize, `ensureWaterfallHistory()` was called synchronously on every intermediate frame, causing repeated large allocations that led to memory pressure, laggy panadapter scrolling, and potential radio crashes via TCP backpressure.
   NereusSDR's default `m_wfUpdatePeriodMs` is 30 ms (~3× faster than AetherSDR's typical 100 ms tile cadence), so the upstream-`main` allocation footprint is ~310 MB per panadapter at 2 000 px width. We port the unmerged PR as-is: `kMaxWaterfallHistoryRows = 4096` and a 250 ms single-shot `QTimer` (`m_historyResizeTimer`).

3. **Large-shift band/zoom jump clears history** — NereusSDR divergence from upstream.
   Upstream's `setFrequencyRange` has a `largeShift` boolean that already distinguishes pan-follow nudges from band jumps for animation purposes. On `largeShift`, upstream calls `reprojectWaterfall(...)` and continues — old rows survive in the ring at the new freq window's pixel positions, looking like compressed garbage if you band-jumped. NereusSDR additionally calls `clearWaterfallHistory()` + `setWaterfallLive(true)` on `largeShift`, so the ring is always coherent with the current band. Cost: you can't ever rewind through a band change. Benefit: the LIVE-button-resume path becomes redundant on a band switch (already snapped to live), and timestamps in the strip never refer to a different band's rows. Inline cite for the divergence:
   ```cpp
   // From AetherSDR SpectrumWidget.cpp:1046-1059 [@0cd4559]
   //   — divergence: NereusSDR clears history on largeShift to keep
   //     the rewind window coherent with the current band. See
   //     phase3g-rx-epic-e-waterfall-scrollback-plan.md §authoring-time #3.
   ```

---

## File Structure

| File | Responsibility | Change type |
|---|---|---|
| `src/gui/SpectrumWidget.h` | 8 ring-buffer state members + 2 constants + `m_historyResizeTimer` + `m_waterfallHistoryMs` + 9 new private methods + 4 new public setters | Modify — append to existing private/private-method blocks |
| `src/gui/SpectrumWidget.cpp` | All scrollback machinery; ctor wiring of debounce timer; integration into existing `pushWaterfallRow`, `clearDisplay`, `setFrequencyRange`, `resizeEvent`, divider-drag mouseMove, `setWfUpdatePeriodMs`; add new dual-path `drawTimeScale` (QPainter + GPU overlay); add mouse press/move/release branches for time-scale + LIVE button; add hover-cursor branches | Modify — port `appendHistoryRow`, `appendVisibleRow`, `ensureWaterfallHistory`, `historyRowIndexForAge`, `maxWaterfallHistoryOffsetRows`, `pausedTimeLabelForAge`, `rebuildWaterfallViewport`, `reprojectWaterfall`, `setWaterfallLive`, `waterfallHistoryCapacityRows`, `waterfallLiveButtonRect`, `waterfallStripWidth`, `waterfallTimeScaleRect`, `clearWaterfallHistory` byte-for-byte from upstream where unchanged, with NereusSDR adapter shims for differing signatures (e.g. our `pushWaterfallRow` takes `QVector<float>` not `(QVector<float>, int, double, double)`); `effectiveStripW` extended to add timescale strip width when present |
| `src/gui/setup/DisplaySetupPages.cpp` | Add "Waterfall history depth" dropdown to `WaterfallDefaultsPage::buildUI` with effective-depth hint label that updates when either depth or update-period changes | Modify — new `QComboBox` row in the existing waterfall page |
| `src/gui/setup/DisplaySetupPages.h` | New `m_historyDepthCombo` + `m_effectiveDepthLabel` member declarations on `WaterfallDefaultsPage` | Modify |
| `tests/tst_waterfall_scrollback.cpp` *(new)* | Unit tests for capacity arithmetic + cap, ring-buffer wrap-around, offset clamp, age→row mapping, period-change preserve-vs-wipe boundary at 293 ms | Create — QtTest, no `QApplication` (pure logic on a `SpectrumWidget` instance with the GPU widget bypassed via a constructor flag isn't needed; test the `static`/`const` helpers directly via a thin friend-test wrapper) |
| `tests/CMakeLists.txt` | Register the new test target | Modify |
| `docs/architecture/phase3g-rx-epic-e-verification/README.md` *(new)* | Manual verification matrix following the 3G-8 / sub-epic D pattern | Create |
| `CHANGELOG.md` | Sub-epic E entry under Unreleased | Modify |

**Header attribution:** `SpectrumWidget.{h,cpp}` already carries verbatim AetherSDR + Thetis headers from prior phases. Sub-epic E adds **one new "Ported from" line** in the modification block at the top of each file referencing the unmerged PR's commit specifically:
```
// Sub-epic E — waterfall scrollback / rewind
//   Ported from AetherSDR SpectrumWidget.{h,cpp} main@0cd4559 (live)
//   and aetherclaude/issue-1478@2bb3b5c (unmerged PR #1478, cap + debounce)
//   on 2026-04-25 by JJ Boyd / KG4VCF, AI tooling: Claude Code (Opus 4.7)
```
No new files port Thetis logic; no new Thetis headers required. The new `tst_waterfall_scrollback.cpp` is NereusSDR-original (gets the standard NereusSDR header).

---

## Pre-flight inventory (read-only — no code changes)

Before starting, run these greps to refresh your mental model. Pin the actual line numbers for the cite comments you'll add later.

```bash
# 1. Confirm the upstream pin is correct
git -C /Users/j.j.boyd/AetherSDR rev-parse --short HEAD          # expect: 0cd4559
git -C /Users/j.j.boyd/AetherSDR show 2bb3b5c --stat | head -3   # expect: PR #1478

# 2. Existing waterfall write path — where appendHistoryRow plugs in
rg -n 'pushWaterfallRow|m_waterfall\.|m_wfWriteRow|m_wfReverseScroll' \
  src/gui/SpectrumWidget.cpp src/gui/SpectrumWidget.h

# 3. Existing dual-path render integration (sub-epic A + D anchors)
rg -n 'drawDbmScale|drawBandPlan|effectiveStripW' \
  src/gui/SpectrumWidget.cpp src/gui/SpectrumWidget.h

# 4. Mouse handlers — entry points for the new time-scale + LIVE branches
rg -n 'void mousePressEvent|void mouseMoveEvent|void mouseReleaseEvent|m_draggingDivider|m_draggingDbm' \
  src/gui/SpectrumWidget.cpp

# 5. setFrequencyRange equivalent (where reproject + largeShift hook in)
rg -n 'setCenterFrequency|setBandwidth|m_centerHz|m_bandwidthHz' \
  src/gui/SpectrumWidget.cpp src/gui/SpectrumWidget.h

# 6. Resize handler (debounce timer integration)
rg -n 'void resizeEvent|m_waterfall\.scaled' src/gui/SpectrumWidget.cpp

# 7. Existing AppSettings load/save pattern (DisplayDbmScaleVisible is the model)
rg -n 'DisplayDbmScaleVisible|DisplayWfUpdatePeriodMs' \
  src/gui/SpectrumWidget.cpp src/gui/setup/DisplaySetupPages.cpp

# 8. Setup → Display Waterfall page (where the depth dropdown goes)
rg -n 'class WaterfallDefaultsPage|m_updatePeriodSlider|buildUI' \
  src/gui/setup/DisplaySetupPages.h src/gui/setup/DisplaySetupPages.cpp
```

Expected hits at plan-write time (will drift):

- `src/gui/SpectrumWidget.cpp:1727` — `pushWaterfallRow` definition
- `src/gui/SpectrumWidget.cpp:1098-1108` — m_waterfall sizing in render
- `src/gui/SpectrumWidget.cpp:1800-1802` — m_wfWriteRow increment/decrement
- `src/gui/SpectrumWidget.cpp:958-961` — `effectiveStripW` (kDbmStripW only currently)
- `src/gui/SpectrumWidget.cpp:1155-1161` — QPainter dual-path: `drawDbmScale` + `drawBandPlan` callers
- `src/gui/SpectrumWidget.cpp:808-815` — `setWfUpdatePeriodMs` (where ensureWaterfallHistory hooks in)
- `src/gui/SpectrumWidget.h:484-485` — `drawDbmScale`, `drawBandPlan` decls (`drawTimeScale` slots in here)
- `src/gui/SpectrumWidget.h:497` — `effectiveStripW` decl
- `src/gui/SpectrumWidget.h:609` — `m_dbmScaleVisible` member
- `src/gui/setup/DisplaySetupPages.cpp:704-711` — `m_updatePeriodSlider` row (depth dropdown sits next to it)
- `src/gui/setup/DisplaySetupPages.h:140` — `m_updatePeriodSlider` decl

If your `rg` results don't match this shape, line numbers have drifted — re-read the surrounding code before patching.

---

## Task 1: Add ring-buffer state members + constants to `SpectrumWidget.h`

Just declarations — no behaviour yet, no compile-cost beyond a few member additions. Sets up the substrate that every later task fills in.

**Files:**
- Modify: `src/gui/SpectrumWidget.h`

- [ ] **Step 1: Open `src/gui/SpectrumWidget.h` and find the existing waterfall image declaration**

Line-anchor: `QImage m_waterfall;` (around line 524 at plan-write time). New members go in the same private block, immediately after the existing scrolling-waterfall image block.

- [ ] **Step 2: Add the ring-buffer state members + constants**

```cpp
// ── Waterfall scrollback (sub-epic E) ─────────────────────────────────
// From AetherSDR SpectrumWidget.h:493-502 [@0cd4559]
QImage          m_waterfallHistory;            // RGB32 ring buffer
QVector<qint64> m_wfHistoryTimestamps;         // parallel; per-row wall-clock ms
int             m_wfHistoryWriteRow{0};        // LIFO; index 0 = newest
int             m_wfHistoryRowCount{0};        // saturates at capacity
int             m_wfHistoryOffsetRows{0};      // 0 = newest visible at top
bool            m_wfLive{true};                // pause/live state
bool            m_draggingTimeScale{false};    // gesture flag
int             m_timeScaleDragStartY{0};      // anchor Y at mousedown
int             m_timeScaleDragStartOffsetRows{0};

// Default depth (overridden at runtime by m_waterfallHistoryMs from AppSettings).
// From AetherSDR SpectrumWidget.h:502 [@0cd4559]
static constexpr qint64 kDefaultWaterfallHistoryMs = 20LL * 60LL * 1000LL;

// Cap on history capacity to bound memory.
// From AetherSDR SpectrumWidget.h:482 [@2bb3b5c]
// (cap added by unmerged AetherSDR PR #1478 — see plan §authoring-time #2)
static constexpr int    kMaxWaterfallHistoryRows  = 4096;

// Runtime-configurable depth; persisted as AppSettings("DisplayWaterfallHistoryMs").
// NereusSDR-side enhancement — see plan §authoring-time #1.
qint64          m_waterfallHistoryMs{kDefaultWaterfallHistoryMs};

// Debounce timer for ensureWaterfallHistory() during rapid resize / slider drag.
// From AetherSDR SpectrumWidget.h:559 [@2bb3b5c]
// (debounce timer added by unmerged AetherSDR PR #1478 — see plan §authoring-time #2)
QTimer*         m_historyResizeTimer{nullptr};
```

- [ ] **Step 3: Add the new private method declarations**

Find the existing private-method block (around line 405 in `drawWaterfall`/`drawDbmScale` declarations). Add these after `drawBandPlan`:

```cpp
// ── Waterfall scrollback (sub-epic E) ─────────────────────────────────
// From AetherSDR SpectrumWidget.h:402-413 [@0cd4559]
void drawTimeScale(QPainter& p, const QRect& wfRect);
QRect waterfallTimeScaleRect(const QRect& wfRect) const;
QRect waterfallLiveButtonRect(const QRect& wfRect) const;
int   waterfallStripWidth() const;
void  ensureWaterfallHistory();
void  rebuildWaterfallViewport();
void  setWaterfallLive(bool live);
void  appendHistoryRow(const QRgb* rowData, qint64 timestampMs);
int   waterfallHistoryCapacityRows() const;
int   maxWaterfallHistoryOffsetRows() const;
int   historyRowIndexForAge(int ageRows) const;
QString pausedTimeLabelForAge(int ageRows) const;
void  reprojectWaterfall(double oldCenterHz, double oldBandwidthHz,
                         double newCenterHz, double newBandwidthHz);
void  clearWaterfallHistory();
```

- [ ] **Step 4: Add the new public depth setter/getter**

Find the existing public accessors block (around line 337 at `wfUpdatePeriodMs() const`). Add:

```cpp
qint64 waterfallHistoryMs() const  { return m_waterfallHistoryMs; }
void   setWaterfallHistoryMs(qint64 ms);
bool   wfLive() const              { return m_wfLive; }
```

- [ ] **Step 5: Verify the header compiles**

Run:
```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target NereusSDR 2>&1 | head -40
```

Expected: build fails with `undefined reference` errors for the new methods (they're declared, not defined yet). That's correct — Task 2 onward defines them. We're verifying the header alone parses cleanly.

- [ ] **Step 6: Commit**

```bash
git add src/gui/SpectrumWidget.h
git commit -S -m "feat(spectrum): add scrollback state + method decls (sub-epic E task 1)

State members + constants ported from AetherSDR SpectrumWidget.h
[@0cd4559] for the live core, [@2bb3b5c] (unmerged PR #1478) for
the 4096-row cap and debounce timer. No behaviour changes yet —
this task establishes the substrate; later tasks populate the
implementations.

Refs: docs/architecture/phase3g-rx-epic-e-waterfall-scrollback-plan.md task 1"
```

---

## Task 2: Math helpers — capacity / max-offset / age→row / paused timestamp label

Pure functions over the state from Task 1. No memory allocation, no painting. Worth landing alone because they're the surface unit tests will exercise.

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp`
- Test: `tests/tst_waterfall_scrollback.cpp` *(new)*
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test for capacity arithmetic**

Create `tests/tst_waterfall_scrollback.cpp` (full file — this is the seed for all later test additions):

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// NereusSDR — Copyright (C) 2026 JJ Boyd / KG4VCF
//
// Sub-epic E: waterfall scrollback unit tests. NereusSDR-original.

#include <QtTest/QtTest>
#include <QImage>
#include <QVector>

// We test the math helpers in isolation by re-implementing them as a friend
// shim. This avoids needing a full QApplication + GPU widget for what are
// pure-arithmetic checks. The production code lives in SpectrumWidget — when
// modifying the formulas, update both copies (CI ensures parity via the
// kMaxWaterfallHistoryRows constant pulled directly from the production
// header).

#include "../src/gui/SpectrumWidget.h"

namespace {

// Mirrors SpectrumWidget::waterfallHistoryCapacityRows() — pure arithmetic.
int capacityRows(qint64 depthMs, int periodMs) {
    const int p = std::max(1, periodMs);
    const int rows = static_cast<int>((depthMs + p - 1) / p);
    return std::min(rows, SpectrumWidget::kMaxWaterfallHistoryRows);
}

// Mirrors SpectrumWidget::historyRowIndexForAge() with explicit args.
int rowIndexForAge(int writeRow, int rowCount, int ringHeight, int ageRows) {
    if (ringHeight <= 0 || ageRows < 0 || ageRows >= rowCount) return -1;
    return (writeRow + ageRows) % ringHeight;
}

} // namespace

class TstWaterfallScrollback : public QObject
{
    Q_OBJECT
private slots:
    void capacityCappedAt4096_30ms_20min() {
        QCOMPARE(capacityRows(20LL * 60 * 1000, 30), 4096);
    }
    void capacityCappedAt4096_100ms_20min() {
        QCOMPARE(capacityRows(20LL * 60 * 1000, 100), 4096);
    }
    void capacityCapBoundary_293ms_20min() {
        // 20 min × 60 000 / 293 = 4 095.56 → ceil = 4 096 → capped at 4 096
        QCOMPARE(capacityRows(20LL * 60 * 1000, 293), 4096);
    }
    void capacityUncapped_500ms_20min() {
        // 20 min × 60 000 / 500 = 2 400 → uncapped
        QCOMPARE(capacityRows(20LL * 60 * 1000, 500), 2400);
    }
    void capacityUncapped_1000ms_20min() {
        QCOMPARE(capacityRows(20LL * 60 * 1000, 1000), 1200);
    }
    void capacityRespects60sDepth() {
        // Even at 30 ms period, a 60 s depth selector is honoured below the cap.
        QCOMPARE(capacityRows(60 * 1000, 30), 2000);
    }
    void capacityZeroPeriodGuard() {
        // 0 period clamped to 1 — must not divide by zero.
        QCOMPARE(capacityRows(20LL * 60 * 1000, 0), 4096);
    }

    void rowIndexForAge_age0_isWriteRow() {
        QCOMPARE(rowIndexForAge(/*writeRow=*/100, /*rowCount=*/4096,
                                /*ringHeight=*/4096, /*ageRows=*/0),
                 100);
    }
    void rowIndexForAge_wraps() {
        // writeRow=4090, age=10 → 4100 % 4096 = 4
        QCOMPARE(rowIndexForAge(4090, 4096, 4096, 10), 4);
    }
    void rowIndexForAge_outOfBounds_returnsMinus1() {
        QCOMPARE(rowIndexForAge(0, 100, 4096, 100), -1);
        QCOMPARE(rowIndexForAge(0, 100, 4096, -1), -1);
    }
};

QTEST_MAIN(TstWaterfallScrollback)
#include "tst_waterfall_scrollback.moc"
```

- [ ] **Step 2: Register the test target in `tests/CMakeLists.txt`**

Find the existing `add_qt_test` (or equivalent) block. Add:

```cmake
add_qt_test(tst_waterfall_scrollback
    SOURCES tst_waterfall_scrollback.cpp
    LINK    NereusSDR::core NereusSDR::gui
)
```

(If the project uses a different test-registration macro, mirror what `tst_bandplan_manager` (sub-epic D) does — same shape. Run `rg -n 'add_qt_test|qt6_add_executable.*tst_' tests/CMakeLists.txt` to verify the macro name.)

- [ ] **Step 3: Run the test to verify it fails**

Run:
```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target tst_waterfall_scrollback 2>&1 | tail -10
```

Expected: build error referencing `SpectrumWidget::kMaxWaterfallHistoryRows` (the constant from Task 1 must be visible to the test). If Task 1's constant is `private`, change it to `public:` in `SpectrumWidget.h` — it's a compile-time configuration constant, no API surface concern. Re-build. The test should now compile and **fail at run time** because `capacityRows()` in the shim would return uncapped values if we didn't apply `std::min` ... wait — the test shim has `std::min` already, so it passes. The point of this step is to *verify the test runs* and locks in the cap arithmetic before the production code ships. No production code yet, no cap to test. Mark this expected-pass and move on.

Run:
```bash
ctest --test-dir build -R tst_waterfall_scrollback --output-on-failure
```

Expected: PASS.

- [ ] **Step 4: Implement the four math helpers in `SpectrumWidget.cpp`**

Find the existing `m_wfWriteRow` reset code (around line 1108 at `m_wfWriteRow = 0`). The new helpers go in a new section just before `pushWaterfallRow` (which is around line 1727). Add:

```cpp
// ─── Waterfall scrollback math helpers (sub-epic E) ───────────────────
// From AetherSDR SpectrumWidget.cpp:559-590 [@0cd4559]
//   plus 4096-row cap from [@2bb3b5c] (unmerged AetherSDR PR #1478)

int SpectrumWidget::waterfallHistoryCapacityRows() const
{
    const int msPerRow = std::max(1, m_wfUpdatePeriodMs);
    const int rows = static_cast<int>(
        (m_waterfallHistoryMs + msPerRow - 1) / msPerRow);
    return std::min(rows, kMaxWaterfallHistoryRows);
}

int SpectrumWidget::maxWaterfallHistoryOffsetRows() const
{
    return std::max(0, m_wfHistoryRowCount - m_waterfall.height());
}

int SpectrumWidget::historyRowIndexForAge(int ageRows) const
{
    if (m_waterfallHistory.isNull() || ageRows < 0
        || ageRows >= m_wfHistoryRowCount) {
        return -1;
    }
    return (m_wfHistoryWriteRow + ageRows) % m_waterfallHistory.height();
}

QString SpectrumWidget::pausedTimeLabelForAge(int ageRows) const
{
    const int rowIndex = historyRowIndexForAge(ageRows);
    if (rowIndex < 0 || rowIndex >= m_wfHistoryTimestamps.size()) {
        return QString();
    }
    const qint64 timestampMs = m_wfHistoryTimestamps[rowIndex];
    if (timestampMs <= 0) {
        return QString();
    }
    const QDateTime utc = QDateTime::fromMSecsSinceEpoch(
        timestampMs, QTimeZone::utc());
    return QStringLiteral("-") + utc.toString(QStringLiteral("HH:mm:ssZ"));
}
```

- [ ] **Step 5: Add `#include <QDateTime>` and `<QTimeZone>` if not already present**

Run `rg -n '^#include <QDateTime>|^#include <QTimeZone>' src/gui/SpectrumWidget.cpp` — add either if missing. Both are needed for `pausedTimeLabelForAge`.

- [ ] **Step 6: Build and verify the test still passes (it should — the shim is independent)**

Run:
```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target tst_waterfall_scrollback 2>&1 | tail -10
ctest --test-dir build -R tst_waterfall_scrollback --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add src/gui/SpectrumWidget.cpp tests/tst_waterfall_scrollback.cpp tests/CMakeLists.txt
git commit -S -m "feat(spectrum): add scrollback math helpers + capacity tests (E task 2)

Pure-function helpers ported from AetherSDR SpectrumWidget.cpp:559-590
[@0cd4559] with the 4096-row cap from the unmerged PR #1478
[@2bb3b5c] (unmerged AetherSDR PR #1478). New tst_waterfall_scrollback
locks in the cap behaviour
at three boundary points (30 ms, 293 ms, 500 ms) plus age->row
arithmetic. No behaviour change yet — pushWaterfallRow is unmodified."
```

---

## Task 3: Ring buffer write paths (`ensureWaterfallHistory`, `appendHistoryRow`, `rebuildWaterfallViewport`)

The three functions that allocate, write to, and read from the ring. Still no integration into the live update path — that comes in Task 4.

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp`
- Test: `tests/tst_waterfall_scrollback.cpp` (extend)

- [ ] **Step 1: Add `ensureWaterfallHistory` to `SpectrumWidget.cpp`**

Same section as Task 2's helpers. Add:

```cpp
// From AetherSDR SpectrumWidget.cpp:594-632 [@0cd4559]
void SpectrumWidget::ensureWaterfallHistory()
{
    if (m_waterfall.isNull()) {
        return;
    }

    const QSize desiredSize(m_waterfall.width(), waterfallHistoryCapacityRows());
    if (desiredSize.width() <= 0 || desiredSize.height() <= 0) {
        return;
    }

    if (m_waterfallHistory.size() == desiredSize) {
        return;
    }

    // Preserve rows across width changes (e.g. divider drag, manual window
    // resize) by horizontally scaling the existing history image. Height
    // capacity is fixed via waterfallHistoryCapacityRows() so row indices
    // and timestamps remain valid.
    QImage newHistory;
    if (!m_waterfallHistory.isNull() && m_wfHistoryRowCount > 0
        && m_waterfallHistory.height() == desiredSize.height()) {
        newHistory = m_waterfallHistory.scaled(
            desiredSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    if (newHistory.isNull() || newHistory.size() != desiredSize) {
        newHistory = QImage(desiredSize, QImage::Format_RGB32);
        newHistory.fill(Qt::black);
        m_wfHistoryTimestamps = QVector<qint64>(desiredSize.height(), 0);
        m_wfHistoryWriteRow = 0;
        m_wfHistoryRowCount = 0;
        m_wfHistoryOffsetRows = 0;
        m_wfLive = true;
    }
    m_waterfallHistory = newHistory;
}
```

- [ ] **Step 2: Add `appendHistoryRow`**

Same section. Add:

```cpp
// From AetherSDR SpectrumWidget.cpp:647-668 [@0cd4559]
void SpectrumWidget::appendHistoryRow(const QRgb* rowData, qint64 timestampMs)
{
    ensureWaterfallHistory();
    if (m_waterfallHistory.isNull() || rowData == nullptr) {
        return;
    }

    const int h = m_waterfallHistory.height();
    m_wfHistoryWriteRow = (m_wfHistoryWriteRow - 1 + h) % h;
    auto* row = reinterpret_cast<QRgb*>(
        m_waterfallHistory.bits()
        + m_wfHistoryWriteRow * m_waterfallHistory.bytesPerLine());
    std::memcpy(row, rowData, m_waterfallHistory.width() * sizeof(QRgb));
    if (m_wfHistoryWriteRow >= 0
        && m_wfHistoryWriteRow < m_wfHistoryTimestamps.size()) {
        m_wfHistoryTimestamps[m_wfHistoryWriteRow] = timestampMs;
    }
    if (m_wfHistoryRowCount < h) {
        ++m_wfHistoryRowCount;
    }
    if (!m_wfLive) {
        // Auto-bump while paused so the displayed row stays visually fixed
        // as new live rows arrive underneath.
        m_wfHistoryOffsetRows = std::min(
            m_wfHistoryOffsetRows + 1, maxWaterfallHistoryOffsetRows());
    }
}
```

- [ ] **Step 3: Add `rebuildWaterfallViewport`**

Same section. Add:

```cpp
// From AetherSDR SpectrumWidget.cpp:670-705 [@0cd4559]
void SpectrumWidget::rebuildWaterfallViewport()
{
    if (m_waterfall.isNull()) {
        return;
    }

    m_wfHistoryOffsetRows = std::clamp(
        m_wfHistoryOffsetRows, 0, maxWaterfallHistoryOffsetRows());
    m_waterfall.fill(Qt::black);
    m_wfWriteRow = 0;

    if (m_waterfallHistory.isNull()) {
        update();
        return;
    }

    const int rowWidthBytes = m_waterfall.width() * static_cast<int>(sizeof(QRgb));
    for (int y = 0; y < m_waterfall.height(); ++y) {
        const int rowIndex = historyRowIndexForAge(m_wfHistoryOffsetRows + y);
        if (rowIndex < 0) {
            break;
        }
        const QRgb* src = reinterpret_cast<const QRgb*>(
            m_waterfallHistory.constScanLine(rowIndex));
        auto* dst = reinterpret_cast<QRgb*>(m_waterfall.scanLine(y));
        std::memcpy(dst, src, rowWidthBytes);
    }

    // Force GPU full re-upload — the per-row delta path can't follow a
    // viewport rebuild because m_wfWriteRow no longer indexes the most
    // recent live row. Two sentinels needed: m_wfTexFullUpload routes
    // the next frame through the full-upload branch (matching upstream
    // AetherSDR's `#ifdef AETHER_GPU_SPECTRUM` path which sets
    // `m_wfTexFullUpload = true`); m_wfLastUploadedRow alone leaves the
    // bottom scanline stale because the incremental loop exits before
    // uploading row texH-1.
    m_wfTexFullUpload = true;
    m_wfLastUploadedRow = -1;
    update();
}
```

- [ ] **Step 4: Add `clearWaterfallHistory` (used by `clearDisplay` and the largeShift branch in Task 9)**

Same section. Add:

```cpp
// Sub-epic E — flush ring buffer + force live. Called from clearDisplay()
// and from setFrequencyRange's largeShift branch (NereusSDR divergence;
// see plan §authoring-time #3).
void SpectrumWidget::clearWaterfallHistory()
{
    if (!m_waterfallHistory.isNull()) {
        m_waterfallHistory.fill(Qt::black);
    }
    std::fill(m_wfHistoryTimestamps.begin(), m_wfHistoryTimestamps.end(), 0);
    m_wfHistoryWriteRow = 0;
    m_wfHistoryRowCount = 0;
    m_wfHistoryOffsetRows = 0;
    m_wfLive = true;
    markOverlayDirty();
}
```

- [ ] **Step 5: Add a wrap-around test to `tst_waterfall_scrollback.cpp`**

Append to the test class:

```cpp
    void wrapAround_writePastCapacity_keepsCapRows() {
        // Simulate appending 5000 rows into a 4096-row ring; verify
        // m_wfHistoryRowCount saturates at 4096 and writeRow wraps.
        // We instrument this via a minimal stand-in (not a real
        // SpectrumWidget) because the production appendHistoryRow needs
        // a live QImage and a parent QWidget. The contract is small
        // enough to mirror in the test.
        constexpr int kHeight = 4096;
        int writeRow = 0;
        int rowCount = 0;
        for (int i = 0; i < 5000; ++i) {
            writeRow = (writeRow - 1 + kHeight) % kHeight;
            if (rowCount < kHeight) ++rowCount;
        }
        QCOMPARE(rowCount, 4096);
        // After 5000 writes from initial writeRow=0, the new writeRow is
        // (0 - 5000) % 4096 = -5000 % 4096 = -904 + 4096 = 3192 ... but
        // because we apply (writeRow - 1 + h) % h each iteration, we
        // wrap once per 4096 writes. After 5000 writes that's one full
        // wrap (4096 writes) plus 904 more, landing at:
        //     ((0 - 5000) % 4096 + 4096) % 4096 = (4096 - 904) = 3192
        QCOMPARE(writeRow, 3192);
    }
```

- [ ] **Step 6: Build and run the test**

Run:
```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target tst_waterfall_scrollback NereusSDR 2>&1 | tail -10
ctest --test-dir build -R tst_waterfall_scrollback --output-on-failure
```

Expected: PASS. The full app should also still build (the new functions are unused; the linker keeps them).

- [ ] **Step 7: Commit**

```bash
git add src/gui/SpectrumWidget.cpp tests/tst_waterfall_scrollback.cpp
git commit -S -m "feat(spectrum): add ring buffer write paths (sub-epic E task 3)

ensureWaterfallHistory, appendHistoryRow, rebuildWaterfallViewport,
clearWaterfallHistory ported byte-for-byte from AetherSDR
SpectrumWidget.cpp:594-754 [@0cd4559]. New wrap-around test locks
in cap saturation + LIFO write-head decrement. Not yet integrated
into pushWaterfallRow — task 4 wires this in."
```

---

## Task 4: Wire scrollback into `pushWaterfallRow` and `clearDisplay`

The first task that changes runtime behaviour. After this task, the history ring buffer fills with rows on every push, and `clearDisplay()` (called on disconnect) flushes it. Pause/resume still doesn't work yet — that comes in Task 5 — but the data is being captured.

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp`

- [ ] **Step 1: Locate `pushWaterfallRow`**

`rg -n 'void SpectrumWidget::pushWaterfallRow' src/gui/SpectrumWidget.cpp` — should hit around line 1727. The function ends after the m_wfReverseScroll branch around line 1820 with the row written to `m_waterfall.scanLine(m_wfWriteRow)`.

- [ ] **Step 2: Add the history-write call after the live-row write**

Find the line `QRgb* scanline = reinterpret_cast<QRgb*>(m_waterfall.scanLine(m_wfWriteRow));` and trace down to the end of the for-loop that fills the scanline (around line 1815). Immediately after that for-loop, add:

```cpp
// ── Sub-epic E: mirror the just-written row into the history ring ───
// From AetherSDR SpectrumWidget.cpp:2808-2812 [@0cd4559]
//   adapter: NereusSDR has a single FFT-derived path (no native tile
//   path), so we always use QDateTime::currentMSecsSinceEpoch().
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    appendHistoryRow(scanline, nowMs);
    if (!m_wfLive) {
        // Paused: don't show the new row — auto-bump in appendHistoryRow
        // already shifted offset, just rebuild the viewport.
        rebuildWaterfallViewport();
        markOverlayDirty();
    }
}
```

**Why the live branch isn't here:** the existing code already writes the live row to `m_waterfall.scanLine(m_wfWriteRow)` *before* this insertion point. That IS the live append. We don't need a separate `appendVisibleRow` like upstream — our live-write loop is inline and writes directly to the QImage. The history mirror is a copy of that same scanline.

- [ ] **Step 3: Wire `clearWaterfallHistory()` to RadioModel disconnection**

NereusSDR has no `SpectrumWidget::clearDisplay()` equivalent — there is no single reset function called on disconnect. The disconnect signal lives on `RadioModel::connectionStateChanged()` + `RadioModel::isConnected()` and is wired through to widgets in `MainWindow`. Sub-epic E adds a clean handler there.

First, ensure `clearWaterfallHistory()` is callable from `MainWindow`. The Task 1 declaration placed it in the private-method block; move it to the **public slots** section of `SpectrumWidget.h` (alongside other public actions). The function has zero parameters, no return — natural fit for a Qt slot signature, and it lets `MainWindow` invoke it via the standard `connect(... &SpectrumWidget::clearWaterfallHistory)` pattern.

In `src/gui/SpectrumWidget.h`, find the `private:` block where Task 1 placed `void clearWaterfallHistory();`. Move it into the existing `public slots:` section (or create one if none exists alongside other public methods). Keep the cite/comment context.

- [ ] **Step 4: Connect RadioModel disconnect signal to clearWaterfallHistory**

In `src/gui/MainWindow.cpp`, find where `m_spectrumWidget` is wired to `m_radioModel` (around line 732 at `m_radioModel->setSpectrumWidget(m_spectrumWidget);`). Add immediately after that block:

```cpp
// Sub-epic E: flush the rewind ring buffer when the radio disconnects so
// a new session starts with a clean history. AetherSDR's clearDisplay()
// did this implicitly; NereusSDR has no equivalent single-call reset, so
// we plumb the connection-state signal through here. See plan §authoring-time.
connect(m_radioModel, &RadioModel::connectionStateChanged, m_spectrumWidget,
        [this]() {
    if (!m_radioModel->isConnected() && m_spectrumWidget) {
        m_spectrumWidget->clearWaterfallHistory();
    }
});
```

The lambda checks `isConnected()` rather than a state-comparison because `connectionStateChanged()` fires for both Connected→Disconnected AND Connecting→Disconnected (failed-connect) AND Connected→Error. All three should flush history. Only Disconnected→Connecting should NOT flush — but that transition happens BEFORE any rows would have arrived, so the ring is already empty.

- [ ] **Step 5: Build and smoke-test**

Run:
```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target NereusSDR 2>&1 | tail -10
killall NereusSDR 2>/dev/null; ./build/NereusSDR &
```

Live-test: connect to the radio, watch the waterfall scroll for ~30 s, then look at memory. Expected: `m_waterfallHistory` is now allocated (RGB32, height = 4 096 at 30 ms / 20 min, width = waterfall width). RAM growth from sub-epic-D baseline ≈ `width × 4 × 4096` bytes per panadapter — at 2 000 px wide that's ~33 MB.

Verify visually that the waterfall still scrolls live, looks unchanged from before, no flicker. The history is recording silently in the background.

- [ ] **Step 6: Commit**

```bash
git add src/gui/SpectrumWidget.cpp
git commit -S -m "feat(spectrum): wire scrollback into push + clear paths (E task 4)

Every pushWaterfallRow now mirrors the live row into m_waterfallHistory
with a wall-clock timestamp; clearDisplay flushes the ring on
disconnect. Live waterfall behaviour is unchanged — no pause UI yet
(task 5+). RAM cost: ~33 MB per 2000px-wide panadapter at default
30 ms × 4096-row cap."
```

---

## Task 5: `setWaterfallLive` + width reflection

Adds the public toggle that LIVE-button click + drag-to-scrub will call. Also extends `effectiveStripW()` to widen the right margin when paused (matching upstream's 36 → 72 px transition).

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp`
- Modify: `src/gui/SpectrumWidget.h`

- [ ] **Step 1: Add `setWaterfallLive` and `waterfallStripWidth` to `SpectrumWidget.cpp`**

Same section as the math helpers from Task 2. Add:

```cpp
// From AetherSDR SpectrumWidget.cpp:705-718 [@0cd4559]
void SpectrumWidget::setWaterfallLive(bool live)
{
    if (m_wfLive == live) {
        return;
    }
    if (live) {
        m_wfHistoryOffsetRows = 0;
    }
    m_wfLive = live;
    rebuildWaterfallViewport();
    markOverlayDirty();
}

int SpectrumWidget::waterfallStripWidth() const
{
    // Live: width matches the dBm strip column for visual continuity.
    // Paused: widens to fit absolute UTC labels ("-HH:mm:ssZ").
    // From AetherSDR SpectrumWidget.cpp:716-719 [@0cd4559]
    constexpr int kPausedStripW = 72;
    return m_wfLive ? kDbmStripW : kPausedStripW;
}
```

(Replace `kDbmStripW` with whatever NereusSDR's existing constant is named — the grep from preflight #3 will name it.)

- [ ] **Step 2: Extend `effectiveStripW` to include the timescale strip**

The current implementation is at line 958:
```cpp
int SpectrumWidget::effectiveStripW() const
{
    return m_dbmScaleVisible ? kDbmStripW : 0;
}
```

The paused timescale strip widens past the dBm strip. Replace with:

```cpp
int SpectrumWidget::effectiveStripW() const
{
    // dBm strip + paused-mode timescale-strip extension.
    // The strip is always present in the *waterfall* row (where the
    // time scale is painted); the dBm strip is in the *spectrum* row.
    // They occupy the same right-edge column.
    const int spectrumStripW = m_dbmScaleVisible ? kDbmStripW : 0;
    const int waterfallStripW = waterfallStripWidth();
    return std::max(spectrumStripW, waterfallStripW);
}
```

**Note:** this changes the *spectrum* rendering to also reserve the wider column when paused, which means the spectrum trace gets slightly narrower while paused. That's correct — the right-edge column extends top-to-bottom, and the dBm scale strip continues to occupy it. Matches upstream behaviour.

- [ ] **Step 3: Build and smoke-test**

Run:
```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target NereusSDR 2>&1 | tail -10
killall NereusSDR 2>/dev/null; ./build/NereusSDR &
```

Live-test: connect to radio, force-call `setWaterfallLive(false)` via a temporary debug shortcut (or skip live-test for this task — pause UI doesn't exist yet, but the function is callable from later tasks). The build should succeed; behaviour is unchanged because `m_wfLive` defaults to `true` and nothing yet flips it.

- [ ] **Step 4: Commit**

```bash
git add src/gui/SpectrumWidget.cpp
git commit -S -m "feat(spectrum): add setWaterfallLive + paused-strip width (E task 5)

setWaterfallLive(bool) clamps offset to 0 on live-resume, calls
rebuildWaterfallViewport. waterfallStripWidth returns 36px live
or 72px paused (matches AetherSDR [@0cd4559]). effectiveStripW
extended to honour the wider paused strip across both spectrum
and waterfall regions."
```

---

## Task 6: Geometry helpers — `waterfallTimeScaleRect` + `waterfallLiveButtonRect`

Two pure-geometry functions that compute the rects for the time-scale strip and the LIVE button. Used by both paint code (Task 8) and mouse hit-testing (Task 7).

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp`

- [ ] **Step 1: Add `waterfallTimeScaleRect`**

Same section as the helpers. Add:

```cpp
// From AetherSDR SpectrumWidget.cpp:720-725 [@0cd4559]
QRect SpectrumWidget::waterfallTimeScaleRect(const QRect& wfRect) const
{
    const int stripWidth = waterfallStripWidth();
    const int stripX = wfRect.right() - stripWidth + 1;
    return QRect(stripX, wfRect.top(), stripWidth, wfRect.height());
}
```

- [ ] **Step 2: Add `waterfallLiveButtonRect`**

```cpp
// From AetherSDR SpectrumWidget.cpp:728-735 [@0cd4559]
//   adapter: NereusSDR uses kFreqScaleH = 28 (vs AetherSDR's 20),
//   button Y inset adjusted accordingly.
QRect SpectrumWidget::waterfallLiveButtonRect(const QRect& wfRect) const
{
    const QRect strip = waterfallTimeScaleRect(wfRect);
    constexpr int kLiveButtonW = 32;
    constexpr int kLiveButtonH = 16;
    const int buttonX = strip.right() - kLiveButtonW - 2;
    const int buttonY = wfRect.top() - kFreqScaleH + 2;  // sits in the freq-scale row
    return QRect(buttonX, buttonY, kLiveButtonW, kLiveButtonH);
}
```

- [ ] **Step 3: Build (no behavior change yet)**

Run:
```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target NereusSDR 2>&1 | tail -10
```

Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add src/gui/SpectrumWidget.cpp
git commit -S -m "feat(spectrum): add timescale + LIVE button rects (E task 6)

Pure-geometry functions ported from AetherSDR SpectrumWidget.cpp
720-735 [@0cd4559] with the freq-scale-row Y inset adapted from
AetherSDR's kFreqScaleH=20 to NereusSDR's kFreqScaleH=28."
```

---

## Task 7: Mouse interactions — press, move, release, hover cursor

Wires the gesture handling for the time-scale strip + LIVE button. After this task, drag-up scrubs back, click on LIVE resumes, and the cursor visibly changes on hover.

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp`

- [ ] **Step 1: Locate the existing `mousePressEvent`**

`rg -n 'void SpectrumWidget::mousePressEvent' src/gui/SpectrumWidget.cpp` — find the function signature. The existing function handles divider-drag, dBm-strip click, slice click, and pan-drag (this is documented in the SpectrumWidget header).

- [ ] **Step 2: Insert the LIVE button + time-scale press handlers**

The new branches go *before* the existing waterfall-area branch (which currently handles pan drag). Find the comment `// Left-click in waterfall area → start pan drag` and insert immediately above it:

```cpp
// ── Sub-epic E: time-scale strip + LIVE button ────────────────────────
// From AetherSDR SpectrumWidget.cpp:1655-1693 [@0cd4559]
const int wfY = scaleY + kFreqScaleH;
const QRect wfRect(0, wfY, width(), height() - wfY);

// LIVE button click (sits in the freq-scale row above the waterfall)
if (waterfallLiveButtonRect(wfRect).contains(ev->position().toPoint())
    && ev->button() == Qt::LeftButton) {
    setWaterfallLive(true);
    ev->accept();
    return;
}

// Time-scale strip drag start (right edge of waterfall)
if (y >= wfY && ev->button() == Qt::LeftButton) {
    const QRect timeScaleRect = waterfallTimeScaleRect(wfRect);
    const QPoint pos = ev->position().toPoint();
    if (timeScaleRect.contains(pos)) {
        m_draggingTimeScale = true;
        m_timeScaleDragStartY = y;
        m_timeScaleDragStartOffsetRows = m_wfHistoryOffsetRows;
        setCursor(Qt::SizeVerCursor);
        ev->accept();
        return;
    }
}
```

(`scaleY` is the freq-scale-row top; check the existing code for the local variable's exact name — likely `scaleY = specH + kDividerH`. NereusSDR's variable may be named differently; mirror what divider-drag uses.)

- [ ] **Step 3: Insert the time-scale drag handler in `mouseMoveEvent`**

`rg -n 'void SpectrumWidget::mouseMoveEvent' src/gui/SpectrumWidget.cpp`. Find the existing `m_draggingDbm` branch (around line 2104). Add before it:

```cpp
// ── Sub-epic E: time-scale drag = scrub through history ───────────────
// From AetherSDR SpectrumWidget.cpp:2122-2145 [@0cd4559]
if (m_draggingTimeScale) {
    const int wfY = specH + kDividerH + kFreqScaleH;
    const QRect wfRect(0, wfY, width(), height() - wfY);
    const QRect timeScaleRect = waterfallTimeScaleRect(wfRect);
    const int dragHeight = std::max(1, timeScaleRect.height());
    const int maxOffset = maxWaterfallHistoryOffsetRows();
    const int dy = m_timeScaleDragStartY - y;  // pull up = scroll back
    const int deltaRows = (maxOffset > 0)
        ? static_cast<int>(std::round(
            (static_cast<double>(dy) / dragHeight) * maxOffset))
        : 0;
    const int newOffset = std::clamp(
        m_timeScaleDragStartOffsetRows + deltaRows, 0, maxOffset);

    if (newOffset != m_wfHistoryOffsetRows) {
        m_wfHistoryOffsetRows = newOffset;
        if (newOffset > 0) {
            m_wfLive = false;  // entering paused state
        }
        rebuildWaterfallViewport();
        markOverlayDirty();
    }

    setCursor(Qt::SizeVerCursor);
    ev->accept();
    return;
}
```

(`specH`, `kDividerH`, `kFreqScaleH` — match NereusSDR's existing variable names from the divider-drag block.)

- [ ] **Step 4: Add the hover-cursor logic in `mouseMoveEvent`**

Find the existing cursor-update block (where current code sets `Qt::CrossCursor` on the waterfall area). Add a check for the LIVE button + time-scale strip *before* the default crosshair assignment:

```cpp
// ── Sub-epic E: hover cursors for LIVE button + time scale ────────────
// From AetherSDR SpectrumWidget.cpp:2200-2225 [@0cd4559]
const int wfY = specH + kDividerH + kFreqScaleH;
if (y >= specH + kDividerH && y < wfY) {
    // In the freq-scale row — LIVE button overlaps here.
    const QRect wfRect(0, wfY, width(), height() - wfY);
    if (waterfallLiveButtonRect(wfRect).contains(ev->position().toPoint())) {
        setCursor(Qt::PointingHandCursor);
        return;
    }
}
if (y >= wfY) {
    const QRect wfRect(0, wfY, width(), height() - wfY);
    const QRect timeScaleRect = waterfallTimeScaleRect(wfRect);
    if (timeScaleRect.contains(ev->position().toPoint())) {
        setCursor(Qt::SizeVerCursor);
        return;
    }
    // fall through to existing crosshair assignment
}
```

- [ ] **Step 5: Insert the time-scale drag-end branch in `mouseReleaseEvent`**

`rg -n 'void SpectrumWidget::mouseReleaseEvent' src/gui/SpectrumWidget.cpp`. Find the existing `m_draggingDbm` reset branch. Add a parallel branch for `m_draggingTimeScale`:

```cpp
// ── Sub-epic E: time-scale drag end ───────────────────────────────────
// From AetherSDR SpectrumWidget.cpp:2382-2387 [@0cd4559]
//   note: drag release does NOT auto-resume to live — m_wfLive is only
//   flipped true by the LIVE button. Drag-to-zero would auto-bump back
//   on the next row otherwise. This is deliberate; see plan
//   §authoring-time decisions discussion.
if (m_draggingTimeScale) {
    m_draggingTimeScale = false;
    setCursor(Qt::CrossCursor);
    ev->accept();
    return;
}
```

- [ ] **Step 6: Build and live-test the gesture**

Run:
```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target NereusSDR 2>&1 | tail -10
killall NereusSDR 2>/dev/null; ./build/NereusSDR &
```

Live-test: connect to radio, watch waterfall fill for ~30 s, then drag UP from anywhere on the right edge of the waterfall area. Expected: the waterfall content rewinds; cursor changes to vertical-resize on hover; releasing the drag holds the position. Drag back to bottom — content continues rewinding (no auto-resume on drag-end). The widget paint will look slightly broken (the strip itself isn't rendered yet — that's Task 8) but the data should rewind visibly.

- [ ] **Step 7: Commit**

```bash
git add src/gui/SpectrumWidget.cpp
git commit -S -m "feat(spectrum): wire scrub gesture + LIVE click handlers (E task 7)

mousePressEvent: LIVE button click -> setWaterfallLive(true);
right-edge drag start -> sets m_draggingTimeScale.
mouseMoveEvent: time-scale drag updates m_wfHistoryOffsetRows;
hover sets PointingHandCursor on LIVE / SizeVerCursor on strip.
mouseReleaseEvent: clears m_draggingTimeScale.

Drag does not auto-resume at offset=0 — only the LIVE button
flips m_wfLive back to true. Faithful to upstream
[@0cd4559] gesture semantics."
```

---

## Task 8: Paint the time scale strip + LIVE button (dual path)

Adds `drawTimeScale` and integrates it into both the QPainter render path and the GPU overlay-static-texture path — same dual-path pattern as `drawDbmScale` (sub-epic A) and `drawBandPlan` (sub-epic D).

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp`

- [ ] **Step 1: Add `drawTimeScale` to `SpectrumWidget.cpp`**

After `drawBandPlan` (or `drawDbmScale` if `drawBandPlan` is conditionally compiled). Add:

```cpp
// ─── Time scale + LIVE button (sub-epic E) ─────────────────────────────
// From AetherSDR SpectrumWidget.cpp:4929-4994 [@0cd4559]
//   adapter: NereusSDR computes msPerRow directly from m_wfUpdatePeriodMs
//   instead of AetherSDR's calibrated m_wfMsPerRow (we have no radio
//   tile clock to calibrate against).

void SpectrumWidget::drawTimeScale(QPainter& p, const QRect& wfRect)
{
    const QRect strip = waterfallTimeScaleRect(wfRect);
    const int stripX = strip.x();

    // Semi-opaque background so spectrum content underneath dims.
    p.fillRect(strip, QColor(0x0a, 0x0a, 0x18, 220));

    // Left border line — separates the strip from waterfall content.
    p.setPen(QColor(0x30, 0x40, 0x50));
    p.drawLine(stripX, wfRect.top(), stripX, wfRect.bottom());

    // LIVE button — grey when live, bright red when paused.
    const QRect liveRect = waterfallLiveButtonRect(wfRect);
    p.setPen(QColor(0x40, 0x50, 0x60));
    p.setBrush(m_wfLive ? QColor(0x45, 0x45, 0x45)
                        : QColor(0xc0, 0x20, 0x20));  // bright red when paused
    p.drawRoundedRect(liveRect, 3, 3);

    QFont liveFont = p.font();
    liveFont.setPointSize(7);
    liveFont.setBold(true);
    p.setFont(liveFont);
    p.setPen(m_wfLive ? QColor(0xb0, 0xb0, 0xb0) : Qt::white);
    p.drawText(liveRect, Qt::AlignCenter, QStringLiteral("LIVE"));

    // Tick labels along the strip.
    const float msPerRow = std::max(1, m_wfUpdatePeriodMs);
    const QRect labelRect = strip.adjusted(0, 4, 0, 0);
    const float totalSec = labelRect.height() * msPerRow / 1000.0f;
    if (totalSec <= 0) {
        return;
    }

    QFont f = p.font();
    f.setPointSize(7);
    f.setBold(false);
    p.setFont(f);
    const QFontMetrics fm(f);

    constexpr float kStepSec = 5.0f;
    for (float sec = 0; sec <= totalSec; sec += kStepSec) {
        const float frac = sec / totalSec;
        const int yy = labelRect.top()
                     + static_cast<int>(frac * labelRect.height());
        if (yy > wfRect.bottom() - 5) {
            continue;
        }

        // Tick mark
        p.setPen(QColor(0x50, 0x70, 0x80));
        p.drawLine(stripX, yy, stripX + 4, yy);

        // Label: elapsed seconds when live, absolute UTC when paused.
        const QString label = m_wfLive
            ? QStringLiteral("%1s").arg(static_cast<int>(sec))
            : pausedTimeLabelForAge(
                m_wfHistoryOffsetRows
                + static_cast<int>(std::round(sec * 1000.0f / msPerRow)));

        p.setPen(QColor(0x80, 0xa0, 0xb0));
        if (m_wfLive) {
            p.drawText(stripX + 6, yy + fm.ascent() / 2, label);
        } else {
            const QRect textRect(stripX + 6, yy - fm.height() / 2,
                                 strip.width() - 10, fm.height());
            p.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, label);
        }
    }
}
```

- [ ] **Step 2: Wire `drawTimeScale` into the QPainter render path**

`rg -n 'drawDbmScale\(p|drawBandPlan\(p' src/gui/SpectrumWidget.cpp`. Find where `drawDbmScale(p, ...)` and `drawBandPlan(p, specRect)` are called from the QPainter overlay block (around line 1155-1161). Add a call to `drawTimeScale` immediately after — but **only on the waterfall rect**, not the spectrum rect.

**IMPORTANT — wfRect width contract:** NereusSDR's existing `wfRect` at line 1141 is clipped via `w - effectiveStripW()` to leave room for the dBm strip. AetherSDR's time-scale helpers (`waterfallTimeScaleRect` / `waterfallLiveButtonRect`) expect a wfRect that extends to the FULL widget width, because the time-scale strip lives in the same right-edge column as the dBm strip. Construct a **separate full-width wfRect** for the `drawTimeScale` call rather than reusing the clipped one:

```cpp
// Sub-epic E: time-scale + LIVE button on the right edge of the
// waterfall area (always painted; widens automatically when paused).
// Use a full-width wfRect (not the clipped `wfRect` at line 1141) so the
// strip lands in the same right-edge column as the dBm scale strip.
const QRect wfRectFull(0, wfRect.top(), width(), wfRect.height());
drawTimeScale(p, wfRectFull);
```

- [ ] **Step 3: Wire `drawTimeScale` into the GPU overlay-static-texture path**

`rg -n 'overlay.*static|m_overlayStatic|paintOverlayToImage|drawDbmScale\b' src/gui/SpectrumWidget.cpp` — find where the GPU path renders chrome to its overlay image. Sub-epic A's `drawDbmScale` is the model: same call pattern, into the same overlay-image painter. Add a `drawTimeScale(...)` call after the `drawDbmScale` and `drawBandPlan` calls.

**SAME wfRect-width contract as Step 2:** the time-scale strip's geometry helpers expect a wfRect spanning the full widget width. If the GPU path's overlay-image-painter callsite has a `wfRect` local that's clipped, construct a separate full-width version (`QRect(0, existingWfRect.top(), width(), existingWfRect.height())`) for `drawTimeScale`.

If the overlay-static-texture path is unclear, run:
```bash
git log --oneline --all -- src/gui/SpectrumWidget.cpp | head -20
git show <sub-epic-D-merge-commit> -- src/gui/SpectrumWidget.cpp | grep -A5 -B2 'drawBandPlan\|overlay'
```
to see exactly where sub-epic D plumbed `drawBandPlan` into the overlay texture, and mirror that.

- [ ] **Step 4: Build and live-test**

Run:
```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target NereusSDR 2>&1 | tail -10
killall NereusSDR 2>/dev/null; ./build/NereusSDR &
```

Live-test:
1. Connect to radio. Time-scale strip appears on the right edge of the waterfall, narrow (36 px), with `5s / 10s / 15s ...` tick labels and a grey "LIVE" button at the top.
2. Drag up on the strip. Strip widens to ~72 px; tick labels switch to `-HH:mm:ssZ` UTC absolute timestamps; LIVE button turns **bright red**.
3. Click the red LIVE button. Snaps back to live; strip narrows; labels return to elapsed seconds; LIVE button turns grey.

Take a screenshot of the paused state for the PR body:
```bash
sleep 2 && screencapture -x /tmp/nereussdr-rewind-paused.png
```

- [ ] **Step 5: Commit**

```bash
git add src/gui/SpectrumWidget.cpp
git commit -S -m "feat(spectrum): paint timescale strip + LIVE button (E task 8)

drawTimeScale ported from AetherSDR SpectrumWidget.cpp:4929-4994
[@0cd4559] with msPerRow source adapted from calibrated
m_wfMsPerRow to NereusSDR's m_wfUpdatePeriodMs (no radio tile
clock). Wired into both QPainter and GPU overlay-static paths,
mirroring the dual-path pattern from sub-epic A (dBm strip) and
sub-epic D (bandplan).

Tick labels: '5s/10s/15s' elapsed when live; '-HH:mm:ssZ' UTC
when paused. LIVE button: grey/grey-text when live, bright red
(#c02020)/white-text when paused."
```

---

## Task 9: Pan/zoom reproject + large-shift clear (Q4 divergence)

Implements `reprojectWaterfall` (matching upstream behaviour for small pans + zooms) AND adds the NereusSDR-specific `clearWaterfallHistory()` call on `largeShift` per Q4. This is the only task in the plan that introduces divergence from upstream `main`.

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp`

- [ ] **Step 1: Add `reprojectWaterfall`**

Same section as the helpers. Add:

```cpp
// From AetherSDR SpectrumWidget.cpp:951-1000 [@0cd4559]
void SpectrumWidget::reprojectWaterfall(double oldCenterHz, double oldBandwidthHz,
                                        double newCenterHz, double newBandwidthHz)
{
    if (oldBandwidthHz <= 0.0 || newBandwidthHz <= 0.0) {
        return;
    }

    const double oldStartHz = oldCenterHz - oldBandwidthHz / 2.0;
    const double oldEndHz   = oldCenterHz + oldBandwidthHz / 2.0;
    const double newStartHz = newCenterHz - newBandwidthHz / 2.0;
    const double newEndHz   = newCenterHz + newBandwidthHz / 2.0;
    const double overlapStartHz = std::max(oldStartHz, newStartHz);
    const double overlapEndHz   = std::min(oldEndHz, newEndHz);

    auto reprojectImage = [&](QImage& image) {
        if (image.isNull()) {
            return;
        }
        const int imageWidth = image.width();
        const int imageHeight = image.height();
        if (imageWidth <= 0 || imageHeight <= 0) {
            return;
        }

        QImage reprojected(imageWidth, imageHeight, QImage::Format_RGB32);
        reprojected.fill(Qt::black);

        if (overlapEndHz > overlapStartHz) {
            const double srcLeft  = (overlapStartHz - oldStartHz) / oldBandwidthHz * imageWidth;
            const double srcRight = (overlapEndHz   - oldStartHz) / oldBandwidthHz * imageWidth;
            const double dstLeft  = (overlapStartHz - newStartHz) / newBandwidthHz * imageWidth;
            const double dstRight = (overlapEndHz   - newStartHz) / newBandwidthHz * imageWidth;

            if (srcRight > srcLeft && dstRight > dstLeft) {
                QPainter painter(&reprojected);
                painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
                painter.drawImage(QRectF(dstLeft, 0.0, dstRight - dstLeft, imageHeight),
                                  image,
                                  QRectF(srcLeft, 0.0, srcRight - srcLeft, imageHeight));
            }
        }
        image = std::move(reprojected);
    };

    reprojectImage(m_waterfall);
    reprojectImage(m_waterfallHistory);
    m_wfLastUploadedRow = -1;  // force GPU full re-upload
}
```

(NereusSDR uses Hz, not MHz, for centerHz/bandwidthHz members per existing code. Verify with the preflight grep — adjust if the units differ.)

- [ ] **Step 2: Locate the equivalent of upstream's `setFrequencyRange`**

NereusSDR splits this into `setCenterFrequency(qint64)` and `setBandwidth(int)` (or similar — confirm with preflight #5). Find the function(s) that update `m_centerHz` / `m_bandwidthHz` and emit invalidations. There's likely a single internal helper that both setters funnel through.

- [ ] **Step 3: Add `largeShift` detection**

Mirror upstream's logic. In whichever function commits the new center/bandwidth, before the assignment to `m_centerHz` / `m_bandwidthHz`, add:

```cpp
// ── Sub-epic E: detect large shifts (band jumps) for history-clear ─────
// From AetherSDR SpectrumWidget.cpp:1042-1062 [@0cd4559]
//   adapter: NereusSDR uses Hz throughout; threshold expressed as
//   fraction of half-bandwidth, same as upstream.
const double oldCenterHz    = static_cast<double>(m_centerHz);
const double oldBandwidthHz = static_cast<double>(m_bandwidthHz);
const double halfBwHz       = newBandwidthHz / 2.0;
const bool   bwChanged      = (newBandwidthHz != oldBandwidthHz);
const bool   largeShift     = bwChanged
    || (halfBwHz > 0.0 && std::abs(newCenterHz - oldCenterHz) > halfBwHz * 0.25);

if (largeShift && oldBandwidthHz > 0.0 && newBandwidthHz > 0.0) {
    // Reproject the still-live image; the history will be cleared next.
    // From AetherSDR SpectrumWidget.cpp:1051 [@0cd4559]
    reprojectWaterfall(oldCenterHz, oldBandwidthHz, newCenterHz, newBandwidthHz);

    // ── NereusSDR divergence: clear history on largeShift to keep the
    //    rewind window coherent with the current band. See
    //    plan §authoring-time #3.
    clearWaterfallHistory();
} else if (oldBandwidthHz > 0.0 && newBandwidthHz > 0.0) {
    // Small pan/zoom: reproject only — history survives.
    // From AetherSDR SpectrumWidget.cpp:1093 [@0cd4559]
    reprojectWaterfall(oldCenterHz, oldBandwidthHz, newCenterHz, newBandwidthHz);
}
```

(Adjust the variable names — `newCenterHz` / `newBandwidthHz` should match the function's actual parameters.)

- [ ] **Step 4: Build and live-test**

Run:
```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target NereusSDR 2>&1 | tail -10
killall NereusSDR 2>/dev/null; ./build/NereusSDR &
```

Live-test:
1. Connect to radio. Watch waterfall fill for ~30 s. Drag UP on the time-scale strip to pause and rewind ~10 seconds. Confirm the LIVE button is red.
2. **Small pan**: click-and-drag the freq scale a small amount (within 25 % of half-bandwidth). Expected: history is reprojected horizontally to fit the new center; LIVE button stays red; you can still rewind through the recent rows but they're slightly horizontally squashed.
3. **Large pan**: click a different band button (or pan freq by more than the 25 % threshold). Expected: history clears; LIVE button turns grey; waterfall starts fresh on the new band. You cannot rewind through this band switch.

- [ ] **Step 5: Commit**

```bash
git add src/gui/SpectrumWidget.cpp
git commit -S -m "feat(spectrum): pan/zoom reproject + largeShift clear (E task 9)

reprojectWaterfall ported from AetherSDR SpectrumWidget.cpp:951-1000
[@0cd4559] — horizontal stretch of m_waterfall + m_waterfallHistory
applied on every center/bandwidth change.

Adds NereusSDR-side divergence: on largeShift (>25% of half-bw or
bw change), clearWaterfallHistory + force-live keeps the rewind
window coherent with the current band. Cost: cannot rewind through
band changes. See plan §authoring-time #3."
```

---

## Task 10: Resize debounce timer (port from unmerged PR)

Wires `m_historyResizeTimer` so widget resizes, divider drags, and update-period changes don't repeatedly thrash `ensureWaterfallHistory()` mid-drag.

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp`

- [ ] **Step 1: Construct the timer in the SpectrumWidget constructor**

`rg -n 'SpectrumWidget::SpectrumWidget' src/gui/SpectrumWidget.cpp` — find the ctor. After the existing timer setup (e.g. `m_tuneGuideTimer` if present), add:

```cpp
// ── Sub-epic E: debounce timer for waterfall history re-allocation ────
// From AetherSDR SpectrumWidget.cpp:158-168 [@2bb3b5c]
// (debounce timer added by unmerged AetherSDR PR #1478 — see plan §authoring-time #2)
m_historyResizeTimer = new QTimer(this);
m_historyResizeTimer->setSingleShot(true);
m_historyResizeTimer->setInterval(250);
connect(m_historyResizeTimer, &QTimer::timeout, this, [this]() {
    ensureWaterfallHistory();
    if (m_wfHistoryRowCount > 0) {
        rebuildWaterfallViewport();
    }
});
```

- [ ] **Step 2: Add `m_historyResizeTimer->start()` to `resizeEvent`'s m_waterfall-reallocation branch**

`rg -n 'm_waterfall\s*=\s*QImage' src/gui/SpectrumWidget.cpp` — find the m_waterfall reallocation in `resizeEvent` (around line 1188). Add immediately after the `m_waterfall = QImage(...)` line + `m_wfTexFullUpload = true` + `markOverlayDirty()` cluster, inside the same `if (wfW > 0 && ...)` branch:

```cpp
// Sub-epic E: schedule history-image rebuild so the ring buffer's QImage
// tracks m_waterfall's new dimensions. Debounced 250 ms so rapid resize
// events don't thrash. Matches the intent of unmerged AetherSDR PR #1478
// [@2bb3b5c] — see plan §authoring-time #2.
if (m_historyResizeTimer) {
    m_historyResizeTimer->start();
}
```

Note: NereusSDR's divider-drag block doesn't currently reallocate `m_waterfall` — that's a pre-existing divergence from upstream. The timer wiring there is deferred to a follow-up.

- [ ] **Step 3: Replace direct call in divider-drag mouseMove**

Same replacement in the `m_draggingDivider` branch of `mouseMoveEvent`.

- [ ] **Step 4: Wire the debounce into `setWfUpdatePeriodMs`**

Find `setWfUpdatePeriodMs` (around line 808). Currently it just updates `m_wfUpdatePeriodMs` and persists. Add a debounced history rebuild at the end:

```cpp
void SpectrumWidget::setWfUpdatePeriodMs(int ms)
{
    ms = qBound(10, ms, 500);
    if (m_wfUpdatePeriodMs == ms) { return; }
    m_wfUpdatePeriodMs = ms;
    scheduleSettingsSave();

    // Sub-epic E: capacity may have changed — debounce history rebuild
    // so slider drag doesn't trash history mid-drag.
    if (m_historyResizeTimer) {
        m_historyResizeTimer->start();
    }
}
```

- [ ] **Step 5: Add `setWaterfallHistoryMs` (depth setter)**

Same pattern. Add to the public setter section:

```cpp
// Sub-epic E: depth setter, called from Setup → Display dropdown.
void SpectrumWidget::setWaterfallHistoryMs(qint64 ms)
{
    ms = qBound(static_cast<qint64>(60 * 1000),     // 60 s minimum
                ms,
                static_cast<qint64>(20 * 60 * 1000)); // 20 min maximum
    if (m_waterfallHistoryMs == ms) { return; }
    m_waterfallHistoryMs = ms;

    auto& s = AppSettings::instance();
    s.setValue(settingsKey(QStringLiteral("DisplayWaterfallHistoryMs"), m_panIndex),
               QString::number(m_waterfallHistoryMs));
    s.save();

    if (m_historyResizeTimer) {
        m_historyResizeTimer->start();
    }
}
```

- [ ] **Step 6: Load the depth from AppSettings during `loadSettings()`**

`rg -n 'm_wfUpdatePeriodMs\s*=\s*readInt' src/gui/SpectrumWidget.cpp` — find the load location (around line 402). Add immediately after:

```cpp
m_waterfallHistoryMs = readQint64(
    QStringLiteral("DisplayWaterfallHistoryMs"),
    static_cast<qint64>(kDefaultWaterfallHistoryMs));
```

If a `readQint64` helper doesn't exist locally, use the same pattern as the other reads (likely `s.value(settingsKey(...), QString::number(default)).toLongLong()`).

- [ ] **Step 7: Save the depth in `saveSettings()`**

`rg -n 'writeInt.*DisplayWfUpdatePeriodMs' src/gui/SpectrumWidget.cpp` — find the persist location (around line 499). Add immediately after:

```cpp
s.setValue(settingsKey(QStringLiteral("DisplayWaterfallHistoryMs"), m_panIndex),
           QString::number(m_waterfallHistoryMs));
```

- [ ] **Step 8: Build and verify**

Run:
```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target NereusSDR 2>&1 | tail -10
killall NereusSDR 2>/dev/null; ./build/NereusSDR &
```

Live-test: drag the divider quickly between spectrum and waterfall splits. Expected: no visible mid-drag flicker, no allocation lag, no segfault. (Without the debounce, fast drag triggers many `ensureWaterfallHistory()` calls; with the debounce, only the final position triggers a rebuild ~250 ms after release.)

Drag the update-period slider quickly across the full range. Expected: no per-step blanking; history is preserved within the common range and only rebuilt 250 ms after release.

- [ ] **Step 9: Commit**

```bash
git add src/gui/SpectrumWidget.cpp
git commit -S -m "feat(spectrum): debounce history resize + period change (E task 10)

m_historyResizeTimer (250 ms single-shot) ported from AetherSDR
[@2bb3b5c] (unmerged AetherSDR PR #1478). resizeEvent, divider-drag mouseMove,
setWfUpdatePeriodMs, setWaterfallHistoryMs all schedule via the
timer instead of calling ensureWaterfallHistory directly.

Adds setWaterfallHistoryMs setter + AppSettings round-trip for
DisplayWaterfallHistoryMs (default 20 min, range 60s..20min).
Persistence is per-SpectrumWidget (panIndex-scoped, like
DisplayDbmScaleVisible)."
```

---

## Task 11: Setup → Display depth dropdown + persistence

Adds the user-facing depth selector to `WaterfallDefaultsPage` with an effective-depth hint string.

**Files:**
- Modify: `src/gui/setup/DisplaySetupPages.h`
- Modify: `src/gui/setup/DisplaySetupPages.cpp`

- [ ] **Step 1: Declare the new members in `DisplaySetupPages.h`**

Find `class WaterfallDefaultsPage` (line 123). In the private member section (alongside `m_updatePeriodSlider`), add:

```cpp
QComboBox* m_historyDepthCombo{nullptr};
QLabel*    m_effectiveDepthLabel{nullptr};
```

- [ ] **Step 2: Build the dropdown row in `WaterfallDefaultsPage::buildUI`**

Find the existing `m_updatePeriodSlider` row (around line 700-712). Add a new group box for "History" *after* the levels group, *before* the timestamp group:

```cpp
// ── Sub-epic E: rewind / history depth ────────────────────────────────
auto* histGroup = new QGroupBox(QStringLiteral("Rewind history"), this);
auto* histForm  = new QFormLayout(histGroup);
histForm->setSpacing(6);

m_historyDepthCombo = new QComboBox(histGroup);
m_historyDepthCombo->addItem(QStringLiteral("60 seconds"),      60LL * 1000);
m_historyDepthCombo->addItem(QStringLiteral("5 minutes"),  5LL * 60 * 1000);
m_historyDepthCombo->addItem(QStringLiteral("15 minutes"), 15LL * 60 * 1000);
m_historyDepthCombo->addItem(QStringLiteral("20 minutes"), 20LL * 60 * 1000);
m_historyDepthCombo->setToolTip(QStringLiteral(
    "Maximum amount of waterfall history kept for rewind. "
    "Effective rewind is capped at 4 096 rows — slow the "
    "update period to extend depth at fast refresh rates."));
histForm->addRow(QStringLiteral("Depth:"), m_historyDepthCombo);

m_effectiveDepthLabel = new QLabel(histGroup);
m_effectiveDepthLabel->setStyleSheet(QStringLiteral("color: #80a0b0;"));
histForm->addRow(QStringLiteral(""), m_effectiveDepthLabel);

mainLayout->addWidget(histGroup);

connect(m_historyDepthCombo, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this](int idx) {
    if (!m_currentSpectrumWidget) return;
    const qint64 ms = m_historyDepthCombo->itemData(idx).toLongLong();
    m_currentSpectrumWidget->setWaterfallHistoryMs(ms);
    updateEffectiveDepthLabel();
});
```

(`m_currentSpectrumWidget` is the existing pointer pattern used by other Waterfall page controls — match its name exactly via grep.)

- [ ] **Step 3: Add `updateEffectiveDepthLabel` helper**

Declare in the header (private slot or method, your choice). Define in the .cpp:

```cpp
void WaterfallDefaultsPage::updateEffectiveDepthLabel()
{
    if (!m_currentSpectrumWidget || !m_effectiveDepthLabel) return;
    const qint64 depthMs  = m_currentSpectrumWidget->waterfallHistoryMs();
    const int    periodMs = std::max(1, m_currentSpectrumWidget->wfUpdatePeriodMs());
    const int    capRows  = 4096;
    const int    rows     = std::min(
        static_cast<int>((depthMs + periodMs - 1) / periodMs), capRows);
    const int    effectiveMs = rows * periodMs;
    const int    minutes = effectiveMs / 60000;
    const int    seconds = (effectiveMs / 1000) % 60;

    m_effectiveDepthLabel->setText(
        QStringLiteral("Effective rewind: %1 ms × %2 rows = %3:%4 (%5)")
            .arg(periodMs)
            .arg(rows)
            .arg(minutes)
            .arg(seconds, 2, 10, QLatin1Char('0'))
            .arg(rows == capRows ? QStringLiteral("cap reached")
                                 : QStringLiteral("uncapped")));
}
```

- [ ] **Step 4: Wire `updateEffectiveDepthLabel` into the existing update-period slider**

Find the slider's `valueChanged` connection (line 708). In its lambda, after `w->setWfUpdatePeriodMs(v)`, add:

```cpp
updateEffectiveDepthLabel();
```

- [ ] **Step 5: Set initial dropdown selection in `applyToWidget` / load path**

Find where the page reads its values from the SpectrumWidget on activation. Add:

```cpp
{
    QSignalBlocker bd(m_historyDepthCombo);
    const qint64 ms = sw->waterfallHistoryMs();
    int idx = 3;  // default: 20 minutes
    for (int i = 0; i < m_historyDepthCombo->count(); ++i) {
        if (m_historyDepthCombo->itemData(i).toLongLong() == ms) {
            idx = i;
            break;
        }
    }
    m_historyDepthCombo->setCurrentIndex(idx);
}
updateEffectiveDepthLabel();
```

- [ ] **Step 6: Build and verify**

Run:
```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target NereusSDR 2>&1 | tail -10
killall NereusSDR 2>/dev/null; ./build/NereusSDR &
```

Live-test: open Setup → Display → Waterfall page. Expected:
1. New "Rewind history" group with Depth dropdown (60 s / 5 min / 15 min / 20 min, "20 minutes" selected by default).
2. "Effective rewind:" hint line below shows `30 ms × 4096 rows = 2:03 (cap reached)` at default settings.
3. Drag the update-period slider to 500 ms — hint updates to `500 ms × 2400 rows = 20:00 (uncapped)`.
4. Quit + relaunch — depth selection persists. Verify with:
```bash
grep DisplayWaterfallHistoryMs ~/.config/NereusSDR/NereusSDR.settings
```

- [ ] **Step 7: Commit**

```bash
git add src/gui/setup/DisplaySetupPages.{h,cpp}
git commit -S -m "feat(setup): add waterfall rewind depth dropdown (E task 11)

New 'Rewind history' group on the Waterfall page with depth
dropdown (60s/5min/15min/20min, default 20min) and a live
effective-rewind hint showing 'NN ms × NN rows = M:SS'.

Persisted as DisplayWaterfallHistoryMs per-SpectrumWidget,
matching the existing DisplayWfUpdatePeriodMs pattern."
```

---

## Task 12: Manual verification matrix

Comprehensive checklist for the manual verification matrix following the 3G-8 / sub-epic D format.

**Files:**
- Create: `docs/architecture/phase3g-rx-epic-e-verification/README.md`

- [ ] **Step 1: Create the verification matrix file**

```markdown
# Phase 3G — RX Epic Sub-epic E Verification Matrix

**Build under test:** NereusSDR `claude/phase3g-rx-epic-e-waterfall-scrollback`

**Setup:** Connect to a real OpenHPSDR radio (HL2 or ANAN). Defaults at first launch (no `~/.config/NereusSDR/NereusSDR.settings`): 30 ms update period, 20 min depth, cap-reached effective rewind ≈ 2 min.

| # | Action | Expected | Pass? |
|---|--------|----------|-------|
| **Live state** | | | |
| 1 | Fresh launch, connect to radio, watch waterfall scroll for 30 s | Waterfall fills top-down (or bottom-up if reverse-scroll on); right-edge shows narrow (36 px) time-scale strip with grey "LIVE" button at top; tick labels read `5s, 10s, 15s, ...` elapsed | |
| 2 | Hover over the "LIVE" button | Cursor changes to pointing-hand | |
| 3 | Hover over the time-scale strip (away from LIVE button) | Cursor changes to vertical-resize | |
| **Pause / scrub** | | | |
| 4 | Click and drag UP from anywhere on the time-scale strip | Strip widens to ~72 px; tick labels switch to `-HH:mm:ssZ` UTC absolute timestamps; LIVE button turns bright red (#c02020) with white "LIVE" text; waterfall content shifts down to show older rows | |
| 5 | Continue dragging — push past max history | Drag bottoms out at the oldest available row; no further movement; no crash | |
| 6 | Release the drag | Position holds; does NOT auto-resume to live | |
| 7 | Wait 5 seconds while paused | Displayed row stays visually fixed; new live rows continue to be recorded silently (verify by checking that the ring buffer's row-count grows — easiest via temp `qDebug()` in `appendHistoryRow` if needed) | |
| 8 | Click the red LIVE button | Snaps back to live; strip narrows; labels return to elapsed-seconds; LIVE button turns grey | |
| **Pan / zoom while paused** | | | |
| 9 | Drag down to pause + rewind ~30 s. Click-drag the freq scale a *small* amount (within 25% of half-bandwidth, e.g. 5 kHz on a 192 kHz panadapter) | History reprojects horizontally to fit the new center; LIVE stays red; rewind history visible but slightly compressed in the freq axis | |
| 10 | While still paused, click a *different band button* (e.g. 20m → 40m) | History clears; LIVE button turns grey; waterfall starts fresh on the new band; cannot rewind through the band switch | |
| 11 | While still paused, change the bandwidth from 192 kHz to 96 kHz via Ctrl+scroll on the freq scale | `bwChanged` triggers largeShift; history clears; LIVE turns grey | |
| **Period change** | | | |
| 12 | Open Setup → Display → Waterfall. Drag update-period slider from 30 ms to 100 ms | Waterfall scrolls slower; rewind history is preserved (capacity stays at 4096 within the common range); effective-depth hint updates to `100 ms × 4096 rows = 6:49 (cap reached)` | |
| 13 | Drag period slider to 500 ms | Waterfall scrolls much slower; rewind history is wiped (capacity drops to 2400, below the cap); LIVE button forced grey; hint reads `500 ms × 2400 rows = 20:00 (uncapped)` | |
| 14 | Drag period slider mid-drag from 30 ms to 500 ms in one motion | No mid-drag flicker; only the final value triggers ensureWaterfallHistory after the 250 ms debounce | |
| **Depth change** | | | |
| 15 | At 500 ms period, change depth dropdown from 20 min to 5 min | Capacity drops to 600 rows; hint updates to `500 ms × 600 rows = 5:00 (uncapped)`; history wiped + forced live | |
| 16 | At 500 ms / 5 min depth, change depth back to 20 min | Capacity climbs to 2400 rows; history wiped + forced live (height changed); hint reads `500 ms × 2400 rows = 20:00 (uncapped)` | |
| **Resize** | | | |
| 17 | Drag the divider between spectrum and waterfall up and down rapidly | No flicker; no allocation lag; once released, viewport rebuilds from preserved history (same rows visible at new height) | |
| 18 | Resize the main window narrow → wide → narrow | History preserved across width changes (verify by pausing with rewind data, then resizing — content stays visually intact, just rescaled horizontally) | |
| **Persistence** | | | |
| 19 | Set depth to 5 minutes. Quit + relaunch | Setup → Display → Waterfall opens with "5 minutes" selected. `~/.config/NereusSDR/NereusSDR.settings` contains `DisplayWaterfallHistoryMs0=300000` | |
| 20 | Set depth to 60 seconds. Quit + relaunch | Persists as `DisplayWaterfallHistoryMs0=60000` | |
| **Disconnect** | | | |
| 21 | While paused with rewind data, click Disconnect | Waterfall + spectrum + history all clear; LIVE button stays present (greyed); reconnecting starts fresh | |
| **Multi-pan (Phase 3F is planned, not yet — sanity-check single-pan only)** | | | |
| 22 | Confirm only one panadapter exists | Sub-epic E ring buffer is per-`SpectrumWidget`; multi-pan validation is deferred to Phase 3F | |
| **Memory** | | | |
| 23 | At default settings, run for 10 minutes; check Activity Monitor / `top` | RAM growth from sub-epic-D baseline ≈ 33 MB (= 2000 × 4 × 4096 bytes for the QImage history + ~32 KB for timestamps) | |
| 24 | Slow update period to 1000 ms; check capacity | History capacity = 1200 rows ≈ 9.6 MB at 2000px width | |
| **Visual states (screenshot for PR body)** | | | |
| 25 | Live state, narrow strip, grey LIVE | Take screenshot | |
| 26 | Paused state, wide strip, red LIVE, UTC labels | Take screenshot | |
| 27 | Setup → Display → Waterfall with rewind group visible | Take screenshot | |

## Deferred / known limitations

- **Multi-pan history sharing:** each `SpectrumWidget` owns its own ring buffer (per the design doc). Sharing is YAGNI until Phase 3F lands and a real use case emerges.
- **Stored format is RGB32, not raw dBm:** changing the colour palette / black level while paused continues to show *old* palette in the rewind window until those rows roll off. Faithful to AetherSDR; revisiting would 5× the per-row memory cost.
- **No per-row centerHz metadata:** rewind across a band change is impossible by design (Q4 = B). The history clears on largeShift instead.
- **Drag-to-zero does NOT auto-resume:** only the LIVE button truly returns to live. Drag-to-bottom would auto-bump back to >0 on the next row arrival (faithful to upstream `m_wfLive` semantics).
```

- [ ] **Step 2: Commit**

```bash
git add docs/architecture/phase3g-rx-epic-e-verification/README.md
git commit -S -m "docs(phase3g): add sub-epic E verification matrix (E task 12)

27-row manual matrix following the 3G-8 / sub-epic D pattern.
Covers live state, pause / scrub, pan / zoom (small + largeShift),
period change (preserve + wipe boundary), depth change, resize,
persistence, disconnect, memory check, and visual screenshots."
```

---

## Task 13: CHANGELOG entry + final smoke pass

**Files:**
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Add the entry under `## [Unreleased]`**

```markdown
### Added — Phase 3G RX Epic Sub-epic E

- **Waterfall rewind / scrollback.** Drag up on the right-edge time-scale
  strip to pause the waterfall and scroll backward through up to ~2 minutes
  of history at default refresh (more if the update period is slowed). A
  bright-red "LIVE" button appears when paused; one click returns to
  real-time. Tick labels switch from elapsed seconds (live) to absolute
  UTC timestamps (paused). Configurable depth via Setup → Display →
  Waterfall (60 s / 5 min / 15 min / 20 min, default 20 min, persisted as
  `DisplayWaterfallHistoryMs`).

  Ported from AetherSDR `main@0cd4559` for the live machinery and the
  unmerged PR `aetherclaude/issue-1478@2bb3b5c` (#1478) for the 4 096-row
  cap and 250 ms resize debounce. Diverges from upstream by clearing
  history on band/zoom largeShift, keeping the rewind window coherent
  with the current band — see
  `docs/architecture/phase3g-rx-epic-e-waterfall-scrollback-plan.md`
  §authoring-time #3.

  Closes the last item in the Phase 3G RX Experience Epic.
```

- [ ] **Step 2: Final full-system smoke**

Walk the verification matrix end-to-end. Take screenshots for items 25, 26, 27. Save them in the worktree at `/tmp/nereussdr-rewind-{live,paused,setup}.png` for the PR body.

- [ ] **Step 3: Run all unit tests**

```bash
ctest --test-dir build --output-on-failure
```

Expected: PASS.

- [ ] **Step 4: Run the attribution verifiers (CI matches these)**

```bash
python3 scripts/verify-thetis-headers.py
python3 scripts/verify-inline-tag-preservation.py
python3 scripts/check-new-ports.py
```

Expected: all three exit 0.

- [ ] **Step 5: Commit + push the branch**

```bash
git add CHANGELOG.md
git commit -S -m "docs: changelog entry for sub-epic E (E task 13)"
git push -u origin claude/phase3g-rx-epic-e-waterfall-scrollback
```

- [ ] **Step 6: Stage the PR draft in chat (do NOT open the PR)**

Per project policy, draft the PR title + body in chat for the user to review before opening. Suggested template:

```
Title:  feat: phase 3g rx epic sub-epic E — waterfall rewind / scrollback

Body:
## Summary
- Per-panadapter waterfall history ring buffer + scrub gesture + LIVE button
- Setup → Display → Waterfall depth dropdown (60s/5m/15m/20m, default 20m)
- 4096-row cap + 250ms resize debounce ported from unmerged AetherSDR PR #1478
- LargeShift band/zoom clears history (NereusSDR divergence — see plan §authoring-time #3)
- Closes the last item in the Phase 3G RX Experience Epic

## Test plan
- [ ] Live state: narrow strip + grey LIVE + elapsed-seconds labels
- [ ] Drag up to pause: wide strip + red LIVE + UTC labels + history visible
- [ ] LIVE click resumes live
- [ ] Small pan reprojects history; band button clears history + force-live
- [ ] Depth dropdown persists across quit/relaunch
- [ ] Period slider drag is debounced (no mid-drag flicker)
- [ ] Pre-commit hooks pass (verify-thetis-headers, verify-inline-tag-preservation, check-new-ports)
- [ ] tst_waterfall_scrollback passes

🤖 Generated with [Claude Code](https://claude.com/claude-code)
```

---

## Self-review

After writing the plan I run a fresh-eyes pass against the spec.

**1. Spec coverage** (vs `phase3g-rx-experience-epic-design.md` § E):

| Spec point | Covered by task |
|---|---|
| `m_waterfallHistory` QImage + parallel timestamp array + write head + row count + offset for scrub | Task 1 (decls), Task 3 (impls) |
| GPU renderer: upload past-N window when offset > 0; resume normal live appends when offset = 0 | Task 4 (live append unchanged), Task 3 (`rebuildWaterfallViewport` + `m_wfLastUploadedRow = -1`) |
| Drag-up gesture on waterfall = scroll back; release at offset 0 = live | Task 7 (gesture), Task 5 (`setWaterfallLive`); note authoring-time clarification: drag does not auto-resume (matches upstream) |
| Timestamp + center-freq overlay at cursor when offset > 0 | Task 8 (`drawTimeScale` paused-mode UTC labels). Note: per-row centerHz is not stored — see Q4 design decision |
| Setup → Display history-depth selector (60s/5min/15min/20min, persisted as `DisplayWaterfallHistoryMs`) | Task 11 |
| Default 20 min, cap 4096 rows | Task 1 (constants), Task 10 (load default) |
| Drag-up ergonomic direction | Task 7 |
| Width resize preserves via `QImage::scaled` | Task 3 (`ensureWaterfallHistory` height-equal branch) |
| Pan/zoom during scrollback: blank or reproject | Task 9 (reproject for small; clear for largeShift = Q4 = B) |
| `-3m 42s` cursor age display | Task 8 (`pausedTimeLabelForAge` returns `-HH:mm:ssZ` per upstream's exact format) |
| Pure in-memory; lost on panadapter close or app restart | Task 4 (clearDisplay calls clearWaterfallHistory) |
| Per-panadapter ownership | Implicit — each SpectrumWidget owns its own ring; matches design doc |

All §E spec points covered.

**2. Placeholder scan:** No "TBD", "TODO", "implement later", "fill in details", or "similar to Task N" placeholders. Every code step shows complete code. Every step has expected output for verification.

**3. Type / signature consistency:**
- `clearWaterfallHistory()` (no params) used uniformly across Tasks 3, 4, 9.
- `setWaterfallLive(bool)` used uniformly across Tasks 5, 7.
- `m_waterfallHistoryMs` (qint64) consistent in Tasks 1, 10, 11.
- `m_wfHistoryOffsetRows` (int) consistent across Tasks 1, 3, 7, 8, 9.
- `reprojectWaterfall` signature `(double, double, double, double)` for centerHz/bandwidthHz consistent with NereusSDR's existing Hz units (verify in preflight #5; switch to MHz if the codebase uses MHz).

**4. Scope:** Single sub-epic, single PR, single SpectrumWidget feature surface. Not over-scoped.

**5. Cross-checks:**
- Header attribution: file-level mod-block update mentioned in §File Structure; inline cites attached to every ported function.
- Pre-commit hooks: Task 13 explicitly runs all three verifiers.
- GPG signing: every `git commit` example in this plan uses `-S`.
- AppSettings (not QSettings): Task 10 uses `AppSettings::instance()` and `settingsKey(... , m_panIndex)` matching the existing pattern.
- Boolean format: no booleans persisted in this sub-epic (depth is int, period is int, live state is runtime-only). N/A.

Plan is complete.
