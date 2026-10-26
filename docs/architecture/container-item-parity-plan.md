# Container Item Options Parity Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** bring every NereusSDR container item's options and controls, and the behaviors
behind them, to parity with Thetis v2.10.3.15, named by slice letter instead of RX1/RX2.

**Architecture:** container items are `MeterItem` subclasses under `src/gui/meters/`,
edited by `src/gui/containers/meter_property_editors/`, created and saved through
`ContainerContentRegistry` (`captureMeterItem` stores only what `item->serialize()`
writes). Fixes stay inside those items, their editors and the registry; no thread,
signal-routing or dependency changes.

**Tech Stack:** C++20, Qt6 Widgets, QtTest, CMake + Ninja.

**Spec:** [container-item-options-parity-audit.md](container-item-options-parity-audit.md)
(the gap tables) plus JJ's rulings recorded in the crew workspace
`/Users/j.j.boyd/NereusSDR-crew/container-item-parity/` and summarized under
Decisions below.

**Stages.** Stage 1 (Tasks 1-3) fixes clear bugs that depend on no open decision and
can run now. Later stages are appended as JJ answers the remaining decisions; each
appended task carries its own full contract.

## Decisions

- D1 (JJ, 2026-10-25, "1 for tje container work"): the plan covers the settings panels
  and the behaviors behind them (VFO display clicks, dial tuning, filter-edge drags,
  text overlay variables, data out sending).
