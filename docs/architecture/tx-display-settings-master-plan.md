# TX Display Settings: Master Plan (All 6 Phases)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement each phase. Each phase below has bite-sized tasks with checkbox (`- [ ]`) syntax for tracking. Phases 2-6 task breakdowns are summary-level and will be expanded when each phase starts.

**Goal:** Port Thetis's full TX-side display settings architecture into NereusSDR. The user transmits and the panadapter / waterfall / grid / colors render with TX-tuned settings independent of RX. Matches Thetis Setup → Display → TX, Setup → Appearance → TX Display, and Setup → General → Calibration → TX Display Cal exactly. No hand-waving, no shortcuts. Every Thetis TX display control either ships in the corresponding NereusSDR phase or is explicitly out-of-scope with rationale.

**Architecture:**
- Per-frame MOX branch in the render path mirrors Thetis `display.cs:6506-6595 [v2.10.3.13+501e3f51]`. No state machine, no save/restore, no event handler. The waterfall colormap function reads TX values from `m_tx*` members when MOX active, RX values otherwise.
- AppSettings keys mirror Thetis DB keys with `DisplayTxWf*` / `DisplayTxPan*` / `DisplayTxGrid*` / `AppearanceTxDisplay*` prefixes.
- Setup tabs mirror Thetis tab IA 1:1 (tpDisplayTransmit, tcAppearanceTXDisplay, grpBoxTXDisplayCal). NereusSDR's existing `TxDisplayPage` (currently 4 stub controls) is expanded; the existing TX color controls in `AppearanceSetupPages.cpp` are migrated, not duplicated.
- Reuse the existing RX FFT slider, window combo, detector combo, averaging combo, time spinbox, and color picker widgets. Refactor into reusable components if they aren't already, then bind one instance to RX state and another to TX state.
- No per-band variants. Confirmed by exhaustive grep across Thetis Console source.

**Tech stack:** C++20, Qt6, AppSettings (NereusSDR XML), QSlider/QSpinBox/QComboBox, ColorSwatchButton, custom multi-stop gradient picker (new in Phase 2), `qCInfo(lcDsp)` logging, GPG-signed commits.

**Spec & mockup:**
- Mockup: [docs/architecture/mockups/tx-display-tab.html](mockups/tx-display-tab.html) (visual layout for all 3 affected Setup tabs).
- Source-read findings: this document, §"Source-read summary" below.

**Worktree:** `/Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display`. Branch `claude/tx-display`. Stays on this branch through all 6 phases. Cleanup-squash before Phase 1 starts.

**Thetis source pin for new cites:** `[v2.10.3.13+501e3f51]`.

**Project conventions reminder:**
- All commits GPG-signed (`-S`) per `feedback_gpg_sign_commits.md`. Never `--no-gpg-sign`.
- No em-dashes (U+2014) in any drafted text including commit messages, per `feedback_no_em_dashes.md`. Inline `// From Thetis ...` cite-format comments are the documented exception.
- Inline cite stamps `[v2.10.3.13+501e3f51]` on every new `// From Thetis` cite per `feedback_inline_cite_versioning.md`.
- READ → SHOW → TRANSLATE protocol on every Thetis port. STOP-AND-ASK if a value cannot be located in Thetis source.

---

## Source-read summary

The Thetis TX display surface enumerated by the prior research dispatch (returned 21 controls + 13 appearance items + 1 cal offset). Group-by-group placement in Thetis:

| Thetis group | File:line | Controls (count) |
|---|---|---|
| `groupBoxTS8` "Fast Fourier Transform" | `setup.designer.cs:36509-36643 [v2.10.3.13+501e3f51]` | FFT Size slider, Window combo |
| `groupBoxTS7` "Panadapter" | `setup.designer.cs:36645-36758 [v2.10.3.13+501e3f51]` | Detector, Averaging, Time, Normalize |
| `groupBoxTS9` "Waterfall" | `setup.designer.cs:36401-36498 [v2.10.3.13+501e3f51]` | Detector, Averaging, Time |
| `grpTXWFAmpScale` "Waterfall" | `setup.designer.cs:36246-36379 [v2.10.3.13+501e3f51]` | Low Level, High Level, Palette, Low Color |
| `grpTXSpectrumGrid` "TX Grid Scale" | `setup.designer.cs:36769-36932 [v2.10.3.13+501e3f51]` | Max, Min, Step, Display Grid, Fill, Label Align |
| `tcAppearanceTXDisplay` "TX Display" | `setup.designer.cs:53042-53090 [v2.10.3.13+501e3f51]` | Custom gradients (panadapter, waterfall), 10+ color/alpha controls, 2 toggle overlays |
| `grpBoxTXDisplayCal` "TX Display Cal" | `setup.designer.cs:11792-11843 [v2.10.3.13+501e3f51]` | TX Display Offset (dB) |

Render-path swap mechanism (critical): `display.cs:6506-6595 [v2.10.3.13+501e3f51]` does an inline per-frame conditional `if (localMox()) use TX values else use RX values`. NereusSDR mirrors this in `SpectrumWidget::pushWaterfallRow` and the trace render path.

---

## Existing NereusSDR state

Discovered during the audit prior to writing this plan:

- `TxDisplayPage` exists at [DisplaySetupPages.cpp:2234-2278](src/gui/setup/DisplaySetupPages.cpp#L2234) but only has 4 stub controls (Background, Grid Color, Line Width [NYI], Cal Offset [NYI]). All 4 are RX-styled mock entries; none wire into `SpectrumWidget` TX state. Phase 1 expands this page to the full Thetis layout.
- `m_txZeroLineColorBtn` at [AppearanceSetupPages.cpp:134](src/gui/setup/AppearanceSetupPages.cpp#L134), TX zero-line color, currently in the same `specGroup` as RX controls.
- `m_txFilterColorBtn` at [AppearanceSetupPages.cpp:155](src/gui/setup/AppearanceSetupPages.cpp#L155), TX passband color, currently in the same `specGroup` as RX controls.
- `m_txDisplayOffsetSpin` at [hardware/CalibrationTab.cpp:325-335](src/gui/setup/hardware/CalibrationTab.cpp#L325), TX Display Offset; **wired in UI** but **not yet applied** to the TX render path. Phase 6 closes the apply gap.
- RX FFT slider widget at [DisplaySetupPages.cpp:438+](src/gui/setup/DisplaySetupPages.cpp#L438), 0-6 slider with bin-width readout. Refactor into reusable component for Phase 3 reuse.
- `SpectrumWidget` has the RX-side waterfall colormap members `m_wfLowThreshold`, `m_wfHighThreshold`, `m_wfBlackLevel`, `m_wfColorGain`, `m_wfColorScheme`. Phase 1 adds parallel `m_txWfLowThreshold`, `m_txWfHighThreshold`, `m_txWfPalette`, `m_txWfLowColor`, `m_txWfGradient`. The existing WfBlackLevel and WfColorGain are MOX-gated to NOT apply during TX (TX uses static thresholds per Thetis).

---

## Phase plan summary

| Phase | Scope | Hours | New files | Modified files |
|---|---|---|---|---|
| **0a** Mockup + master plan | This doc + HTML mockup | done | 2 | 0 |
| **0b** Cleanup squash | Drop 3 throwaway probes (gen-state, attempt-2-frame, wf-histogram). Keep real fixes (stash apply, params + bridge, BH4 swap, Clarity-during-TX gate). | 1-2 | 0 | -3 commits net |
| **1** TX Waterfall Colormap | 5 controls + Setup tab structure + per-frame MOX branch. **The "still hot" fix.** **DONE 2026-05-09 (5e1cdb7..2ca79db; bench passed).** | 6-8 | 0 | ~7 |
| **2** Custom Gradient Picker | Reusable multi-stop gradient editor widget. Used by Phase 1 (Custom palette) and Phase 5 (panadapter/waterfall gradients). **DONE 2026-05-10 (c996c2f..f7b5c7e; bench passed; tst_gradient_picker 12/12 green).** | 8-12 | 2-3 | ~4 |
| **3** TX FFT + Detector + Averaging | 9 controls. Reuse RX FFT slider + window combo + detector combo + averaging combo. Wire `TxAnalyzer::applySetAnalyzer` from settings instead of hardcoded. | 6-8 | 0-1 | ~5 |
| **4** TX Grid Scale | 6 controls. New TX-specific grid renderer state in `SpectrumWidget`. Per-frame MOX branch in grid-draw path. | 4-6 | 0 | ~4 |
| **5** TX Appearance colors | 13+ controls + 2 toggles + 1 line-width slider. **Migrate** existing TX color controls (`m_txZeroLineColorBtn`, `m_txFilterColorBtn`) into a dedicated "TX Display" sub-tab in Appearance. **Audit-before-add** for any duplicate. | 6-9 | 1 | ~5 |
| **6** TX Display Cal Offset | Wire existing `m_txDisplayOffsetSpin` to actually apply in the TX render path. | 1-2 | 0 | ~3 |

**Total: 32-47 hours.** Multi-week feature. Each phase gets its own commit chain on `claude/tx-display`.

---

## Phase 0b: Cleanup squash (do this BEFORE Phase 1 starts)

**Files:** none new; this is a git history operation.

**Goal:** Drop the 3 throwaway probes from the branch, leaving a clean foundation of (a) docs commits and (b) real-fix commits. The real fixes (stash apply, params + bridge, BH4 swap, Clarity-during-TX gate) are KEPT because they are part of the TX panadapter feature and will land with the eventual PR.

**Branch state today:**

```
e931aee debug(tx-display): disable Clarity during TX                    [keep, real fix]
87540f3 debug(tx-display): add waterfall histogram probe                [DROP, throwaway]
deb4f13 debug(tx-display): swap window back to BH4                      [keep, real fix]
7387ad3 debug(tx-display): add attempt-2 diagnostic probe               [DROP, throwaway]
e2b36d2 debug(tx-display): swap to Thetis initAnalyzer params + bridge  [keep, real fix]
b93b487 debug(tx-display): re-apply attempt 1 stash on rebased tree     [keep, real fix]
59ff5a1 docs(tx-display): attempt-2 implementation plan                  [keep, historical]
489e6c5 docs(tx-display): attempt-2 strict Thetis-parity design          [keep, historical]
c2e62e6 debug(tx): throwaway gen0/gen1 state probe                       [DROP, throwaway]
4eef91e Merge pull request #226                                          [origin/main tip]
```

Steps:

- [ ] **Step 0b.1: Capture current branch SHA for safety.**

```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display rev-parse HEAD > /tmp/tx-display-pre-cleanup.sha
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display tag tx-display-pre-cleanup
```

- [ ] **Step 0b.2: Interactive rebase, drop the 3 throwaway commits.**

```bash
GIT_SEQUENCE_EDITOR='sed -i.bak -e "/c2e62e6/d" -e "/7387ad3/d" -e "/87540f3/d"' \
  git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display rebase -S -i 4eef91e
```

(Or open the rebase editor manually and `drop` the three lines if a non-destructive interactive flow is preferred.)

- [ ] **Step 0b.3: Verify the throwaway probe code is gone from the working tree.**

```bash
grep -n "TXDIAG-attempt2-cfg\|TXDIAG-attempt2-frame\|TXDIAG-attempt2-wf\|TXDIAG-gen\|diagLogGenState\|kProbeMaxFrames\|m_moxRiseSeq\|m_probeFrameCount\|resetMoxRiseProbe" \
  /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display/src/core/TxAnalyzer.cpp \
  /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display/src/core/TxChannel.cpp \
  /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display/src/core/TxChannel.h \
  /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display/src/core/TwoToneController.cpp \
  /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display/src/gui/SpectrumWidget.cpp 2>&1 | head -10
```

Expected: empty output. Probes are gone.

- [ ] **Step 0b.4: Build clean.**

```bash
cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display && cmake --build build --parallel --target NereusSDR 2>&1 | tail -10
```

Expected: build succeeds.

- [ ] **Step 0b.5: Run full ctest.**

```bash
cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display && ctest --test-dir build --output-on-failure 2>&1 | tail -20
```

Expected: 100% pass.

- [ ] **Step 0b.6: Verify branch state.**

```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display log --oneline -10
```

Expected: 6 commits between `4eef91e` (origin/main) and the new HEAD: docs/design, docs/plan, stash apply, params + bridge, BH4 swap, Clarity-during-TX gate. No throwaway probe commits.

---

## Phase 1: TX Waterfall Colormap (the "still hot" fix)

**Goal:** Setup → Display → TX gains the full 5-group Thetis layout (FFT, Panadapter, Waterfall, Waterfall Amplitude Scale, TX Grid Scale). All groups get their group boxes; only the **Waterfall Amplitude Scale** group is wired to functional controls in this phase. The other 4 groups get placeholder UI that becomes functional in Phases 3-4. Per-frame MOX branch lands in `SpectrumWidget::pushWaterfallRow` and the colormap function `dbmToRgb`.

**Files:**
- Create:
  - `tests/setup/tst_tx_waterfall_colormap.cpp` (TDD red-then-green for the 4 acceptance behaviors that are testable without a live MOX state machine)
- Modify:
  - `tests/setup/CMakeLists.txt` (or wherever new test executables are registered)
  - `src/gui/SpectrumWidget.h` (add `m_txWf*` members + setters + signals; possibly mark `dbmToRgb` testable or expose a thin test seam)
  - `src/gui/SpectrumWidget.cpp` (per-frame MOX branch in `pushWaterfallRow` + `dbmToRgb`; settings load/save)
  - `src/gui/setup/DisplaySetupPages.h` (`TxDisplayPage` private member additions)
  - `src/gui/setup/DisplaySetupPages.cpp` (`TxDisplayPage::buildUI` rewrite, settings wiring)
  - `src/gui/MainWindow.cpp` (MOX-rise lambda no longer needs to disable Clarity for TX as the per-frame branch supersedes it, but keep the Clarity-gate as belt-and-suspenders for safety)

### Acceptance criteria

- [ ] Setup → Display → TX shows the 5 group boxes per the mockup.
- [ ] The Waterfall Amplitude Scale group has 4 working controls (Low Level, High Level, Palette, Low Color) plus a placeholder for Custom Gradient (visible only when Palette = Custom; placeholder text "(custom gradient editor lands in 3M-5c)").
- [ ] Defaults match Thetis verbatim: Low = -70 dBm, High = +30 dBm, Palette = Enhanced, Low Color = #000000FF.
- [ ] Settings persist across app restart via `AppSettings`.
- [ ] During TX (MOX active), the waterfall renders using the TX values. AGC + Clarity are bypassed for the colormap; thresholds are static.
- [ ] During RX (MOX off), the existing AGC + Clarity behavior is preserved (no regression).
- [ ] `tst_tx_waterfall_colormap` passes 4/4 cases (defaults_match_thetis, settings_round_trip, mox_gate_thresholds, mox_gate_palette). Per `feedback_minimize_test_invocations.md`, this test runs 2x: once before impl (red) and once after impl (green); no full ctest suite during 3M-5b.
- [ ] Bench: TUN + 2-tone visually clean. Background dark (not warm sea). Tones clearly visible. User confirms.

### Tasks

Each code-modifying step uses **READ → SHOW → TRANSLATE** per `feedback_subagent_thetis_source_first.md`. Subagents that cannot locate a Thetis source for a value/range/default/behavior STOP and report; they do not invent, infer, or paraphrase.

#### Step 1.1: Write `tst_tx_waterfall_colormap` (TDD red, tests-first)

**READ first:**
- `Display.cs:1911-1937 [v2.10.3.13+501e3f51]`: `TXWFAmpMin` / `TXWFAmpMax` C# property getter/setter pair (defaults are the integers in the field initializer).
- `Display.cs:2516-2521 [v2.10.3.13+501e3f51]`: `waterfall_low_color_tx` field default initializer (Black).
- `display.cs:6506-6595 [v2.10.3.13+501e3f51]`: the per-frame MOX-conditional render path; the test verifies our NereusSDR mirror of this branch.

**SHOW (in the test source's header comment):** quote each Thetis default verbatim with cite.

**TRANSLATE:** create `tests/setup/tst_tx_waterfall_colormap.cpp` with these four `QTest` slots:

```cpp
private slots:
    void defaults_match_thetis();
    void settings_round_trip();
    void mox_gate_thresholds();
    void mox_gate_palette();
```

Test bodies:

```cpp
// From Thetis Display.cs:1911-1937 + :428-437 + setup.cs:33627 (handler) + setup.cs:223 (default text)
// [v2.10.3.13+501e3f51].  These four defaults are the Thetis-verbatim
// values that 3M-5b ports.
void TestTxWaterfallColormap::defaults_match_thetis()
{
    SpectrumWidget w(0);  // m_panIndex=0, no AppSettings populated
    QCOMPARE(w.txWfLowLevel(), -70);
    QCOMPARE(w.txWfHighLevel(), 30);
    QCOMPARE(static_cast<int>(w.txWfPalette()),
             static_cast<int>(WfColorScheme::Enhanced));
    QCOMPARE(w.txWfLowColor(), QColor(Qt::black));
}

void TestTxWaterfallColormap::settings_round_trip()
{
    SpectrumWidget w(0);
    w.setTxWfLowLevel(-100);
    w.setTxWfHighLevel(50);
    w.setTxWfPalette(WfColorScheme::ClarityBlue);
    w.setTxWfLowColor(QColor("#FF112233"));
    // Force a save+load cycle (saveSettings()/loadSettings()).
    w.saveSettingsForTest();
    SpectrumWidget w2(0);
    w2.loadSettingsForTest();
    QCOMPARE(w2.txWfLowLevel(), -100);
    QCOMPARE(w2.txWfHighLevel(), 50);
    QCOMPARE(static_cast<int>(w2.txWfPalette()),
             static_cast<int>(WfColorScheme::ClarityBlue));
    QCOMPARE(w2.txWfLowColor(), QColor("#FF112233"));
}

void TestTxWaterfallColormap::mox_gate_thresholds()
{
    SpectrumWidget w(0);
    w.setTxWfLowLevel(-100);
    w.setTxWfHighLevel(20);
    // RX-side thresholds set to obviously different values so we can tell
    // which path dbmToRgb chose.
    w.setWfLowThresholdForTest(-200);
    w.setWfHighThresholdForTest(-50);
    // MOX off: dbmToRgb should use RX thresholds.
    w.setMoxOverlay(false);
    const QRgb rxColor = w.dbmToRgbForTest(-100);  // should hit the RX colormap
    // MOX on: dbmToRgb should use TX thresholds.
    w.setMoxOverlay(true);
    const QRgb txColor = w.dbmToRgbForTest(-100);  // should hit the TX colormap
    QVERIFY(rxColor != txColor);  // different mappings produce different colors
}

void TestTxWaterfallColormap::mox_gate_palette()
{
    SpectrumWidget w(0);
    w.setWfColorScheme(WfColorScheme::Default);
    w.setTxWfPalette(WfColorScheme::ClarityBlue);
    w.setMoxOverlay(false);
    const QRgb rxColor = w.dbmToRgbForTest(-90);  // Default palette
    w.setMoxOverlay(true);
    const QRgb txColor = w.dbmToRgbForTest(-90);  // ClarityBlue palette
    QVERIFY(rxColor != txColor);
}
```

If `dbmToRgb`, `saveSettings`, `loadSettings`, `m_wfLowThreshold` writes are private and there's no existing test seam, add minimal test-only accessors guarded by `#ifdef NEREUS_BUILD_TESTS` (the existing test-build flag NereusSDR uses) named `*ForTest`. Do NOT widen the public API.

Register the test executable in `tests/setup/CMakeLists.txt` following the existing `tst_setup_helpers` / `tst_clarity_defaults` pattern.

#### Step 1.2: Run failing tests (TDD red)

```bash
cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display && \
cmake --build build --parallel --target tst_tx_waterfall_colormap && \
ctest --test-dir build -R tst_tx_waterfall_colormap --output-on-failure 2>&1 | tail -20
```

Expected: 0/4 pass (the four behaviors don't exist yet). If the test fails to BUILD (compile errors on `setTxWfLowLevel` etc.), that's also a valid red-state: the symbols are defined in subsequent steps.

#### Step 1.3: Add AppSettings keys with Thetis-verbatim defaults

**READ first:**
- `Display.cs:1911-1937 [v2.10.3.13+501e3f51]`: `TXWFAmpMin` / `TXWFAmpMax` property getter+setter pair.
- `Display.cs:2516-2521 [v2.10.3.13+501e3f51]`: `waterfall_low_color_tx` field default initializer (`Color.Black`).
- `setup.cs:33627 (handler) + setup.cs:223 (default text) [v2.10.3.13+501e3f51]`: `comboColorPalette_tx_SelectedIndexChanged` handler that writes `_tx_color_scheme` (default Enhanced).

**SHOW (in commit message):** quote each cited C# block (~10 lines total) so the cite trail survives the port.

**TRANSLATE:** in `SpectrumWidget::loadSettings()`, add:

```cpp
// From Thetis Display.cs:1911-1937 [v2.10.3.13+501e3f51] — TXWFAmpMin / TXWFAmpMax defaults.
m_txWfLowLevel  = readInt(QStringLiteral("DisplayTxWfLowLevel"), -70);
m_txWfHighLevel = readInt(QStringLiteral("DisplayTxWfHighLevel"), 30);
// From Thetis setup.cs:33627 (handler) + setup.cs:223 (default text) [v2.10.3.13+501e3f51] — comboColorPalette_tx default.
m_txWfPalette = static_cast<WfColorScheme>(qBound(0,
    s.value(settingsKey(QStringLiteral("DisplayTxWfPalette"), m_panIndex),
            QString::number(static_cast<int>(WfColorScheme::Enhanced))).toInt(),
    static_cast<int>(WfColorScheme::Count) - 1));
// From Thetis Display.cs:2516-2521 [v2.10.3.13+501e3f51] — waterfall_low_color_tx default Black.
m_txWfLowColor = QColor::fromString(s.value(
    settingsKey(QStringLiteral("DisplayTxWfLowColor"), m_panIndex),
    QStringLiteral("#FF000000")).toString());
// Custom gradient (3M-5c placeholder; default = same as RX gradient default).
m_txWfGradient = s.value(settingsKey(QStringLiteral("DisplayTxWfGradient"), m_panIndex),
                         QString()).toString();
```

Mirror writes in `saveSettings`.

#### Step 1.4: Add `m_txWf*` private members to `SpectrumWidget.h`

**READ first:**
- `display.cs:6506-6595 [v2.10.3.13+501e3f51]`: the per-frame MOX-conditional render path that consumes these members.

**SHOW (in commit message):** the architecture statement: NereusSDR mirrors Thetis's render path exactly. No state machine, no save/restore on MOX edge, render path reads `m_txWf*` when isTx else `m_wf*`.

**TRANSLATE:**

```cpp
// 3M-5b (TX Waterfall Colormap): TX-specific colormap settings.
// Active during MOX per Thetis display.cs:6506-6595 [v2.10.3.13+501e3f51]
// inline per-frame branch.  No state machine.
int           m_txWfLowLevel{-70};       // dBm, from Thetis udTXWFAmpMin default
int           m_txWfHighLevel{30};       // dBm, from Thetis udTXWFAmpMax default
WfColorScheme m_txWfPalette{WfColorScheme::Enhanced};
QColor        m_txWfLowColor{Qt::black}; // Thetis waterfall_low_color_tx default
QString       m_txWfGradient;            // encoded gradient string for Custom palette
```

#### Step 1.5: Add public setters + signals to `SpectrumWidget.h`

**READ first:** the existing `setWfLowThreshold` / `setWfHighThreshold` / `setWfColorScheme` setters in `SpectrumWidget.cpp:1855-1885`: match their idempotency-guard, `scheduleSettingsSave()`, `update()`, and signal-emit shape.

**TRANSLATE:**

```cpp
public:
    int  txWfLowLevel() const noexcept  { return m_txWfLowLevel;  }
    int  txWfHighLevel() const noexcept { return m_txWfHighLevel; }
    WfColorScheme txWfPalette() const noexcept { return m_txWfPalette; }
    QColor txWfLowColor() const noexcept { return m_txWfLowColor; }
    QString txWfGradient() const noexcept { return m_txWfGradient; }

    void setTxWfLowLevel(int dbm);
    void setTxWfHighLevel(int dbm);
    void setTxWfPalette(WfColorScheme s);
    void setTxWfLowColor(const QColor& c);
    void setTxWfGradient(const QString& encoded);

signals:
    void txWfSettingsChanged();
```

#### Step 1.6: Implement setters in `SpectrumWidget.cpp`

Each setter follows the existing pattern: idempotency guard via `qFuzzyCompare` / `==`, then assign + `scheduleSettingsSave()` + `update()` + emit `txWfSettingsChanged()`. ~25-40 lines total.

#### Step 1.7: Add the per-frame MOX branch to `dbmToRgb`

**READ first:**
- `display.cs:6506-6595 [v2.10.3.13+501e3f51]`: verbatim per-frame MOX-conditional. Critical lines: `if (local_mox) { use TX values } else { use RX values }`. Branch is inside the per-pixel rendering loop (no edge handler).

**SHOW (in commit message):** quote the `display.cs:6506-6595` block (~30 lines of C#) so the inline-branch architecture is captured for future readers.

**TRANSLATE:**

```cpp
QRgb SpectrumWidget::dbmToRgb(float dbm) const
{
    // From Thetis display.cs:6506-6595 [v2.10.3.13+501e3f51] — per-frame
    // MOX-conditional render path.  No state machine; branch is inline
    // per pixel.
    const bool isTx = m_moxOverlay;  // m_moxOverlay is set by setMoxOverlay
    const float lowThreshold  = isTx ? static_cast<float>(m_txWfLowLevel)
                                     : m_wfLowThreshold;
    const float highThreshold = isTx ? static_cast<float>(m_txWfHighLevel)
                                     : m_wfHighThreshold;
    const WfColorScheme scheme = isTx ? m_txWfPalette : m_wfColorScheme;
    // For TX, black-level / color-gain sliders are NOT applied (Thetis
    // doesn't expose them per-direction); use raw thresholds.  For RX
    // the existing black-level / color-gain math stays.
    float effectiveLow, effectiveHigh;
    if (isTx) {
        effectiveLow  = lowThreshold;
        effectiveHigh = highThreshold;
    } else {
        effectiveLow  = lowThreshold + (125.0f - static_cast<float>(m_wfBlackLevel)) * 0.4f;
        effectiveHigh = highThreshold - static_cast<float>(m_wfColorGain) * 0.3f;
    }
    if (effectiveHigh <= effectiveLow) {
        effectiveHigh = effectiveLow + 1.0f;
    }
    // ...rest of dbmToRgb (gradient stop interpolation) unchanged...
}
```

#### Step 1.8: MOX-gate the AGC + Clarity threshold writes in `pushWaterfallRow`

**READ first:**
- `display.cs:6506-6595 [v2.10.3.13+501e3f51]` (re-read): Thetis does NOT run an AGC during MOX; thresholds are static from `TXWFAmpMin` / `TXWFAmpMax`. Our existing AGC + Clarity must skip when isTx so they don't write to `m_wfLowThreshold` / `m_wfHighThreshold` mid-MOX.
- `feedback_clarity_addon_not_replacement.md`: Clarity is an add-on, not a supersession; gating it during TX preserves the RX behavior unchanged.

**TRANSLATE:** wrap the existing AGC and NF-AGC blocks with `if (!isTx) { ... }`:

```cpp
// 3M-5b: AGC + NF-AGC are RX-only.  TX uses static thresholds from
// m_txWfLowLevel / m_txWfHighLevel set in Setup → Display → TX.
const bool isTx = m_moxOverlay;
if (!isTx && m_wfAgcEnabled && !m_clarityActive) {
    // ...existing AGC code (unchanged inside)...
}
if (!isTx && m_wfNfAgcEnabled && !m_clarityActive) {
    // ...existing NF-AGC code (unchanged inside)...
}
// Note: the existing setClarityActive(false) call in the MainWindow MOX-rise
// lambda becomes belt-and-suspenders; the per-frame `!isTx` gate is the
// primary mechanism per Thetis display.cs:6506-6595 [v2.10.3.13+501e3f51].
```

#### Step 1.9: Run tests passing (TDD green; 2x ctest invocation total)

```bash
cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display && \
cmake --build build --parallel --target tst_tx_waterfall_colormap && \
ctest --test-dir build -R tst_tx_waterfall_colormap --output-on-failure 2>&1 | tail -20
```

Expected: 4/4 pass. If any case fails, the impl deviates from the Thetis-verbatim defaults or the per-frame branch is wired wrong.

#### Step 1.10: Build TxDisplayPage UI per the mockup

**READ first:**
- `setup.designer.cs:36246-36379 [v2.10.3.13+501e3f51]`: `grpTXWFAmpScale` group box layout (the "Waterfall" amp-scale group containing Low / High / Palette / Low Color).
- `setup.designer.cs:36278 [v2.10.3.13+501e3f51]`: `udTXWFAmpMin` spinbox specifics (Min/Max/Increment/Value defaults for tooltip + control range).
- Existing NereusSDR `TxDisplayPage::buildUI` at `DisplaySetupPages.cpp:2240-2278`: the 4-stub buildUI that 3M-5b replaces.

**SHOW (in commit message):** quote the C# designer block for `grpTXWFAmpScale` so the layout port is traceable.

**TRANSLATE:** replace the current 4-stub buildUI with the 5-group layout from the mockup. For 3M-5b, only the Waterfall Amplitude Scale group is functional; the other 4 groups have placeholder labels saying which sub-phase wires them (3M-5d / 3M-5e).

```cpp
void TxDisplayPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // Group 1: Fast Fourier Transform (3M-5d)
    auto* fftGroup = makePlaceholderGroup(QStringLiteral("Fast Fourier Transform"),
        QStringLiteral("FFT Size + Window controls (lands in 3M-5d)"));
    contentLayout()->addWidget(fftGroup);

    // Group 2: Panadapter (3M-5d)
    auto* panGroup = makePlaceholderGroup(QStringLiteral("Panadapter"),
        QStringLiteral("Detector + Averaging + Time + Normalize (lands in 3M-5d)"));
    contentLayout()->addWidget(panGroup);

    // Group 3: Waterfall (3M-5d)
    auto* wfFftGroup = makePlaceholderGroup(QStringLiteral("Waterfall"),
        QStringLiteral("Detector + Averaging + Time (lands in 3M-5d)"));
    contentLayout()->addWidget(wfFftGroup);

    // Group 4: Waterfall Amplitude Scale (3M-5b: THE FIX)
    auto* ampGroup = new QGroupBox(QStringLiteral("Waterfall Amplitude Scale"), this);
    auto* ampForm  = new QFormLayout(ampGroup);
    ampForm->setSpacing(6);

    // From Thetis setup.designer.cs:36278 [v2.10.3.13+501e3f51] — udTXWFAmpMin.
    m_txWfLowLevelSpin = new QSpinBox(ampGroup);
    m_txWfLowLevelSpin->setRange(-200, 200);
    m_txWfLowLevelSpin->setSingleStep(5);
    m_txWfLowLevelSpin->setSuffix(QStringLiteral(" dBm"));
    m_txWfLowLevelSpin->setToolTip(
        QStringLiteral("Waterfall palette \"Low Color\" maps to bins at or below "
                       "this level. Default -70 dBm. From Thetis udTXWFAmpMin "
                       "[setup.designer.cs:36278 v2.10.3.13+501e3f51]."));
    ampForm->addRow(QStringLiteral("Low Level:"), m_txWfLowLevelSpin);

    // High Level spinbox (analogous, default +30, range, tooltip)
    // Palette combo (analogous, items match Thetis enum, tooltip)
    // Low Color picker (ColorSwatchButton, default black, tooltip)
    // Custom Gradient placeholder (3M-5c)

    contentLayout()->addWidget(ampGroup);

    // Group 5: TX Grid Scale (3M-5e)
    auto* gridGroup = makePlaceholderGroup(QStringLiteral("TX Grid Scale"),
        QStringLiteral("Max + Min + Step + Display Grid + Fill + Label Align (lands in 3M-5e)"));
    contentLayout()->addWidget(gridGroup);

    contentLayout()->addStretch();
}
```

#### Step 1.11: Wire the 4 functional controls to SpectrumWidget setters

**READ first:** the existing Setup-page wiring patterns in `DisplaySetupPages.cpp` for RX waterfall thresholds (search for `setWfHighThreshold` / `setWfLowThreshold` connect blocks): match their signal-blocker, valueChanged, settingChanged shape.

**TRANSLATE:** standard Setup-page connect pattern for each of Low Level, High Level, Palette, Low Color.

#### Step 1.12: Build clean

```bash
cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display && \
cmake --build build --parallel --target NereusSDR 2>&1 | tail -10
```

Expected: clean build, zero warnings on the new code paths.

#### Step 1.13: Manual bench (TUN + 2-tone)

Verify defaults give a clean waterfall on the live radio. If defaults need tweaking (e.g., user moves Low Level to -90 instead of -70 to get the visual they want), that's user tuning; defaults stay at Thetis verbatim values.

#### Step 1.14: Commit (GPG-signed)

Single commit titled "feat(setup,display): TX waterfall amplitude scale (3M-5b)". Body explains:
- Per-frame MOX branch matching `display.cs:6506-6595 [v2.10.3.13+501e3f51]`.
- 5 controls + Setup tab structure with placeholder groups for 3M-5d / 3M-5e.
- Defaults verbatim from Thetis (Low -70, High +30, Palette Enhanced, Low Color #000000FF).
- AGC + NF-AGC + Clarity all MOX-gated to RX-only paths.
- `tst_tx_waterfall_colormap` 4/4 green.
- Acceptance criteria met.

---

## Phase 2: Custom Gradient Picker widget

**Goal:** Port Thetis `ucLGPicker` (1009-line `UserControl` from `ucLGPicker.cs`) to a reusable Qt widget `GradientPickerWidget`. Phase 1 (TX Waterfall Custom palette) and Phase 5 (TX Appearance panadapter/waterfall gradients) both consume it. Eventual RX-side migration is tracked separately.

**Source-first context (Thetis v2.10.3.13+501e3f51):**

The Thetis widget is `Thetis.ucLGPicker : UserControl` at `ucLGPicker.cs:55-955`. Four instances at `setup.designer.cs:3283-3318`:
- `lgLinearGradientTX_waterfall` (TX waterfall)
- `lgLinearGradientTX` (TX panadapter spectrum gradient)
- `lgLinearGradientRX1` (RX1 panadapter spectrum gradient)
- `lgLinearGradient_waterfall` (RX waterfall)

Defaults (8 grayscale grippers): `ucLGPicker.cs:98-108` linear ramp `i*(256/8-1)` for i in 0..7 + final `(255,255,255)`.
Internal dBm range: `LOW=150`, `HIGH=10`, `SPAN=160` ⇒ widget covers `-150..+10` dBm. `ucLGPicker.cs:57-59`.
Stop struct: `GradColours { float percent; Color color; bool enabled; bool highlighted }` at `ucLGPicker.cs:75-80`.
Storage: `Dictionary<int, GradColours> m_dictColours` at `ucLGPicker.cs:83`. `m_nGrippers = 8` total; min 2 enabled.

Public API (verified in `ucLGPicker.cs`):
- `Low / High / ShowAsPercent / IncludeAlphaInPreview / ColourForSelectedGripper`
- `Text` (raw pipe-delimited) at `:666-738`
- `EncodedText` (base64-of-UTF-8-Text) at `:912-930`
- `GetColourForDBM(float)` at `:764-769`, `GetColourAtPercent(float)` at `:770-920`
- `HighlightFirstGripper()`, `RemoveSelectedGripper(bool)`
- Events: `Changed`, `GripperSelected`, `GripperDBMChanged`, `GripperMouseEnter`, `GripperMouseLeave`

Consumers (waterfall LUT build) at `setup.cs:33304-33323`: builds `Color[101]` by sampling `GetColourAtPercent(perc / 100f)` per integer percent, broadcasts via `WaterfallRXGradientChangedHandlers` / `WaterfallTXGradientChangedHandlers`.

**Encoded format (CRITICAL — must match byte-for-byte for skin import):**

Raw `Text` format from `ucLGPicker.cs:666-682`:
```
<count>|<en0>|<perc0>|<argb0>|<en1>|<perc1>|<argb1>|...|
```
- `count` = int (decimal)
- `enabled` = `"0"` or `"1"`
- `percent` = `"0.000"` format (3 decimals)
- `argb` = signed int32 from `Color.ToArgb()` as decimal string
- Trailing pipe present

`EncodedText` = `Convert.ToBase64String(Encoding.UTF8.GetBytes(Text))` per `ucLGPicker.cs:912-930`.

`Text` setter (`ucLGPicker.cs:684-738`) defensively no-ops on malformed input — partial parse failure leaves `m_dictColours` untouched.

**Files:**
- Create: `src/gui/widgets/GradientPickerWidget.h` (new — Q_OBJECT, public API)
- Create: `src/gui/widgets/GradientPickerWidget.cpp` (new — paint, mouse, serialization, color interpolation)
- Create: `src/gui/widgets/GradientStop.h` (new — small POD struct: `float percent; QColor color; bool enabled`)
- Create: `tests/tst_gradient_picker.cpp` (new — TDD tests)
- Modify: `tests/CMakeLists.txt` (add tst_gradient_picker)
- Modify: `src/CMakeLists.txt` (add new sources)
- Modify: `src/gui/setup/DisplaySetupPages.cpp` (replace TxDisplayPage Custom Gradient placeholder with real widget)
- Modify: `src/gui/SpectrumWidget.{h,cpp}` (consume 101-entry LUT when palette = Custom; rebuild on changed signal)

**Design (NereusSDR-architectural):**
- The widget itself is a Qt UI control — UI is NereusSDR-native (per `feedback_source_first_ui_vs_dsp.md`). API freedom granted.
- Encoded format is **strict 1:1 with Thetis** (skin-import compat in 3H).
- Default stop count + default colors **strict 1:1 with Thetis** (so brand-new NereusSDR users get the same Custom-palette starting point).

**Acceptance criteria:**

- [ ] `GradientPickerWidget` constructs with 8 grayscale stops matching Thetis `ucLGPicker.cs:106-108`.
- [ ] `text()` getter produces byte-for-byte the format from Thetis `ucLGPicker.cs:666-683`.
- [ ] `setText()` round-trips Thetis-emitted text without data loss.
- [ ] `encodedText()` round-trips through base64 per Thetis `ucLGPicker.cs:912-930`.
- [ ] `colorAtPercent(p)` linear-interpolates ARGB between adjacent enabled stops; clamps at endpoints.
- [ ] `colorTable(101)` returns the same 101-entry array Thetis `setup.cs:33304-33312` builds.
- [ ] Add stop on empty strip, drag stop, double-click → color picker, right-click → delete (min 2 stops).
- [ ] `changed()` signal fires on every mutation.
- [ ] TxDisplayPage Custom Gradient placeholder is replaced by a real widget; settings round-trip.
- [ ] SpectrumWidget consumes the 101-LUT when `m_txWfPalette == Custom` during MOX.
- [ ] `tst_gradient_picker` 10/10 green.
- [ ] No port-check verifier failures.

### Tasks

#### Step 2.1: Write `tst_gradient_picker` (TDD red, tests-first)

**READ:**
- Thetis `ucLGPicker.cs:55-130` — class scaffolding, constants, default constructor.
- Thetis `ucLGPicker.cs:75-80` — `GradColours` struct.
- Thetis `ucLGPicker.cs:98-108` — default 8-stop grayscale ramp.
- Thetis `ucLGPicker.cs:666-738` — `Text` getter + setter, both formats and the malformed-input no-op.
- Thetis `ucLGPicker.cs:770-920` — `GetColourAtPercent` interpolation logic.
- Thetis `ucLGPicker.cs:912-930` — `EncodedText` base64.
- Thetis `setup.cs:33304-33312` — 101-entry LUT build pattern.

**SHOW:** quote each block at top of test file under `// From Thetis ucLGPicker.cs:NNN-MMM [v2.10.3.13+501e3f51]`.

**TRANSLATE:** TDD tests first (no implementation yet). 10 cases:

1. `default_constructor_has_8_grayscale_stops` — verify count == 9 entries (8 grippers indices 0..7 + the final addColour at index 8 with white per `:108`), enabled count == 9, colors match `Color.FromArgb(255, i*(256/8-1), i*(256/8-1), i*(256/8-1))` for i 0..7 and `(255,255,255,255)` for i=8.
2. `text_format_matches_thetis_byte_for_byte` — call `text()`, compare to a Thetis-emitted reference string captured in a fixture file or hex-encoded literal.
3. `text_round_trip_via_setText` — set text → get text == identity.
4. `encoded_text_round_trip` — `setEncodedText(s); encodedText() == s`.
5. `empty_string_to_setText_is_noop` — state unchanged.
6. `malformed_text_to_setText_is_noop` — verify `m_dictColours` (or our equivalent) is untouched, count and stops unchanged.
7. `add_stop_at_percent_inserts_with_interpolated_color` — `addStop(0.5)` returns an index, color at that stop matches what `colorAtPercent(0.5)` returned BEFORE the add (`ucLGPicker.cs:415` — Thetis grabs `GetColourAtPercent(midWay)` for new stops).
8. `remove_stop_enforces_min_2` — removing stops down to 2 enabled, third remove is a no-op.
9. `color_at_percent_clamps_to_endpoints` — perc < first stop → first stop color; perc > last → last.
10. `color_table_101_matches_thetis_LUT_build` — `colorTable(101)[k] == colorAtPercent(k/100.0f)` for k in 0..100.

Header gets `// no-port-check:` marker (NereusSDR-original test, cites Thetis values for verification not as a port).

#### Step 2.2: Run failing tests (TDD red)

```bash
cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display
cmake --build build --parallel --target tst_gradient_picker 2>&1 | tail -5
ctest --test-dir build -R tst_gradient_picker --output-on-failure 2>&1 | tail -10
```

Expected: 10/10 fail (no impl yet).

#### Step 2.3: Add `GradientStop.h` POD struct

**READ:** Thetis `ucLGPicker.cs:75-80` (`GradColours` struct).

**TRANSLATE:**
```cpp
struct GradientStop {
    float percent;   // 0.0 .. 1.0
    QColor color;    // ARGB
    bool enabled;    // matches Thetis enabled flag
};
```

No `highlighted` flag in the data model — that's view state, kept inside the widget.

#### Step 2.4: Add `GradientPickerWidget.h` API skeleton

**READ:**
- Thetis `ucLGPicker.cs:118-128` (Low/High/ShowAsPercent properties).
- Thetis `ucLGPicker.cs:552-564` (`ColourForSelectedGripper`).
- Thetis `ucLGPicker.cs:666-738` (`Text`).
- Thetis `ucLGPicker.cs:912-930` (`EncodedText`).
- Thetis `ucLGPicker.cs:86-96` (event delegates).

**TRANSLATE:** Q_OBJECT widget. Public:
- Q_PROPERTY: `low`, `high`, `showAsPercent`, `includeAlphaInPreview`, `colorForSelectedStop`, `text`, `encodedText`.
- Methods: `colorAtPercent(float) const`, `colorForDbm(float) const`, `colorTable(int n=101) const`, `addStop(float)`, `removeSelectedStop()`, `highlightFirstStop()`, `gradientStops() const`.
- Signals: `changed()`, `stopSelected(QColor)`, `stopDbmChanged(int dBm, float percent)`, `stopMouseEnter(int dBm, float percent)`, `stopMouseLeave(int dBm, float percent)`.
- Test seams under `#ifdef NEREUS_BUILD_TESTS`: `setStopsForTest(QVector<GradientStop>)`, `stopsForTest() const`.

#### Step 2.5: Implement encoded-format round-trip

**READ:**
- Thetis `ucLGPicker.cs:666-682` — `Text` getter (forward).
- Thetis `ucLGPicker.cs:684-738` — `Text` setter (with malformed-input no-op).
- Thetis `ucLGPicker.cs:912-930` — `EncodedText` base64.

**TRANSLATE:** Match the format byte-for-byte. Notes:
- Use `QString::number(stop.percent, 'f', 3)` to match `.ToString("0.000")`.
- ARGB int32: `static_cast<qint32>(color.rgba())` (Qt stores ARGB in `QRgb`/`uint`, Thetis uses signed `Color.ToArgb()`; negative values for high alpha — careful to round-trip the sign bit).
- Base64: `QByteArray(text.toUtf8()).toBase64()` and inverse.
- Trailing pipe in serializer; setter must tolerate its presence (Thetis splits on `'|'` which produces an empty trailing element — match that arithmetic).

#### Step 2.6: Implement color interpolation

**READ:**
- Thetis `ucLGPicker.cs:770-920` — `GetColourAtPercent`.
- Thetis `ucLGPicker.cs:970-1008` — `ColorInterpolator.InterpolateBetween` (component-wise byte interp).

**TRANSLATE:** linear interpolate ARGB component-wise between adjacent enabled stops. Edge clamp.

#### Step 2.7: Run tests pass (TDD green; 2x ctest invocation total per `feedback_minimize_test_invocations.md`)

```bash
cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display
cmake --build build --parallel --target tst_gradient_picker 2>&1 | tail -5
ctest --test-dir build -R tst_gradient_picker --output-on-failure 2>&1 | tail -10
```

Expected: 10/10 pass.

#### Step 2.8: Implement painting

**READ:**
- Thetis `ucLGPicker.cs:175-211` — `actualWidth`, `drawTextCentre`, `drawScales`.
- Thetis `ucLGPicker.cs:213-278` — `LGPicker_Paint`.

**TRANSLATE:** Override `paintEvent`. QPainter with QLinearGradient for the strip. Triangular gripper markers below. Highlighted stop drawn distinctively. Scale labels respect `showAsPercent` (0..100% vs `low..high` dBm).

#### Step 2.9: Implement mouse interactions

**READ:**
- Thetis `ucLGPicker.cs:296-362` — `LGPicker_MouseMove`.
- Thetis `ucLGPicker.cs:387-446` — `LGPicker_MouseDown` (LMB on strip = add at midway; LMB on stop = drag).
- Thetis `ucLGPicker.cs:447-468` — `LGPicker_MouseUp`.
- Thetis `ucLGPicker.cs:582-597` — `RemoveSelectedGripper`.

**TRANSLATE:** Override `mousePressEvent` / `mouseMoveEvent` / `mouseReleaseEvent` / `mouseDoubleClickEvent` / `contextMenuEvent`. LMB-on-stop drag + clamp to neighbors. LMB-on-strip-empty add. Double-click → `QColorDialog`. RMB → remove (gated by min-2).

#### Step 2.10: Build clean

```bash
cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display
cmake --build build --parallel --target NereusSDR 2>&1 | tail -10
```

#### Step 2.11: Wire into TxDisplayPage Custom Gradient slot

**READ:** `src/gui/setup/DisplaySetupPages.cpp` Phase-1 placeholder for "Custom Gradient" inside the Waterfall Amplitude Scale group; Phase 1 master plan §1.10 for the placeholder location.

**TRANSLATE:** replace placeholder QLabel with `GradientPickerWidget`. Visibility binds to `m_txWfPaletteCombo->currentIndex() == Custom`. Encoded text persists under `DisplayTxWfGradient` (already reserved in Phase 1 SpectrumWidget settings keys).

#### Step 2.12: Wire SpectrumWidget consumption

**READ:** `src/gui/SpectrumWidget.cpp` `dbmToRgb` — Phase 1 added the per-frame MOX branch; the Custom path currently falls back to defaults.

**TRANSLATE:** When `isTx && m_txWfPalette == Custom`, index into a cached `m_txCustomLut[101]` rebuilt whenever the gradient changes. Connect to the widget's `changed()` signal at MainWindow wiring level (or via a settings-changed re-emit hub).

#### Step 2.13: Manual bench

1. Setup → Display → TX → Palette: Custom.
2. Verify GradientPickerWidget appears (replacing placeholder).
3. Run TUN + 2-tone. Waterfall renders custom gradient.
4. Drag stops, add stops, delete stops, double-click → recolor. Waterfall live-updates.
5. Restart app: gradient persists (encoded text in settings file).
6. Switch palette back to Enhanced → reverts cleanly. Switch back to Custom → restored.
7. Verify RX waterfall is unaffected (still uses RX-side palette logic; Custom gradient not yet wired to RX).

#### Step 2.14: Commit (GPG-signed)

Single commit closing Phase 2. Conventional message:

```
feat(tx-display): port ucLGPicker → GradientPickerWidget (3M-5c)

Reusable multi-stop linear gradient editor widget. Phase 2 of the
3M-5 TX Display Refactor master plan. Lands the Custom palette path
that Phase 1 (3M-5b TX Waterfall Colormap) reserved.

Source-first port from Thetis ucLGPicker.cs [v2.10.3.13+501e3f51]:
- Default 8 grayscale stops (ucLGPicker.cs:98-108)
- Pipe-delimited Text format (ucLGPicker.cs:666-738)
- Base64 EncodedText (ucLGPicker.cs:912-930)
- Color interpolation (ucLGPicker.cs:770-920)
- 101-entry LUT consumer pattern (setup.cs:33304-33312)

UI is NereusSDR-native (Qt). Encoded format is byte-for-byte Thetis
for skin-import compatibility (3H).

Tests: tst_gradient_picker 10/10 green.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
```

---

## Phase 3: TX FFT + Detector + Averaging

**Goal:** Wire 9 TX Display controls (Setup → Display → TX groups 1-3) to TxAnalyzer state so the user can change FFT size, window type, pan/WF detector, pan/WF averaging, pan/WF av-time, and pan normalize during TX and see the spectrum/waterfall respond live. Today these three groups are placeholder text. TxAnalyzer hardcodes FFT=4096, window=BH4, detector=peak, averaging=off; 3M-5d turns them into bound state.

**Source-first context (Thetis v2.10.3.13+501e3f51):**

All 9 controls live in `tpDisplayTransmit`. Designer block at `setup.designer.cs:2200-2235`:

**Group 1 — Fast Fourier Transform (`groupBoxTS8`):**
- `tbTXDisplayFFTSize` TrackBarTS at `:2227`. Handler `setup.cs:18136-18143`: `FFTSize = 4096 * 2^slider`. Also emits bin-width readout = `SampleRate / FFTSize`.
- `comboTXDispWinType` ComboBoxTS at `:2220`. Handler `setup.cs:18145-18150`: `WindowType = combo.SelectedIndex`. Items at `:36555-36562` (source-verified Thetis order: **Rectangular, Blackman-Harris 4T, Hann, Flat-Top, Hamming, Kaiser, Blackman-Harris 7T**; Hamming = index 4). Earlier draft of this spec listed wrong combo labels; corrected at 3M-5d ship time.
- Readouts: `lblTXFFT_size` + `lblTXDispBinWidth` at `:2218 / :2224`.

**Group 2 — Panadapter (`groupBoxTS7`):**
- `comboTXDispPanDetector` ComboBoxTS at `:2233`. Handler `setup.cs:18105-18113`: `DetTypePan = combo.SelectedIndex`. Also: `chkDispTXNormalize.Enabled = (DetTypePan >= 2)`. Author note `[2.10.3.5]MW0LGE` preserved at `:18111`.
- `comboTXDispPanAveraging` ComboBoxTS at `:2231`. Handler `setup.cs:18115-18120`: `AverageMode = combo.SelectedIndex`.
- `udTXDisplayAVGTime` NumericUpDownTS at `:2234`. Handler `setup.cs:18122-18127`: `AvTau = 0.001 * value` (value in milliseconds; tau in seconds).
- `chkDispTXNormalize` CheckBoxTS at `:2229`. Handler `setup.cs:18129-18134`: `NormOneHzPan = checked`.

**Group 3 — Waterfall (`groupBoxTS9`):**
- `comboTXDispWFDetector` ComboBoxTS at `:2214`. Handler `setup.cs:18152-18157`: `DetTypeWF = combo.SelectedIndex`.
- `comboTXDispWFAveraging` ComboBoxTS at `:2212`. Handler `setup.cs:18159-18164`: `AverageModeWF = combo.SelectedIndex`.
- `udTXDisplayAVTime` NumericUpDownTS at `:2215`. Handler `setup.cs:18166-18171`: `AvTauWF = 0.001 * value`.

Each handler calls `console.UpdateTXSpectrumDisplayVars()` (`console.cs:7968-7971`) which forwards to `UpdateTXDisplayVars` (`console.cs:8015+`). That re-runs `specRX.GetSpecRX(...).CalcSpectrum(...)` which pushes the new SetAnalyzer config to WDSP.

All 9 properties live on the `SpecHPSDR` class (`specHPSDR.cs:301-322` for DetType*, `:529 + :534-643` for the SetAnalyzer call, `:134` for the WindowType default = 4 / Hamming, `:335` for FrameRate = 15).

**Current NereusSDR state (`src/core/TxAnalyzer.cpp:170-229`):**
- `m_fftSize{4096}`, `m_outputFps{15}` are bound state. The other 7 properties are hardcoded inside `applySetAnalyzer`:
  - `win_type=1` (Blackman-Harris 4-term) — **NereusSDR-architectural divergence** from Thetis default 4/Hamming. Inline rationale at `TxAnalyzer.cpp:201-208`: Hamming -42 dB sidelobes + AGC colormap interaction = splattery waterfall.
  - `n_pixout=1` (single pixel-out — pan and WF share the same detector + avg mode).
  - `DetTypePan/AverageMode/NumAverage = 0/0/1` (peak / off / 1).
  - `DetTypeWF/AverageModeWF` — not configured (only one pixel-out anyway).
- No settings binding. Defaults baked in at compile time.

**Required architectural change — `n_pixout=2`:**

To make pan vs WF Detector/Averaging independently configurable (Thetis parity), `n_pixout` must bump from 1 to 2. Today's `GetPixels(dispId, 0, pixBuf)` reads pixout-0; bumping requires the WF tap to read `GetPixels(dispId, 1, pixBuf)` as the second pixel output. SpectrumWidget consumes the same FFT data today for both trace and waterfall; this phase splits the read points so detector + avg apply differently to each.

**Default window — reverts to Thetis Hamming (3M-5d controller decision 2026-05-10):**

3M-5b shipped with `TxAnalyzer.cpp:201-208` hardcoded to BH4 (window 1) as a NereusSDR-architectural divergence from Thetis default Hamming (window 4 per `specHPSDR.cs:134`). That divergence rationale ("Hamming sidelobes + auto-AGC = splattery waterfall") predated 3M-5b's avenger-clear-on-MOX + tightened Low/High thresholds. With 3M-5b's colormap fix landed, the controller has elected to revert the divergence and ship Thetis-faithful Hamming as the fresh-install default. If splatter returns on bench, the user can flip the combo to BH4 (or any of 7 windows) without recompile.

Implication: existing 3M-5b users with no `DisplayTxWindowType` key present will see Hamming take effect on next launch after 3M-5d. The implementer must replace the BH4 literal in `TxAnalyzer.cpp:209` with a member read so the settings-driven default propagates correctly.

**Files:**
- Modify: `src/core/TxAnalyzer.{h,cpp}` — add 7 new state members + setters + load-from-settings, bump `n_pixout=2`, propagate pan/WF detector/avg/normalize via WDSP setters.
- Modify: `src/core/TxAnalyzer.h` (signal `pixelsForWaterfallReady` or similar so SpectrumWidget can tap pixout=1).
- Modify: `src/gui/SpectrumWidget.{h,cpp}` — accept TX waterfall pixout=1 frames during MOX (separate path from the existing trace path).
- Modify: `src/gui/setup/DisplaySetupPages.cpp` — replace 3 placeholder groups in `TxDisplayPage::buildUI` (lines around 2241-2360) with 9 functional controls. Mirror RX-side widget patterns (do NOT refactor RX widgets; per `feedback_clarity_addon_not_replacement.md` we add alongside not in-place).
- Create: `tests/tst_tx_analyzer_settings.cpp` — TDD tests for the new state surface + persistence.

**Settings keys (PascalCase, persisted in AppSettings):**

| Key | Type | Default | Thetis source |
|---|---|---|---|
| `DisplayTxFftSize` | int | 4096 | `setup.cs:18138` formula = `4096 * 2^0` |
| `DisplayTxWindowType` | int | **4 (Hamming)** | Thetis default at `specHPSDR.cs:134`. 3M-5d reverts the 3M-5b BH4 divergence (controller decision 2026-05-10). Combo lets user switch to any of 7 windows. |
| `DisplayTxPanDetector` | int | 0 (Peak) | `specHPSDR.cs:301` default |
| `DisplayTxPanAveraging` | int | 0 (Off) | `specHPSDR.cs:312` default |
| `DisplayTxPanAvTimeMs` | int | **30 (= 0.030 s tau)** | Thetis `udTXDisplayAVGTime.Value = 30` at `setup.designer.cs:36753 [v2.10.3.13+501e3f51]` (source-first correction; was 120 in earlier draft) |
| `DisplayTxPanNormalize` | bool | false | `specHPSDR.cs` default |
| `DisplayTxWfDetector` | int | 0 (Peak) | `specHPSDR.cs:301` default |
| `DisplayTxWfAveraging` | int | 0 (Off) | `specHPSDR.cs:312` default |
| `DisplayTxWfAvTimeMs` | int | 120 (= 0.120 s tau) | Thetis `udTXDisplayAVTime.Value = 120` at `setup.designer.cs:36493 [v2.10.3.13+501e3f51]` (source-verified) |

Implementer must source-read the actual Thetis designer defaults for `udTXDisplayAVGTime.Value` and `udTXDisplayAVTime.Value` before committing the AvTimeMs defaults. If Thetis says 100 not 120, match Thetis.

**Acceptance criteria:**

- [ ] All 9 controls render in Setup → Display → TX. FFT trackbar + 2 readouts (FFT size + bin width). 4 combos with correct item lists. 2 spinboxes (AvTimeMs). 1 checkbox (Normalize) with `enabled = (DetTypePan >= 2)` gate per Thetis MW0LGE comment.
- [ ] Each control round-trips through AppSettings (close + relaunch preserves user choice).
- [ ] Changing any control during TX visibly changes the spectrum and/or waterfall live (with the appropriate detector / avg / window behaviour).
- [ ] FFT size change updates bin-width readout = `SampleRate / FFTSize` per `setup.cs:18140`.
- [ ] DetTypePan switch enables/disables Normalize checkbox per `setup.cs:18112`.
- [ ] `n_pixout=2` lets pan and WF use independent detector + averaging.
- [ ] RX side unchanged — no regressions on the RX FFT slider, detector, averaging.
- [ ] `tst_tx_analyzer_settings` green (8+ cases).
- [ ] `tst_tx_waterfall_colormap` + `tst_gradient_picker` still green (no regressions on shipped phases).
- [ ] Default window = Hamming (Thetis-faithful per controller 2026-05-10 decision). User can pick BH4 (or any of 7 windows) via combo if splatter returns.

### Tasks

#### Step 3.1: Write `tst_tx_analyzer_settings` (TDD red)

**READ:**
- Thetis `specHPSDR.cs:301-322` (detector + avg + normalize property setters)
- Thetis `specHPSDR.cs:534-643` (SetAnalyzer call)
- Thetis `setup.cs:18105-18171` (all 9 handlers)
- Thetis `setup.designer.cs:2200-2235` (control declarations)
- NereusSDR `src/core/TxAnalyzer.{h,cpp}` (current state surface)

**TRANSLATE:** 8 test cases. Header has `// no-port-check:` marker.
1. `defaults_match_spec` — TxAnalyzer constructs with FFT=4096, Window=Hamming (4), PanDet=0, PanAvg=0, PanAvTimeMs=<Thetis>, PanNormalize=false, WfDet=0, WfAvg=0, WfAvTimeMs=<Thetis>.
2. `settings_round_trip` — set each of the 9 via setter, save to AppSettings, reload, verify each one survives.
3. `fft_size_formula` — `setFftSizeSliderPosition(0)` ⇒ fftSize=4096; position(1) ⇒ 8192; position(2) ⇒ 16384.
4. `bin_width_formula` — `binWidthHz() == sampleRate / fftSize`.
5. `normalize_gated_on_pan_detector` — `panNormalizeEnabled() == false` when PanDet < 2; `true` when PanDet >= 2.
6. `av_time_ms_to_tau_seconds` — setter takes ms, internal storage / SetAnalyzer call uses seconds (×0.001).
7. `setAnalyzer_called_on_each_setter` — under test seam, each setter increments a call counter on a mock SetAnalyzer / SetDisplayDetectorMode hook.
8. `pixout_2_after_phase` — `nPixout()` reads 2 (used by SpectrumWidget tap point).

#### Step 3.2: Run failing tests (red)

#### Step 3.3: Add 7 new state members + setters to `TxAnalyzer.{h,cpp}`

**READ:** Thetis `specHPSDR.cs:301-322`. NereusSDR `TxAnalyzer.h:128-138`.

**TRANSLATE:** add private members + public setters/getters mirroring the RX-side conventions in `WdspEngine` / `RxChannel`. Setters call `applySetAnalyzer()` (existing entry point) or finer-grained WDSP setters.

#### Step 3.4: Bump `n_pixout=2`; route WF reads to `GetPixels(dispId, 1, ...)`

**READ:** Thetis `specHPSDR.cs:534-643` (SetAnalyzer with `n_pixout=2`), `:301-322` (per-pixout detector setters).

**TRANSLATE:** Pass `n_pixout=2` into `SetAnalyzer`. Add a second `m_pixBufWf` member for pixout=1. Poll loop reads both and emits a separate signal for the WF path.

#### Step 3.5: Wire 9 controls in `TxDisplayPage::buildUI`

**READ:** existing RX-side widgets in `DisplaySetupPages.cpp`. Find a representative RX FFT trackbar / detector combo / averaging combo / normalize checkbox / AvTimeMs spinbox. DO NOT refactor those — copy the pattern and use it standalone for TX.

**TRANSLATE:** replace the 3 placeholder groups (search for `makePlaceholderGroup` in TxDisplayPage::buildUI). Each group becomes a real `QGroupBox` with the listed controls. Wire each to TxAnalyzer setters via `connect(..., this, [sw=...](){ sw->setXxx(...); });` lambdas. Tooltips plain English (no source cites in user strings per `feedback_no_cites_in_user_strings.md`).

#### Step 3.6: Settings load/save in `TxAnalyzer::loadSettings` + `::saveSettings`

**READ:** `RxChannel::loadSettings`-equivalent or existing TX settings load pattern.

**TRANSLATE:** read the 9 keys on construction (or via a `loadSettings()` entry point called from MainWindow wiring), persist via `AppSettings::setValue` on every setter (or batched via `scheduleSettingsSave`).

#### Step 3.7: SpectrumWidget WF pixout=1 tap

**READ:** existing `pushWaterfallRow` and `updateSpectrumLinear` paths in SpectrumWidget. Phase 1 added the per-frame MOX branch for thresholds; this phase adds a parallel path for the WF data source when isTx.

**TRANSLATE:** when `isTx && MOX`, the waterfall row data comes from pixout=1 (the WF-detector-with-WF-averaging pixels) instead of pixout=0 (the trace path). RX path is unaffected.

#### Step 3.8: Tests pass (green; 1 build + 1 ctest of new test only)

#### Step 3.9: Clean build of `NereusSDR` target

#### Step 3.10: Commit (GPG-signed). 4 commits planned:
- Commit 1 after 3.2: `test(3m-5d): tst_tx_analyzer_settings red (TDD)`.
- Commit 2 after 3.4 + 3.8: `feat(3m-5d): TxAnalyzer 9-control state + n_pixout=2 (green)`.
- Commit 3 after 3.5 + 3.6 + 3.7 + 3.9: `feat(3m-5d): wire 9 controls in Setup -> Display -> TX + SpectrumWidget WF pixout=1 tap`.
- Commit 4 after manual bench (controller, not implementer): `docs(3m-5d): close Phase 3`.

#### Step 3.11: Manual bench (controller — Step 2.13-equivalent)

- Setup → Display → TX → all 9 controls render.
- TX + 2-tone: change FFT size (slider) → bin width readout updates; spectrum bin width changes live.
- Change window type → spectrum sidelobe shape changes (BH4 narrow vs Hamming wide).
- Change Pan Detector → pan trace changes (peak vs average vs sample). Normalize checkbox enables for Det >= 2.
- Change Pan Averaging → trace smoothness changes.
- Change WF Detector → waterfall response changes independently of pan.
- Change WF Averaging → waterfall smoothness changes independently.
- Change AvTimeMs values → smoothness ramps.
- Toggle Normalize → trace normalizes to 1 Hz reference.
- Restart app → all 9 choices persist.
- RX side: confirm RX FFT slider, detector, averaging unaffected.
- 3M-5b/5c regressions: confirm waterfall colormap controls + Custom gradient editor + MOX cycle visual still behave per their acceptance.

---

## Phase 4: TX Grid Scale

**Goal:** 6 controls in group 5 of Setup → Display → TX wired to a TX-specific grid renderer state in `SpectrumWidget`. Per-frame MOX branch in the grid-draw path.

**Files:**
- Modify: `src/gui/setup/DisplaySetupPages.cpp` (TxDisplayPage Grid Scale group)
- Modify: `src/gui/SpectrumWidget.{h,cpp}` (TX grid Max/Min/Step/Align/ShowGrid/Fill members + per-frame MOX branch in grid-draw)

**Tasks (summary; expand when phase starts):**

- [ ] Add TX grid AppSettings keys with Thetis defaults: Max=+20, Min=-80, Step=2, Label Align=Center, Display Grid=true, Fill=true.
- [ ] Add `m_txGrid*` members + setters + signals to `SpectrumWidget`.
- [ ] MOX-gate the grid-draw function to use TX values when MOX active.
- [ ] Wire 6 controls in TxDisplayPage Grid Scale group.
- [ ] Bench: verify grid lines and labels respect the TX values during TX.
- [ ] Commit.

---

## Phase 5: TX Appearance colors (with audit-and-migrate)

**Goal:** 13+ color/alpha controls + 2 toggle overlays + 1 line-width slider in a dedicated Setup → Appearance → TX Display sub-tab. Migrates the 2 existing TX color controls (`m_txZeroLineColorBtn`, `m_txFilterColorBtn`) from their current home in the mixed RX/TX `specGroup` of `AppearanceSetupPages.cpp`.

**Files:**
- Create: a new sub-page or section helper in `AppearanceSetupPages.cpp` for the TX Display tab.
- Modify: `src/gui/setup/AppearanceSetupPages.cpp` (move the 2 existing TX entries; add the new ones)
- Modify: `src/gui/SpectrumWidget.{h,cpp}` (add the new TX-specific color members, MOX-gated in render paths)

**Audit-before-add requirement (per `feedback_audit_codebase_before_claiming.md`):**
Before writing any new Phase 5 control, grep `AppearanceSetupPages.cpp` for existing TX-prefixed members. Each must be either reused (if it matches Thetis) or migrated (if it's in the wrong group). No duplicates.

**Tasks (summary; expand when phase starts):**

- [ ] Audit `AppearanceSetupPages.cpp` for all `*Tx*` / `*tx*` controls. Document each (current location, Thetis equivalent, action: keep / move / merge / replace).
- [ ] Create the Setup → Appearance → TX Display sub-tab matching Thetis `tcAppearanceTXDisplay` IA.
- [ ] Migrate `m_txZeroLineColorBtn` and `m_txFilterColorBtn` into the new sub-tab.
- [ ] Add the new TX color controls per the mockup (Data Line, Data Fill, V Grid course/fine, H Grid, Zero Line, Text, Band Edge, Background, plus alpha sliders).
- [ ] Add line width slider for TX trace.
- [ ] Add Show Filter on TX Waterfall + Show Zero Line on TX Waterfall toggles.
- [ ] Wire the panadapter custom gradient slot (Phase 2 widget).
- [ ] MOX-gate the corresponding render-path color reads in `SpectrumWidget`.
- [ ] Bench: each color/alpha visibly affects TX panadapter without affecting RX.
- [ ] Commit.

---

## Phase 6: TX Display Cal Offset

**Goal:** Wire the existing `m_txDisplayOffsetSpin` (UI exists, value persists via `m_calCtrl`) so it actually applies to the TX render path. The offset shifts all TX-side dBm readings by ±100 dB.

**Files:**
- Modify: `src/gui/SpectrumWidget.{h,cpp}` (add `m_txDisplayCalOffset` member, MOX-gated in dBm conversion)
- Modify: `src/core/CalController.{h,cpp}` (or wherever `txDisplayOffsetDb` lives) to emit changes to SpectrumWidget
- Modify: `src/gui/MainWindow.cpp` (wire the calibration controller signal to SpectrumWidget)

**Tasks (summary; expand when phase starts):**

- [ ] Find `txDisplayOffsetDb` getter/setter and verify the persistence + UI is correct.
- [ ] Add SpectrumWidget hook: `setTxDisplayCalOffset(float dbm)` and member.
- [ ] Apply offset in the TX dBm pipeline: `effective_dbm = raw_dbm + m_txDisplayCalOffset`.
- [ ] Wire calibration controller signal to SpectrumWidget.
- [ ] Bench: change offset in Setup → General → Calibration; verify TX panadapter dBm readings shift by the configured amount during TX. RX unaffected.
- [ ] Commit.

---

## File structure summary (new + modified across all phases)

| Path | Status | Phase(s) |
|---|---|---|
| `docs/architecture/mockups/tx-display-tab.html` | created | 0a |
| `docs/architecture/tx-display-settings-master-plan.md` | this file | 0a |
| `src/gui/SpectrumWidget.h` | modified | 1, 3, 4, 5, 6 |
| `src/gui/SpectrumWidget.cpp` | modified | 1, 3, 4, 5, 6 |
| `src/gui/setup/DisplaySetupPages.h` | modified | 1, 3, 4 |
| `src/gui/setup/DisplaySetupPages.cpp` | modified | 1, 3, 4 |
| `src/gui/setup/AppearanceSetupPages.cpp` | modified | 5 |
| `src/gui/MainWindow.cpp` | modified | 1, 6 (light) |
| `src/core/TxAnalyzer.h` | modified | 3 |
| `src/core/TxAnalyzer.cpp` | modified | 3 |
| `src/gui/widgets/GradientPickerWidget.h` | created | 2 |
| `src/gui/widgets/GradientPickerWidget.cpp` | created | 2 |
| `src/gui/widgets/GradientStop.h` | created | 2 |
| `src/core/CalController.h` (or equivalent) | modified | 6 |
| `src/core/CalController.cpp` (or equivalent) | modified | 6 |
| `tests/...` various | created | 1-6 (per-phase) |

---

## Risks and open questions

1. **Per-frame MOX branch performance.** The render path runs at 15-30 fps with potentially 1024+ pixels per frame; an extra `if (m_moxOverlay)` per frame is negligible, but the `dbmToRgb` change does have a small runtime cost. Profile if waterfall rendering shows regression. Likely irrelevant.

2. **AGC behavior preservation for RX.** Phase 1 wraps the AGC and NF-AGC blocks in `!isTx` guards. Verify that RX behavior matches pre-Phase-1 state on the bench (no regression).

3. **Custom Gradient widget scope creep.** Phase 2 might balloon if we pursue full Thetis `ucLGPicker` parity (Thetis has very specific snapping, color theory, etc.). First pass should be functional, not pixel-perfect. Defer parity polish to a follow-up.

4. **Phase 3 widget refactor might require bigger structural changes** if the existing RX FFT slider isn't easy to extract into a reusable form. Worst case, Phase 3 grows by 2-4 hours for the refactor itself.

5. **AppSettings key collisions.** All new keys use the `DisplayTxWf*` / `DisplayTxPan*` / `DisplayTxGrid*` / `AppearanceTxDisplay*` prefixes. Verify no existing key uses these prefixes before adding (grep `AppSettings` keys / `settingsKey` callers in `SpectrumWidget.cpp` and `setup/`).

6. **Multi-pan future.** This plan assumes one panadapter (`m_panIndex` is 0 or 1 for RX1/RX2). The TX overlay shows on whichever pan is active during MOX. Phase 3F multi-panadapter work (per `CLAUDE.md`) is a separate epic; this plan does not anticipate per-pan TX settings.

7. **Per-band TX waterfall.** Out of scope (Thetis doesn't do it). If a future user request asks for it, that becomes a follow-up phase.

---

## Sign-off

This plan is awaiting JJ review before Phase 0b cleanup squash and Phase 1 implementation begin. Updates to this plan during implementation are tracked via amended commits (not PR rebase, since we are staying on `claude/tx-display` and not pursuing a PR yet).
