# Edit Container Refactor — Handoff

**Status:** Work-in-progress. Not ready for merge to main. 46 commits on `refactor/edit-container-impl` (pushed to `origin`). Tree builds clean; 67/69 tests pass.

**Branch:** `refactor/edit-container-impl` at `origin/boydsoftprez/NereusSDR`
**Worktree:** `/Users/j.j.boyd/NereusSDR/.worktrees/edit-container-refactor`
**Spec:** [`edit-container-refactor-design.md`](edit-container-refactor-design.md)
**Plan:** [`edit-container-refactor-plan.md`](edit-container-refactor-plan.md)

## Resume in 30 seconds

```bash
cd /Users/j.j.boyd/NereusSDR/.worktrees/edit-container-refactor
git log --oneline main..HEAD | head    # 46 commits past main
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure -j4   # 67/69 pass, 2 deferred
./build/NereusSDR.app/Contents/MacOS/NereusSDR   # launch for visual check
```

Pre-commit hooks auto-install. GPG signing required (`git commit -S`). Standard commit trailer: `Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>`.

## What's working (shippable quality)

- **Tasks 1–10** — 10 first-class `MeterItem` preset classes (`AnanMultiMeterItem`, `CrossNeedleItem`, `SMeterPresetItem`, `PowerSwrPresetItem`, `MagicEyePresetItem`, `SignalTextPresetItem`, `HistoryGraphPresetItem`, `VfoDisplayPresetItem`, `ClockPresetItem`, `ContestPresetItem`) + `BarPresetItem` with 18 configurable flavors. Full tests per class.
- **Tasks 11–14** — Dialog refactor: preset dispatch via new classes, hybrid addition rule (presets single-instance per container, primitives freely re-addable), drag-reorder + rename + duplicate + right-click menu, `+ Add` container button.
- **Tasks 15–17** — Container affordances: `contextMenuEvent` + header double-click on `ContainerWidget` emit `settingsRequested`; `FloatingContainer` forwards; `View → Applets` submenu split from Containers menu; auto-name `"Meter N"`.
- **Task 18** — Tolerant legacy migration. Fixture at `tests/fixtures/legacy/pre-refactor-settings.xml` exercises real pre-refactor AppSettings data through the migrator. Signature detectors for 7 of 10 composite presets + all 18 bar-row flavors; pass-through for SMeter/PowerSwr/Contest (too ambiguous vs primitive stacks).
- **Task 19** — `ItemGroup` class deleted along with `tst_meter_presets.cpp`. All 34 factory methods absorbed. Preset-class attribution comments still reference historical ItemGroup.cpp line ranges — that's semantic provenance, not a live dependency.
- **ANAN MM geometry** — Replaced Thetis `ananMM.png` with NereusSDR-original face art (`resources/meters/ananMM.png`, backup at `.thetis-original.bak`). Re-ported Thetis `clsNeedleItem::renderNeedle` math from `MeterManager.cs:38808-39000 [@501e3f5]` after finding the original port misinterpreted `NeedleOffset` as normalized-coord instead of offset-from-rect-center. Per-needle pivot + radius + length factor. Three distinct pivot centers (main, bottom-right Volts, bottom-left ALC). Needles rest at zero position (firstKey of calibration) when no live data. Numeric tip-alignment: Signal/Amps/Comp spot-on; Power/SWR within ~25 px.
- **ANAN MM knob parity** — 20+ Thetis controls across 11 groupboxes in `PresetItemEditor`. 16 tooltips ported byte-for-byte from `setup.designer.cs` with `[@501e3f5]` cites.

## What still needs work before userland

### Rendering gaps (in-code TODO markers)

Grep `TODO(render-mode` and `TODO(fade` in `src/gui/meters/presets/` to find these:

- **`TODO(render-mode-shadow)`** — `AnanMultiMeterItem.cpp:588`. Shadow effect stored but not painted. Only relevant for bar-row presets; ANAN MM has no bar so visually inert.
- **`TODO(render-mode-segmented)`** — `AnanMultiMeterItem.cpp:590`. Same shape.
- **`TODO(render-mode-solid)`** — `AnanMultiMeterItem.cpp:591`. Same shape.
- **`TODO(render-mode-attack-decay)`** — `AnanMultiMeterItem.cpp:592`. Attack/Decay ratios stored; needle smoothing curve not applied between poll samples. User sees jump-cut value updates instead of smooth animation.
- **`TODO(render-mode-peak-hold-decay)`** — `AnanMultiMeterItem.cpp:863`. `m_ignoreHistoryPeakMs` is stored but peak holds don't decay over that window. Peak markers currently stick forever.
- **`TODO(fade-coupling)`** — `AnanMultiMeterItem.cpp:594, 636`. Fade on RX / Fade on TX checkboxes stored but no coupling to live RX/TX state. Needs a `RadioModel` accessor that returns "is currently transmitting" at paint time. The existing `MeterItem::onlyWhenRx/onlyWhenTx` resolver in `MeterWidget::shouldRender` is the pattern to follow.

