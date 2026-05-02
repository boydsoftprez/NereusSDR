# UI Polish — Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land the mechanical foundation work for the UI polish PR — scrollbar gutter fix, style palette consolidation, SetupDialog regime unification, and modal dialog cleanup. ~70 files touched, virtually no behavior change beyond the intended QGroupBox padding correction. Sets a clean canvas for the cross-surface ownership work in the next plan.

**Architecture:** Three sub-systems land together: (A1) one-line `setContentsMargins(0, 0, 8, 0)` change to `AppletPanelWidget`'s stack layout; (A2/D) snap raw hex literals across ~14 file-local style blocks to canonical `Style::` constants in `StyleConstants.h`, promote 2-3 missing palette colours, and wire the dead `Style::sliderVStyle()` helper from `EqApplet`; (A3) consolidate 4 byte-for-byte copies of `applyDarkStyle()` into a single `Style::applyDarkPageStyle()` helper, and fix `SetupPage::addLabeledX()` helpers to call canonical `Style::k*Style` constants instead of pasting strings.

**Tech Stack:** Qt 6 / C++20 / CMake. Existing tooling: `Style::` namespace at `src/gui/StyleConstants.h`, `ColorSwatchButton` for color picker (no new widgets here), `AppSettings` for any new persistence (none in this plan). Pre-commit hooks (`scripts/install-hooks.sh`) run attribution + port-classification + inline-cite checks on every commit.

---

## Reference

- **Design doc:** `docs/architecture/ui-audit-polish-plan.md` (commit `50a2830`) — Phase A and Phase D sections.
- **Inventory snapshot:** `.superpowers/brainstorm/33882-1777661197/inventory-snapshot.md` — all the file-by-file style drift findings.
- **Source files most affected:**
  - `src/gui/StyleConstants.h` — palette additions go here
  - `src/gui/applets/AppletPanelWidget.cpp` — A1 scrollbar gutter
  - `src/gui/SetupPage.{h,cpp}` — A3 helper fixes
  - 14 files with file-local style blocks (enumerated below)
  - 4 files with `applyDarkStyle()` copies (enumerated below)

---

## File-Structure Overview

### New code (3 helpers added to StyleConstants.h)
| Symbol | Purpose | Used by |
|---|---|---|
| `Style::kLabelMid` | New `#8899aa` palette constant | VfoWidget AGC-T label, pan label, RxApplet AGC-T label |
| `Style::dspToggleStyle()` | Brighter green for DSP toggles (vs `kGreenBg` action buttons) | VfoWidget DSP tab toggles, SpectrumOverlayPanel DSP toggle |
| `Style::doubleSpinBoxStyle()` | Extract shared inline `QDoubleSpinBox` block | TxEqDialog, TxCfcDialog |
| `Style::applyDarkPageStyle(QWidget*)` | Replace 4 copies of `applyDarkStyle()` | TransmitSetupPages, DisplaySetupPages, AppearanceSetupPages, GeneralOptionsPage |

### Files modified
- `src/gui/StyleConstants.h` (+~50 lines for new helpers/constants)
- `src/gui/applets/AppletPanelWidget.cpp` (1 line for A1)
- `src/gui/applets/{Phone,Eq,Fm,Vax,Rx,Tx}Applet.cpp` (style consolidation)
- `src/gui/widgets/VfoWidget.cpp` (consolidate 4 local constants)
- `src/gui/SpectrumOverlayPanel.cpp` (consolidate 7+ local constants; preserve translucent rgba overlays as documented exception)
- `src/gui/TitleBar.cpp` (consolidate 3 local constants)
- `src/gui/SetupPage.cpp` (4 helper fixes)
- `src/gui/setup/{TransmitSetupPages,DisplaySetupPages,AppearanceSetupPages,GeneralOptionsPage}.cpp` (replace local applyDarkStyle copies)
- `src/gui/{AboutDialog,AddCustomRadioDialog,NetworkDiagnosticsDialog,SupportDialog}.cpp` (palette migration)
- `src/gui/applets/{TxEqDialog,TxCfcDialog}.cpp` (use new doubleSpinBoxStyle helper)

### Tests added
- `tests/tst_applet_panel_gutter.cpp` (new) — verifies A1 contentsMargins
- `tests/tst_style_constants.cpp` (new or extend existing) — verifies new helpers compile + return non-empty QString
- `tests/tst_setup_page_helpers.cpp` (new) — verifies A3 helpers don't paste duplicate stylesheets

### Verification commands

Throughout the plan you'll see references to these:

- **Build:** `cmake --build build -j$(nproc)` (or `--parallel` on Windows)
- **Run all tests:** `ctest --test-dir build --output-on-failure`
- **Visual smoke check:** kill any running `NereusSDR` instance, then `./build/NereusSDR &` — see "Auto-launch after build" in user feedback memory.
- **Pre-commit hook (auto-runs):** `scripts/git-hooks/pre-commit` — verifies Thetis attribution, port classification, inline-cite preservation. Don't bypass with `--no-verify`.

---

## Tasks

### Task 1: Branch setup + baseline

**Files:**
- None modified (verification only)

- [ ] **Step 1: Confirm working directory and branch state**

```bash
pwd  # expect: /Users/j.j.boyd/NereusSDR/.claude/worktrees/stoic-dhawan-d8b720
git status  # expect: on branch claude/stoic-dhawan-d8b720, clean tree (after prior design-doc commit)
git log --oneline -3  # expect: latest commit is the ui-audit-polish-plan.md design doc
```

- [ ] **Step 2: Run baseline tests**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -20
ctest --test-dir build --output-on-failure 2>&1 | tail -20
```

Expected: clean build, all existing tests pass. Capture pass count for later comparison ("baseline: 246 tests passing").

- [ ] **Step 3: Launch app to confirm visual baseline**

```bash
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
screencapture -x -t png /tmp/nereussdr-baseline.png
```

Note: visual baseline screenshot saved at `/tmp/nereussdr-baseline.png`. Useful reference for "no regression" checks later.

- [ ] **Step 4: No commit needed (verification only)**

---

### Task 2: A1 — Scrollbar gutter fix

**Files:**
- Modify: `src/gui/applets/AppletPanelWidget.cpp:81`
- Test: `tests/tst_applet_panel_gutter.cpp` (new)

- [ ] **Step 1: Write the failing test**

Create `tests/tst_applet_panel_gutter.cpp`:

```cpp
// Verify A1: AppletPanelWidget reserves an 8 px scrollbar gutter on the right.
// Per docs/architecture/ui-audit-polish-plan.md §A1.

#include <QApplication>
#include <QtTest/QtTest>
#include "gui/applets/AppletPanelWidget.h"
#include <QVBoxLayout>
#include <QScrollArea>

using NereusSDR::AppletPanelWidget;

