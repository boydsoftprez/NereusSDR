# Phase 3G — RX Epic Sub-epic A: Right-edge dBm Scale Strip Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port AetherSDR's right-edge dBm scale strip into NereusSDR's `SpectrumWidget`, replacing the minimal left-edge strip that's currently hidden behind the docked display applet.

**Architecture:** Flip the strip's position from the left edge to the right edge by changing `specRect` / `wfRect` / `freqRect` offsets in both the QPainter paint path and the GPU overlay-texture path, then port `drawDbmScale()` from AetherSDR verbatim (semi-opaque background, left border, tick marks, adaptive step, up/down arrow buttons). Extend the existing `m_draggingDbm` state machine with arrow-click, hover-cursor, and wheel-zoom interactions. Emit a new `dbmRangeChangeRequested(float min, float max)` signal so the MainWindow can observe range changes (AetherSDR pattern).

**Tech Stack:** C++20, Qt6, QRhi widget (existing GPU + QPainter dual-path), QTest for unit tests.

**Spec reference:** [phase3g-rx-experience-epic-design.md](phase3g-rx-experience-epic-design.md) § sub-epic A.

**Upstream stamp:** AetherSDR `main@0cd4559` (2026-04-21). All inline cites in ports use `// From AetherSDR SpectrumWidget.cpp:<line> [@0cd4559]`.

---

## File Structure

| File | Responsibility | Change type |
|---|---|---|
| `src/gui/SpectrumWidget.h` | Widget declaration | Modify — add `kDbmArrowH` constant, `dbmRangeChangeRequested` signal, static helper decls |
| `src/gui/SpectrumWidget.cpp` | Widget implementation | Modify — rect layout flip, `drawDbmScale()` rewrite, mouse/wheel handlers, helpers |
| `src/gui/dbm_strip_math.h` *(new)* | Pure geometry + step helpers | Create — unit-testable non-Qt math |
| `src/gui/dbm_strip_math.cpp` *(new)* | Helpers impl | Create |
| `tests/tst_spectrum_dbm_strip.cpp` *(new)* | Unit tests | Create — test pure helpers + signal emission |
| `tests/CMakeLists.txt` | Test registration | Modify — register new test target |
| `docs/architecture/phase3g-rx-epic-a-verification/README.md` *(new)* | Manual verification matrix | Create — following 3G-8 pattern |

## Pre-flight inventory (read-only — no code changes)

Before starting, run this exact grep against the worktree to refresh your mental model of every call-site touched:

```bash
rg -n 'kDbmStripW|m_draggingDbm|drawDbmScale|DBM_STRIP' src/gui/SpectrumWidget.cpp src/gui/SpectrumWidget.h
```

Expected hits (at time of plan writing, will drift):

**Header:**
- `src/gui/SpectrumWidget.h:521` — `static constexpr int kDbmStripW = 36;`
- `src/gui/SpectrumWidget.h:465` — `void drawDbmScale(QPainter& p, const QRect& specRect);`
- `src/gui/SpectrumWidget.h:623` — `bool m_draggingDbm{false};`

**Layout rect construction sites (all 6 must flip left→right):**
- `src/gui/SpectrumWidget.cpp:1081` — `paintEvent` main rects
- `src/gui/SpectrumWidget.cpp:1887` — `mousePressEvent` rect for downstream hit-tests
- `src/gui/SpectrumWidget.cpp:2021` — `mouseMoveEvent` rect for drag modes + hover
- `src/gui/SpectrumWidget.cpp:2166` — `mouseReleaseEvent` rect for click-detect-in-pan
- `src/gui/SpectrumWidget.cpp:2484` — **GPU renderer** (`renderGpuFrame` — currently FULL-WIDTH, must become `w - kDbmStripW`)
- `src/gui/SpectrumWidget.cpp:2964` — `updateVfoPositions` — VFO widget coordinate mapping

**drawDbmScale call sites (both paint the left-edge strip currently):**
- `src/gui/SpectrumWidget.cpp:1097` — QPainter path
- `src/gui/SpectrumWidget.cpp:2564` — GPU overlay texture path
- `src/gui/SpectrumWidget.cpp:1452` — existing minimal `drawDbmScale()` impl itself

**Drag/hit-test sites:**
- `src/gui/SpectrumWidget.cpp:1938` — hit-test `if (mx < kDbmStripW)` (LEFT edge) in mousePressEvent
- `src/gui/SpectrumWidget.cpp:2025,2174,2183` — drag-pan math + commit + cleanup

**Double-click handling:** NereusSDR's SpectrumWidget has no `mouseDoubleClickEvent` override (confirmed via `rg mouseDoubleClickEvent`). AetherSDR's "consume double-click on strip" feature (SpectrumWidget.cpp:2526 [@0cd4559]) is therefore N/A for NereusSDR. Revisit if/when a spectrum double-click handler is added.

Any additional hit from the `rg` command above that isn't in this list needs review before you edit — ad-hoc "strip is on the left" math may exist that this inventory missed.

---

## Task 1: Extract pure strip-math helpers

Extract the geometry and adaptive-step logic into non-Qt pure functions so later tasks can unit-test them without a display. None of these are behavior changes yet; they exist as no-op scaffolding after this task.

**Files:**
- Create: `src/gui/dbm_strip_math.h`
- Create: `src/gui/dbm_strip_math.cpp`
- Modify: `CMakeLists.txt` (or equivalent build file for `src/gui/` sources)

- [ ] **Step 1: Create the header**

Create `src/gui/dbm_strip_math.h`:

```cpp
#pragma once

#include <QRect>

namespace NereusSDR::DbmStrip {

// Strip occupies the rightmost kDbmStripW pixels of the spectrum area.
// Returns the strip rect given the full spectrum area rect and strip width.
QRect stripRect(const QRect& specRect, int stripW);

// Arrow row (up/down triangles side-by-side) sits at the top of the strip.
// Height is kDbmArrowH; width matches strip. Returns the arrow row rect.
QRect arrowRowRect(const QRect& strip, int arrowH);

// Hit-test an x coordinate against the left/right halves of the arrow row.
// Returns:
//   -1 = not in arrow row
//    0 = left half (Up)
//    1 = right half (Down)
int arrowHit(int x, const QRect& arrowRow);

// Adaptive dB step sizing matching AetherSDR's 4-6-label target.
// rawStep = dynamicRange / 5.0f
// stepDb  = 20 if rawStep >= 20
//           10 if rawStep >= 10
//            5 if rawStep >=  5
//            2 otherwise
// From AetherSDR SpectrumWidget.cpp:4901-4906 [@0cd4559]
float adaptiveStepDb(float dynamicRange);

}  // namespace NereusSDR::DbmStrip
```

