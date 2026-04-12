# Phase 3G-6: Container Settings Dialog — Debug Handoff

> **Status:** **RESOLVED 2026-04-12.** Every issue identified in
> this handoff is fixed by the Phase 3G-6 (one-shot) rewrite on
> branch `feature/phase3g6-oneshot` (PR #2). See
> [`phase3g6a-plan.md`](phase3g6a-plan.md) for the executed plan
> and [`../../CHANGELOG.md`](../../CHANGELOG.md) for the per-block
> commit summary. Specific resolutions:
>
> - **ANANMM/CrossNeedle missing PNG paths** → block 1 commits 1-2:
>   meter PNGs registered as Qt resources under `/meters` qresource
>   prefix, ItemGroup paths switched to `:/meters/...`.
> - **Scale labels overlap from `NeedleScalePwrItem`** → block 1
>   commits 3-5: `onlyWhenRx`/`onlyWhenTx`/`displayGroup` ported to
>   `MeterItem` base, Thetis filter rule wired into
>   `MeterWidget::shouldRender()`, ANANMM `NeedleScalePwrItem`
>   pinned to power/SWR group.
> - **Live preview panel rendered as solid black** → block 3
>   commit 11: live preview panel removed entirely; replaced by
>   in-place editing against the live container with
>   snapshot/revert on Cancel (block 3 commit 14).
> - **Property editors only existed for 6 of ~30 item types** →
>   block 4: every implemented `MeterItem` subclass has a
>   dedicated `BaseItemEditor` subclass under
>   `src/gui/containers/meter_property_editors/`. 31 editors
>   produced in parallel by 4 subagents.
> - **No menu-based access to floating containers** → block 6:
>   `Containers → Edit Container` submenu populated dynamically
>   from `ContainerManager::allContainers()`.
>
> Original handoff text retained below for historical context.

> **Status:** Infrastructure complete, needs visual debugging and UX polish.
> **Branch:** `feature/phase3g6-container-settings-dialog`
> **PR:** boydsoftprez/NereusSDR#1

## Debug Session Prompt

Paste the following into a fresh Claude Code session to start debugging:

---

I'm working on NereusSDR Phase 3G-6 (Container Settings Dialog). The implementation is functionally complete but has usability bugs, visual rendering issues, and likely needs workflow rethinking.

**Worktree:** `/Users/j.j.boyd/NereusSDR/.worktrees/phase3g6`
**Branch:** `feature/phase3g6-container-settings-dialog`
**PR:** boydsoftprez/NereusSDR#1

## Your Mission

This is a deep investigation, not a quick fix pass. You need to understand the container + meter system end-to-end, visually verify what's actually rendering on screen, and fix both functional bugs and rendering issues.

### Phase 1: Read Everything, Build a Mental Model

Before touching anything, read every file listed below thoroughly. Build a complete mental model of:

- How containers are created, docked, floated, persisted, and destroyed
- How MeterWidget renders items (GPU pipeline vs CPU fallback, the 3-pipeline architecture: background texture, vertex geometry, QPainter overlay)
- How MeterItems paint themselves across the 4 render layers (Background, Geometry, OverlayStatic, OverlayDynamic)
- How items are serialized/deserialized (pipe-delimited format, type tags)
- How ItemGroup presets are constructed and installed via `installInto()`
- How the AppletPanelWidget nests a MeterWidget as its header in Container #0
- How ContainerSettingsDialog is supposed to interact with all of the above
- How Thetis handles this in MeterManager.cs (AddMeterContainer, ShowMultiMeterSetupTab, container settings in setup.cs)

### Phase 2: Visual Verification — Screenshot-Driven Debugging

Launch the app and **take screenshots at every step** to verify what's actually rendering. Use this pattern:

```bash
# Launch the app
open build/NereusSDR.app

# Wait for window, then screenshot
sleep 3
screencapture -l $(osascript -e 'tell app "NereusSDR" to id of window 1') /tmp/nereus_main.png

# After each action (open dialog, load preset, click OK), screenshot again
screencapture -l $(osascript -e 'tell app "NereusSDR" to id of window 1') /tmp/nereus_step2.png
```

If `screencapture -l` doesn't cooperate with window IDs, use region capture or full-screen:
```bash
screencapture /tmp/nereus_full.png
```

Then **read the screenshots** with the Read tool to visually inspect what rendered. Compare what you see against what the code says should render. This is your primary debugging tool — don't just read code and guess, actually look at the pixels.

**Visual checkpoints to capture and inspect:**

1. Main window on launch — is the S-Meter in Container #0 rendering correctly? Are the needle, arc, scale ticks, readouts all visible?
2. Container Settings dialog opened — does it show the 3-panel layout? Are items listed?
3. Item selected — does the property editor appear in the center panel?
4. Preset loaded (try S-Meter Only, ANANMM, Power/SWR) — does the preview panel show anything? Is it correct?
5. After OK — did the container's meter actually change? Screenshot before and after.
6. New Container → ANANMM preset → OK — does the floating window render the 7-needle meter?
7. Each type of item rendering — are bars, needles, scales, text readouts, LEDs all painting correctly?

### Phase 3: Objective Assessment + Recommendations

After you understand the system and have visual evidence, assess honestly:

**Questions to ask yourself (and me):**

- What is the actual user journey for creating a new ANANMM floating meter? Walk through every click. Where does it break? Where is it confusing?
- What is the journey for editing Container #0's S-Meter? Does the dialog find the MeterWidget? Do properties populate? Does apply write back? Does the meter actually update visually?
- Are meters rendering correctly at all right now, independent of the dialog? If the S-Meter in the main panel is broken, that's a different bug than the dialog.
- Is the 3-panel dialog layout the right UX? Would a simpler approach work better? What would Thetis users expect?
- Is the preview panel useful or dead weight? Could we show changes live in the actual container instead?
- Should "New Container" and "Edit Container" be the same dialog or different workflows?
- Are there Qt6 dialog patterns or widget practices we're not following?
- Think about the ham radio operator using this — they want to set up their meter display quickly and get back to operating. Every extra click is friction.

## What Was Built

- `ContainerSettingsDialog` — 3-panel modal (content list / property editor / live preview)
- Opened from: Containers menu → "Container Settings..." (edits Container #0) or "New Container..." (creates floating container)
- Also opens from gear icon on container title bars
- 30+ presets via ItemGroup factories, import/export via Base64 clipboard
- 6-category Add Item picker (28 item types), type-specific property editors for 6 item types
- MeterWidget::deserializeItems() extended to all 28 type tags

## Known and Suspected Issues

### Rendering / Visual
1. **Meters may be visually broken** — the S-Meter, bar meters, needle items may not be painting correctly. Need visual verification via screenshots.
2. **Live preview rendering** — MeterWidget inside dialog may not GPU-init correctly (QRhiWidget needs native window attribute + proper API selection). May need CPU fallback path.
3. **Preset rendering** — ANANMM/CrossNeedle presets depend on background PNG images from `resources/meters/`. These may not exist yet or may not load correctly.
4. **Item layer rendering** — multi-layer items (NeedleItem participates in all 4 pipelines) may not composite correctly in the preview.

### Functional
5. **Property editor population** — QScrollArea/QStackedWidget mismatch was fixed but needs visual verification
6. **Apply/OK write-back** — items written back via serialize round-trip; may not actually render in container after apply
7. **New Container flow** — floating container + dialog; cancel should destroy; whole flow untested visually
8. **Item reorder** — does move up/down change actual rendering order?
9. **Import/Export** — Base64 round-trip edge cases

### UX / Polish
10. **Dark theme consistency** — are all widgets styled? Any white/system-default widgets leaking through?
11. **Widget sizing** — do spinboxes, combos, color buttons look right?
12. **Scroll behavior** — does the property editor scroll properly with many properties?
13. **Focus handling** — do tab order and keyboard navigation work?

## How I Want You To Work

1. **Read all the code first.** Build context before touching anything.
2. **Take screenshots at every step.** Visual verification is mandatory — read screenshots with the Read tool.
3. **When you find a problem, present 2-3 approaches** with trade-offs and your recommendation before implementing:
   - A) Quick fix — describe
   - B) Better fix — describe
   - C) Rethink — describe
   - Recommendation: B because...