class TestAppletPanelGutter : public QObject {
    Q_OBJECT
private slots:
    void stackLayoutReservesEightPxRightMargin();
};

void TestAppletPanelGutter::stackLayoutReservesEightPxRightMargin()
{
    AppletPanelWidget panel;
    // Find the QScrollArea, then its widget()'s layout — that's the stack layout.
    auto* scroll = panel.findChild<QScrollArea*>();
    QVERIFY(scroll != nullptr);
    auto* stackWidget = scroll->widget();
    QVERIFY(stackWidget != nullptr);
    auto* stackLayout = qobject_cast<QVBoxLayout*>(stackWidget->layout());
    QVERIFY(stackLayout != nullptr);

    QMargins m = stackLayout->contentsMargins();
    QCOMPARE(m.right(), 8);  // 8 px reserved for the scrollbar
    QCOMPARE(m.left(), 0);
    QCOMPARE(m.top(), 0);
    QCOMPARE(m.bottom(), 0);
}

QTEST_MAIN(TestAppletPanelGutter)
#include "tst_applet_panel_gutter.moc"
```

Update `tests/CMakeLists.txt` to register the test (mirror an existing `add_executable(...)` + `add_test(...)` block — there are several precedents in the file).

- [ ] **Step 2: Build and run; verify FAIL**

```bash
cmake --build build -j$(nproc) --target tst_applet_panel_gutter 2>&1 | tail -10
ctest --test-dir build -R tst_applet_panel_gutter --output-on-failure
```

Expected: test fails with `QCOMPARE(m.right(), 8)` returning the current value of `0`.

- [ ] **Step 3: Implement the fix**

Edit `src/gui/applets/AppletPanelWidget.cpp:81` (currently `m_stackLayout->setContentsMargins(0, 0, 0, 0);`):

```cpp
// Reserve 8 px on the right for the always-visible vertical scrollbar.
// Per docs/architecture/ui-audit-polish-plan.md §A1: with 13 applets stacked,
// the scrollbar is virtually always present; reserving the gutter prevents
// content from clipping under the bar.
m_stackLayout->setContentsMargins(0, 0, 8, 0);
```

- [ ] **Step 4: Build, run test, verify PASS**

```bash
cmake --build build -j$(nproc) --target tst_applet_panel_gutter
ctest --test-dir build -R tst_applet_panel_gutter --output-on-failure
```

Expected: test passes.

- [ ] **Step 5: Visual smoke check**

```bash
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

Open the app. Verify:
- Right panel applets show 8 px breathing room at right edge.
- Scrollbar (when shown) sits in the gutter, not over content.

- [ ] **Step 6: Commit**

```bash
git add tests/tst_applet_panel_gutter.cpp tests/CMakeLists.txt src/gui/applets/AppletPanelWidget.cpp
git commit -m "$(cat <<'EOF'
ui(applet-panel): reserve 8 px scrollbar gutter to prevent content clipping

The right-side applet panel hosts a QScrollArea with a vertical scrollbar
that is virtually always visible (13 applets stacked, one rarely fits the
viewport). The scrollbar lives inside the viewport, so content rendered
to the full width was being overlaid by the rightmost 8 px of the bar —
value boxes ("72", "-90", "100") and slider handles clipped, button
borders cut off.

Fix: m_stackLayout->setContentsMargins(0, 0, 8, 0). Content shifts left
by 8 px, scrollbar sits in its own column. Layout is stable whether the
bar is shown or hidden.

Per docs/architecture/ui-audit-polish-plan.md §A1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: A2 prep — Promote missing palette colours and helpers in StyleConstants.h

**Files:**
- Modify: `src/gui/StyleConstants.h`

- [ ] **Step 1: Read current StyleConstants.h to confirm insertion points**

```bash
sed -n '32,191p' src/gui/StyleConstants.h | head -30  # applet-side helpers
sed -n '32,40p' src/gui/StyleConstants.h  # see kAppBg etc.
```

- [ ] **Step 2: Add `kLabelMid` constant**

In `src/gui/StyleConstants.h`, after the existing text constants block (around line 38-40, near `kTextSecondary`/`kTextTertiary`/`kTextScale`):

```cpp
constexpr auto kTextScale       = "#607080";
constexpr auto kTextInactive    = "#405060";
// NereusSDR-original — used in 5+ places for AGC-T / pan / similar labels;
// sits between kTextSecondary (#8090a0) and kTextScale (#607080).
constexpr auto kLabelMid        = "#8899aa";
```

- [ ] **Step 3: Add `dspToggleStyle()` helper**

In `src/gui/StyleConstants.h`, after the existing `redCheckedStyle()` helper (around line 145), add:

```cpp
// DSP toggle — brighter green than the kGreenBg action buttons.
// Used by VfoWidget DSP tab and SpectrumOverlayPanel DSP toggles.
// Source: NereusSDR-original. Distinct semantic from action-button
// "checked" state because DSP toggles communicate "feature on" not
// "action engaged."
constexpr auto kDspToggleBg     = "#1a6030";
constexpr auto kDspToggleBorder = "#20a040";
constexpr auto kDspToggleText   = "#80ff80";

inline QString dspToggleStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kDspToggleBg, kDspToggleText, kDspToggleBorder);
}
```

- [ ] **Step 4: Add `doubleSpinBoxStyle()` helper (used in Task 27)**

After `kSpinBoxStyle` (around line 233):

```cpp
constexpr auto kSpinBoxStyle =
    "QSpinBox { background: #1a2a3a; border: 1px solid #304050;"
    " border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 2px 4px; }";

constexpr auto kDoubleSpinBoxStyle =
    "QDoubleSpinBox { background: #1a2a3a; border: 1px solid #304050;"
    " border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 2px 4px; }";