- [ ] **Step 2: Create the implementation**

Create `src/gui/dbm_strip_math.cpp`:

```cpp
// Pure helpers for the right-edge dBm strip geometry and adaptive step.
// Extracted so SpectrumWidget's paint + hit-test logic is unit-testable
// without a display. No Qt widget dependency.

#include "dbm_strip_math.h"

namespace NereusSDR::DbmStrip {

QRect stripRect(const QRect& specRect, int stripW)
{
    const int stripX = specRect.right() - stripW + 1;
    return QRect(stripX, specRect.top(), stripW, specRect.height());
}

QRect arrowRowRect(const QRect& strip, int arrowH)
{
    return QRect(strip.left(), strip.top(), strip.width(), arrowH);
}

int arrowHit(int x, const QRect& arrowRow)
{
    if (x < arrowRow.left() || x > arrowRow.right()) {
        return -1;
    }
    const int half = arrowRow.left() + arrowRow.width() / 2;
    return (x < half) ? 0 : 1;
}

// From AetherSDR SpectrumWidget.cpp:4901-4906 [@0cd4559]
float adaptiveStepDb(float dynamicRange)
{
    const float rawStep = dynamicRange / 5.0f;
    if (rawStep >= 20.0f) return 20.0f;
    if (rawStep >= 10.0f) return 10.0f;
    if (rawStep >=  5.0f) return  5.0f;
    return 2.0f;
}

}  // namespace NereusSDR::DbmStrip
```

- [ ] **Step 3: Register the new sources with the build**

Find the existing `src/gui/SpectrumWidget.cpp` entry in `CMakeLists.txt` (top-level) or the appropriate target and add the two new files. Example pattern (the exact surrounding text may differ — match the adjacent listing):

```cmake
    src/gui/SpectrumWidget.cpp
    src/gui/SpectrumWidget.h
    src/gui/dbm_strip_math.cpp
    src/gui/dbm_strip_math.h
```

- [ ] **Step 4: Verify the project still builds**

Run:

```bash
cmake --build build -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

Expected: clean build, no new warnings. The helper file compiles standalone and has no call-sites yet (zero behavior change).

- [ ] **Step 5: Commit**

```bash
git add src/gui/dbm_strip_math.h src/gui/dbm_strip_math.cpp CMakeLists.txt
git commit -m "feat(spectrum): add pure dBm-strip geometry helpers

Extracts strip rect, arrow-row hit-test, and adaptive step-sizing
into src/gui/dbm_strip_math.{h,cpp} so the upcoming right-edge
strip port can be unit-tested without a QApplication.

Behavior unchanged — no call-sites yet."
```

---

## Task 2: Unit tests for the helpers

Write tests before wiring up call-sites so the math is locked in before the visual port lands.

**Files:**
- Create: `tests/tst_spectrum_dbm_strip.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the test file**

Create `tests/tst_spectrum_dbm_strip.cpp`:

```cpp
#include <QtTest>
#include <QRect>

#include "gui/dbm_strip_math.h"

using namespace NereusSDR::DbmStrip;

class TestSpectrumDbmStrip : public QObject
{
    Q_OBJECT

private slots:
    void stripRect_occupiesRightEdge();
    void arrowRowRect_atTopOfStrip();
    void arrowHit_leftHalfReturnsZero();
    void arrowHit_rightHalfReturnsOne();
    void arrowHit_outsideReturnsNegativeOne();
    void adaptiveStepDb_selectsByRange_data();
    void adaptiveStepDb_selectsByRange();
};

void TestSpectrumDbmStrip::stripRect_occupiesRightEdge()
{
    const QRect spec(0, 0, 800, 400);
    const QRect strip = stripRect(spec, 36);
    QCOMPARE(strip.left(), 765);  // 800 - 36 + 1
    QCOMPARE(strip.top(),  0);
    QCOMPARE(strip.width(), 36);
    QCOMPARE(strip.height(), 400);
}

void TestSpectrumDbmStrip::arrowRowRect_atTopOfStrip()
{
    const QRect strip(765, 10, 36, 400);
    const QRect arrows = arrowRowRect(strip, 14);
    QCOMPARE(arrows.left(), 765);
    QCOMPARE(arrows.top(),  10);
    QCOMPARE(arrows.width(), 36);
    QCOMPARE(arrows.height(), 14);
}

void TestSpectrumDbmStrip::arrowHit_leftHalfReturnsZero()
{
    const QRect arrows(100, 0, 36, 14);
    QCOMPARE(arrowHit(100, arrows), 0);  // leftmost
    QCOMPARE(arrowHit(117, arrows), 0);  // just left of midpoint (100 + 18)
}

void TestSpectrumDbmStrip::arrowHit_rightHalfReturnsOne()
{
    const QRect arrows(100, 0, 36, 14);
    QCOMPARE(arrowHit(118, arrows), 1);  // at midpoint
    QCOMPARE(arrowHit(135, arrows), 1);  // rightmost
}

void TestSpectrumDbmStrip::arrowHit_outsideReturnsNegativeOne()
{
    const QRect arrows(100, 0, 36, 14);
    QCOMPARE(arrowHit(99,  arrows), -1);
    QCOMPARE(arrowHit(136, arrows), -1);
    QCOMPARE(arrowHit(-5,  arrows), -1);
}

void TestSpectrumDbmStrip::adaptiveStepDb_selectsByRange_data()
{
    QTest::addColumn<float>("dynamicRange");
    QTest::addColumn<float>("expectedStep");

    // rawStep = dynamicRange / 5
    QTest::newRow("120dB → 20 step") << 120.0f << 20.0f;  // raw 24
    QTest::newRow("100dB → 20 step") << 100.0f << 20.0f;  // raw 20
    QTest::newRow("99dB  → 10 step") <<  99.0f << 10.0f;  // raw 19.8
    QTest::newRow("50dB  → 10 step") <<  50.0f << 10.0f;  // raw 10
    QTest::newRow("49dB  →  5 step") <<  49.0f <<  5.0f;  // raw 9.8
    QTest::newRow("25dB  →  5 step") <<  25.0f <<  5.0f;  // raw 5
    QTest::newRow("24dB  →  2 step") <<  24.0f <<  2.0f;  // raw 4.8
    QTest::newRow("10dB  →  2 step") <<  10.0f <<  2.0f;  // raw 2
}

void TestSpectrumDbmStrip::adaptiveStepDb_selectsByRange()
{
    QFETCH(float, dynamicRange);
    QFETCH(float, expectedStep);
    QCOMPARE(adaptiveStepDb(dynamicRange), expectedStep);
}

QTEST_APPLESS_MAIN(TestSpectrumDbmStrip)
#include "tst_spectrum_dbm_strip.moc"
```

