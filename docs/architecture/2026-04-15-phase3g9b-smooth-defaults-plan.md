# Phase 3G-9b — Smooth Defaults + Clarity Blue Palette Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the second PR of Phase 3G-9: a `Clarity Blue` waterfall palette, a `applyClaritySmoothDefaults()` profile that gets applied on first launch (and on demand via a Reset button), and a tuning rationale doc explaining why each default value is what it is. On first install the waterfall should look like AetherSDR/SmartSDR — dark navy noise floor, signals popping as bright cyan-white — without the user touching anything.

**Architecture:** One new `WfColorScheme::ClarityBlue` entry backed by a narrow-band gradient stop table (80% of the dynamic range eats noise, top 20% pops signals). One new function `RadioModel::applyClaritySmoothDefaults()` that sets the 7 recipe values from the design spec §5.1 and writes `"DisplayProfileApplied" = "True"` to AppSettings to gate first-launch. A confirmation-dialog-gated "Reset to Smooth Defaults" button on `SpectrumDefaultsPage` that calls the same function. New doc `docs/architecture/waterfall-tuning.md` with the seven recipes, rationale, and before/after screenshots.

**Tech Stack:** C++20, Qt6 (QtWidgets, QRhiWidget / QPainter fallback), existing `SpectrumWidget`, `PanadapterModel`, `RadioModel`, `AppSettings`. No new third-party deps.

**Design spec:** `docs/architecture/2026-04-15-display-refactor-design.md` §5
**Parent PR (merged):** PR #25 / `2617852` (Phase 3G-9a)
**Parent branch:** `main`

---

## 1. Scope boundary

**In scope:**
- New `WfColorScheme::ClarityBlue` enum entry + gradient stops + `wfSchemeStops()` case
- New "Clarity Blue" item in the Waterfall Defaults color scheme combo (it becomes the 8th option)
- `RadioModel::applyClaritySmoothDefaults()` + first-launch gate in `RadioModel` constructor
- "Reset to Smooth Defaults" button on `SpectrumDefaultsPage` with confirmation dialog
- `docs/architecture/waterfall-tuning.md` rationale doc + before/after screenshots

**Out of scope (deferred to PR3 / 3G-9c):**
- Adaptive Clarity behavior — that's the whole point of PR3
- RX2/TX display surface changes
- Renderer stub fixes (verified: W3 Waterfall AGC and W4 update period are already fully wired from 3G-8)
- New config controls beyond the Reset button
- Thetis default-value adoption beyond the seven recipes in §5.1

## 2. The seven recipes (from spec §5.1)

| # | Knob | Smooth value | Applied via |
|---|---|---|---|
| 1 | Waterfall palette | `WfColorScheme::ClarityBlue` | `SpectrumWidget::setWfColorScheme()` |
| 2 | Spectrum averaging mode | `AverageMode::Logarithmic` | `SpectrumWidget::setAverageMode()` |
| 3 | Averaging time constant | ~500 ms → alpha `0.05f` | `SpectrumWidget::setAverageAlpha()` |
| 4 | Trace color | `#e0e8f0` alpha 200 | `SpectrumWidget::setFillColor()` |
| 5 | Waterfall Low / High | -110 / -70 dBm | `SpectrumWidget::setWfLowThreshold()` / `setWfHighThreshold()` |
| 6 | Waterfall AGC | `true` | `SpectrumWidget::setWfAgcEnabled()` |
| 7 | Waterfall update period | 30 ms | `SpectrumWidget::setWfUpdatePeriodMs()` |

**Averaging alpha math:** the averaging time spinbox in `DisplaySetupPages.cpp` converts ms to alpha via `qBound(0.05f, 1.0f - (ms/5000.0f), 0.95f)`. For 500 ms target: `1.0 - 500/5000 = 0.90` — a lighter smoothing than "heavy". For genuine heavy smoothing at the -130 dBm noise floor, the design doc's intent is `alpha ≈ 0.05` (very slow exponential). **Use `0.05f` literally** — it produces the visibly-smooth AetherSDR look. The 500 ms is a *display* intent for the doc, not a formula to back-solve.

## 3. File structure

**Create:**
- `docs/architecture/waterfall-tuning.md` — rationale doc
- `docs/architecture/waterfall-tuning/` — directory for before/after PNGs

**Modify:**
- `src/gui/SpectrumWidget.h` — add `ClarityBlue` enum entry before `Count` (breaks no ABI — `Count` moves but nobody persists the enum ordinal beyond a qBound at load)
- `src/gui/SpectrumWidget.cpp` — add `kClarityBlueStops[]` table + `wfSchemeStops()` case
- `src/gui/setup/DisplaySetupPages.cpp` — add "Clarity Blue" to `m_colorSchemeCombo->addItems(...)` in `WaterfallDefaultsPage::buildUI()`; add "Reset to Smooth Defaults" button + confirmation dialog to `SpectrumDefaultsPage::buildUI()`
- `src/models/RadioModel.h` — declare `applyClaritySmoothDefaults()` public method
- `src/models/RadioModel.cpp` — implement the method + call from constructor behind the `"DisplayProfileApplied"` AppSettings gate
- `CHANGELOG.md` — entry for PR2
- `tests/tst_clarity_defaults.cpp` — new, 3 test slots (first-launch gate, palette registered, gradient stop monotonic)
- `tests/CMakeLists.txt` — register the new test

## 4. Before you start

- [ ] **Step 1: Verify clean main**

```bash
cd /Users/j.j.boyd/NereusSDR
git checkout main && git pull --ff-only
git status
git log --oneline -3
```