4. **Be creative.** If the current approach is fundamentally wrong, say so. Propose alternatives.
5. **Ask me questions** when you need design decisions. Don't guess at UX intent.
6. **After each fix, rebuild, relaunch, screenshot** to verify the fix actually worked visually.
7. **Think about the ham radio operator** — they want to set up their meter display quickly and get back to operating.

## Key Files (read all of these)

### Project Context
- `CLAUDE.md` — project context, read first
- `CONTRIBUTING.md` — coding conventions
- `STYLEGUIDE.md` — visual design language (colors, fonts, button states)
- `docs/architecture/phase3g4-g6-advanced-meters-design.md` — Phase 3G-6 design spec

### Dialog + Containers
- `src/gui/containers/ContainerSettingsDialog.h/.cpp` — the dialog under test
- `src/gui/containers/ContainerWidget.h/.cpp` — container shell (dock/float/resize)
- `src/gui/containers/ContainerManager.h/.cpp` — container lifecycle + persistence
- `src/gui/containers/FloatingContainer.h/.cpp` — floating window wrapper
- `src/gui/MainWindow.cpp:832-870` — Containers menu actions

### Meter Rendering Pipeline
- `src/gui/meters/MeterWidget.h/.cpp` — GPU/CPU renderer (3-pipeline architecture)
- `src/gui/meters/MeterItem.h/.cpp` — base item + core types (BarItem, NeedleItem, ScaleItem, TextItem, SolidColourItem, ImageItem)
- `src/gui/meters/ItemGroup.h/.cpp` — preset factories + serialize/deserialize registry

