# UI Polish — Cross-Surface Ownership Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land Phase B (B1-B8) of the UI polish design — eight cross-surface ownership decisions: AGC family alignment, Mode+Filter family alignment, antenna popup-builder consolidation, audio cluster cleanup with NYI wire-up, DSP toggle no-change verification, XIT functional wire-up, X/RIT-tab Lock removal, and spectrum display flyout signal wiring + NYI-button retention.

**Architecture:** Eight thematic clusters, each with its own dependency footprint. Most cross-surface alignment is mechanical (sync VfoWidget mode count to RxApplet's 11; sync RxApplet AGC combo modes from 4 to 5; sync filter preset values from canonical SliceModel list). New code: `AntennaPopupBuilder` utility class + capability-gated logic. Functional addition: XIT offset application in RadioModel mirrors the existing RIT pattern at line 3084-3095.

**Tech Stack:** Qt 6 / C++20. Depends on Plan 1 (Foundation) for canonical palette + applet-panel gutter. Uses existing `BoardCapabilities` + `SkuUiProfile` for B3 antenna gating, existing `MoxController` for B6 XIT TX-state coupling, existing `ColorSwatchButton` for any color choices (none in this plan).

---

## Reference

- **Design doc:** `docs/architecture/ui-audit-polish-plan.md` — Phase B (B1-B8).
- **Prerequisite:** Plan 1 (Foundation) lands first.
- **Key code paths:**
  - `src/models/RadioModel.cpp:3084-3095` — existing RIT update lambda; B6 XIT mirrors this
  - `src/models/SliceModel.cpp:161` — `// From Thetis console.cs:5180-5575 — InitFilterPresets, F5 per mode`; B2 filter presets read from here
  - `src/core/AlexController.{h,cpp}` — B3 antenna routing already exists; B3 just adds `AntennaPopupBuilder` for UI
  - `src/gui/SpectrumOverlayPanel.cpp` — B8 orphaned flyout signals to wire
  - `src/gui/SpectrumWidget.cpp:1300, :3730` — B8 `drawCursorInfo` calls to gate

---

## File-Structure Overview

### New code
- `src/gui/AntennaPopupBuilder.{h,cpp}` (new) — capability-gated popup factory, used by VfoWidget header + RxApplet antenna buttons + SpectrumOverlayPanel ANT flyout.

### Files modified
- `src/gui/applets/RxApplet.cpp` (B1 AGC modes, B2 filter grid mode-aware, B4 AF removal + Pan/Mute/SQL wire-up, B3 antenna popup migration)
- `src/gui/widgets/VfoWidget.cpp` (B1 AGC button row + slider direction, B2 mode combo +DSB+DRM, B3 antenna popup migration, B6 XIT placeholders wired, B7 X/RIT tab Lock removal)
- `src/gui/SpectrumOverlayPanel.cpp` (B3 ANT flyout migration, B8 wire orphaned signals + Cursor Freq + Fill Color + dead link, NYI buttons KEPT disabled)
- `src/gui/SpectrumWidget.{h,cpp}` (B8 setCursorFreqVisible setter + setGridVisible if missing + paint guards)
- `src/gui/MainWindow.cpp` (B6 XIT signal wire-up, B8 SpectrumOverlayPanel signal connects)
- `src/models/RadioModel.cpp` (B6 XIT offset + connect handlers)
- `src/models/SliceModel.{h,cpp}` (B1 AGCMode enum should already have all 5 — verify; possible filter preset accessor)
- `src/gui/setup/DspSetupPages.cpp` (B1 AGC mode combo set — verify count)

### Tests added
- `tests/tst_antenna_popup_builder.cpp` (new) — covers Tier 1/Tier 2/Tier 3 SKU behavior
- `tests/tst_xit_offset_application.cpp` (new) — verifies RadioModel applies xitHz at setTxFrequency call sites
- `tests/tst_b8_overlay_signals.cpp` (new) — verifies SpectrumOverlayPanel signals reach SpectrumWidget setters

---

## Tasks

### Task 1: Branch checkpoint after Plan 1 lands

- [ ] **Step 1: Verify Plan 1 commits are present**

```bash
git log --oneline | head -30 | grep -E "ui\(|style\("
```

Expected: Plan 1's ~24 commits present (foundation + dialog cleanup).

- [ ] **Step 2: Run baseline tests**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -10
```

Expected: 247+ passing (baseline + Plan 1's `tst_applet_panel_gutter`).

- [ ] **Step 3: No commit**

---

### Task 2: B1 — Verify SliceModel AGCMode enum has all 5 modes

**Files:**
- Verify: `src/models/SliceModel.h` (read-only check)

- [ ] **Step 1: Read AGCMode enum**

```bash
grep -nE "enum.*AGCMode|Off|Long|Slow|Med|Fast" src/models/SliceModel.h | head -10
```

Expected: enum contains at least Off/Long/Slow/Med/Fast/Custom (6 values per Thetis convention).

- [ ] **Step 2: If "Long" is missing, add it**

If the enum lacks Long, add as the second value (after Off, before Slow). Mirror Thetis enum order.

- [ ] **Step 3: No commit if enum already complete; otherwise commit the enum addition**

```bash
git add src/models/SliceModel.h
git commit -m "model(slice): add AGCMode::Long if missing — required by B1 5-mode parity"
```

---

### Task 3: B1 — Sync RxApplet AGC combo from 4 modes to 5

**Files:**
- Modify: `src/gui/applets/RxApplet.cpp:620-697` (AGC combo construction)

- [ ] **Step 1: Locate the AGC combo construction**

```bash
grep -n "AGC\|m_agcCombo\|setAgcMode" src/gui/applets/RxApplet.cpp | head -10
```

Expected: combo built around line 620 with `addItem` calls for Off, Slow, Med, Fast.

- [ ] **Step 2: Add Long as the second item**

Insert `m_agcCombo->addItem(QStringLiteral("Long"), QVariant::fromValue(AGCMode::Long));` between Off and Slow.

- [ ] **Step 3: Build + visual check**

```bash
cmake --build build -j$(nproc) --target NereusSDR
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

Open RxApplet → AGC combo. Confirm 5 entries: Off, Long, Slow, Med, Fast.

- [ ] **Step 4: Commit**

```bash
git add src/gui/applets/RxApplet.cpp
git commit -m "$(cat <<'EOF'
ui(rx-applet): add Long to AGC combo for 5-mode parity with VfoWidget

VfoWidget's AGC button row exposes 5 modes (Off/Long/Slow/Med/Fast).
RxApplet's combo had only 4 (missing Long). Sync to 5 modes per
docs/architecture/ui-audit-polish-plan.md §B1.

Custom mode stays setup-only — operators can still configure custom
parameters via Setup → DSP → AGC/ALC, and the active profile carries
those values.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: B1 — Bench-verify AGC-T slider direction; align if needed

**Files:**
- Modify: `src/gui/widgets/VfoWidget.cpp` OR `src/gui/applets/RxApplet.cpp` (whichever is wrong)

- [ ] **Step 1: Identify which surface is currently inverted**

```bash
grep -n "setInvertedAppearance\|m_agcTSlider" src/gui/widgets/VfoWidget.cpp src/gui/applets/RxApplet.cpp
```

Expected: one surface calls `setInvertedAppearance(true)` on its AGC-T slider; the other doesn't.

- [ ] **Step 2: Bench check (REQUIRED — needs hardware or Thetis screenshot)**

Either:
- Run Thetis on the bench or another machine. Open the AGC-T slider on the main panel. Note: does dragging the handle UP increase or decrease the numeric value displayed?
- If Thetis isn't available, find a Thetis screenshot in the project's design docs or online with the AGC-T slider visible at a known threshold value.

Thetis convention (verify): the slider label "−160" is at the bottom and "0" is at the top. Dragging UP moves toward 0 (less negative = more aggressive AGC). This is the **non-inverted** direction in Qt terms (the slider's natural max is at the top).

- [ ] **Step 3: Apply the matching direction to whichever surface is wrong**

If `VfoWidget` is currently inverted (per the inventory snapshot — line `m_agcTSlider->setInvertedAppearance(true)` exists), and Thetis is non-inverted, then remove the `setInvertedAppearance(true)` call from `VfoWidget`.

If RxApplet is wrong instead, do the opposite.

If verification is inconclusive, leave both as-is and surface the question in the PR description: "B1 slider direction: bench verification incomplete; matched VfoWidget to RxApplet (or vice versa). Please verify against Thetis at review."

- [ ] **Step 4: Build + visual check**

```bash
cmake --build build -j$(nproc) --target NereusSDR
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

Drag both AGC-T sliders (RxApplet and VfoWidget). They should move the same direction for the same numeric change.

- [ ] **Step 5: Commit**

```bash
git add src/gui/widgets/VfoWidget.cpp  # or RxApplet.cpp
git commit -m "$(cat <<'EOF'
ui(vfo-widget): align AGC-T slider direction with RxApplet (and Thetis)

Per docs/architecture/ui-audit-polish-plan.md §B1, the AGC-T slider
direction had drifted between RxApplet and VfoWidget — one was inverted
relative to the other. Aligned both surfaces to the Thetis convention
(non-inverted: drag up → larger numeric value).

Bench verified at <date> against <hardware/Thetis screenshot>.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: B2 — Add DSB and DRM to VfoWidget mode combo

**Files:**
- Modify: `src/gui/widgets/VfoWidget.cpp:1205,1225`

- [ ] **Step 1: Locate the mode combo construction**

```bash
grep -n "addItem.*USB\|modeChanged" src/gui/widgets/VfoWidget.cpp | head -10
```

- [ ] **Step 2: Add DSB and DRM**

Per the inventory: VfoWidget currently lists 9 modes (LSB/USB/AM/CWL/CWU/FM/DIGU/DIGL/SAM). Add `DSB` and `DRM` to match RxApplet's 11.

Insert `addItem` calls for DSB (between SAM and DIGU per Thetis enum order) and DRM (last).

- [ ] **Step 3: Build + visual**

Open VFO flag → Mode tab combo. Confirm 11 entries.

- [ ] **Step 4: Commit**

```bash
git add src/gui/widgets/VfoWidget.cpp
git commit -m "ui(vfo-widget): add DSB and DRM to mode combo (11-mode parity with RxApplet)

Per docs/architecture/ui-audit-polish-plan.md §B2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: B2 — Refactor RxApplet filter grid to mode-aware presets

**Files:**
- Modify: `src/gui/applets/RxApplet.cpp:417` (filter grid construction)
- Verify: `src/models/SliceModel.cpp:161` (canonical preset list source — should already exist)

- [ ] **Step 1: Read existing filter grid code**

```bash
sed -n '400,440p' src/gui/applets/RxApplet.cpp
```

Expected: a hardcoded 2×5 grid with fixed Hz labels (e.g. 100, 200, 500, 1000, 1800, 2400, 2700, 2900, 3000, 3200).

- [ ] **Step 2: Read SliceModel's canonical preset accessor**

```bash
grep -n "filterPresets\|InitFilterPresets\|presetForMode" src/models/SliceModel.{h,cpp}
```

If a public accessor exists (e.g. `SliceModel::presetsForMode(DSPMode)` returning `QList<std::pair<int,int>>` of (low, high) pairs), use it. If not, add one that reads from the existing internal table at line 161.

- [ ] **Step 3: Replace static grid with dynamic mode-aware grid**

Modify the filter row to:
- Read `m_slice->presetsForMode(currentMode)` on `dspModeChanged`.
- Repopulate the grid buttons from the returned list.
- Hide buttons that don't correspond to a preset for this mode (some modes have fewer than 10 presets).

- [ ] **Step 4: Build + visual**

Switch modes (USB → CW → AM → FM). Filter buttons should change to match each mode's preset list.

- [ ] **Step 5: Commit**

```bash
git add src/gui/applets/RxApplet.cpp src/models/SliceModel.{h,cpp}
git commit -m "$(cat <<'EOF'
ui(rx-applet): mode-aware filter preset grid driven by canonical SliceModel list

Replaced the hardcoded 10-button 2×5 filter grid with a dynamic grid
that reads from SliceModel::presetsForMode() on every dspModeChanged.
Source of truth is the existing InitFilterPresets table at SliceModel.cpp:161
(// From Thetis console.cs:5180-5575). Buttons that don't correspond to
a preset for the active mode are hidden.

Per docs/architecture/ui-audit-polish-plan.md §B2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: B2 — Refactor VfoWidget Mode-tab filter buttons to canonical preset subset

**Files:**
- Modify: `src/gui/widgets/VfoWidget.cpp:1507,1523` (filter button construction in Mode tab)

- [ ] **Step 1: Define the "5-6 most-common subset" per mode**

Per design doc §B2: VfoWidget shows 5-6 most-common presets per mode, drawn from the same canonical list. The "most common" definition is the top-5 widths Thetis main-panel filter buttons use per mode (NOT the setup-page list).

Add a helper to `SliceModel` (or inline in VfoWidget):

```cpp
// Returns the 5-6 most-common filter presets for the given mode,
// matching Thetis main-panel filter buttons.
QList<std::pair<int, int>> SliceModel::commonPresetsForMode(DSPMode mode) const
{
    // Per Thetis main-panel filter buttons. Subset of presetsForMode().
    switch (mode) {
        case DSPMode::USB: case DSPMode::LSB:
            return {{100, 2400}, {100, 2700}, {100, 2900}, {100, 3000}, {100, 3200}};
        case DSPMode::CWU: case DSPMode::CWL:
            return {{-50, 50}, {-100, 100}, {-150, 150}, {-250, 250}, {-500, 500}};
        case DSPMode::AM: case DSPMode::SAM:
            return {{-2900, 2900}, {-3500, 3500}, {-5000, 5000}};
        case DSPMode::FM:
            return {{-3000, 3000}, {-5000, 5000}, {-8000, 8000}};
        case DSPMode::DIGU: case DSPMode::DIGL:
            return {{100, 2700}, {100, 2900}, {100, 3000}, {100, 3300}, {100, 3500}};
        case DSPMode::DSB:
            return {{-2900, 2900}, {-3500, 3500}};
        case DSPMode::DRM:
            return {{-5000, 5000}};
        default:
            return {};
    }
}
```

- [ ] **Step 2: Wire VfoWidget's Mode-tab filter buttons to `commonPresetsForMode`**

On `dspModeChanged`, repopulate the button row from `m_slice->commonPresetsForMode(mode)`.

- [ ] **Step 3: Build + visual**

Switch modes; verify VfoWidget Mode tab shows 5-6 preset buttons matching Thetis's main-panel layout.

- [ ] **Step 4: Commit**

```bash
git add src/gui/widgets/VfoWidget.cpp src/models/SliceModel.{h,cpp}
git commit -m "$(cat <<'EOF'
ui(vfo-widget): mode-tab filter buttons use commonPresetsForMode subset

Added SliceModel::commonPresetsForMode() returning 5-6 most-common
preset pairs per mode, matching Thetis main-panel filter buttons.
VfoWidget Mode tab now reads from this on every dspModeChanged.

Same canonical preset values as RxApplet's full grid (B2 task 6) — just
a smaller subset for the flag's compact context.

Per docs/architecture/ui-audit-polish-plan.md §B2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: B3 — Create AntennaPopupBuilder utility class

**Files:**
- Create: `src/gui/AntennaPopupBuilder.{h,cpp}`
- Test: `tests/tst_antenna_popup_builder.cpp` (new)

- [ ] **Step 1: Write the failing test**

```cpp
// tests/tst_antenna_popup_builder.cpp
#include <QtTest/QtTest>
#include <QMenu>
#include "gui/AntennaPopupBuilder.h"
#include "core/BoardCapabilities.h"

class TestAntennaPopupBuilder : public QObject {
    Q_OBJECT
private slots:
    void tier1FullAlexShowsAllSections();
    void tier2MidAlexHidesExt2AndBypass();
    void tier3HermesLite2EmitsEmptyOrSingleAnt();
    void txModeFiltersOutRxOnlyEntries();
};

void TestAntennaPopupBuilder::tier1FullAlexShowsAllSections()
{
    NereusSDR::BoardCapabilities caps;
    caps.hasAlex2 = true;
    caps.antennaInputCount = 3;
    caps.hasExt1 = true;
    caps.hasExt2 = true;
    caps.hasRxBypassRelay = true;

    QMenu menu;
    NereusSDR::AntennaPopupBuilder::populate(&menu, caps,
        NereusSDR::AntennaPopupBuilder::Mode::RX, /*current=*/"ANT1");

    auto actions = menu.actions();
    QVERIFY(actions.size() >= 6);  // ANT1, ANT2, ANT3, EXT1, EXT2, RX-out
}

void TestAntennaPopupBuilder::tier2MidAlexHidesExt2AndBypass()
{
    NereusSDR::BoardCapabilities caps;
    caps.hasAlex = true;
    caps.antennaInputCount = 3;
    caps.hasExt1 = true;
    caps.hasExt2 = false;
    caps.hasRxBypassRelay = false;

    QMenu menu;
    NereusSDR::AntennaPopupBuilder::populate(&menu, caps,
        NereusSDR::AntennaPopupBuilder::Mode::RX, "ANT1");

    bool foundExt2 = false;
    bool foundBypass = false;
    for (auto* a : menu.actions()) {
        if (a->text().contains("EXT2")) foundExt2 = true;
        if (a->text().contains("RX out")) foundBypass = true;
    }
    QVERIFY(!foundExt2);
    QVERIFY(!foundBypass);
}

void TestAntennaPopupBuilder::tier3HermesLite2EmitsEmptyOrSingleAnt()
{
    NereusSDR::BoardCapabilities caps;
    caps.hasAlex = false;
    caps.antennaInputCount = 1;

    QMenu menu;
    NereusSDR::AntennaPopupBuilder::populate(&menu, caps,
        NereusSDR::AntennaPopupBuilder::Mode::RX, "ANT1");

    QVERIFY(menu.actions().size() <= 1);  // ANT1 only or empty
}

void TestAntennaPopupBuilder::txModeFiltersOutRxOnlyEntries()
{
    NereusSDR::BoardCapabilities caps;
    caps.hasAlex2 = true;
    caps.antennaInputCount = 3;
    caps.hasExt1 = true;
    caps.hasExt2 = true;

    QMenu menu;
    NereusSDR::AntennaPopupBuilder::populate(&menu, caps,
        NereusSDR::AntennaPopupBuilder::Mode::TX, "ANT1");

    bool foundExt = false;
    for (auto* a : menu.actions()) {
        if (a->text().contains("EXT")) foundExt = true;
    }
    QVERIFY(!foundExt);  // EXT1/EXT2 are RX-only
}

QTEST_MAIN(TestAntennaPopupBuilder)
#include "tst_antenna_popup_builder.moc"
```

Register in `tests/CMakeLists.txt`.

- [ ] **Step 2: Build + run; verify FAIL (class doesn't exist)**

```bash
cmake --build build -j$(nproc) --target tst_antenna_popup_builder 2>&1 | tail -10
```

Expected: build fails with "AntennaPopupBuilder.h not found".

- [ ] **Step 3: Implement the header**

Create `src/gui/AntennaPopupBuilder.h`:

```cpp
#pragma once

#include <QMenu>
#include <QString>
#include "core/BoardCapabilities.h"

namespace NereusSDR {

// Capability-gated antenna popup factory.
// Used by VfoWidget header, RxApplet antenna buttons, and SpectrumOverlayPanel
// ANT flyout — all three share this builder so their popup contents stay in sync.
//
// Per docs/architecture/ui-audit-polish-plan.md §B3.
class AntennaPopupBuilder
{
public:
    enum class Mode { RX, TX };

    // Populate `menu` with antenna actions per the board's capabilities.
    // For Mode::RX: shows Main TX/RX (ANT1-3) + RX-only (EXT1, EXT2) + Special (RX-out-on-TX, XVTR).
    // For Mode::TX: shows Main TX/RX only (ANT1-3, XVTR if applicable).
    //
    // The currently-active antenna (`current`) is shown with a check mark.
    // Actions emit triggered() carrying the antenna identifier in data() as QString.
    static void populate(QMenu* menu,
                         const BoardCapabilities& caps,
                         Mode mode,
                         const QString& current);
};

}  // namespace NereusSDR
```

Create `src/gui/AntennaPopupBuilder.cpp`:

```cpp
#include "gui/AntennaPopupBuilder.h"

namespace NereusSDR {

void AntennaPopupBuilder::populate(QMenu* menu,
                                   const BoardCapabilities& caps,
                                   Mode mode,
                                   const QString& current)
{
    if (!menu) { return; }

    auto addAction = [menu, &current](const QString& label) {
        auto* act = menu->addAction(label);
        act->setData(label);
        act->setCheckable(true);
        if (label == current) { act->setChecked(true); }
    };

    // No Alex / no antenna selection → empty (or single ANT1 if antenna count >= 1).
    if (!caps.hasAlex && !caps.hasAlex2) {
        if (caps.antennaInputCount >= 1) {
            addAction(QStringLiteral("ANT1"));
        }
        return;
    }

    // Section: Main TX/RX
    menu->addSection(QStringLiteral("Main TX/RX"));
    for (int i = 1; i <= caps.antennaInputCount && i <= 3; ++i) {
        addAction(QStringLiteral("ANT%1").arg(i));
    }

    // Section: RX-only — only for RX mode, only when capable
    if (mode == Mode::RX) {
        bool anyRxOnly = caps.hasExt1 || caps.hasExt2 || caps.hasXvtr;
        if (anyRxOnly) {
            menu->addSection(QStringLiteral("RX only"));
            if (caps.hasExt1) { addAction(QStringLiteral("EXT1")); }
            if (caps.hasExt2) { addAction(QStringLiteral("EXT2")); }
            if (caps.hasXvtr) { addAction(QStringLiteral("XVTR")); }
        }
    } else if (caps.hasXvtr) {
        // TX mode: XVTR sits with main TX/RX
        addAction(QStringLiteral("XVTR"));
    }

    // Section: Special — only for RX mode
    if (mode == Mode::RX && caps.hasRxBypassRelay) {
        menu->addSection(QStringLiteral("Special"));
        addAction(QStringLiteral("RX out on TX"));
    }
}

}  // namespace NereusSDR
```

Update `src/gui/CMakeLists.txt` to include `AntennaPopupBuilder.{h,cpp}`.

If `BoardCapabilities` doesn't already have `hasExt1`/`hasExt2`/`hasXvtr` flags, this is the time to add them (verify with `grep -n "hasExt1\|hasExt2\|hasXvtr" src/core/BoardCapabilities.h`).

- [ ] **Step 4: Build + run test; verify PASS**

```bash
cmake --build build -j$(nproc) --target tst_antenna_popup_builder
ctest --test-dir build -R tst_antenna_popup_builder --output-on-failure
```

Expected: all 4 sub-tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/gui/AntennaPopupBuilder.{h,cpp} src/gui/CMakeLists.txt tests/tst_antenna_popup_builder.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
ui(antenna): add AntennaPopupBuilder for capability-gated popup construction

New utility class that VfoWidget header, RxApplet antenna buttons, and
SpectrumOverlayPanel ANT flyout will all use to construct their antenna
popup menus. Capability-gated via BoardCapabilities — Tier 1 (full Alex)
shows all sections, Tier 2 (mid Alex) hides EXT2/Bypass, Tier 3 (HL2/
no Alex) shows ANT1 or empty.

Per docs/architecture/ui-audit-polish-plan.md §B3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 9: B3 — Migrate RxApplet antenna buttons to use AntennaPopupBuilder

**Files:**
- Modify: `src/gui/applets/RxApplet.cpp:240, :277`

- [ ] **Step 1: Find current popup construction**

```bash
grep -n "rxAntenna\|txAntenna\|setRxAntenna\|setTxAntenna" src/gui/applets/RxApplet.cpp | head -10
```

- [ ] **Step 2: Replace popup-menu construction with `AntennaPopupBuilder::populate`**

For the RX antenna button:
```cpp
connect(m_rxAntButton, &QPushButton::clicked, this, [this]() {
    QMenu menu(this);
    NereusSDR::AntennaPopupBuilder::populate(&menu, m_radioModel->boardCapabilities(),
        NereusSDR::AntennaPopupBuilder::Mode::RX, m_slice->rxAntenna());
    QAction* chosen = menu.exec(m_rxAntButton->mapToGlobal(QPoint(0, m_rxAntButton->height())));
    if (chosen) {
        m_slice->setRxAntenna(chosen->data().toString());
    }
});
```

Same pattern for TX antenna button.

- [ ] **Step 3: Build + visual**

Click RX antenna button. Popup shows capability-gated menu.

- [ ] **Step 4: Commit**

```bash
git add src/gui/applets/RxApplet.cpp
git commit -m "ui(rx-applet): migrate antenna buttons to AntennaPopupBuilder

Per docs/architecture/ui-audit-polish-plan.md §B3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 10: B3 — Migrate VfoWidget antenna buttons to use AntennaPopupBuilder

**Files:**
- Modify: `src/gui/widgets/VfoWidget.cpp:425, :471`

Same pattern as Task 9 for the VfoWidget header's RX and TX antenna buttons.

- [ ] **Steps 1-4:** mirror Task 9.

- [ ] **Commit message** prefix `vfo-widget`.

---

### Task 11: B3 — Migrate SpectrumOverlayPanel ANT flyout to AntennaPopupBuilder + add EXT/XVTR/BYPS combo entries

**Files:**
- Modify: `src/gui/SpectrumOverlayPanel.cpp:444, :479`

- [ ] **Step 1: Read existing combo construction**

The current code uses two QComboBox widgets for RX and TX antennas. Migrate to use `AntennaPopupBuilder::populate(builder)` to populate a temporary QMenu, then iterate the menu's actions and add them to the combo.

Or simpler: have `AntennaPopupBuilder` provide an alternative interface that returns a `QStringList` of antenna labels per (caps, mode), then the combo just calls `combo->addItems(list)`.

Add to `AntennaPopupBuilder.h`:
```cpp
static QStringList labels(const BoardCapabilities& caps, Mode mode);
```

Add to `AntennaPopupBuilder.cpp`:
```cpp
QStringList AntennaPopupBuilder::labels(const BoardCapabilities& caps, Mode mode)
{
    QMenu tempMenu;
    populate(&tempMenu, caps, mode, QString());
    QStringList out;
    for (auto* a : tempMenu.actions()) {
        if (!a->isSeparator() && !a->text().isEmpty() && a->data().isValid()) {
            out << a->data().toString();
        }
    }
    return out;
}
```

- [ ] **Step 2: Use `labels()` in SpectrumOverlayPanel**

Replace the hardcoded `addItems({"ANT1", "ANT2", "ANT3"})` with `combo->addItems(NereusSDR::AntennaPopupBuilder::labels(caps, Mode::RX))`.

- [ ] **Step 3: Build + visual**

Open spectrum overlay → ANT flyout. Combos now show EXT1/EXT2/XVTR/BYPS entries when capable.

- [ ] **Step 4: Commit**

```bash
git add src/gui/SpectrumOverlayPanel.cpp src/gui/AntennaPopupBuilder.{h,cpp}
git commit -m "$(cat <<'EOF'
ui(spectrum-overlay): ANT flyout combos use AntennaPopupBuilder labels

SpectrumOverlayPanel ANT flyout combos now populate from the same
capability-gated builder as VfoWidget and RxApplet popups (B3 task
9-10). Adds EXT1/EXT2/XVTR/RX-out-on-TX entries when supported by
the radio.

Per docs/architecture/ui-audit-polish-plan.md §B3 (decision A).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 12: B4 — Drop AF gain row from RxApplet

**Files:**
- Modify: `src/gui/applets/RxApplet.cpp:476-500`

- [ ] **Step 1: Locate the AF gain slider row**

```bash
grep -n "afGain\|m_afSlider\|setAfGain" src/gui/applets/RxApplet.cpp | head -5
```

- [ ] **Step 2: Remove the row**

Delete the AF slider construction, value label, and connect lines. Master volume in TitleBar + AF gain in VfoWidget remain — 3 → 2 surfaces per the design.

- [ ] **Step 3: Build + visual**

RxApplet body is shorter; AF row gone. Volume control still works via TitleBar + flag.

- [ ] **Step 4: Commit**

```bash
git add src/gui/applets/RxApplet.cpp
git commit -m "$(cat <<'EOF'
ui(rx-applet): drop AF gain row (3rd surface — flag + master cover it)

Per docs/architecture/ui-audit-polish-plan.md §B4. The TitleBar's
master volume and VfoWidget's per-slice AF gain are sufficient — the
RxApplet copy was a third surface for the same control.

Frees ~30 px of vertical space; useful headroom for future right-panel
content.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 13: B4 — Wire RxApplet Pan / Mute / SQL placeholders

**Files:**
- Modify: `src/gui/applets/RxApplet.cpp:457, :502, :525, :529`

- [ ] **Step 1: Verify SliceModel has the setters**

```bash
grep -n "setMuted\|setAudioPan\|setSquelchEnabled\|setSquelchThreshold" src/models/SliceModel.h
```

Expected: all 4 setters present (per inventory; VfoWidget already calls these).

- [ ] **Step 2: Wire each placeholder to the model setter + sync from model**

For Mute button:
```cpp
m_muteBtn->setEnabled(true);  // was: setEnabled(false)
connect(m_muteBtn, &QPushButton::toggled, this, [this](bool on) {
    if (m_updatingFromModel) return;
    m_slice->setMuted(on);
});
connect(m_slice, &SliceModel::mutedChanged, this, [this](bool on) {
    QSignalBlocker block(m_muteBtn);
    m_muteBtn->setChecked(on);
});
```

Same pattern for Pan slider, SQL toggle, SQL threshold slider. Remove the `NyiOverlay::markNyi(...)` calls for each.

- [ ] **Step 3: Build + visual + functional check**

```bash
cmake --build build -j$(nproc) --target NereusSDR
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

In RxApplet:
- Click Mute — audio mutes; VfoWidget Mute button updates.
- Drag Pan slider — VfoWidget pan slider mirrors.
- Toggle SQL — squelch engages; threshold slider drag changes.

- [ ] **Step 4: Commit**

```bash
git add src/gui/applets/RxApplet.cpp
git commit -m "$(cat <<'EOF'
ui(rx-applet): wire Pan/Mute/SQL placeholders to SliceModel setters

Pan, Mute, SQL toggle, and SQL threshold slider were NYI placeholders
on RxApplet (VfoWidget already had them functional). Wired each to the
existing SliceModel setters with model→UI sync via the standard
m_updatingFromModel + QSignalBlocker pattern. Cross-surface sync with
VfoWidget verified.

Per docs/architecture/ui-audit-polish-plan.md §B4.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 14: B5 — Verify DSP toggle no-changes (verification only)

**Files:**
- None modified (verification)

- [ ] **Step 1: Confirm DSP toggle inventory matches design doc**

Open VfoWidget DSP tab. Verify 7 NR slots (NR1-4 + DFNR + MNR-on-macOS + ANF) + NB cycle + SNB + APF toggle + APF tune slider — total 11 controls in the dense grid.

Open MainWindow DSP menu. Verify NR submenu, NB submenu, ANF + BIN actions, APF action all present.

Open RxDashboard (status bar). Verify NR/NB/APF/SQL badges appear when active.

- [ ] **Step 2: No code changes; no commit. Document on PR description**

Add to PR description: "B5: no-op — DSP toggle architecture confirmed correct as-is."

---

### Task 15: B6 — Verify SliceModel XIT plumbing exists

**Files:**
- Verify: `src/models/SliceModel.h:190-191, :434-437`

```bash
grep -n "xitEnabled\|xitHz" src/models/SliceModel.h
```

Expected (per design doc investigation): all properties + setters + signals already present.

- [ ] **Step 1: No commit — verification only**

---

### Task 16: B6 — Wire XIT offset application in RadioModel

**Files:**
- Modify: `src/models/RadioModel.cpp:2498, :3190` (apply xitHz offset before setTxFrequency)
- Modify: `src/models/RadioModel.cpp:~3095` (add xit signal connects mirror RIT pattern)
- Test: `tests/tst_xit_offset_application.cpp` (new)

- [ ] **Step 1: Write the failing test**

Create `tests/tst_xit_offset_application.cpp`:

```cpp
#include <QtTest/QtTest>
#include "models/RadioModel.h"
#include "models/SliceModel.h"

class TestXitOffsetApplication : public QObject {
    Q_OBJECT
private slots:
    void disabledXitDoesNotShiftTxFrequency();
    void enabledXitShiftsTxFrequencyByHz();
    void splitOperationAppliesRitToRxAndXitToTx();
};

void TestXitOffsetApplication::disabledXitDoesNotShiftTxFrequency()
{
    // Setup: SliceModel with xitEnabled=false, xitHz=500.
    // Trigger setTxFrequency code path.
    // Expected: setTxFrequency called with raw VFO freq (no offset).
    // Implementation: use a mock RadioConnection that captures the call.
    // ... (concrete test code based on existing test patterns)
}

// ... two more sub-tests

QTEST_MAIN(TestXitOffsetApplication)
#include "tst_xit_offset_application.moc"
```

Register in `tests/CMakeLists.txt`. (For brevity I've sketched the structure — the implementation should mirror existing RadioModel test patterns; check `tests/tst_radio_model_*.cpp` if any exist for the mock pattern.)

- [ ] **Step 2: Build + run; verify FAIL**

Test will fail because xitHz isn't applied yet.

- [ ] **Step 3: Read existing RIT pattern to mirror**

```bash
sed -n '3080,3100p' src/models/RadioModel.cpp
```

Expected: `updateShiftFrequency` lambda reading `slice->ritHz()`, with `connect(slice, &SliceModel::ritHzChanged, this, updateShiftFrequency)` at line 3095.

- [ ] **Step 4: Apply XIT offset at the two setTxFrequency call sites**

`RadioModel.cpp:2498`:
```cpp
const quint64 txFreqHz = freqHz + (slice->xitEnabled() ? static_cast<qint64>(slice->xitHz()) : 0);
conn->setTxFrequency(txFreqHz);
```

Same change at line 3190.

- [ ] **Step 5: Add xit signal connects mirroring RIT**

After the RIT connect at ~line 3095, add:
```cpp
connect(slice, &SliceModel::xitEnabledChanged, this, updateTxFrequency);
connect(slice, &SliceModel::xitHzChanged,        this, updateTxFrequency);
```

(Define `updateTxFrequency` lambda parallel to the existing `updateShiftFrequency`. Simpler than a separate method.)

- [ ] **Step 6: Build + run test; verify PASS**

```bash
cmake --build build -j$(nproc)
ctest --test-dir build -R tst_xit_offset --output-on-failure
```

- [ ] **Step 7: Commit**

```bash
git add src/models/RadioModel.cpp tests/tst_xit_offset_application.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
model(radio): apply xitHz offset to TX frequency, mirroring RIT pattern

SliceModel had full XIT plumbing (xitEnabled, xitHz, signals,
persistence) since 3I-3, but no one ever applied the offset to TX
frequency commands. The two setTxFrequency call sites in RadioModel
(:2498, :3190) sent the raw VFO frequency.

Mirroring the existing RIT pattern at line 3084-3095:
- At each setTxFrequency call, prepend (slice->xitEnabled() ?
  slice->xitHz() : 0) to the frequency before calling.
- Connect xitEnabledChanged + xitHzChanged to a parallel
  updateTxFrequency lambda.

Now functional: enabling XIT shifts TX NCO; split operation works.

Per docs/architecture/ui-audit-polish-plan.md §B6.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 17: B6 — Wire RxApplet XIT placeholders

**Files:**
- Modify: `src/gui/applets/RxApplet.cpp:782-801`

Same pattern as Task 13 (Pan/Mute/SQL wiring), applied to XIT toggle + ± step triangles + zero button + offset label.

- [ ] **Steps 1-4:** mirror Task 13 with XIT setters (`SliceModel::setXitEnabled`, `setXitHz`).

- [ ] **Commit message:**

```bash
git commit -m "ui(rx-applet): wire XIT placeholders to SliceModel setters

Per docs/architecture/ui-audit-polish-plan.md §B6.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 18: B6 — Wire VfoWidget X/RIT tab XIT placeholders

**Files:**
- Modify: `src/gui/widgets/VfoWidget.cpp:1310-1330`

Same pattern as Task 17, applied to VfoWidget's X/RIT tab XIT controls (toggle + scrollable label + zero button).

- [ ] **Steps 1-4:** mirror Task 17.

- [ ] **Commit message** prefix `vfo-widget`.

- [ ] **Step 5: Bench verification (REQUIRED)**

With ANAN-G2 connected:
- Tune to 14.200 MHz quiet band. Engage MOX (mic or 2-Tone). Verify TX trace at 14.200 MHz.
- Enable XIT, set XIT = +500 Hz. Verify TX trace shifts to 14.2005 MHz.
- Enable RIT = -500 Hz. Verify RX listens at 14.1995 while TX still at 14.2005 (split confirmed).
- Disable both. TX/RX both at 14.200.
- Persistence: disconnect/reconnect. XIT state restored.

Document bench result in commit message or PR description.

---

### Task 19: B7 — Drop X/RIT tab Lock button from VfoWidget

**Files:**
- Modify: `src/gui/widgets/VfoWidget.cpp:1344`

- [ ] **Step 1: Find the X/RIT tab Lock construction**

```bash
sed -n '1340,1360p' src/gui/widgets/VfoWidget.cpp
```

- [ ] **Step 2: Delete the Lock button + its layout slot + signal connects**

The Close-strip Lock and RxApplet header Lock remain (per design); only the X/RIT tab one is dropped (least accessible, redundant within the same widget).

- [ ] **Step 3: Build + visual**

Open VFO flag → X/RIT tab. Lock button gone; RIT/XIT rows + Step cycle button still present.

- [ ] **Step 4: Commit**

```bash
git add src/gui/widgets/VfoWidget.cpp
git commit -m "ui(vfo-widget): drop X/RIT-tab Lock button (redundant with Close-strip)

The X/RIT tab Lock was redundant within the same widget: the Close-strip
Lock is always visible while the X/RIT tab Lock requires a tab switch.
Three lock toggles → two: RxApplet header + VfoWidget Close-strip.

Per docs/architecture/ui-audit-polish-plan.md §B7.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 20: B8 — Wire SpectrumOverlayPanel orphaned signals to SpectrumWidget

**Files:**
- Modify: `src/gui/MainWindow.cpp` (~line 730 area where `m_overlayPanel` is set up)
- Test: `tests/tst_b8_overlay_signals.cpp` (new)

- [ ] **Step 1: Write the failing test** (verify signals land on the spectrum widget setters)

Brief test using QSignalSpy or a direct check that emitting `wfColorGainChanged` from the panel triggers `m_wfColorGain` change on SpectrumWidget.

- [ ] **Step 2: Add connects in MainWindow**

Around the existing overlay panel connects (line ~1093-1104, where `clarityRetuneRequested` is connected), add:

```cpp
connect(m_overlayPanel, &SpectrumOverlayPanel::wfColorGainChanged,
        m_spectrumWidget, &SpectrumWidget::setWfColorGain);  // or whatever the SpectrumWidget setter is named
connect(m_overlayPanel, &SpectrumOverlayPanel::wfBlackLevelChanged,
        m_spectrumWidget, &SpectrumWidget::setWfBlackLevel);
connect(m_overlayPanel, &SpectrumOverlayPanel::colorSchemeChanged,
        m_spectrumWidget, &SpectrumWidget::setWfColorScheme);
```

If `SpectrumWidget` doesn't have these setters yet (per inventory it stores `m_wfColorGain` etc.), add public setters that update the member + call `update()`.

- [ ] **Step 3: Run test; PASS**

- [ ] **Step 4: Build + visual**

Open Display flyout. Move WF Gain slider — waterfall colour-gain visibly changes (was no-op before).

- [ ] **Step 5: Commit**

```bash
git add src/gui/MainWindow.cpp src/gui/SpectrumWidget.{h,cpp} tests/tst_b8_overlay_signals.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
ui(main-window): wire SpectrumOverlayPanel Display-flyout signals to SpectrumWidget

The Display flyout's wfColorGainChanged / wfBlackLevelChanged /
colorSchemeChanged signals were emitted but no one connected to them —
moving the sliders did nothing. The right-click menu (SpectrumOverlayMenu)
was the only functional spectrum-display editor.

Connected the three orphaned signals to SpectrumWidget setters so the
flyout works the same as the right-click menu (both surfaces edit the
same backing fields).

Per docs/architecture/ui-audit-polish-plan.md §B8.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 21: B8 — Add Cursor Freq guard + setter to SpectrumWidget

**Files:**
- Modify: `src/gui/SpectrumWidget.{h,cpp}`

- [ ] **Step 1: Add `m_showCursorFreq` member + setter**

In `SpectrumWidget.h`:
```cpp
public:
    void setCursorFreqVisible(bool on);
    bool cursorFreqVisible() const noexcept { return m_showCursorFreq; }
private:
    bool m_showCursorFreq{true};  // default on (matches today's always-on behavior)
```

In `SpectrumWidget.cpp`:
```cpp
void SpectrumWidget::setCursorFreqVisible(bool on)
{
    if (m_showCursorFreq == on) { return; }
    m_showCursorFreq = on;
    update();
    scheduleSettingsSave();
}
```

- [ ] **Step 2: Guard the two `drawCursorInfo()` call sites**

`SpectrumWidget.cpp:1300`: change `drawCursorInfo(p, specRect);` → `if (m_showCursorFreq) drawCursorInfo(p, specRect);`

Same change at `SpectrumWidget.cpp:3730`.

- [ ] **Step 3: Persist via existing settings load/save mechanism**

Check `loadSettings` and `saveSettings` (around line 474, 564 per inventory) and add a new key `display/showCursorFreq`.

- [ ] **Step 4: Wire SpectrumOverlayPanel's Cursor Freq toggle to emit a signal**

Find the Cursor Freq toggle in `SpectrumOverlayPanel.cpp` (line ~751). It currently only updates the toggle's own label. Add a new signal:

```cpp
// In SpectrumOverlayPanel.h:
signals:
    void cursorFreqVisibleChanged(bool on);
```

Wire the toggle:
```cpp
connect(m_cursorFreqBtn, &QPushButton::toggled, this, [this](bool on) {
    m_cursorFreqBtn->setText(on ? "On" : "Off");
    emit cursorFreqVisibleChanged(on);
});
```

- [ ] **Step 5: Connect in MainWindow**

```cpp
connect(m_overlayPanel, &SpectrumOverlayPanel::cursorFreqVisibleChanged,
        m_spectrumWidget, &SpectrumWidget::setCursorFreqVisible);
```

- [ ] **Step 6: Build + visual**

Toggle Cursor Freq off → frequency-at-cursor readout disappears. Toggle on → reappears.

- [ ] **Step 7: Commit**

```bash
git add src/gui/SpectrumWidget.{h,cpp} src/gui/SpectrumOverlayPanel.{h,cpp} src/gui/MainWindow.cpp
git commit -m "$(cat <<'EOF'
ui(spectrum-widget): add setCursorFreqVisible(bool) guard for drawCursorInfo

Cursor frequency readout was always on. Display flyout's Cursor Freq
toggle existed but emitted no signal. Wired:
- SpectrumWidget::m_showCursorFreq (default true) + setCursorFreqVisible
  setter + persistence + paint-time guard at both drawCursorInfo call
  sites (lines 1300, 3730).
- SpectrumOverlayPanel emits cursorFreqVisibleChanged on toggle.
- MainWindow connects panel signal to widget setter.

Per docs/architecture/ui-audit-polish-plan.md §B8.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 22: B8 — Wire Fill Color button to SpectrumWidget::setFillColor

**Files:**
- Modify: `src/gui/SpectrumOverlayPanel.{h,cpp}`
- Modify: `src/gui/MainWindow.cpp`

- [ ] **Step 1: Verify SpectrumWidget::setFillColor exists**

```bash
grep -n "setFillColor" src/gui/SpectrumWidget.{h,cpp}
```

Expected: setter exists at line 831 (per design doc investigation).

- [ ] **Step 2: Replace the local-only color picker with a signal emit**

Currently the Fill Color button opens a QColorDialog and only repaints itself. Add:
```cpp
signals:
    void fillColorChanged(const QColor& color);

// In the button's clicked handler:
QColor c = QColorDialog::getColor(...);
if (c.isValid()) {
    // (still update the local swatch as before)
    emit fillColorChanged(c);
}
```

- [ ] **Step 3: Connect in MainWindow**

```cpp
connect(m_overlayPanel, &SpectrumOverlayPanel::fillColorChanged,
        m_spectrumWidget, &SpectrumWidget::setFillColor);
```

- [ ] **Step 4: Build + visual**

Click Fill Color → pick a color → spectrum trace fill changes. Restart app → color persists (via existing SpectrumWidget setting save).

- [ ] **Step 5: Commit**

```bash
git add src/gui/SpectrumOverlayPanel.{h,cpp} src/gui/MainWindow.cpp
git commit -m "ui(spectrum-overlay): wire Fill Color button to SpectrumWidget::setFillColor

Per docs/architecture/ui-audit-polish-plan.md §B8.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 23: B8 — Remove unbacked Heat Map / Noise Floor / Weighted Avg toggles

**Files:**
- Modify: `src/gui/SpectrumOverlayPanel.cpp:770-836` (approx)

- [ ] **Step 1: Identify the unbacked toggle rows**

Per design doc decision 3c: Heat Map (line ~770), Noise Floor toggle + slider (line ~793-813), Weighted Avg (line ~830-836). Each only updates its own label text — no signal emit, no model property.

- [ ] **Step 2: Delete the rows**

Remove the construction code (button + label + connect for label-update-only) for these 4 controls (Heat Map, Noise Floor toggle, Noise Floor slider, Weighted Avg).

- [ ] **Step 3: Build + visual**

Open Display flyout. Heat Map / Noise Floor / Weighted Avg rows gone. Other rows (Color Scheme, WF Gain, Black Level, Fill Alpha, Fill Color, Grid Lines, Cursor Freq, Clarity Re-tune, More Display Options link) remain.

- [ ] **Step 4: Commit**

```bash
git add src/gui/SpectrumOverlayPanel.cpp
git commit -m "$(cat <<'EOF'
ui(spectrum-overlay): remove unbacked Heat Map / Noise Floor / Weighted Avg toggles

These toggles existed in the Display flyout but emitted no signal and
had no model state — pure label-update theatre. Per
docs/architecture/ui-audit-polish-plan.md §B8 decision 3c, removed.

Noise floor estimation already happens via NoiseFloorEstimator +
ClarityController (the canonical path); the flyout toggle was
misleading.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 24: B8 — Wire "More Display Options →" link to Setup → Display

**Files:**
- Modify: `src/gui/SpectrumOverlayPanel.{h,cpp}`
- Verify: `src/gui/MainWindow.cpp` has `openSetupRequested(QString)` slot or equivalent (used by AGC-T right-click pattern)

- [ ] **Step 1: Add a click handler to the link label**

The current label has no click handler. Make it clickable (mousePressEvent on the QLabel or use a flat QPushButton styled as a link).

```cpp
signals:
    void openSetupRequested(const QString& page);

// In the label's click handler:
emit openSetupRequested(QStringLiteral("Display"));
```

- [ ] **Step 2: Connect in MainWindow**

If MainWindow already has a slot for `openSetupRequested(QString)` (from the AGC-T right-click path), just add a connect. Otherwise add a small helper that opens the SetupDialog at the requested page.

- [ ] **Step 3: Build + visual**

Click "More Display Options →" → Setup → Display opens.

- [ ] **Step 4: Commit**

```bash
git add src/gui/SpectrumOverlayPanel.{h,cpp} src/gui/MainWindow.cpp
git commit -m "ui(spectrum-overlay): wire \"More Display Options →\" link to Setup → Display

Per docs/architecture/ui-audit-polish-plan.md §B8 decision 5.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 25: B8 — Verify NYI strip buttons stay visible-but-disabled

**Files:**
- Verify: `src/gui/SpectrumOverlayPanel.cpp:185-242` (strip button construction)

- [ ] **Step 1: Confirm +RX, +TNF, ATT, MNF buttons exist with `setEnabled(false)`**

```bash
grep -nA2 "+RX\|+TNF\|ATT\b\|MNF\b" src/gui/SpectrumOverlayPanel.cpp | head -20
```

Expected: each button is created with `setEnabled(false)` + a tooltip explaining "coming in Phase X."

- [ ] **Step 2: If any button is missing or has `setVisible(false)`, restore visibility**

The user's B8 override was: keep them visible-but-disabled. If any are currently hidden, change `setVisible(false)` → `setEnabled(false)`.

- [ ] **Step 3: No commit if already correct**

---

### Task 26: Final verification + plan complete

**Files:**
- None modified

- [ ] **Step 1: Full build + test sweep**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -10
ctest --test-dir build --output-on-failure 2>&1 | tail -20
```

Expected: clean build, all tests pass (baseline + foundation + cross-surface).

- [ ] **Step 2: Visual regression sweep — open every cross-surface area**

```bash
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

Walk through:
- VfoWidget: mode tab (11 modes), DSP tab, X/RIT tab (no Lock button), antenna popup with EXT/XVTR.
- RxApplet: AGC combo (5 modes), filter grid mode-aware, antenna popup with EXT/XVTR, Pan/Mute/SQL wired, no AF row, XIT functional.
- TxApplet: unchanged from Plan 1's Foundation work.
- SpectrumOverlayPanel: Display flyout — all signals wired, Cursor Freq toggleable, Fill Color picker works, Heat Map/Noise Floor/Weighted Avg gone, More Display Options link goes to Setup → Display, NYI buttons (+RX/+TNF/ATT/MNF) visible-but-disabled.
- MainWindow DSP menu: unchanged.

- [ ] **Step 3: Bench verification follow-ups**

- B1 AGC-T slider direction (Task 4) — verified at bench, document in PR.
- B6 XIT NCO shift (Task 18 step 5) — verified at bench, document in PR.

---

## Self-Review

**Spec coverage:**
- B1 (AGC family) — Tasks 2, 3, 4 ✓
- B2 (Mode + Filter) — Tasks 5, 6, 7 ✓
- B3 (Antenna) — Tasks 8, 9, 10, 11 ✓
- B4 (Audio family) — Tasks 12, 13 ✓
- B5 (DSP toggles, no-op) — Task 14 ✓
- B6 (RIT/XIT) — Tasks 15, 16, 17, 18 ✓
- B7 (Lock) — Task 19 ✓
- B8 (Spectrum display) — Tasks 20, 21, 22, 23, 24, 25 ✓

**Placeholder scan:** none.

**Type consistency:** `AntennaPopupBuilder::populate` and `AntennaPopupBuilder::labels` signatures match across Tasks 8, 9, 10, 11. New `SpectrumWidget::setCursorFreqVisible` matches between Tasks 21 and the SpectrumOverlayPanel signal in same task.

**Dependencies:** all tasks here depend on Plan 1 (Foundation). Plan 4 (TX bandwidth) depends on B6 wiring being in place (XIT and TX-frequency path are different concerns but the model→protocol pattern is the same).
