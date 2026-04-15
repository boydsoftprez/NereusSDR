# Phase 3G-9a — Source-First Audit + Tooltips + Slider Readouts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Take the three 3G-8 display Setup pages from "every control works" to "every control has a source-first citation, a useful tooltip, and — for sliders — a bidirectional numeric spinbox companion."

**Architecture:** Add a small `SetupHelpers` factory (two functions) that returns a prebuilt slider+spinbox pair wired bidirectionally against feedback loops. Refactor `DisplaySetupPages.cpp` to use the factory. Then walk the 47 controls and add per-control Thetis citation comments + tooltips (verbatim where Thetis is substantive, rewritten where weak, new where NereusSDR-original). Zero model or renderer changes.

**Tech Stack:** C++20, Qt6 (QtWidgets, QtTest). No new third-party deps.

**Scope boundary:** RX1 Display only — `SpectrumDefaultsPage`, `WaterfallDefaultsPage`, `GridScalesPage`. RX2 Display, TX Display, Spectrum Overlay flyouts, and meter editors are out of scope per spec §2.

**Parent branch:** `main` (commits `4b0a027`, `8437888`). New branch `feature/display-audit-pr1`.

**Design spec:** `docs/architecture/2026-04-15-display-refactor-design.md` §4.

---

## File Structure

**Create:**
- `src/gui/setup/SetupHelpers.h` — two factory functions, two POD struct return types
- `src/gui/setup/SetupHelpers.cpp` — implementations, ~100 LOC
- `tests/tst_setup_helpers.cpp` — QtTest smoke for the factories (~80 LOC)

**Modify:**
- `src/gui/setup/DisplaySetupPages.cpp` — 8 slider refactors (4 in Spectrum, 4 in Waterfall) + 47 tooltip/citation blocks
- `CMakeLists.txt` — register `SetupHelpers.cpp` as part of `GUI_SOURCES`
- `tests/CMakeLists.txt` — register `tst_setup_helpers`

**Untouched:**
- `src/gui/setup/DisplaySetupPages.h` — no API change
- Any `src/models/` or `src/gui/SpectrumWidget*` — zero behavior changes

## Slider Inventory (exact)

After reading `DisplaySetupPages.h`, there are **8 sliders** in scope (not 12 as the spec estimated — `GridScalesPage` uses only spinboxes/combos/swatches). The spec's "~12 sliders" estimate is corrected here:

**SpectrumDefaultsPage (4 sliders):**
| Member | Range | Suffix | Connects to |
|---|---|---|---|
| `m_fpSlider` | 10–60 | ` fps` | `FFTEngine::setOutputFps` |
| `m_fillAlphaSlider` | 0–100 | `%` | `SpectrumWidget::setFillAlpha(v/100.f)` |
| `m_lineWidthSlider` | 1–3 | ` px` | `SpectrumWidget::setLineWidth` |
| `m_dataLineAlphaSlider` | 0–255 | ` (0-255)` | `SpectrumWidget::setFillColor` alpha channel |

**WaterfallDefaultsPage (4 sliders):**
| Member | Range | Suffix | Connects to |
|---|---|---|---|
| `m_highThresholdSlider` | -200 to 0 | ` dBm` | `SpectrumWidget::setWaterfallHighThreshold` |
| `m_lowThresholdSlider` | -200 to 0 | ` dBm` | `SpectrumWidget::setWaterfallLowThreshold` |
| `m_updatePeriodSlider` | 10–500 | ` ms` | `SpectrumWidget::setWaterfallUpdatePeriod` |
| `m_opacitySlider` | 0–100 | `%` | `SpectrumWidget::setWaterfallOpacity` |

**GridScalesPage: 0 sliders** — no refactor needed for this page. Only the tooltip/citation pass touches it.

---

## Tooltip Decision Rule (spec §4.1.2)

For each of the 47 controls:

1. Grep `../Thetis/Project Files/Source/Console/setup.designer.cs` for the Thetis control name from the 3G-8 plan tables (§3.1/§4.1/§5.1).
2. Find `toolTip1.SetToolTip(this.<controlName>, "<text>");`.
3. **If Thetis tooltip is substantive** (>10 chars, says something about what the control does): port verbatim.
   - Add comment: `// Tooltip from Thetis setup.designer.cs:NNNN`
4. **If Thetis tooltip is empty, missing, or just echoes the label**: write a new one describing what the user will see change.
   - Add comment: `// Tooltip rewritten — Thetis original: "<original or empty>"`
5. **If the control is a NereusSDR extension** (S2 FFT Window, S9/S10 Peak Hold, W5 Reverse Scroll, W8/W9 Timestamp): write from scratch, no attribution comment needed.

Citation comment format (above every `connect(...)` block):
```cpp
// Thetis: setup.designer.cs:NNNN (controlName) → target
```
or for extensions:
```cpp
// NereusSDR extension — no Thetis equivalent
```

---

## Task 1: Branch + SetupHelpers Skeleton

**Files:**
- Create: `src/gui/setup/SetupHelpers.h`
- Create: `src/gui/setup/SetupHelpers.cpp` (empty body for now)
- Modify: `CMakeLists.txt` (register new source file)

- [ ] **Step 1: Create the branch**

Run:
```bash
cd ~/NereusSDR
git checkout main && git pull --ff-only
git checkout -b feature/display-audit-pr1
```

Expected: clean switch, no conflicts.