- D2 (JJ, 2026-10-25, "we should drop vfo a and b nomenclature and replace with slic a
  the n"): no "VFO A / VFO B" labels or two-receiver tokens on container items; items
  are named by slice letter, Slice A up to the slice count.
- Settled (veto list): keep NereusSDR-only options beside Thetis's (CLAUDE.md "Don't
  remove code you didn't add"); port Thetis Fade on RX/TX and keep the hide option;
  Thetis 4-character data IDs; the Thetis text overlay TX-LED gating bug
  (MeterManager.cs:20552-20559) is fixed, not ported.

## Global Constraints

- Work in `/Users/j.j.boyd/NereusSDR/.claude/worktrees/gracious-dirac-71a651`, branch `claude/gracious-dirac-71a651`. Never create a worktree or branch, never rebase, never force-push, never push.
- Commits are GPG-signed (`git commit -S`), with no `Co-Authored-By` trailer.
- Ported code follows `docs/attribution/HOW-TO-PORT.md`: every block and constant cites `// From Thetis <file>:<line> [v2.10.3.15]`; Thetis inline comments are copied verbatim; constants stay exact as named `constexpr`.
- C++ rules (CLAUDE.md): no `goto`, no raw `new`/`delete` (unique_ptr or Qt parent), no `#define` constants, braces on all control flow, `auto` only when the type is obvious; `PascalCase` classes, `camelCase` methods, `kPascalCase` constants, `m_camelCase` members; platform guards `Q_OS_WIN`/`Q_OS_MAC`/`Q_OS_LINUX`; errors via `qCWarning(lcCategory)`, no exceptions. Don't remove code you didn't add.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header.
- Settings go through `AppSettings`, never `QSettings`.
- Never change a container item `typeId`; saved layouts load by `typeId` (`ContainerDocumentCodec.cpp:488`). A saved layout written before a change must still load and look the same.
- User-visible strings never say RX1/RX2 or VFO A/B; they name slices (Slice A, B, C, D). No Thetis cites in user-visible strings.
- Build lanes: before any configure, build or test, take a lane exactly as `/Users/j.j.boyd/.config/nereus/work/pr-review-20261021/build-lane-lock.md` says (mkdir lock + owner file, `nice -n 20`, `-j 6`, release the lock the moment the last build or test command ends). Write and self-review the code before taking the lane.
- Disk: run `df -h /System/Volumes/Data` before building; stop and report BLOCKED if free space is under 40 GB.
- One reused build folder: `build/` in the worktree. If it has no `CMakeCache.txt`, configure it only with `/Users/j.j.boyd/NereusSDR-crew/container-item-parity/mac-lane-configure.sh` (offline, network denied). Never configure any other build dir, never download.
- Tests run offscreen, one binary at a time: `QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_name$' --output-on-failure`. Build single test targets (`nice -n 20 cmake --build build -j 6 --target tst_name`), never the whole suite by default. A new test is registered in `tests/CMakeLists.txt` the way neighbouring tests are.
- JJ's own app, settings (`~/.config/NereusSDR/`) and window are read-only; never launch or close his app.

## What already exists

- `src/gui/containers/ContainerContentRegistry.cpp`: item creation (`allocate`), save (`captureMeterItem`, about :500-550) and restore of per-entry context (`onlyWhenRx`/`onlyWhenTx` at :409-410, :545-546).
- `src/gui/meters/MeterItem.cpp` and subclasses: `serialize()` / `deserialize()` write a `|`-separated record per item; the legacy CLOCK record is `CLOCK|x|y|w|h|binding|z|24h|showType`.
- Editors: `src/gui/containers/meter_property_editors/*ItemEditor.cpp`.
- `ContainerManager::setTransmitting(bool)` (`src/gui/containers/ContainerManager.cpp:672`), fed from `RadioModel::transmittingChanged` (`src/gui/MainWindow.cpp:8919-8920`); today it only shows and hides whole containers.
- `ContentPropertyEditor` (`src/gui/containers/ContentPropertyEditor.cpp:303-309`) builds the per-entry property panel and prints "Unsupported configuration" for entries it cannot edit.
- Tests to extend or copy: `tests/tst_container_document.cpp`, `tests/tst_tx_section_content.cpp`, `tests/tst_vfo_display_item_tx_badge.cpp`, `tests/tst_meter_item_bar.cpp`.

## File Structure

| Path | Change |
|---|---|
| `src/gui/meters/ClockItem.cpp`, `VfoDisplayItem.cpp`, `TextOverlayItem.cpp`, `FilterDisplayItem.cpp`, `ButtonBoxItem.cpp` (+ headers as needed) | serialize/deserialize the fields the editors already edit |
| `src/gui/meters/RotatorItem.cpp` | correct a wrong Thetis cite |
| `src/gui/containers/ContainerManager.cpp`/`.h`, meter widget plumbing | route transmit state to items that paint differently on TX |
| `src/gui/containers/ContentPropertyEditor.cpp` | no "Unsupported configuration" for entries with nothing to configure |
| `tests/tst_container_item_persistence.cpp` (new), `tests/tst_container_item_tx_state.cpp` (new), `tests/tst_tx_section_content.cpp` | acceptance tests |

## Task 1: Settings that are lost on reload

**Requirements:** audit Part C "Bugs noticed" and Part D "Button grid common options"
(rows marked "not serialized"); audit Part C ROTATOR "Text colour" row.

**Files:**
- Modify: `src/gui/meters/ClockItem.cpp`/`.h` (serialize and restore time, date and title colours)
- Modify: `src/gui/meters/VfoDisplayItem.cpp`/`.h` (serialize and restore all six colours: frequency, mode, filter, band, RX label, TX label)
- Modify: `src/gui/meters/TextOverlayItem.cpp`/`.h` (serialize and restore textBackColour1/2 and showTextBack1/2)
- Modify: `src/gui/meters/FilterDisplayItem.cpp`/`.h` (serialize and restore the text colour)
- Modify: `src/gui/meters/ButtonBoxItem.cpp`/`.h` (serialize and restore border width, margin, corner radius, height ratio, fade on RX, fade on TX), and the button-grid item types that call its serialize
- Modify: `src/gui/meters/RotatorItem.cpp` (the comment at about :394 only)
- Create: `tests/tst_container_item_persistence.cpp`, registered in `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces: each item's `serialize()` output gains the new fields **appended after the existing fields**, so `deserialize()` of an older record (fewer fields) still succeeds and leaves the new fields at their current defaults.

**Acceptance:**
- For each of Clock, VFO display, text overlay, filter display and every button grid type: create the item, set every listed field to a non-default value, `serialize()`, `deserialize()` into a fresh item; every listed field equals the value set.
- Each item's old-format record (the exact string today's `serialize()` writes, captured in the test as a literal) deserializes without error, the old fields keep their values, and the new fields keep their defaults.
- A round trip through `ContainerContentRegistry` capture and allocate (the path a saved layout takes) keeps the values; use the existing `tst_container_document` pattern.
- RotatorItem: the comment that cites "MeterManager.cs:35275" is replaced by an accurate note: Thetis draws every cardinal, "N" included, in OuterTextColour (`MeterManager.cs:36809 [v2.10.3.15]`); NereusSDR draws "N" bold in the large-dot colour, which is our own choice. Drawing is unchanged.
- No other visible change; existing tests for these items still pass.

**Verification:** risk tier: data-format change with backward compatibility (unit tests
required, test-first for the old-record case). Commands:
`nice -n 20 cmake --build build -j 6 --target tst_container_item_persistence tst_container_document` then
`QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_container_item_persistence|tst_container_document)$' --output-on-failure`,
plus any existing test named for an item touched (`ls tests | grep -i <item>`).

**Execution note (advisory):** opus. No networking, config apply or transmit path.
First task; configures `build/` if needed.

