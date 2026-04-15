# Phase 3G-10 — RX DSP Parity + AetherSDR Flag Port — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Spec:** [`docs/architecture/2026-04-15-phase3g10-rx-dsp-flag-design.md`](./2026-04-15-phase3g10-rx-dsp-flag-design.md)
**Branch:** `feature/phase3g10-rx-dsp-flag` (already created at commit `8c0c43d`)
**Goal:** Port the AetherSDR `VfoWidget` visual shell into NereusSDR with faithful styling, then wire every RX-side DSP NYI stub through `SliceModel → RxChannel → WDSP` with per-slice-per-band bandstack persistence.
**Architecture:** Two sequential stages on a single branch. Stage 1 rebuilds `VfoWidget` against AetherSDR's `VfoWidget.cpp` while leaving all new `SliceModel` setters as stubs. Stage 2 fills in the WDSP wiring one feature-vertical slice at a time. Each commit is self-contained, compiles clean, passes existing tests, and is small enough to review in isolation.
**Tech Stack:** C++20, Qt6 (Widgets + Test), WDSP (C sources in `third_party/wdsp/src/`), `AppSettings` XML persistence, GoogleTest for in-process WDSP unit tests, qtest for widget tests.

---

## 0. Pre-flight checks (one-time, at the start of the plan)

- [ ] **0.1** Verify we are on the correct branch:
  ```
  cd ~/NereusSDR && git status
  ```
  Expected: `On branch feature/phase3g10-rx-dsp-flag`. If not, `git checkout feature/phase3g10-rx-dsp-flag`.

- [ ] **0.2** Verify the local AetherSDR clone is on `main` and clean:
  ```
  cd ~/AetherSDR && git status && git log --oneline -1
  ```
  Expected: clean tree, `origin/main` up to date. AetherSDR is the authoritative *visual* source for Stage 1. If the clone is missing, `git clone git@github.com:ten9876/AetherSDR ~/AetherSDR` before proceeding.

- [ ] **0.3** Verify the WDSP C sources referenced by Stage 2 all exist:
  ```
  cd ~/NereusSDR && for f in wcpAGC.h wcpAGC.c emnr.h emnr.c snb.c amsq.h amsq.c fmsq.h fmsq.c ssql.h ssql.c apfshadow.h apfshadow.c anr.h anr.c anf.h anf.c matchedCW.h; do test -f third_party/wdsp/src/$f || echo MISSING $f; done
  ```
  Expected: zero MISSING lines. All required headers exist.

- [x] **0.4** Verify the Thetis clone is *full* (has `Project Files/Source/Console/`). **Resolved during 2026-04-15 handoff** with a fresh clone at `/Users/j.j.boyd/Thetis`. Sanity check:
  ```
  ls ~/Thetis/Project\ Files/Source/Console/console.cs \
     ~/Thetis/Project\ Files/Source/Console/dsp.cs \
     ~/Thetis/Project\ Files/Source/Console/setup.cs
  ```
  Expected: all three files present. **Filename correction**: the C# P/Invoke declarations are in `dsp.cs`, not `wdsp.cs` as earlier drafts said. Stage 1 does not need Thetis Console; this gate must still pass before Stage 2 task S2.1.1.

- [ ] **0.5** Build clean baseline:
  ```
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build -j
  ```
  Expected: build succeeds. Records the baseline for regression diffs.

- [ ] **0.6** Run existing test suite to record the baseline pass count:
  ```
  ctest --test-dir build --output-on-failure 2>&1 | tee /tmp/nereus-baseline-tests.log | tail -5
  ```
  Record the pass count. Every Stage 1/2 commit must leave this count ≥ baseline.

---

## File Structure Overview

### New files (Stage 1)

| Path | Responsibility |
|---|---|
| `src/gui/widgets/VfoStyles.h` | Stylesheet constants + color `constexpr`s, verbatim from AetherSDR `VfoWidget.cpp` lines 134–177 with source line citations. Single header, zero runtime. |
| `src/gui/widgets/VfoLevelBar.h` | Declaration of the S-meter widget: QWidget subclass, `setValue(float dbm)` slot, fixed min-height. |
| `src/gui/widgets/VfoLevelBar.cpp` | `paintEvent` ports AetherSDR `LevelBar::paintEvent` (`src/gui/VfoWidget.cpp:43-61`) and adds a S-unit tick strip rendered above the bar. |
| `src/gui/widgets/ScrollableLabel.h` | NereusSDR native `QStackedWidget` subclass with wheel-step + double-click inline edit (`label \| QLineEdit`). Qt skeleton pattern informed by AetherSDR's trivial `ScrollableLabel` in `GuardedSlider.h:81-100`. |
| `src/gui/widgets/ScrollableLabel.cpp` | Implementation. |
| `src/gui/widgets/GuardedComboBox.h` | NereusSDR native `QComboBox` subclass (Qt skeleton pattern informed by AetherSDR `GuardedSlider.h:56-76`): wheel events only when popup visible, honors `ControlsLock`. |
| `src/gui/widgets/VfoModeContainers.h` | Three `QWidget` subclasses: `FmOptContainer`, `DigOffsetContainer`, `RttyMarkShiftContainer`. Each with `setSlice(SliceModel*)` + `syncFromSlice()`. (CW autotune cut from Stage 1 — see §S1.5 amendment.) |
| `src/gui/widgets/VfoModeContainers.cpp` | Implementations. |
| `src/gui/widgets/ResetSlider.h` | Ported utility (AetherSDR `VfoWidget.cpp:68-76`). |
| `src/gui/widgets/CenterMarkSlider.h` | Ported utility (AetherSDR `VfoWidget.cpp:79-94`). |
| `src/gui/widgets/TriBtn.h` | Ported utility (AetherSDR `VfoWidget.cpp:97-129`) — used by RIT/XIT zero buttons and FM OPT up/down. |
| `tests/tst_vfo_level_bar.cpp` | qtest: dBm → fill-fraction mapping, tick-strip layout math. |
| `tests/tst_scrollable_label.cpp` | qtest: wheel-step math, inline-edit parse, disable-on-lock. |
| `tests/tst_vfo_mode_containers.cpp` | qtest: each container's `syncFromSlice` populates correctly. |
| `tests/tst_vfo_tooltip_coverage.cpp` | qtest: walks `VfoWidget` children, asserts every enabled `QAbstractButton` / `QSlider` / `QComboBox` has non-empty `toolTip()`. |

### Rewritten files (Stage 1)

| Path | Change |
|---|---|
| `src/gui/widgets/VfoWidget.h` | Expand header member set to match new class map (see spec §7.2). Replace existing member set wholesale. |
| `src/gui/widgets/VfoWidget.cpp` | Full rewrite against AetherSDR layout. Currently 1014 lines → expected ~2400–2600 lines post-Stage 1. Filename preserved. |
| `src/models/SliceModel.h` | Additive: declare new `Q_PROPERTY`s, setters, and signals. Stage 1 bodies are pure state-and-emit stubs. |
| `src/models/SliceModel.cpp` | Additive: stub bodies. No `RxChannel` calls in Stage 1. |
| `src/core/WdspTypes.h` | Additive: enum extensions. |
| `CMakeLists.txt` | Add new source files to the `NereusSDRObjs` list and the new test executables. |

### Modified files (Stage 2)

| Path | Change |
|---|---|
| `src/core/RxChannel.h` | Declare new WDSP-calling setters (one per feature slice). All new parameters backed by `std::atomic`. |
| `src/core/RxChannel.cpp` | Implement new WDSP-calling setters. Each body follows the `// From Thetis <file>:<line>` / `// WDSP third_party/wdsp/src/<file>:<line>` citation pattern. |
| `src/models/SliceModel.cpp` | Replace Stage 1 stubs with real bodies that forward to `RxChannel`. |
| `src/gui/widgets/VfoWidget.cpp` | Remove `NyiOverlay::markNyi()` on the buttons corresponding to each landed slice; wire live signals to `SliceModel`. |
| `src/gui/applets/RxApplet.cpp` | Remove NYI guards on AGC-T, squelch, mute, pan, RIT, XIT, frequency lock as each slice lands. |
| `src/core/AppSettings.h` | No code change; documentation update only (new key namespace). |

### Manual verification + documentation (Phase close)

| Path | Responsibility |
|---|---|
| `docs/architecture/phase3g10-verification/README.md` | Manual A/B matrix, one row per control × {Hermes Lite 2, ANAN-G2}. |
| `docs/architecture/phase3g10-verification/rx-dsp-grid.md` | DSP tab per-button smoke test. |
| `docs/architecture/phase3g10-verification/mode-containers.md` | FM/DIG/RTTY/CW container smoke test. |
| `docs/architecture/phase3g10-verification/persistence.md` | Per-slice-per-band restore-on-band-change matrix. |
| `CHANGELOG.md` | Append 3G-10 entry. |
| `CLAUDE.md` | Update "Current Phase" table with 3G-10 row + design/plan doc links. |

---

# Stage 1 — VfoWidget shell port (10 commits)

## Commit S1.1 — VfoStyles.h

### Task S1.1 — Port AetherSDR style constants

**Files:**
- Create: `src/gui/widgets/VfoStyles.h`
- Source: `~/AetherSDR/src/gui/VfoWidget.cpp:134-177` (the `kBgStyle`, `kFlatBtn`, `kTabLblNormal`, `kTabLblActive`, `kDisabledBtn`, `kDspToggle`, `kModeBtn`, `kSliderStyle`, `kLabelStyle` statics)

- [ ] **S1.1.1** Read `~/AetherSDR/src/gui/VfoWidget.cpp:134-177` to load the exact style strings into context. Copy the nine QString constants verbatim.

- [ ] **S1.1.2** Create `src/gui/widgets/VfoStyles.h`. Use `#pragma once` and place everything inside `namespace NereusSDR {`. Convert each AetherSDR `static const QString kFoo = "..."` into `inline constexpr QStringView kFoo { u"..." }`. Reason: `constexpr QStringView` is zero-overhead, header-safe, and matches NereusSDR's "no `#define` macros for constants" style rule. Each constant must carry a `// From AetherSDR src/gui/VfoWidget.cpp:NN — verbatim port` comment on the line above.