- [ ] **Step 2: Re-read CLAUDE.md and CONTRIBUTING.md**

Per standing preference, start each branch with a fresh read of both files. No code yet — just load them into context.

- [ ] **Step 3: Write `SetupHelpers.h`**

Create `src/gui/setup/SetupHelpers.h`:

```cpp
#pragma once

// Setup page factory helpers — produce prewired slider+spinbox rows so every
// numeric control in the Setup dialog has a live readout and supports both
// mouse and keyboard entry without copy-pasted boilerplate.
//
// Bidirectional sync uses QSignalBlocker to prevent signal feedback loops.

#include <QWidget>
#include <QString>

class QSlider;
class QSpinBox;
class QDoubleSpinBox;

namespace NereusSDR {

// Integer slider + QSpinBox pair.
struct SliderRow {
    QSlider*  slider;
    QSpinBox* spin;
    QWidget*  container;  // HBox with slider + spin, ready to drop into a form layout
};

// Double slider + QDoubleSpinBox pair. Slider operates in integer units
// scaled by 10^decimals under the hood.
struct SliderRowD {
    QSlider*        slider;
    QDoubleSpinBox* spin;
    QWidget*        container;
};

// Build an int slider+spinbox row. The slider and spinbox are wired
// bidirectionally via QSignalBlocker so dragging the slider updates the
// spinbox value without retriggering the slider's valueChanged signal,
// and typing into the spinbox updates the slider without retriggering
// the spinbox's valueChanged signal. Consumers connect to slider->valueChanged
// (one source of truth) to drive model updates.
//
// Arguments:
//   min, max, initial — range and start value
//   suffix            — unit string appended in the spinbox (" dBm", "%", etc.)
//   parent            — ownership of the returned container widget
SliderRow makeSliderRow(int min, int max, int initial,
                        const QString& suffix = QString(),
                        QWidget* parent = nullptr);

// Double variant. decimals controls the spinbox's decimal places and the
// slider's internal integer granularity (step = 10^-decimals in user space).
SliderRowD makeDoubleSliderRow(double min, double max, double initial,
                               int decimals,
                               const QString& suffix = QString(),
                               QWidget* parent = nullptr);

} // namespace NereusSDR
```

- [ ] **Step 4: Write empty `SetupHelpers.cpp`**

Create `src/gui/setup/SetupHelpers.cpp`:

```cpp
#include "SetupHelpers.h"

#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>

namespace NereusSDR {

// Intentionally empty — filled in by Task 2.

} // namespace NereusSDR
```

- [ ] **Step 5: Register the new file in CMakeLists.txt**

Find the `GUI_SOURCES` list (around line 200–300 in `CMakeLists.txt`, search for `src/gui/setup/DisplaySetupPages.cpp`) and add the new path right below it:

```cmake
    src/gui/setup/DisplaySetupPages.cpp
    src/gui/setup/SetupHelpers.cpp
```

- [ ] **Step 6: Build to verify it compiles**

Run:
```bash
cmake --build build --target NereusSDRObjs
```

Expected: PASS. `SetupHelpers.cpp` compiles as an empty translation unit with no warnings.

- [ ] **Step 7: Commit**

```bash
git add src/gui/setup/SetupHelpers.h src/gui/setup/SetupHelpers.cpp CMakeLists.txt
git commit -S -m "feat(setup): scaffold SetupHelpers.h/.cpp for slider readout factory

Empty skeleton — implementations and tests land in the next commit.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 2: Implement `makeSliderRow` with TDD

**Files:**
- Create: `tests/tst_setup_helpers.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `src/gui/setup/SetupHelpers.cpp`

- [ ] **Step 1: Write the failing test**

Create `tests/tst_setup_helpers.cpp`:

```cpp
// tst_setup_helpers.cpp
//
// Phase 3G-9a — unit smoke for SetupHelpers factory functions. Verifies
// bidirectional slider↔spinbox sync without signal feedback loops and that
// returned widgets match the requested range/initial/suffix.

#include <QtTest/QtTest>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSignalSpy>

#include "gui/setup/SetupHelpers.h"

using namespace NereusSDR;

class TestSetupHelpers : public QObject {
    Q_OBJECT
private slots:

    // --- makeSliderRow (int) -------------------------------------------------

    void sliderRow_honorsRangeAndInitial()
    {
        SliderRow row = makeSliderRow(10, 60, 30, QStringLiteral(" fps"));
        QVERIFY(row.slider);
        QVERIFY(row.spin);
        QVERIFY(row.container);

        QCOMPARE(row.slider->minimum(), 10);
        QCOMPARE(row.slider->maximum(), 60);
        QCOMPARE(row.slider->value(),   30);

        QCOMPARE(row.spin->minimum(), 10);
        QCOMPARE(row.spin->maximum(), 60);
        QCOMPARE(row.spin->value(),   30);
        QCOMPARE(row.spin->suffix(),  QStringLiteral(" fps"));

        delete row.container;
    }

    void sliderRow_sliderDragUpdatesSpinbox()
    {
        SliderRow row = makeSliderRow(0, 100, 50);
        row.slider->setValue(77);
        QCOMPARE(row.spin->value(), 77);
        delete row.container;
    }

    void sliderRow_spinboxEntryUpdatesSlider()
    {
        SliderRow row = makeSliderRow(0, 100, 50);
        row.spin->setValue(22);
        QCOMPARE(row.slider->value(), 22);
        delete row.container;
    }

    void sliderRow_noFeedbackLoop()
    {
        // Dragging the slider must emit slider->valueChanged exactly once,
        // not bounce back through the spinbox and emit again.
        SliderRow row = makeSliderRow(0, 100, 50);
        QSignalSpy spy(row.slider, &QSlider::valueChanged);
        row.slider->setValue(60);
        QCOMPARE(spy.count(), 1);

        // And typing into the spinbox must emit spin->valueChanged exactly once.
        QSignalSpy spinSpy(row.spin, qOverload<int>(&QSpinBox::valueChanged));
        row.spin->setValue(70);
        QCOMPARE(spinSpy.count(), 1);

        delete row.container;
    }

    // --- makeDoubleSliderRow -------------------------------------------------

    void doubleSliderRow_honorsRangeAndDecimals()
    {
        SliderRowD row = makeDoubleSliderRow(-30.0, 30.0, 0.0, 1, QStringLiteral(" dBm"));
        QVERIFY(row.slider);
        QVERIFY(row.spin);

        QCOMPARE(row.spin->minimum(),  -30.0);
        QCOMPARE(row.spin->maximum(),   30.0);
        QCOMPARE(row.spin->value(),      0.0);
        QCOMPARE(row.spin->decimals(),   1);
        QCOMPARE(row.spin->suffix(),   QStringLiteral(" dBm"));

        // Slider granularity is 10^decimals = 10 units per unit.
        QCOMPARE(row.slider->minimum(), -300);
        QCOMPARE(row.slider->maximum(),  300);
        QCOMPARE(row.slider->value(),     0);

        delete row.container;
    }

    void doubleSliderRow_bidirectionalSync()
    {
        SliderRowD row = makeDoubleSliderRow(-30.0, 30.0, 0.0, 1);
        row.spin->setValue(12.5);
        QCOMPARE(row.slider->value(), 125);

        row.slider->setValue(-75);
        QCOMPARE(row.spin->value(), -7.5);

        delete row.container;
    }
};

QTEST_MAIN(TestSetupHelpers)
#include "tst_setup_helpers.moc"
```

- [ ] **Step 2: Register test in `tests/CMakeLists.txt`**

Find the block of `nereus_add_test(...)` calls near the `tst_hardware_page_capability_gating` entry and add:

```cmake
nereus_add_test(tst_setup_helpers)
```

- [ ] **Step 3: Run test to verify it fails**

Run:
```bash
cmake --build build --target tst_setup_helpers 2>&1 | tail -20
```

Expected: BUILD FAIL with linker errors (unresolved `makeSliderRow` / `makeDoubleSliderRow`). That proves the test reaches the factory symbols.

- [ ] **Step 4: Implement `makeSliderRow`**

Replace the body of `src/gui/setup/SetupHelpers.cpp` with:

```cpp
#include "SetupHelpers.h"

#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <cmath>

namespace NereusSDR {

SliderRow makeSliderRow(int min, int max, int initial,
                        const QString& suffix,
                        QWidget* parent)
{
    auto* container = new QWidget(parent);
    auto* layout    = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(min, max);
    slider->setValue(initial);

    auto* spin = new QSpinBox(container);
    spin->setRange(min, max);
    spin->setValue(initial);
    spin->setSuffix(suffix);
    spin->setFixedWidth(80);
    spin->setAlignment(Qt::AlignRight);

    // Bidirectional sync. QSignalBlocker on the opposite widget prevents the
    // receiver from re-emitting and creating an infinite loop. Consumers of
    // the row should connect to slider->valueChanged as the single source of
    // truth for model updates.
    QObject::connect(slider, &QSlider::valueChanged, spin, [spin](int v) {
        QSignalBlocker block(spin);
        spin->setValue(v);
    });
    QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged),
                     slider, [slider](int v) {
        QSignalBlocker block(slider);
        slider->setValue(v);
    });

    layout->addWidget(slider, /*stretch=*/1);
    layout->addWidget(spin,   /*stretch=*/0);

    return { slider, spin, container };
}

SliderRowD makeDoubleSliderRow(double min, double max, double initial,
                               int decimals,
                               const QString& suffix,
                               QWidget* parent)
{
    const int scale = static_cast<int>(std::pow(10, decimals));

    auto* container = new QWidget(parent);
    auto* layout    = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(static_cast<int>(min * scale),
                     static_cast<int>(max * scale));
    slider->setValue(static_cast<int>(initial * scale));

    auto* spin = new QDoubleSpinBox(container);
    spin->setDecimals(decimals);
    spin->setRange(min, max);
    spin->setValue(initial);
    spin->setSuffix(suffix);
    spin->setSingleStep(1.0 / scale);
    spin->setFixedWidth(90);
    spin->setAlignment(Qt::AlignRight);

    QObject::connect(slider, &QSlider::valueChanged, spin, [spin, scale](int v) {
        QSignalBlocker block(spin);
        spin->setValue(static_cast<double>(v) / scale);
    });
    QObject::connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                     slider, [slider, scale](double v) {
        QSignalBlocker block(slider);
        slider->setValue(static_cast<int>(v * scale));
    });

    layout->addWidget(slider, 1);
    layout->addWidget(spin,   0);

    return { slider, spin, container };
}

} // namespace NereusSDR
```

- [ ] **Step 5: Run test to verify it passes**

Run:
```bash
cmake --build build --target tst_setup_helpers && ctest --test-dir build -R tst_setup_helpers --output-on-failure
```