- [ ] **Step 1:** Write the round-trip and old-record tests; see them fail for the dropped fields.
- [ ] **Step 2:** Append the fields to each `serialize()` and read them in `deserialize()` only when present.
- [ ] **Step 3:** Fix the rotator comment.
- [ ] **Step 4:** Build, run the commands above, commit signed: `fix(containers): keep item colours and grid styling across reload`.

## Task 2: Items that change on transmit never hear about it

**Requirements:** audit Part B FADECOVER row (BUG: `FadeCoverItem::setTxState` has no
caller, so a fade cover never paints); audit Part C VFO "TX colour" row
(`VfoDisplayItem::setTransmitting` is never called); audit Part D "Fade on RX / TX"
(ButtonBoxItem guards exist, `setTransmitting` has no caller).

**Files:**
- Modify: `src/gui/containers/ContainerManager.cpp`/`.h` and whatever owns the meter widgets of each container (follow `ContainerManager::setTransmitting` and the `onlyWhenRx`/`onlyWhenTx` path to find where items are reachable)
- Modify: `src/gui/meters/FadeCoverItem.cpp` only if needed for repaint
- Create: `tests/tst_container_item_tx_state.cpp`, registered in `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `ContainerManager::setTransmitting(bool)` (already fed from `RadioModel::transmittingChanged`).
- Produces: every live item of type FadeCoverItem gets `setTxState(tx)`, every VfoDisplayItem and ButtonBoxItem gets `setTransmitting(tx)`, when the transmit state changes, when an item is created or loaded while transmitting (initial state), and the owning widget repaints.

**Acceptance:**
- A fade cover with Fade on RX on paints while receiving and not while transmitting; with Fade on TX on, the reverse (rules at `FadeCoverItem.cpp:84`, from Thetis MeterManager.cs:7887-7888).
- A fade cover added to a container while transmitting starts in the transmitting state.
- A VFO display item's TX label colour shows while transmitting (existing `tst_vfo_display_item_tx_badge` still passes).
- A button grid with Fade on TX ignores clicks while transmitting (guard at `ButtonBoxItem.cpp:265-267`).
- No new connection to RadioModel outside the existing `ContainerManager::setTransmitting` wiring; no core or model file changes.

**Verification:** risk tier: GUI state routing (unit tests with a fake transmit toggle;
no radio). Commands: build and run `tst_container_item_tx_state`, `tst_vfo_display_item_tx_badge`
offscreen as in Global Constraints.

**Execution note (advisory):** opus. Reads transmit state, never sets it. Runs after
Task 1 (shared build dir); touches different files.

- [ ] **Step 1:** Test: toggle `ContainerManager::setTransmitting` on a container holding the three item types; assert each item's state and paint result.
- [ ] **Step 2:** Route the state; set initial state on creation and load.
- [ ] **Step 3:** Build, run, commit signed: `fix(containers): pass transmit state to fade cover, VFO display and button grids`.

## Task 3: TX section items show "Unsupported configuration"

**Requirements:** audit Part E summary item 5 and open question 6: the seven `tx.*`
items (`TxSectionContent.cpp`) have no options, and `ContentPropertyEditor.cpp:306-309`
labels them "Unsupported configuration". Same check for PR 635's `rx.*` items
(`RxSectionContent.cpp`).

**Files:**
- Modify: `src/gui/containers/ContentPropertyEditor.cpp` (the condition at about :306)
- Test: `tests/tst_tx_section_content.cpp` (extend)

**Interfaces:**
- Consumes: `ContainerContentRegistry::declaredProperties(typeId)`, `isAvailable`, `unavailableReason`.
- Produces: nothing new.

**Acceptance:**
- Opening the property editor for any `tx.*` or `rx.*` entry shows no "Unsupported configuration" text. It shows the normal entry rows (name, geometry and whatever general rows every entry gets) and no item-specific group.
- An entry whose typeId the registry does not know at all (a layout from a newer build) still shows "Unsupported configuration. Original data is retained; this object can be removed explicitly."
- An entry with an `unavailableReason` still shows that reason.

**Verification:** risk tier: low (one condition, UI text). Build and run
`tst_tx_section_content` offscreen; add an `rx.*` case there or in the nearest RX
section test (`ls tests | grep -i rx_section`).

**Execution note (advisory):** opus. No device or transmit path. Runs after Task 2.

- [ ] **Step 1:** Test the three cases above.
- [ ] **Step 2:** Make the condition tell "known type with no options" from "unknown type".
- [ ] **Step 3:** Build, run, commit signed: `fix(containers): TX and RX section items are not unsupported`.