- [ ] **Step 2: Register the test target**

Open `tests/CMakeLists.txt` and find an existing small `add_executable(tst_*)` block (for example `tst_about_dialog`). Add a sibling block for the new test:

```cmake
add_executable(tst_spectrum_dbm_strip
    tst_spectrum_dbm_strip.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/dbm_strip_math.cpp
)
target_link_libraries(tst_spectrum_dbm_strip PRIVATE Qt6::Test Qt6::Gui)
target_include_directories(tst_spectrum_dbm_strip PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME tst_spectrum_dbm_strip COMMAND tst_spectrum_dbm_strip)
```

If the existing pattern uses a helper function / macro, follow that exactly instead of inventing new CMake. Copy the adjacent `tst_about_dialog` block and adapt.

- [ ] **Step 3: Build and run the tests**

```bash
cmake --build build -j $(nproc 2>/dev/null || sysctl -n hw.ncpu) --target tst_spectrum_dbm_strip
ctest --test-dir build -R tst_spectrum_dbm_strip --output-on-failure
```

Expected: **PASS — 7 test cases, all green.**

- [ ] **Step 4: Commit**

```bash
git add tests/tst_spectrum_dbm_strip.cpp tests/CMakeLists.txt
git commit -m "test(spectrum): unit tests for dBm-strip helpers

Covers stripRect (right-edge), arrowRowRect (top-of-strip),
arrowHit (left=0, right=1, outside=-1), and adaptiveStepDb
across the 20/10/5/2 dB boundaries.

7 test cases, QTEST_APPLESS_MAIN (no QApplication needed)."
```

---

## Task 3: Flip strip layout from left to right edge (behavior-preserving visual)

Move the strip's position without touching its visual appearance or adding new interactions. After this task the existing minimal strip renders on the right edge; everything else on the spectrum (grid, trace, waterfall, freq scale, VFO markers) shifts left by 36 px.

**All six `specRect` construction sites must flip from `QRect(kDbmStripW, 0, w - kDbmStripW, specH)` to `QRect(0, 0, w - kDbmStripW, specH)`.** This task walks each one.

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp` — 6 rect-construction sites + 2 drawDbmScale call sites + 1 hit-test + `drawDbmScale()` body

- [ ] **Step 1: Update paintEvent rect layout**

Open `src/gui/SpectrumWidget.cpp`. Find the QPainter `paintEvent` block near line 1075. Change the three rects from `(kDbmStripW, …, w - kDbmStripW, …)` to `(0, …, w - kDbmStripW, …)` and move the `drawDbmScale` call to use the right-edge rect.

Find:

```cpp
    // Spectrum area (right of dBm strip)
    QRect specRect(kDbmStripW, 0, w - kDbmStripW, specH);
    // Waterfall area
    QRect wfRect(kDbmStripW, wfTop, w - kDbmStripW, wfH);
    // Frequency scale bar
    QRect freqRect(kDbmStripW, h - kFreqScaleH, w - kDbmStripW, kFreqScaleH);
```

Replace with:

```cpp
    // Spectrum area (left of dBm strip — strip lives on the right edge per
    // AetherSDR convention). From AetherSDR SpectrumWidget.cpp:4858 [@0cd4559]
    QRect specRect(0, 0, w - kDbmStripW, specH);
    // Waterfall area
    QRect wfRect(0, wfTop, w - kDbmStripW, wfH);
    // Frequency scale bar
    QRect freqRect(0, h - kFreqScaleH, w - kDbmStripW, kFreqScaleH);
```

Then find the `drawDbmScale(...)` call below it:

```cpp
    drawDbmScale(p, QRect(0, 0, kDbmStripW, specH));
```

Replace with:

```cpp
    drawDbmScale(p, specRect);  // strip rect derived from specRect.right() inside drawDbmScale
```

Rationale: passing `specRect` lets `drawDbmScale` compute the strip as `stripRect(specRect, kDbmStripW)` — keeps the call site simple and the strip offset lives in one place.

- [ ] **Step 2: Update the GPU overlay path**

Near line 2555 (`m_overlayStaticDirty`) the GPU path constructs the same rects. Apply the identical change. Find:

```cpp
            drawGrid(p, specRect);
            drawDbmScale(p, QRect(0, 0, kDbmStripW, specH));
            p.fillRect(0, specH, w, kDividerH, QColor(0x30, 0x40, 0x50));
            drawFreqScale(p, QRect(0, specH + kDividerH, w, kFreqScaleH));
```

The `specRect` / `wfRect` construction in the GPU path is a few lines above this block — locate it. If it already uses `kDbmStripW` as a left offset in the same pattern as paintEvent, apply the same `(0, …, w - kDbmStripW, …)` flip. Then update the `drawDbmScale` and `drawFreqScale` calls to use the right-edge-clipped rects:

```cpp
            drawGrid(p, specRect);
            drawDbmScale(p, specRect);
            p.fillRect(0, specH, w, kDividerH, QColor(0x30, 0x40, 0x50));
            drawFreqScale(p, QRect(0, specH + kDividerH, w - kDbmStripW, kFreqScaleH));