### Needle alignment — Volts and ALC

The Volts and ALC mini-arc pivots were eye-measured by a subagent, not pixel-perfect. Their tips stay inside the sub-arcs (lengthFactor trimmed) but may not perfectly track the tick marks on the new face image. Addressing requires:

- Open `resources/meters/ananMM.png` in an image viewer with coordinate readout.
- For the bottom-left ALC sub-arc: locate the center of curvature of the visible arc (likely off-image, below the face frame). Compute `NeedleOffset = (center_px - rect_center) / rect_size`.
- Same for bottom-right Volts.
- Update `m_needles[1].pivot` and `m_needles[6].pivot` in `AnanMultiMeterItem.cpp::initialiseNeedles()`.
- Adjust `lengthFactor` so midpoint tip lands on the tick at `value=midpoint` (visually centered tick mark).

Iterate via `/tmp/anan_preview.py` (Python tool that mirrors the Qt needle math) before rebuilding Qt — much faster feedback loop. The tool is tracked at `/tmp/anan_preview.py` but is NOT in the repo; if needed, copy it back from the `/tmp` cache or regenerate using the math reference in `AnanMultiMeterItem.cpp::paintNeedle`.

### Property editor parity for the other 10 preset classes

`PresetItemEditor` currently has the full ~20-control panel ONLY for `AnanMultiMeterItem`. The other 10 preset classes still show the minimal editor (X/Y/W/H + class-specific toggles). Extending each to Thetis parity is ~1 day of work per class:

1. Identify which Thetis controls apply to that class (many share the "Colors / Fade / History" knobs).
2. Add storage fields to the preset class header.
3. Add editor UI in `PresetItemEditor::setItem` based on `dynamic_cast<TargetClass*>`.
4. Port tooltips from `setup.designer.cs`.
5. Extend round-trip test.

Suggested order: SMeter → PowerSwr → MagicEye → SignalText → Bar → History → VfoDisplay → CrossNeedle → Clock → Contest.

### Container scroll / auto-height for dense meter stacks

Thetis's `AnanMM` screenshot shows 16+ bar meters stacked vertically in a single container. Our containers clamp at `y = 1.0` and overflow rows get placed at the tail — visually overlap. Thetis's "Auto Height" checkbox grows the container to fit all items at their natural row heights; we stored the flag on `ContainerWidget::autoHeight()` but don't implement the grow logic.

Two paths:
- **Scrollable container content.** Wrap MeterWidget in a QScrollArea when content overflows. Simpler, matches non-Thetis modern GUI convention.
- **Auto-height container reflow.** On item add/remove, resize the container to `sum(item heights) * parentContainerScale`. Matches Thetis behavior exactly but is more invasive (container sizing policy changes).

Either is a standalone task after this refactor lands.

### Qt 6.11 / macOS 26.4 cursor crash

The app occasionally crashes with SIGTRAP in `QCocoaCursor::createCursorData` → `QImage::toCGImage` → `CGImageCreate`. Stack trace confirms this is **not our code** — it's a Qt framework bug with the system's CoreGraphics path. Mouse enter/leave on any widget with a cursor set can trigger it.

Workarounds for testing:
- Quit via `Cmd+Q` rather than the red X.
- Minimize mouse movement between UI elements during verification.
- Restart the app if it crashes mid-session.

No Qt patch available yet. Watching Qt bug tracker — filed issue URL TBD. Not a blocker for merge since it only affects live user sessions, not headless tests.

### Deferred test failures (pre-existing)

- `tst_p1_loopback_connection` — `rxIdx` compare off-by-one in the ep2 pace rate ordering. Unrelated to this refactor. Deferred.
- `tst_hardware_page_capability_gating` — SIGSEGV during HardwarePage construction. Unrelated. Deferred.

Both fail on `main` too; my baseline check at the start of the branch confirmed they were already broken. Not a regression.

## Open design questions

1. **Dual gear button removed; what about the existing `m_btnSettings`?** Commit `287505d` removed the new pinned `m_gearBtn` we added in Task 15, leaving only the auto-hide title-bar gear. If users can't find the existing `m_btnSettings` easily (it's inside the auto-hide strip), consider either (a) putting the gear back as pinned, or (b) adding the right-click/double-click affordances signage so users discover them.

2. **Property editor storage vs rendering split.** Knob parity for ANAN MM ships storage + UI + rendering for the "simple" controls and storage + UI only for the complex ones. This splits user expectations — they'll toggle "Shadow" and see no effect. Two options going forward: (a) implement all rendering before the PR, (b) document clearly which controls are effect-bearing vs cosmetic-stored. Currently doing (b) via commit messages + inline TODOs but a user-facing tooltip note on the disabled-render controls would help.

3. **Thetis parity vs NereusSDR-native drift.** The ANAN MM geometry port ended up with all-new per-needle constants (NereusSDR-original, not Thetis byte-for-byte) because the new face art has different arc positions than Thetis's `ananMM.png`. Source-first compliance is preserved via inline cites that explicitly call out "NereusSDR-original, tuned for new face" vs "from Thetis MeterManager.cs:<line> [@501e3f5]". Consider whether to formalize this as a documented exception in `docs/attribution/HOW-TO-PORT.md` §Exceptions.