// Backwards-compat wrapper for places that prefer a function form.
inline QString doubleSpinBoxStyle() { return QStringLiteral(kDoubleSpinBoxStyle); }
```

- [ ] **Step 5: Add `applyDarkPageStyle()` helper (used in Tasks 18-21)**

After the existing setup-side constants (around line 244, after `kButtonStyle`), add:

```cpp
// Apply the canonical "dark page" stylesheet to a Setup page that lays
// itself out manually (i.e. doesn't inherit the SetupPage::addLabeledX
// helper-based widgets). Replaces the 4 byte-for-byte copies of
// applyDarkStyle() that previously lived in TransmitSetupPages,
// DisplaySetupPages, AppearanceSetupPages, GeneralOptionsPage.
//
// Per docs/architecture/ui-audit-polish-plan.md §A3.
//
// Note: this helper uses the canonical kBorder (#205070) and
// padding-top: 12 px values, NOT the drifted #203040 / 4 px that
// previously appeared in the 4 local copies. The drift is intentionally
// fixed — pages that used the local copies will gain 8 px of QGroupBox
// padding-top, matching the rest of the app.
inline void applyDarkPageStyle(QWidget* w)
{
    if (!w) { return; }
    w->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; color: %2; }"
        "QGroupBox { border: 1px solid %3; border-radius: 4px;"
        "  margin-top: 8px; padding-top: 12px; font-weight: bold; color: %4; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QLabel { color: %2; }"
        "QComboBox { background: %5; border: 1px solid %3;"
        "  border-radius: 3px; color: %2; padding: 2px 4px; }"
        "QComboBox QAbstractItemView { background: %5; color: %2; "
        "  selection-background-color: %6; }"
        "QSlider::groove:horizontal { background: %5; height: 4px;"
        "  border-radius: 2px; }"
        "QSlider::handle:horizontal { background: %6; width: 12px;"
        "  height: 12px; border-radius: 6px; margin: -4px 0; }"
        "QSpinBox, QDoubleSpinBox { background: %5; border: 1px solid %3;"
        "  border-radius: 3px; color: %2; padding: 2px 4px; }"
        "QCheckBox { color: %2; }"
        "QLineEdit { background: %5; border: 1px solid %3;"
        "  border-radius: 3px; color: %2; padding: 2px 4px; }"
        "QPushButton { background: %5; border: 1px solid %3;"
        "  border-radius: 3px; color: %2; padding: 3px 10px; }"
        "QPushButton:hover { background: %7; }"
    ).arg(kAppBg, kTextPrimary, kBorder, kTitleText, kButtonBg, kAccent, kButtonHover));
}
```

- [ ] **Step 6: Build to verify it compiles**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -10
```

Expected: clean build (no behavior change yet — just new symbols added).

- [ ] **Step 7: Commit**

```bash
git add src/gui/StyleConstants.h
git commit -m "$(cat <<'EOF'
style(palette): add kLabelMid, dspToggleStyle, doubleSpinBoxStyle, applyDarkPageStyle helpers

Foundation additions to StyleConstants.h to support the ~14-file style
consolidation in subsequent tasks:

- kLabelMid (#8899aa) — promoted from raw hex used in VfoWidget AGC-T
  label, pan label, and RxApplet AGC-T label (5+ call sites).
- kDspToggleBg/Border/Text + dspToggleStyle() helper — brighter green
  than kGreenBg action buttons, used for VfoWidget DSP tab toggles
  and SpectrumOverlayPanel DSP toggles.
- kDoubleSpinBoxStyle + doubleSpinBoxStyle() — extract from inline
  block used by TxEqDialog and TxCfcDialog (see §D5).
- applyDarkPageStyle(QWidget*) — replaces 4 byte-for-byte copies of
  local applyDarkStyle() functions in TransmitSetupPages,
  DisplaySetupPages, AppearanceSetupPages, GeneralOptionsPage. Uses
  canonical kBorder (#205070, vs drifted #203040) and 12 px QGroupBox
  padding-top (vs drifted 4 px) — the drift is intentionally fixed.

No call sites updated yet — those land in subsequent commits per
docs/architecture/ui-audit-polish-plan.md §A2/A3/D.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: A2 — Wire dead `Style::sliderVStyle()` from EqApplet

**Files:**
- Modify: `src/gui/applets/EqApplet.cpp`

- [ ] **Step 1: Locate EqApplet's local `kVSliderStyle` constant**

```bash
grep -n "kVSliderStyle" src/gui/applets/EqApplet.cpp
```

Expected: appears around line 108-110 in the local-constants block, plus 1+ usages.

- [ ] **Step 2: Verify the values match (so swap is safe)**

The local `kVSliderStyle` defines a 4 px-wide groove with `#203040` background and a 10 px-tall handle. `Style::sliderVStyle()` (StyleConstants.h:161-172) uses `%1` (= `kGroove` = `#203040`) for the groove and `%2` (= `kAccent`) for the handle. Visually identical — confirm by reading both.

```bash
sed -n '161,172p' src/gui/StyleConstants.h
sed -n '108,120p' src/gui/applets/EqApplet.cpp  # adjust line range to match actual file
```

- [ ] **Step 3: Replace `kVSliderStyle` usage**

In `src/gui/applets/EqApplet.cpp`, find each line that uses the local constant (e.g. `slider->setStyleSheet(kVSliderStyle);`) and replace with:

```cpp
slider->setStyleSheet(NereusSDR::Style::sliderVStyle());
```

Add `#include "gui/StyleConstants.h"` at the top if not already present.

- [ ] **Step 4: Remove the local `kVSliderStyle` constant**

Delete the `static constexpr const char* kVSliderStyle = ...` line(s).

- [ ] **Step 5: Build + run**

```bash
cmake --build build -j$(nproc) --target NereusSDR 2>&1 | tail -5
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

Open the EqApplet (right panel). The 10-band sliders should look identical to before.

- [ ] **Step 6: Commit**

```bash
git add src/gui/applets/EqApplet.cpp
git commit -m "$(cat <<'EOF'
style(eq-applet): use Style::sliderVStyle() instead of local kVSliderStyle

Style::sliderVStyle() at StyleConstants.h:161-172 had zero callers — the
canonical helper was effectively dead code. EqApplet.cpp had its own
file-local kVSliderStyle string with byte-identical groove/handle values
(via kGroove and kAccent substitution).

Migrating EqApplet to call the canonical helper. Removes the duplicate
local constant.

Per docs/architecture/ui-audit-polish-plan.md §A2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: A2 — Consolidate PhoneCwApplet local style block

**Files:**
- Modify: `src/gui/applets/PhoneCwApplet.cpp`

- [ ] **Step 1: Read the existing local-constants block**

```bash
sed -n '96,127p' src/gui/applets/PhoneCwApplet.cpp
```

Expected: 8 file-local constants — `kSliderStyle`, `kButtonBase`, `kBlueActive`, `kGreenActive`, `kLabelStyle`, `kDimLabelStyle`, `kInsetValueStyle`, `kTickLabelStyle`.

- [ ] **Step 2: Map each local constant to its canonical replacement**

| Local | Replacement |
|---|---|
| `kSliderStyle` | `Style::sliderHStyle()` |
| `kButtonBase` | `Style::buttonBaseStyle()` |
| `kBlueActive` | `Style::blueCheckedStyle()` |
| `kGreenActive` | `Style::greenCheckedStyle()` |
| `kLabelStyle` | inline `QLabel { color: Style::kTextPrimary; font-size: 10px; }` (no exact helper) |
| `kDimLabelStyle` | inline `QLabel { color: Style::kTextSecondary; font-size: 9.5px; }` |
| `kInsetValueStyle` | `Style::insetValueStyle()` |
| `kTickLabelStyle` | inline `QLabel { color: Style::kTextScale; font-size: 8.5px; }` |