```

Note the freq-scale rect: width is `w - kDbmStripW`, not `w`, so the freq scale aligns with the spectrum and stops at the strip's left border.

- [ ] **Step 3: Update the existing drawDbmScale to derive strip rect from specRect**

Find the existing `drawDbmScale` at ~line 1452. The signature takes `specRect` but the body paints into `QRect(0, specRect.top(), kDbmStripW, specRect.height())`. Replace the body's rect derivation with the helper:

```cpp
void SpectrumWidget::drawDbmScale(QPainter& p, const QRect& specRect)
{
    // Strip occupies the rightmost kDbmStripW pixels of specRect.
    // From AetherSDR SpectrumWidget.cpp:4858 [@0cd4559]
    const QRect strip = NereusSDR::DbmStrip::stripRect(specRect, kDbmStripW);

    p.fillRect(strip, QColor(0x10, 0x15, 0x20));

    QFont font = p.font();
    font.setPixelSize(9);
    p.setFont(font);
    p.setPen(m_gridTextColor);

    float bottom = m_refLevel - m_dynamicRange;
    float step = 10.0f;
    if (m_dynamicRange <= 50.0f) {
        step = 5.0f;
    }

    for (float dbm = bottom; dbm <= m_refLevel; dbm += step) {
        int y = dbmToY(dbm, specRect);
        QString label = QString::number(static_cast<int>(dbm));
        QRect textRect(strip.left() + 2, y - 6, strip.width() - 4, 12);
        p.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, label);
    }
}
```

Add the include at the top of `SpectrumWidget.cpp`:

```cpp
#include "dbm_strip_math.h"
```

- [ ] **Step 4: Update mousePressEvent strip hit-test**

Find the dBm hit-test near line 1937:

```cpp
    // 1. dBm scale strip — drag to adjust ref level
    if (mx < kDbmStripW) {
        m_draggingDbm = true;
        m_dragStartY = my;
        m_dragStartRef = m_refLevel;
        setCursor(Qt::SizeVerCursor);
        return;
    }
```

Replace with:

```cpp
    // 1. dBm scale strip — drag to adjust ref level.
    // Strip lives on the right edge per AetherSDR convention.
    // From AetherSDR SpectrumWidget.cpp:1712-1745 [@0cd4559]
    const int stripX = width() - kDbmStripW;
    int specH_ = static_cast<int>(height() * m_spectrumFrac);
    if (mx >= stripX && my < specH_) {
        // Arrow-click and wheel interactions land in Task 4 / Task 6.
        // For now, below the arrow row becomes a drag-pan as before.
        m_draggingDbm = true;
        m_dragStartY = my;
        m_dragStartRef = m_refLevel;
        setCursor(Qt::SizeVerCursor);
        return;
    }
```

- [ ] **Step 5: Update mouseMoveEvent rect construction (site 3 of 6)**

Find `QRect specRect(kDbmStripW, 0, w - kDbmStripW, specH);` near line 2021 and replace with:

```cpp
    QRect specRect(0, 0, w - kDbmStripW, specH);
```

- [ ] **Step 5a: Update mousePressEvent rect construction (site 4 of 6)**

Near line 1887:

```cpp
    QRect specRect(kDbmStripW, 0, w - kDbmStripW, specH);
```

Replace with:

```cpp
    QRect specRect(0, 0, w - kDbmStripW, specH);
```

- [ ] **Step 5b: Update mouseReleaseEvent rect construction (site 5 of 6)**

Near line 2166 (inside the click-detect-in-pan block):

```cpp
                QRect specRect(kDbmStripW, 0, w - kDbmStripW, specH);
```

Replace with:

```cpp
                QRect specRect(0, 0, w - kDbmStripW, specH);
```

- [ ] **Step 5c: Update GPU renderer rect (site 6 of 6) — IMPORTANT**

In `renderGpuFrame()` near line 2484, the GPU path currently uses **full width**:

```cpp
    const QRect specRect(0, 0, w, specH);
    const QRect wfRect(0, wfY, w, wfH);
```

Change to clip at the strip's left border so the GPU trace doesn't draw underneath the strip:

```cpp
    // Strip lives on the right edge (overlay texture paints it there).
    // Clip GPU content to w - kDbmStripW so the trace stops at the
    // strip's border instead of being drawn under it.
    const QRect specRect(0, 0, w - kDbmStripW, specH);
    const QRect wfRect(0, wfY, w - kDbmStripW, wfH);
```

**Rationale:** the old layout had the strip on the LEFT edge, and the GPU renderer ran full-width which meant the strip overlay COVERED the left 36 px of the GPU spectrum trace. To keep visual parity, we could either (a) keep GPU full-width and let the strip cover the right 36 px, or (b) clip GPU content to match the QPainter path. Option (b) is cleaner — trace stops cleanly at the strip border, matches QPainter path pixel-for-pixel, avoids wasted GPU fill.

- [ ] **Step 5d: Update updateVfoPositions rect (same pattern as QPainter path)**

Near line 2964:

```cpp
    QRect specRect(kDbmStripW, 0, width() - kDbmStripW, specH);
```

Replace with:

```cpp
    QRect specRect(0, 0, width() - kDbmStripW, specH);
```

This keeps VFO widget positions anchored correctly so they don't drift off the trace.

- [ ] **Step 6: Build and sanity-check**

```bash
cmake --build build -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)
./build/NereusSDR
```

Expected when run: the dBm labels now appear on the **right** edge of the spectrum. All other behavior (drag-pan, grid, spectrum trace, waterfall, freq scale, VFO markers) unchanged. Grid lines and trace extend fully from x=0 to `width() - kDbmStripW`.

Verify: clicking the dBm strip area on the right and dragging up/down still pans the ref level.

- [ ] **Step 7: Commit**

```bash
git add src/gui/SpectrumWidget.cpp
git commit -m "feat(spectrum): flip dBm strip layout from left to right edge

Moves specRect/wfRect/freqRect origin to x=0, sizes them to
(w - kDbmStripW), and relocates the dBm strip to the rightmost
kDbmStripW (36 px) column per AetherSDR convention.

Existing drag-pan hit-test and visual layout preserved; strip
rect now derived via DbmStrip::stripRect(specRect, kDbmStripW)
in both the QPainter and GPU overlay paths.

From AetherSDR SpectrumWidget.cpp:4858 [@0cd4559]"
```

---

## Task 4: Port AetherSDR's full drawDbmScale() visual

Replace the minimal NereusSDR strip paint with AetherSDR's full version: semi-opaque background, left border line, up/down arrow triangles, tick marks, adaptive step labels.

**Files:**
- Modify: `src/gui/SpectrumWidget.h` — add `kDbmArrowH` constant
- Modify: `src/gui/SpectrumWidget.cpp` — `drawDbmScale()` rewrite

- [ ] **Step 1: Add arrow-height constant**

Open `src/gui/SpectrumWidget.h`. Find the existing:

```cpp
    static constexpr int kDbmStripW = 36;
```

Add below it:

```cpp
    // Height of each arrow button at the top of the dBm strip.
    // From AetherSDR SpectrumWidget.h:539 [@0cd4559]
    static constexpr int kDbmArrowH = 14;