Expected: all 6 test slots pass.

- [ ] **Step 6: Commit**

```bash
git add src/gui/setup/SetupHelpers.cpp tests/tst_setup_helpers.cpp tests/CMakeLists.txt
git commit -S -m "feat(setup): implement SliderRow + SliderRowD factories

Bidirectional slider↔spinbox pair with QSignalBlocker guards against
feedback loops. Unit-test covers range honor, bidirectional sync, no
feedback loop, and double-precision scaling via decimals.

Used by DisplaySetupPages in subsequent commits to give every numeric
Setup control a live readout and keyboard entry path.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: Refactor `SpectrumDefaultsPage` Sliders

**Files:**
- Modify: `src/gui/setup/DisplaySetupPages.cpp` (lines ~145–388, `SpectrumDefaultsPage::buildUI`)

**Goal:** Convert the 4 sliders in `SpectrumDefaultsPage` to use `makeSliderRow`. The refactor pattern is identical for each slider — this task shows the FPS slider in full, then lists the other three with their specific parameters.

- [ ] **Step 1: Add include for SetupHelpers.h**

At the top of `src/gui/setup/DisplaySetupPages.cpp`, add the include right after the `DisplaySetupPages.h` include:

```cpp
#include "DisplaySetupPages.h"
#include "SetupHelpers.h"
#include "gui/ColorSwatchButton.h"
// ... rest of existing includes
```

- [ ] **Step 2: Refactor the FPS slider (reference pattern)**

Find this block in `SpectrumDefaultsPage::buildUI()` (around line 194–198):

```cpp
    m_fpSlider = new QSlider(Qt::Horizontal, renderGroup);
    m_fpSlider->setRange(10, 60);
    m_fpSlider->setValue(30);
    connect(m_fpSlider, &QSlider::valueChanged, this, [this](int v) { pushFps(v); });
    renderForm->addRow(QStringLiteral("FPS (10–60):"), m_fpSlider);
```

Replace with:

```cpp
    {
        auto row = makeSliderRow(10, 60, 30, QStringLiteral(" fps"), renderGroup);
        m_fpSlider = row.slider;
        connect(m_fpSlider, &QSlider::valueChanged, this, [this](int v) { pushFps(v); });
        renderForm->addRow(QStringLiteral("FPS:"), row.container);
    }
```

Note the braces: they scope `row` so its name can be reused in subsequent slider blocks without shadowing complaints. `m_fpSlider` now points at the slider half of the pair — the spinbox is owned by the row container and stays in sync automatically.

- [ ] **Step 3: Refactor `m_fillAlphaSlider` (0–100, "%")**

Find (around line 241–248):

```cpp
    m_fillAlphaSlider = new QSlider(Qt::Horizontal, renderGroup);
    m_fillAlphaSlider->setRange(0, 100);
    m_fillAlphaSlider->setValue(70);
    connect(m_fillAlphaSlider, &QSlider::valueChanged, this, [this](int v) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setFillAlpha(v / 100.0f);
        }
    });
    renderForm->addRow(QStringLiteral("Fill Alpha:"), m_fillAlphaSlider);
```

Replace with:

```cpp
    {
        auto row = makeSliderRow(0, 100, 70, QStringLiteral("%"), renderGroup);
        m_fillAlphaSlider = row.slider;
        connect(m_fillAlphaSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setFillAlpha(v / 100.0f);
            }
        });
        renderForm->addRow(QStringLiteral("Fill Alpha:"), row.container);
    }
```

- [ ] **Step 4: Refactor `m_lineWidthSlider` (1–3, " px")**

Find (around line 251–258):

```cpp
    m_lineWidthSlider = new QSlider(Qt::Horizontal, renderGroup);
    m_lineWidthSlider->setRange(1, 3);
    m_lineWidthSlider->setValue(1);
    connect(m_lineWidthSlider, &QSlider::valueChanged, this, [this](int v) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setLineWidth(static_cast<float>(v));
        }
    });
    renderForm->addRow(QStringLiteral("Line Width:"), m_lineWidthSlider);
```

Replace with:

```cpp
    {
        auto row = makeSliderRow(1, 3, 1, QStringLiteral(" px"), renderGroup);
        m_lineWidthSlider = row.slider;
        connect(m_lineWidthSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setLineWidth(static_cast<float>(v));
            }
        });
        renderForm->addRow(QStringLiteral("Line Width:"), row.container);
    }
```

- [ ] **Step 5: Refactor `m_dataLineAlphaSlider` (0–255, no suffix)**

Find (around line 290–300):

```cpp
    m_dataLineAlphaSlider = new QSlider(Qt::Horizontal, colorGroup);
    m_dataLineAlphaSlider->setRange(0, 255);
    m_dataLineAlphaSlider->setValue(230);
    connect(m_dataLineAlphaSlider, &QSlider::valueChanged, this, [this](int a) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            QColor c = w->fillColor();
            c.setAlpha(a);
            w->setFillColor(c);
        }
    });
    colorForm->addRow(QStringLiteral("Line Alpha:"), m_dataLineAlphaSlider);
```

Replace with:

```cpp
    {
        auto row = makeSliderRow(0, 255, 230, QString(), colorGroup);
        m_dataLineAlphaSlider = row.slider;
        connect(m_dataLineAlphaSlider, &QSlider::valueChanged, this, [this](int a) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                QColor c = w->fillColor();
                c.setAlpha(a);
                w->setFillColor(c);
            }
        });
        colorForm->addRow(QStringLiteral("Line Alpha:"), row.container);
    }