For the three "no exact helper" mappings, inline a `QStringLiteral(...)` using the named palette constants (NOT raw hex).

- [ ] **Step 3: Add include**

```cpp
#include "gui/StyleConstants.h"
```

- [ ] **Step 4: Replace each `setStyleSheet(kFoo)` call site**

Search and replace systematically:

```bash
grep -n "kSliderStyle\|kButtonBase\|kBlueActive\|kGreenActive\|kLabelStyle\|kDimLabelStyle\|kInsetValueStyle\|kTickLabelStyle" src/gui/applets/PhoneCwApplet.cpp
```

Walk each result and replace. Example: `slider->setStyleSheet(kSliderStyle);` becomes `slider->setStyleSheet(NereusSDR::Style::sliderHStyle());`.

For the inline label styles, use:
```cpp
m_someLabel->setStyleSheet(QStringLiteral(
    "QLabel { color: %1; font-size: 10px; }"
).arg(NereusSDR::Style::kTextPrimary));
```

- [ ] **Step 5: Verify the pitch label inline strings at line 672**

Per the inventory: "pitch label at :672 has a one-off inline C-string with `#0a0a18`, `#1e2e3e`". Snap these:

- `#0a0a18` → `Style::kInsetBg` (or `kPanelBg` — same value, semantically `kInsetBg` here since it's a value display).
- `#1e2e3e` → `Style::kInsetBorder`.

- [ ] **Step 6: Delete the local-constants block (lines 96-127)**

- [ ] **Step 7: Build, run, visual check**

```bash
cmake --build build -j$(nproc) --target NereusSDR 2>&1 | tail -5
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

Open PhoneCwApplet. Phone tab: mic level gauge, mic gain slider, PROC button, etc. — all should look identical.

- [ ] **Step 8: Commit**

```bash
git add src/gui/applets/PhoneCwApplet.cpp
git commit -m "style(phone-cw-applet): consolidate 8 local style constants to canonical Style::

Per docs/architecture/ui-audit-polish-plan.md §A2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: A2 — Consolidate FmApplet local style block

**Files:**
- Modify: `src/gui/applets/FmApplet.cpp`

- [ ] **Step 1: Read the existing local-constants block (~lines 132-165)**

```bash
sed -n '132,165p' src/gui/applets/FmApplet.cpp
```

- [ ] **Step 2: Map each local constant to canonical** (similar pattern to Task 5)

The mapping follows Task 5's table — file-local style-string constants in NereusSDR's applets share a pattern (button-base + blue/green checked + label + slider).

- [ ] **Step 3: Apply replacements + remove local block**

Same process as Task 5.

- [ ] **Step 4: Build + visual check**

```bash
cmake --build build -j$(nproc) --target NereusSDR 2>&1 | tail -5
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

FmApplet is currently NYI (per inventory) — visual check is just "the placeholder still renders correctly."

- [ ] **Step 5: Commit**

```bash
git add src/gui/applets/FmApplet.cpp
git commit -m "style(fm-applet): consolidate 4 local style constants to canonical Style::

Per docs/architecture/ui-audit-polish-plan.md §A2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: A2 — Consolidate VaxApplet local style block

**Files:**
- Modify: `src/gui/applets/VaxApplet.cpp`

- [ ] **Step 1: Read existing block (~lines 52-68)**

5 local constants per inventory: `kSectionStyle`, `kGreenToggle`, `kDimLabel`, `kStatusLabel`, `kDeviceLabel`.

- [ ] **Step 2: Map and replace** (analogous to Task 5)

| Local | Replacement |
|---|---|
| `kSectionStyle` | inline using `kPanelBg` + `kBorderSubtle` + `kTitleText` |
| `kGreenToggle` | `Style::buttonBaseStyle() + Style::greenCheckedStyle()` |
| `kDimLabel` | inline using `kTextSecondary` |
| `kStatusLabel` | inline using `kTextPrimary` |
| `kDeviceLabel` | inline using `kTextScale` |

- [ ] **Step 3: Include + remove block + apply replacements**

- [ ] **Step 4: Build + visual check**

Open VAX applet. Per-channel meters + mute toggles render unchanged.

- [ ] **Step 5: Commit**

```bash
git add src/gui/applets/VaxApplet.cpp
git commit -m "style(vax-applet): consolidate 5 local style constants to canonical Style::

Per docs/architecture/ui-audit-polish-plan.md §A2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: A2 — Consolidate VfoWidget local style block + introduce kLabelMid + dspToggleStyle

**Files:**
- Modify: `src/gui/widgets/VfoWidget.cpp`

- [ ] **Step 1: Read existing block (~lines 291-333)**

4 file-local constants: `kFlatBtn`, `kTabBtn`, `kDspToggle`, `kModeBtn`.

- [ ] **Step 2: Map**

| Local | Replacement |
|---|---|
| `kFlatBtn` | `Style::buttonBaseStyle()` (variants for hover state inline if needed) |
| `kTabBtn` | inline using `kButtonBg` + `kBorder` + `kTextPrimary` (tab-row pattern) |
| `kDspToggle` | `Style::buttonBaseStyle() + Style::dspToggleStyle()` (NEW from Task 3) |
| `kModeBtn` | inline using `kButtonBg` (mode-tab buttons) |

- [ ] **Step 3: Replace AGC-T label + pan label hex (line 763, 776, 872, 1101)**

Find each occurrence of raw `"#8899aa"` and replace with `NereusSDR::Style::kLabelMid` (substitution via `.arg()` or direct).

- [ ] **Step 4: Apply replacements**

- [ ] **Step 5: Build + visual check**

Open VFO flag. DSP tab — NR1/NR2/NR3/NR4/DFNR/MNR buttons should be the brighter `dspToggleStyle()` green when active. AGC-T label color unchanged. Mode tab buttons unchanged.

- [ ] **Step 6: Commit**

```bash
git add src/gui/widgets/VfoWidget.cpp
git commit -m "style(vfo-widget): consolidate 4 local style constants; use kLabelMid + dspToggleStyle

VfoWidget had 4 file-local style constants (kFlatBtn, kTabBtn, kDspToggle,
kModeBtn). Migrated to canonical Style:: helpers from Task 3 — kFlatBtn →
buttonBaseStyle(), kDspToggle → buttonBaseStyle() + dspToggleStyle() (newly
added for the brighter DSP toggle green), kTabBtn and kModeBtn inlined
with named palette constants.

Also snapped the four raw \"#8899aa\" occurrences (AGC-T label, pan label,
etc.) to the new Style::kLabelMid constant.

Per docs/architecture/ui-audit-polish-plan.md §A2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 9: A2 — Consolidate SpectrumOverlayPanel local style block