```

- [ ] **Step 2: Rewrite drawDbmScale**

Replace the `drawDbmScale` body (~line 1452) with:

```cpp
// From AetherSDR SpectrumWidget.cpp:4856-4925 [@0cd4559]
void SpectrumWidget::drawDbmScale(QPainter& p, const QRect& specRect)
{
    const QRect strip = NereusSDR::DbmStrip::stripRect(specRect, kDbmStripW);

    // Semi-opaque background
    p.fillRect(strip, QColor(0x0a, 0x0a, 0x18, 220));

    // Left border line
    p.setPen(QColor(0x30, 0x40, 0x50));
    p.drawLine(strip.left(), specRect.top(), strip.left(), specRect.bottom());

    // ── Up/Down arrows side by side at top ─────────────────────────────
    const int halfW    = kDbmStripW / 2;
    const int upCx     = strip.left() + halfW / 2;          // left half center
    const int dnCx     = strip.left() + halfW + halfW / 2;  // right half center
    const int arrowTop = specRect.top() + 2;
    const int arrowBot = specRect.top() + kDbmArrowH - 2;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x60, 0x80, 0xa0));

    // Up arrow (▲) — left side
    QPolygon upTri;
    upTri << QPoint(upCx - 5, arrowBot)
          << QPoint(upCx + 5, arrowBot)
          << QPoint(upCx,     arrowTop);
    p.drawPolygon(upTri);

    // Down arrow (▼) — right side
    QPolygon dnTri;
    dnTri << QPoint(dnCx - 5, arrowTop)
          << QPoint(dnCx + 5, arrowTop)
          << QPoint(dnCx,     arrowBot);
    p.drawPolygon(dnTri);

    // ── dBm labels ───────────────────────────────────────────────────────
    QFont f = p.font();
    f.setPointSize(7);
    p.setFont(f);
    const QFontMetrics fm(f);

    const int labelTop = specRect.top() + kDbmArrowH + 4;
    const float stepDb = NereusSDR::DbmStrip::adaptiveStepDb(m_dynamicRange);

    const float bottomDbm  = m_refLevel - m_dynamicRange;
    const float firstLabel = std::ceil(bottomDbm / stepDb) * stepDb;

    for (float dbm = firstLabel; dbm <= m_refLevel; dbm += stepDb) {
        const float frac = (m_refLevel - dbm) / m_dynamicRange;
        const int y = specRect.top() + static_cast<int>(frac * specRect.height());
        if (y < labelTop || y > specRect.bottom() - 5) continue;

        // Tick mark
        p.setPen(QColor(0x50, 0x70, 0x80));
        p.drawLine(strip.left(), y, strip.left() + 4, y);

        // Label
        const QString label = QString::number(static_cast<int>(dbm));
        p.setPen(QColor(0x80, 0xa0, 0xb0));
        p.drawText(strip.left() + 6, y + fm.ascent() / 2, label);
    }
}
```

- [ ] **Step 3: Add the `<cmath>` include if not already present**

Check the top of `SpectrumWidget.cpp`:

```bash
rg -n '^#include <cmath>' src/gui/SpectrumWidget.cpp | head -1
```

If it's absent, add `#include <cmath>` alongside the other standard headers. Required for `std::ceil`.

- [ ] **Step 4: Build and sanity-check**

```bash
cmake --build build -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)
./build/NereusSDR
```

Expected on run:
- Strip on the right edge now has a semi-opaque dark-blue background
- Two triangular arrows (▲ left, ▼ right) at the top of the strip
- Tick marks + labels step adaptively (20/10/5/2 dB depending on range)
- Labels use 7-point font, muted blue-grey colour

Verify: dragging on the strip body still pans ref level. Arrows are visible but clicking them does nothing yet (that's Task 5).

- [ ] **Step 5: Commit**

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp
git commit -m "feat(spectrum): port AetherSDR dBm strip visual

Replaces the minimal left-edge dBm labels with AetherSDR's
full right-edge strip: semi-opaque background, left border,
up/down arrow triangles, tick marks, adaptive 20/10/5/2 dB
step sizing. Adds kDbmArrowH = 14 constant.

From AetherSDR SpectrumWidget.cpp:4856-4925 [@0cd4559]"
```

---

## Task 5: Arrow-click interaction + dbmRangeChangeRequested signal

Wire the up/down arrow triangles to adjust the reference level in ±10 dB steps with a 10 dB minimum dynamic-range floor. Add a `dbmRangeChangeRequested(float min, float max)` signal so external observers (MainWindow, tests) can react.

**Files:**
- Modify: `src/gui/SpectrumWidget.h` — new signal
- Modify: `src/gui/SpectrumWidget.cpp` — arrow hit-test in `mousePressEvent`, signal emission on drag-end

- [ ] **Step 1: Declare the signal**

Open `src/gui/SpectrumWidget.h`. Find the `bandwidthChangeRequested` signal declaration (~line 435) and add the new signal right below it:

```cpp
    void bandwidthChangeRequested(double newBandwidthHz);

    // Emitted when user-visible dBm range changes via the scale strip
    // (arrow click, drag-pan on strip body, wheel zoom). Args are the
    // new floor (min) and ceiling (max) in dBm.
    // From AetherSDR SpectrumWidget.cpp:1734 [@0cd4559]
    void dbmRangeChangeRequested(float minDbm, float maxDbm);
```

- [ ] **Step 2: Wire arrow-click in mousePressEvent**

Find the hit-test block you edited in Task 3, Step 4. Replace its body with:

```cpp
    // 1. dBm scale strip — right edge. Arrow row adjusts ref level,
    // body is drag-pan. From AetherSDR SpectrumWidget.cpp:1712-1745 [@0cd4559]
    const int stripX = width() - kDbmStripW;
    const int specH_ = static_cast<int>(height() * m_spectrumFrac);
    if (mx >= stripX && my < specH_) {
        const QRect specRect_(0, 0, width() - kDbmStripW, specH_);
        const QRect strip    = NereusSDR::DbmStrip::stripRect(specRect_, kDbmStripW);
        const QRect arrowRow = NereusSDR::DbmStrip::arrowRowRect(strip, kDbmArrowH);

        if (arrowRow.contains(mx, my)) {
            const int hit = NereusSDR::DbmStrip::arrowHit(mx, arrowRow);
            const float bottom = m_refLevel - m_dynamicRange;
            if (hit == 0) {
                // Up arrow: raise ref level by 10 dB, keep bottom fixed
                m_refLevel += 10.0f;
            } else if (hit == 1) {
                // Down arrow: lower ref level by 10 dB, keep bottom fixed
                m_refLevel -= 10.0f;
            }
            m_dynamicRange = m_refLevel - bottom;
            if (m_dynamicRange < 10.0f) {
                m_dynamicRange = 10.0f;
                m_refLevel = bottom + m_dynamicRange;
            }
            emit dbmRangeChangeRequested(m_refLevel - m_dynamicRange, m_refLevel);
            update();
            return;
        }

        // Below arrows: start drag-pan (existing behavior)
        m_draggingDbm = true;
        m_dragStartY = my;
        m_dragStartRef = m_refLevel;
        setCursor(Qt::SizeVerCursor);
        return;
    }
```

- [ ] **Step 3: Emit the signal on drag-end**

Find the drag-end block around line 2173:

```cpp
        // Persist display settings after drag adjustments
        if (m_draggingDbm || m_draggingDivider) {
            scheduleSettingsSave();
        }
```

Replace with:

```cpp
        // Persist display settings after drag adjustments.
        // Also emit range-change for observers (MainWindow, tests).
        // From AetherSDR SpectrumWidget.cpp:2115 [@0cd4559]
        if (m_draggingDbm) {
            emit dbmRangeChangeRequested(m_refLevel - m_dynamicRange, m_refLevel);
        }
        if (m_draggingDbm || m_draggingDivider) {
            scheduleSettingsSave();
        }
```

- [ ] **Step 4: Build and verify**

```bash
cmake --build build -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)
./build/NereusSDR
```

Expected:
- Click the Up ▲ arrow → ref level increases by 10 dB, labels shift
- Click the Down ▼ arrow → ref level decreases by 10 dB
- Repeated Down clicks eventually clamp dynamic range at 10 dB minimum
- Drag on strip body still pans as before and emits the signal on release

- [ ] **Step 5: Extend the unit test file with a signal test**

Add to `tests/tst_spectrum_dbm_strip.cpp` (inside the existing class definition):

```cpp
    // ... existing test slots ...

    // Note: interactive signal tests for SpectrumWidget itself would need a
    // QApplication and window — those live in the manual verification matrix
    // at docs/architecture/phase3g-rx-epic-a-verification/ rather than unit
    // tests, to keep this translation unit free of Qt Widgets dependencies.
};
```

*(No new test code needed — the signal is adequately exercised by manual verification. This plan does not add a Qt Widgets test target just for one signal.)*

- [ ] **Step 6: Commit**

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp
git commit -m "feat(spectrum): dBm strip arrow clicks + range-change signal

Arrow-row hit-test in mousePressEvent dispatches to ±10 dB
refLevel adjustment with a 10 dB minimum dynamic-range floor
(matches AetherSDR behavior). Adds dbmRangeChangeRequested
signal, emitted on arrow-click and drag-end.

From AetherSDR SpectrumWidget.cpp:1712-1745,2115 [@0cd4559]"
```