```

- [ ] **Step 6: Build, launch, manually verify**

Run:
```bash
cmake --build build --target NereusSDR 2>&1 | tail -5
pkill -x NereusSDR 2>/dev/null; sleep 0.3
open build/NereusSDR.app
```

Open Setup → Display → Spectrum Defaults. Verify:
- Each of the 4 refactored sliders shows a spinbox to its right with the correct suffix.
- Dragging each slider updates the spinbox.
- Typing into each spinbox updates the slider.
- The underlying behavior (FPS changes, fill alpha changes, etc.) still fires — drag each and watch the spectrum react.

- [ ] **Step 7: Run smoke test suite**

Run:
```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -15
```

Expected: all tests pass, including `tst_setup_helpers` and `tst_smoke`.

- [ ] **Step 8: Commit**

```bash
git add src/gui/setup/DisplaySetupPages.cpp
git commit -S -m "refactor(setup): SpectrumDefaultsPage uses SliderRow helpers

Converts m_fpSlider, m_fillAlphaSlider, m_lineWidthSlider, and
m_dataLineAlphaSlider to use makeSliderRow(), giving each slider a
live numeric spinbox readout with unit suffix and keyboard entry path.
No behavior changes — the backing connects() still drive the same
FFTEngine / SpectrumWidget setters.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 4: Refactor `WaterfallDefaultsPage` Sliders

**Files:**
- Modify: `src/gui/setup/DisplaySetupPages.cpp` (`WaterfallDefaultsPage::buildUI`)

**Goal:** Convert the 4 sliders in `WaterfallDefaultsPage` using the same pattern as Task 3.

- [ ] **Step 1: Locate `WaterfallDefaultsPage::buildUI()`**

Search `src/gui/setup/DisplaySetupPages.cpp` for `void WaterfallDefaultsPage::buildUI()`. The 4 sliders are `m_highThresholdSlider`, `m_lowThresholdSlider`, `m_updatePeriodSlider`, `m_opacitySlider`.

- [ ] **Step 2: Refactor `m_highThresholdSlider` (-200 to 0, " dBm", initial -40)**

Find the existing `m_highThresholdSlider = new QSlider(...)` block and replace it with:

```cpp
    {
        auto row = makeSliderRow(-200, 0, -40, QStringLiteral(" dBm"), levelsGroup);
        m_highThresholdSlider = row.slider;
        connect(m_highThresholdSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setWaterfallHighThreshold(static_cast<float>(v));
            }
        });
        levelsForm->addRow(QStringLiteral("High Threshold:"), row.container);
    }
```

If the existing block uses a different group/form variable name, substitute. Preserve the existing connect body — copy the lambda from the `connect(m_highThresholdSlider, ...)` line verbatim. Initial value `-40` is NereusSDR's current default; do not change it in this PR (that's PR2's job).

- [ ] **Step 3: Refactor `m_lowThresholdSlider` (-200 to 0, " dBm", initial -130)**

Apply the same pattern. Initial value -130. Suffix " dBm".

```cpp
    {
        auto row = makeSliderRow(-200, 0, -130, QStringLiteral(" dBm"), levelsGroup);
        m_lowThresholdSlider = row.slider;
        connect(m_lowThresholdSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setWaterfallLowThreshold(static_cast<float>(v));
            }
        });
        levelsForm->addRow(QStringLiteral("Low Threshold:"), row.container);
    }
```

- [ ] **Step 4: Refactor `m_updatePeriodSlider` (10–500, " ms", initial 50)**

```cpp
    {
        auto row = makeSliderRow(10, 500, 50, QStringLiteral(" ms"), displayGroup);
        m_updatePeriodSlider = row.slider;
        connect(m_updatePeriodSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setWaterfallUpdatePeriod(v);
            }
        });
        displayForm->addRow(QStringLiteral("Update Period:"), row.container);
    }
```

If the existing connect body calls a different SpectrumWidget setter, preserve the original call.

- [ ] **Step 5: Refactor `m_opacitySlider` (0–100, "%", initial 100)**

```cpp
    {
        auto row = makeSliderRow(0, 100, 100, QStringLiteral("%"), displayGroup);
        m_opacitySlider = row.slider;
        connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setWaterfallOpacity(v / 100.0f);
            }
        });
        displayForm->addRow(QStringLiteral("Opacity:"), row.container);
    }
```

Preserve whatever scale conversion the existing connect used (some setters take 0–100, some take 0.0–1.0 — check the existing code and keep it).

- [ ] **Step 6: Build and launch**

```bash
cmake --build build --target NereusSDR 2>&1 | tail -5
pkill -x NereusSDR 2>/dev/null; sleep 0.3
open build/NereusSDR.app
```

Open Setup → Display → Waterfall Defaults. Drag each of the 4 refactored sliders and confirm (a) the spinbox tracks, (b) typing in the spinbox updates the slider, (c) the waterfall reacts live.

- [ ] **Step 7: Run tests**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -10
```

Expected: all pass.

- [ ] **Step 8: Commit**

```bash
git add src/gui/setup/DisplaySetupPages.cpp
git commit -S -m "refactor(setup): WaterfallDefaultsPage uses SliderRow helpers