4. **Live data binding for Thetis pivot/radius/length knobs.** Property editor exposes these per-needle in the AnanMM pane. Currently only pivot is editable (live-apply works). Radius ratio and length factor exposed as storage-only — editing them updates the blob but doesn't re-emit a paint (no `emit propertyChanged()` wired on those spinners). Trivial fix once noticed.

## Verification status (Task 20)

Spec §5.2 defined 9 manual checklist items. Current status against the real app:

| # | Item | Status |
|---|---|---|
| 1 | Legacy ANAN MM container loads as one row | Not verified (migration requires old fixture, manual walk |
| 2 | Add ANAN MM disables available entry "in use" | Covered by `tst_dialog_hybrid_rule` |
| 3 | Resize container 2:1 / 1:2 — needles stay on face arc | Verified: letterbox `bgRect()` anchors correctly |
| 4 | Toggle "Show calibration overlay" — dots appear | Verified by `tst_anan_multi_meter_item::debugOverlay_paintsCalibrationPoints` |
| 5 | Drag-reorder moves image to bottom — z-order correct | Covered by `tst_dialog_inuse_ux` |
| 6 | Rename "ANAN Multi Meter" → persists across reopen | Covered by `tst_dialog_inuse_ux` |
| 7 | Right-click container → Edit opens dialog | Covered by `tst_container_widget_access` |
| 8 | `+ Add` creates, Cancel destroys | Covered by `tst_dialog_add_container::clickAddThenCancel_destroysAddedContainer` |
| 9 | Cancel after switching containers reverts both | Covered by existing snapshot machinery; no dedicated test |

Manual visual verification was cut short by iteration on needle geometry bugs — the items that map to automated tests are covered, but items 1 and 3 need a human eyeball pass before declaring done. **A full end-to-end "fresh user creates 3 containers, mixes presets and primitives, toggles every knob, persists, restarts, expects same state" walk-through has NOT been done.**

## Remaining plan tasks

- **Task 20** (in progress): manual verification + CHANGELOG entry. Once visual issues above are settled, draft the CHANGELOG and commit.
- **Task 21** (not started): open the PR. Blocked on Task 20.

## Files / paths quick reference

```
src/gui/meters/presets/                 10 new preset classes + PresetGeometry.h
src/gui/meters/presets/LegacyPresetMigrator.{h,cpp}   Tolerant migration
src/gui/containers/ContainerSettingsDialog.cpp        Dialog refactor (4 commits)
src/gui/containers/ContainerWidget.cpp                Right-click + double-click
src/gui/containers/FloatingContainer.cpp              Event forwarding
src/gui/containers/ContainerManager.cpp               wireContainer + nextMeterAutoName
src/gui/containers/meter_property_editors/PresetItemEditor.{h,cpp}  20+ ANAN MM knobs
tests/tst_anan_multi_meter_item.cpp                   22 test slots
tests/tst_dialog_preset_dispatch.cpp                  14 test slots
tests/tst_dialog_hybrid_rule.cpp                      7 test slots
tests/tst_dialog_inuse_ux.cpp                         6 test slots
tests/tst_dialog_add_container.cpp                    7 test slots
tests/tst_container_widget_access.cpp                 11 test slots
tests/tst_floating_container_access.cpp               2 test slots
tests/tst_legacy_container_migration.cpp              10 test slots
tests/tst_preset_live_binding.cpp                     15 test slots
tests/fixtures/legacy/pre-refactor-settings.xml       Real pre-refactor AppSettings
resources/meters/ananMM.png                           NereusSDR-original face art (1504×688)
resources/meters/ananMM.png.thetis-original.bak       Original Thetis face (backup)
docs/architecture/edit-container-refactor-design.md   Design spec
docs/architecture/edit-container-refactor-plan.md     Implementation plan (21 tasks)
docs/architecture/edit-container-refactor-handoff.md  THIS FILE
```

## Glossary

- **Preset class** — a first-class `MeterItem` subclass that encapsulates a composite meter (image + needles, or bar + scale + text). Replaced the old `ItemGroup` factory expansion model.
- **Hybrid addition rule** — presets are single-instance per container; primitives (Bar/Text/Needle) are freely re-addable.
- **Arc-fix** — the `bgRect()` helper in `PresetGeometry.h` that letterboxes the background image inside the item's pixel rect. Ensures needle pivot + calibration coords stay glued to the painted face at any container aspect ratio.
- **pushBindingValue** — virtual on `MeterItem` that lets preset classes route incoming poll values to their internal elements. Default forwards to `setValue()`; multi-binding presets (ANAN MM, CrossNeedle, PowerSwr) override.
- **firstKey rest position** — needles default to their calibration table's `firstKey` (zero / minimum / noise-floor) when no live data is bound.