---

## Task 6: Hover cursor change outside drag mode

When the mouse hovers over the strip without a button held, change the cursor: pointing-hand over the arrow row, size-vertical over the body.

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp` — `mouseMoveEvent` non-drag path

- [ ] **Step 1: Locate the non-drag hover path**

`mouseMoveEvent` handles drag modes first (Task 3 Step 5 edit is there), then falls through to hover logic. Find the block that sets cursors for filter edges / slice markers / spectrum area. Exact location will be near the end of `mouseMoveEvent` — look for the default `setCursor(Qt::CrossCursor)` or similar.

- [ ] **Step 2: Insert strip hover check before the existing hover fallback**

Directly before the existing hover logic, add:

```cpp
    // Hover over dBm strip → change cursor.
    // From AetherSDR SpectrumWidget.cpp:2241-2248 [@0cd4559]
    const int stripX = width() - kDbmStripW;
    if (mx >= stripX && my < specH) {
        if (my < kDbmArrowH) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::SizeVerCursor);
        }
        return;
    }
```

*(The exact `specH` variable name must match what's already in scope in mouseMoveEvent. Check the surrounding context — if `specH` is computed locally as `int specH = static_cast<int>(h * m_spectrumFrac);`, match that. Otherwise recompute it inside this block.)*

- [ ] **Step 3: Build and verify**

```bash
cmake --build build -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)
./build/NereusSDR
```

Expected: hovering over the arrow row shows a pointing-hand cursor, over the strip body shows a size-vertical cursor. Off the strip, cursor returns to whatever the existing logic chooses (crosshair or filter-edge).

- [ ] **Step 4: Commit**

```bash
git add src/gui/SpectrumWidget.cpp
git commit -m "feat(spectrum): hover cursor on dBm strip

Pointing-hand over the arrow row, size-vertical over the body.
From AetherSDR SpectrumWidget.cpp:2241-2248 [@0cd4559]"
```

---

## Task 7: Wheel-zoom consume + range adjustment inside strip

When the mouse is over the strip and the user rolls the wheel, consume the event and adjust dynamic range.

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp` — `wheelEvent`

- [ ] **Step 1: Find the existing wheelEvent handler**

Locate `wheelEvent` in `SpectrumWidget.cpp`. It already handles bandwidth zoom on the spectrum body.

- [ ] **Step 2: Add strip-consume block at the top of wheelEvent**

Insert at the start of `wheelEvent` (before any bandwidth-zoom logic):

```cpp
    // Wheel over dBm strip: adjust dynamic range in ±5 dB steps.
    // From AetherSDR SpectrumWidget.cpp:2630-2636 [@0cd4559]
    const int mx = static_cast<int>(event->position().x());
    const int my = static_cast<int>(event->position().y());
    const int specH = static_cast<int>(height() * m_spectrumFrac);
    const int stripX = width() - kDbmStripW;
    if (mx >= stripX && my < specH) {
        const int notches = event->angleDelta().y() / 120;
        if (notches != 0) {
            const float bottom = m_refLevel - m_dynamicRange;
            m_dynamicRange = qBound(10.0f, m_dynamicRange - notches * 5.0f, 200.0f);
            m_refLevel = bottom + m_dynamicRange;
            emit dbmRangeChangeRequested(bottom, m_refLevel);
            update();
            scheduleSettingsSave();
        }
        event->accept();
        return;
    }
```

Behaviour: wheel-up narrows the range (zooms in); wheel-down widens (zooms out). Range clamped to [10, 200] dB.

- [ ] **Step 3: Build and verify**

```bash
cmake --build build -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)
./build/NereusSDR
```