Converts m_highThresholdSlider, m_lowThresholdSlider,
m_updatePeriodSlider, and m_opacitySlider to use makeSliderRow().
Each slider now has a live numeric spinbox with unit suffix.
No behavior changes.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 5: Source-First Citations + Tooltip Port — SpectrumDefaultsPage

**Files:**
- Modify: `src/gui/setup/DisplaySetupPages.cpp` (`SpectrumDefaultsPage` section)

**Goal:** Add per-control `// Thetis: setup.designer.cs:NNNN (controlName)` citation comments above every `connect(...)` block in `SpectrumDefaultsPage::buildUI()`, plus a `setToolTip(...)` call for every control following the tooltip decision rule above.

This is a mechanical pass. The workflow is: for each control, grep Thetis for the control name, extract tooltip, classify, write comment + tooltip.

**Reference:** The 3G-8 plan's §3.1/§3.2 already identified the Thetis control name for all 17 `SpectrumDefaultsPage` controls. Use that table as the starting point. Re-verify each line number against current `../Thetis/` state — line numbers may have drifted.

- [ ] **Step 1: Extract Thetis tooltip for `udDisplayFPS`**

Run:
```bash
grep -n "udDisplayFPS" "../Thetis/Project Files/Source/Console/setup.designer.cs" | head -5
grep -n 'SetToolTip(this.udDisplayFPS' "../Thetis/Project Files/Source/Console/setup.designer.cs"
```

Expected: find the `toolTip1.SetToolTip(this.udDisplayFPS, "...");` line. Copy the string.

Classify it:
- Substantive (>10 chars, useful) → port verbatim with attribution comment.
- Empty/echo → rewrite with comment citing original.

- [ ] **Step 2: Annotate FPS slider**

Above the `connect(m_fpSlider, ...)` block from Task 3, add:

```cpp
    // Thetis: setup.designer.cs:NNNN (udDisplayFPS) → console.DisplayFPS
    {
        auto row = makeSliderRow(10, 60, 30, QStringLiteral(" fps"), renderGroup);
        m_fpSlider = row.slider;
        m_fpSlider->setToolTip(QStringLiteral(
            "<EXACT THETIS STRING OR REWRITE>"));
        // If rewritten: // Tooltip rewritten — Thetis original: "<original>"
        connect(m_fpSlider, &QSlider::valueChanged, this, [this](int v) { pushFps(v); });
        renderForm->addRow(QStringLiteral("FPS:"), row.container);
    }
```

Replace `NNNN` with the actual line number from step 1. Replace `<EXACT THETIS STRING OR REWRITE>` with the actual tooltip text.

- [ ] **Step 3: Repeat steps 1–2 for every control in `SpectrumDefaultsPage`**

17 controls total. The Thetis names are (from 3G-8 plan §3.1/§3.2):

| Member | Thetis control name | Type |
|---|---|---|
| `m_fftSizeCombo` | `tbDisplayFFTSize` | Thetis (TrackBar → we use combo) |
| `m_windowCombo` | — | **NereusSDR extension** — write new tooltip |
| `m_fpSlider` | `udDisplayFPS` | Thetis |
| `m_averagingCombo` | `comboDispPanAveraging` | Thetis |
| `m_averagingTimeSpin` | `udDisplayAVGTime` | Thetis |
| `m_decimationSpin` | `udDisplayDecimation` | Thetis |
| `m_fillToggle` | `chkDisplayPanFill` | Thetis |
| `m_fillAlphaSlider` | `tbDataFillAlpha` | Thetis |
| `m_lineWidthSlider` | `udDisplayLineWidth` | Thetis |
| `m_gradientToggle` | `chkDataLineGradient` | Thetis |
| `m_dataLineColorBtn` | `clrbtnDataLine` | Thetis |
| `m_dataLineAlphaSlider` | `tbDataLineAlpha` | Thetis |
| `m_dataFillColorBtn` | `clrbtnDataFill` | Thetis |
| `m_calOffsetSpin` | (programmatic only, `Display.RX1DisplayCalOffset`) | Thetis — no designer tooltip; write useful one, cite `display.cs:1372` |
| `m_peakHoldToggle` | — | **NereusSDR extension** |
| `m_peakHoldDelaySpin` | — | **NereusSDR extension** |
| `m_threadPriorityCombo` | `comboDisplayThreadPriority` | Thetis |

For each, run the same grep pattern, extract, classify, paste with the appropriate citation + tooltip comment. Use single `setToolTip(QStringLiteral("..."))` calls on the widget right after it's constructed (or after the row unwraps for sliders).

**Tooltip strings must never span multiple lines in the source** — if a Thetis tooltip has a newline, use `\n` inline. Keep the QStringLiteral on one logical C++ line or use adjacent string concatenation.

- [ ] **Step 4: Build and verify tooltips appear**

```bash
cmake --build build --target NereusSDR 2>&1 | tail -5
pkill -x NereusSDR 2>/dev/null; sleep 0.3
open build/NereusSDR.app
```

Open Setup → Display → Spectrum Defaults. Hover every control for ~1 second. Confirm each one shows a tooltip that reads sensibly.

- [ ] **Step 5: Commit**