**Files:**
- Modify: `src/gui/SpectrumOverlayPanel.cpp`

- [ ] **Step 1: Read existing block (~lines 73-116)**

7+ local constants: `kPanelStyle`, `kLabelStyle`, `kSliderStyle`, `kMenuBtnNormal`, `kMenuBtnActive`, `kMenuBtnDisabled`, `kDspBtnStyle`, `kDisplayToggleStyle`.

- [ ] **Step 2: Identify the legitimate exception — translucent rgba overlays**

`SpectrumOverlayPanel` uses translucent backgrounds like `rgba(20, 32, 48, 0.85)` for buttons that float over the spectrum. These are NOT in the canonical palette (which is opaque) and serve a different purpose. Keep these in a clearly-named local block with a comment justifying.

- [ ] **Step 3: Map non-overlay constants to canonical**

| Local | Replacement |
|---|---|
| `kPanelStyle` | translucent — KEEP as `OverlayColors::kPanelBg` (rename to clarify scope) |
| `kLabelStyle` | inline using `kTextPrimary` |
| `kSliderStyle` | `Style::sliderHStyle()` |
| `kMenuBtnNormal` | translucent — KEEP under `OverlayColors::*` |
| `kMenuBtnActive` | translucent — KEEP |
| `kMenuBtnDisabled` | translucent — KEEP |
| `kDspBtnStyle` | `Style::buttonBaseStyle() + Style::dspToggleStyle()` |
| `kDisplayToggleStyle` | `Style::buttonBaseStyle() + Style::blueCheckedStyle()` |

- [ ] **Step 4: Add comment block to justify retained `OverlayColors::*`**

```cpp
// Translucent overlay colors — these are intentionally NOT in
// StyleConstants.h because they're designed to render over the
// changing spectrum background (an alpha-blended composition is
// inherent to their purpose). Per
// docs/architecture/ui-audit-polish-plan.md §A2 — "tightly-scoped
// local blocks for legitimate exceptions."
namespace OverlayColors {
    constexpr auto kPanelBg       = "rgba(20, 32, 48, 0.85)";
    constexpr auto kMenuBtnNormal = "rgba(20, 32, 48, 0.95)";
    constexpr auto kMenuBtnActive = "rgba(0, 112, 192, 0.55)";
    constexpr auto kMenuBtnDisabled = "rgba(20, 32, 48, 0.4)";
}
```

- [ ] **Step 5: Apply replacements**

- [ ] **Step 6: Build + visual check**

Spectrum overlay panel — translucent buttons over the spectrum unchanged. DSP toggle (when wired) uses the brighter green from `dspToggleStyle()`.

- [ ] **Step 7: Commit**

```bash
git add src/gui/SpectrumOverlayPanel.cpp
git commit -m "$(cat <<'EOF'
style(spectrum-overlay): consolidate canonical-replaceable constants; retain translucent overlays

SpectrumOverlayPanel had 7+ file-local style constants. Migrated the
opaque ones to canonical Style:: helpers:
- kSliderStyle → Style::sliderHStyle()
- kDspBtnStyle → Style::buttonBaseStyle() + Style::dspToggleStyle()
- kDisplayToggleStyle → Style::buttonBaseStyle() + Style::blueCheckedStyle()
- kLabelStyle → inline using Style::kTextPrimary

Retained translucent overlay colours (kPanelBg, kMenuBtnNormal/Active/Disabled)
as a documented local exception in an OverlayColors:: namespace — these are
designed to alpha-blend over the changing spectrum background and have no
canonical-palette equivalent.

Per docs/architecture/ui-audit-polish-plan.md §A2 ("tightly-scoped local
blocks for legitimate exceptions").

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 10: A2 — Consolidate TitleBar local style block

**Files:**
- Modify: `src/gui/TitleBar.cpp`

- [ ] **Step 1: Read existing block (~lines 78-91)**

3 local constants: `kStripStyle`, `kAppNameStyle`, `kMenuBarStyle`. Plus inline hex for the 💡 feature button.

- [ ] **Step 2: Snap `#0a0a14` to `Style::kStatusBarBg` (already exists at StyleConstants.h:104)**

The TitleBar background was using raw `#0a0a14` — that's exactly `kStatusBarBg`. Replace.

- [ ] **Step 3: Snap other duplicates**

`#203040` → `kBorderSubtle`. `#00b4d8` → `kAccent`. `#8aa8c0` → `kTitleText`. `#0f0f1a` → `kAppBg`. `#304050` → already a duplicate of `kOverlayBorder` value.

- [ ] **Step 4: 💡 feature button — flag for B7 review**

The button uses `#3a2a00` / `#806020`. These are currently one-off "amber dark" colors. Per design doc D9b: flagged as one-off, no promotion needed yet. Keep as inline hex with a `// NereusSDR-original — amber dark for the 💡 feature-request button` comment.

- [ ] **Step 5: Apply replacements + retain block (if anything is genuinely TitleBar-specific) with rename**

If after substitutions nothing genuinely-local remains, delete the local block. Otherwise rename to `TitleBarColors::*` per the same pattern as SpectrumOverlayPanel.

- [ ] **Step 6: Build + visual check**

TitleBar — connection segment, master output widget, app name "NereusSDR", 💡 button — all render identically.

- [ ] **Step 7: Commit**

```bash
git add src/gui/TitleBar.cpp
git commit -m "$(cat <<'EOF'
style(title-bar): snap palette duplicates to canonical Style::

#0a0a14 → kStatusBarBg (already named at StyleConstants.h:104)
#203040 → kBorderSubtle
#00b4d8 → kAccent
#8aa8c0 → kTitleText
#0f0f1a → kAppBg

Retained #3a2a00 / #806020 inline (NereusSDR-original amber-dark for the
💡 feature-request button) per docs/architecture/ui-audit-polish-plan.md
§A2 — flagged as one-off; no palette promotion warranted.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 11: A2 — Snap RxApplet raw-hex literals to canonical

**Files:**
- Modify: `src/gui/applets/RxApplet.cpp`

- [ ] **Step 1: Inventory raw-hex usages**

Per inventory snapshot: RxApplet has hardcoded hex in 12+ places — `#4488ff`, `#ff4444`, `#ff6666`, `#00c8ff`, `#8aa8c0`, `#8899aa`, `#1a2a3a`, `#00b4d8`, `#1a1a1a`, `#445`, `#556`, `#adff2f`, `#33aa33`.

```bash
grep -nE "#[0-9a-fA-F]{6}|#[0-9a-fA-F]{3}\b" src/gui/applets/RxApplet.cpp | head -30
```

- [ ] **Step 2: Snap mapping**