Expected: wheel over the strip adjusts dynamic range visibly (labels redistribute); wheel over the spectrum body still zooms bandwidth as before.

- [ ] **Step 4: Commit**

```bash
git add src/gui/SpectrumWidget.cpp
git commit -m "feat(spectrum): wheel-zoom dynamic range inside dBm strip

Wheel over the strip adjusts dynamic range in ±5 dB steps,
clamped to [10, 200] dB. Consumed inside the strip rect so
spectrum-body bandwidth zoom remains unchanged.

From AetherSDR SpectrumWidget.cpp:2630-2636 [@0cd4559]"
```

---

## Task 8: MainWindow wiring — persist range changes to PanadapterModel

The SpectrumWidget now emits `dbmRangeChangeRequested` on every user-initiated range change. Wire it so MainWindow pushes the new values into PanadapterModel (which owns per-band grid storage) and through to AppSettings round-trip.

**Files:**
- Modify: `src/gui/MainWindow.cpp` — connect the signal

- [ ] **Step 1: Find where the SpectrumWidget is instantiated**

```bash
rg -n 'SpectrumWidget|spectrumWidget|m_spectrum' src/gui/MainWindow.cpp | head -20
```

Locate the `connect(m_spectrumWidget, &SpectrumWidget::…)` block(s) or the setup method where signal/slot connections for the spectrum live.

- [ ] **Step 2: Add the new connect**

Following the style of adjacent `connect()` calls (auto-queued, lambda-based):

```cpp
    connect(m_spectrumWidget, &SpectrumWidget::dbmRangeChangeRequested,
            this, [this](float minDbm, float maxDbm) {
                // Push into PanadapterModel so per-band grid storage records
                // the user's choice. Model emits dbmFloor/CeilingChanged →
                // persistSettings() picks it up via its existing observers.
                if (m_panadapterModel) {
                    m_panadapterModel->setdBmFloor(static_cast<int>(minDbm));
                    m_panadapterModel->setdBmCeiling(static_cast<int>(maxDbm));
                }
            });
```