```bash
git add src/gui/setup/DisplaySetupPages.cpp
git commit -S -m "feat(setup): tooltips + Thetis citations for SpectrumDefaultsPage

Every control in SpectrumDefaultsPage now has:
- A // Thetis: setup.designer.cs:NNNN (controlName) citation comment
  above its connect(...) block (or 'NereusSDR extension' for
  project-specific controls).
- A setToolTip() call, verbatim from Thetis where the upstream is
  substantive, rewritten where Thetis was empty or echoed the label,
  and written from scratch for the 3 NereusSDR extensions.

17 controls, no behavior changes.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 6: Source-First Citations + Tooltip Port — WaterfallDefaultsPage

**Files:**
- Modify: `src/gui/setup/DisplaySetupPages.cpp` (`WaterfallDefaultsPage` section)

**Goal:** Same pass as Task 5, for the 17 controls in `WaterfallDefaultsPage`.

**Reference table** (from 3G-8 plan §4.1/§4.2):

| Member | Thetis control name | Type |
|---|---|---|
| `m_highThresholdSlider` | `udDisplayWaterfallHighLevel` | Thetis |
| `m_lowThresholdSlider` | `udDisplayWaterfallLowLevel` | Thetis |
| `m_agcToggle` | `chkRX1WaterfallAGC` | Thetis |
| `m_lowColorBtn` | `clrbtnWaterfallLow` | Thetis |
| `m_useSpectrumMinMaxToggle` | `chkWaterfallUseRX1SpectrumMinMax` | Thetis |
| `m_updatePeriodSlider` | `udDisplayWaterfallUpdatePeriod` | Thetis |
| `m_reverseToggle` | — | **NereusSDR extension** |
| `m_opacitySlider` | `tbRX1WaterfallOpacity` | Thetis |
| `m_colorSchemeCombo` | `comboColorPalette` | Thetis |
| `m_wfAveragingCombo` | `comboDispWFAveraging` | Thetis |
| `m_showRxFilterToggle` | `chkShowRXFilterOnWaterfall` | Thetis |
| `m_showTxFilterToggle` | `chkShowTXFilterOnRXWaterfall` | Thetis |
| `m_showRxZeroLineToggle` | `chkShowRXZeroLineOnWaterfall` | Thetis |
| `m_showTxZeroLineToggle` | `chkShowTXZeroLineOnWaterfall` | Thetis |
| `m_timestampPosCombo` | — | **NereusSDR extension** |
| `m_timestampModeCombo` | — | **NereusSDR extension** |

- [ ] **Step 1: Walk the 17 controls**

For each row in the table: grep Thetis designer for the control name, extract SetToolTip string, classify, paste citation comment + `setToolTip(...)` call in `WaterfallDefaultsPage::buildUI()`. Use the same pattern as Task 5.

- [ ] **Step 2: Build + hover-check every control**

```bash
cmake --build build --target NereusSDR 2>&1 | tail -5
pkill -x NereusSDR 2>/dev/null; sleep 0.3
open build/NereusSDR.app
```

Setup → Display → Waterfall Defaults. Hover all 17 controls, confirm tooltips appear.

- [ ] **Step 3: Commit**

```bash
git add src/gui/setup/DisplaySetupPages.cpp
git commit -S -m "feat(setup): tooltips + Thetis citations for WaterfallDefaultsPage

Same pattern as SpectrumDefaultsPage — 17 controls, each with a source
citation comment and a tooltip (Thetis-verbatim, rewritten, or new for
NereusSDR extensions: Reverse Scroll, Timestamp Position, Timestamp Mode).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 7: Source-First Citations + Tooltip Port — GridScalesPage

**Files:**
- Modify: `src/gui/setup/DisplaySetupPages.cpp` (`GridScalesPage` section)

**Goal:** Last of the three pages. 13 controls, no sliders (all combos/spinboxes/checkboxes/color swatches).

**Reference table** (from 3G-8 plan §5.1/§5.2):

| Member | Thetis control name | Type |
|---|---|---|
| `m_gridToggle` | `chkGridControl` | Thetis |
| `m_dbMaxSpin` | `udDisplayGridMax` | Thetis (per-band) |
| `m_dbMinSpin` | `udDisplayGridMin` | Thetis (per-band) |
| `m_dbStepSpin` | `udDisplayGridStep` | Thetis (global) |
| `m_freqLabelAlignCombo` | `comboDisplayLabelAlign` | Thetis |
| `m_zeroLineToggle` | `chkShowZeroLine` | Thetis |
| `m_showFpsToggle` | `chkShowFPS` | Thetis |
| `m_gridColorBtn` | `clrbtnGrid` | Thetis |
| `m_gridFineColorBtn` | `clrbtnGridFine` | Thetis |
| `m_hGridColorBtn` | `clrbtnHGridColor` | Thetis |
| `m_gridTextColorBtn` | `clrbtnText` | Thetis |
| `m_zeroLineColorBtn` | `clrbtnZeroLine` | Thetis |
| `m_bandEdgeColorBtn` | `clrbtnBandEdge` | Thetis |

All 13 are Thetis-sourced; no extensions. Note the per-band dB Max / dB Min slots — the tooltip should mention "edits the current band's grid slot; see band indicator above."

- [ ] **Step 1: Walk the 13 controls**

Same workflow as Tasks 5–6. grep → extract → classify → paste.

- [ ] **Step 2: Build + hover-check**

```bash
cmake --build build --target NereusSDR 2>&1 | tail -5
pkill -x NereusSDR 2>/dev/null; sleep 0.3
open build/NereusSDR.app
```

Setup → Display → Grid & Scales. Hover all 13 controls.