| Raw | Replace with |
|---|---|
| `#1a2a3a` | `Style::kButtonBg` |
| `#00b4d8` | `Style::kAccent` |
| `#8aa8c0` | `Style::kTitleText` |
| `#8899aa` | `Style::kLabelMid` (Task 3) |
| `#1a1a2a` | `Style::kDisabledBg` |
| Lock/RX-ant `#4488ff` | KEEP as one-off — flagged for B7/B3 review (saved Step 7 below) |
| TX-ant `#ff4444` | already named `kRedBorder`/`kGaugeDanger` — semantic mismatch (used as "TX color" not error). KEEP as one-off; flag for B3 review. |
| `#33aa33` | KEEP as one-off (NereusSDR AGC-info green). Add comment. |

- [ ] **Step 3: Apply replacements**

For each non-flagged hex, use `.arg(NereusSDR::Style::kFoo)` substitution.

- [ ] **Step 4: Build + visual check**

RxApplet — all controls render identically. Lock button still uses the one-off `#4488ff` (will be reconsidered in Plan 2 / B7).

- [ ] **Step 5: Commit**

```bash
git add src/gui/applets/RxApplet.cpp
git commit -m "$(cat <<'EOF'
style(rx-applet): snap raw-hex duplicates to canonical Style:: constants

#1a2a3a → kButtonBg
#00b4d8 → kAccent
#8aa8c0 → kTitleText
#8899aa → kLabelMid (newly added in §A2 prep)
#1a1a2a → kDisabledBg

Retained as one-offs (flagged for downstream B-series review):
- #4488ff (Lock button + RX-ant 'live blue') — B7/B3 territory
- #ff4444 (TX-ant 'TX color') — B3 territory; semantic mismatch with
  the named kRedBorder/kGaugeDanger which connote error/danger
- #33aa33 (AGC-info green) — NereusSDR-original; one location

Per docs/architecture/ui-audit-polish-plan.md §A2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 12: A2 — Snap TxApplet raw-hex literals to canonical (preserve MON button)

**Files:**
- Modify: `src/gui/applets/TxApplet.cpp`

- [ ] **Step 1: Inventory raw-hex usages**

```bash
grep -nE "#[0-9a-fA-F]{6}|#[0-9a-fA-F]{3}\b" src/gui/applets/TxApplet.cpp | head -20
```

Per inventory: `#001a33`, `#3399ff` (MON button), `#006030`, `#008040` (PS-A), `#405060`.

- [ ] **Step 2: Snap mapping**

| Raw | Replace with |
|---|---|
| `#405060` | `Style::kTextInactive` |
| MON button `#001a33` / `#3399ff` | KEEP — flag as one-off NereusSDR-original. Add comment explaining "MON button intentionally uses dark-navy/cyan to distinguish from other blue toggles." |
| PS-A `#006030` / `#008040` | KEEP — flag for 3M-4 review (PureSignal phase). |

- [ ] **Step 3: Apply + commit**