*(Field names `m_spectrumWidget` / `m_panadapterModel` must match the actual names used in MainWindow. If the project's MainWindow uses a non-lambda connect style, follow that instead.)*

- [ ] **Step 3: Build and verify**

```bash
cmake --build build -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)
./build/NereusSDR
```

Expected: change range via strip, quit NereusSDR, relaunch → range restored to what the user last set. Grid lines in the GPU overlay texture refresh on band change because PanadapterModel's `bandChanged` signal already triggers overlay redraw (from Phase 3G-8).

- [ ] **Step 4: Commit**

```bash
git add src/gui/MainWindow.cpp
git commit -m "feat(mainwindow): persist dBm range from strip to PanadapterModel

Connects SpectrumWidget::dbmRangeChangeRequested to
PanadapterModel setdBmFloor/setdBmCeiling so strip
interactions round-trip through the existing per-band grid
storage and AppSettings."
```

---

## Task 9: `DisplayDbmScaleVisible` toggle in Setup → Display

Spec calls for a toggle to hide the strip entirely (default on). Lightweight polish pass: adds one AppSettings key and one checkbox.

**Files:**
- Modify: `src/gui/SpectrumWidget.h` — new `m_dbmScaleVisible` field + setter
- Modify: `src/gui/SpectrumWidget.cpp` — honour the flag in paintEvent, GPU overlay path, and rect-width math
- Modify: `src/gui/setup/DisplaySetupPages.cpp` (or the equivalent Display setup page) — add the checkbox

- [ ] **Step 1: Add the field + setter**

In `SpectrumWidget.h`, near other `m_*Enabled` display-toggle fields:

```cpp
    // Right-edge dBm scale strip visibility. When false, the strip is
    // hidden and the spectrum fills the full widget width.
    bool m_dbmScaleVisible{true};

public:
    void setDbmScaleVisible(bool on);
    bool dbmScaleVisible() const { return m_dbmScaleVisible; }
```

In `SpectrumWidget.cpp`, add:

```cpp
void SpectrumWidget::setDbmScaleVisible(bool on)
{
    if (m_dbmScaleVisible != on) {
        m_dbmScaleVisible = on;
        m_overlayStaticDirty = true;  // GPU path needs to re-render overlay
        update();
    }
}
```

- [ ] **Step 2: Honour the flag in rect-width math**

The strip occupies the rightmost `kDbmStripW` pixels **only when visible**. Introduce a helper in `SpectrumWidget.cpp` (near the top of the impl or inside the class):

```cpp
int SpectrumWidget::effectiveStripW() const
{
    return m_dbmScaleVisible ? kDbmStripW : 0;
}
```

Declare it in the header next to `dbmScaleVisible()`. Then update all six rect-construction sites from Task 3 to use `effectiveStripW()` instead of `kDbmStripW`:

```cpp
    // Before
    QRect specRect(0, 0, w - kDbmStripW, specH);
    // After
    QRect specRect(0, 0, w - effectiveStripW(), specH);
```

Repeat for `wfRect`, `freqRect`, and the 5 other sites.

- [ ] **Step 3: Skip drawDbmScale when hidden**

Wrap both `drawDbmScale(...)` call sites:

```cpp
    if (m_dbmScaleVisible) {
        drawDbmScale(p, specRect);
    }
```

- [ ] **Step 4: Add the Setup → Display checkbox**

Find the existing Display setup page (`src/gui/setup/DisplaySetupPages.cpp` — name may vary; grep for `DisplayGrid` or similar to locate it). Following the existing pattern for other toggles on that page, add:

```cpp
    auto* dbmScaleBox = new QCheckBox("Show dBm scale strip (right edge)", this);
    dbmScaleBox->setToolTip("Show the reference-level scale on the right edge of the spectrum. "
                            "Disable to give the spectrum trace the full widget width.");
    dbmScaleBox->setChecked(
        AppSettings::instance().value("DisplayDbmScaleVisible", "True").toString() == "True");
    connect(dbmScaleBox, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings::instance().setValue("DisplayDbmScaleVisible", on ? "True" : "False");
        if (m_spectrumWidget) {
            m_spectrumWidget->setDbmScaleVisible(on);
        }
    });
```

Drop the widget into the existing layout in the same place neighbours like "Show grid" toggles live.

- [ ] **Step 5: Load on startup**

Where MainWindow or the spectrum-widget owner restores Display settings from AppSettings at startup, add:

```cpp
    m_spectrumWidget->setDbmScaleVisible(
        AppSettings::instance().value("DisplayDbmScaleVisible", "True").toString() == "True");
```

- [ ] **Step 6: Build and verify**

```bash
cmake --build build -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)
./build/NereusSDR
```

Expected: new "Show dBm scale strip" checkbox in Setup → Display, default checked. Unchecking it hides the strip and expands the spectrum to full width; re-checking restores the strip. Setting persists across relaunch.

- [ ] **Step 7: Commit**

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp src/gui/setup/DisplaySetupPages.cpp src/gui/MainWindow.cpp
git commit -m "feat(spectrum): DisplayDbmScaleVisible toggle

New AppSettings key DisplayDbmScaleVisible (default true)
controlling whether the right-edge dBm strip renders. When
hidden, the spectrum fills the full widget width. Toggle
lives in Setup → Display."
```

---

## Task 10: Manual verification matrix

Document the observable behaviors so a future reviewer (or alpha tester) can validate all interactions.

**Files:**
- Create: `docs/architecture/phase3g-rx-epic-a-verification/README.md`

- [ ] **Step 1: Create the verification doc**

```bash
mkdir -p docs/architecture/phase3g-rx-epic-a-verification
```

Create `docs/architecture/phase3g-rx-epic-a-verification/README.md`:

```markdown
# Phase 3G RX Epic — Sub-epic A Verification Matrix

Manual verification for the right-edge dBm scale strip port. Run through
each row; mark PASS / FAIL. FAIL rows block the PR merge.

| # | Interaction | Expected | Result |
|---|---|---|---|
| 1 | Launch NereusSDR, look at spectrum | dBm labels on the RIGHT edge (not left); two arrow triangles at top of strip (▲▼); grid lines, trace, waterfall, freq scale all fill the remaining width and stop cleanly at the strip's left border | |
| 2 | Click the ▲ up arrow | Ref level increases by 10 dB, labels shift up, trace visibly moves down relative to the grid | |
| 3 | Click the ▼ down arrow | Ref level decreases by 10 dB | |
| 4 | Click ▼ repeatedly until range clamps | Dynamic range stops shrinking at 10 dB (minimum floor); further ▼ clicks have no effect | |
| 5 | Drag vertically on the strip body | Ref level pans with the drag; on release, new range persists | |
| 6 | Hover the arrow row without clicking | Cursor becomes pointing-hand | |
| 7 | Hover the strip body without clicking | Cursor becomes size-vertical | |
| 8 | Hover off the strip | Cursor reverts to whatever existing hover logic chooses (crosshair / filter-edge / slice-marker) | |
| 9 | Wheel up over the strip | Dynamic range narrows in 5 dB steps; labels redistribute adaptively (20/10/5/2 dB step boundaries visible) | |
| 10 | Wheel down over the strip | Dynamic range widens in 5 dB steps | |
| 11 | Wheel over the spectrum body | Bandwidth zoom (existing behavior — strip wheel should NOT have stolen this) | |
| 12 | Adjust range via strip, quit, relaunch | Range restored to the last user-set value | |
| 13 | Switch bands while on one range | Per-band storage kicks in — returning to the original band restores its last range (Phase 3G-8 pre-existing behavior — verify strip port didn't break it) | |
| 14 | Change panadapter theme / colour scheme | Strip background stays semi-opaque blue-dark; labels remain legible | |
| 15 | Narrow the widget window width aggressively | Strip stays at 36 px on the right; spectrum squeezes but never overlaps the strip | |
| 16 | Setup → Display → uncheck "Show dBm scale strip" | Strip disappears; spectrum trace, grid, waterfall, and freq scale expand to fill the full widget width | |
| 17 | Re-check the toggle, quit, relaunch | Strip reappears; setting persists across restart (AppSettings `DisplayDbmScaleVisible`) | |

## Regressions to watch

- Filter-edge drag (spectrum body): must still work unchanged
- Divider-bar drag (between spectrum and waterfall): unchanged
- Bandwidth drag on freq-scale bar: unchanged
- VFO tuning: unchanged
- Off-screen slice indicator: unchanged

## Upstream reference

Ported from AetherSDR `main@0cd4559` (2026-04-21) — file [SpectrumWidget.cpp](https://github.com/ten9876/AetherSDR/blob/0cd4559/src/gui/SpectrumWidget.cpp) at lines noted in each commit's inline cites.
```

- [ ] **Step 2: Commit**

```bash
git add docs/architecture/phase3g-rx-epic-a-verification/
git commit -m "docs(verify): 15-row manual matrix for dBm strip port"
```

---

## Task 11: Run full test suite + pre-commit hooks + post-port inventory

Final gate before marking the sub-epic done.

- [ ] **Step 1: Build clean**

```bash
cmake --build build -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

Expected: clean build, no new warnings.

- [ ] **Step 2: Run the full ctest suite**

```bash
ctest --test-dir build --output-on-failure
```

Expected: **all green**, including the new `tst_spectrum_dbm_strip`.

- [ ] **Step 3: Verify pre-commit hooks are happy**

```bash
git log -10 --oneline
```

You should see the 7 commits from Tasks 1–9 on top of the starting branch. Every commit should have passed the `verify-thetis-headers.py` / `verify-inline-tag-preservation.py` / `check-new-ports.py` hook chain as part of its commit (they run on each `git commit`).

- [ ] **Step 4: Confirm no stray `kDbmStripW` left-edge assumptions remain**

```bash
rg -n 'mx < kDbmStripW|x < kDbmStripW|kDbmStripW, 0|QRect\(0, 0, kDbmStripW' src/
```

Expected: **no hits**. Every strip-related coordinate now derives from `width() - kDbmStripW` or `stripRect(specRect, kDbmStripW)`.

- [ ] **Step 5: Manual verification against docs/architecture/phase3g-rx-epic-a-verification/README.md**

Walk the 15-row matrix yourself (or hand off to the user). Every row must pass before the PR opens.

---

## Completion

When all tasks above are committed and the verification matrix is 15/15 PASS:

1. The sub-epic branch is ready for PR against `main`.
2. PR title: `feat(spectrum): port AetherSDR right-edge dBm scale strip (Phase 3G RX Epic, sub-epic A)`
3. PR body: link the verification README, paste the matrix result, attach before/after screenshots of the spectrum.
4. Do NOT open the PR via `gh` without explicit user approval (per `feedback_no_public_posts_without_review` memory).

Sub-epic B (Noise Blanker port) is the next unit of work; its plan is written separately via `writing-plans` after sub-epic A merges.