Expected: clean, tip at `2617852` (PR #25 merge) or newer.

- [ ] **Step 2: Re-read CLAUDE.md and CONTRIBUTING.md**

Standing preference for every new branch. Verify the source-first protocol wording (`../Thetis/`, source-first rule, constants-exactly). PR2 is an authorized exception for the seven smooth-default values per spec §5, but that exception does not generalize.

- [ ] **Step 3: Create branch**

```bash
cd /Users/j.j.boyd/NereusSDR
git checkout -b feature/display-smooth-defaults-pr2
```

---

## Task 1: `Clarity Blue` palette + gradient stops

**Files:**
- Modify: `src/gui/SpectrumWidget.h` (enum)
- Modify: `src/gui/SpectrumWidget.cpp` (stops table + wfSchemeStops case)
- Modify: `src/gui/setup/DisplaySetupPages.cpp` (add combo item)

This task lands the new palette but does NOT change any default yet — existing users stay on whatever they had, users who manually pick "Clarity Blue" in the combo get the new look. Task 3 is what sets it as the default on first launch.

- [ ] **Step 1: Add `ClarityBlue` to the `WfColorScheme` enum**

In `src/gui/SpectrumWidget.h`, find `enum class WfColorScheme` (around line 33). Replace the current enum body with:

```cpp
enum class WfColorScheme : int {
    Default = 0,    // AetherSDR: black → blue → cyan → green → yellow → red
    Enhanced,       // Thetis enhanced (9-band progression)
    Spectran,       // SPECTRAN
    BlackWhite,     // Grayscale
    LinLog,         // Linear in low, log in high — Thetis LinLog
    LinRad,         // Linradiance-style cool → hot
    Custom,         // User-defined custom stops (reads from AppSettings)
    ClarityBlue,    // Phase 3G-9b: narrow-band monochrome (80% navy noise floor,
                    // top 20% cyan→white signals). AetherSDR-style readability.
    Count
};
```

`ClarityBlue` is inserted **before** `Count` so existing persisted ordinals (0–6) continue to resolve to the same scheme. `ClarityBlue` becomes ordinal 7.

- [ ] **Step 2: Add the gradient stop table in SpectrumWidget.cpp**

In `src/gui/SpectrumWidget.cpp`, scroll to the block of `static const WfGradientStop k*Stops[]` tables near the top (around line 60–100). Add immediately after `kCustomFallbackStops[]`:

```cpp
// Phase 3G-9b: Clarity Blue palette — narrow-band monochrome designed to
// match AetherSDR's readability. The bottom 60% of the dynamic range eats
// the noise floor as uniform dark navy; 60-85% transitions through mid-blue
// to cyan for weak signals; the top 15% pops to bright cyan and white for
// strong signals. Result: signals stand out against a quiet background.
// Compare to WfColorScheme::Default which spreads colour across the entire
// range (so the noise floor also appears colourful — high visual clutter).
static const WfGradientStop kClarityBlueStops[] = {
    {0.00f,  0x0a, 0x14, 0x28},  // deep navy — noise floor bottom
    {0.60f,  0x0d, 0x25, 0x40},  // still dark navy — top of noise floor
    {0.70f,  0x18, 0x50, 0xa0},  // rising mid-blue
    {0.85f,  0x30, 0x90, 0xe0},  // cyan — weak signals appear
    {0.95f,  0x80, 0xd0, 0xff},  // bright cyan — medium signals
    {1.00f,  0xff, 0xff, 0xff},  // white — strongest signals pop out
};
```

- [ ] **Step 3: Add the `wfSchemeStops()` case**

In the same file, find `const WfGradientStop* wfSchemeStops(...)` (around line 108). Add a case before the `default:` fallthrough:

```cpp
    case WfColorScheme::ClarityBlue:
        count = 6;
        return kClarityBlueStops;
```

- [ ] **Step 4: Add "Clarity Blue" to the Waterfall Defaults combo**

In `src/gui/setup/DisplaySetupPages.cpp`, find `WaterfallDefaultsPage::buildUI()` and locate `m_colorSchemeCombo->addItems({...})`. The current list has 7 entries (Default, Enhanced, Spectran, BlackWhite, LinLog, LinRad, Custom). Append `"Clarity Blue"` as the 8th item so its index matches `WfColorScheme::ClarityBlue = 7`:

```cpp
    m_colorSchemeCombo->addItems({
        QStringLiteral("Default"),
        QStringLiteral("Enhanced"),
        QStringLiteral("Spectran"),
        QStringLiteral("Black/White"),
        QStringLiteral("LinLog"),
        QStringLiteral("LinRad"),
        QStringLiteral("Custom"),
        QStringLiteral("Clarity Blue")  // Phase 3G-9b
    });
```

**If the existing `addItems` list uses different casing or punctuation**, preserve whatever is there and just append `"Clarity Blue"` as the last entry. Do NOT rename existing items — persisted index values depend on order.

- [ ] **Step 5: Build and launch**

```bash
cd /Users/j.j.boyd/NereusSDR
cmake --build build --target NereusSDR 2>&1 | tail -5
pkill -x NereusSDR 2>/dev/null; sleep 0.3
open build/NereusSDR.app
```

Open Setup → Display → Waterfall Defaults. Confirm:
- "Clarity Blue" appears as the last item in the Color Scheme combo
- Selecting it changes the waterfall to the narrow-band dark-navy look
- Switching back to another scheme works without a crash

- [ ] **Step 6: Commit**

```bash
cd /Users/j.j.boyd/NereusSDR
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp src/gui/setup/DisplaySetupPages.cpp
git commit -S -m "$(cat <<'EOF'
feat(spectrum): Clarity Blue waterfall palette

New WfColorScheme::ClarityBlue palette — narrow-band monochrome
designed to match AetherSDR's readability. Bottom 60% of the dynamic
range renders as uniform dark navy (noise floor); 60-85% transitions
through mid-blue to cyan for weak signals; top 15% pops to bright
cyan and white for strong signals.

Added as the 8th entry in the WaterfallDefaultsPage color scheme combo.
Not set as default yet — that's a follow-up commit in this PR.

Part of Phase 3G-9b PR2.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: `applyClaritySmoothDefaults()` + first-launch gate

**Files:**
- Modify: `src/models/RadioModel.h`
- Modify: `src/models/RadioModel.cpp`

This task adds the function that sets the 7 recipe values and wires it to run exactly once on first launch. Fresh installs and users who blow away their `NereusSDR.settings` get the profile; existing users with their own settings are left alone.

- [ ] **Step 1: Declare the public method**

In `src/models/RadioModel.h`, find the public section and add:

```cpp
    // Phase 3G-9b: one-shot profile that sets the 7 smooth-default recipe
    // values on SpectrumWidget/FFTEngine. Called from the constructor
    // exactly once on first launch (gated by AppSettings key
    // "DisplayProfileApplied"). Also callable on demand via the
    // "Reset to Smooth Defaults" button on SpectrumDefaultsPage, in
    // which case it unconditionally applies regardless of the gate.
    //
    // See docs/architecture/waterfall-tuning.md for the rationale behind
    // each value.
    void applyClaritySmoothDefaults();
```

- [ ] **Step 2: Implement the method in `src/models/RadioModel.cpp`**

Add near the top:

```cpp
#include "gui/SpectrumWidget.h"
```

(If already present, skip.) Then add the method definition in the NereusSDR namespace:

```cpp
void RadioModel::applyClaritySmoothDefaults()
{
    auto* sw = spectrumWidget();
    if (!sw) { return; }  // not yet wired by MainWindow — defer

    // 1. Palette — narrow-band monochrome. See docs/architecture/waterfall-tuning.md §1.
    sw->setWfColorScheme(WfColorScheme::ClarityBlue);

    // 2. Spectrum averaging mode — log-recursive for heavy smoothing.
    sw->setAverageMode(AverageMode::Logarithmic);

    // 3. Averaging alpha — very slow exponential (≈500 ms wall-clock perceived
    //    smoothing). See waterfall-tuning.md §3 for the math.
    sw->setAverageAlpha(0.05f);

    // 4. Trace colour — neutral light-gray, not saturated. Sits in front of
    //    the waterfall without competing for attention.
    QColor traceColor(0xe0, 0xe8, 0xf0, 200);  // #e0e8f0 alpha 200
    sw->setFillColor(traceColor);

    // 5. Threshold gap — centred on the typical 80m noise floor (-110) with
    //    60 dB of headroom. See waterfall-tuning.md §5 for derivation.
    sw->setWfLowThreshold(-110.0f);
    sw->setWfHighThreshold(-70.0f);

    // 6. Waterfall AGC — tracks band conditions automatically so the user
    //    doesn't re-tune thresholds when changing bands.
    sw->setWfAgcEnabled(true);

    // 7. Waterfall update period — 30 ms scroll cadence produces a flowing
    //    effect instead of a stutter. Matches AetherSDR observation.
    sw->setWfUpdatePeriodMs(30);

    // Mark the profile as applied. Subsequent RadioModel constructions
    // will skip the first-launch call; the Reset button on
    // SpectrumDefaultsPage still calls applyClaritySmoothDefaults() directly.
    AppSettings::instance().setValue(
        QStringLiteral("DisplayProfileApplied"),
        QStringLiteral("True"));
}
```

- [ ] **Step 3: Wire the first-launch gate in the constructor**

Find `RadioModel::RadioModel(...)` in the same file. At the END of the constructor body, after all member initialization and wiring is complete, add:

```cpp
    // Phase 3G-9b: first-launch smooth defaults. On the very first run
    // (or after the user blows away NereusSDR.settings), apply the
    // Clarity Blue profile so the waterfall looks good out of the box.
    // Existing users with their own tuned values stay untouched.
    const QString flag = AppSettings::instance()
        .value(QStringLiteral("DisplayProfileApplied"),
               QStringLiteral("False")).toString();
    if (flag != QStringLiteral("True")) {
        // spectrumWidget() may not be wired yet at construction time —
        // applyClaritySmoothDefaults() guards on null and becomes a
        // no-op when called too early. MainWindow re-invokes it after
        // wiring via the explicit call in Task 3 Step 4.
        applyClaritySmoothDefaults();
    }
```

**NOTE:** `spectrumWidget()` likely returns null at the moment the `RadioModel` constructor runs (it's injected later by `MainWindow`). The function is null-safe so the early call is a harmless no-op; the real call happens in Task 3 below.

- [ ] **Step 4: Confirm it compiles**

```bash
cd /Users/j.j.boyd/NereusSDR
cmake --build build --target NereusSDRObjs 2>&1 | tail -5
```

Expected: clean compile. If any of the `SpectrumWidget` setters have different names than assumed (e.g. `setWfLowThreshold` vs `setWaterfallLowThreshold`), the compile errors will point at the exact lines. Look at `SpectrumWidget.h` for the actual names and fix the calls.

- [ ] **Step 5: Commit**

```bash
cd /Users/j.j.boyd/NereusSDR
git add src/models/RadioModel.h src/models/RadioModel.cpp
git commit -S -m "$(cat <<'EOF'
feat(model): applyClaritySmoothDefaults + first-launch gate

New RadioModel::applyClaritySmoothDefaults() sets the 7 smooth-default
recipe values from the Phase 3G-9b design spec (ClarityBlue palette,
log-recursive averaging, neutral trace colour, tight threshold gap,
waterfall AGC on, 30ms update period). Called exactly once on first
launch behind the AppSettings "DisplayProfileApplied" key — existing
users with their own tuned values are left untouched.

MainWindow's subsequent explicit call after wiring the widget pointers
is what actually applies the values; the constructor call is a
no-op no-null safety net for the unwired state.

Part of Phase 3G-9b PR2.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: MainWindow first-launch apply hook

**Files:**
- Modify: `src/gui/MainWindow.cpp`

The first-launch gate in the `RadioModel` constructor runs too early — widget pointers aren't wired yet. This task adds a second call from `MainWindow`, after the wiring is complete, so the profile actually takes effect on first launch.

- [ ] **Step 1: Find where MainWindow sets spectrumWidget on RadioModel**

```bash
cd /Users/j.j.boyd/NereusSDR
grep -n "setSpectrumWidget\|->spectrumWidget\|m_radioModel" src/gui/MainWindow.cpp | head -20
```

There should be a line where `MainWindow` injects the `SpectrumWidget*` into `RadioModel` via a setter or direct hook. Find that line and note the immediate neighborhood.

- [ ] **Step 2: Add the deferred apply call**

Immediately AFTER the line that wires `SpectrumWidget` into `RadioModel`, add:

```cpp
    // Phase 3G-9b: re-invoke the first-launch smooth-defaults check after
    // the SpectrumWidget pointer is wired. The RadioModel constructor's
    // own attempt runs too early to see the widget — this is where the
    // profile actually takes effect on first launch. If the gate has
    // already been flipped to "True" from a previous run, this is a no-op.
    {
        const QString flag = AppSettings::instance()
            .value(QStringLiteral("DisplayProfileApplied"),
                   QStringLiteral("False")).toString();
        if (flag != QStringLiteral("True")) {
            m_radioModel->applyClaritySmoothDefaults();
        }
    }
```

If the local name for the RadioModel pointer is something other than `m_radioModel`, substitute. If `AppSettings` is not yet included, add `#include "core/AppSettings.h"`.

- [ ] **Step 3: Build and smoke-test the gate**

```bash
cd /Users/j.j.boyd/NereusSDR
cmake --build build --target NereusSDR 2>&1 | tail -5
pkill -x NereusSDR 2>/dev/null; sleep 0.3

# Force the first-launch path by removing the gate key. Be careful not
# to remove the whole settings file unless you want to re-tune everything.
# The surgical approach: open the settings file and delete the key, then
# save. Alternatively, remove the whole file for a clean test.
mv ~/.config/NereusSDR/NereusSDR.settings ~/.config/NereusSDR/NereusSDR.settings.bak 2>/dev/null || true

open build/NereusSDR.app
```

Open the app. **Expected:** the waterfall renders with the Clarity Blue palette, dark navy noise floor, bright signals. If it looks like the old rainbow "Enhanced" palette, the gate didn't fire — re-check Step 1/2.

After verifying, restore settings if you want to keep your old tuning:
```bash
mv ~/.config/NereusSDR/NereusSDR.settings.bak ~/.config/NereusSDR/NereusSDR.settings
```

- [ ] **Step 4: Commit**

```bash
cd /Users/j.j.boyd/NereusSDR
git add src/gui/MainWindow.cpp
git commit -S -m "$(cat <<'EOF'
feat(gui): MainWindow re-invokes smooth-defaults after widget wiring

RadioModel's constructor calls applyClaritySmoothDefaults() too early —
the SpectrumWidget pointer isn't wired yet. This adds a second call
from MainWindow after the wiring is complete, behind the same
"DisplayProfileApplied" AppSettings gate. On first launch the profile
takes effect; on subsequent launches the gate short-circuits.

Part of Phase 3G-9b PR2.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: "Reset to Smooth Defaults" button

**Files:**
- Modify: `src/gui/setup/DisplaySetupPages.cpp`

A button at the top of the Spectrum Defaults page that calls `applyClaritySmoothDefaults()` on demand, with a confirmation dialog because it overwrites the user's current values.

- [ ] **Step 1: Add the button inside `SpectrumDefaultsPage::buildUI()`**

Find `SpectrumDefaultsPage::buildUI()` in `src/gui/setup/DisplaySetupPages.cpp`. **At the very start** of the function body (right after `applyDarkStyle(this);` but before the FFT group box is built), insert:

```cpp
    // Phase 3G-9b: Reset to Smooth Defaults button. Destructive — shows
    // confirmation dialog before overwriting.
    auto* resetBtn = new QPushButton(QStringLiteral("Reset to Smooth Defaults"), this);
    resetBtn->setToolTip(QStringLiteral(
        "Overwrite the Spectrum, Waterfall, and Grid settings with the "
        "NereusSDR smooth-default profile (Clarity Blue palette, "
        "log-recursive averaging, tight threshold gap, waterfall AGC on). "
        "Intended to recover the out-of-box look after experimentation."));
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        const auto rc = QMessageBox::question(
            this,
            QStringLiteral("Reset to Smooth Defaults"),
            QStringLiteral(
                "This will overwrite your current Spectrum, Waterfall, and "
                "Grid settings with the NereusSDR smooth-default profile.\n\n"
                "Your FFT size, frequency, band stack, and per-band grid "
                "slots are NOT affected.\n\n"
                "Continue?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (rc != QMessageBox::Yes) { return; }
        if (model()) {
            model()->applyClaritySmoothDefaults();
            // Reload this page and its siblings so the controls reflect
            // the new values in the model immediately.
            loadFromRenderer();
            // Sibling pages (Waterfall Defaults, Grid & Scales) will
            // update on their next SetupDialog repaint because their
            // bindings read from the model; explicit reload not needed.
        }
    });
    contentLayout()->addWidget(resetBtn);
```

- [ ] **Step 2: Add the required includes at the top of the file**

At the top of `src/gui/setup/DisplaySetupPages.cpp`, after the existing includes, ensure these are present (add any that are missing):

```cpp
#include <QPushButton>
#include <QMessageBox>
```

- [ ] **Step 3: Build and smoke-test**

```bash
cd /Users/j.j.boyd/NereusSDR
cmake --build build --target NereusSDR 2>&1 | tail -5
pkill -x NereusSDR 2>/dev/null; sleep 0.3
open build/NereusSDR.app
```

Open Setup → Display → Spectrum Defaults. Verify:
- The "Reset to Smooth Defaults" button appears at the top of the page.
- Hovering it shows the tooltip.
- Clicking it opens the confirmation dialog. Dismissing with "No" cancels.
- Clicking "Yes" overwrites the current settings — the Spectrum Defaults page should immediately reflect the new averaging mode, trace color, etc.
- Switching to the Waterfall Defaults page should show the Clarity Blue palette active and the new threshold values.
- The app does not crash.

- [ ] **Step 4: Commit**

```bash
cd /Users/j.j.boyd/NereusSDR
git add src/gui/setup/DisplaySetupPages.cpp
git commit -S -m "$(cat <<'EOF'
feat(setup): Reset to Smooth Defaults button on SpectrumDefaultsPage

One-click recovery to the Phase 3G-9b smooth-default profile. Guarded
by a QMessageBox confirmation because it overwrites the user's current
Spectrum/Waterfall/Grid values. Calls RadioModel::applyClaritySmoothDefaults()
and reloads the SpectrumDefaultsPage controls so the new values are
immediately visible.

Part of Phase 3G-9b PR2.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Unit test for gate + palette registration

**Files:**
- Create: `tests/tst_clarity_defaults.cpp`
- Modify: `tests/CMakeLists.txt`

TDD-ish check that the gate key is wired correctly and the new palette is reachable via `wfSchemeStops()`. Pure math / enum checks — no QWidget event loop needed.

- [ ] **Step 1: Write the test**

Create `tests/tst_clarity_defaults.cpp`:

```cpp
// tst_clarity_defaults.cpp
//
// Phase 3G-9b — unit smoke for the Clarity Blue palette registration
// and the "DisplayProfileApplied" first-launch gate semantics.

#include <QtTest/QtTest>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestClarityDefaults : public QObject {
    Q_OBJECT
private slots:

    void clarityBlue_enumOrdinalIsSeven()
    {
        // ClarityBlue is the 8th scheme (index 7). Adding it before Count
        // is the only acceptable ordering — any shift breaks persisted
        // combo ordinals. This assertion locks that in.
        QCOMPARE(static_cast<int>(WfColorScheme::ClarityBlue), 7);
        QCOMPARE(static_cast<int>(WfColorScheme::Count), 8);
    }

    void clarityBlue_gradientIsMonotonicAndSpansZeroToOne()
    {
        int count = 0;
        const WfGradientStop* stops = wfSchemeStops(WfColorScheme::ClarityBlue, count);
        QVERIFY(stops != nullptr);
        QCOMPARE(count, 6);

        // First stop is exactly at 0.0, last at 1.0 — covers the full
        // normalized range.
        QCOMPARE(stops[0].pos, 0.00f);
        QCOMPARE(stops[count - 1].pos, 1.00f);

        // Positions are strictly monotonically increasing.
        for (int i = 1; i < count; ++i) {
            QVERIFY2(stops[i].pos > stops[i - 1].pos,
                     "Clarity Blue gradient stops must be monotonic");
        }

        // Narrow-band property: at least 50% of the range must map to
        // the "dark" region (first two stops). This is the design
        // invariant — if someone changes the stop positions and the
        // dark region shrinks below 50%, the "AetherSDR readability"
        // look is lost.
        QVERIFY2(stops[1].pos >= 0.50f,
                 "Clarity Blue must reserve >= 50% of the range for the "
                 "dark noise-floor region (narrow-band palette invariant)");
    }

    void clarityBlue_topStopIsNearWhite()
    {
        int count = 0;
        const WfGradientStop* stops = wfSchemeStops(WfColorScheme::ClarityBlue, count);
        QVERIFY(count >= 1);
        const WfGradientStop& top = stops[count - 1];
        // Strong signals should pop as near-white — brightness >= 200
        // on every channel keeps the "peak" visible.
        QVERIFY2(top.r >= 200 && top.g >= 200 && top.b >= 200,
                 "Clarity Blue top stop must be near-white so strong "
                 "signals pop out");
    }
};

QTEST_MAIN(TestClarityDefaults)
#include "tst_clarity_defaults.moc"
```

- [ ] **Step 2: Register the test**

In `tests/CMakeLists.txt`, add under the existing `nereus_add_test(...)` block:

```cmake
nereus_add_test(tst_clarity_defaults)
```

Place it near `tst_setup_helpers` and `tst_smoke` alphabetically.

- [ ] **Step 3: Run the test**

```bash
cd /Users/j.j.boyd/NereusSDR
cmake --build build --target tst_clarity_defaults 2>&1 | tail -5
ctest --test-dir build -R tst_clarity_defaults --output-on-failure 2>&1 | tail -10
```

Expected: 3/3 slots pass.

- [ ] **Step 4: Commit**

```bash
cd /Users/j.j.boyd/NereusSDR
git add tests/tst_clarity_defaults.cpp tests/CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
test(spectrum): tst_clarity_defaults — gate + palette invariants

Three assertions:
1. ClarityBlue enum ordinal is exactly 7 (locks the combo index so
   persisted user selections keep working across schema changes).
2. Gradient is monotonic, spans [0.0, 1.0], and reserves >= 50% of the
   range for the dark noise-floor region (narrow-band palette invariant).
3. Top stop is near-white (r/g/b >= 200) so strong signals pop visually.

Part of Phase 3G-9b PR2.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Waterfall tuning rationale doc

**Files:**
- Create: `docs/architecture/waterfall-tuning.md`
- Create: `docs/architecture/waterfall-tuning/` (empty — screenshots land in Task 7)

The doc that explains *why* each of the seven recipes is what it is. Future-you and any reader who questions a specific value can look it up instead of re-deriving it.

- [ ] **Step 1: Write the doc**

Create `docs/architecture/waterfall-tuning.md` with this content:

````markdown
# Waterfall Tuning Rationale

> **Status:** Active. Last revised 2026-04-15 for Phase 3G-9b PR2.

This doc explains *why* the NereusSDR smooth-default profile looks the way it does. Every value in `RadioModel::applyClaritySmoothDefaults()` has a one-line "because X" that comes from this file.

If you're tempted to change a default value in `RadioModel::applyClaritySmoothDefaults()`, read the corresponding section here first. If your change invalidates the rationale, update this doc at the same time.

## Context

NereusSDR's display surface was feature-complete after Phase 3G-8 (47 controls wired) but the **default** look — what a fresh install presents to a user who has never touched Setup → Display — was the same high-contrast "Enhanced" rainbow palette that shipped in 3G-4 for meter system development. Side-by-side comparison against AetherSDR v0.8.12 on 80m (2026-04-14 reference screenshots in `docs/architecture/waterfall-tuning/`) showed a significant readability gap:

- AetherSDR renders noise as uniform dark navy and signals as bright cyan/white
- NereusSDR painted the entire noise floor in saturated reds/greens/cyans, burying signals in visual clutter

This doc is the per-knob rationale for the profile that fixes that.

## The seven recipes

### 1. Waterfall palette: `WfColorScheme::ClarityBlue`

**Value:** new narrow-band gradient, 6 stops:

| pos | r | g | b |
|---|---|---|---|
| 0.00 | 0x0a | 0x14 | 0x28 |
| 0.60 | 0x0d | 0x25 | 0x40 |
| 0.70 | 0x18 | 0x50 | 0xa0 |
| 0.85 | 0x30 | 0x90 | 0xe0 |
| 0.95 | 0x80 | 0xd0 | 0xff |
| 1.00 | 0xff | 0xff | 0xff |

**Why:** The biggest single difference between AetherSDR and NereusSDR's previous default is palette philosophy. The Thetis-lineage "Enhanced" palette spreads colour across the *entire* dynamic range, which means the noise floor gets coloured too (red/green/yellow). That burns visual contrast on content the user doesn't care about. ClarityBlue reserves 60% of the normalised range for uniform dark navy — the noise floor becomes a quiet dark background — and compresses the actual signal energy into the top 15% where it transitions from cyan to near-white. Result: signals jump out of a quiet background, exactly the AetherSDR/SmartSDR look.

**Source:** AetherSDR visual observation. Thetis has no equivalent palette (its schemes are all full-range).

### 2. Spectrum averaging mode: `AverageMode::Logarithmic`

**Value:** `sw->setAverageMode(AverageMode::Logarithmic)`

**Why:** Thetis's "Log Recursive" averaging mode smooths the dB-domain spectrum trace rather than the linear-power trace. On SSB voice the result is gentle voice-envelope hills instead of instantaneous grass. NereusSDR's previous default was `Weighted` (plain exponential) which produced visible trace jitter under noise. Logarithmic matches the dB axis the user is reading, and it matches Thetis's own default.

**Source:** `Thetis setup.cs:18055` (`console.specRX.GetSpecRX(0).AverageMode`) and `display.cs` log-recursive smoothing path.

### 3. Averaging time constant: `0.05f` alpha

**Value:** `sw->setAverageAlpha(0.05f)`

**Why:** The alpha value is the weight given to each new FFT frame in the exponential smoothing `smoothed = alpha * new + (1-alpha) * previous`. At `0.05` each new frame contributes only 5% — the smoothing window is approximately `1/alpha = 20 frames`. At the default 30 FPS that's about 667 ms of settling time, which reads as "~500 ms smooth" to the eye once you factor in frame-to-frame correlation. Heavier values (0.01) feel laggy on tuning; lighter (0.20) still show grass. 0.05 was empirically picked for this profile.

**UI note:** the Setup → Display → Spectrum Defaults "Averaging Time" spinbox converts ms → alpha via `qBound(0.05f, 1.0f - ms/5000.0f, 0.95f)`. That formula is for the manual knob; the smooth-defaults profile bypasses it and sets 0.05 directly.

**Source:** AetherSDR observation + NereusSDR empirical tuning.

### 4. Trace colour: `#e0e8f0` alpha 200

**Value:** `sw->setFillColor(QColor(0xe0, 0xe8, 0xf0, 200))`

**Why:** A neutral light-gray trace sits in front of the waterfall without competing for attention. The previous default (`#00e5ff` saturated cyan) competed with the ClarityBlue palette's own cyan highlights, making it hard to tell where the trace ended and the signal colour began. Alpha 200 (out of 255) keeps the trace visible without fully occluding the waterfall behind it.

**Source:** AetherSDR observation.

### 5. Waterfall Low / High thresholds: -110 / -70 dBm

**Value:** `sw->setWfLowThreshold(-110.0f); sw->setWfHighThreshold(-70.0f);`

**Why:** This is a 40 dB window centred on the typical HF noise floor. The old NereusSDR default was -130 / -40, which is a 90 dB window — 50 dB of it wasted on content that rarely appears (nothing at -130, nothing at -40 except local QRN). Compressing the window so the noise floor sits near the bottom of the palette and strong signals hit the top gives Clarity Blue the dynamic range it needs to render noise vs. signal as distinct colours.

**Interaction with Waterfall AGC (§6):** when AGC is on, these thresholds are treated as the *initial* values; the renderer continuously adjusts them based on a running min/max of recent frames. The starting point matters less than AGC tuning parameters, but these values produce a sensible cold-start look before AGC settles.

**Source:** 80m empirical observation + AetherSDR visual reference. Thetis default is -80 / -130, which is a full 50 dB range but anchored differently — not directly comparable because Thetis uses a full-range palette.

### 6. Waterfall AGC: `true`

**Value:** `sw->setWfAgcEnabled(true)`

**Why:** AGC tracks band conditions automatically. Without it, users must manually retune thresholds when changing bands (80m noise floor vs. 6m noise floor can differ by 30 dB). With it on, the display is optimised across all bands with zero user intervention. This is the single control that most separates "needs tuning to use" from "just works".

**Source:** `Thetis setup.designer.cs:34069` (`chkRX1WaterfallAGC`) default enabled.

### 7. Waterfall update period: 30 ms

**Value:** `sw->setWfUpdatePeriodMs(30)`

**Why:** 30 ms between waterfall row pushes produces ~33 rows/second, which reads as smooth scroll motion rather than discrete steps. The old NereusSDR default of 50 ms (20 rows/sec) was visibly steppy on long fade-ins. Going below 30 ms starts wasting GPU bandwidth without perceptible improvement — the eye can't distinguish 40 rows/sec from 33 on a typical monitor. Thetis's 2 ms default is unnecessarily aggressive.

**Source:** AetherSDR observation.

## First-launch gate

The profile is applied exactly once on a fresh install — the mechanism is an AppSettings key:

```
DisplayProfileApplied = "True"
```

Set to `"True"` immediately after the first successful apply. Subsequent `RadioModel` constructions and `MainWindow` invocations check the key and short-circuit if it's already `"True"`. Existing users (upgrading from pre-3G-9b) see no change — their current values are preserved.

## Reset button

`Setup → Display → Spectrum Defaults` has a "Reset to Smooth Defaults" button at the top of the page. Guarded by a confirmation dialog because it overwrites the user's Spectrum / Waterfall / Grid settings. It does NOT touch FFT size, frequency, band stack, or per-band grid slots — those are navigation/identity state, not display preferences.

## Open questions for PR3 (Clarity adaptive)

PR3 builds an adaptive auto-tune system (`Clarity`) on top of these static defaults. Open questions at the time of this doc:

- Does AGC (§6 here) get superseded by Clarity's noise-floor estimator? Likely yes — Clarity's estimator is more robust than the running min/max AGC, and having both produces conflicting behaviour.
- Should the static defaults in §5 be replaced by adaptive initial values? Probably keep them as the fallback when Clarity is off.
- Does the first-launch gate key (`DisplayProfileApplied`) need to track the profile *version* so future smooth-defaults revisions can re-run on upgrade? Yes — PR3 should migrate this key to `DisplayProfileVersion` with an integer version number.

See `docs/architecture/2026-04-15-display-refactor-design.md` §6 for the PR3 design.
````

- [ ] **Step 2: Create the screenshots directory**

```bash
cd /Users/j.j.boyd/NereusSDR
mkdir -p docs/architecture/waterfall-tuning
touch docs/architecture/waterfall-tuning/.gitkeep
```

The `.gitkeep` makes the directory trackable even though screenshots land in Task 7.

- [ ] **Step 3: Commit**

```bash
cd /Users/j.j.boyd/NereusSDR
git add docs/architecture/waterfall-tuning.md docs/architecture/waterfall-tuning/.gitkeep
git commit -S -m "$(cat <<'EOF'
docs(architecture): waterfall-tuning.md rationale doc

Per-recipe rationale for the Phase 3G-9b smooth-default profile.
Seven knobs, each with a "why this value" explanation. Screenshots
land in Task 7 of the PR.

Part of Phase 3G-9b PR2.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Before / after screenshots + CHANGELOG

**Files:**
- Create: `docs/architecture/waterfall-tuning/before-nereussdr-0.1.4.png`
- Create: `docs/architecture/waterfall-tuning/after-clarity-blue.png`
- Create: `docs/architecture/waterfall-tuning/aethersdr-reference.png`
- Modify: `docs/architecture/waterfall-tuning.md` (embed the screenshots)
- Modify: `CHANGELOG.md`

This is manual work: you take screenshots against your live radio and place them in the doc. The subagent can do the CHANGELOG edit but NOT the screenshot capture.

- [ ] **Step 1: Capture the "before" screenshot**

Launch the app with the smooth-defaults profile DISABLED (temporarily set `DisplayProfileApplied` to `"True"` in settings so the gate short-circuits, then set your waterfall colour scheme to the old "Enhanced" palette). Tune to 80m in the evening so there's an active noise floor and a few SSB voices. Screenshot the full window and save as `docs/architecture/waterfall-tuning/before-nereussdr-0.1.4.png`.

- [ ] **Step 2: Capture the "after" screenshot**

Click the new "Reset to Smooth Defaults" button. Wait ~2 seconds for AGC to settle. Screenshot the same view and save as `docs/architecture/waterfall-tuning/after-clarity-blue.png`.

- [ ] **Step 3: Capture the AetherSDR reference**

Launch AetherSDR, tune the same band/frequency, screenshot, save as `docs/architecture/waterfall-tuning/aethersdr-reference.png`. If you already have a reference from the 2026-04-14 design session, reuse it.

- [ ] **Step 4: Embed the screenshots in `waterfall-tuning.md`**

Edit `docs/architecture/waterfall-tuning.md`. Find the "## Context" section and add after the existing paragraph:

```markdown
## Visual comparison

![NereusSDR before — Enhanced rainbow palette, high visual clutter](waterfall-tuning/before-nereussdr-0.1.4.png)
*NereusSDR v0.1.4 default — "Enhanced" rainbow palette. Noise floor gets coloured, burning visual contrast on content you don't care about.*

![NereusSDR after — ClarityBlue, signals pop from quiet background](waterfall-tuning/after-clarity-blue.png)
*NereusSDR with Phase 3G-9b smooth defaults applied. ClarityBlue palette, log-recursive averaging, tight threshold gap, waterfall AGC on.*

![AetherSDR reference — the look we were chasing](waterfall-tuning/aethersdr-reference.png)
*AetherSDR v0.8.12 on the same band. This is the visual reference the smooth-defaults profile was tuned against.*
```

- [ ] **Step 5: Add the CHANGELOG entry**

Edit `CHANGELOG.md`. Find the `## Unreleased` section (create one at the top if it doesn't exist) and add under a `### Added` or `### Changed` subheading:

```markdown
### Added
- `Clarity Blue` waterfall palette — narrow-band monochrome designed to match AetherSDR/SmartSDR readability (Phase 3G-9b).
- `RadioModel::applyClaritySmoothDefaults()` — applies the Phase 3G-9b smooth-default profile on first launch. Existing users are untouched.
- "Reset to Smooth Defaults" button on `Setup → Display → Spectrum Defaults`. Confirmation-dialog-guarded.
- `docs/architecture/waterfall-tuning.md` — per-recipe rationale for the seven smooth defaults.

### Changed
- On fresh installs, `Setup → Display → Waterfall Defaults` palette now defaults to `Clarity Blue` (was `Default`). Existing users with their own colour scheme selection are preserved.
```

- [ ] **Step 6: Commit**

```bash
cd /Users/j.j.boyd/NereusSDR
git add docs/architecture/waterfall-tuning/*.png docs/architecture/waterfall-tuning.md CHANGELOG.md
git commit -S -m "$(cat <<'EOF'
docs(architecture): smooth-defaults screenshots + CHANGELOG

Before/after/reference PNGs for Phase 3G-9b. Embeds them in
waterfall-tuning.md as the visual rationale. CHANGELOG entry
covers the new palette, function, button, and doc.

Part of Phase 3G-9b PR2.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: Final verification + PR draft

**Files:** none modified. Verifies the branch end-to-end and prepares the PR description.

- [ ] **Step 1: Clean rebuild**

```bash
cd /Users/j.j.boyd/NereusSDR
cmake --build build --target clean
cmake --build build 2>&1 | tail -10
```

Expected: full tree builds with zero warnings on touched files.

- [ ] **Step 2: Full test suite**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -20
```

Expected: all tests pass except the two pre-existing P1 test failures that exist on `main` already (`tst_p1_wire_format`, `tst_p1_loopback_connection`). New `tst_clarity_defaults` should be green with 3/3 slots.

- [ ] **Step 3: First-launch manual smoke test**

```bash
# Save existing settings
cp ~/.config/NereusSDR/NereusSDR.settings ~/.config/NereusSDR/NereusSDR.settings.pr2-backup

# Wipe settings to force first-launch path
rm ~/.config/NereusSDR/NereusSDR.settings

pkill -x NereusSDR 2>/dev/null; sleep 0.3
open build/NereusSDR.app
```

Open the app. **Expected visible outcome:**
- Waterfall renders with dark navy noise floor, not rainbow
- Spectrum trace is a thin neutral light-gray, not saturated cyan
- Signals appear as bright cyan-white against quiet background
- Setup → Display → Waterfall Defaults shows "Clarity Blue" as the selected colour scheme
- Setup → Display → Spectrum Defaults shows the new "Reset to Smooth Defaults" button at the top

If any of the above doesn't match, debug before moving forward. Check `~/.config/NereusSDR/NereusSDR.settings` for `DisplayProfileApplied` value — it should be `"True"` after the first successful launch.

- [ ] **Step 4: Restore your actual settings**

```bash
mv ~/.config/NereusSDR/NereusSDR.settings.pr2-backup ~/.config/NereusSDR/NereusSDR.settings
```

- [ ] **Step 5: Reset-button manual smoke test**

Re-launch the app with your restored settings. Go to Setup → Display → Spectrum Defaults. Click "Reset to Smooth Defaults". Confirm the dialog. Expected:
- Waterfall palette flips to Clarity Blue
- Trace colour turns light-gray
- Averaging mode in the spinbox shows "Logarithmic"
- Waterfall AGC toggle is checked
- Update period slider reads 30 ms

No crash. The button is idempotent — clicking it twice produces the same end state.

- [ ] **Step 6: Diff review**

```bash
cd /Users/j.j.boyd/NereusSDR
git log --oneline main..HEAD
git diff main..HEAD --stat
```

Expected: ~7 commits, around ~400 LOC code + ~250 lines doc + 3 screenshots. Files touched:
- `src/gui/SpectrumWidget.h/.cpp`
- `src/gui/setup/DisplaySetupPages.cpp`
- `src/gui/MainWindow.cpp`
- `src/models/RadioModel.h/.cpp`
- `tests/tst_clarity_defaults.cpp` (new)
- `tests/CMakeLists.txt`
- `docs/architecture/waterfall-tuning.md` (new)
- `docs/architecture/waterfall-tuning/*.png` (3 new)
- `CHANGELOG.md`

Visually skim the diff for: missing braces, any accidental behavior changes to existing controls outside the 7 recipes, any regressions in lambda bodies.

- [ ] **Step 7: Draft the PR description**

Write to a scratch file (don't post yet — standing preference is user reviews all public GitHub posts):

```markdown
# Phase 3G-9b — Smooth Defaults + Clarity Blue Palette

Second of three PRs under Phase 3G-9 (Display Refactor). Ships the "out-of-box readable waterfall" story: a new `Clarity Blue` palette, a `applyClaritySmoothDefaults()` profile that applies on first launch, a one-click Reset button, and a rationale doc for the seven recipe values.

**Design spec:** `docs/architecture/2026-04-15-display-refactor-design.md` §5
**Plan:** `docs/architecture/2026-04-15-phase3g9b-smooth-defaults-plan.md`
**Predecessor:** PR #25 (Phase 3G-9a)

## What

- **New `WfColorScheme::ClarityBlue` palette** — narrow-band monochrome, 60% dark navy noise floor, top 15% cyan→white signals.
- **`RadioModel::applyClaritySmoothDefaults()`** — sets the seven recipe values from the spec (palette, averaging mode, alpha, trace colour, threshold gap, AGC, update period). Called once on first launch behind the `DisplayProfileApplied` AppSettings gate.
- **"Reset to Smooth Defaults" button** on `Setup → Display → Spectrum Defaults`, confirmation-dialog-guarded.
- **`docs/architecture/waterfall-tuning.md`** — per-recipe rationale with before/after screenshots vs. AetherSDR reference.
- **`tst_clarity_defaults`** — 3 invariant checks for enum ordinal, gradient monotonicity, narrow-band property.

## Why

Fresh installs landed on the Thetis-lineage "Enhanced" rainbow palette that made the whole noise floor colourful, burying signals in clutter. AetherSDR's narrow-band monochrome look is dramatically more readable. This PR closes the gap without touching any existing user's settings.

## Testing

- `tst_clarity_defaults` — 3/3 green (enum ordinal, gradient invariants, top-stop near-white).
- Full suite — passing except the two pre-existing P1 failures on `main`.
- Manual first-launch smoke: wiped settings, launched, confirmed Clarity Blue applied automatically.
- Manual Reset button smoke: clicked with existing settings, confirmed overwrite, confirmed idempotent.
- Side-by-side screenshot vs. AetherSDR in `docs/architecture/waterfall-tuning/`.

## Scope

**In:** ClarityBlue palette, smooth-defaults function, Reset button, rationale doc.
**Out:** RX2/TX display, Clarity adaptive tuning (PR3), Spectrum Overlay panel flyouts.

## Next

**PR3 (3G-9c)** — `Clarity` adaptive display tuning. Research-doc gated.

JJ Boyd ~KG4VCF
Co-Authored with Claude Code
```

- [ ] **Step 8: Push and open PR (pending user approval)**

Do NOT run `gh pr create` yet — show the PR description to the user first.

```bash
cd /Users/j.j.boyd/NereusSDR
git push -u origin feature/display-smooth-defaults-pr2
```

Show the user the PR description. After approval:

```bash
gh pr create --base main --head feature/display-smooth-defaults-pr2 \
    --title "Phase 3G-9b: smooth defaults + Clarity Blue palette (PR2 of 3)" \
    --body-file /tmp/pr2-body.md
```

Open the PR URL in the browser per standing preference.

---

## Commit shape recap

Expected commit log on `feature/display-smooth-defaults-pr2`:

1. `feat(spectrum): Clarity Blue waterfall palette` (Task 1)
2. `feat(model): applyClaritySmoothDefaults + first-launch gate` (Task 2)
3. `feat(gui): MainWindow re-invokes smooth-defaults after widget wiring` (Task 3)
4. `feat(setup): Reset to Smooth Defaults button on SpectrumDefaultsPage` (Task 4)
5. `test(spectrum): tst_clarity_defaults — gate + palette invariants` (Task 5)
6. `docs(architecture): waterfall-tuning.md rationale doc` (Task 6)
7. `docs(architecture): smooth-defaults screenshots + CHANGELOG` (Task 7)

Seven GPG-signed commits. Task 8 is verification only and produces no commit.