```bash
git add src/gui/applets/TxApplet.cpp
git commit -m "$(cat <<'EOF'
style(tx-applet): snap kTextInactive raw-hex; retain MON + PS-A one-offs

Snapped #405060 → kTextInactive.

Retained as documented one-offs:
- #001a33 / #3399ff (MON button checked state — NereusSDR-original
  intentional dark-navy/cyan to distinguish from generic blue toggles).
- #006030 / #008040 (PS-A button — flagged for 3M-4 PureSignal phase
  review).

Per docs/architecture/ui-audit-polish-plan.md §A2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 13: A2 — Verification sweep across all consolidated files

**Files:**
- None modified (verification)

- [ ] **Step 1: Confirm no raw `kBootBg` / `kButtonBg` value leaks remain in applet files**

```bash
grep -nE "#1a2a3a|#c8d8e8|#0f0f1a|#205070|#203040|#0a0a18|#1e2e3e" src/gui/applets/*.cpp src/gui/widgets/VfoWidget.cpp src/gui/SpectrumOverlayPanel.cpp src/gui/TitleBar.cpp 2>&1 | grep -v "OverlayColors\|TitleBarColors\|// NereusSDR-original\|// From Thetis"
```

Expected: empty output (or only allowed exceptions in retained local blocks). Any unexpected hits → re-snap or document.

- [ ] **Step 2: Build + run all tests**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -10
ctest --test-dir build --output-on-failure 2>&1 | tail -10
```

Expected: clean build, all tests pass (no behavior change).

- [ ] **Step 3: Visual regression sweep**

```bash
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
screencapture -x -t png /tmp/nereussdr-after-A2.png
```

Compare `/tmp/nereussdr-after-A2.png` against `/tmp/nereussdr-baseline.png` (Task 1) — should be visually identical.

- [ ] **Step 4: No commit needed (verification step)**

---

### Task 14: A3 — Fix `SetupPage::addLabeledCombo` to use canonical Style::kComboStyle

**Files:**
- Modify: `src/gui/SetupPage.cpp:228-236`

- [ ] **Step 1: Read existing inline string**

```bash
sed -n '225,245p' src/gui/SetupPage.cpp
```

Expected: the function pastes the same QSS as `Style::kComboStyle` (StyleConstants.h:208-213) byte-for-byte.

- [ ] **Step 2: Replace inline string with `Style::kComboStyle`**

Change the body of `SetupPage::addLabeledCombo` from:
```cpp
combo->setStyleSheet(QStringLiteral("QComboBox { background: #1a2a3a; ... }"));
```
to:
```cpp
combo->setStyleSheet(NereusSDR::Style::kComboStyle);
```

Add `#include "gui/StyleConstants.h"` if not already present.

- [ ] **Step 3: Build + run tests**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
ctest --test-dir build --output-on-failure 2>&1 | tail -10
```

Expected: clean build, tests pass.

- [ ] **Step 4: Visual check — open one Setup page that uses addLabeledCombo**

```bash
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

Open Setup → General → Navigation. Combo box renders identically.

- [ ] **Step 5: Commit**

```bash
git add src/gui/SetupPage.cpp
git commit -m "$(cat <<'EOF'
style(setup-page): addLabeledCombo uses canonical Style::kComboStyle

The base-class addLabeledCombo helper was pasting a byte-for-byte
duplicate of Style::kComboStyle inline. Replaced with the canonical
constant. ~30 setup pages that go through this helper inherit any
future palette changes automatically.

Per docs/architecture/ui-audit-polish-plan.md §A3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 15: A3 — Fix `SetupPage::addLabeledSlider` to use canonical Style::kSliderStyle

Same pattern as Task 14, applied to `addLabeledSlider` at `SetupPage.cpp:241-246`.

- [ ] **Step 1-5:** mirror Task 14 with the slider helper.

- [ ] **Step 6: Commit**

```bash
git add src/gui/SetupPage.cpp
git commit -m "style(setup-page): addLabeledSlider uses canonical Style::kSliderStyle

Per docs/architecture/ui-audit-polish-plan.md §A3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 16: A3 — Fix `SetupPage::addLabeledButton` to use canonical Style::kButtonStyle

Same pattern, applied to `SetupPage.cpp:181-185`.

- [ ] **Step 1-5:** mirror Task 14 with the button helper.

- [ ] **Step 6: Commit**

```bash
git add src/gui/SetupPage.cpp
git commit -m "style(setup-page): addLabeledButton uses canonical Style::kButtonStyle

Per docs/architecture/ui-audit-polish-plan.md §A3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 17: A3 — Fix `SetupPage::addLabeledEdit` to use canonical Style::kLineEditStyle

Same pattern, applied to `SetupPage.cpp:292-296`.

- [ ] **Step 1-5:** mirror Task 14 with the edit helper.

- [ ] **Step 6: Commit**

```bash
git add src/gui/SetupPage.cpp
git commit -m "style(setup-page): addLabeledEdit uses canonical Style::kLineEditStyle

Per docs/architecture/ui-audit-polish-plan.md §A3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 18: A3 — Replace TransmitSetupPages applyDarkStyle with canonical helper

**Files:**
- Modify: `src/gui/setup/TransmitSetupPages.cpp:118-151`

- [ ] **Step 1: Locate the local `applyDarkStyle()` function**

```bash
grep -n "applyDarkStyle" src/gui/setup/TransmitSetupPages.cpp
```

Expected: function definition around line 118-151 plus usage call sites.

- [ ] **Step 2: Replace each call site**

Find each `applyDarkStyle(this);` call (or `applyDarkStyle(someWidget);`) and replace with `NereusSDR::Style::applyDarkPageStyle(this);`. Add `#include "gui/StyleConstants.h"` if needed.

- [ ] **Step 3: Delete the local `applyDarkStyle` function definition**

Delete lines 118-151 in `TransmitSetupPages.cpp`.

- [ ] **Step 4: Build, run tests**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
ctest --test-dir build --output-on-failure 2>&1 | tail -10
```

- [ ] **Step 5: Visual check — Setup → Transmit → Power & PA**

```bash
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

Open Setup → Transmit → Power & PA. **Note expected change:** QGroupBox padding-top moves from 4 px (drift) to 12 px (canonical) — visible 8 px shift in groupbox header positions. This is the intended fix per design doc §A3.

- [ ] **Step 6: Commit**

```bash
git add src/gui/setup/TransmitSetupPages.cpp
git commit -m "$(cat <<'EOF'
style(transmit-setup): use Style::applyDarkPageStyle instead of local copy

TransmitSetupPages had a local applyDarkStyle() function (lines 118-151)
that drifted to #203040 borders (vs canonical kBorder #205070) and
padding-top: 4px (vs canonical 12px). Replaced with calls to the new
Style::applyDarkPageStyle helper which uses canonical values.

Visible change: QGroupBox padding-top corrects from 4 px to 12 px (an
8 px header-position shift). This is the intended fix per
docs/architecture/ui-audit-polish-plan.md §A3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 19: A3 — Replace DisplaySetupPages applyDarkStyle with canonical helper

Same pattern as Task 18, applied to `src/gui/setup/DisplaySetupPages.cpp:98-128`.

Visual check: Setup → Display → Spectrum Defaults / Waterfall / Grid — same +8 px QGroupBox padding-top shift expected.

Commit message mirrors Task 18 with `display-setup` prefix.

---

### Task 20: A3 — Replace AppearanceSetupPages applyDarkStyle with canonical helper

Same pattern as Task 18, applied to `src/gui/setup/AppearanceSetupPages.cpp:19-50`.

Visual check: Setup → Appearance → Colors & Theme. Pages are mostly NYI placeholders but the QGroupBox layout shift is still visible.

Commit message mirrors Task 18 with `appearance-setup` prefix.

---

### Task 21: A3 — Replace GeneralOptionsPage applyDarkStyle with canonical helper

Same pattern as Task 18, applied to `src/gui/setup/GeneralOptionsPage.cpp:79-101`.

Visual check: Setup → General → Options — same QGroupBox padding shift.

Commit message mirrors Task 18 with `general-options` prefix.

---

### Task 22: D — Migrate AddCustomRadioDialog to canonical Style::

**Files:**
- Modify: `src/gui/AddCustomRadioDialog.cpp`

- [ ] **Step 1: Identify the 4 file-local style constants**

```bash
grep -n "kFieldStyle\|kPrimaryButtonStyle\|kSecondaryButtonStyle\|kCheckStyle" src/gui/AddCustomRadioDialog.cpp | head -10
```

- [ ] **Step 2: Map**

| Local | Replacement |
|---|---|
| `kFieldStyle` | `Style::kLineEditStyle` (for line edits) and `Style::kSpinBoxStyle` (for spinboxes) — split per widget type |
| `kPrimaryButtonStyle` | `Style::buttonBaseStyle() + ":hover { background: kAccent; }"` — primary uses accent for hover |
| `kSecondaryButtonStyle` | `Style::buttonBaseStyle()` (default hover) |
| `kCheckStyle` | `Style::kCheckBoxStyle` |

- [ ] **Step 3: Snap raw hex**

The dialog has many raw hex literals (per inventory: `#12202e`, `#c8d8e8`, `#304050`, `#00b4d8`, etc.). Map to named constants where possible; keep dialog-specific ones (`#2a1010` for inline error background) as documented exceptions.

- [ ] **Step 4: Apply replacements + remove local block**

- [ ] **Step 5: Build + visual check**

```bash
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

Open ConnectionPanel → Add Manually. Dialog renders identically to baseline.

- [ ] **Step 6: Commit**

```bash
git add src/gui/AddCustomRadioDialog.cpp
git commit -m "$(cat <<'EOF'
style(add-custom-radio-dialog): consolidate 4 local style constants

Replaced kFieldStyle/kPrimaryButtonStyle/kSecondaryButtonStyle/kCheckStyle
with canonical Style:: helpers (kLineEditStyle, buttonBaseStyle,
kCheckBoxStyle). Snapped raw hex duplicates to named constants.

Per docs/architecture/ui-audit-polish-plan.md §D.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 23: D — Migrate NetworkDiagnosticsDialog to canonical Style::

**Files:**
- Modify: `src/gui/NetworkDiagnosticsDialog.cpp`

- [ ] **Steps 1-6:** mirror Task 22 with this dialog's 3 local `constexpr const char*` constants and inline styles. Mapping similar — labels, frame backgrounds, button styles.

- [ ] **Commit message** prefix `network-diagnostics-dialog`.

---

### Task 24: D — Migrate SupportDialog to canonical Style::

**Files:**
- Modify: `src/gui/SupportDialog.cpp`

- [ ] **Steps 1-6:** mirror Task 22. SupportDialog uses per-widget inline strings — replace each `setStyleSheet(QStringLiteral(...))` call with the appropriate `Style::` helper.

- [ ] **Commit message** prefix `support-dialog`.

---

### Task 25: D — Finish AboutDialog migration

**Files:**
- Modify: `src/gui/AboutDialog.cpp`

- [ ] **Step 1: Find remaining raw hex**

AboutDialog already partially uses `Style::k*` constants. Identify the ~6 remaining raw hex literals (`#8899aa`, `#aabbcc`, `#334455`, etc.).

- [ ] **Step 2: Snap to canonical or flag as one-off**

`#8899aa` → `Style::kLabelMid`. Others are AboutDialog-specific subtle gray-blues; keep with comment justifying.

- [ ] **Step 3-5:** mirror Task 22.

- [ ] **Commit message** prefix `about-dialog`.

---

### Task 26: D — Extract Style::doubleSpinBoxStyle from TxEqDialog inline block

**Files:**
- Modify: `src/gui/applets/TxEqDialog.cpp`

- [ ] **Step 1: Find the inline `QDoubleSpinBox { ... }` block**

```bash
grep -n "QDoubleSpinBox" src/gui/applets/TxEqDialog.cpp
```

Expected: an inline string at the dialog-level setStyleSheet that includes a `QDoubleSpinBox { background: #1a2a3a; ... }` rule.

- [ ] **Step 2: Replace with `Style::doubleSpinBoxStyle()` (added in Task 3)**

Concatenate `Style::doubleSpinBoxStyle()` with the existing setup-side constants in the dialog-level setStyleSheet call.

- [ ] **Step 3: Remove the inline block**

- [ ] **Step 4: Build + visual check**

Right-click EQ button on TxApplet → opens TxEqDialog. QDoubleSpinBox rows in the parametric panel render identically.

- [ ] **Step 5: Commit**

```bash
git add src/gui/applets/TxEqDialog.cpp
git commit -m "$(cat <<'EOF'
style(tx-eq-dialog): use Style::doubleSpinBoxStyle helper

Replaces the inline QDoubleSpinBox { background: #1a2a3a; border: 1px solid
#304050; ... } block with the canonical helper added in §A2 prep.

Per docs/architecture/ui-audit-polish-plan.md §D.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 27: D — Extract Style::doubleSpinBoxStyle from TxCfcDialog inline block

Same pattern as Task 26, applied to `src/gui/applets/TxCfcDialog.cpp`.

The CFC dialog's inline `QDoubleSpinBox` block is identical to TxEqDialog's. Replace with `Style::doubleSpinBoxStyle()`.

Commit message prefix `tx-cfc-dialog`.

---

### Task 28: Final verification + plan complete

**Files:**
- None modified

- [ ] **Step 1: Full build + test sweep**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -10
ctest --test-dir build --output-on-failure 2>&1 | tail -20
```

Expected: clean build. Test count matches baseline + 1 (Task 2 added `tst_applet_panel_gutter`). All passing.

- [ ] **Step 2: Visual regression sweep**

```bash
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
screencapture -x -t png /tmp/nereussdr-after-foundation.png
```

Open every page in Setup. Look for unexpected visual differences. Expected differences:
- Setup → Transmit / Display / Appearance / GeneralOptions: QGroupBox header positions shifted by ~8 px (intended fix from A3).
- Right panel applets: 8 px gutter on the right (intended fix from A1).

Anything else surprising → file a follow-up ticket; do not block the plan.

- [ ] **Step 3: Confirm pre-commit hooks ran on every commit**

```bash
git log --oneline 50a2830..HEAD | head -30
```

All commits should have logged `[pre-commit]` checks (visible from earlier commit output).

- [ ] **Step 4: Plan summary**

The Foundation plan is complete. ~28 commits land:
- 1 design-doc commit (already in history)
- 1 Task 1 baseline (no commit; verification only)
- 1 A1 (scrollbar gutter)
- 1 A2 prep (palette additions)
- 8 A2 file consolidations (sliderVStyle wire-up, PhoneCwApplet, FmApplet, VaxApplet, VfoWidget, SpectrumOverlayPanel, TitleBar, RxApplet, TxApplet)
- 1 A2 verification sweep (no commit; verification only)
- 4 A3 SetupPage helper fixes (combo, slider, button, edit)
- 4 A3 applyDarkStyle replacements (Transmit, Display, Appearance, GeneralOptions)
- 5 D dialog migrations (AddCustomRadio, NetworkDiagnostics, Support, About, TxEq+TxCfc shared block × 2)
- 1 final verification (no commit)

Net: ~24 commits, ~70 files touched, no behavior change beyond the intended +8 px QGroupBox padding correction on 4 setup pages and the +8 px right-edge gutter in the applet panel.

- [ ] **Step 5: Plan handoff**

Foundation is now ready for Plan 2 (Cross-surface ownership) and Plan 3 (Right-panel cleanup) to land on top. Plans 4 and 5 (TX bandwidth + Profile-awareness) also depend on this foundation but can land in parallel once Plans 2/3 are clear.

---

## Self-Review

**Spec coverage:** Each Phase A and Phase D decision in `docs/architecture/ui-audit-polish-plan.md` is implemented:
- A1 scrollbar gutter — Task 2 ✓
- A2 palette additions — Task 3 ✓
- A2 dead helper wire-up — Task 4 ✓
- A2 file consolidations — Tasks 5-12 (8 files) ✓
- A2 verification — Task 13 ✓
- A3 SetupPage helpers — Tasks 14-17 ✓
- A3 applyDarkStyle replacements — Tasks 18-21 ✓
- D modal dialog cleanup — Tasks 22-27 ✓

**Placeholder scan:** No "TBD", "TODO", "fill in" entries remain. Every Step has a concrete command, code block, or commit text.

**Type consistency:** New helpers introduced in Task 3 (`Style::dspToggleStyle`, `Style::doubleSpinBoxStyle`, `Style::applyDarkPageStyle`, `Style::kLabelMid`) are referenced consistently in subsequent tasks (Tasks 8, 9, 18-21, 26, 27).

**Outstanding flags forwarded to subsequent plans:**
- `#4488ff` (RxApplet Lock + RX-ant) — Plan 2 / B7 review
- `#ff4444` (RxApplet TX-ant) — Plan 2 / B3 review
- `#001a33` / `#3399ff` (TxApplet MON button) — kept as one-off; flagged in commit message
- `#006030` / `#008040` (TxApplet PS-A) — kept; flagged for 3M-4 phase
- `#33aa33` (RxApplet AGC-info green) — kept as NereusSDR-original
- `#3a2a00` / `#806020` (TitleBar 💡) — kept as NereusSDR-original

Plan 2 will revisit any of these that touch B-series ownership decisions.