- [x] **S1.1.3** Add color constants drawn from the same region (Qt6 `QColor` is not `constexpr`, so `inline const QColor`). Required set — nine colors:
  - `kHdrRxBlue = QColor(0x44,0x88,0xff)` — RX antenna label
  - `kHdrTxRed = QColor(0xff,0x44,0x44)` — TX antenna label
  - `kFilterCyan = QColor(0x00,0xc8,0xff)` — filter-width label
  - `kSliceBadgeBlue = QColor(0x00,0x70,0xc0)` — slice letter badge background
  - `kMeterCyan = QColor(0x00,0xb4,0xd8)` — S-meter fill below S9
  - `kMeterGreen = QColor(0x00,0xd8,0x60)` — S-meter fill at/above S9
  - `kLabelMuted = QColor(0x68,0x88,0xa0)` — muted label text
  - `kBodyText = QColor(0xc8,0xd8,0xe8)` — default body text
  - `kBgDark = QColor(0x10,0x10,0x1c)` — flag dark bg, consumed by `VfoLevelBar::paintEvent` in S1.2

- [ ] **S1.1.4** Add `src/gui/widgets/VfoStyles.h` to `CMakeLists.txt` under `NereusSDRObjs`. It is a header-only file, but adding it to the sources list ensures it is moc-scanned if Qt ever needs to. (It doesn't now; cheap insurance.)

- [ ] **S1.1.5** Build:
  ```
  cmake --build build -j
  ```
  Expected: succeeds. VfoStyles.h is used by nothing yet so zero behavior change.

- [ ] **S1.1.6** Commit:
  ```
  git add src/gui/widgets/VfoStyles.h CMakeLists.txt
  git commit -S -m "phase3g10(style): add VfoStyles.h with verbatim AetherSDR style constants

All style strings, color constants, and utility constexprs for the
VFO flag, ported verbatim from AetherSDR src/gui/VfoWidget.cpp:134-177.
Header-only, zero runtime. Consumed by VfoWidget rewrite in later commits.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
  ```

---

## Commit S1.2 — VfoLevelBar widget

### Task S1.2 — S-meter bar with S-unit ticks above

**Files:**
- Create: `src/gui/widgets/VfoLevelBar.h`, `src/gui/widgets/VfoLevelBar.cpp`
- Create: `tests/tst_vfo_level_bar.cpp`
- Source: `~/AetherSDR/src/gui/VfoWidget.cpp:38-64` (AetherSDR `LevelBar::paintEvent`)
- Per spec choice C: ticks rendered *above* the bar (not through it).

**Amendment (post-review, 2026-04-15):** `minimumSizeHint` raised from `{120, 22}` to `{150, 22}` — at 120px the S1 tick label clipped 1px off-screen left. Code-quality review finding applied as a fixup commit on S1.2.

- [ ] **S1.2.1** Write the failing test `tests/tst_vfo_level_bar.cpp`:

```cpp
#include <QtTest/QtTest>
#include "gui/widgets/VfoLevelBar.h"
using namespace NereusSDR;

class TestVfoLevelBar : public QObject {
    Q_OBJECT
private slots:
    void fillFractionAtFloor() {
        VfoLevelBar bar; bar.resize(200, 24);
        bar.setValue(-130.0f);
        QCOMPARE(bar.fillFraction(), 0.0);
    }
    void fillFractionAtCeiling() {
        VfoLevelBar bar; bar.resize(200, 24);
        bar.setValue(-20.0f);
        QCOMPARE(bar.fillFraction(), 1.0);
    }
    void fillFractionAtS9() {
        VfoLevelBar bar; bar.resize(200, 24);
        bar.setValue(-73.0f);  // S9 boundary
        QCOMPARE(bar.fillFraction(), (-73.0 - -130.0) / (-20.0 - -130.0));
    }
    void clampsBelowFloor() {
        VfoLevelBar bar; bar.setValue(-200.0f);
        QCOMPARE(bar.fillFraction(), 0.0);
    }
    void clampsAboveCeiling() {
        VfoLevelBar bar; bar.setValue(10.0f);
        QCOMPARE(bar.fillFraction(), 1.0);
    }
    void colorSwitchesAtS9() {
        VfoLevelBar bar;
        bar.setValue(-74.0f); QCOMPARE(bar.isAboveS9(), false);
        bar.setValue(-73.0f); QCOMPARE(bar.isAboveS9(), true);
    }
};
QTEST_MAIN(TestVfoLevelBar)
#include "tst_vfo_level_bar.moc"
```

- [ ] **S1.2.2** Write `src/gui/widgets/VfoLevelBar.h`:

```cpp
#pragma once
#include <QWidget>
namespace NereusSDR {
class VfoLevelBar : public QWidget {
    Q_OBJECT
public:
    explicit VfoLevelBar(QWidget* parent = nullptr);
    void setValue(float dbm);   // slot-safe; schedules update()
    float value() const { return m_value; }
    double fillFraction() const;  // 0..1, clamped
    bool isAboveS9() const { return m_value >= -73.0f; }
    QSize sizeHint() const override { return {180, 22}; }
    QSize minimumSizeHint() const override { return {150, 22}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    float m_value{-130.0f};
    static constexpr float kFloorDbm   = -130.0f;
    static constexpr float kCeilingDbm =  -20.0f;
    static constexpr float kS9Dbm      =  -73.0f;
};
}
```

- [ ] **S1.2.3** Write `src/gui/widgets/VfoLevelBar.cpp`:

```cpp
#include "VfoLevelBar.h"
#include "VfoStyles.h"
#include <QPainter>
#include <algorithm>
namespace NereusSDR {
// From AetherSDR src/gui/VfoWidget.cpp:38-64 — LevelBar port,
// extended with an S-unit tick strip above the bar (NereusSDR native).
VfoLevelBar::VfoLevelBar(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}
void VfoLevelBar::setValue(float dbm) {
    if (dbm == m_value) { return; }
    m_value = dbm;
    update();
}
double VfoLevelBar::fillFraction() const {
    const double v = std::clamp(static_cast<double>(m_value),
                                static_cast<double>(kFloorDbm),
                                static_cast<double>(kCeilingDbm));
    return (v - kFloorDbm) / (kCeilingDbm - kFloorDbm);
}
void VfoLevelBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    const int tickH = 8;
    const QRect barRect(0, tickH, width(), height() - tickH);

    // ── Tick strip above the bar ───────────────────────────────────────
    // Tick labels for S1, S3, S5, S7, S9, +20, +40
    static constexpr float kTickDbm[]    = {-121, -109, -97, -85, -73, -53, -33};
    static constexpr const char* kTickTxt[] = {"S1","3","5","7","9","+20","+40"};
    p.setPen(kLabelMuted);
    QFont f = p.font(); f.setPixelSize(8); p.setFont(f);
    for (int i = 0; i < 7; ++i) {
        double frac = (kTickDbm[i] - kFloorDbm) / (kCeilingDbm - kFloorDbm);
        int x = static_cast<int>(frac * (width() - 1));
        p.drawLine(x, tickH - 2, x, tickH);  // tick mark
        p.drawText(QRect(x - 10, 0, 20, tickH - 2),
                   Qt::AlignCenter, QString::fromLatin1(kTickTxt[i]));
    }

    // ── Bar itself (ported from AetherSDR LevelBar::paintEvent) ────────
    p.fillRect(barRect, QColor(0x10, 0x10, 0x1c));
    p.setPen(QColor(0x30, 0x40, 0x50));
    p.drawRect(barRect.adjusted(0, 0, -1, -1));
    const int fillW = static_cast<int>(fillFraction() * (barRect.width() - 2));
    if (fillW > 0) {
        const QColor color = isAboveS9() ? kMeterGreen : kMeterCyan;
        p.fillRect(barRect.x() + 1, barRect.y() + 1,
                   fillW, barRect.height() - 2, color);
    }
}
}
```

- [ ] **S1.2.4** Add `src/gui/widgets/VfoLevelBar.{h,cpp}` to `CMakeLists.txt` under `NereusSDRObjs`.

- [ ] **S1.2.5** Add `tests/tst_vfo_level_bar.cpp` to `CMakeLists.txt` as a new qtest executable linked against `NereusSDRObjs` + `Qt6::Test`.

- [ ] **S1.2.6** Run the test:
  ```
  cmake --build build -j && ctest --test-dir build -R tst_vfo_level_bar --output-on-failure
  ```
  Expected: PASS on all 6 cases.

- [ ] **S1.2.7** Commit:
  ```
  git add src/gui/widgets/VfoLevelBar.{h,cpp} tests/tst_vfo_level_bar.cpp CMakeLists.txt
  git commit -S -m "phase3g10(meter): add VfoLevelBar widget + unit test

Port of AetherSDR LevelBar::paintEvent (src/gui/VfoWidget.cpp:38-64)
with an S-unit tick strip rendered above the bar (S1/3/5/7/9/+20/+40).
Cyan below S9 (-73 dBm), green at/above. 6-case unit test covers
floor/ceiling clamping, S9 color boundary, fill fraction math.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
  ```

---

## Commit S1.3 — ScrollableLabel

### Task S1.3 — NereusSDR native ScrollableLabel widget informed by AetherSDR patterns

**Files:**
- Create: `src/gui/widgets/ScrollableLabel.h`, `.cpp`
- Create: `tests/tst_scrollable_label.cpp`
- Reference: `~/AetherSDR/src/gui/GuardedSlider.h:81-100` — base Qt skeleton pattern (QLabel + wheel event handler). NereusSDR's version is a richer native widget, not a port.

**Amendment (post-review, 2026-04-15):** Reframed from a "port" to a **NereusSDR native widget informed by AetherSDR patterns**. AetherSDR's `ScrollableLabel` (`~/AetherSDR/src/gui/GuardedSlider.h:81-100`) is a trivial ~20-line `QLabel` that emits `scrolled(±1)` on wheel. NereusSDR's version is a richer composite (QStackedWidget label+editor, range/step/format, inline edit, valueChanged signal, parseValue) per user-approved source-first exception for control surfaces (see `feedback_source_first_ui_vs_dsp.md`). Source-first still fully applies to DSP/radio behavior the widget binds to in Stage 2.

- [ ] **S1.3.1** Read `~/AetherSDR/src/gui/GuardedSlider.h:81-100` to see the base `ScrollableLabel` pattern (QLabel + wheel event handler). NereusSDR's widget will extend this with label/editor composition, range clamping, inline edit, and typed value state.

- [ ] **S1.3.2** Write `tests/tst_scrollable_label.cpp`:

```cpp
#include <QtTest/QtTest>
#include "gui/widgets/ScrollableLabel.h"
using namespace NereusSDR;

class TestScrollableLabel : public QObject {
    Q_OBJECT
private slots:
    void wheelIncrementsByStep() {
        ScrollableLabel lbl;
        lbl.setRange(-10000, 10000);
        lbl.setStep(10);
        lbl.setValue(0);
        QSignalSpy spy(&lbl, &ScrollableLabel::valueChanged);
        QWheelEvent up(QPointF(5,5), QPointF(), QPoint(), QPoint(0, 120),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&lbl, &up);
        QCOMPARE(lbl.value(), 10);
        QCOMPARE(spy.count(), 1);
    }
    void wheelDecrementsByStep() {
        ScrollableLabel lbl;
        lbl.setRange(-10000, 10000); lbl.setStep(10); lbl.setValue(0);
        QWheelEvent down(QPointF(5,5), QPointF(), QPoint(), QPoint(0, -120),
                         Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&lbl, &down);
        QCOMPARE(lbl.value(), -10);
    }
    void clampsAtMin() {
        ScrollableLabel lbl;
        lbl.setRange(-100, 100); lbl.setStep(50); lbl.setValue(-80);
        QWheelEvent down(QPointF(), QPointF(), QPoint(), QPoint(0, -120),
                         Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&lbl, &down);
        QCOMPARE(lbl.value(), -100);
    }
    void parsesInlineEdit() {
        ScrollableLabel lbl;
        lbl.setRange(-10000, 10000);
        QCOMPARE(lbl.parseValue("+120"), 120);
        QCOMPARE(lbl.parseValue("-3500"), -3500);
        QCOMPARE(lbl.parseValue("abc"), std::optional<int>{});
    }
};
QTEST_MAIN(TestScrollableLabel)
#include "tst_scrollable_label.moc"
```

- [ ] **S1.3.3** Write `src/gui/widgets/ScrollableLabel.h` — NereusSDR native widget. Header responsibilities:
  - Derives from `QStackedWidget` internally (label + QLineEdit).
  - `setRange(int min, int max)`, `setStep(int)`, `setValue(int)`, `value()`, `setFormat(std::function<QString(int)>)` for custom display (e.g. `+0.120 kHz`).
  - Signal: `valueChanged(int)`.
  - Static helper `parseValue(const QString&)` returning `std::optional<int>` for test hookability.
  - Double-click switches to inline `QLineEdit`; Enter commits, Esc cancels.
  - Wheel step sign: up = increment, down = decrement, clamp at range edges.
  - Tooltip: passthrough from parent.

- [ ] **S1.3.4** Write the matching `ScrollableLabel.cpp`. All stylesheet calls use `VfoStyles.h`.

- [ ] **S1.3.5** Add to `CMakeLists.txt` (sources + test exe).

- [ ] **S1.3.6** Build + run:
  ```
  cmake --build build -j && ctest --test-dir build -R tst_scrollable_label --output-on-failure
  ```
  Expected: PASS on all 4 cases.

- [ ] **S1.3.7** Commit:
  ```
  git add src/gui/widgets/ScrollableLabel.{h,cpp} tests/tst_scrollable_label.cpp CMakeLists.txt tests/CMakeLists.txt
  git commit -S -m "phase3g10(widget): add native ScrollableLabel widget + unit test

NereusSDR native composite widget (QStackedWidget with QLabel + QLineEdit)
for editable numeric displays: RIT/XIT/DIG-offset/FM-offset on the VFO
flag. Wheel step with range clamp, double-click inline edit with Enter
commit / Esc cancel, custom format callback. 4-case unit test covers
wheel up/down, min clamp, parseValue.

Qt skeleton patterns informed by AetherSDR's trivial ScrollableLabel
(GuardedSlider.h:81-100); NereusSDR's version is a richer native widget
per source-first exception for control surfaces (Thetis source-first
still applies to the DSP/radio behavior wired in Stage 2).

Plan §S1.3 reframed from 'port' to 'native widget informed by AetherSDR
patterns' with amendment note.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
  ```

---

## Commit S1.4 — Small utility widgets (ResetSlider, CenterMarkSlider, TriBtn)

### Task S1.4 — Port the three AetherSDR slider/button utilities

**Files:**
- Create: `src/gui/widgets/ResetSlider.h` (header-only, inline impl OK)
- Create: `src/gui/widgets/CenterMarkSlider.h`
- Create: `src/gui/widgets/TriBtn.h`
- Sources:
  - `~/AetherSDR/src/gui/VfoWidget.cpp:68-76` — `ResetSlider`
  - `~/AetherSDR/src/gui/VfoWidget.cpp:79-94` — `CenterMarkSlider`
  - `~/AetherSDR/src/gui/VfoWidget.cpp:97-129` — `TriBtn`

These three are small enough to live inline in headers.

- [ ] **S1.4.1** Read the three source ranges into context.

- [ ] **S1.4.2** For each: create a header-only file in `src/gui/widgets/` containing the verbatim port inside `namespace NereusSDR`. Source comment `// From AetherSDR src/gui/VfoWidget.cpp:NN-MM — port` above each class. `ResetSlider` must inherit from the existing `GuardedSlider` (grep `src/gui/` for its current location; AetherSDR uses `src/gui/GuardedSlider.h` — NereusSDR likely has the same or needs the port). If NereusSDR does not already have `GuardedSlider`, port that too into the same commit.

- [ ] **S1.4.3** Build:
  ```
  cmake --build build -j
  ```
  Expected: clean (no consumers yet).

- [ ] **S1.4.4** Commit:
  ```
  git add src/gui/widgets/ResetSlider.h src/gui/widgets/CenterMarkSlider.h src/gui/widgets/TriBtn.h CMakeLists.txt
  git commit -S -m "phase3g10(widget): add ResetSlider, CenterMarkSlider, TriBtn utilities

Header-only ports from AetherSDR src/gui/VfoWidget.cpp:68-129.
Used by the VfoWidget rewrite for AF gain, pan, APF tune, and
RIT/XIT zero buttons.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
  ```

---

## Commit S1.5 — VfoModeContainers (FM / DIG / RTTY)

### Task S1.5 — Three mode-specific container widgets (NereusSDR native, informed by AetherSDR)

**Files:**
- Create: `src/gui/widgets/GuardedComboBox.h` (deferred from S1.4 per that task's scope lock)
- Create: `src/gui/widgets/VfoModeContainers.h`, `.cpp`
- Create: `tests/tst_vfo_mode_containers.cpp`
- Informed by: `~/AetherSDR/src/gui/VfoWidget.cpp:996-1300` (mode sub-widget blocks)

**Amendment (post-review, 2026-04-15):** Reframed from a "verbatim port" task to a **NereusSDR native widget commit informed by AetherSDR patterns**. Scouting AetherSDR revealed three blockers that forced this reframe:

1. **`CwAutotuneContainer` cut from Stage 1.** Neither AetherSDR nor Thetis has a CW-autotune container — AetherSDR's closest code is dynamic button insertion in `updateTabsForMode()` (not a container); Thetis has no CW autotune feature at all (`EnableApolloAutoTune` matches are for the Apollo antenna tuner, not matched-filter auto-adjust). Deferred to a future phase with an explicit native design. S1.5 ships **three** containers: FM / DIG / RTTY.

2. **SliceModel API gap.** AetherSDR's containers bound to properties our S1.6 stubs didn't have in the right shape (single `digOffsetHz` vs per-sideband, `fmSimplex` bool vs `FmTxMode` enum). Resolved by a third S1.6 fixup commit (`c753836`) that split `digOffsetHz` → `diglOffsetHz`+`diguOffsetHz` and replaced `fmSimplex{bool}` with `fmTxMode{FmTxMode enum}`, both Thetis-authoritative.

3. **`ScrollableLabel` is different-shaped.** AetherSDR's `ScrollableLabel` is a trivial 20-line `QLabel` emitting `scrolled(±1)`; NereusSDR's (S1.3) is a richer composite emitting `valueChanged(int)`. Container connect lambdas target our API, not AetherSDR's. Also ports `GuardedComboBox` here (deferred from S1.4 per that task's scope lock).

Per `feedback_source_first_ui_vs_dsp.md`: control surfaces are native; Thetis source-first still applies to DSP behavior the containers will bind to in Stage 2 (routing, scaling, enum-to-WDSP translation).

Build the four containers as independent standalone widgets. This commit does *not* embed them in `VfoWidget`; it only ships the widgets and their tests.

- [x] **S1.5.1** Scouted AetherSDR `VfoWidget.cpp` for mode container blocks. Documented controls for FM, DIG, and RTTY. CW autotune dropped (see amendment above). Controls for FM:
  - `m_toneModeCmb` — `GuardedComboBox` (Off / CTCSS Encode / CTCSS Decode / CTCSS Enc+Dec)
  - `m_toneValueCmb` — `GuardedComboBox` populated with 41-entry ITU CTCSS tone table
  - `m_offsetKhzSpin` — `QSpinBox` (0–10000 kHz, suffix " kHz", step 50)
  - `m_txLowBtn` / `m_simplexBtn` / `m_txHighBtn` — mutually exclusive `QPushButton` (kDspToggle)
  - `m_revBtn` — independent `QPushButton` (kDspToggle), checkable

- [x] **S1.5.2** Wrote `src/gui/widgets/VfoModeContainers.h` — three NereusSDR native containers:
  `FmOptContainer`, `DigOffsetContainer`, `RttyMarkShiftContainer`. All use `setObjectName` on child
  widgets for test-friendly `findChild` discovery. `GuardedComboBox` also written in `GuardedComboBox.h`.

- [x] **S1.5.3** Wrote implementations in `VfoModeContainers.cpp`:
  - `FmOptContainer::buildUi()` — 41-entry ITU CTCSS table, kDspToggle style, `QSignalBlocker` in `syncFromSlice()`.
  - `DigOffsetContainer` — routes read/write through `dspMode()` to `diglOffsetHz`/`diguOffsetHz`.
  - `RttyMarkShiftContainer` — Mark range 1000–3500 Hz step 25, Shift range 50–1000 Hz step 5.
  - All lambdas guard on `if (!m_slice) return;`.

- [x] **S1.5.4** Wrote `tests/tst_vfo_mode_containers.cpp` — 9 test cases covering tone mode,
  tone value, FM offset, TX direction, FM reverse, DIG offset routing (DIGL/DIGU), RTTY mark, RTTY shift.

- [x] **S1.5.5** Built + tested. All 21 tests pass; `tst_vfo_mode_containers` PASS;
  `tst_p1_loopback_connection` fails as before (pre-existing).

- [x] **S1.5.6** Committed (see commit message template below):
  ```
  git add src/gui/widgets/GuardedComboBox.h \
          src/gui/widgets/VfoModeContainers.{h,cpp} \
          tests/tst_vfo_mode_containers.cpp \
          CMakeLists.txt tests/CMakeLists.txt \
          docs/architecture/phase3g10-rx-dsp-flag-plan.md
  git commit -S -m "phase3g10(widget): add VfoModeContainers (FM/DIG/RTTY) + GuardedComboBox

Three NereusSDR native mode containers informed by AetherSDR Qt
skeleton patterns, each bound to the S1.6 SliceModel stub API:

- FmOptContainer: CTCSS mode + value combos, FM repeater offset
  spinbox (kHz), 3-way TX direction (Low/Simplex/High) + independent
  Reverse toggle.
- DigOffsetContainer: single offset control routed by dspMode() to
  diglOffsetHz or diguOffsetHz (Thetis splits these per-sideband).
- RttyMarkShiftContainer: Mark (2295 default) and Shift (170 default)
  scrollable labels + nudge buttons.

CW autotune container dropped from Stage 1 scope — no Thetis source
exists (only Apollo antenna tuner matches), deferred to a future phase
with explicit native design.

Also ports GuardedComboBox from AetherSDR GuardedSlider.h (deferred
from S1.4 per that task's scope lock) — wheel events only when popup
is visible, honors ControlsLock for mouse press.

Tests exercise each container's setSlice/syncFromSlice path and the
DIG container's mode-based routing. Not yet embedded in VfoWidget
(done in S1.7).

Plan §S1.5 amended with the source-first reframe, CW drop, and
SliceModel API resolution story.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
  ```

---

## Commit S1.6 — SliceModel stub extensions (lands *before* S1.5 per reorder note)

### Task S1.6 — Declare all new DSP properties as emit-only stubs

**Files:**
- Modify: `src/models/SliceModel.h`
- Modify: `src/models/SliceModel.cpp`
- Modify: `src/core/WdspTypes.h`

Stage 1 SliceModel stubs: property storage + emit changed signal, **no** `RxChannel` calls. Stage 2 commits replace each stub body with the real WDSP forwarding.

**Amendment (post-review, 2026-04-15):** Two default values reconciled against Thetis source-of-truth after the implementer flagged drift:

- `m_fmCtcssValueHz`: plan specified 88.5 Hz; Thetis `console.cs:40500` and `radio.cs:2899` set `ctcss_freq = 100.0`. Corrected to 100.0 Hz.
- `m_rttyMarkHz`: plan specified 2125 Hz; Thetis `setup.designer.cs:40635` labels `udDSPRX1DollyF1 = 2295` as "RTTY MARK" and `udDSPRX1DollyF0 = 2125` as "RTTY SPACE". The plan's value was the SPACE tone misfiled as the mark. Corrected to 2295 Hz; `rttyShiftHz = 170` is unchanged.
- `m_agcHang`: plan specified 0 ms; Thetis `radio.cs:1057` sets `rx_agc_hang = 250 ms`. Corrected to 250 ms. This correction was silently applied during the initial S1.6 commit (`9d2d50f`) along with the other reconciliations; this fixup amends the plan text to match.

All three corrections are pure DSP-constant source-first fixes per CLAUDE.md / `feedback_source_first_exceptions.md`. Property names stay unchanged (user-approved Option A).

**Amendment (post-review, 2026-04-15, round 3 — API expansion):** Three S1.6 API surface changes reconciled against Thetis source-of-truth after scouting for Task S1.5 revealed that the original property set couldn't express the semantics the mode containers need:

- `digOffsetHz` split into `diglOffsetHz` + `diguOffsetHz`. Thetis `console.cs:14672` (`DIGLClickTuneOffset`) and `console.cs:14637` (`DIGUClickTuneOffset`) are independent `int` properties, one per digital-sideband variant. The single `digOffsetHz` in the original S1.6 spec was a drift.
- `fmSimplex{bool}` replaced with `fmTxMode{FmTxMode}` enum. Thetis `enums.cs:380` declares `FMTXMode { High = 0, Simplex = 1, Low = 2 }` (order chosen for memory form per Thetis comment). Thetis `console.cs:20873` uses `FMTXMode { High, Simplex, Low }` — a 3-state enum. A single bool can't express it. New enum class `FmTxMode` added to `WdspTypes.h`. Note: Thetis declares `High = 0`, `Simplex = 1` (implicit), `Low = 2` (implicit) — the task instructions listed a different order; Thetis source-of-truth prevails.
- `fmReverse{bool}` kept — it's a separate flag (reverse-listen toggle), not part of the repeater-direction enum.

These are pure DSP-constant source-first reconciliations per CLAUDE.md / `feedback_source_first_exceptions.md`. The changes unblock Task S1.5 (VfoModeContainers) to target a Thetis-authoritative API surface.

- [ ] **S1.6.1** Extend `src/core/WdspTypes.h` with new enums:

```cpp
enum class NrMode   : int { Off = 0, ANR = 1, EMNR = 2 };
enum class NbMode   : int { Off = 0, NB1 = 1, NB2 = 2 };
enum class SquelchMode : int { Off, Voice, AM, FM };
enum class AgcHangMode : int { Off, Fast, Med, Slow };  // Thetis AGC hang classes
enum class FmTxMode : int { High = 0, Simplex = 1, Low = 2 };  // From Thetis enums.cs:380 — FMTXMode; added in round-3 fixup
```

These are declarations only. Build to confirm nothing references them yet.

- [ ] **S1.6.2** Extend `src/models/SliceModel.h` with the full NYI list from spec §6.2. Each property follows this shape (example for `agcThreshold`):

```cpp
Q_PROPERTY(int agcThreshold READ agcThreshold WRITE setAgcThreshold NOTIFY agcThresholdChanged)
// ...
int agcThreshold() const { return m_agcThreshold; }
void setAgcThreshold(int dBu);
signals:
void agcThresholdChanged(int dBu);
// ...
private:
int m_agcThreshold{-20};
```

Replicate this shape for each NYI row in spec §6.2. Use exact names:
`setLocked/setMuted/setAudioPan/setSsqlEnabled/setSsqlThresh/setAmsqEnabled/setAmsqThresh/setFmsqEnabled/setFmsqThresh/setAgcThreshold/setAgcHang/setAgcSlope/setAgcAttack/setAgcDecay/setRitEnabled/setRitHz/setXitEnabled/setXitHz/setEmnrEnabled/setSnbEnabled/setApfEnabled/setApfTuneHz/setBinauralEnabled/setFmCtcssMode/setFmCtcssValueHz/setFmOffsetHz/setFmTxMode/setFmReverse/setDiglOffsetHz/setDiguOffsetHz/setRttyMarkHz/setRttyShiftHz`.

*(Round-3 amendment: `setFmSimplex` replaced by `setFmTxMode`; `setDigOffsetHz` split into `setDiglOffsetHz` + `setDiguOffsetHz`.)*

Default values (from Thetis `setup.cs` / `console.cs`):
- `m_agcThreshold{-20}`, `m_agcHang{250}`, `m_agcSlope{0}`, `m_agcAttack{2}`, `m_agcDecay{250}`
- `m_ritHz{0}`, `m_ritEnabled{false}`, `m_xitHz{0}`, `m_xitEnabled{false}`
- `m_locked{false}`, `m_muted{false}`, `m_audioPan{0.0}` (−1..+1, 0 = center)
- `m_emnrEnabled{false}`, `m_snbEnabled{false}`, `m_apfEnabled{false}`, `m_apfTuneHz{0}`
- `m_binauralEnabled{false}`
- FM: `m_fmCtcssMode{0}` (Off), `m_fmCtcssValueHz{100.0}`, `m_fmOffsetHz{0}`, `m_fmTxMode{FmTxMode::Simplex}`, `m_fmReverse{false}`
- DIG: `m_diglOffsetHz{0}`, `m_diguOffsetHz{0}`
- RTTY: `m_rttyMarkHz{2295}`, `m_rttyShiftHz{170}`
- Squelch: `m_ssqlEnabled{false}`, `m_ssqlThreshDb{-150.0}`, `m_amsqEnabled{false}`, `m_amsqThreshDb{-150.0}`, `m_fmsqEnabled{false}`, `m_fmsqThreshDb{-150.0}`

**Every default gets a `// From Thetis <file>:<line>` comment.** If you can't find the Thetis line number (because Console/ is missing per §5), add a `// Thetis default — citation pending Stage 2 gate` comment. The Stage 2 source-first gate will force a replacement of that comment with a real citation before the feature-slice commit.

- [ ] **S1.6.3** Implement all stub bodies in `src/models/SliceModel.cpp`. Each body follows the same shape:

```cpp
void SliceModel::setAgcThreshold(int dBu) {
    if (m_agcThreshold == dBu) return;
    m_agcThreshold = dBu;
    emit agcThresholdChanged(dBu);
}
```

Every body is pure state-and-emit. **No** `RxChannel` references.

- [ ] **S1.6.4** Build + run all existing tests:
  ```
  cmake --build build -j && ctest --test-dir build --output-on-failure 2>&1 | tail -20
  ```
  Expected: existing test count unchanged, build clean. No behavioral change — SliceModel has new storage that nothing reads yet.

- [ ] **S1.6.5** Commit:
  ```
  git add src/models/SliceModel.{h,cpp} src/core/WdspTypes.h
  git commit -S -m "phase3g10(model): extend SliceModel with stub setters for new DSP state

Additive Q_PROPERTYs for every RX-side DSP NYI listed in spec §6.2:
AGC threshold/hang/slope/attack/decay, squelch SSB/AM/FM, EMNR, SNB,
APF + tune, binaural, mute, audio pan, lock, RIT/XIT, and the
FM/DIG/RTTY mode-container state.

Each setter body is state-and-emit only; no RxChannel forwarding yet
(Stage 2 wires). Default values marked for Thetis citation — Stage 2
source-first gate enforces replacement of placeholder citations.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
  ```

---

*(Reorder reminder: S1.6 commits before S1.5. The header of this plan keeps the logical numbering; in the working branch S1.6 is commit 5, S1.5 is commit 6.)*

---

## Commit S1.7 — VfoWidget rewrite: header row + freq row + meter row

### Task S1.7 — Top third of the flag

**Files:**
- Rewrite: `src/gui/widgets/VfoWidget.h` — replace member set
- Rewrite: `src/gui/widgets/VfoWidget.cpp` — first stage (tabs still wrong in this commit; fixed in S1.8)
- Source: `~/AetherSDR/src/gui/VfoWidget.cpp:235-520` — `buildUI()` header + freq + meter rows

- [ ] **S1.7.1** Read AetherSDR `VfoWidget.cpp:235-520` and capture the exact layout calls.

- [ ] **S1.7.2** Rewrite `src/gui/widgets/VfoWidget.h` to match the class map in spec §7.2. Drop all members no longer in the map (current `m_nbBtn`/`m_nrBtn`/`m_anfBtn` single-use members; they will be replaced by the grid in S1.8). Keep the class name and public API surface minimal for now — add only the header/freq/meter members plus a placeholder `QStackedWidget* m_tabStack{nullptr}` that S1.8 will populate.

- [ ] **S1.7.3** Rewrite `buildUI()` in `VfoWidget.cpp` for the header + freq + meter rows. For every control:
  - Use `VfoStyles.h` constants.
  - Connect `m_rxAntBtn` / `m_txAntBtn` to `SliceModel::setRxAntenna` / `setTxAntenna` via QMenu popup (port AetherSDR pattern).
  - `m_filterWidthLbl` derives from `SliceModel::filterLow/High` via a helper `formatFilterWidth(int lo, int hi)` that shows `"2.7K"` for 2700 Hz etc.
  - `m_splitBadge` click toggles `SliceModel::setTxSlice` on the *other* slice — for Stage 1 just emit `splitToggled()` signal; wiring of split semantics happens in a later phase.
  - `m_txBadge` click toggles `SliceModel::setTxSlice(!isTxSlice())`.
  - `m_sliceBadge` shows the slice letter (A/B/C/D from `sliceIndex()`).
  - `m_freqStack` contains `m_freqLabel` (26px bold) and `m_freqEdit` (cyan 22px, editing mode); double-click on label flips to edit.
  - `m_levelBar` is a `VfoLevelBar*` (from S1.2); connect to meter poller via `MeterPoller::smeterChanged(double)` — if that signal name differs in current code, grep `src/core/MeterPoller*` for the correct name.

- [ ] **S1.7.4** Build and launch the app manually:
  ```
  cmake --build build -j && ./build/NereusSDR
  ```
  Expected: launches without crash. The tab bar below the meter row is missing or stub (S1.8 fixes). The header/freq/meter rows are present and styled correctly. Live connect to a radio and confirm the S-meter fills.

- [ ] **S1.7.5** Commit:
  ```
  git add src/gui/widgets/VfoWidget.{h,cpp}
  git commit -S -m "phase3g10(flag): rewrite VfoWidget header + freq row + meter row

Top third of the flag rebuilt against AetherSDR VfoWidget.cpp:235-520.
Flat transparent header with colored antenna/filter labels, 26px bold
frequency display with faint white border, VfoLevelBar S-meter with
tick strip above. Tab bar is stubbed in this commit and fully built
in the next.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
  ```

---

## Commit S1.8 — VfoWidget tab bar + four tab panes (NYI-badged)

### Task S1.8 — Build the tab bar and all four tab contents

**Files:**
- Modify: `src/gui/widgets/VfoWidget.{h,cpp}`
- Source: `~/AetherSDR/src/gui/VfoWidget.cpp:553-1500` (the `buildTabContent` block and its per-tab subroutines)

- [ ] **S1.8.1** Read AetherSDR `buildTabContent` start (line 578) and walk each tab sub-block (Audio/DSP/Mode/X-RIT). For each, capture the child-widget construction sequence.

- [ ] **S1.8.2** Add the tab bar widget: four `QLabel`s that look like tabs, wired via `eventFilter` to emit a `tabChanged(int)` signal that switches the stacked widget. Stylesheets from `kTabLblNormal` / `kTabLblActive`.

- [ ] **S1.8.3** Build `AudioTab` content. Every control calls the corresponding `SliceModel` setter on change (stubs from S1.6 — signal fires, no audible effect, that's Stage 1). Every control calls `NyiOverlay::markNyi(widget, "pending Phase 3G-10 Stage 2")` so users see the badge. Structure:
  - `m_afGainSlider` — wired to `SliceModel::setAfGain` (already live from pre-3G-10, **not NYI-badged**)
  - `m_panSlider` — NYI-badged, stub
  - `m_muteBtn` / `m_binBtn` — NYI-badged
  - `m_sqlBtn` + `m_sqlSlider` — NYI-badged
  - AGC 5-button row (Off/Long/Slow/Med/Fast) — Already wired at mode level from pre-3G-10. **Not NYI-badged**; replaces the old combo from current VfoWidget.
  - `m_agcTSlider` — NYI-badged

- [ ] **S1.8.4** Build `DspTab` content:
  - 4×2 grid of DSP toggles: NB, NB2, NR, NR2, ANF, SNB, APF, (spacer)
  - NB and NR and ANF are **already wired**; don't NYI-badge.
  - NB2, NR2, SNB, APF are new; NYI-badged.
  - Below the grid, the APF tune slider (NYI-badged) and four `QWidget*` members for `m_fmContainer` / `m_digContainer` / `m_rttyContainer` — instantiated from `VfoModeContainers.h` (S1.5), hidden by default.

- [ ] **S1.8.5** Build `ModeTab` content:
  - `m_modeCombo` — already live from pre-3G-10
  - `m_quickModeBtns[3]` — NYI-badged for now (user assignment is a polish item but works as static USB/CW/DIG in Stage 1)
  - `m_filterGrid` — already live from pre-3G-10; just re-style with `kModeBtn`
  - `m_cwAutotune` (from `CwAutotuneContainer` in S1.5) — NYI-badged

- [ ] **S1.8.6** Build `XRitTab` content:
  - RIT row: `m_ritBtn` (kDspToggle), `m_ritLabel` (`ScrollableLabel` from S1.3, range ±10 kHz, step from `SliceModel::stepHz()`), `m_ritZeroBtn` (TriBtn-esque, label "0")
  - XIT row: same shape
  - LOCK button + STEP cycle button row
  - All NYI-badged except STEP cycle (which maps to existing `SliceModel::setStepHz`).

- [ ] **S1.8.7** Update `MainWindow::wireVfoWidget()` (or equivalent) to connect every new `VfoWidget` signal to its matching `SliceModel` slot. For Stage 1 this means many `connect(vfo, &VfoWidget::xxxChanged, slice, &SliceModel::setXxx)` lines that do nothing visible yet.

- [ ] **S1.8.8** Build + launch:
  ```
  cmake --build build -j && ./build/NereusSDR
  ```
  Expected: all four tabs render correctly. Clicking a NYI-badged button shows the badge but does not crash. The already-wired controls (mode combo, filter grid, AF gain, AGC mode row, NB/NR/ANF toggles) still behave identically to pre-3G-10. S-meter still live.

- [ ] **S1.8.9** Commit:
  ```
  git add src/gui/widgets/VfoWidget.{h,cpp} src/gui/MainWindow.{h,cpp}
  git commit -S -m "phase3g10(flag): rewrite VfoWidget tab bar + 4 tab panes (NYI-badged)

Audio/DSP/Mode/X-RIT tabs populated against AetherSDR layout. New
controls (NB2, NR2, SNB, APF, squelch, AGC-T, mute, pan, binaural,
RIT/XIT, lock, quick-mode slots, CW autotune) present with NYI
badges; existing wired controls (mode, filter, AF gain, AGC mode,
NB/NR/ANF) carry their behavior unchanged.

Mode-specific containers (FM OPT, DIG offset, RTTY mark/shift, CW
autotune) instantiated but hidden; S1.9 wires mode-change visibility.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
  ```

---

## Commit S1.9 — Mode-specific container visibility rules

### Task S1.9 — Show/hide mode containers on mode change

- [ ] **S1.9.1** In `VfoWidget::buildUI()` final setup, connect `m_slice->dspModeChanged` (or however `SliceModel` signals mode changes) to `VfoWidget::onModeChanged(DSPMode)`.

- [ ] **S1.9.2** Implement `onModeChanged`:

```cpp
void VfoWidget::onModeChanged(DSPMode mode) {
    m_fmContainer->setVisible(mode == DSPMode::FM || mode == DSPMode::NFM);
    m_digContainer->setVisible(mode == DSPMode::DIGL || mode == DSPMode::DIGU);
    m_rttyContainer->setVisible(mode == DSPMode::RTTY);
    m_cwAutotune->setVisible(mode == DSPMode::CWL || mode == DSPMode::CWU);
    // APF slider visible only when APF is enabled AND mode is CW
    m_apfTuneSlider->setVisible(m_slice && m_slice->apfEnabled()
                                && (mode == DSPMode::CWL || mode == DSPMode::CWU));
}
```

Verify the `DSPMode` enum contains `FM`, `NFM`, `DIGL`, `DIGU`, `RTTY`, `CWL`, `CWU`. If any are missing from `WdspTypes.h`, add them (they are all valid WDSP modes per Thetis `DSPMode.cs`).

- [ ] **S1.9.3** Manually test: launch the app, click the mode combo through USB → LSB → CWU → FM → DIGU → RTTY → USB. After each transition, visually confirm:
  - FM shows `m_fmContainer`
  - DIGU shows `m_digContainer`
  - RTTY shows `m_rttyContainer`
  - CWU shows `m_cwAutotune` inside the Mode tab
  - All other modes hide all mode containers
  - Tab layouts recompact correctly (no blank gaps)

- [ ] **S1.9.4** Commit:
  ```
  git add src/gui/widgets/VfoWidget.{h,cpp}
  git commit -S -m "phase3g10(flag): embed VfoModeContainers with mode-visibility rules

FM/NFM shows FmOptContainer; DIGL/DIGU shows DigOffsetContainer;
RTTY shows RttyMarkShiftContainer; CWL/CWU shows CwAutotuneContainer
in the Mode tab. All other modes hide all mode containers.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
  ```

---

## Commit S1.10 — Floating lock/close/record/play buttons

### Task S1.10 — Port the floating sibling buttons

**Files:**
- Modify: `src/gui/widgets/VfoWidget.{h,cpp}`
- Source: `~/AetherSDR/src/gui/VfoWidget.cpp:322-398` (close/lock/rec/play sibling button construction)

- [ ] **S1.10.1** Port the four sibling-button constructions. Each is parented to `parentWidget()` (the SpectrumWidget), not `this`, and positioned by `updatePosition()` to sit to the left or right of the flag based on `m_onLeft`.

- [ ] **S1.10.2** Wire clicks:
  - close → `emit closeRequested(sliceIndex())`
  - lock → `SliceModel::setLocked(!m_slice->isLocked())` (stub setter from S1.6 — no visible effect yet)
  - rec → `emit recordToggled(bool)` (no-op in Stage 1; wired to a recording subsystem in a later phase)
  - play → same pattern

- [ ] **S1.10.3** Implement `VfoWidget::~VfoWidget()` to `delete` each sibling button's `QPointer::data()` (AetherSDR pattern at `:223-233`).

- [ ] **S1.10.4** Build + manually verify the four floating buttons appear beside the flag, clickable, no crash on flag close.

- [ ] **S1.10.5** Commit:
  ```
  git add src/gui/widgets/VfoWidget.{h,cpp}
  git commit -S -m "phase3g10(flag): floating lock/close/rec/play against SpectrumWidget parent

Ported from AetherSDR VfoWidget.cpp:322-398. Four sibling buttons
parented to SpectrumWidget so they can render outside the flag's
geometry; lifecycle guarded by QPointer to avoid double-free on
widget-tree teardown.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
  ```

---

## Commit S1.11 — Tooltip coverage test + Stage 1 exit snapshot

### Task S1.11 — Tooltip coverage test

- [ ] **S1.11.1** Write `tests/tst_vfo_tooltip_coverage.cpp`:

```cpp
#include <QtTest/QtTest>
#include "gui/widgets/VfoWidget.h"
#include <QAbstractButton>
#include <QSlider>
#include <QComboBox>
using namespace NereusSDR;

class TestVfoTooltipCoverage : public QObject {
    Q_OBJECT
private slots:
    void everyEnabledControlHasTooltip() {
        VfoWidget w;
        // Walk all children
        auto check = [](QWidget* child) -> QString {
            if (!child->isEnabled()) return {};
            const QString tt = child->toolTip();
            if (tt.isEmpty()) return QStringLiteral("%1 (%2) has no tooltip")
                .arg(child->objectName(), child->metaObject()->className());
            return {};
        };
        QStringList failures;
        for (auto* btn  : w.findChildren<QAbstractButton*>()) if (auto e = check(btn)) failures << e;
        for (auto* sl   : w.findChildren<QSlider*>())         if (auto e = check(sl))  failures << e;
        for (auto* cmb  : w.findChildren<QComboBox*>())       if (auto e = check(cmb)) failures << e;
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join("\n")));
    }
};
QTEST_MAIN(TestVfoTooltipCoverage)
#include "tst_vfo_tooltip_coverage.moc"
```

- [ ] **S1.11.2** Run:
  ```
  cmake --build build -j && ctest --test-dir build -R tst_vfo_tooltip_coverage --output-on-failure
  ```
  Expected: FAIL initially — many new controls have no tooltip yet.

- [ ] **S1.11.3** Populate tooltips for every newly-added control in `VfoWidget::buildUI`. For each button/slider/combo added in S1.7–S1.10, add a `setToolTip(QStringLiteral("..."))` call **immediately after** widget construction with:
  - **Thetis-first text** where a Thetis equivalent exists. If `~/NereusSDR/../Thetis/Project Files/Source/Console/setup.cs` is available (see §0.4 gate), grep for the control name and port the string verbatim with `// From Thetis Project Files/Source/Console/setup.cs:NNNN` comment.
  - **NereusSDR fallback** (short, imperative, sentence-case, period-terminated) for controls with no Thetis equivalent — tag with `// NereusSDR native — no Thetis equivalent`.
  - **NYI controls** get `"<Control name> — pending Phase 3G-10 Stage 2"`.

  Source the Thetis text strings opportunistically: if the Thetis Console/ directory is missing (§0.4 failed), use the NereusSDR fallback form for Stage 1 and rely on the Stage 2 source-first gate to replace them during slice wiring.

- [ ] **S1.11.4** Re-run:
  ```
  ctest --test-dir build -R tst_vfo_tooltip_coverage --output-on-failure
  ```
  Expected: PASS.

- [ ] **S1.11.5** Commit:
  ```
  git add src/gui/widgets/VfoWidget.cpp tests/tst_vfo_tooltip_coverage.cpp CMakeLists.txt
  git commit -S -m "phase3g10(test): add tooltip coverage test + populate flag tooltips

Walks VfoWidget children and asserts every enabled button/slider/
combo has a non-empty tooltip(). Fails build if a new control is
added without one. Stage 1 populates Thetis-first tooltips where
the Thetis Console/ source is available, NereusSDR-voice fallbacks
otherwise — Stage 2 source-first gate replaces fallbacks during
slice wiring.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
  ```

---

## Stage 1 exit verification

- [ ] **S1-exit.1** Full build clean:
  ```
  rm -rf build && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build -j
  ```
  Expected: succeeds with zero warnings introduced by the 3G-10 files (existing warnings OK).

- [ ] **S1-exit.2** Full test suite green:
  ```
  ctest --test-dir build --output-on-failure
  ```
  Expected: zero failures, pass count = baseline + 4 (the four new Stage 1 tests).

- [ ] **S1-exit.3** Manual smoke test on a connected ANAN-G2 or HL2:
  - Launch, connect, tune 14.225 MHz USB → confirm audible RX unchanged vs pre-3G-10
  - Cycle mode through all modes → confirm mode containers show/hide correctly
  - S-meter fills live
  - Every NYI-badged button clicks without crash
  - Freq label double-click enters edit, Enter commits, Esc cancels
  - RIT/XIT `ScrollableLabel` scrolls but has no audible effect (Stage 2 wires)

- [ ] **S1-exit.4** Open a draft PR against `main` with title `[Stage 1] Phase 3G-10 — AetherSDR VfoWidget shell port`. **Do not merge** — Stage 2 lands on the same branch and the PR is flipped to ready after Stage 2 completes.

---

# Stage 2 — SliceModel + RxChannel + WDSP backfill (10 feature slices)

## Pre-Stage-2 gate (mandatory — do this before S2.1.1)

- [ ] **S2.0.1** Re-run the Stage 2 gate from §0.4:
  ```
  ls ~/NereusSDR/../Thetis/Project\ Files/Source/Console/console.cs
  ```
  If missing:
  ```
  rm -rf ~/NereusSDR/../Thetis && git clone --depth 1 https://github.com/ramdor/Thetis ~/NereusSDR/../Thetis
  ```
  Or accept the raw-URL fallback: every Stage 2 commit message must cite the exact `raw.githubusercontent.com` URL + commit hash in addition to the file/line number.

- [ ] **S2.0.2** Record the Thetis commit hash in use:
  ```
  cd ~/NereusSDR/../Thetis && git rev-parse HEAD
  ```
  Write this hash into `docs/architecture/phase3g10-verification/README.md` under a `**Thetis commit for source-first citations:** <hash>` line so every Stage 2 commit has a stable citation anchor.

---

## Stage 2 feature-slice template

Every Stage 2 slice follows this shape. The slice-specific parameters are listed per slice in S2.1 through S2.10. Perform every numbered sub-step of the template for each slice.

### Slice template

**Files touched (each slice):**
- `src/models/SliceModel.h` — replace stub body comment
- `src/models/SliceModel.cpp` — replace stub body with forward to `RxChannel`
- `src/core/RxChannel.h` — declare new WDSP-calling method
- `src/core/RxChannel.cpp` — implement using WDSP C API
- `src/gui/widgets/VfoWidget.cpp` — remove `NyiOverlay::markNyi` on corresponding button(s)
- `src/gui/applets/RxApplet.cpp` — remove NYI guard on corresponding control(s)
- `tests/tst_slice_<slug>.cpp` — SliceModel unit test (new)
- `tests/tst_rxchannel_<slug>.cpp` — RxChannel GoogleTest against WDSP in-process (new)

**Sub-steps (per slice, in order):**

- [ ] **T.1** Read the Thetis source for the slice's `<feature>`. Use `grep -rn "<ThetisSymbol>" ~/NereusSDR/../Thetis/Project\ Files/Source/Console/`. Record the file+line. State in the worklog: `Porting from Thetis <file>:<line> — original C# logic: <quote>`.

- [ ] **T.2** Read the WDSP C source for each `SetRXA*` call being ported. Use `grep -n "<WDSPSymbol>" third_party/wdsp/src/*.{h,c}`. Record signatures.

- [ ] **T.3** Write the failing `RxChannel` test `tests/tst_rxchannel_<slug>.cpp`. Pattern:

```cpp
#include <gtest/gtest.h>
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
using namespace NereusSDR;
TEST(RxChannelXxx, CallsSetRxaXxxRunOnEnable) {
    WdspEngine engine;
    engine.init();
    RxChannel ch(0, 1024, 48000);
    ch.setActive(true);
    // Test the observable: after setXxxEnabled(true), WDSP's internal
    // run flag for XXX is 1. Use the WDSP state accessor if available;
    // if not, the test asserts that the method call does not throw +
    // the atomic flag on RxChannel reflects the call.
    EXPECT_NO_THROW(ch.setXxxEnabled(true));
    EXPECT_EQ(ch.xxxEnabled(), true);
    // If the WDSP module exposes a Get<module>Run or similar accessor,
    // use it here. Otherwise rely on a smoke-level invariant.
}
```

- [ ] **T.4** Run to confirm failure (method doesn't exist yet).

- [ ] **T.5** Implement `RxChannel::setXxxEnabled` (or `setXxx(value)`) in the header/source. Body:

```cpp
void RxChannel::setXxxEnabled(bool enabled) {
    if (m_xxxEnabled.exchange(enabled) == enabled) return;
    // From Thetis Project Files/Source/Console/<file>:<line>
    //   original C# call: wdsp.SetRXAXxxRun(channel, enabled ? 1 : 0);
    // WDSP third_party/wdsp/src/<file>:<line>
    SetRXAXxxRun(m_channelId, enabled ? 1 : 0);
}
```

- [ ] **T.6** Run the test, confirm PASS.

- [ ] **T.7** Write the `SliceModel` test `tests/tst_slice_<slug>.cpp` — asserts that `SliceModel::setXxxEnabled` forwards to the `RxChannel` via the existing plumbing. Use a test double `FakeRxChannel` if needed; otherwise make the test construct a real `WdspEngine` + `RxChannel` and observe through the public `xxxEnabled()` accessor.

- [ ] **T.8** Replace the Stage 1 stub body in `SliceModel::setXxxEnabled` with a real body that forwards to `m_rxChannel->setXxxEnabled(v)` before emitting the signal.

- [ ] **T.9** Run both new tests + full suite:
  ```
  cmake --build build -j && ctest --test-dir build --output-on-failure
  ```
  Expected: all pass.

- [ ] **T.10** Remove the `NyiOverlay::markNyi` call on the corresponding `VfoWidget` button(s). Update the button tooltip from the Stage 1 fallback to the Thetis-first text sourced in T.1.

- [ ] **T.11** Remove the NYI guard on the corresponding control in `src/gui/applets/RxApplet.cpp`. Grep for `NYI` + the slice name.

- [ ] **T.12** Manual A/B regression on a live radio:
  - ANAN-G2 + HL2
  - Before-state: parameter at Stage 1 default
  - After-state: parameter toggled
  - Record audible / S-meter / spectrum delta in `docs/architecture/phase3g10-verification/rx-dsp-grid.md` under the matching row

- [ ] **T.13** Commit the slice with the standard template message:
```
phase3g10(rx-dsp): wire <feature>

- SliceModel: <setters touched>
- RxChannel: <wdsp calls added>
- VfoWidget: remove NYI badge on <button(s)>
- RxApplet: remove NYI guard on <control>
- tests: tst_slice_<slug>, tst_rxchannel_<slug>

From Thetis Project Files/Source/Console/<file>.cs:<line> — <quoted C# call>
WDSP: third_party/wdsp/src/<file>.{c,h}:<line>

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
```

---

## S2.1 — AGC threshold / hang / slope / attack / decay

**Slug:** `agc_advanced`
**Slice template parameters:**
- **Thetis symbols to grep:** `SetRXAAGCThresh`, `SetRXAAGCHang`, `SetRXAAGCSlope`, `SetRXAAGCAttack`, `SetRXAAGCDecay` in `Project Files/Source/Console/console.cs`
- **WDSP symbols:** all five live in `third_party/wdsp/src/wcpAGC.h` at lines 148, 150, 152, 166, 168 (verified during spec writing — see `grep` output in §2 of the design doc)
- **RxChannel methods added:** `setAgcThreshold(int)`, `setAgcHang(int)`, `setAgcSlope(int)`, `setAgcAttack(int)`, `setAgcDecay(int)`
- **SliceModel setters replaced:** same five, from S1.6 stubs
- **VfoWidget buttons un-NYI'd:** AGC-T slider (Audio tab)
- **RxApplet NYI guards removed:** AGC-T slider block around RxApplet.cpp:418
- **Units and ranges** (from WDSP comments + Thetis setup.cs):
  - Threshold: dBu, range -160..0, default -20
  - Hang: milliseconds, range 0..5000, default 500
  - Slope: 1/10 dB per step, range 0..10, default 0
  - Attack: milliseconds, range 1..10, default 2
  - Decay: milliseconds, range 1..5000, default 250
- **Quirk:** `SetRXAAGCThresh` takes three parameters (`channel, thresh, size, rate`) per WDSP header. `size` = bandpass filter size, `rate` = channel sample rate. These come from `RxChannel::bufferSize()` and `RxChannel::sampleRate()`.

Perform the slice template T.1–T.13 with the above parameters. Each sub-setter gets its own inner sub-test within `tst_rxchannel_agc_advanced.cpp` so the commit remains one slice.

---

## S2.2 — EMNR (NR2)

**Slug:** `emnr`
**Parameters:**
- **Thetis symbol:** `SetRXAEMNRRun`
- **WDSP:** `third_party/wdsp/src/emnr.c:1283` — `void SetRXAEMNRRun(int channel, int run)` (verified)
- Additional setters: `SetRXAEMNRgainMethod`, `SetRXAEMNRnpeMethod`, `SetRXAEMNRaeRun` — grep `emnr.c` for signatures; default position/gain method/npe values come from the Thetis Setup.cs tab for NR2 (gate on §S2.0 Thetis access)
- **RxChannel methods:** `setEmnrEnabled(bool)`, `setEmnrGainMethod(int)`, `setEmnrNpeMethod(int)`, `setEmnrAeRun(bool)`
- **SliceModel setter replaced:** `setEmnrEnabled`
- **VfoWidget button un-NYI'd:** NR2 (DSP grid, position 4)
- **RxApplet guard:** grep `NR2` in RxApplet.cpp and remove
- **Caveat:** EMNR has multiple "position" choices (pre-AGC / post-AGC). Port Thetis default faithfully.

Template T.1–T.13.

---

## S2.3 — SNB

**Slug:** `snb`
**Parameters:**
- **WDSP:** `third_party/wdsp/src/snb.c:579` — `PORT void SetRXASNBARun(int channel, int run)` (verified)
- **Thetis symbol:** `SetRXASNBARun`
- **RxChannel method:** `setSnbEnabled(bool)`
- **VfoWidget button un-NYI'd:** SNB
- **RxApplet guard:** SNB

Template T.1–T.13.

---

## S2.4 — APF (Audio Peak Filter)

**Slug:** `apf`
**Parameters:**
- **WDSP:** `third_party/wdsp/src/apfshadow.h` + `apfshadow.c` — grep for `SetRXAAPFRun`, `SetRXAAPFFreq`, `SetRXAAPFBw` signatures. Also check `apf.c` in case the canonical module is there not in `apfshadow.c`.
- **RxChannel methods:** `setApfEnabled(bool)`, `setApfTuneHz(int)`, `setApfBandwidthHz(int)`
- **SliceModel setters:** `setApfEnabled`, `setApfTuneHz`
- **VfoWidget buttons un-NYI'd:** APF button (DSP grid), APF tune slider (below the grid)
- **Slider range:** -500..+500 Hz offset from the demod center, default 0 (per Thetis)
- **Open question R3 from spec:** AetherSDR uses a slider for "tune" but Thetis APF may be a fixed-bandwidth peak filter. **During T.1, if the Thetis Setup APF page does not expose a tune slider, pause the slice and ask the user which semantics to honor.** Source-first wins unless user approves AetherSDR UX override.

Template T.1–T.13, with the extra pause at T.1 above.

---

## S2.5 — Squelch (SSB / AM / FM)

**Slug:** `squelch`
**Parameters:**
- **WDSP:** three modules —
  - SSQL: grep `third_party/wdsp/src/ssql.{h,c}` for `SetRXASSQLRun` + `SetRXASSQLThreshold`
  - AMSQ: `third_party/wdsp/src/amsq.h:75` — `SetRXAAMSQRun(int, int)`, `:77` — `SetRXAAMSQThreshold(int, double)` (verified)
  - FMSQ: `third_party/wdsp/src/fmsq.h:90,92,94` — `SetRXAFMSQThreshold`, `SetRXAFMSQNC`, `SetRXAFMSQMP` (verified)
- **RxChannel methods:** `setSsqlEnabled/Thresh`, `setAmsqEnabled/Thresh`, `setFmsqEnabled/Thresh`
- **VfoWidget behavior:** the single `m_sqlBtn` + `m_sqlSlider` pair on the Audio tab routes to the correct variant based on `m_slice->dspMode()`. Helper:

```cpp
void VfoWidget::applySqlChange(bool on, double threshDb) {
    if (!m_slice) return;
    switch (m_slice->dspMode()) {
    case DSPMode::USB: case DSPMode::LSB: case DSPMode::CWU: case DSPMode::CWL:
    case DSPMode::DIGU: case DSPMode::DIGL: case DSPMode::DRM: case DSPMode::SPEC:
        m_slice->setSsqlEnabled(on); m_slice->setSsqlThresh(threshDb); break;
    case DSPMode::AM: case DSPMode::SAM:
        m_slice->setAmsqEnabled(on); m_slice->setAmsqThresh(threshDb); break;
    case DSPMode::FM: case DSPMode::NFM:
        m_slice->setFmsqEnabled(on); m_slice->setFmsqThresh(threshDb); break;
    default: break;
    }
}
```

Template T.1–T.13. The slice test exercises mode→variant routing.

---

## S2.6 — Mute / audio pan / binaural

**Slug:** `audio_panel`
**Parameters:**
- **WDSP:** `SetRXAPanelRun`, `SetRXAPanelPan`, `SetRXAPanelBinaural` — grep `third_party/wdsp/src/` for exact locations
- **RxChannel methods:** `setMuted(bool)`, `setAudioPan(double -1..+1)`, `setBinauralEnabled(bool)`
- **Quirk:** Mute = `SetRXAPanelRun(channel, 0)` (stops the panel stage), unmute = `SetRXAPanelRun(channel, 1)`. `SetRXAPanelPan(channel, double)` where -1 = full left, +1 = full right, 0 = center.

Template T.1–T.13.

---

## S2.7 — NB2 (xnobEXTF) polish

**Slug:** `nb2_polish`
**Parameters:**
- **WDSP:** `xnobEXTF` gate already wired from pre-3G-10; this slice exposes position/lead/tau/hang parameters to match Thetis defaults.
- **Thetis symbols to grep:** `SetEXTNOBMode`, `SetEXTNOBTau`, `SetEXTNOBAdvslewtime`, `SetEXTNOBAdvtime`, `SetEXTNOBHangtime` — these are EXT (outside the RXA chain) so the C# calls are named differently from the `SetRXA*` pattern
- **RxChannel methods:** `setNb2Mode(int)`, `setNb2Tau(double)`, `setNb2LeadTime(double)`, `setNb2HangTime(double)`
- **VfoWidget button un-NYI'd:** NB2

Template T.1–T.13.

---

## S2.8 — RIT / XIT client offset

**Slug:** `rit_xit`
**Parameters:**
- **Client-side only** — no new WDSP call. RIT offset is added to the existing `setShiftFrequency` path. XIT is stored for 3M-1 to consume on keydown.
- **SliceModel bodies:** `setRitEnabled`, `setRitHz`, `setXitEnabled`, `setXitHz`. When `rit` changes, `SliceModel::effectiveRxFrequency()` returns `m_frequency + (m_ritEnabled ? m_ritHz : 0)`; this drives the existing `setShiftFrequency` path.
- **VfoWidget un-NYI'd:** RIT row, XIT row (XIT keeps NYI badge on its signal-test — marked "TX gated by 3M-1" tooltip change).
- **RxApplet guard:** RIT/XIT section around RxApplet.cpp:439, 477

Template T.1–T.13. No WDSP unit test needed for this slice; `tst_slice_rit_xit.cpp` is the only new test.

---

## S2.9 — Frequency lock

**Slug:** `frequency_lock`
**Parameters:**
- **Client-side only.** `SliceModel::setFrequency` becomes:
  ```cpp
  void SliceModel::setFrequency(double f) {
      if (m_locked) return;  // 3G-10: client-side lock guard
      if (f == m_frequency) return;
      m_frequency = f;
      emit frequencyChanged(f);
  }
  ```
- **VfoWidget un-NYI'd:** LOCK button (X/RIT tab) + floating 🔒 sibling button
- **RxApplet guard:** around RxApplet.cpp:114

Template T.1–T.13. Test asserts `setFrequency` is a no-op while locked.

---

## S2.10 — Mode-specific container behaviors

**Slug:** `mode_containers_wire`

This slice is larger — it wires FM OPT, DIG offset, and RTTY mark/shift container outputs to `SliceModel` → `RxChannel` chains. CW autotune is a separate sub-slice at the end.

- **FM OPT (RX-only portions in 3G-10):**
  - CTCSS decode via FMSQ tone squelch — `SetRXAFMSQNC` / `SetRXAFMSQMP` carry the tone number
  - Offset preview (not TX) — stored in SliceModel, displayed on the flag, no RF effect
  - Simplex button forces offset to 0
  - Reverse toggle swaps display order (no RF effect in RX-only phase)
  - TX-side CTCSS encode + repeater shift are TODO-commented with `// Phase 3M-1: wire TX CTCSS encode and keydown shift`
- **DIG offset:** client offset added to `effectiveRxFrequency`; feeds existing `setShiftFrequency` path just like RIT
- **RTTY mark/shift:** computes `filterLow = markHz - shiftHz/2 - 100`, `filterHigh = markHz + shiftHz/2 + 100` (from Thetis `RTTY` setup page — cite exact line during T.1) and pushes through `SliceModel::setFilter`
- **CW autotune:** invokes `matchedCW.h` API via RxChannel helper. Once mode = one call; loop = QTimer at 500ms. Grep `third_party/wdsp/src/matchedCW.h` for the exact API.

Template T.1–T.13, executed as **three sub-commits** inside this slice:

1. `phase3g10(rx-dsp): wire FM OPT RX-side (CTCSS decode, offset preview)`
2. `phase3g10(rx-dsp): wire DIG offset and RTTY mark/shift`
3. `phase3g10(rx-dsp): wire CW autotune via matchedCW.h`

Each sub-commit carries its own test, its own NYI-badge removals, its own verification doc update.

---

## Stage 2 persistence task group

### Task S2.P — Per-slice-per-band persistence

**Files:**
- Modify: `src/models/SliceModel.{h,cpp}`
- Modify: `src/models/PanadapterModel.{h,cpp}` (tiny: subscribe `SliceModel::onBandChanged`)
- Modify: `src/core/AppSettings.h` — no code change; add a documentation block describing the new key namespace
- Create: `tests/tst_slice_persistence_per_band.cpp`

- [ ] **S2.P.1** Write `tst_slice_persistence_per_band.cpp` with 4 cases:
  1. `saveAndRestoreEachKey` — for each per-band key in spec §9.1, set a non-default value, trigger save, re-instantiate SliceModel, call `restoreFromSettings(currentBand)`, assert the value round-trips.
  2. `bandChangeRestoresPreviouslyStoredValuesForNewBand` — store different AGC thresholds on 20m and 40m, switch bands, assert each band recalls its own value.
  3. `sessionStateIsNotPerBand` — store mute=true, switch bands, assert mute is still true.
  4. `migratesLegacyKeys` — pre-populate `AppSettings` with a legacy `Slice0/FilterLowHz` key, call the migration routine, assert the key moved to `Slice0/Band20m/FilterLowHz` and the old key is deleted.

- [ ] **S2.P.2** Implement `SliceModel::restoreFromSettings(Band)` and `SliceModel::saveToSettings(Band)`. Key layout per spec §9.1. Use `Band::bandKeyName()` from 3G-8. Session-state keys (lock, mute, RIT/XIT values) are band-agnostic and use `Slice<N>/<Key>`; per-band keys use `Slice<N>/Band<Key>/<Key>`.

- [ ] **S2.P.3** Implement `SliceModel::migrateLegacyKeys()` — one-shot on first launch of 3G-10. Detects the legacy 3G-8-era `Slice<N>/FilterLowHz` keys, moves them under the current band namespace, deletes the originals, logs an info message.

- [ ] **S2.P.4** Wire `PanadapterModel::bandChanged` to a new `SliceModel::onBandChanged(Band)` slot in `MainWindow::wireSlice()` (or equivalent). On band change, save the old band's state, restore the new band's state.

- [ ] **S2.P.5** Run:
  ```
  cmake --build build -j && ctest --test-dir build -R tst_slice_persistence_per_band --output-on-failure
  ```
  Expected: all 4 cases PASS.

- [ ] **S2.P.6** Manual regression: band-hop on a live radio across 5 bands, set different DSP values on each, quit, relaunch, band-hop again. All values should restore.

- [ ] **S2.P.7** Commit:
  ```
  git add src/models/SliceModel.{h,cpp} src/models/PanadapterModel.{h,cpp} tests/tst_slice_persistence_per_band.cpp
  git commit -S -m "phase3g10(persist): per-slice-per-band DSP bandstack persistence

Slice<N>/Band<KEY>/* namespace for DSP-flavored state; session-state
(lock, mute, RIT/XIT) remains slice-global. One-shot migration of
legacy 3G-8-era keys. 4-case unit test + manual band-hop smoke
test on ANAN-G2.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
  ```

---

## Stage 2 tooltip sweep task group

### Task S2.T — Replace Stage 1 fallback tooltips with Thetis-first text

Stage 1 Task S1.11 populated tooltips with whichever source was available. With Thetis Console/ now accessible (§S2.0 gate), replace every `// NereusSDR native — no Thetis equivalent` comment that does have a Thetis equivalent with the verbatim Thetis text + source line citation.

- [ ] **S2.T.1** Grep `VfoWidget.cpp` and `VfoModeContainers.cpp` for `// NereusSDR native — no Thetis equivalent`. For each hit, determine the Thetis control that corresponds.

- [ ] **S2.T.2** For each correspondence, read the Thetis tooltip source (usually `Project Files/Source/Console/setup.cs` tooltip registration or a resource file) and port the text verbatim. Update the comment to `// From Thetis Project Files/Source/Console/<file>:<line>`.

- [ ] **S2.T.3** Re-run `tst_vfo_tooltip_coverage` — must still pass.

- [ ] **S2.T.4** Commit:
  ```
  git add src/gui/widgets/VfoWidget.cpp src/gui/widgets/VfoModeContainers.cpp
  git commit -S -m "phase3g10(tooltips): replace Stage 1 fallback tooltips with Thetis-first text

For every control that has a Thetis equivalent, replaces the Stage 1
NereusSDR-voice fallback with the verbatim Thetis tooltip text from
Project Files/Source/Console/setup.cs and related sources. Controls
with no Thetis equivalent (LOCK, BIN, our S-unit tick strip) retain
NereusSDR-voice text.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
  ```

---

## Phase exit — verification, docs, CHANGELOG

- [ ] **S2-exit.1** Create `docs/architecture/phase3g10-verification/README.md` with the per-control × radio matrix (spec §11.2). Populate each row with date + tester initials.

- [ ] **S2-exit.2** Fill `docs/architecture/phase3g10-verification/rx-dsp-grid.md` with the DSP tab per-button before/after smoke results collected during Stage 2 slice work.

- [ ] **S2-exit.3** Fill `docs/architecture/phase3g10-verification/mode-containers.md` with the FM/DIG/RTTY/CW smoke results.

- [ ] **S2-exit.4** Fill `docs/architecture/phase3g10-verification/persistence.md` with the per-band restore matrix from S2.P.

- [ ] **S2-exit.5** Append 3G-10 entry to `CHANGELOG.md` under a new `## [Unreleased]` or next-version section per the project's changelog convention.

- [ ] **S2-exit.6** Update `CLAUDE.md` Phase 3 table row for 3G-10 to **Complete** with the design + plan doc links.

- [ ] **S2-exit.7** Update `docs/MASTER-PLAN.md` 3G-10 subsection status line to **Complete**.

- [ ] **S2-exit.8** Flip the draft PR to ready-for-review. Title: `[Phase 3G-10] RX DSP Parity + AetherSDR Flag Port (Stages 1 + 2)`. Body: use the spec §14 exit criteria as a checklist with every box ticked.

- [ ] **S2-exit.9** After merge, run `/release patch` to cut v0.1.5 (or whatever the next semver increment is per the release skill rules). This is optional per maintainer judgment; the plan does not mandate a release per phase.

---

## Plan self-review

**1. Spec coverage** — Every spec §6.2 NYI row has a Stage 2 slice (S2.1 AGC-adv covers 5 rows; S2.2 EMNR; S2.3 SNB; S2.4 APF; S2.5 Squelch 3-in-1; S2.6 mute/pan/bin; S2.7 NB2 polish; S2.8 RIT/XIT; S2.9 lock; S2.10 FM/DIG/RTTY/CW). Mute + audio pan appear in S2.6 as the "Panel" slice. Step cycle is a pre-3G-10 setter already; promoted to a stage-1-only wiring in S1.8 (Mode tab STEP button). All covered.

**2. Placeholder scan** — No TBDs in concrete code. The plan uses `<file>:<line>` and `<slug>` placeholders inside the feature-slice template, which are **parameters the slice sections fill in**, not missing content. Each S2.1–S2.10 section lists the concrete parameters.

**3. Type consistency** — Checked: `setAgcThreshold` / `agcThresholdChanged` used consistently. `setEmnrEnabled` / `emnrEnabled` consistent. Slug names match test filenames. `Slice<N>/Band<KEY>/*` namespace matches spec §9.1.

**4. Ordering gotcha** — S1.5 (VfoModeContainers) refers to `SliceModel::fmCtcssMode()` which is declared in S1.6 (SliceModel stubs). The plan explicitly reorders: **S1.6 commits before S1.5**, called out in the note under S1.5.

**5. Stage 2 gate** — Pre-Stage-2 §S2.0 gate is mandatory and checked before S2.1.1 starts. Without it, slice T.1 source-first citations cannot be generated.

**6. Risks from spec §13** — R1 (partial Thetis clone) handled by §S2.0 gate. R2 (large file churn) handled by 11-commit decomposition in Stage 1 + `VfoStyles.h`/`VfoModeContainers.h`/`VfoLevelBar.h` splits. R3 (APF semantics) handled by explicit pause at S2.4 T.1. R4 (FM CTCSS TX) handled by explicit TODO-comment language in S2.10. R5 (settings growth) is a monitoring item, not a blocking risk. R6 (CW autotune API) handled by S2.10 sub-commit 3 source review.

---

## Execution handoff

Plan complete and saved to `docs/architecture/phase3g10-rx-dsp-flag-plan.md`. Two execution options:

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration. Best fit for this plan's 60+ small commits where batch review would drown the main context.

2. **Inline Execution** — Execute tasks in this session using `executing-plans`, batch execution with checkpoints for review.

Which approach?