### Specific Item Types (check rendering of each)
- `src/gui/meters/HistoryGraphItem.h/.cpp`
- `src/gui/meters/MagicEyeItem.h/.cpp`
- `src/gui/meters/DialItem.h/.cpp`
- `src/gui/meters/LEDItem.h/.cpp`
- `src/gui/meters/SignalTextItem.h/.cpp`
- `src/gui/meters/NeedleScalePwrItem.h/.cpp`
- `src/gui/meters/FilterDisplayItem.h/.cpp`
- `src/gui/meters/RotatorItem.h/.cpp`

### Supporting
- `src/gui/meters/MeterPoller.h` — binding IDs
- `src/gui/applets/AppletPanelWidget.h/.cpp` — Container #0's content wrapper
- `src/gui/StyleConstants.h` — shared color palette

### Thetis Reference
- `../Thetis/Project Files/Source/Console/MeterManager.cs` — AddMeterContainer, container lifecycle, meter rendering
- `../Thetis/Project Files/Source/Console/setup.cs:26962` — ShowMultiMeterSetupTab, container settings UI
- `../Thetis/Project Files/Source/Console/ucMeter.cs` — container widget equivalent

---

## Commits on Branch

```
e1b94f6 fix(3G-6): fix property editor not showing when item selected
6a60c86 fix(3G-6): fix dialog item discovery, write-back, and new container action
4f37d46 feat(3G-6): wire Container Settings to Containers menu
c84f9ab docs: mark Phase 3G-6 Container Settings Dialog as complete
cf4a815 fix(3G-6): extend MeterWidget deserializeItems() with all item types
cb19926 feat(3G-6): wire container settings button to dialog
287f7f9 feat(3G-6): preset browser with 30+ built-in presets
810a83e feat(3G-6): property editors — common props + type-specific for 6 types
eb100c0 feat(3G-6): add/remove/reorder items with categorized picker
5e55171 feat(3G-6): add item type registry + content list population
429c9a3 feat(3G-6): add ContainerSettingsDialog shell with 3-panel layout
```