- [ ] **Step 3: Full test suite**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -15
```

Expected: all green, including `tst_setup_helpers` and `tst_smoke`.

- [ ] **Step 4: Tooltip coverage audit**

```bash
grep -c "setToolTip" src/gui/setup/DisplaySetupPages.cpp
```

Expected: at least 47 (one per control). Can be more if any control uses multiple tooltips (e.g., on label + widget).

- [ ] **Step 5: Commit**

```bash
git add src/gui/setup/DisplaySetupPages.cpp
git commit -S -m "feat(setup): tooltips + Thetis citations for GridScalesPage

Final page — 13 controls, all Thetis-sourced (no extensions). Per-band
dB Max / dB Min tooltips note that they edit the currently-active band
slot. Brings PR1 tooltip coverage to 47/47.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 8: Final Verification + PR Draft

**Files:** none modified. This task validates the branch end-to-end.

- [ ] **Step 1: Clean rebuild from scratch**

```bash
cmake --build build --target clean
cmake --build build 2>&1 | tail -10
```

Expected: build green, no warnings on the touched files.

- [ ] **Step 2: Full test suite**

```bash
ctest --test-dir build --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 3: Launch + manual sweep**

```bash
pkill -x NereusSDR 2>/dev/null; sleep 0.3
open build/NereusSDR.app
```

Open Setup → Display. For each of the three pages:
- Every slider has a spinbox readout with a unit suffix.
- Drag slider → spinbox tracks. Type in spinbox → slider tracks.
- Hover every control → tooltip appears, reads sensibly, no `<EXACT THETIS STRING OR REWRITE>` placeholders.
- Every connect target still fires (spectrum/waterfall react live to every change).

- [ ] **Step 4: Diff review**

```bash
git log --oneline main..HEAD
git diff main..HEAD --stat
```

Expected: ~7 commits, `DisplaySetupPages.cpp` delta around ~600 lines (mostly additive tooltip/citation blocks), new `SetupHelpers.{h,cpp}` ~200 lines, new `tst_setup_helpers.cpp` ~100 lines.

Visually skim the diff for: missing braces, leftover placeholder strings, tooltip typos, accidental behavior changes (any `connect(...)` lambda bodies should be unchanged from pre-refactor).

- [ ] **Step 5: Draft the PR description**

Write to a scratch file (don't post yet — per standing preference, user reviews all public GitHub posts):

```markdown
# Phase 3G-9a — Source-First Audit + Tooltips + Slider Readouts

First of three PRs under Phase 3G-9 (Display Refactor). Design spec:
`docs/architecture/2026-04-15-display-refactor-design.md` §4.

## What

- Added `SetupHelpers.h/.cpp` with `makeSliderRow` / `makeDoubleSliderRow`
  factories. Each returns a bidirectionally-synced slider+spinbox pair with
  unit suffix, guarded against signal feedback loops via QSignalBlocker.
- Refactored all 8 sliders in `SpectrumDefaultsPage` and `WaterfallDefaultsPage`
  to use the helpers. Every numeric Setup control now has a live spinbox readout
  and a keyboard entry path.
- Added `setToolTip(...)` calls for all 47 controls across the three Setup pages
  (Spectrum 17, Waterfall 17, Grid & Scales 13). Tooltip text is verbatim from
  Thetis `setup.designer.cs` where the upstream string is substantive, rewritten
  (with the original recorded in an adjacent comment) where Thetis was weak, and
  written from scratch for the 6 NereusSDR-original extensions.
- Added `// Thetis: setup.designer.cs:NNNN (controlName)` source-first citation
  comments above every `connect(...)` block — the source-first protocol is now
  self-documenting in the code.

## Why

Phase 3G-8 shipped working wire-up for 47 controls. This PR closes the polish
gap: missing tooltips (10/47 → 47/47), missing source citations (0/47 → 47/47),
and slider-only controls that forced mouse-only input with no numeric readout.

## Testing

- New `tst_setup_helpers` QtTest smoke: 6 assertions covering range honor,
  bidirectional sync, no feedback loops, double-precision scaling.
- Existing tests stay green.
- Manual: hovered every control across all three pages, dragged every slider
  and typed into every spinbox to verify bidirectional sync.

## Scope

Explicitly out of scope: RX2 Display, TX Display, Spectrum Overlay flyouts,
meter editors. These deferred per spec §2.

## Next

PR2 (3G-9b) — smooth defaults + Clarity Blue palette + tuning doc.
PR3 (3G-9c) — Clarity adaptive display tuning (research-doc gated).
```

- [ ] **Step 6: Push the branch and open the PR**

```bash
git push -u origin feature/display-audit-pr1
```

Then per standing preference, show the PR description draft in the terminal and wait for user approval before running `gh pr create`.

---

## Commit Shape Recap

Expected final commit log on `feature/display-audit-pr1`:

1. `feat(setup): scaffold SetupHelpers.h/.cpp for slider readout factory` (Task 1)
2. `feat(setup): implement SliderRow + SliderRowD factories` (Task 2)
3. `refactor(setup): SpectrumDefaultsPage uses SliderRow helpers` (Task 3)
4. `refactor(setup): WaterfallDefaultsPage uses SliderRow helpers` (Task 4)
5. `feat(setup): tooltips + Thetis citations for SpectrumDefaultsPage` (Task 5)
6. `feat(setup): tooltips + Thetis citations for WaterfallDefaultsPage` (Task 6)
7. `feat(setup): tooltips + Thetis citations for GridScalesPage` (Task 7)

7 GPG-signed commits, no `--no-verify`, no `--amend`. Task 8 is verification only and produces no commit.
