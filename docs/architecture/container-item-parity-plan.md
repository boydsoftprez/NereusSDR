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

**Stages.** Stage 1 (Tasks 1-3) fixes clear bugs that depend on no open decision.
Stage 2 (Tasks 4-66) brings each item's options and behaviors to parity, in dependency
order; every decision it needs is settled in the task text. Each task carries its own
full contract and names its prerequisite tasks. Work deliberately left out is listed in
the appendix at the end.

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
- Settled at plan merge (controller, under JJ's delegation): LED No TX in a remote
  window calls `stopAllTx` through that window's session and Task 27 adds a
  core-and-window loopback test proving the stop reaches the Core (if it does not,
  the implementer reports BLOCKED rather than shipping a local-only stop); Task 17
  keeps the power limit and peak colours for `meter.powerSwr` (superset rule); an
  entry bound to another session follows that session's transmit state, never this
  window's (fade, stage dim and LED No TX alike).
- Voice record/play is out of this plan (JJ reopened the design); its buttons stay in
  the follow-up appendix.

## Global Constraints

- Work in `/Users/j.j.boyd/NereusSDR/.claude/worktrees/gracious-dirac-71a651`, branch `claude/gracious-dirac-71a651`. Never create a worktree or branch, never rebase, never force-push, never push.
- Commits are GPG-signed (`git commit -S`), with no `Co-Authored-By` trailer.
- Ported code follows `docs/attribution/HOW-TO-PORT.md`: every block and constant cites `// From Thetis <file>:<line> [v2.10.3.15]`; Thetis inline comments are copied verbatim; constants stay exact as named `constexpr`.
- C++ rules (CLAUDE.md): no `goto`, no raw `new`/`delete` (unique_ptr or Qt parent), no `#define` constants, braces on all control flow, `auto` only when the type is obvious; `PascalCase` classes, `camelCase` methods, `kPascalCase` constants, `m_camelCase` members; platform guards `Q_OS_WIN`/`Q_OS_MAC`/`Q_OS_LINUX`; errors via `qCWarning(lcCategory)`, no exceptions. Don't remove code you didn't add.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header.
- Settings go through `AppSettings`, never `QSettings`.
- Never change a container item `typeId`; saved layouts load by `typeId` (`ContainerDocumentCodec.cpp:488`). A saved layout written before a change must still load and look the same.
- User-visible strings never say RX1/RX2 or VFO A/B; they name slices (Slice A, B, C, D). No Thetis cites in user-visible strings.
- Build admission (load based, JJ 2026-10-25): write and self-review the code first; then, before any configure, build or test, run `/Users/j.j.boyd/.config/nereus/work/lane-admit.sh take <task-name>` in the background (never a chain of foreground sleeps) and wait for it to return; keep `nice -n 20` and `-j 6`, tests one binary at a time; run `/Users/j.j.boyd/.config/nereus/work/lane-admit.sh release <task-name>` the moment the last build or test command ends. Rules: `/Users/j.j.boyd/.config/nereus/work/pr-review-20261021/build-lane-lock.md`.
- Disk: run `df -h /System/Volumes/Data` before building; stop and report BLOCKED if free space is under 40 GB.
- One reused build folder: `build/` in the worktree. If it has no `CMakeCache.txt`, configure it only with `/Users/j.j.boyd/NereusSDR-crew/container-item-parity/mac-lane-configure.sh` (offline, network denied). Never configure any other build dir, never download.
- Tests run offscreen, one binary at a time: `QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_name$' --output-on-failure`. Build single test targets (`nice -n 20 cmake --build build -j 6 --target tst_name`), never the whole suite by default. A new test is registered in `tests/CMakeLists.txt` the way neighbouring tests are.
- JJ's own app, settings (`~/.config/NereusSDR/`) and window are read-only; never launch or close his app.

## What already exists

- `src/gui/containers/ContainerContentRegistry.cpp`: item creation (`allocate`), save (`captureMeterItem`, about :500-550) and restore of per-entry context (`onlyWhenRx`/`onlyWhenTx` at :409-410, :545-546).
- `src/gui/meters/MeterItem.cpp` and subclasses: `serialize()` / `deserialize()` write a `|`-separated record per item; the legacy CLOCK record is `CLOCK|x|y|w|h|binding|z|24h|showType`.
- Editors: `src/gui/containers/meter_property_editors/*ItemEditor.cpp`.
- `ContainerManager::setTransmitting(bool)` (`src/gui/containers/ContainerManager.cpp`, about :672-685), fed from `RadioModel::transmittingChanged` (`src/gui/MainWindow.cpp:8919-8920`); today it only shows and hides whole containers.
- `ContentPropertyEditor` (`src/gui/containers/ContentPropertyEditor.cpp:303-309`) builds the per-entry property panel and prints "Unsupported configuration" for entries it cannot edit.
- Tests to extend or copy: `tests/tst_container_document.cpp`, `tests/tst_tx_section_content.cpp`, `tests/tst_vfo_display_item_tx_badge.cpp`, `tests/tst_meter_item_bar.cpp`.

Stage 2 (Tasks 4 onward) also builds on these:

- Thetis source at `../Thetis/Project Files/Source/Console/` (pinned v2.10.3.15): `MeterManager.cs`, `setup.cs`, `setup.designer.cs`, `console.cs`, `common.cs`, `enums.cs`, `ucOtherButtonsOptionsGrid.cs`. Cites in the tasks are to these files.
- `MeterItem` (`src/gui/meters/MeterItem.h`): static `formatValue(float, MeterUnit, bool)` (:214), virtual `setUnitMode` (:218), `readingBindings`/`pushBindingValue` (:239-240), `advanceMeter(qint64)` (:242), `resetForTxTransition(bool)` (:243), `setPowerScale` (:246-247), `setBindingUnavailable` (:250), `setMmioBinding(QUuid, QString)` (:292), `serialize`/`deserialize` (:319-320), mouse and wheel handlers (:323-327); `BarItem::BarStyle { Filled, Edge, Line }` (:480); `SolidColourItem` (:429, default `#0f0f1a`); `NeedleItem::kS9Dbm` (:894).
- `MeterWidget` (`src/gui/meters/MeterWidget.h/.cpp`): `updateMeterValue` (:118), `advanceMeters(qint64)` (`.cpp:374`), `drawUnavailableVeils` (`.cpp:727`, called on the CPU path at :722 and the GPU path at :1247), `setMox`/`setDisplayGroup` (:174-176), `m_lastBindingValue` (:269). Task 2 adds `MeterWidget::setItemTxState(bool)` / `itemTxState()`, which `ContainerManager::setTransmitting` and `ContainerManager::meterReadyForPolling` feed.
- `MeterPoller` (`src/gui/meters/MeterPoller.h`): the `MeterBinding` id namespace (:154-204: `SignalPeak` 0, `SignalAvg` 1, `TxPower` 100, `TxReversePower` 101, `TxSwr` 102, `TxAlcGain` 109, `TxAlcGroup` 110, `TxMicPeak` 113 to `TxCfcPeak` 118, `HwVolts` 200, `HwAmps` 201), `addReadingConsumer` (:362), signals `readingUpdated` (:455) and `rxSourceChanged` (:467).
- Presets (`src/gui/meters/presets/`): `BarPresetItem` (`variants()` at `.cpp:91`, `typeId()` at `.cpp:127`, `setAboveS9Frequency` at `.h:63`), `CompositePresetItem` (`editableFields`, `applyConfiguration`; `fadeRx`/`fadeTx` keys at `.cpp:262`, `:554`, `:592`, instant 25% fade at about `.cpp:1181`; `setAboveS9Frequency` at `.h:89`).
- `ContainerSourceAdapter::slice` (`src/gui/containers/ContainerSourceAdapter.cpp:26-35`): a context naming a session other than the window's returns no slice.
- `MainWindow` (`src/gui/MainWindow.cpp`): `refreshContainerMeter` (:15030), its `applyTo` (:15177-15183, today `slice->frequency() > 30000000.0`), `onContainerItemAdded`, `watchSlicesForContainers`, `onContainerOtherButtonClicked` (:15248), `containerSessionId()` (`MainWindow.h:1144`).
- `ContainerButtonDispatcher` (`src/gui/containers/ContainerButtonDispatcher.h`): `Hooks` (:81-118, including `spectrumFor`, `transmitSettingsPermitted`/`transmitSettingsReason`), `sliceFor(int)` (:130), `noSliceReason(int)` (:132), `stateOf` (:135).
- MMIO (`src/core/mmio/`): `ExternalVariableEngine` (`endpoints()`, `endpoint(QUuid)`), `MmioEndpoint` (`guid()`), `ITransportWorker` and the UDP, TCP listener, TCP client and serial workers; `src/gui/containers/MmioEndpointsDialog`.
- Models: `SliceModel` (`kTuneStepList`, 26 entries, `SliceModel.h:338`; `tuneStepIndexForHz`; `locked()` :1142; `isTxSlice()`, `txSliceChanged(bool)`; `setDspMode` refuses only an unavailable RADE V2 mode and a held listener write, `SliceModel.cpp:667-676`, so SPEC and DRM are accepted). `RadioModel` (`stopAllTx(QString)` :5732, `requestTxHandoffToSlice(int sliceId)` :2431, `onBandButtonClicked(SliceModel*, Band)` :3211, `addNotchForSlice` :5653, `addTnfForSlice(SliceModel*)` :5664, `txBoundSlice()` :3708, `maxSlices()` :2436, `activeSlice()` :2408, `sliceAdded`/`sliceRemoved` :6413-6414, `bandPlanManager()` :2280). `TransmitModel` (`txEqEnabled`, `txLevelerOn`, `cfcEnabled`, `cpdrOn` and their `...Changed` signals; `setFilterLow`/`setFilterHigh`). `BandPlanManager` (`src/models/BandPlanManager.h`, `segments()`) and `BandSegment::label` (`src/models/BandPlan.h:26-32`), `src/models/Band.h`, `NotchModel`.
- Instruments: `src/gui/instruments/LongpathContent.cpp/.h`, `ReadingSource.cpp/.h`, `SMeterDialContent.cpp` (`applyConfig` :88, `configFrom` :114, `dialProperties` :129), `src/gui/SMeterWidget.h` (`TxMode { Power, SWR, Level, Compression }` :192).
- Containers: `ContainerContentHost` (`createWidget` call sites at `ContainerContentHost.cpp:479` and `:600`), `ContainerDocumentCodec.cpp` (`decodeEntry`), `ContainerWidget::contentBackground(const QColor&)` and `backgroundColor()` (`ContainerWidget.h:273-279`), `ContentProperty::Kind { Reading, Choice, Toggle, Text, Integer, Color }` (`ContainerContentRegistry.h`), `src/core/UnbuiltFeatureList.cpp`.
- Applets: `src/gui/applets/TxSectionContent.cpp`, `RxSectionContent.cpp`, `RxControlSections.cpp`, the seven `Tx*Section.cpp` files, `AppletVisibilityController.cpp`.
- Build and test plumbing: new tests register with `nereus_add_test(<name>)` in `tests/CMakeLists.txt`; ctest sets the test's `HOME`, `CFFIXED_USER_HOME` and `NEREUS_TEST_HOME` itself (`tests/CMakeLists.txt:434-453`). New sources are listed in the root `CMakeLists.txt` beside their neighbours (for example `src/gui/meters/MeterItem.cpp` :2100, `src/gui/containers/ContainerContentRegistry.cpp` :1995, `src/core/mmio/ExternalVariableEngine.cpp` :2034, `src/gui/applets/TxSectionContent.cpp` :2213).

## File Structure

### Create

| Path | Responsibility | Task |
|---|---|---|
| `tests/tst_container_item_persistence.cpp`, `tests/tst_container_item_tx_state.cpp` | stage 1 acceptance tests | 1, 2 |
| `src/gui/containers/ContentFade.h/.cpp` | the one fade mechanism: Thetis `fade()` step, keys, properties, widget overlay | 6 |
| `src/gui/meters/AboveS9Frequency.h` | VHF S9 rule (S9 = -93 dBm at or above 30 MHz) | 8 |
| `src/core/mmio/FourChar.h/.cpp` | Thetis `Common.FourChar` port | 10 |
| `src/gui/meters/ThetisReadingNames.h/.cpp` | binding id to Thetis reading name | 11 |
| `src/core/LedConditionExpression.h/.cpp` | GUI-free LED condition parser and evaluator | 26 |
| `src/gui/meters/LedResultRegistry.h/.cpp` | LED true/false results by 4-char ID | 40 |
| `src/gui/meters/OverlayTokens.h/.cpp` | text overlay token resolver and Thetis import translator | 39 |
| `src/gui/meters/BandGroups.h/.cpp` | shared GEN/HF/VHF band layouts | 35 |
| `src/gui/meters/OtherButtonCatalog.h/.cpp` | Thetis other-button id table and legacy map | 58 |
| `src/gui/containers/ContentEntryMigration.h/.cpp` | load-time entry migrations | 63 |
| `src/gui/applets/SectionParts.h/.cpp` | show, hide and order of TX/RX section parts | 64 |
| `tests/tst_*.cpp` named in each task | acceptance tests | 4-66 |

### Modify

| Path | Change | Task |
|---|---|---|
| `src/gui/meters/ClockItem.cpp`, `VfoDisplayItem.cpp`, `TextOverlayItem.cpp`, `FilterDisplayItem.cpp`, `ButtonBoxItem.cpp` (+ headers as needed) | serialize/deserialize the fields the editors already edit | 1 |
| `src/gui/meters/RotatorItem.cpp` | correct a wrong Thetis cite | 1 |
| `src/gui/containers/ContainerManager.cpp`/`.h`, meter widget plumbing | route transmit state to items that paint differently on TX | 2 |
| `src/gui/containers/ContentPropertyEditor.cpp` | no "Unsupported configuration" for entries with nothing to configure | 3 |
| `src/gui/meters/MeterItem.h/.cpp` | new-item hook, update interval, fade state, stage disable, S9 flag, MMIO variables, BAR/NEEDLE/TEXT/SCALE/IMAGE options, key and hover input | 4-9, 12, 16, 18, 36 |
| `src/gui/meters/MeterWidget.h/.cpp` | interval throttle, fade veil, MMIO delivery per variable, latest-value accessor, key and hover dispatch, click focus, click box actions | 5, 6, 9, 30, 36, 47 |
| `src/gui/containers/ContainerContentRegistry.cpp/.h` | hook call, context keys, new bar types, `control.*` ids, picker groups, `Kind::Order` | 4-6, 9, 14, 58, 64, 66 |
| `src/gui/containers/meter_property_editors/*ItemEditor.cpp` | editor rows for every new option | 5, 6, 12-66 |
| `src/gui/containers/ContentPropertyEditor.cpp` | preset rows, MMIO rows and 4-char IDs, Order control, Advanced bindings rule | 6, 9, 10, 13, 17, 22, 33, 64, 65 |
| `src/gui/meters/presets/*.cpp` | preset options, defaults, stage dim, two-trace history | 6, 7, 8, 13, 14, 17, 19, 20, 22, 23, 33 |
| Each item under `src/gui/meters/` (`SpacerItem`, `LEDItem`, `HistoryGraphItem`, `MagicEyeItem`, `SignalTextItem`, `NeedleScalePwrItem`, `DataOutItem`, `ClockItem`, `VfoDisplayItem`, `DialItem`, `TextOverlayItem`, `WebImageItem`, `FilterDisplayItem`, `RotatorItem`, `ClickBoxItem`, `ButtonBoxItem`, `BandButtonItem`, `NereusBandButtonItem`, `ModeButtonItem`, `TuneStepButtonItem`, `AntennaButtonItem`, `FilterButtonItem`, `OtherButtonItem`, `FadeCoverItem`) | that item's options, appended record fields, defaults | 6, 18-62 |
| `src/core/mmio/` (`MmioEndpoint`, `ITransportWorker`, the four workers, `ExternalVariableEngine`), `src/gui/containers/MmioEndpointsDialog.cpp` | 4-char IDs, direction, terminator, outbound queue | 10 |
| `src/gui/MainWindow.cpp` | foreign-session flag, S9 helper, stage-disable wiring, history trace 2, LED stop, item signals to models, hooks | 6, 7, 8, 22, 27, 30, 32, 35-37, 39, 42, 44, 48-52, 58, 60-62 |
| `src/gui/containers/ContainerManager.cpp/.h`, `FloatingContainer.cpp/.h`, `ContainerSettingsDialog.cpp`, `ContainerContentHost.cpp/.h` | container options, copy/paste, fade on widget rows, grouped picker | 29, 34, 52, 55, 66 |
| `src/gui/containers/ContainerButtonDispatcher.cpp/.h`, `ContainerControlCatalog.h`, `src/gui/widgets/MasterOutputWidget.cpp/.h` | other-button actions and hooks | 58-62 |
| `src/gui/instruments/LongpathContent.cpp/.h`, `ReadingSource.cpp/.h`, `SMeterDialContent.cpp/.h`, `src/gui/SMeterWidget.cpp/.h` | Longpath readings, S9, fade, second line; S-meter dial modes and per-copy options | 53-57, 63, 66 |
| `src/gui/SpectrumWidget.cpp/.h` | web image background override | 42 |
| `src/gui/applets/Tx*Section.cpp/.h`, `TxSectionContent.cpp`, `TxApplet.cpp`, `RxControlSections.cpp/.h`, `RxSectionContent.cpp`, `RxApplet.cpp` | section parts | 64, 65 |
| `src/gui/containers/ContainerDocumentCodec.cpp` | run load migrations | 63 |
| `src/core/UnbuiltFeatureList.cpp` | click box built | 47 |
| `CMakeLists.txt`, `tests/CMakeLists.txt` | new sources and tests | every task that creates one |
| `docs/attribution/THETIS-PROVENANCE.md`, `docs/attribution/LONGPATH-PROVENANCE.md` | rows for new ports | 6, 8, 10, 11, 35, 39, 53, 58 |

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
First task; configures `build/` if needed. Done: commit 7145b8506.

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

**Execution note (advisory):** opus. Reads transmit state, never sets it.
Prerequisite tasks: 1 (shared build dir); touches different files.

- [ ] **Step 1:** Test: toggle `ContainerManager::setTransmitting` on a container holding the three item types; assert each item's state and paint result.
- [ ] **Step 2:** Route the state; set initial state on creation and load.
- [ ] **Step 3:** Build, run, commit signed: `fix(containers): pass transmit state to fade cover, VFO display and button grids`.

## Task 3: TX section items show "Unsupported configuration"

**Requirements:** audit Part E summary item 5 and open question 6: the seven `tx.*`
items (`src/gui/applets/TxSectionContent.cpp`) have no options, and `ContentPropertyEditor.cpp:306-309`
labels them "Unsupported configuration". Same check for PR 635's `rx.*` items
(`src/gui/applets/RxSectionContent.cpp`).

**Files:**
- Modify: `src/gui/containers/ContentPropertyEditor.cpp` (the condition at about :306)
- Test: `tests/tst_tx_section_content.cpp`, `tests/tst_rx_section_content.cpp` (extend)

**Interfaces:**
- Consumes: `ContainerContentRegistry::declaredProperties(typeId)`, `isAvailable`, `unavailableReason`.
- Produces: nothing new.

**Acceptance:**
- Opening the property editor for any `tx.*` or `rx.*` entry shows no "Unsupported configuration" text. It shows the normal entry rows (name, geometry and whatever general rows every entry gets) and no item-specific group.
- An entry whose typeId the registry does not know at all (a layout from a newer build) still shows the registry's unknown-type text, "Unknown content type; original data retained".
- An entry with an `unavailableReason` still shows that reason.

**Verification:** risk tier: low (one condition, UI text). Build and run
`tst_tx_section_content` offscreen; add an `rx.*` case in `tests/tst_rx_section_content.cpp`.

**Execution note (advisory):** opus. No device or transmit path. Prerequisite tasks: 2.

- [ ] **Step 1:** Test the three cases above.
- [ ] **Step 2:** Make the condition tell "known type with no options" from "unknown type".
- [ ] **Step 3:** Build, run, commit signed: `fix(containers): TX and RX section items are not unsupported`.

## Task 4: New items take Thetis defaults

**Requirements:** ruling D5 (manager, 2026-10-25, JJ may veto): "new items get Thetis defaults, saved items keep their look." This task builds the shared hook and the first table (SOLID); every other item's table is added by that item's own task. Audit Part B SOLID row: Thetis `clsSolidColour` sets Black (`MeterManager.cs:7660-7664`); NereusSDR's `SolidColourItem` defaults to `#0f0f1a`.

**Files:**
- Modify: `src/gui/meters/MeterItem.h`/`.cpp` (add `virtual void applyNewItemDefaults()`, a no-op in the base; `SolidColourItem` overrides it)
- Modify: `src/gui/containers/ContainerContentRegistry.cpp` (`makeEntry`, :327-352: call the hook on the item that `allocate(typeId)` returns at :346, before the `properties` and `legacyRecord` values are taken; the `control.*` branch at :335-345 returns earlier and stays untouched)
- Create: `tests/tst_meter_new_item_defaults.cpp`, registered with `nereus_add_test`

**Interfaces:**
- Consumes: `ContainerContentRegistry::makeEntry` and its callers: `src/gui/MainWindow.cpp:8952`, `:10773`; `src/gui/applets/AppletVisibilityController.cpp:289`; `src/gui/meters/ItemGroup.cpp:114`; `src/gui/containers/ContainerSettingsDialog.cpp:1636`, `:1708`.
- Produces:
  - `virtual void MeterItem::applyNewItemDefaults()`.
  - The convention every later defaults task follows: a `constexpr` table `k<Item>ThetisDefaults` in the item's `.cpp`, each value cited `// From Thetis MeterManager.cs:<line> [v2.10.3.15]`; the item's override calls `MeterItem::applyNewItemDefaults()` first, then copies its table onto itself. Constructors keep today's values, so an old record, or a configuration with keys missing, looks as it does today.
  - `kSolidThetisDefaults` (colour Black, `MeterManager.cs:7663`).

**Acceptance:**
- `makeEntry("SOLID")`: the item restored from its `legacyRecord` has colour `QColor(Qt::black)`.
- `allocate("SOLID")` followed by `deserialize(<today's SOLID record literal, colour #0f0f1a>)` keeps `#0f0f1a`, and `createMeterItem` on an entry holding that record returns an item with `#0f0f1a`. The restore path never applies the hook to the item it returns (`createMeterItem`'s own name lookup through `makeEntry`, :419, only touches a temporary).
- A fresh SOLID entry from `makeEntry`, restored with `createMeterItem` and captured with `captureMeterItem` without edits, has an empty `overrides` object (the baseline at :519-529 deserializes the post-hook record).
- `makeEntry("control.mox")` produces exactly the record it produces today.

**Verification:** data-format adjacent; unit tests, test-first. Build and run per Global Constraints: `tst_meter_new_item_defaults`, `tst_container_item_persistence`, `tst_container_document`.

**Execution note (advisory):** opus. Prerequisite tasks: 1. No networking or transmit path.

- [ ] **Step 1:** Write the four acceptance tests; see the first fail.
- [ ] **Step 2:** Add the hook, the `makeEntry` call and the SOLID table.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): new items take Thetis defaults`.

## Task 5: Update interval on every item

**Requirements:** audit "Update interval" rows (Part A item-group level, Part B shared base). Thetis's group-level `UpdateInterval` (`setup.cs:25515`) becomes a per-item control (veto list: "Thetis group-level options (sub marker color, fade, update rate) become per-item controls"). Range 15-5000 ms, default 100 (`setup.designer.cs:55680-55698`). `BarPresetItem` and `CompositePresetItem` keep their own `updateIntervalMs` configuration keys; this task does not touch them.

**Files:**
- Modify: `src/gui/meters/MeterItem.h`/`.cpp` (`setUpdateIntervalMs(int)`, `updateIntervalMs()`, 0 meaning every poll; `bool acceptsUpdate(qint64 monotonicMs)`; the base `applyNewItemDefaults()` sets 100)
- Modify: `src/gui/meters/MeterWidget.cpp` (`updateMeterValue`: an item takes a value only when `acceptsUpdate` says its interval has passed; bar presets, composites and waveform faces are not throttled here)
- Modify: `src/gui/containers/ContainerContentRegistry.cpp` (context key `updateIntervalMs`: read in `createMeterItem` beside `onlyWhenRx`/`onlyWhenTx` at :409-410, written in `captureMeterItem` beside :545-546 only when non-zero; `makeEntry` writes it from the item after the hook, for item types other than the three preset kinds)
- Modify: `src/gui/containers/meter_property_editors/BaseItemEditor.cpp` (a "Limit update rate" check box and an "Update interval (ms)" spin box 15-5000, enabled while the box is checked; unchecked removes the key)
- Create: `tests/tst_meter_item_update_interval.cpp`, registered

**Interfaces:**
- Consumes: Task 4 (`applyNewItemDefaults`).
- Produces: `void MeterItem::setUpdateIntervalMs(int ms)`, `int MeterItem::updateIntervalMs() const` (0 = every poll), `bool MeterItem::acceptsUpdate(qint64 monotonicMs)`, `constexpr int kDefaultUpdateIntervalMs = 100` (cited).

**Acceptance:**
- With the interval at 500 ms and values pushed every 50 ms through `MeterWidget::updateMeterValue` under a fake clock, `setValue` runs at most once per 500 ms.
- A saved entry without the key updates on every poll, as today.
- A newly placed building block (`makeEntry("BAR")`) carries `updateIntervalMs` 100 in its context.
- `meter.signal` (a bar preset) behaves exactly as before; its entry has no `updateIntervalMs` context key.
- The editor clamps the spin box to 15-5000.

**Verification:** low-medium risk; unit tests with a fake clock. Build and run per Global Constraints: `tst_meter_item_update_interval`, `tst_meter_presets`.

**Execution note (advisory):** opus. Prerequisite tasks: 4. No networking or transmit path.

- [ ] **Step 1:** Write the throttle, absent-key, new-entry and preset tests.
- [ ] **Step 2:** Implement the throttle, the key and the editor rows.
- [ ] **Step 3:** Build, run, commit signed: `feat(meters): per-item update interval`.

## Task 6: Fade on RX / Fade on TX for every item (ContentFade)

**Requirements:**
- Rulings: veto list "port Thetis Fade on RX/TX and keep the hide option"; D4 (manager): the Thetis animated dim, keep `onlyWhenRx`/`onlyWhenTx` as they are, and the fade cover stays a user-placed block.
- Audit rows: "Fade on RX/TX" in Part A (every preset) and the Part B shared base; Part B FADECOVER; the Part C fade rows (CLOCK, VFO, DIAL, TEXTOVERLAY, WEBIMAGE, FILTERDISPLAY, ROTATOR).
- Item coverage (the union of both scout lists): every `MeterItem` type gets the option: building blocks, the seven Part C items, bar presets, composites (including `meter.powerSwr` and `meter.contest`) and waveform faces. Button grids keep the fade flags Task 1 stores in their records and gain the veil. FADECOVER runs the same ramp for itself and gets no veil on top.
- This task is the single fade mechanism. It creates `ContentFade`, which Task 55 attaches to widget-factory rows and Task 57 declares for S-meter dial copies.
- Thetis `fade()` (`MeterManager.cs:33153-33177`): dimmed level 48; `updateInterval = min(interval, 500)`; `steps_needed = ceil(500 / updateInterval)`; `stepSize = ceil(207 / steps_needed)`; the value steps down while `Disabled || (MOX && FadeOnTx) || (!MOX && FadeOnRx)`, otherwise up, clamped to 48..255; the early returns at :33156-33157 keep 255. Copy its inline comments verbatim, including `// [2.10.1.0] MW0LGE` (:33152), `// the level to dim to`, `// fade to take half a second` and `// 255-48 = 207`.
- Thetis `renderFadeCover` (`MeterManager.cs:37826-37845`): the cover is the container background colour at alpha `255 - fade`, over the item rect inflated by `max(2 px, 1%)` of its width and height.
- Tick interval: the item's own update interval (Task 5) when set, otherwise 100 ms (`kDefaultUpdateIntervalMs`), capped at 500. Thetis uses the container's quickest interval; NereusSDR's options are per item (veto list).
- Input: the veil takes no mouse input (Thetis has no fade guard on its VFO or filter items; only the button boxes have one, `MeterManager.cs:7913-7914`). Button grids keep their click guard (`ButtonBoxItem.cpp:270-271`, `:394-395`).
- Persistence: a `MeterItem` entry stores `fadeOnRx`/`fadeOnTx` in `entry.context` beside `onlyWhenRx`/`onlyWhenTx`, written only when the item's fade was set explicitly; absent means off, so a layout saved before this task stays byte-identical on re-save. A widget-factory entry keeps the same two keys in `entry.config`, where declared properties live (Task 55).
- Legacy composite fade: a composite whose configuration has `fadeRx`/`fadeTx` true and whose entry has no `fadeOnRx`/`fadeOnTx` context key keeps today's instant `p.setOpacity(.25)` (about `CompositePresetItem.cpp:1181`). Editing either fade row in the editor writes the context keys and sets the configuration's `fadeRx`/`fadeTx` to false, so from then on the composite uses the animated veil.
- Foreign session: an entry whose context names a session other than `MainWindow::containerSessionId()` is unavailable in this window (`ContainerSourceAdapter.cpp:29-31`), so its fade stays in the receive state and does not follow this window's transmit state.
- Fade cover option: FADECOVER appends a "use container background" field (absent = false keeps today's colour1/colour2 gradient times alpha).

**Files:**
- Create: `src/gui/containers/ContentFade.h`/`.cpp` (MeterManager.cs header byte for byte, as `src/gui/meters/FadeCoverItem.cpp` carries it, plus a Modification history block), registered in the root `CMakeLists.txt` beside `src/gui/containers/ContainerContentRegistry.cpp`; add a `docs/attribution/THETIS-PROVENANCE.md` row
- Modify: `src/gui/meters/MeterItem.h`/`.cpp` (fade state and accessors below; `advanceMeter` advances the fade)
- Modify: `src/gui/meters/MeterWidget.h`/`.cpp` (`setItemTxState`, added by Task 2, calls `MeterItem::setTransmitting` on every item; draw the veil per item on the CPU and GPU paths beside `drawUnavailableVeils`; `advanceMeters` repaints while a fade moves)
- Modify: `src/gui/meters/VfoDisplayItem.h`, `src/gui/meters/ButtonBoxItem.h`/`.cpp` (their `setTransmitting`, and ButtonBoxItem's `fadeOnRx`/`setFadeOnRx`/`fadeOnTx`/`setFadeOnTx`, become overrides of the base virtuals; grids keep their record fields)
- Modify: `src/gui/meters/FadeCoverItem.cpp`/`.h` (the 500 ms ramp in place of the instant `m_active`; the inflate; the background field)
- Modify: `src/gui/meters/presets/CompositePresetItem.cpp` (legacy 25% rule above)
- Modify: `src/gui/containers/ContainerContentRegistry.cpp` (read and write the context keys, :409-410, :545-546)
- Modify: `src/gui/MainWindow.cpp` (`refreshContainerMeter`: call `setForeignSession` on each item from the entry context)
- Modify: `src/gui/containers/meter_property_editors/BaseItemEditor.cpp` ("Fade on RX" and "Fade on TX" beside "Only when RX/TX"), `src/gui/containers/ContentPropertyEditor.cpp` (the same rows for bar presets and composites), `src/gui/containers/meter_property_editors/FadeCoverItemEditor.cpp` (the background option)
- Create: `tests/tst_meter_item_fade.cpp`, registered

**Interfaces:**
- Consumes: Task 2's `MeterWidget::setItemTxState(bool)` (fed by `ContainerManager::setTransmitting` and `meterReadyForPolling`); Task 5's `updateIntervalMs()` and `kDefaultUpdateIntervalMs`.
- Produces, in `namespace NereusSDR::ContentFade` (`ContentFade.h`):
  - `inline constexpr auto kFadeOnRxKey = "fadeOnRx";`, `inline constexpr auto kFadeOnTxKey = "fadeOnTx";`
  - `constexpr int kDimmedLevel = 48;` (From Thetis MeterManager.cs:33155)
  - `int nextValue(int current, bool fadeOnRx, bool fadeOnTx, bool transmitting, int updateIntervalMs);`, a port of one `fade()` step. A stage-disabled item (Task 7) passes `true` for both flags, which is Thetis's `Disabled` branch.
  - `QRectF coverRect(const QRectF& itemRect);` (the `max(2 px, 1%)` inflate)
  - `QColor coverColour(const QWidget* content);` (`ContainerWidget::contentBackground(container->backgroundColor())` of the enclosing `ContainerWidget`; the content's `palette().color(QPalette::Window)` when there is none)
  - `QVector<ContentProperty> properties();`, two `Toggle` properties "Fade on RX" and "Fade on TX" on the two keys, default false
  - `void attach(QWidget* content, const QJsonObject& config, ContentRenderMode mode);`: creates a `ContentFadeOverlay` child of `content`, transparent to the mouse, painting `coverColour` at alpha `255 - value()`, advancing by `nextValue` on a timer at `kDefaultUpdateIntervalMs`; nothing in `Preview` or `Validation` mode or with both keys off
  - `void setTransmitting(QWidget* content, bool transmitting);` (reaches the overlay `attach` created, if any)
  - `ContentFadeOverlay* overlayFor(QWidget* content);` and `int ContentFadeOverlay::value() const`, `void ContentFadeOverlay::advance()` (one step; the timer calls it, tests call it directly)
- Produces, on `MeterItem`: `virtual void setTransmitting(bool tx)`, `bool transmitting() const`, `virtual void setFadeOnRx(bool)`, `virtual void setFadeOnTx(bool)`, `virtual bool fadeOnRx() const`, `virtual bool fadeOnTx() const`, `virtual bool hasExplicitFade() const` (true once a setter ran; ButtonBoxItem and FadeCoverItem return false because their records hold the flags), `int fadeValue() const` (48..255), `void setForeignSession(bool)`.

**Acceptance:**
- `nextValue` at a 100 ms interval steps by 42 (`ceil(207 / 5)`): with Fade on TX and transmitting, 255 becomes 213, 171, 129, 87, 48 on five calls; with transmit off it climbs back to 255 in five calls. At a 50 ms interval the step is 21 and ten calls reach 48.
- A Fade on TX item under a fake clock at the 100 ms default: `fadeValue()` is 171 at 250 ms after transmit starts (between 48 and 255, not instant), 48 at 500 ms, and back to 255 500 ms after transmit ends. Fade on RX is the mirror image.
- The veil covers `coverRect` of the item in `coverColour` at alpha `255 - fadeValue()`; with both flags off nothing is drawn, on either paint path.
- A click on a faded CLOCK or VFO item still reaches it; a faded button grid with Fade on TX still ignores clicks while transmitting.
- Fade on RX together with `onlyWhenTx`: hidden while receiving; both options save independently.
- An item with `setForeignSession(true)` stays at 255 with Fade on TX while this window transmits.
- A layout saved before this task loads with fade off, paints pixel-identical, and re-saves byte-identical (no fade keys written).
- An entry carrying both keys round-trips through `captureMeterItem` and `createMeterItem` byte for byte.
- A saved composite with `fadeTx=true` and no context key still renders at 25% opacity while transmitting; after the editor's Fade on TX row is toggled, its entry has `fadeOnTx` in context and `fadeTx` false in configuration.
- FADECOVER: a record without the new field paints its gradient as before, apart from the ramp; with the field on, it paints the container background.
- `ContentFade::attach` with both keys off creates no overlay; with Fade on RX and receiving, `advance()` five times brings `value()` to 48; in `Preview` mode no overlay is created.

**Verification:** GUI behaviour plus persistence; unit tests with a fake transmit toggle and a fake monotonic clock, old-record tests first. Build and run per Global Constraints: `tst_meter_item_fade`, `tst_container_item_tx_state`, `tst_meter_composite_presets`, `tst_container_item_persistence`.

**Execution note (advisory):** opus. Prerequisite tasks: 2, 4, 5. Reads transmit state and never sets it. No networking.

- [ ] **Step 1:** Tests for `nextValue`, the item timing, the foreign session, the old record, the round trip, the legacy composite fade and the overlay.
- [ ] **Step 2:** Port `fade()` into `ContentFade` with its comments; add the overlay, `properties()`, `attach` and `setTransmitting`.
- [ ] **Step 3:** Add the base fade state, the context keys, the veil on both paint paths, the overrides on VfoDisplayItem and ButtonBoxItem, and `setForeignSession` in `refreshContainerMeter`.
- [ ] **Step 4:** Port the FADECOVER ramp, inflate and background option; add the editor rows.
- [ ] **Step 5:** Build, run, commit signed: `feat(meters): animated fade on RX/TX for every item`.

## Task 7: Dim TX-stage meters while the stage is off

**Requirements:**
- Audit Part A rows for EQ, Leveler, Leveler Gain, CFC, CFC Gain and Compressor (the "Disabled" dim).
- Thetis: `fade()` also dims when `mi.Disabled` (`MeterManager.cs:33153-33177`); `DisableMeterType` (`:26773-26792`) is driven by OnEQChanged (`:5313-5325`, EQ), OnLevelerChanged (`:5326-5339`, LEVELER and LEVELER_GAIN), OnCFCChanged (`:5340-5353`, CFC and CFC_GAIN) and OnCompandChanged (`:5354-5366`, COMP).
- Applies to the bar presets `meter.eq`, `meter.leveler`, `meter.levelerGain`, `meter.cfc`, `meter.cfcGain` and `meter.comp` (`BarPresetItem` flavours Eq, Leveler, LevelerGain, Cfc, CfcGain, Comp).

**Files:**
- Modify: `src/gui/meters/MeterItem.h`/`.cpp` (`setStageDisabled(bool)`; while set, the fade steps with both flags true)
- Modify: `src/gui/meters/presets/BarPresetItem.cpp`/`.h` (the flavour-to-stage map)
- Modify: `src/gui/MainWindow.cpp` (connect `TransmitModel::txEqEnabledChanged`, `txLevelerOnChanged`, `cfcEnabledChanged` and `cpdrOnChanged` to every container meter's matching items; set the state on `onContainerItemAdded`)
- Create: `tests/tst_meter_tx_stage_disabled.cpp`, registered

**Interfaces:**
- Consumes: Task 6 (`ContentFade::nextValue`, the veil, `setForeignSession`); `TransmitModel::txEqEnabled()`, `txLevelerOn()`, `cfcEnabled()`, `cpdrOn()` and their change signals (read only).
- Produces: `void MeterItem::setStageDisabled(bool)`, `bool MeterItem::stageDisabled() const`; `enum class TxStage { None, Eq, Leveler, Cfc, Compander }` and `TxStage BarPresetItem::txStage() const`.

**Acceptance:**
- TX EQ off: a `meter.eq` bar steps down to 48 over 500 ms whatever its fade options; EQ on: it recovers to 255.
- Leveler off dims `meter.leveler` and `meter.levelerGain`; CFC off dims `meter.cfc` and `meter.cfcGain`; compander off dims `meter.comp`. Other bars are unaffected.
- A matching bar added while the stage is off starts dimming at once.
- A foreign-session entry is never dimmed by this window's `TransmitModel`.
- Nothing writes to `TransmitModel`.

**Verification:** GUI state routing; unit tests with a `TransmitModel` instance. Build and run per Global Constraints: `tst_meter_tx_stage_disabled`.

**Execution note (advisory):** opus. Prerequisite tasks: 6. Reads TX settings, never writes them.

- [ ] **Step 1:** Tests for each of the four stage signals and the initial state.
- [ ] **Step 2:** Add the state, the map and the wiring.
- [ ] **Step 3:** Build, run, commit signed: `feat(meters): dim TX stage meters while the stage is off`.

## Task 8: VHF S9 rule per slice on every S-unit item

**Requirements:**
- Settled call: S9 = -93 dBm at or above 30 MHz, per slice, in dial, needle, bar and signal text.
- Thetis: `dbmOffsetForAboveS9Frequency` (`MeterManager.cs:5419-5424`) and `IsAboveS9Frequency` (`:5426-5429`); `_s9Frequency = 30.0` (`:358`); the test is "at or above" (`_rx1VHForAbove = ... >= _s9Frequency`, `:3342`, `:4508`); `getPerc` adds 20 dB to SIGNAL_STRENGTH and AVG_SIGNAL_STRENGTH only (`:41049-41052`); `GetSMeterUnits` uses 93 or 73 (`common.cs:881-887`); S0 is -153 (VHF) or -133 (`MeterManager.cs:2015-2030`).
- NereusSDR today: `MainWindow.cpp:15178` and `:15182` use `slice->frequency() > 30000000.0`, which misses exactly 30 MHz. The building blocks always use -73: `MeterItem::formatValue` (static, `MeterItem.h:214`), `NeedleItem::kS9Dbm` (`MeterItem.h:894`) and `SignalTextItem.cpp:212-217`.
- This task creates the shared helper; Tasks 54 and 56 consume it. No other file defines the rule (no helper in `ControlRanges.h`).

**Files:**
- Create: `src/gui/meters/AboveS9Frequency.h` (header only; the MeterManager.cs header byte for byte, as `src/gui/meters/FadeCoverItem.cpp` carries it, plus a Modification history block). Contents:
  - `constexpr double kS9FrequencyHz = 30.0e6;` (From Thetis MeterManager.cs:358)
  - `constexpr double kAboveS9OffsetDb = 20.0;` (:5419-5424)
  - `constexpr bool isAboveS9Frequency(double hz) { return hz >= kS9FrequencyHz; }` (:3342, :4508)
  - `inline double aboveS9Adjusted(int bindingId, double dbm, bool above)`: adds the offset only for `MeterBinding::SignalPeak` and `MeterBinding::SignalAvg` (:41049-41052)
- Modify: `src/gui/meters/MeterItem.h`/`.cpp` (`virtual void setAboveS9Frequency(bool)` with a stored flag; a `formatValue` overload taking the flag, with `-73.0f` and `-93.0f` as named `constexpr` S9 levels cited to `common.cs:881-887`; BarItem, TextItem and NeedleItem S-unit text and calibration use the flag)
- Modify: `src/gui/meters/SignalTextItem.cpp`, `src/gui/meters/MagicEyeItem.cpp` (S-unit text uses the flag)
- Modify: `src/gui/meters/presets/BarPresetItem.h`, `src/gui/meters/presets/CompositePresetItem.h` (their `setAboveS9Frequency` become overrides)
- Modify: `src/gui/MainWindow.cpp` (`applyTo` in `refreshContainerMeter`, :15177-15183: `isAboveS9Frequency(slice->frequency())`, called on every item, each entry resolved to its own slice, including Follow selected RX)
- Modify: `docs/attribution/THETIS-PROVENANCE.md` (a row for `AboveS9Frequency.h`)
- Create: `tests/tst_meter_vhf_s9.cpp`, registered

**Interfaces:**
- Consumes: `ContainerSourceAdapter::slice` (the entry's slice).
- Produces: `AboveS9Frequency.h` as above (`NereusSDR::kS9FrequencyHz`, `kAboveS9OffsetDb`, `isAboveS9Frequency(double)`, `aboveS9Adjusted(int, double, bool)`); `virtual void MeterItem::setAboveS9Frequency(bool)`, `bool MeterItem::aboveS9Frequency() const`; `static QString MeterItem::formatValue(float dBm, MeterUnit unit, bool decimal, bool aboveS9)`.

**Acceptance:**
- `isAboveS9Frequency(30.0e6)` is true; `isAboveS9Frequency(29999999.0)` is false.
- A BAR in S units reads "S9" for -93 dBm on a slice at exactly 30.000000 MHz; at 29.999999 MHz, -73 dBm reads "S9".
- Two entries in one container, on Slice A (14 MHz) and Slice B (50 MHz), each show their own scale.
- A Follow selected RX entry flips its scale when the selected slice changes band.
- `aboveS9Adjusted(MeterBinding::SignalPeak, -93, true)` is -73; for `MeterBinding::TxSwr` it returns the input.
- The presets keep their behaviour apart from the exactly-30 MHz fix.

**Verification:** medium risk (shared formatter); unit tests. Build and run per Global Constraints: `tst_meter_vhf_s9`, `tst_meter_presets`, `tst_meter_item_bar`.

**Execution note (advisory):** opus. Prerequisite tasks: 4. No transmit path.

- [ ] **Step 1:** Tests: the boundary, two slices, follow, `aboveS9Adjusted`.
- [ ] **Step 2:** Create the helper; add the flag and the overload; apply them in each item; move MainWindow to `isAboveS9Frequency`.
- [ ] **Step 3:** Provenance row. Build, run, commit signed: `fix(meters): VHF S9 rule per slice for every S-unit item`.

## Task 9: Second MMIO variable for two-variable meters

**Requirements:**
- Audit "Variable 2" and "MMIO variables" rows. Thetis `MeterVariables` (`MeterManager.cs:22684-22738`): 2 variables for ADC, AGC, MIC, ALC, EQ, LEVELER, COMP, CFC, CROSS, HISTORY and CUSTOM; 7 for ANANMM; otherwise 1 or 0.
- NereusSDR stores one pair, `mmioGuid`/`mmioVariable` (`ContainerContentRegistry.cpp:402`, `:539-540`).

**Files:**
- Modify: `src/gui/containers/ContainerContentRegistry.cpp` (context key `mmioVariables`: an array of `{guid, variable}` for index 1 and up; index 0 stays in the existing keys; absent = none)
- Modify: `src/gui/meters/MeterItem.h`/`.cpp` (per-index bindings; index 1 and up feed `setSecondaryValue`)
- Modify: `src/gui/meters/MeterWidget.cpp` (deliver MMIO values for every index, the way index 0 reaches `setValue` today)
- Modify: `src/gui/containers/ContentPropertyEditor.cpp` (about :408-416: one MMIO source and variable row per variable the type has)
- Create: `tests/tst_meter_mmio_variables.cpp`, registered

**Interfaces:**
- Consumes: the existing MMIO path behind `MeterItem::setMmioBinding(QUuid, QString)`.
- Produces: `void MeterItem::setMmioBinding(int index, const QUuid& guid, const QString& variableName)` (index 0 is the existing pair), `QUuid MeterItem::mmioGuid(int index) const`, `QString MeterItem::mmioVariable(int index) const`, `virtual void MeterItem::setSecondaryValue(int index, double value)` (no-op default), `static int MeterItem::mmioVariableCount(const QString& typeId)` (Thetis `MeterVariables`, cited).

**Acceptance:**
- `meter.agc` with variable 2 bound to an MMIO value shows that value as its secondary marker.
- An entry without `mmioVariables` behaves as today and saves no such key.
- The editor shows 7 MMIO rows for `meter.ananMulti`, 2 for `meter.agc` and 1 for `meter.signal`.
- Index 1 round-trips through capture and restore.

**Verification:** persistence; unit tests. Build and run per Global Constraints: `tst_meter_mmio_variables`.

**Execution note (advisory):** opus. Prerequisite tasks: 4. Reads MMIO values only.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** Add the key, the per-index binding and delivery, and the editor rows.
- [ ] **Step 3:** Build, run, commit signed: `feat(meters): second MMIO variable for two-reading meters`.

## Task 10: MMIO 4-character IDs, direction, terminator and outbound queue

**Requirements:**
- Ruling D7 (manager): Thetis 4-character IDs for data out, rotator, web image and overlay tokens.
- Port `Common.FourChar(string data1, int data2, Guid guid)` (`common.cs:1261-1287`): input `"data1:data2:guid"`, where `guid` is C#'s default `Guid.ToString()`, lowercase with hyphens and no braces (Qt: `guid.toString(QUuid::WithoutBraces)`); UTF-8, SHA-256, base64; then `convertToFourChar`: for each base64 character `i`, `indices[i % 4] = (indices[i % 4] + char) % 62` over `"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"`.
- Each `MmioEndpoint` gets an ID: network endpoints `FourChar(host, port, guid)` (`MeterManager.cs:41396`, `:41444`); serial endpoints per `:41461`; recomputed when the guid changes (`:41468`), computed lazily when missing (`:41484`), persisted with the endpoint.
- New endpoint fields, appended to its settings: `direction` In=0, Out=1, Both=2 (`MMIODirection`, `MeterManager.cs:41325-41330`); `terminator` None, CR, LF, CRLF or Custom with a custom string (`:41349-41356`). Defaults for a new endpoint (`:41409-41436`): direction In, terminator None, host 127.0.0.1, port 9000. An existing endpoint loads as In with no terminator (today's behaviour).
- Outbound queue: `enqueueOutbound` does nothing when the endpoint is disabled or its direction is In, and keeps at most 1000 entries (`MeterManager.cs:41606-41615`). Each worker drains up to 20 messages per send, joins them with the terminator and sends ASCII (`:41986-42020`).
- Lookup by ID: Thetis `GuidfromFourChar` (`MeterManager.cs:43194-43202`).
- UI: `MmioEndpointsDialog` shows each endpoint's ID with a Copy button and adds Direction and Terminator; the property editor's "MMIO source UUID" row (`ContentPropertyEditor.cpp:413`) also accepts a 4-char ID and stores the guid.

**Files:**
- Create: `src/core/mmio/FourChar.h`/`.cpp` (the `common.cs` header byte for byte, plus Modification history), registered in the root `CMakeLists.txt` beside `src/core/mmio/ExternalVariableEngine.cpp` (:2034)
- Modify: `src/core/mmio/MmioEndpoint.h`/`.cpp`, `ITransportWorker.h`/`.cpp`, `UdpEndpointWorker.cpp`, `TcpListenerEndpointWorker.cpp`, `TcpClientEndpointWorker.cpp`, `SerialEndpointWorker.cpp`, `ExternalVariableEngine.h`/`.cpp`
- Modify: `src/gui/containers/MmioEndpointsDialog.cpp`, `src/gui/containers/ContentPropertyEditor.cpp`
- Modify: `docs/attribution/THETIS-PROVENANCE.md`
- Create: `tests/tst_mmio_four_char.cpp`, registered

**Interfaces:**
- Consumes: the `ExternalVariableEngine` endpoint store and its settings load and save.
- Produces:
  - `QString NereusSDR::fourChar(const QString& data1, int data2, const QUuid& guid)`
  - `MmioEndpoint::fourChar()`, `enum class MmioEndpoint::Direction { In = 0, Out = 1, Both = 2 }`, `direction()`/`setDirection()`, `enum class MmioEndpoint::Terminator { None, Cr, Lf, CrLf, Custom }`, `terminator()`/`setTerminator()`, `customTerminator()`/`setCustomTerminator()`, `QStringList takeOutboundBatch(int max = 20)`, `int outboundQueueSize() const`
  - `MmioEndpoint* ExternalVariableEngine::endpointByFourChar(const QString& id) const`, `void ExternalVariableEngine::enqueueOutbound(const QUuid& guid, const QString& message)`
  - `constexpr int kMaxOutbound = 1000; constexpr int kSendBatch = 20;` (cited)

**Acceptance:**
- With guid `3f2504e0-4f89-11d3-9a0c-0305e82c3301`: `fourChar("webimage", 0, g)` is `"web4"`, `fourChar("ledindicator", 0, g)` is `"tCyp"`, `fourChar("127.0.0.1", 9000, g)` is `"pcWQ"` (computed from the Thetis algorithm above).
- An endpoint saved before this task loads as direction In with no terminator and gets an ID that is the same across reloads.
- `enqueueOutbound` on a direction-In endpoint leaves its queue empty.
- After 1005 enqueues on an Out endpoint the queue length is 1000.
- `takeOutboundBatch` returns at most 20 messages; with CRLF set, a worker sends them joined with `"\r\n"`.
- `endpointByFourChar(id)` returns the endpoint; an unknown ID returns nullptr.
- Rule R1: `tst_core_has_no_gui_includes` passes.

**Verification:** medium risk (networking workers). Build and run per Global Constraints: `tst_mmio_four_char`, `tst_core_has_no_gui_includes`.

**Execution note (advisory):** opus. Prerequisite tasks: none. Networking: it adds sends to the existing workers. No transmit path.

- [ ] **Step 1:** Write the FourChar test with the three literals, then port it with the header.
- [ ] **Step 2:** Add the endpoint fields, their persistence and the lazy ID.
- [ ] **Step 3:** Add the outbound queue and the worker drain.
- [ ] **Step 4:** Add the dialog column with Copy, and let the property editor accept a 4-char ID.
- [ ] **Step 5:** Provenance row. Build, run, commit signed: `feat(mmio): Thetis 4-char endpoint IDs, direction, terminator and outbound queue`.

## Task 11: Thetis reading names and clock readings

**Requirements:**
- `thetisReadingName` maps each `MeterBinding` id (`src/gui/meters/MeterPoller.h:154-204`) to the lowercase name of its Thetis `Reading` entry (`MeterManager.cs:79-160`, for example `SIGNAL_STRENGTH` → `signal_strength`, `SWR` → `swr`), empty when Thetis has none. Data out (Task 30), the LED condition (Task 26) and the overlay tokens (Task 39) use these names.
- `MeterItem::publishedReadings()`: named values an item publishes for data out and overlay tokens, empty by default.
- `ClockItem` publishes `time_utc`, `time_loc` (with `am`/`pm` appended when the clock is not in 24-hour mode), `date_utc` and `date_loc` (`ddd d MMM yyyy`), as Thetis does (`MeterManager.cs:31065-31099`). It takes an injectable time source for tests.
- The slice readings (`sliceA_freq` and the rest) are published by the VFO display in Task 32.

**Files:**
- Create: `src/gui/meters/ThetisReadingNames.h`/`.cpp` (MeterManager.cs header byte for byte plus Modification history; each table entry cited to its enum line), registered in the root `CMakeLists.txt` beside `src/gui/meters/MeterItem.cpp`
- Modify: `src/gui/meters/MeterItem.h`, `src/gui/meters/ClockItem.h`/`.cpp`
- Modify: `docs/attribution/THETIS-PROVENANCE.md`
- Create: `tests/tst_meter_published_readings.cpp`, registered

**Interfaces:**
- Consumes: `MeterBinding` ids.
- Produces: `QString NereusSDR::thetisReadingName(int bindingId)`; `int NereusSDR::bindingForThetisReadingName(const QString& name)` (-1 when unknown, case-insensitive); `virtual QHash<QString, QVariant> MeterItem::publishedReadings() const`; `void ClockItem::setTimeSource(std::function<QDateTime()>)`.

**Acceptance:**
- `thetisReadingName(MeterBinding::TxSwr)` is `"swr"`; `thetisReadingName(MeterBinding::SignalPeak)` is `"signal_strength"`; `bindingForThetisReadingName("SWR")` is `MeterBinding::TxSwr`.
- A clock with 24-hour mode off and a fixed local time of 13:05:09 publishes `time_loc` = `"1:05:09pm"`; with 24-hour mode on, `"13:05:09"`.
- `date_loc` for 2026-10-25 is `"Sun 25 Oct 2026"`.

**Verification:** low risk. Build and run per Global Constraints: `tst_meter_published_readings`.

**Execution note (advisory):** opus. Prerequisite tasks: none. No networking or transmit path.

- [ ] **Step 1:** Write the name table with cites.
- [ ] **Step 2:** Add the virtual and the clock implementation with the time source.
- [ ] **Step 3:** Provenance row. Build, run, commit signed: `feat(meters): Thetis reading names and clock readings`.

## Task 12: BAR building block: styles, peak hold, power limit and Thetis defaults

**Requirements:** audit Part B BAR rows marked missing or partial:
- Styles: Thetis `BarStyle` (`MeterManager.cs:21247-21254`). NereusSDR offers Filled, Edge and Line. Add Segments and Solid as `renderHBar` draws them (`MeterManager.cs:37424-37673`): segment geometry at `:37458-37467` (block width 2% of w, minimum 7, gap 0.2), bar height `h*0.85` at `:37461`, the high transition at `:37482`.
- A peak hold marker separate from the live marker (today `m_showMarker` drives both).
- Peak value colour (`MeterManager.cs:28278`); value text drawn at `:37609`, peak text at `:37641`.
- Ignore history 0-4000, default 2000 (`setup.designer.cs:55145-55164`).
- Power limit 0.1-5000, default 100 (`setup.designer.cs:55308-55326`), normalised `value *= 100f / mi.MaxPower` (`MeterManager.cs:41053-41056`).
- Editor controls for model fields that exist but have none today (show marker, sub marker, history, colours, calibration).

**Files:**
- Modify: `src/gui/meters/MeterItem.h`/`.cpp` (BarItem: paint about :644-812, serialize about :848-907, deserialize about :909-1022; append fields after field 31; add `Segments` and `Solid` after `Line` in `BarStyle`, leaving Filled, Edge and Line at their values)
- Modify: `src/gui/containers/meter_property_editors/BarItemEditor.cpp`
- Test: extend `tests/tst_meter_item_bar.cpp`

**Interfaces:**
- Consumes: Task 4 (hook), Task 5 (interval), Task 8 (S9 flag in S-unit text).
- Produces: `BarItem::BarStyle::Segments`, `BarItem::BarStyle::Solid`; setters and getters for peak hold marker, peak hold colour, peak value colour, ignore history ms, power limit; `kBarItemThetisDefaults` from the `clsBarItem` constructor (`MeterManager.cs:21284-21300`): history 2000, ignore 2000, value and peak shown, Line style, colour Red, high colour Orange, marker Yellow, peak hold Red, peak value Red, stroke 3.

**Acceptance:**
- Segments geometry on a 200×20 bar matches the Thetis numbers (block width `max(7, 4)` = 7 px, gap 0.2 of a block, bar height 17).
- Solid fills the low colour below the transition and the high colour above it.
- The peak hold marker can be on with the live marker off.
- Peak text uses the peak value colour.
- Power limit 50: a 25 W reading draws at the 50% calibration point.
- A 32-field record (today's `serialize()` output, as a literal) deserializes with every new field at its legacy value and paints as before.
- `makeEntry("BAR")` yields each table value.

**Verification:** data format plus paint; unit tests, old-record test first. Build and run per Global Constraints: `tst_meter_item_bar`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 5, 8.

- [ ] **Step 1:** Old-record, geometry and option tests.
- [ ] **Step 2:** Append the fields, add the styles, port the paint.
- [ ] **Step 3:** Editor rows and the defaults table. Build, run, commit signed: `feat(meters): BAR styles, peak hold, power limit and Thetis defaults`.

## Task 13: Bar presets: Thetis colour set, sub indicator and per-variant defaults

**Requirements:** audit Part A rows for `meter.mic`, `alc`, `comp`, `eq`, `leveler`, `cfc`, `cfcGain`, `levelerGain`, `alcGain`, `alcGroup`, `agc`, `agcGain`, `signal`, `signalAvg`, `signalMaxBin`, `adc`, `adcMax`, `pbSnr`, `customBar` and `sMeter`. The missing controls:
- Scale low and high colours (`MeterManager.cs:28310-28315`); paint hard-codes `Qt::white` and `Qt::red` today.
- Show indicator (`:28272`); peak hold indicator (`:28274`) and its colour.
- Segmented/solid low colour (`:28275`) and high colour (`:28276`), saved at `setup.cs:25531-25532`.
- Peak value colour (`:28278`).
- Sub indicator colour (`:28288`) and show (`:28289`), saved at `setup.cs:25511`, `:25513`; today the average marker is hard-coded `QColor(169,169,169)`.
- Show title; background (solid colour at `:28301`).
- Custom bar Thetis defaults: 0/10/7.5, "?" and "Title" (`MeterManager.cs:28251-28257`).

**Files:**
- Modify: `src/gui/meters/presets/BarPresetItem.cpp`/`.h` (configuration keys; an absent key keeps today's hard-coded paint; `applyNewItemDefaults` per variant)
- Modify: `src/gui/containers/ContentPropertyEditor.cpp` (the bar preset key list at about :348-357)
- Test: extend `tests/tst_meter_presets.cpp`

**Interfaces:**
- Consumes: Task 4; Task 6 (fade rows come from the base); Task 12 (Segments and Solid styles).
- Produces: the configuration keys `lowColor`, `highColor` (scale), `showMarker`, `peakHoldColor`, `segLowColor`, `segHighColor`, `peakValueColor`, `subColor`, `showSub`, `showTitle`, `backgroundColor`; `kBarPresetThetisDefaults`, one row per variant, each value cited to its builder: signal/avg/max bin `MeterManager.cs:22822/22826/22830`; ADC max `:22940`; ADC `:23063`; PB SNR `:23151`; AGC gain `:23222`; AGC `:23284`; custom `:23374`; mic `:24327`; EQ `:24414`; leveler `:24502`; leveler gain `:24588`; ALC `:24649`; ALC gain `:24735`; ALC group `:24796`; CFC `:24857`; CFC gain `:24943`; comp `:25004`.

**Acceptance:**
- Each new key round-trips through `configuration()` and `deserialize`.
- A saved preset without the keys paints identically, including the average marker at (169,169,169) and the white/red scale.
- Setting the sub indicator colour recolours the average marker; turning sub indicator show off hides it.
- A newly placed `meter.adc` gets marker Orange and history CornflowerBlue/128 (its builder, `MeterManager.cs:23063` onward).
- A newly placed `meter.customBar` gets 0/10/7.5, units "?" and title "Title".
- No typeId changes.

**Verification:** data format plus paint; unit tests. Build and run per Global Constraints: `tst_meter_presets`, `tst_meter_bar_face`.

**Execution note (advisory):** opus. Prerequisite tasks: 6, 12. Large and mechanical: work variant group by variant group.

- [ ] **Step 1:** Old-configuration and new-key tests.
- [ ] **Step 2:** Keys, paint and editor rows.
- [ ] **Step 3:** The per-variant table. Build, run, commit signed: `feat(meters): bar preset colours, sub indicator and Thetis defaults`.

## Task 14: Standalone PWR, Reverse PWR and SWR bars

**Requirements:**
- Settled call: standalone PWR, Reverse PWR and SWR bars beside the Power/SWR composite. These carry Thetis's PWR/SWR options (audit Part A `meter.powerSwr` rows); `meter.powerSwr` stays NereusSDR's composite.
- Thetis `AddPWRBar` (`MeterManager.cs:25185-25311`): attack .8 and decay .1 (`:25198-25199`); history 2000, shown (`:25201-25202`); marker Yellow (`:25203`); history Red/128 (`:25204`); Line style (`:25205`); calibration 0..120 (`:25231-25236`); `NormaliseTo100W` (`:25272`); high point index 4 (`:25276`); background (32,32,32) (`:25292`). PWR is created at `:25177`, Reverse PWR at `:25181`.
- `AddSWRBar` (`:25313-25375`): history Orange/128 (`:25332`); calibration 1, 1.5, 2, 3, 5 (`:25334-25338`); high point index 3 (`:25342`); background (`:25358`).
- Power limit through `MaxPower` (`:28292`).

**Files:**
- Modify: `src/gui/meters/presets/BarPresetItem.cpp`/`.h` (append `Pwr`, `ReversePwr`, `Swr` to `variants()`, giving typeIds `meter.pwr`, `meter.reversePwr`, `meter.swr`; a `powerLimitW` key)
- Modify: `src/gui/containers/ContainerContentRegistry.cpp` (descriptors at about :243-326; `allocate` already loops over `variants()`)
- Create: `tests/tst_meter_power_swr_bars.cpp`, registered

**Interfaces:**
- Consumes: Task 13 (colour keys); `MeterBinding::TxPower`, `TxReversePower`, `TxSwr`.
- Produces: the three typeIds and the `powerLimitW` configuration key.

**Acceptance:**
- Each new bar binds the same reading as the matching `meter.powerSwr` channel.
- PWR with power limit 100 and a 50 W reading draws at the 50 W calibration point.
- SWR 2.0 draws at the calibration point for 2.
- New placements get the Thetis values above.
- Existing `meter.powerSwr` layouts are unchanged.

**Verification:** new types; unit tests. Build and run per Global Constraints: `tst_meter_power_swr_bars`, `tst_meter_presets`.

**Execution note (advisory):** opus. Prerequisite tasks: 13. Reads TX meters only.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** Add the variants, descriptors and defaults.
- [ ] **Step 3:** Build, run, commit signed: `feat(meters): standalone power, reverse power and SWR bars`.

## Task 15: ADC bars read the ADC behind the chosen slice

**Requirements:**
- Settled call: the ADC bar reads the ADC behind the chosen slice. `meter.adc` reads the slice channel's RXA_ADC_PK/AV (`src/core/meters/SliceMeterPump.cpp:389-390`), which is per slice already; this task proves it with tests.
- `meter.adcMax` has no provider today (binding -1, `src/gui/meters/presets/BarPresetItem.cpp:118-121`). Thetis reads ADC_MAX_MAG from Protocol 2 high-priority bytes 35-38 (`console.cs:47057`, `:47221`; `netInterface.c:159-166`; `network.c:721-722`), which NereusSDR does not parse. Until a core provider exists (appendix), the bar shows the reason "ADC magnitude is not available" instead of a blank bar.

**Files:**
- Modify: `src/gui/meters/presets/BarPresetItem.cpp` (`AdcMax` calls `setBindingUnavailable` with the reason)
- Create: `tests/tst_meter_adc_slice.cpp`, registered

**Interfaces:**
- Consumes: `RadioModel::sliceAdcIndex(int)` (`RadioModel.h:2231`), in the test only.
- Produces: nothing new.

**Acceptance:**
- Two `meter.adc` entries on Slice A and Slice B, fed different ADC peak values, show their own values.
- A Follow selected RX entry switches when the selected slice changes.
- `meter.adcMax` draws the unavailable veil and hovering it gives the reason.

**Verification:** low risk; unit tests. Build and run per Global Constraints: `tst_meter_adc_slice`, `tst_container_follow_selected_rx`.

**Execution note (advisory):** opus. Prerequisite tasks: none.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** Add the unavailable reason.
- [ ] **Step 3:** Build, run, commit signed: `test(meters): ADC bars follow the chosen slice`.

## Task 16: TEXT, SCALE and IMAGE building blocks

**Requirements:** audit Part B rows.
- TEXT: `clsText` constructor (`MeterManager.cs:22259-22267`): font Trebuchet MS, Regular, White, not centred, size 9. Tokens `%valueWithType%` and `%value%` (`:22273-22290`, `:22305-22318`); `%group%` in `renderText` (`:37357-37386`). Missing in NereusSDR: font family, style, centring and the tokens.
- SCALE: `clsScaleItem` constructor (`:16118-16140`): low White, high Red, type DarkGray, size 20. The editor shows only tick and label colours although `ScaleItem` has low and high colours.
- IMAGE: `renderImage` (`:40629`) uses a dark key (`:40641-40642`), `-small` at a diagonal of 450 or less and `-large` at 1200 or more (`:40644-40650`).
- SOLID's defaults were added in Task 4.

**Files:**
- Modify: `src/gui/meters/MeterItem.h`/`.cpp` (TextItem: paint about :1389, serialize about :1420/:1432; ScaleItem; ImageItem; append fields)
- Modify: `src/gui/containers/meter_property_editors/TextItemEditor.cpp`, `ScaleItemEditor.cpp`, `ImageItemEditor.cpp`
- Create: `tests/tst_meter_item_text_image.cpp`, registered; extend `tests/tst_meter_item_scale.cpp`

**Interfaces:**
- Consumes: Task 4.
- Produces: `kTextThetisDefaults`, `kScaleThetisDefaults`, `kImageThetisDefaults`; `static QString ImageItem::variantPath(const QString& basePath, bool darkMode, double diagonalPx)`, which returns `<base>-dark`, `-small` or `-large` (in that precedence, per the cited render) only when that file exists, else `basePath`; `bool ImageItem::darkMode() const`/`setDarkMode(bool)`.

**Acceptance:**
- `%value%` renders the formatted reading, `%valueWithType%` adds the unit, and centring centres the text.
- The scale's low and high colours are editable and saved.
- With dark mode on, an image whose `-dark` file exists uses it; when the file is missing the base file is used. A 400 px diagonal picks `-small` when it exists.
- Old records look the same.

**Verification:** low-medium risk; unit tests (temporary image files for the variant lookup). Build and run per Global Constraints: `tst_meter_item_text_image`, `tst_meter_item_scale`.

**Execution note (advisory):** opus. Prerequisite tasks: 4.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** Append the fields, add the tokens and the variant lookup, add the editor rows.
- [ ] **Step 3:** The defaults. Build, run, commit signed: `feat(meters): text tokens, scale colours, image variants`.

## Task 17: ANAN multi, Cross needle and Power/SWR composite options

**Requirements:** audit Part A rows for `meter.ananMulti`, `meter.crossNeedle` and `meter.powerSwr`. Thetis applies these at `MeterManager.cs:27895-28035`:
- per-item power limit (`MaxPower`, `setup.cs:25545`, applied `MeterManager.cs:28292`); today the power scale is shared;
- power scale colour (`:28012-28014`, saved `setup.cs:25557`), ANAN and Cross only;
- peak hold for signal and PWR with its colour (`setup.cs:25534-25535`); peak value colour (`setup.cs:25541-25542`);
- sub needle colour taken from the sub marker colour; shadow (hard-coded today); dark mode (`:27997`, `setup.cs:25577`), ANAN and Cross only.
- `meter.powerSwr` gets the per-item power limit, peak hold colour and peak value colour keys (they live in `CompositePresetItem`); its bar style and segment colours stay with the standalone bars of Task 14.

**Files:**
- Modify: `src/gui/meters/presets/CompositePresetItem.cpp` (`m_config` keys, `editableFields` at about :553-567, paint)
- Modify: `src/gui/meters/presets/AnanMultiMeterItem.cpp`, `src/gui/meters/presets/CrossNeedleItem.cpp`, `src/gui/meters/presets/PowerSwrPresetItem.cpp`
- Test: extend `tests/tst_meter_composite_presets.cpp`

**Interfaces:**
- Consumes: Task 4; Task 9 (ANAN's 7 MMIO variables); Task 16 (`ImageItem::variantPath`); `MeterItem::setPowerScale(int limitWatts, double topWatts)`.
- Produces: configuration keys `powerLimitW`, `powerScaleColor`, `peakHoldColor`, `peakValueColor`, `subNeedleColor`, `shadow`, `darkMode`; `kAnanThetisDefaults` (`AddAnanMM`, `MeterManager.cs:23784`; VHF at `:23833`) and `kCrossThetisDefaults` (`AddCrossNeedle`, `:24140`).

**Acceptance:**
- Two ANAN entries with power limits 100 and 1000 scale independently.
- The peak hold marker uses its colour.
- Dark mode loads the face image's `-dark` variant when the file exists, else the normal image.
- `meter.powerSwr` with power limit 50 draws a 25 W reading at mid-scale.
- Old configurations without the keys paint the same.

**Verification:** paint plus persistence; unit tests. Build and run per Global Constraints: `tst_meter_composite_presets`, `tst_composite_main_power_scale`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 9, 16.

- [ ] **Step 1:** Old-configuration and option tests.
- [ ] **Step 2:** Keys, paint and editor rows.
- [ ] **Step 3:** The defaults tables. Build, run, commit signed: `feat(meters): ANAN, Cross and Power/SWR per-item options`.

## Task 18: NEEDLE and NEEDLESCALEPWR building blocks

**Requirements:** audit Part B NEEDLE and NEEDLESCALEPWR rows. The `clsNeedleItem` constructor (`MeterManager.cs:21942-21973`): shadow true, peak hold false, peak hold colour Red, history 500, ignore history 2000, colour White, stroke 3, attack .8 and decay .2. `renderNeedle` (`:40740`) draws history at about `:40785-40851` and peak hold at `:40885`. NereusSDR hard-codes the shadow (about `MeterItem.cpp:1670`) and the peak (about :1823/:1864); history fields exist but are not drawn; the power limit is unused. `clsNeedleScalePwrItem` constructor `:16229-16245`; power limit minimum 0.1 (`setup.designer.cs:55308-55326`).

**Files:**
- Modify: `src/gui/meters/MeterItem.h`/`.cpp` (NeedleItem paint about :1634-2040, serialize about :2042/:2078; append fields)
- Modify: `src/gui/meters/NeedleScalePwrItem.cpp`/`.h`
- Modify: `src/gui/containers/meter_property_editors/NeedleItemEditor.cpp`, `NeedleScalePwrItemEditor.cpp`
- Create: `tests/tst_meter_item_needle.cpp`, registered

**Interfaces:**
- Consumes: Task 4, Task 8.
- Produces: `kNeedleThetisDefaults`, `kNeedleScalePwrThetisDefaults`; NeedleItem setters for shadow, peak hold, peak hold colour, show history, history ms, ignore history ms, power limit.

**Acceptance:**
- Shadow off removes the shadow; peak hold on draws the peak marker in its colour.
- Show history draws the history arc for the duration set; ignore history drops samples for the time set.
- Power limit 50 normalises a 25 W reading to the 50% point.
- NEEDLESCALEPWR accepts a power limit of 0.1.
- An old record paints the same.

**Verification:** paint plus persistence; unit tests. Build and run per Global Constraints: `tst_meter_item_needle`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 8.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** Append the fields, port the paint, add the editor rows.
- [ ] **Step 3:** The defaults tables. Build, run, commit signed: `feat(meters): NEEDLE shadow, peak hold, history and power limit`.

## Task 19: Magic eye (preset and building block)

**Requirements:** audit Part A `meter.magicEye` and Part B MAGICEYE rows. Thetis applies these at `MeterManager.cs:27805-27893`: eye scale 0.01-1 (`setup.designer.cs:55518-55536`), bezel scale (`:55450-55468`), bezel image, show history and the history fields, colour from the marker colour, Max bin / Average. `AddMagicEye` (`MeterManager.cs:23572-23618`): attack .2 and decay .05 (`:23586-23587`), Lime (`:23589`), AVG (`:23590`), calibration -127→0, -73→.85, -13→1 (`:23591-23593`), VHF (`:23594`), bezel "eye-bezel" (`:23602`). `clsMagicEyeItem` constructor `:17188-17200`. NereusSDR has fixed smoothing.

**Files:**
- Modify: `src/gui/meters/MagicEyeItem.cpp`/`.h` (append attack/decay, history, eye scale, bezel scale, bezel image and calibration fields)
- Modify: `src/gui/meters/presets/MagicEyePresetItem.cpp`, `src/gui/meters/presets/CompositePresetItem.cpp` (keys)
- Modify: `src/gui/containers/meter_property_editors/MagicEyeItemEditor.cpp`
- Create: `tests/tst_meter_magic_eye.cpp`, registered

**Interfaces:**
- Consumes: Task 4; Task 8 (`aboveS9Frequency()` shifts the calibration by `kAboveS9OffsetDb`).
- Produces: `kMagicEyeThetisDefaults`.

**Acceptance:**
- Attack and decay change the response to a step input.
- Eye scale 0.5 halves the eye radius.
- -73 dBm sits at .85 below 30 MHz; -93 dBm sits at .85 at or above 30 MHz.
- Old records keep the fixed smoothing and paint the same.

**Verification:** paint plus persistence; unit tests. Build and run per Global Constraints: `tst_meter_magic_eye`, `tst_meter_composite_presets`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 8.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** Append the fields, the dynamics and the scale layout; add the editor rows.
- [ ] **Step 3:** The defaults. Build, run, commit signed: `feat(meters): magic eye scale, bezel, dynamics and calibration`.

## Task 20: Signal text (preset and building block)

**Requirements:** audit Part A `meter.signalText` and Part B SIGNALTEXT rows. Thetis applies at `MeterManager.cs:28147-28219`: show type and type colour, font colour from the marker colour, the µV/dBm sub lines from the sub marker colour, peak value colour, show sub marker, history and ignore history, background. `renderSignalTextDisplay` (`:40538-40627`) draws the PK/AVG/FFT glyph, the sub lines and the peaks. `AddSMeterBarText` (`:23001-23062`): solid (32,32,32) `:23012`, AVG `:23021`, attack .8 and decay .2 `:23023-23024`, history 4000 `:23026`, history colour Gray `:23028`, show peak value `:23030`, font Yellow `:23031`, size 56 `:23032`. `clsSignalText` constructor (`:21644-21670`): peak value Red `:21655`, show sub marker `:21658`, show type `:21667`, type colour Gray `:21668`. NereusSDR ties the peak value to peak hold.

**Files:**
- Modify: `src/gui/meters/SignalTextItem.cpp`/`.h` (append fields after the format at about :415-421)
- Modify: `src/gui/meters/presets/SignalTextPresetItem.cpp`, `src/gui/meters/presets/CompositePresetItem.cpp`
- Modify: `src/gui/containers/meter_property_editors/SignalTextItemEditor.cpp`
- Create: `tests/tst_meter_signal_text.cpp`, registered

**Interfaces:**
- Consumes: Task 4, Task 8.
- Produces: `kSignalTextThetisDefaults`.

**Acceptance:**
- The type glyph shows PK, AVG or FFT for the binding chosen (SignalPeak, SignalAvg, SignalMaxBin).
- The sub lines show µV and dBm in the sub colour.
- The peak value shows with peak hold off.
- The background fills.
- An old record paints the same.

**Verification:** paint plus persistence; unit tests. Build and run per Global Constraints: `tst_meter_signal_text`, `tst_meter_composite_presets`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 8.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** Append the fields and port the render; add the editor rows.
- [ ] **Step 3:** The defaults. Build, run, commit signed: `feat(meters): signal text type glyph, sub lines and colours`.

## Task 21: HISTORY building block: second axis feed and options

**Requirements:**
- Audit Part B HISTORY rows.
- Bug: `HistoryGraphItem` does not override `readingBindings`/`pushBindingValue` (`MeterItem.h:239-240`), so axis 1 (`setValue1`) is never fed.
- Thetis applies at `MeterManager.cs:26924-26990`: vertical ratio, back colour, update interval 50-10000, keep 1-86400, both readings, auto scale, manual min/max, show scale 1, per-axis MMIO, colours. Designer ranges: keep 1-1800 default 20 (`setup.designer.cs:62504-62529`), update 50-10000 default 100 (`:62534-62559`), ratio 0.130-1 (`:62565-62590`), min/max -40000..200 (`:62443-62500`).
- `clsHistoryItem` constructor (`MeterManager.cs:17528-17553`): ratio .5 `:17528`, back Black `:17529`, axis 0 Red `:17530`, axis 1 Yellow `:17531`, lines White `:17532`, time Gray `:17533`, readings SIGNAL_STRENGTH `:17536-17537`, keep 60 s `:17538`, manual -150/0 `:17545-17548`, auto true `:17552-17553`. The update interval default is 100 (designer), not the Apply clamp of 50.

**Files:**
- Modify: `src/gui/meters/HistoryGraphItem.cpp`/`.h` (override `readingBindings`/`pushBindingValue` for both bindings; append after `durationMs`: ratio, back colour, lines colour, time colour, manual min/max per axis, update interval)
- Modify: `src/gui/containers/meter_property_editors/HistoryGraphItemEditor.cpp` (the full reading list for each axis)
- Create: `tests/tst_meter_history_item.cpp`, registered

**Interfaces:**
- Consumes: Task 4; Task 9 (MMIO index 1 feeds axis 1 through `setSecondaryValue`).
- Produces: `kHistoryThetisDefaults`; axis 1 fed through the base reading path.

**Acceptance:**
- A HISTORY item with bindings 0 and 1 set records both traces from `MeterWidget::updateMeterValue`.
- Manual min/max is used when auto scale is off.
- An old 17-field record loads with the legacy look.

**Verification:** bug fix plus persistence; unit tests, axis-1 test first. Build and run per Global Constraints: `tst_meter_history_item`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 9.

- [ ] **Step 1:** A failing axis-1 feed test, then the option tests.
- [ ] **Step 2:** Fix the feed, append the fields, add the editor rows.
- [ ] **Step 3:** The defaults. Build, run, commit signed: `fix(meters): history second trace is fed; history options`.

## Task 22: History composite: two traces, each on its own slice

**Requirements:**
- Audit Part A `meter.historyGraph` rows; settled call: each trace of the two-trace history picks its own slice.
- Thetis `AddHistory` (`MeterManager.cs:25585-25612`); setup saves at `setup.cs:25017-25052`, loads at `:25680-25716`.
- `CompositePresetItem::applyConfiguration` rejects a channels array whose size changes (about :568-620); a saved 1-channel configuration must load and be padded to 2 with trace 2 off.

**Files:**
- Modify: `src/gui/meters/presets/HistoryGraphPresetItem.cpp`, `src/gui/meters/presets/CompositePresetItem.cpp` (accept and pad a 1-channel configuration)
- Modify: `src/gui/meters/HistoryGraphItem.cpp` (trace 2 takes its own context)
- Modify: `src/gui/MainWindow.cpp` (where container meters are fed: register trace 2 with `MeterPoller::addReadingConsumer(owner, context, bindings, replay)` and take `readingUpdated`, as `src/gui/instruments/SMeterDialContent.cpp:283-292` does, so trace 2's slice can differ from the entry's)
- Modify: `src/gui/containers/ContentPropertyEditor.cpp` (a Source for trace 2: Slice A-D or Follow selected RX)
- Create: `tests/tst_meter_history_two_trace.cpp`, registered

**Interfaces:**
- Consumes: Task 21; `MeterPoller::addReadingConsumer`, `MeterPoller::readingUpdated`, `MeterPoller::rxSourceChanged`.
- Produces: configuration key `trace2Context` (a context object; absent = trace 2 off).

**Acceptance:**
- Trace 1 on Slice A and trace 2 on Slice B record their own slice's signal.
- Trace 2 on Follow selected RX switches when the selected slice changes.
- An old 1-channel configuration loads without rejection and looks the same.
- A trace 2 context naming a foreign session shows the unavailable veil for that trace only.

**Verification:** medium risk (reading routing); unit tests with a `MeterPoller`. Build and run per Global Constraints: `tst_meter_history_two_trace`, `tst_meter_composite_presets`, `tst_container_follow_selected_rx`.

**Execution note (advisory):** opus. Prerequisite tasks: 21.

- [ ] **Step 1:** Tests, the old-configuration case first.
- [ ] **Step 2:** Pad the channels; route trace 2 through `addReadingConsumer`.
- [ ] **Step 3:** The editor Source. Build, run, commit signed: `feat(meters): two-trace history with a slice per trace`.

## Task 23: Clock preset: Thetis format for new clocks

**Requirements:**
- Audit Part A `meter.clock` rows and Part C open question 10. Thetis `renderClock` (`MeterManager.cs:40474`): 12-hour `h:mm:ss` plus a separate am/pm (`:40493-40494`), date `ddd d MMM yyyy` (`:40510-40511`), titles "local"/"utc" (`:40515-40516`); show 24 hour comes from show marker (`:28143`). `AddClock` `:25815`.
- Ruling D5: a newly placed `meter.clock` uses the Thetis format; a saved one keeps `yyyy-MM-dd` and today's time format (`CompositePresetItem.cpp:1225-1242`). The composite's NereusSDR extras (clock mode Both/UTC/Local, date toggle) stay.
- This task owns the composite clock; Task 31 owns the CLOCK building block.

**Files:**
- Modify: `src/gui/meters/presets/ClockPresetItem.cpp`, `src/gui/meters/presets/CompositePresetItem.cpp` (configuration key `thetisFormat`; absent = today's format; `applyNewItemDefaults` sets it true)
- Modify: `src/gui/containers/ContentPropertyEditor.cpp` (a "Thetis time and date format" row)
- Test: extend `tests/tst_meter_composite_presets.cpp`

**Interfaces:**
- Consumes: Task 4; Task 11 (`ClockItem::setTimeSource` for the fixed clock in tests).
- Produces: the `thetisFormat` key; `kClockPresetThetisDefaults`.

**Acceptance:**
- With `thetisFormat` true, 24 h off, at 13:05:09 local the clock draws "1:05:09" and "pm" as separate runs.
- The date for 2026-10-25 reads "Sun 25 Oct 2026".
- A saved configuration without the key keeps `yyyy-MM-dd` and today's time format.
- `makeEntry("meter.clock")` has `thetisFormat` true.

**Verification:** low risk; unit tests with a fixed clock. Build and run per Global Constraints: `tst_meter_composite_presets`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 11.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** The format, the key, the editor row and the defaults.
- [ ] **Step 3:** Build, run, commit signed: `feat(meters): Thetis clock format for new clock presets`.

## Task 24: Spacer: RX/TX colours and padding height

**Requirements:**
- Audit `meter.spacer` and Part B SPACER rows; Part A open question 4. `clsSpacerItem` constructor: colours (32,32,32) (`MeterManager.cs:17443-17444`), padding 0.1 (`:17446`). Apply at `:27608-27650`: size = width × padding. `renderSpacer` (`:36544-36576`) shows the RX colour while receiving and the TX colour while transmitting; on a MOX flip the fade resets to 48 and ramps up (the pattern at `:34017-34037`). Padding range 0.001-10 (`setup.designer.cs:63174-63192`).
- NereusSDR paints a gradient, and its comment at about `src/gui/meters/SpacerItem.cpp:70-80` wrongly calls that Thetis RX/TX. Ruling D3 keeps the gradient as a NereusSDR option.

**Files:**
- Modify: `src/gui/meters/SpacerItem.cpp`/`.h` (append "RX/TX colour mode", absent = gradient; append "padding sets height", absent = off; correct the comment)
- Modify: `src/gui/containers/meter_property_editors/SpacerItemEditor.cpp`
- Create: `tests/tst_meter_spacer.cpp`, registered

**Interfaces:**
- Consumes: Task 4; Task 6 (`MeterItem::transmitting()`, `ContentFade::nextValue`).
- Produces: `kSpacerThetisDefaults`: new placements take RX/TX mode on, colours (32,32,32), padding 0.1.

**Acceptance:**
- In RX/TX mode colour 1 shows while receiving and colour 2 while transmitting, ramping from 48 on each flip.
- With "padding sets height" on and padding 0.1, the spacer height is width × 0.1.
- An old record keeps the gradient.

**Verification:** low-medium risk; unit tests. Build and run per Global Constraints: `tst_meter_spacer`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 6.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** Append the fields, port the render, correct the comment.
- [ ] **Step 3:** The defaults and editor rows. Build, run, commit signed: `feat(meters): spacer RX/TX colours and padding height`.

## Task 25: LED options and Thetis defaults

**Requirements:** audit Part B LED rows. Thetis applies at `MeterManager.cs:27421-27512`: an update interval below 50 becomes 100 (`:27446`); light modes 0, 1 and 2; "process when hidden" (`:27490-27492`). Ranges: offset -10..10 (`setup.designer.cs:63956-63974`), size -10..10 (`:63894-63912`), update 50-10000 default 100 (`:63735-63753`), padding (`:63987-64005`). `clsLed` constructor (`MeterManager.cs:20825-20873`): true colour (0,255,0) `:20836`, false colour (192,192,192) `:20837`, show back panel `:20840`, offsets .5/.05 `:20841-20842`, size .05 `:20845-20846`, padding .1 `:20847`, show true/false `:20872-20873`. `renderLed` (`:34010-34130`) blinks or pulses at 250/100 ms. NereusSDR's green/amber/red thresholds stay (ruling D3).

**Files:**
- Modify: `src/gui/meters/LEDItem.cpp`/`.h` (append offset, size, show true, show false, process when hidden, light mode; `isTrue()`)
- Modify: `src/gui/containers/meter_property_editors/LedItemEditor.cpp`
- Create: `tests/tst_meter_led_options.cpp`, registered

**Interfaces:**
- Consumes: Task 4; Task 5 (`updateIntervalMs`).
- Produces: `kLedThetisDefaults`; `bool LEDItem::isTrue() const`: the LED's true state, which without a condition (Task 26) is "reading at or above the green threshold", the state in which today's LED is lit (`LEDItem.cpp:76-89`).

**Acceptance:**
- The blink period is 250 ms; the pulse step is 100 ms.
- With show false off, the LED is not drawn while `isTrue()` is false.
- An update interval of 20 becomes 100.
- An old record keeps threshold behaviour and looks the same.

**Verification:** low-medium risk; unit tests. Build and run per Global Constraints: `tst_meter_led_options`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 5.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** Append the fields and the render; add the editor rows.
- [ ] **Step 3:** The defaults. Build, run, commit signed: `feat(meters): LED options and Thetis defaults`.

## Task 26: LED condition expression

**Requirements:**
- Settled call: the Thetis condition beside the thresholds. Thetis compiles C# with Roslyn (`clsMeterScriptEngine.cs:325-381`; usings `:46-50`); placeholders `%name%` are expanded by `expand_placeholders` (`MeterManager.cs:21007-21066`) from reading names, custom strings such as `time_utc`, and MMIO variables (`provide_variables`, `:410-438`).
- NereusSDR ports a defined subset, not C#: numbers, double-quoted strings, `%var%` placeholders, `+ - * /`, comparisons (`< <= > >= == !=`), `&& || !` and parentheses. An expression outside the subset returns an error string; there are no exceptions. Thetis scripts that use other C# show that error. CAT variables are not offered (appendix).
- Variables: Thetis reading names (Task 11) resolved from the entry's slice readings; the clock names from Task 11; MMIO variables as `%<4char>.<variable>%`, resolved through `ExternalVariableEngine::endpointByFourChar`.

**Files:**
- Create: `src/core/LedConditionExpression.h`/`.cpp` (GUI-free, rule R1; NereusSDR-original, no port header), registered in the root `CMakeLists.txt` beside `src/core/mmio/ExternalVariableEngine.cpp`
- Modify: `src/gui/meters/LEDItem.cpp`/`.h` (append a condition field; absent = thresholds; `isTrue()` uses the condition when set)
- Modify: `src/gui/containers/meter_property_editors/LedItemEditor.cpp` (a condition text field with an inline error)
- Create: `tests/tst_led_condition_expression.cpp`, registered

**Interfaces:**
- Consumes: Task 10 (`endpointByFourChar`), Task 11 (`thetisReadingName`, `bindingForThetisReadingName`), Task 25 (`isTrue`).
- Produces: `struct LedConditionResult { bool ok; bool value; QString error; }`; `LedConditionResult evaluateLedCondition(const QString& expression, const std::function<std::optional<QVariant>(const QString& name)>& lookup)`; `QString LEDItem::condition() const`/`setCondition(const QString&)`.

**Acceptance:**
- `%SIGNAL_STRENGTH% > -73` is true for a slice reading -60 dBm and false for -80.
- `(%swr% >= 2) && !(%mic% < -20)` evaluates per its operands.
- `Math.Abs(%swr%)` returns `ok == false` with an error; the LED shows the error in the editor and stays false.
- An unknown placeholder gives an error naming it.
- An LED without a condition uses the thresholds as today.
- `tst_core_has_no_gui_includes` passes.

**Verification:** medium risk (a new parser); unit tests over a table of expressions. Build and run per Global Constraints: `tst_led_condition_expression`, `tst_meter_led_options`, `tst_core_has_no_gui_includes`.

**Execution note (advisory):** opus. Prerequisite tasks: 10, 11, 25.

- [ ] **Step 1:** Table tests.
- [ ] **Step 2:** The evaluator and the LED wiring.
- [ ] **Step 3:** The editor field. Build, run, commit signed: `feat(meters): LED condition expression`.

## Task 27: LED "No TX"

**Requirements:**
- Thetis `Update` (`MeterManager.cs:21177-21202`) calls `stopMox` (`:21205-21211`), which calls `StopAllTx("Led Indicator stopped MOX")`.
- Settled call: it stops only the radio that owns the entry's session: this window's radio, and only when the entry's context has no session or names `MainWindow::containerSessionId()`.

**Files:**
- Modify: `src/gui/meters/LEDItem.cpp`/`.h` (append the No TX field; a signal requesting a stop)
- Modify: `src/gui/MainWindow.cpp` (connect the signal in `onContainerItemAdded`; forward to `RadioModel::stopAllTx(const QString&)` with the Thetis text after the session check)
- Create: `tests/tst_meter_led_no_tx.cpp`, registered

**Interfaces:**
- Consumes: Task 26 (`isTrue` with the condition), Task 6 (`transmitting()`), `RadioModel::stopAllTx(const QString&)` (`RadioModel.h:5732`).
- Produces: `bool LEDItem::noTx() const`/`setNoTx(bool)`; signal `LEDItem::txStopRequested(const QString& message)`; a free function `bool NereusSDR::ledStopAppliesToSession(const QJsonObject& entryContext, const QString& currentSessionId)` in `LEDItem.h`, so the session rule is testable without a `MainWindow`.

**Acceptance:**
- An LED with No TX on whose `isTrue()` turns true while transmitting emits exactly one `txStopRequested("Led Indicator stopped MOX")` per transition to true.
- With No TX off, or while receiving, nothing is emitted.
- `ledStopAppliesToSession` is true for an empty or matching session and false for a foreign one; a foreign-session entry causes no `stopAllTx` call.
- In a local `RadioModel` test with MOX on, the forwarded stop leaves `isTransmitting()` false.

**Verification:** high risk (transmit path; it only ever stops). Unit tests; no device. Build and run per Global Constraints: `tst_meter_led_no_tx`.

**Execution note (advisory):** opus. Prerequisite tasks: 6, 26.
**TX-path flag:** this task can stop transmit (`RadioModel::stopAllTx`) from a container LED. It never keys.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** The field, the signal and the forwarding with the session check.
- [ ] **Step 3:** Build, run, commit signed: `feat(meters): LED can stop transmit on its own radio`.

## Task 28: TX peak readings in the building-block reading list

**Requirements:** audit "Reading" rows: `MeterBinding::TxMicPeak` through `TxCfcPeak` (113-118, `src/gui/meters/MeterPoller.h:195-200`) exist but are not offered in the building-block reading list.

**Files:**
- Modify: `src/gui/containers/meter_property_editors/BaseItemEditor.cpp`
- Create: `tests/tst_meter_reading_list.cpp`, registered

**Interfaces:**
- Consumes: nothing.
- Produces: nothing.

**Acceptance:** the reading list offers all six peak bindings, labelled "TX: Mic Peak", "TX: EQ Peak", "TX: Leveler Peak", "TX: ALC Peak", "TX: Compressor Peak" and "TX: CFC Peak" (no RX1/RX2 wording), and choosing one saves its id.

**Verification:** low risk. Build and run per Global Constraints: `tst_meter_reading_list`.

**Execution note (advisory):** opus. Prerequisite tasks: none.

- [ ] **Step 1:** Write the test.
- [ ] **Step 2:** Add the entries.
- [ ] **Step 3:** Build, run, commit signed: `feat(meters): TX peak readings in the reading list`.

## Task 29: Paste settings across compatible types

**Requirements:**
- Audit Part A item-group row "Copy / paste item settings" (same-type only today). Thetis `canPasteSettings` (`setup.cs:27085-27125`) allows pasting between compatible bar types and keeps the target's Unit and SubIndicators (`setup.cs:26988-27030`). Self-contained types paste only onto the same type: MAGIC_EYE, CROSS, ANANMM, SIGNAL_TEXT, SPACER, TEXT_OVERLAY, LED, ROTATOR, HISTORY, VFO_DISPLAY, CLOCK, FILTER_DISPLAY, CUSTOM_METER_BAR, WAVE_RECORD and VOICE_RECORD_PLAY_BUTTONS (`:27110-27125`).
- NereusSDR requires an exact type match (`src/gui/containers/ContainerSettingsDialog.cpp` about :2050-2070).

**Files:**
- Modify: `src/gui/containers/ContainerSettingsDialog.cpp`
- Create: `tests/tst_container_copy_paste.cpp`, registered

**Interfaces:**
- Consumes: Task 13 (bar preset keys); `BarPresetItem::variants()`.
- Produces: `static bool ContainerSettingsDialog::canPasteSettings(const QString& fromTypeId, const QString& toTypeId)`.

**Acceptance:**
- Settings copied from `meter.alc` paste onto `meter.mic` and keep the target's reading binding and units.
- `meter.magicEye` onto `meter.signalText` is refused; `meter.customBar` onto `meter.mic` is refused.

**Verification:** low risk. Build and run per Global Constraints: `tst_container_copy_paste`.

**Execution note (advisory):** opus. Prerequisite tasks: 13.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** Implement the rule.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): paste settings between compatible bar meters`.

## Task 30: Data out sends readings by slice on an interval

**Requirements:**
- A DATAOUT item sends on a timer. Interval 100-5000 ms, step 10. A new item uses 500 ms (`setup.cs:25273-25285`, `:26005-26018`); a saved record has no interval field and keeps the class's 1000 ms (`MeterManager.cs:17367-17431`). Today's record has 11 parts and no interval.
- Payload: the hosting `MeterWidget`'s latest binding values (`MeterWidget::m_lastBindingValue`, `MeterWidget.h:269`), named with `thetisReadingName`, plus the `publishedReadings()` of the other items in the same `MeterWidget`. Values use the Thetis `0.0#####` format. The three formats follow `MeterManager.cs:17367-17431`:
  - JSON: the Thetis `"rx"` key becomes `"slice":"A"`, and a `"radio"` field is added.
  - XML: `<data>`, with `<slice>` and `<radio>` in place of `<rx/>`.
  - RAW: a leading `radio:<id>:`, then `slice:A:<name>:<value>:` per reading.
- The slice letter comes from the entry's Source, resolved through `ContainerSourceAdapter::slice` as other items resolve it. `radio` is `MainWindow::containerSessionId()` (the MAC locally, the station fingerprint on a remote Core session). The field is always present and is an empty string when there is no session id.
- The target endpoint is chosen by 4-char ID through `ExternalVariableEngine::endpointByFourChar`; the record stores the endpoint guid. Sending goes through `ExternalVariableEngine::enqueueOutbound`, so a direction-In endpoint receives nothing.
- The existing Format, Transport and "Value name" options stay as NereusSDR options beside Thetis's.
- Constants, each cited: `kDataOutNewIntervalMs = 500` (`setup.cs:25283`, `:26017`), `kDataOutClassIntervalMs = 1000` (`MeterManager.cs:17367-17431`), `kDataOutMinMs = 100`, `kDataOutMaxMs = 5000`, `kDataOutStepMs = 10`.

**Files:**
- Modify: `src/gui/meters/DataOutItem.h`/`.cpp` (append interval and endpoint guid after field 10; the timer; `applyNewItemDefaults`)
- Modify: `src/gui/meters/MeterWidget.h`/`.cpp` (a const accessor for the latest binding values)
- Modify: `src/gui/containers/meter_property_editors/DataOutItemEditor.cpp`
- Modify: `src/gui/MainWindow.cpp` (give each DATAOUT item its slice letter and the session id in `onContainerItemAdded`/`refreshContainerMeter`)
- Create: `tests/tst_data_out_item_send.cpp`, registered

**Interfaces:**
- Consumes: Task 10 (`endpointByFourChar`, `enqueueOutbound`), Task 11 (`thetisReadingName`, `publishedReadings`), Task 4, `MainWindow::containerSessionId()`.
- Produces: `QString DataOutItem::buildPayload(const QHash<int, double>& readings, const QHash<QString, QVariant>& published, QChar sliceLetter, const QString& radioId) const` (pure; the timer and the tests call it); `QHash<int, double> MeterWidget::lastBindingValues() const`.

**Acceptance:**
- JSON for Slice A with SWR 1.5 and forward power 50: valid JSON containing `"slice":"A"`, `"radio"` and both Thetis reading names.
- RAW starts with `radio:<id>:` and contains `slice:A:swr:1.5:`.
- An endpoint with direction In receives nothing.
- The editor's interval spin box enforces 100-5000 in steps of 10.
- An old 11-part record loads with 1000 ms; `makeEntry("DATAOUT")` has 500 ms.

**Verification:** medium risk (networking through the Task 10 queue; no new wire code). Build and run per Global Constraints: `tst_data_out_item_send`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 10, 11. No transmit path.

- [ ] **Step 1:** Payload tests, then `buildPayload`.
- [ ] **Step 2:** The timer, the appended fields, the editor rows and the new-item default.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): data out sends Thetis-format readings by slice on an interval`.

## Task 31: CLOCK building block: Thetis layout and defaults

**Requirements:**
- A newly placed CLOCK gets Thetis's look: two background halves in (32,32,32) (`MeterManager.cs:25815-25859`); Trebuchet MS 18, time Orange, date Gray, title Gray, 24-hour on, title on (`MeterManager.cs:15409-15416`).
- Thetis layout (`renderClock`, `MeterManager.cs:40474-40528`), chosen by an appended `layout` field (0 = NereusSDR, 1 = Thetis): time `h:mm:ss` with a separate `am`/`pm` at 0.8× size and a pad of `w*0.03` when the time string is 7 characters; `local` title at x+1% and `utc` at x+52%, both at y+3%; time at 1.9× font size at x+12%; date `ddd d MMM yyyy`.
- New items get layout 1. Old records have no field, stay 0 and look the same.
- Background fill: an appended colour plus an enable flag.
- The title colour row is enabled only while the title is shown (`setup.cs:26468`).
- The composite clock's date format is Task 23's, not this task's.

**Files:**
- Modify: `src/gui/meters/ClockItem.h`/`.cpp` (append after the Task 1 colour fields 9-11)
- Modify: `src/gui/containers/meter_property_editors/ClockItemEditor.cpp`
- Create: `tests/tst_clock_item_layout.cpp`, registered; extend `tests/tst_container_item_persistence.cpp`

**Interfaces:**
- Consumes: Task 4; Task 11 (`ClockItem::setTimeSource`).
- Produces: `enum class ClockItem::Layout { NereusSDR = 0, Thetis = 1 }`, `setLayout`/`layout`, `setBackColour`/`backColour`, `setBackEnabled`/`backEnabled`; `kClockThetisDefaults`.

**Acceptance:**
- `makeEntry("CLOCK")` has layout Thetis and the table values.
- An old record loads with the NereusSDR layout and its colours unchanged.
- With a fixed time of 09:05:07 and 24-hour off, the Thetis layout draws "9:05:07" and "am" as separate runs and applies the pad.
- With the title hidden, the title colour row is disabled.

**Verification:** low risk. Build and run per Global Constraints: `tst_clock_item_layout`, `tst_container_item_persistence`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 11. No networking or transmit path.

- [ ] **Step 1:** Defaults table and tests.
- [ ] **Step 2:** Port the Thetis layout path; keep the NereusSDR path as it is.
- [ ] **Step 3:** Append the fields; editor rows. Build, run, commit signed: `feat(containers): Thetis clock layout and defaults for new clocks`.

## Task 32: VFO display: slice modes, colours, paint and published readings

**Requirements:**
- Display modes. `VfoDisplayItem::DisplayMode` (`VfoDisplayItem.h:71`, today `{VfoA, VfoB, VfoBoth}`) keeps its saved integers and is renamed:
  - `SourceSlice = 0` (was `VfoA`): the slice the entry's Source follows.
  - `LegacySliceB = 1` (was `VfoB`): load-only; it loads as fixed Slice B and re-saves as fixed Slice B (4).
  - `AllSlices = 2` (was `VfoBoth`): Slice A through the last open slice. A saved "Both" therefore loads as All slices.
  - Fixed slice: `3 + index` for Slice A+index, up to `RadioModel::maxSlices()`.
- The editor offers "Source slice", "All slices" and "Slice A" through the last possible slice, replacing "VFO A", "VFO B" and "Both" (`VfoDisplayItemEditor.cpp:61-66`). A new item starts on Source slice.
- Thetis import helper `fromThetisDisplayMode(int)`: BOTH=0 → All slices, A=1 → Slice A, B=2 → Slice B (`MeterManager.cs:14249-14254`; clamp `:28036-28115`). Nothing calls it yet (no Thetis item import exists; appendix).
- Paint (`MeterManager.cs:39857` onward):
  - The background colour replaces the hard-coded `#0a0a18` for new items; old records keep `#0a0a18`.
  - Titles read "Slice A" and so on in the title colour, in place of Thetis's "VFO A/VFO Sub/VFO B" (`:40248-40261`).
  - Small-digit colour, with a "Copy to small digits" button in the editor.
  - Split and split-back colours. The split box is drawn per `:40333-40335`, greyed with the reason "Split is not available.", because `RadioModel::split(int rx)` always returns false.
  - Digit highlight colour.
  - Band text from the `BandSegment::label` (`src/models/BandPlan.h:26-32`) of the segment holding the slice frequency, from `RadioModel::bandPlanManager().segments()`, in its own colour, drawn only when band text is on (Thetis `DB.BandText`, `console.cs:31657-31668`; drawn at `MeterManager.cs:40404-40417`).
  - Lock and sync icons (`:40436-40457`): lock reflects `SliceModel::locked()`; sync is greyed because NereusSDR has no VFO sync.
  - RX/TX boxes (`:40365-40388`); the transmitting slice shows the existing TX marker.
  - In All slices mode a slice that is not the active slice draws at alpha 24 (`:39883-39891`).
- Data: `MainWindow::refreshContainerMeter` feeds every slice the item shows.
- Published readings (moved here from Task 11): the item publishes `sliceA_freq`, `sliceA_mode`, `sliceA_filter`, `sliceA_band` and `sliceA_step`, and the same per letter, for each slice it shows. No `vfoa`/`vfob` names.
- Defaults table (`MeterManager.cs:14304-14347`): frequency and small digits Orange; mode, filter and title Gray; split back (64,64,64); split Orange; RX LimeGreen; TX Red; band White; digit highlight Gray; band text, lock and sync LimeGreen; band text off.

**Files:**
- Modify: `src/gui/meters/VfoDisplayItem.h`/`.cpp` (append the new colours and flags after the Task 1 colour fields 8-13)
- Modify: `src/gui/containers/meter_property_editors/VfoDisplayItemEditor.cpp`
- Modify: `src/gui/MainWindow.cpp` (`refreshContainerMeter`, :15030 onward)
- Create: `tests/tst_vfo_display_item_modes.cpp`, registered; extend `tests/tst_vfo_display_item_tx_badge.cpp`, `tests/tst_container_item_persistence.cpp`

**Interfaces:**
- Consumes: Task 4; Task 11 (`MeterItem::publishedReadings`); Task 2 transmit routing; `SliceModel`, `RadioModel::activeSlice()`, `RadioModel::maxSlices()`, `RadioModel::bandPlanManager()`, `RadioModel::split(int)`.
- Produces: `struct VfoSliceState { int index; qint64 frequencyHz; QString mode; QString filter; QString band; QString bandText; int stepHz; bool locked; bool transmitting; bool active; bool open; }`; `void VfoDisplayItem::setSliceState(int sliceIndex, const VfoSliceState&)`; `QVector<int> VfoDisplayItem::displayedSlices() const`; `static int VfoDisplayItem::fromThetisDisplayMode(int)`; `int VfoDisplayItem::fixedSliceMode(int sliceIndex)`; the `publishedReadings()` override.

**Acceptance:**
- A saved record with mode 2 loads as All slices; mode 1 shows Slice B and re-saves as 4.
- `fromThetisDisplayMode(1)` is the Slice A mode; `fromThetisDisplayMode(0)` is All slices.
- A new item has Source slice and the table colours; an old record keeps `#0a0a18`.
- With Slices A and B open and B active in All slices mode, A draws at alpha 24.
- Band text is drawn only when enabled; the split box is drawn greyed.
- `publishedReadings()` for Slice A at 14.200 MHz USB contains `sliceA_freq` and `sliceA_mode` = "USB".
- No editor label contains "VFO A", "VFO B", "RX1" or "RX2".
- The existing TX-badge test passes.

**Verification:** medium risk. Build and run per Global Constraints: `tst_vfo_display_item_modes`, `tst_vfo_display_item_tx_badge`, `tst_container_item_persistence`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 11. Display only; no transmit path.

- [ ] **Step 1:** Rename the enumerators with values unchanged; the import map and its test.
- [ ] **Step 2:** Per-slice state and the multi-slice paint, each region cited to its Thetis line.
- [ ] **Step 3:** Append the colours and flags; editor rows, including the copy button; published readings.
- [ ] **Step 4:** Feed every displayed slice from `refreshContainerMeter`. Build, run, commit signed: `feat(containers): VFO display by slice letter with Thetis colours and band text`.

## Task 33: VFO display preset colours and slice mode

**Requirements:**
- Audit Part A `meter.vfoDisplay` rows. Thetis applies them at `MeterManager.cs:28036-28095`: colours at `:28052-28070`; band text, small digits, lock and sync (GetSetting defaults LimeGreen and Orange); display mode at `:28072-28074`. `AddVFODisplay` `:25751`; `clsVfoDisplay` constructor `:14298`.
- The composite wraps a `VfoDisplayItem` (`m_vfo`); its display mode uses Task 32's modes (Source slice, All slices, Slice A-D) and the entry's Source picks the source slice.

**Files:**
- Modify: `src/gui/meters/presets/VfoDisplayPresetItem.cpp`, `src/gui/meters/presets/CompositePresetItem.cpp` (configuration keys: the Task 32 colours, background, title colour, display mode; absent = today's look)
- Modify: `src/gui/containers/ContentPropertyEditor.cpp`
- Test: extend `tests/tst_meter_composite_presets.cpp`

**Interfaces:**
- Consumes: Task 32 (`DisplayMode` values, colour setters, `setSliceState`).
- Produces: configuration key `displayMode` (Task 32 integers) and the colour keys; `kVfoPresetThetisDefaults`.

**Acceptance:**
- Display mode Source slice shows the entry's slice; All slices shows every open slice; Slice C shows C.
- The TX colour shows while the shown slice transmits.
- No user-visible "VFO A/B".
- An old configuration without the keys looks the same.

**Verification:** low-medium risk. Build and run per Global Constraints: `tst_meter_composite_presets`, `tst_vfo_display_item_tx_badge`.

**Execution note (advisory):** opus. Prerequisite tasks: 32. No transmit path.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** Keys, editor rows and defaults.
- [ ] **Step 3:** Build, run, commit signed: `feat(meters): VFO display preset colours and slice display mode`.

## Task 34: Button grids: column ranges, new-grid columns and copy/paste in the editor

**Requirements:**
- Button-box styling (on/off/fill/hover/click colours, fonts, icons, indicator styles) is not planned (ruling). Grids keep NereusSDR's look.
- The column count is clamped per grid type as `setup.cs:25814-25870`. New grids get 3 columns (`MeterManager.cs:25390`, `:25462`, `:25563`, `:25628`, `:25664`) through `applyNewItemDefaults`; saved grids keep theirs.
- Copy and Paste item settings work in the draft edit session. Today they are hidden on that path (`ContainerSettingsDialog.cpp:506`; actions at about `:656-675` and `:2058-2072`).
- Fade on RX/TX and the click guard are Task 6's.

**Files:**
- Modify: `src/gui/meters/ButtonBoxItem.h`/`.cpp`, each grid subclass (`BandButtonItem`, `NereusBandButtonItem`, `ModeButtonItem`, `FilterButtonItem`, `AntennaButtonItem`, `TuneStepButtonItem`, `OtherButtonItem`) for its range
- Modify: `src/gui/containers/meter_property_editors/ButtonBoxItemEditor.cpp`
- Modify: `src/gui/containers/ContainerSettingsDialog.cpp`
- Test: extend `tests/tst_container_settings_workflow.cpp`

**Interfaces:**
- Consumes: Task 4; Task 29 (`canPasteSettings`).
- Produces: `virtual std::pair<int, int> ButtonBoxItem::columnRange() const`.

**Acceptance:**
- A new mode grid has 3 columns; a saved mode grid with 5 keeps 5.
- The editor clamps to the type's range.
- Copy and Paste item settings appear and work in the draft edit session.

**Verification:** low risk. Build and run per Global Constraints: `tst_container_settings_workflow`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 29. No networking or transmit path.

- [ ] **Step 1:** Column ranges and new-grid columns, with tests.
- [ ] **Step 2:** Copy and Paste in the draft path, with a test.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): Thetis grid column ranges and copy/paste in the editor`.

## Task 35: Band grid: Thetis band groups shared by both band grids

**Requirements:**
- Button styling is not planned (ruling).
- Port the band groups and layouts (`MeterManager.cs:12932-13060`): GEN = the SWL broadcast bands `Band120m` to `Band11m` plus an HF jump at position 14; HF = 160 m to 6 m, a blank at 11, a VHF jump at 12, WWV at 13, SWL at 14; VHF = the VHF and transverter bands plus an HF jump at 14.
- Each slice keeps its own current group.
- One shared table in `BandGroups`, used by `BandButtonItem` and `NereusBandButtonItem`. `Band` values come from `src/models/Band.h` (`bandFromUiIndex`/`uiIndexFromBand`, `:226-229`).
- A click calls `RadioModel::onBandButtonClicked(SliceModel*, Band)`.
- XVTR stays hidden until Transverters is built. Right-click band stack stays gated with "Band stacking is not ready." (BandStack is unbuilt).
- Saved visible bits keep their meaning: when indices move, an appended layout marker selects the remap.

**Files:**
- Create: `src/gui/meters/BandGroups.h`/`.cpp` (MeterManager.cs header byte for byte plus Modification history), registered in the root `CMakeLists.txt` beside `src/gui/meters/OtherButtonItem.cpp`; a `docs/attribution/THETIS-PROVENANCE.md` row
- Modify: `src/gui/meters/BandButtonItem.h`/`.cpp`, `src/gui/meters/NereusBandButtonItem.cpp`
- Modify: `src/gui/containers/meter_property_editors/BandButtonItemEditor.cpp`
- Modify: `src/gui/MainWindow.cpp`
- Test: extend `tests/tst_radio_model_band_click.cpp`, `tests/tst_rx_section_content.cpp`

**Interfaces:**
- Consumes: Task 34; `Band.h`; `RadioModel::onBandButtonClicked(SliceModel*, Band)`.
- Produces: `enum class BandGroup { Gen, Hf, Vhf }`; `struct BandGroupSlot { Band band; enum Kind { Band, Blank, JumpGen, JumpHf, JumpVhf } kind; }`; `const QVector<BandGroupSlot>& bandGroupLayout(BandGroup)`; `BandGroup bandGroupForBand(Band)`; per-slice group state in `BandButtonItem` (`setGroup(int sliceIndex, BandGroup)`, `group(int sliceIndex)`).

**Acceptance:**
- The HF layout has WWV at 13 and SWL at 14; clicking SWL switches to GEN; the VHF jump switches to VHF.
- `NereusBandButtonItem` shows the same buttons.
- A saved band grid's visible bits keep their meaning.
- Slice A on GEN and Slice B on HF keep their own groups.

**Verification:** medium risk. Build and run per Global Constraints: `tst_radio_model_band_click`, `tst_rx_section_content`.

**Execution note (advisory):** opus. Prerequisite tasks: 34. Changes band through the model; nothing keys.

- [ ] **Step 1:** Layout tests, then the shared table.
- [ ] **Step 2:** Both grids on the table with per-slice groups and the remap marker.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): Thetis band groups shared by both band grids`.

## Task 36: VFO display: clicks, wheel, keys and screens by slice

**Requirements:**
- Hit boxes as a `constexpr` table of fractions of the item rect (`MeterManager.cs:39857` onward): band x 0.333-0.480, y 0.555-0.900; mode x 0.008-0.078; filter x 0.250-0.332; split x 0.100-0.200, y 0.030-0.430; TX x 0.152-0.200, lock 0.200-0.223, sync 0.224-0.250, all y 0.520-0.920; MHz 5 boxes x 0.216-0.330; kHz 3 boxes x 0.346-0.416; Hz 3 boxes x 0.425-0.480.
- Digits (`MeterManager.cs:14397-14583`): hover highlights a digit; wheel and left/right clicks step by that digit's weight; a typed digit with the pointer over a digit replaces it (`:14527-14559`); a locked slice refuses every change (`abortForLockedVFO`).
- Long press of 1000 ms or more (`:14626`): band → band stack, gated (BandStack is unbuilt); filter → the existing filter preset menu; kHz digits → the TUNE_STEP screen.
- Screens (state machine `:14631-14808`): short clicks open BAND, MODE and FILTER; each closes after more than 5 s without input (`:15282-15300`); BAND and MODE refuse on a locked slice.
- Actions, through existing setters:
  - band: `RadioModel::onBandButtonClicked(SliceModel*, Band)`, with Task 35's groups (`setBand`, `:15030-15210`);
  - mode: `SliceModel::setDspMode` (`:14991`);
  - filter: `FilterPresetStore` (`src/models/FilterPresetStore.h`) presets as `MainWindow::onContainerFilterClicked` (`MainWindow.cpp:15287`) applies them;
  - tune step: `SliceModel::setStepHz` through the 16-button map (`:14809-14877`): `constexpr int kVfoStepButtonToIndex[16] = {0,2,3,4,5,6,7,8,11,12,14,15,17,18,20,21};` into `kTuneStepList`;
  - TX: `RadioModel::requestTxHandoffToSlice(int sliceId)`. Thetis `setTX` (`:14905`) sets the TX VFO without MOX;
  - lock: `SliceModel::setLocked(!locked())` (`:14879`);
  - split and sync: not actionable; the tooltip gives the reason.
- Input plumbing: `MeterItem::handleKeyPress(QKeyEvent*)` and `MeterItem::handleHoverLeave()`; `MeterWidget` sets `Qt::ClickFocus`, forwards `keyPressEvent` to the item under the pointer, and sends hover-leave to the last hovered item on move-out and on `leaveEvent`. Typing works after the container has been clicked (Thetis needs only hover).

**Files:**
- Modify: `src/gui/meters/MeterItem.h`/`.cpp`, `src/gui/meters/MeterWidget.h`/`.cpp`, `src/gui/meters/VfoDisplayItem.h`/`.cpp`
- Modify: `src/gui/MainWindow.cpp` (connect the new signals in `onContainerItemAdded`, :14920)
- Create: `tests/tst_vfo_display_item_input.cpp`, registered

**Interfaces:**
- Consumes: Task 32 (`VfoSliceState`, `displayedSlices`); Task 35 (`bandGroupLayout`); `SliceModel::setLocked`, `setStepHz`, `setDspMode`; `RadioModel::onBandButtonClicked`, `requestTxHandoffToSlice(int)`.
- Produces: `virtual bool MeterItem::handleKeyPress(QKeyEvent*)` (false = not handled), `virtual void MeterItem::handleHoverLeave()`; `VfoDisplayItem` signals `frequencyChangeRequested(int slice, qint64 deltaHz)`, `frequencySetRequested(int slice, qint64 hz)`, `bandRequested(int slice, NereusSDR::Band)`, `modeRequested(int slice, NereusSDR::DSPMode)`, `filterRequested(int slice, int presetIndex)`, `tuneStepRequested(int slice, int stepHz)`, `txRequested(int slice)`, `lockToggleRequested(int slice)`.

**Acceptance:**
- A wheel step over the third kHz box emits a delta of +1000; on a locked slice nothing is emitted.
- Typing 7 over the 100 Hz digit replaces that digit.
- A 1100 ms press over kHz opens TUNE_STEP; button 8 sets the step to `kTuneStepList[11].stepHz`.
- A screen closes after 5 s without input.
- A TX click calls `requestTxHandoffToSlice` with the clicked slice's id and never calls a MOX setter.
- A split click emits nothing.

**Verification:** high risk (user input that changes the TX slice). Build and run per Global Constraints: `tst_vfo_display_item_input`, `tst_vfo_display_item_tx_badge`.

**Execution note (advisory):** opus. Prerequisite tasks: 32, 35.
**TX-path flag:** the TX box calls `RadioModel::requestTxHandoffToSlice`, which can move transmit to another slice and drop MOX. It never keys.

- [ ] **Step 1:** Key and hover-leave dispatch in `MeterWidget`, with a test.
- [ ] **Step 2:** Hit-box table, digit stepping and replacement, tests first.
- [ ] **Step 3:** Long press, screens and the 5 s timeout.
- [ ] **Step 4:** Wire the actions in `MainWindow`, refusing on a locked slice; disabled reasons for split, sync and band stack. Build, run, commit signed: `feat(containers): Thetis VFO display clicks, wheel, keys and screens by slice`.

## Task 37: Dial: settings and tuning

**Requirements:**
- Dial input (`MeterManager.cs:16908-17159`): dragging around the dial, or the wheel, tunes the chosen slice by one `stepHz()` per `degreesPerChange`; align rounds to the step; lock blocks tuning (the `Degrees` setter returns when locked).
- Acceleration: above `upThreshold` °/s the step moves to the next `kTuneStepList` entry every `interval` changes, at most `maxChanges` times; below `downThreshold` it moves down; up and down stay 90 apart (`:16829-16846`); the index wraps modulo the list length (`console.cs:6124-6134`); the step is restored on release (`undoTuneStep`, `:17062-17099`); speed is smoothed 0.95/0.05 after 4 frames (`:17104-17117`).
- Drive the existing `setSpeedState`, `setAccelEnabled` and `setLockEnabled` (no caller today).
- Slice select replaces Thetis VFOA/VFOB and "always show": the dial follows its Source slice by default, or "Any slice", which tunes the active slice; the top buttons step to the previous or next open slice.
- Appended settings: pad pressed colour (Thetis's 11th colour), vertical ratio 0.01-1, font scale 0.01-1.1, up/down thresholds 90-720, interval 1-10, max changes 1-30, degrees 1-90, align, accelerate, lock. Ranges per `MeterManager.cs:27732-27790`.
- Defaults from the constructor (`MeterManager.cs:16764-16806`): max changes 3, align true, up 540, down 360, interval 2, degrees 5, ratio 1.0, font scale 1.0, and the 11 colours in constructor order (White, Black, Blue, Orange, CornflowerBlue, Black, Gray, Gray, Blue, Green, Red), each bound to the `DialItem` colour named by its Thetis field at those lines. The constructor's 3 is used, not the setup default of 4 (`:27748-27772`).
- Old 17-part records load unchanged; `deserialize` still requires 17 or more parts.

**Files:**
- Modify: `src/gui/meters/DialItem.h`/`.cpp`
- Modify: `src/gui/containers/meter_property_editors/DialItemEditor.cpp`
- Modify: `src/gui/MainWindow.cpp` (connect the signals to `SliceModel`)
- Create: `tests/tst_dial_item_tuning.cpp`, registered

**Interfaces:**
- Consumes: Task 4; Task 36 (`MeterWidget` hover-leave dispatch); `SliceModel::stepHz`, `setStepHz`, `locked`; `kTuneStepList`, `kTuneStepListSize`, `tuneStepIndexForHz` (`SliceModel.h:338-380`).
- Produces: `DialItem` signals `tuneRequested(int slice, qint64 deltaHz)`, `stepChangeRequested(int slice, int hz)`, `sliceSelectRequested(int slice)`; `kDialThetisDefaults`.

**Acceptance:**
- A 25° drag at 5° per change with a 100 Hz step tunes +500 Hz.
- A locked slice gets no tune signal.
- A drag above 540 °/s moves the step up one entry; release restores the original step.
- Up threshold 400 with down 360 pushes down to 310.
- An old 17-part record round-trips; a new dial gets the table values.

**Verification:** medium risk. Build and run per Global Constraints: `tst_dial_item_tuning`, `tst_container_item_persistence`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 36. Tunes through `SliceModel`; nothing keys.

- [ ] **Step 1:** Tuning-maths tests from the cited setter, then the port.
- [ ] **Step 2:** Mouse and wheel handling, acceleration with undo, smoothing.
- [ ] **Step 3:** Slice select and the top buttons.
- [ ] **Step 4:** Appended settings, editor rows, defaults. Build, run, commit signed: `feat(containers): Thetis dial tuning with acceleration, lock and slice select`.

## Task 38: Text overlay: RX/TX swap, layout and fonts

**Requirements:**
- RX/TX swap (`MeterManager.cs:34135-34290`): line 1 shows on RX and line 2 on TX (`:34182`); Show RX and Show TX gate the item (`:34178-34179`). `LineMode` chooses between this and "Both lines" (today's look). New items use the swap; old records have no field and use Both lines.
- The panel back colour drops to 48 on each MOX flip and ramps to 255 over 500 ms (`:34147-34167`), using the `fade()` step of Task 6; the cover fade follows `do_cover_fade` (`:34146`).
- Layout: per-line X/Y offsets with copy buttons (`setup.cs:29692-29696`); "Copy font 1 to 2" (`setup.cs:31291-31298`); italic, underline and strikeout per font; padding as Thetis; the 2.1 factor (`:34185`); back fill (`:34255`); auto scroll when the text is wider than the item, speed `-0.15f` for new items (`:20154`).
- Defaults (`MeterManager.cs:20138-20176`): existing values already match except the scroll speed and padding 0.1.

**Files:**
- Modify: `src/gui/meters/TextOverlayItem.h`/`.cpp`
- Modify: `src/gui/containers/meter_property_editors/TextOverlayItemEditor.cpp`
- Create: `tests/tst_text_overlay_item.cpp`, registered; extend `tests/tst_container_item_persistence.cpp`

**Interfaces:**
- Consumes: Task 4; Task 6 (`MeterItem::setTransmitting`/`transmitting`, `ContentFade::nextValue`); Task 1's overlay fields.
- Produces: `enum class TextOverlayItem::LineMode { Both, RxTxSwap }`, `setLineMode`/`lineMode`, per-line offset and font-style setters, `copyFont1To2()`; `kTextOverlayThetisDefaults`.

**Acceptance:**
- In swap mode RX paints only line 1 and TX only line 2.
- An old record paints both lines as today.
- The panel alpha is 255 at 500 ms after MOX starts.
- "Copy font 1 to 2" copies family, size and styles.
- A text wider than the item scrolls; a text that fits does not.

**Verification:** low risk. Build and run per Global Constraints: `tst_text_overlay_item`, `tst_container_item_persistence`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 6. No networking or transmit path.

- [ ] **Step 1:** Swap and legacy paint tests; `LineMode` and the transmit handling.
- [ ] **Step 2:** Offsets, font styles, copy actions, the panel MOX ramp.
- [ ] **Step 3:** Append the fields and defaults. Build, run, commit signed: `feat(containers): Thetis text overlay RX/TX swap, offsets and fonts`.

## Task 39: Text overlay tokens

**Requirements:**
- Replace the `%`-split resolver with a token table, case-insensitive (`MeterManager.cs:20587-20716`). `%precis=N%` 0-20 and `%nl%` kept.
- Families:
  - reading tokens: every `thetisReadingName` name (for example `%swr%`) from the entry's bindings and slice;
  - time and date tokens as listed at `MeterManager.cs:788-840` and produced at `:1020-1250`. `time_utc_int`/`time_loc_int` are the integer `Hour*10000 + Minute*100 + Second` (09:05:07 → 90507, 13:05:09 → 130509). `date_utc_int`/`date_loc_int`: Thetis computes the same time integer (`MeterManager.cs:1044-1049`), an upstream bug; NereusSDR produces `yyyyMMdd` (2026-10-25 → 20261025), with a comment at the line citing `MeterManager.cs:1044-1049 [v2.10.3.15]` and saying why ours differs;
  - slice tokens: `%sliceA_freq%`, `%sliceA_mode%`, `%sliceA_filter%`, `%sliceA_band%`, `%sliceA_step%`, `%sliceA_open%` ("Slice A on" or empty), per letter; `%tx_slice%` (the TX slice letter); `%mox%` ("MOX" while transmitting);
  - MMIO tokens `%<4char>.<variable>%` through `ExternalVariableEngine::endpointByFourChar`.
- `translateThetisOverlayText(QString)` (for a future Thetis import; appendix) rewrites `%vfoa%` → `%sliceA_freq%`, `%vfob%` → `%sliceB_freq%`, `%rx2%` → `%sliceB_open%`, and the other `vfoa_*`/`vfob_*` forms the same way. The picker never offers Thetis names.
- A "%" button in the editor lists the offered tokens (`setup.cs:35280`). The token list is documented in the header comment of `OverlayTokens.h`.
- QSO timer, CAT and Discord tokens are out (appendix).

**Files:**
- Create: `src/gui/meters/OverlayTokens.h`/`.cpp` (MeterManager.cs header byte for byte plus Modification history), registered in the root `CMakeLists.txt` beside `src/gui/meters/OtherButtonItem.cpp`; a `docs/attribution/THETIS-PROVENANCE.md` row
- Modify: `src/gui/meters/TextOverlayItem.cpp`, `src/gui/containers/meter_property_editors/TextOverlayItemEditor.cpp`
- Modify: `src/gui/MainWindow.cpp` (supply slice state to overlays, as Task 32 does for VFO items)
- Create: `tests/tst_text_overlay_tokens.cpp`, registered

**Interfaces:**
- Consumes: Task 10 (`endpointByFourChar`), Task 11 (`thetisReadingName`, clock readings), Task 32 (`VfoSliceState`), Task 38.
- Produces: `struct OverlayContext { QHash<int, double> readings; QVector<VfoSliceState> slices; QChar txSlice; bool mox; QDateTime nowUtc; QDateTime nowLocal; }`; `QString resolveOverlayTokens(const QString& text, const OverlayContext&)`; `QString translateThetisOverlayText(const QString&)`; `QStringList offeredOverlayTokens()`.

**Acceptance:**
- `%SliceA_Freq%` resolves as `%slicea_freq%`.
- `%swr% %precis=1%` gives one decimal place.
- `%time_utc_int%` at 13:05:09 UTC is `130509`; at 09:05:07 it is `90507`.
- `%date_loc_int%` on 2026-10-25 is `20261025`.
- `translateThetisOverlayText("%VFOA% %rx2%")` is `"%sliceA_freq% %sliceB_open%"`.
- `offeredOverlayTokens()` contains no `vfoa`, `vfob` or `rx2` entry.

**Verification:** low risk. Build and run per Global Constraints: `tst_text_overlay_tokens`.

**Execution note (advisory):** opus. Prerequisite tasks: 10, 11, 32, 38. Reads the MMIO cache only; no transmit path.

- [ ] **Step 1:** Token tests, then port `parseText` with cites.
- [ ] **Step 2:** Slice family and the import translator.
- [ ] **Step 3:** Picker and header documentation. Build, run, commit signed: `feat(containers): text overlay tokens by slice letter`.

## Task 40: Text overlay LED gating, with the Thetis TX logic fixed

**Requirements:**
- `LEDItem` gains a 4-char ID, `fourChar("ledindicator", 0, guid)` (`MeterManager.cs:20829`, `:10407`), with the guid appended to its record. It publishes `isTrue()` to `LedResultRegistry` under that ID, the NereusSDR stand-in for `_led_results` (`:21189`).
- The overlay gains RX and TX LED gating (`MeterManager.cs:20529-20559`; setup keys `:27554-27558`): a 4-char ID per state plus an on/off logic flag; when a gate is set and the LED's result does not match the logic, that line is hidden.
- Upstream bug: the second block tests `_rx_led_logic` while reading `_tx_four_char` (`MeterManager.cs:20552-20559`), so the TX gate follows the RX logic flag. NereusSDR uses the TX logic flag, with this comment at the fix:
  `// From Thetis MeterManager.cs:20552-20559 [v2.10.3.15]: upstream tests _rx_led_logic for the TX LED;`
  `// NereusSDR uses the TX logic flag so the TX gate follows its own setting.`

**Files:**
- Create: `src/gui/meters/LedResultRegistry.h`/`.cpp` (NereusSDR-original), registered in the root `CMakeLists.txt` beside `src/gui/meters/OtherButtonItem.cpp`
- Modify: `src/gui/meters/LEDItem.h`/`.cpp`, `src/gui/meters/TextOverlayItem.h`/`.cpp`
- Modify: `src/gui/containers/meter_property_editors/LedItemEditor.cpp` (show and copy the ID), `TextOverlayItemEditor.cpp`
- Test: extend `tests/tst_text_overlay_item.cpp`

**Interfaces:**
- Consumes: Task 10 (`fourChar`); Task 27 (`LEDItem::isTrue`, the LED record as Task 27 leaves it); Task 38.
- Produces: `class LedResultRegistry { static LedResultRegistry& instance(); void set(const QString& id, bool value); std::optional<bool> get(const QString& id) const; }`; `QString LEDItem::fourChar() const`.

**Acceptance:**
- TX gate on LED `abcd`, TX logic on and RX logic off: the TX line shows while `abcd` is true (the case Thetis would hide).
- The RX gate works the same way.
- An LED's ID is the same after save and load.
- An unknown ID in a gate hides nothing.

**Verification:** low risk. Build and run per Global Constraints: `tst_text_overlay_item`, `tst_meter_led_options`.

**Execution note (advisory):** opus. Prerequisite tasks: 10, 27, 38. No networking or transmit path.

- [ ] **Step 1:** The divergence test first.
- [ ] **Step 2:** LED ID and registry, then the gates and the cited fix.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): text overlay LED gating with the Thetis TX logic fixed`.

## Task 41: Web image settings

**Requirements:**
- Interval: Thetis 30-7200 s (`setup.designer.cs:62677-62696`); NereusSDR's 1 s minimum stays as a NereusSDR option. New items get 120 s (constructor, `MeterManager.cs:15523-15561`); saved records keep their value (today `m_refreshInterval{300}`).
- Width scale (default 1); bypass cache; the known-host lock (some hosts force 600 s and turn bypass off, with `fixBSDWorldUrls` rewriting, `setup.cs:31492-31531`); a state label (loading, OK, error); presets.
- 4-char ID `fourChar("webimage", 0, guid)` (`MeterManager.cs:15528`) with a Copy button; the guid is appended to the record.
- Drawing keeps the aspect ratio; "No Image" when nothing has loaded. `file://` URLs load locally with no network request.

**Files:**
- Modify: `src/gui/meters/WebImageItem.h`/`.cpp`
- Modify: `src/gui/containers/meter_property_editors/WebImageItemEditor.cpp`
- Create: `tests/tst_web_image_item.cpp`, registered

**Interfaces:**
- Consumes: Task 4; Task 10 (`fourChar`).
- Produces: `QString WebImageItem::fourChar() const`, `QImage WebImageItem::currentImage() const`, `QString WebImageItem::stateText() const`; `kWebImageThetisDefaults` (interval 120 s, background interval 5 s, width scale 1.0).

**Acceptance:**
- A known-host URL forces 600 s and turns bypass off.
- A `file://` URL loads with no network request.
- With no image, "No Image" is painted.
- An old record keeps its interval (300 s); a new item has 120 s.

**Verification:** medium risk (network fetch through the existing path). Build and run per Global Constraints: `tst_web_image_item`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 10. No transmit path.

- [ ] **Step 1:** Host-lock and `file://` tests, then the port.
- [ ] **Step 2:** Settings, state label, ID, aspect drawing.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): Thetis web image settings, host lock and 4-char ID`.

## Task 42: Web image as a pan background, and the image chain

**Requirements:**
- A web image item can feed a pan's spectrum background (`MeterManager.cs:15578-15720`): a pan picker in the editor and an image override on `SpectrumWidget` beside the per-pan background fields (`SpectrumWidget.h:1130-1151`).
- Ruled 2026-10-26 (replaces an earlier "share an image" reading of the same lines): Thetis runs a timed background rotation. Each item names the next item's 4-char ID (`backgroundFourChar`) and holds the background for `backgroundIntervalSecs` (5-3600, default 5, `MeterManager.cs:27259`, `:15655-15670`) before handing over (`:15696-15706`). NereusSDR runs one rotation per pan (`targetPan`, -1 = none, the per-pan form of the Background check box); the editor's Show next selects the named item (`setup.cs:33212-33260`). A preview copy never joins a rotation. An old record without a guid gets a stable one derived from its record text, written on the next save.

**Files:**
- Modify: `src/gui/meters/WebImageItem.h`/`.cpp`, `src/gui/SpectrumWidget.h`/`.cpp`
- Modify: `src/gui/containers/meter_property_editors/WebImageItemEditor.cpp`
- Modify: `src/gui/MainWindow.cpp`
- Test: extend `tests/tst_web_image_item.cpp`

**Interfaces:**
- Consumes: Task 41.
- Produces: `void SpectrumWidget::setBackgroundImageOverride(const QImage&)` (a null image clears it), `int WebImageItem::targetPan() const`/`setTargetPan(int)` (-1 = none), `QString WebImageItem::sourceFourChar() const`/`setSourceFourChar(const QString&)`.

**Acceptance:**
- With a pan selected, the override receives the decoded image.
- Clearing the pan restores the pan's own background.
- An item naming another's ID shows that item's image.

**Verification:** medium risk (spectrum paint). Build and run per Global Constraints: `tst_web_image_item`.

**Execution note (advisory):** opus. Prerequisite tasks: 41. No transmit path.

- [ ] **Step 1:** Tests for the override and the chain.
- [ ] **Step 2:** Override setter, pan picker, chain and wiring.
- [ ] **Step 3:** Build, run, commit signed: `feat(spectrum): web image item can supply a pan background`.

## Task 43: Filter display settings, labels and the palette load fix

**Requirements:**
- Audit Part C FILTERDISPLAY rows, with Thetis ranges (`MeterManager.cs:27651-27705`): vertical ratio 0.15-1; show limits; RX/TX zoom 1-10 / 1-20; scales 0-10; display mode (default Panafall); font scale 0.01-4; sideband markers; frame interval 1-1000; grey when not active; snap and autozoom; characteristic; low limit -300 to -50. Missing colour rows; button labels MNF, +MNF, SNAP, FIT.
- Defaults (`MeterManager.cs:18316-18350`): vertical 0.2, limits on, frame 4 ms, grey when not active on, sideband/snap/autozoom/characteristic off, low -250; colours in Apply order LimeGreen, Black, White, DarkRed, Yellow, Red, White, Black, OrangeRed, LimeGreen, Gray; snap Gray, setting-on CornflowerBlue, button highlight Gray, each bound to its field name at those lines.
- Bug (`FilterDisplayItem.cpp:673`): `m_waterfallPalette = static_cast<WaterfallPalette>(waterfallFrameInterval < 0 ? 0 : waterfallPalette);` resets the palette whenever the frame interval is negative and never range-checks the palette. Fix: the palette loads as itself when within `WaterfallPalette` (0..6, `Enhanced` to `Custom`), else `Enhanced`, independent of the frame interval; the frame-interval rule at :674 stays.

**Files:**
- Modify: `src/gui/meters/FilterDisplayItem.h`/`.cpp`
- Modify: `src/gui/containers/meter_property_editors/FilterDisplayItemEditor.cpp`
- Test: extend `tests/tst_filter_display_high_resolution.cpp`, `tests/tst_container_item_persistence.cpp`

**Interfaces:**
- Consumes: Task 4; Task 1's filter display colours.
- Produces: setters and getters for each setting above; `kFilterDisplayThetisDefaults`.

**Acceptance:**
- A new item has the table values; an old record keeps its look.
- The editor enforces the ranges.
- A record with palette 2 and frame interval -1 loads as `BlackWhite` with frame interval 4; palette -1 or 7 loads as `Enhanced`.
- The existing high-resolution test passes.

**Verification:** low risk; the palette test first. Build and run per Global Constraints: `tst_filter_display_high_resolution`, `tst_container_item_persistence`.

**Execution note (advisory):** opus. Prerequisite tasks: 4. No networking or transmit path.

- [ ] **Step 1:** Failing palette test, then the fix.
- [ ] **Step 2:** Settings, appended fields, editor rows, defaults.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): Thetis filter display settings; fix palette load`.

## Task 44: Filter display: edge drags, notches and zoom

**Requirements:**
- Edges (`MeterManager.cs:19186-19378`): dragging an edge sets that edge; two-sideband modes mirror the change unless Shift is held; Shift-drag inside the passband shifts both edges; wheel per `:19013` onward. On RX the edges go to `SliceModel::setFilter(int low, int high)`; while transmitting to `TransmitModel::setFilterLow`/`setFilterHigh` (`TransmitModel.h:2284-2285`).
- Clicks (`MeterManager.cs:18858-19012`): right click top right resets the zoom; clicking a notch opens a notch menu; Ctrl+right adds a notch only when not transmitting; notch steps are 10 Hz, 1 Hz with Shift; MNF toggles TNF (`NotchModel::setGlobalEnabled`); +MNF adds a TNF when not transmitting (`RadioModel::addTnfForSlice`); SNAP snaps edges; FIT and autozoom set the zoom.
- Notches go through `RadioModel::addNotchForSlice(SliceModel*, double, double)` and `NotchModel::setCenter`/`setWidth`/`setActive`/`removeNotch` (by id). The item emits requests; `MainWindow` applies them.
- `UnbuiltFeature::ContainerFilterDisplay` is already reported built (`UnbuiltFeatureList.cpp:185`): no change.

**Files:**
- Modify: `src/gui/meters/FilterDisplayItem.h`/`.cpp`, `src/gui/MainWindow.cpp`
- Create: `tests/tst_filter_display_item_input.cpp`, registered

**Interfaces:**
- Consumes: Task 43; Task 6 (`transmitting()`); the setters above.
- Produces: `FilterDisplayItem` signals `filterEdgesRequested(int slice, int lowHz, int highHz, bool tx)`, `notchAddRequested(int slice, double hz)`, `notchChangeRequested(int id, double hz, double widthHz)`, `notchRemoveRequested(int id)`, `tnfToggleRequested()`, `tnfAddRequested(int slice)`.

**Acceptance:**
- Dragging the USB high edge +100 Hz emits a request with only `high` changed.
- In AM the low edge mirrors unless Shift is held.
- While transmitting the request has `tx = true` and `MainWindow` calls `TransmitModel`, not `SliceModel`.
- Ctrl+right while transmitting adds no notch; +MNF while transmitting adds nothing.
- MNF toggles the global TNF.

**Verification:** high risk (changes the TX filter during MOX). Build and run per Global Constraints: `tst_filter_display_item_input`.

**Execution note (advisory):** opus. Prerequisite tasks: 43.
**TX-path flag:** edge drags while transmitting change the TX filter through `TransmitModel::setFilterLow`/`setFilterHigh`. It never keys.

- [ ] **Step 1:** Input tests, then port the edge setters and mouse-up with cites.
- [ ] **Step 2:** Wire the requests in `MainWindow`, keeping the not-transmitting guards.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): Thetis filter display edge drags, notches and zoom`.

## Task 45: Rotator settings

**Requirements:**
- Update rate (new items 100 ms, `MeterManager.cs:23466`, `:23490`); full-background option; beam width 0-180 in 0.05 steps, disabled when the beam is off; padding used in the layout; dark mode as Thetis (`Apply`, `:27298-27345`); an ELE MMIO source (endpoint 4-char ID plus variable).
- The Thetis skin images are not in its repository (`ImageName`, `:16507-16521`); the image path stays user-chosen (appendix).
- Existing defaults already match Thetis (`MeterManager.cs:16402-16437`); `applyNewItemDefaults` sets only the update interval.

**Files:**
- Modify: `src/gui/meters/RotatorItem.h`/`.cpp` (append after field 21)
- Modify: `src/gui/containers/meter_property_editors/RotatorItemEditor.cpp`
- Test: extend `tests/tst_rotator_item_drag.cpp`

**Interfaces:**
- Consumes: Task 4, Task 5, Task 10 (`endpointByFourChar`).
- Produces: setters and getters for the settings above.

**Acceptance:**
- A 22-part record round-trips unchanged.
- The beam-width row is disabled when the beam is off.
- A new item updates at 100 ms.

**Verification:** low risk. Build and run per Global Constraints: `tst_rotator_item_drag`.

**Execution note (advisory):** opus. Prerequisite tasks: 4, 10. No transmit path.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** Append the fields; editor rows and defaults.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): Thetis rotator settings`.

## Task 46: Rotator MMIO command path

**Requirements:**
- "Allow control" (default off) and a control colour (default LimeGreen) (`MeterManager.cs:16402-16437`).
- When control is on and the command path is "MMIO", a click or drag sends the AZ, ELE or STOP string to the output endpoint chosen by 4-char ID through `ExternalVariableEngine::enqueueOutbound` (`SendRotatorMessage`, `MeterManager.cs:16475-16505`): AZ formatted `"000"`, ELE `"00"`, escapes expanded, defaults the pstRotator strings from the constructor.
- The existing `RotatorItem::setRotor(RotorLink::RotorModel*, RotorCommandSink*)` path stays the default. The command path is chosen per item: "Rotor link" or "MMIO".

**Files:**
- Modify: `src/gui/meters/RotatorItem.h`/`.cpp`
- Modify: `src/gui/containers/meter_property_editors/RotatorItemEditor.cpp`
- Create: `tests/tst_rotator_item_mmio.cpp`, registered

**Interfaces:**
- Consumes: Task 10 (`enqueueOutbound`, `endpointByFourChar`), Task 45.
- Produces: `enum class RotatorItem::CommandKind { Azimuth, Elevation, Stop }`; `QString RotatorItem::buildRotatorMessage(CommandKind, double value) const` (pure).

**Acceptance:**
- AZ 7° with the default string gives the pstRotator message containing `007`.
- With control off nothing is enqueued.
- With the path "Rotor link", the existing sink is used and nothing is enqueued.

**Verification:** medium risk (networking; moves a physical rotor). Build and run per Global Constraints: `tst_rotator_item_mmio`, `tst_rotator_item_drag`.

**Execution note (advisory):** opus. Prerequisite tasks: 10, 45. Hardware motion note: this can turn an antenna rotor through MMIO; it is not transmit.

- [ ] **Step 1:** Message test, then port `SendRotatorMessage`.
- [ ] **Step 2:** Path choice, editor rows, wiring.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): Thetis rotator MMIO command path beside the rotor link`.

## Task 47: Click box actions

**Requirements:**
- A mouse button mask for left, right and middle (default all three) and these actions (`MeterManager.cs:7597-7652`, `:29814-29848`):
  - go to a display group;
  - change group: left goes to the next group, right to the previous (`incDisplayGroup`/`decDisplayGroup`, `:31165-31186`), through `MeterWidget::setDisplayGroup`; the cycle wraps at the highest group in use;
  - cycle units on the topmost item under the click box that supports units (Thetis `HandleIncrement`, `clsBarItem` `:21539`, `clsSignalText` `:21836`), through `MeterItem::setUnitMode`; order follows `SignalTextItem::Units` (`Dbm, SUnits, Uv`, `MeterManager.cs:20288`).
- The click box stays user-addable.
- Mark `UnbuiltFeature::ContainerClickBox` built (`UnbuiltFeatureList.cpp:155-156`, `isBuilt` `:183-187`).

**Files:**
- Modify: `src/gui/meters/ClickBoxItem.h`/`.cpp`, `src/gui/meters/MeterWidget.cpp`
- Modify: `src/gui/containers/meter_property_editors/ClickBoxItemEditor.cpp`
- Modify: `src/core/UnbuiltFeatureList.cpp`
- Create: `tests/tst_click_box_item.cpp`, registered

**Interfaces:**
- Consumes: `MeterWidget::setDisplayGroup(int)`, `MeterItem::setUnitMode(MeterUnit)`.
- Produces: `enum class ClickBoxItem::Action { GoToGroup, ChangeGroup, CycleUnits }`, `setButtonMask(Qt::MouseButtons)`/`buttonMask()`, signal `actionRequested(ClickBoxItem::Action, int value)`; `kClickBoxThetisDefaults` (left, right, middle on; change group on).

**Acceptance:**
- A right click with the right button masked off does nothing.
- With change group on, a left click moves the group from 0 to 1.
- A unit cycle over a signal text item moves dBm → S → µV.
- `isBuilt(UnbuiltFeature::ContainerClickBox)` is true.

**Verification:** low risk. Build and run per Global Constraints: `tst_click_box_item`.

**Execution note (advisory):** opus. Prerequisite tasks: none. No networking or transmit path.

- [ ] **Step 1:** Tests, then the actions and the mask.
- [ ] **Step 2:** Mark the feature built.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): click box group and unit actions per Thetis`.

## Task 48: Mode grid: 12 modes in DSPMode order

**Requirements:**
- Button styling is not planned (ruling).
- 12 buttons in `DSPMode` order: LSB, USB, DSB, CWL, CWU, FM, AM, DIGU, SPEC, DIGL, SAM, DRM (`src/core/WdspTypes.h:177`; Thetis `Buttons = 12`, `MeterManager.cs:10093`, index `:10103`).
- Grids saved in the old 10-button order (LSB, USB, DSB, CWL, CWU, FM, AM, SAM, DIGL, DIGU; `ModeButtonItem.cpp:64-66`) are remapped on load, signalled by an appended order marker: visible bits and the active index move SAM 7→10, DIGL 8→9, DIGU 9→7; SPEC and DRM start hidden on those grids.
- SPEC and DRM are enabled: `SliceModel::setDspMode` accepts them (it refuses only an unavailable RADE V2 and a held listener write). A refused click shows the slice's existing reason.

**Files:**
- Modify: `src/gui/meters/ModeButtonItem.h`/`.cpp`
- Modify: `src/gui/containers/meter_property_editors/ModeButtonItemEditor.cpp`
- Modify: `src/gui/MainWindow.cpp` (`onContainerModeClicked`)
- Test: extend `tests/tst_container_item_persistence.cpp`

**Interfaces:**
- Consumes: Task 34 (column range); `DSPMode`; `SliceModel::setDspMode`.
- Produces: `static DSPMode ModeButtonItem::modeForIndex(int)`.

**Acceptance:**
- An old record with DIGU active loads with index 7 active; its visible bits map as above and SPEC and DRM are hidden.
- A new grid has 12 buttons.
- A click on index 10 sets SAM; on index 8 sets SPEC.

**Verification:** medium risk (remaps saved data; test first). Build and run per Global Constraints: `tst_container_item_persistence`.

**Execution note (advisory):** opus. Prerequisite tasks: 34. No networking or transmit path.

- [ ] **Step 1:** Remap test, then the marker and the remap.
- [ ] **Step 2:** The 12-mode order and click mapping.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): mode grid uses Thetis's 12 modes in DSPMode order`.

## Task 49: Tune step grid: Thetis's 26 steps

**Requirements:**
- Button styling is not planned (ruling).
- The grid shows `kTuneStepList` (`SliceModel.h:338-366`, 26 entries from `console.cs:1953-1982`). Labels drop "Hz" (`MeterManager.cs:8066`).
- New grids show the first ten (visible bits 1023, `:8041`); the editor has a visibility checklist for all 26.
- A click calls `SliceModel::setStepHz` on the entry's slice (`:8228-8242`).
- Old 7-step grids (1, 10, 100, 1k, 10k, 100k, 1M; `TuneStepButtonItem.cpp:86`) are remapped through `tuneStepIndexForHz` (to indices 0, 2, 5, 8, 14, 21, 24), both visible bits and active step, signalled by an appended marker.

**Files:**
- Modify: `src/gui/meters/TuneStepButtonItem.h`/`.cpp`
- Modify: `src/gui/containers/meter_property_editors/TuneStepButtonItemEditor.cpp`
- Modify: `src/gui/MainWindow.cpp` (`onContainerTuneStepSelected`)
- Test: extend `tests/tst_slice_tune_step_list.cpp`, `tests/tst_container_item_persistence.cpp`

**Interfaces:**
- Consumes: Task 34; `kTuneStepList`, `tuneStepIndexForHz`, `SliceModel::setStepHz`.
- Produces: `static int TuneStepButtonItem::stepHz(int index)` over 26 entries.

**Acceptance:**
- A new grid has 26 buttons with the first 10 visible.
- An old record with 1k active shows the 1kHz entry (index 8) active after the remap.
- A click sets `stepHz` on the item's slice only.

**Verification:** medium risk (remap). Build and run per Global Constraints: `tst_slice_tune_step_list`, `tst_container_item_persistence`.

**Execution note (advisory):** opus. Prerequisite tasks: 34. No networking or transmit path.

- [ ] **Step 1:** Remap tests, then the 26-entry list.
- [ ] **Step 2:** Visibility editor.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): tune step grid offers Thetis's 26 steps`.

## Task 50: Antenna grid follows its slice

**Requirements:**
- Button styling is not planned (ruling).
- Port the on-state logic for RX and TX antenna buttons (`MeterManager.cs:9700-9800`).
- The grid follows its entry's slice; TX buttons act on `RadioModel::txBoundSlice()`.
- Editor: per-button visibility (visible bits, default 1023) and the Task 34 column range.
- Rx/Tx (index 9) stays gated (`UnbuiltFeature::AntennaRxTxSplit`).

**Files:**
- Modify: `src/gui/meters/AntennaButtonItem.h`/`.cpp`
- Modify: `src/gui/containers/meter_property_editors/AntennaButtonItemEditor.cpp`
- Modify: `src/gui/MainWindow.cpp` (`onContainerAntennaSelected`)
- Create: `tests/tst_antenna_button_item.cpp`, registered

**Interfaces:**
- Consumes: Task 34; `SliceModel::setRxAntenna`/`setTxAntenna`; `RadioModel::txBoundSlice()`.
- Produces: none new.

**Acceptance:**
- On a slice using ANT2 only RX2 is lit.
- A TX antenna click changes `txAntenna` on `txBoundSlice()` and on no other slice.
- Hiding a button removes it from the layout.
- Index 9 shows its disabled reason.

**Verification:** medium risk (TX antenna selection). Build and run per Global Constraints: `tst_antenna_button_item`.

**Execution note (advisory):** opus. Prerequisite tasks: 34.
**TX-path flag:** TX antenna buttons change the transmit antenna through `SliceModel::setTxAntenna`. They never key.

- [ ] **Step 1:** On-state tests, then the port.
- [ ] **Step 2:** Visibility editor.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): antenna grid follows its slice with Thetis on-state logic`.

## Task 51: Filter grid: 12 buttons on every slice

**Requirements:**
- Button styling is not planned (ruling).
- 12 filter buttons on every slice. Thetis gives RX2 only 9 and guards it (`MeterManager.cs:7721`, `:7726`, `:7935`); NereusSDR does not.
- The Task 34 column range applies. Var1 and Var2 stay hidden (`UnbuiltFeature::VariableFilters`).
- A Thetis click also calls `MatchTXFilterToRXFilter` (`MeterManager.cs:7956-7975`; `console.cs:34901-34921`, acting only with Shift held). NereusSDR has no equivalent (no `SetTXFilter` port, `console.cs:17839-17870`), so the click sets only the RX filter; a code comment with the cite records this (appendix item).

**Files:**
- Modify: `src/gui/meters/FilterButtonItem.h`/`.cpp`
- Modify: `src/gui/containers/meter_property_editors/FilterButtonItemEditor.cpp`
- Modify: `src/gui/MainWindow.cpp` (`onContainerFilterClicked`)
- Create: `tests/tst_filter_button_item.cpp`, registered

**Interfaces:**
- Consumes: Task 34; `FilterPresetStore`; `SliceModel::setFilter`.
- Produces: none new.

**Acceptance:**
- A grid on Slice B shows the same 12 buttons as on Slice A.
- A click sets the slice's RX filter and leaves `TransmitModel` unchanged.

**Verification:** low risk. Build and run per Global Constraints: `tst_filter_button_item`.

**Execution note (advisory):** opus. Prerequisite tasks: 34. No transmit path.

- [ ] **Step 1:** Write the tests.
- [ ] **Step 2:** Remove any limit by slice; add the gap comment with the cite.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): filter grid has 12 buttons on every slice`.

## Task 52: Container options: Active slice, hide when slice not open, minimize for any slice, no controls

**Requirements:**
- The Container Settings slice combo gains "Active slice", saved as `rxSourceMode followSelectedRx`, as the workspace header source menu already does. The combo lists Slice A to the slice count (`ContainerSettingsDialog.cpp:1048-1053`).
- "Hide when RX not used" means the container's slice is not open (Thetis `MeterManager.cs:2783-2805`, which considers only RX2). Enable the checkbox (`ContainerSettingsDialog.cpp:1121-1133`); `ContainerManager` hides and shows on slice open and close.
- "Minimizes" works for any slice: `FloatingContainer::onConsoleWindowStateChanged(Qt::WindowStates, bool rx2Enabled)` (`FloatingContainer.h:146`) is generalised from `rx2Enabled` to "slice open"; `MainWindow` calls it on window state change (Thetis `frmMeterDisplay.cs:114-139`).
- "No controls" checkbox using `ContainerWidget::setNoControls(bool)`.
- Auto height already exists; the audit row is wrong and needs no work.

**Files:**
- Modify: `src/gui/containers/ContainerSettingsDialog.cpp`, `src/gui/containers/ContainerManager.h`/`.cpp`, `src/gui/containers/FloatingContainer.h`/`.cpp`, `src/gui/MainWindow.cpp`
- Test: extend `tests/tst_container_settings_workflow.cpp`, `tests/tst_container_follow_selected_rx.cpp`

**Interfaces:**
- Consumes: the `followSelectedRx` mode; `RadioModel` slice add and remove signals.
- Produces: `void ContainerManager::onSliceOpenChanged(int sliceIndex, bool open)`; `void FloatingContainer::onMainWindowStateChanged(Qt::WindowStates state, bool sliceOpen)` replacing `onConsoleWindowStateChanged`.

**Acceptance:**
- Choosing "Active slice" saves `followSelectedRx`.
- A container on Slice C with "hide when not open" is hidden until Slice C opens.
- A Minimizes container on Slice B follows the main window's minimize.
- "No controls" round-trips.

**Verification:** medium risk. Build and run per Global Constraints: `tst_container_settings_workflow`, `tst_container_follow_selected_rx`.

**Execution note (advisory):** opus. Prerequisite tasks: none. No networking or transmit path.

- [ ] **Step 1:** Combo entry and No controls, with tests.
- [ ] **Step 2:** Hide when not open and minimize for any slice, with tests.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): Active slice, hide when slice not open, and minimize for any slice`.

## Task 53: Every MeterPoller reading on the Longpath needle and bar, with Thetis scales

**Requirements:**
- Ruling D9 (Longpath instruments get the shared options only); audit Part E longpath.needle "Primary reading" and longpath.bar "Reading" rows and open question 3.
- Today the Reading list holds only readings with a scale (`readingsWithScale`, `LongpathContent.cpp:337-348`; `ReadingSource.cpp:597-604`), 8 entries; every other reading is a plain row that `setPrimary` turns down (`NeedleInstrument.cpp:162`, `BarInstrument.cpp:147`).
- Each reading below gets the scale of the Thetis bar or needle that draws it: range from the first and last calibration keys, shape from the calibration points, red threshold from Thetis `HighPoint` (the second calibration point, for example `MeterManager.cs:24700`).

| Binding (`MeterPoller.h`) | Picker label | Thetis calibration (`MeterManager.cs`) |
|---|---|---|
| AdcPeak, AdcAvg | RX: ADC Peak / Avg | -120 @0, -20 @0.8333, 0 @0.99 (:23084-23086, :23109-23111) |
| AgcPeak, AgcAvg | RX: AGC Peak / Avg | -125 @0, 0 @0.5, 125 @0.99 (:23306-23308, :23332-23334) |
| AgcGain | RX: AGC Gain | -50 @0, 100 @0.857, 125 @0.99 (:23244-23246) |
| PbSnr | RX: PB SNR | 0 @0, 50 @0.8333, 60 @0.99 (:23173-23175) |
| TxReversePower | TX: Reverse Power | as forward power: `AddREVPWRBar` calls `AddPWRBar` (:25181-25184; calibration :25231-25236) |
| TxMic, TxMicPeak | TX: Mic, TX: Mic Peak (new row) | -30 @0, 0 @0.665, 12 @0.99 (:24348-24350, :24372-24374) |
| TxEq, TxEqPeak | TX: EQ, TX: EQ Peak (new) | same three points (:24435-24437, :24460-24462) |
| TxLeveler, TxLevelerPeak | TX: Leveler, TX: Leveler Peak (new) | same (:24523-24525, :24547-24549) |
| TxAlcPeak | TX: ALC Peak (new) | same (:24670-24672) |
| TxCfc, TxCfcPeak | TX: CFC, TX: CFC Peak (new) | same (:24878-24880, :24902-24904) |
| TxComp, TxCompPeak | TX: Compressor, TX: Compressor Peak (new) | same (:25025-25027, :25049-25051) |
| TxLevelerGain, TxAlcGain, TxCfcGain | TX: Leveler / ALC / CFC Gain | 0 @0, 20 @0.8, 25 @0.99 (:24609-24611, :24756-24758, :24964-24966) |
| TxAlcGroup | TX: ALC Group | -30 @0, 0 @0.5, 25 @0.99 (:24817-24819) |
| HwVolts | HW: Volts | 10 to 15 V from ANANMM's volts needle (:23855-23857), linear |
| HwAmps | HW: Amps | 0 to 20 A from ANANMM's amps needle (:23884-23894), linear |

- HwTemperature, RotatorAz and RotatorEle stay unscaled (Thetis has none; `ReadingSource.cpp:485-513`). TX: ALC keeps its scale (`ReadingSource.cpp:444-473`). The peak labels match Task 28's.

**Files:**
- Modify: `src/gui/instruments/ReadingSource.cpp` (descriptors; the six peak bindings become rows; cites as the TX: ALC block)
- Modify: `src/gui/instruments/LongpathContent.cpp` (the per-instance power scale at about `:553-564` also covers `TxReversePower`)
- Modify: `docs/attribution/THETIS-PROVENANCE.md` (the ReadingSource.cpp row), `docs/attribution/LONGPATH-PROVENANCE.md` (the ReadingSource.cpp row)
- Test: extend `tests/tst_longpath_instruments.cpp`

**Interfaces:**
- Consumes: nothing from other tasks.
- Produces: `readingFor(id)->hasScale` true for every binding in the table, so `readingsWithScale()` and both Reading lists in `meterProperties` offer them.

**Acceptance:**
- For each binding: `hasScale`, `available`, `min`/`max` equal to the first and last keys, `fraction()` equal to the cited position at each key (within 1e-9), `threshold` equal to the second key. `txAlcHasThetisScale` passes unchanged.
- HwTemperature, RotatorAz and RotatorEle keep `hasScale == false`.
- A needle and a bar with each new primary are non-null and online after one reading; a bar on `TxReversePower` follows `MainPowerMeter::scaleChanged` as TxPower does.
- The editor's `property_binding` combo lists every new reading, enabled.
- A saved needle or bar on an already scaled binding draws as before.

**Verification:** ported constants; unit tests, failing first. Build and run per Global Constraints: `tst_longpath_instruments`, `tst_accessory_readouts`.

**Execution note (advisory):** opus. Prerequisite tasks: none. Every calibration constant is a named `constexpr` with `// From Thetis MeterManager.cs:<line> [v2.10.3.15]`; the volts and amps cites say the keys give the range only.

- [ ] **Step 1:** Table tests; see them fail.
- [ ] **Step 2:** Descriptors with a three-point fraction in the style of `alcFraction`; the reverse-power scale.
- [ ] **Step 3:** Provenance rows. Build, run, commit signed: `feat(longpath): needle and bar read every MeterPoller reading on its Thetis scale`.

## Task 54: VHF S9 rule on the Longpath needle and bar, per slice

**Requirements:**
- Ruling D9; audit Part E longpath.needle and longpath.bar "VHF S9 shift" and open question 7.
- Thetis adds 20 dB to Signal Strength and Avg Signal Strength in `getPerc` (`MeterManager.cs:41041-41052`) when the receiver is at or above the S9 frequency; Max Bin and TX readings are not shifted. NereusSDR keys the rule on the entry's slice, using Task 8's helper.
- The Longpath known-gap note (`ReadingSource.h:203-208`) gets a NereusSDR line saying the shift is now applied; the upstream text stays.
- `MainWindow`'s `>=` change is Task 8's; this task does not touch it.

**Files:**
- Modify: `src/gui/instruments/LongpathContent.cpp` (meterFactory, about `:500-599`: follow the entry's slice and pass readings through `aboveS9Adjusted` before `onReading`, in the live and the preview branch)
- Modify: `src/gui/instruments/ReadingSource.h` (the note)
- Test: extend `tests/tst_longpath_instruments.cpp`

**Interfaces:**
- Consumes: Task 8 (`isAboveS9Frequency`, `aboveS9Adjusted`), Task 53 (descriptors); `SliceFollower`, `sliceNow` (`LongpathContent.cpp:157-236`); `SliceModel::frequencyChanged`.
- Produces: nothing new.

**Acceptance:**
- A needle on Signal Peak, on a slice at 50.1 MHz and fed -93 dBm, shows the S9 position and text; at 14.2 MHz the same reading sits 20 dB lower; at exactly 30.000000 MHz it is shifted.
- Signal Avg is shifted; Signal Max Bin and TX readings are not.
- Moving the slice across 30 MHz changes the shift on the next reading without a rebuild; a Follow selected RX entry uses the newly selected slice.
- The second reading obeys the rule for its own binding.
- An inert preview applies the rule once, from the slice at build time.

**Verification:** display scaling; unit tests. Build and run per Global Constraints: `tst_longpath_instruments`.

**Execution note (advisory):** opus. Prerequisite tasks: 8, 53. No transmit path.

- [ ] **Step 1:** Tests for the cases above.
- [ ] **Step 2:** Wire the follower and `aboveS9Adjusted` into meterFactory.
- [ ] **Step 3:** The note. Build, run, commit signed: `feat(longpath): S9 at -93 dBm at or above 30 MHz, per slice`.

## Task 55: Fade on RX/TX for widget-factory rows (Longpath instruments)

**Requirements:**
- Ruling D9 and the veto list; audit Part E "No common per-item RX/TX behavior" and the Fade rows for longpath.needle, longpath.bar and longpath.filter.
- Task 6's `ContentFade` is attached to every widget-factory row by `ContainerContentHost`; this task adds that attachment and declares the toggles on the five Longpath types. The cover takes no mouse input (Thetis has no fade guard on VFO or filter items).

**Files:**
- Modify: `src/gui/containers/ContainerContentHost.h`/`.cpp` (both `createWidget` sites, about `:479` and `:600`, call `ContentFade::attach(widget, effective.config, mode)`; `setTransmitting(bool)` forwards to each attached overlay; a widget created while transmitting starts in that state)
- Modify: `src/gui/containers/ContainerManager.cpp` (`ContainerManager::setTransmitting` calls `ContainerContentHost::setTransmitting` on every host, beside Task 2's meter routing)
- Modify: `src/gui/instruments/LongpathContent.cpp` (append `ContentFade::properties()` to `frequencyProperties`, `frequencyDisplayProperties`, `meterProperties` and `filterProperties`, about `:350-447`)
- Test: extend `tests/tst_longpath_instruments.cpp`

**Interfaces:**
- Consumes: Task 6 (`ContentFade::kFadeOnRxKey`, `kFadeOnTxKey`, `kDimmedLevel`, `nextValue`, `properties()`, `attach`, `setTransmitting`, `overlayFor`, `ContentFadeOverlay::value`/`advance`); Task 2 (`ContainerManager::setTransmitting` routing).
- Produces: `void ContainerContentHost::setTransmitting(bool)`; the five Longpath types declare the two toggles.

**Acceptance:**
- `longpath.frequency`, `longpath.frequencyCustom`, `longpath.needle`, `longpath.bar` and `longpath.filter` each show "Fade on RX" and "Fade on TX" and write `fadeOnRx`/`fadeOnTx` to the entry config.
- With Fade on RX on, `overlayFor(widget)->value()` reaches 48 after five `advance()` calls while receiving and returns to 255 after transmit starts; Fade on TX is the reverse; with both off no overlay exists.
- Mouse-wheel tuning on a faded frequency instrument and dragging a faded filter edge still reach the slice.
- An entry saved without the keys looks the same.

**Verification:** GUI state; reads transmit state, never sets it. Build and run per Global Constraints: `tst_longpath_instruments`, `tst_container_item_tx_state`.

**Execution note (advisory):** opus. Prerequisite tasks: 2, 6, 53. Uses Task 6's mechanism only; no second fade mechanism.

- [ ] **Step 1:** Tests: transmit toggle over each type, overlay value, input pass-through, absent keys.
- [ ] **Step 2:** Host attach and forwarding; the properties.
- [ ] **Step 3:** Build, run, commit signed: `feat(longpath): Fade on RX and Fade on TX`.

## Task 56: S-meter dial: ALC, ALC group, volts and amps, power limit and VHF S9

**Requirements:**
- Append to `SMeterWidget::TxMode` (`src/gui/SMeterWidget.h:192`, today `{Power, SWR, Level, Compression}`) without renumbering:
  - `Alc`: ANANMM's ALC_G "alc_comp", bound to `MeterBinding::TxAlcGain` (109) (`MeterManager.cs:24029-24047`);
  - `AlcGroup`: `MeterBinding::TxAlcGroup` (110) (`:24061-24080`);
  - `Volts`: `MeterBinding::HwVolts` (200) (`:23837-23860`);
  - `Amps`: `MeterBinding::HwAmps` (201) (`:23865-23883`).
  Display groups follow `:24131-24137`.
- A power limit per copy, with `MeterItem::setPowerScale(int limitWatts, double topWatts)` semantics, alongside `followExternalAmp` (`SMeterDialContent.cpp:175`). Thetis has no per-copy dial limit; this uses NereusSDR's existing power scale API.
- VHF S9 per slice on the dial using Task 8's `isAboveS9Frequency`: at or above 30 MHz, S9 is -93 dBm. No helper is added to `ControlRanges.h`.

**Files:**
- Modify: `src/gui/SMeterWidget.h`/`.cpp`
- Modify: `src/gui/instruments/SMeterDialContent.h`/`.cpp` (`applyConfig` :88, `configFrom` :114, `dialProperties` :129)
- Test: extend `tests/tst_smeter_dial_content.cpp`, `tests/tst_smeter_widget_scale.cpp`

**Interfaces:**
- Consumes: Task 8 (`AboveS9Frequency.h`); the `MeterBinding` ids above.
- Produces: `SMeterWidget::TxMode::Alc`, `AlcGroup`, `Volts`, `Amps`; `void SMeterWidget::setAboveS9Frequency(bool)`; dial property `powerLimitW`.

**Acceptance:**
- TX mode ALC reads `TxAlcGain`; Volts reads `HwVolts`.
- A saved dial with `TxMode::Compression` loads unchanged.
- At exactly 30.000 MHz S9 is -93 dBm; at 29.999 MHz it is -73 dBm.
- Power limit 50 puts a 25 W reading at mid-scale.

**Verification:** medium risk. Build and run per Global Constraints: `tst_smeter_dial_content`, `tst_smeter_widget_scale`.

**Execution note (advisory):** opus. Prerequisite tasks: 8. Reads TX meters only; no transmit path.

- [ ] **Step 1:** S9 boundary test.
- [ ] **Step 2:** TX modes, bindings and the power limit property.
- [ ] **Step 3:** Build, run, commit signed: `feat(instruments): S-meter dial ALC, volts/amps, power limit and VHF S9 at 30 MHz`.

## Task 57: S-meter dial: appearance and dynamics for each copy

**Requirements:**
- Each `applet:s_meter` copy gets these as declared properties in `dialProperties`, `applyConfig` and `configFrom`: peak marker colour, update interval, attack and decay, history, needle colours, shadow, power scale colour, dark mode, and Fade on RX/TX (`ContentFade::properties()`, attached by Task 55's host path).
- The rows come from `docs/architecture/container-item-options-parity-audit.md:1919-1970`; the implementer re-verifies each row's Thetis cite at `[v2.10.3.15]` before porting it.
- Copies saved before this task keep today's look.

**Files:**
- Modify: `src/gui/instruments/SMeterDialContent.h`/`.cpp`, `src/gui/SMeterWidget.h`/`.cpp`
- Test: extend `tests/tst_smeter_dial_content.cpp`, `tests/tst_smeter_widget_peak_hold.cpp`

**Interfaces:**
- Consumes: Task 6 (`ContentFade::properties()`), Task 55 (host attach), Task 56.
- Produces: the declared properties.

**Acceptance:**
- Two copies keep different peak colours and decay values across save and load.
- A copy saved before this task renders unchanged.
- Fade on TX on a copy dims it while transmitting.

**Verification:** low risk. Build and run per Global Constraints: `tst_smeter_dial_content`, `tst_smeter_widget_peak_hold`.

**Execution note (advisory):** opus. Prerequisite tasks: 55, 56. No networking or transmit path.

- [ ] **Step 1:** Per-copy tests.
- [ ] **Step 2:** Properties with cites.
- [ ] **Step 3:** Build, run, commit signed: `feat(instruments): per-copy S-meter dial appearance and dynamics`.

## Task 58: Other buttons use Thetis button numbers, and every button's visibility is saved

**Requirements:**
- Audit Part D OTHERBTNS rows "Id numbering", "Visible bits storage" and "Button catalog" (`container-item-options-parity-audit.md:1524-1527`); ruling D6.
- Adopt Thetis numbering: each `OtherButtonItem::ButtonId` becomes `bit_group * 32 + bit` from `ucOtherButtonsOptionsGrid.cs:529-755`. Thetis uses that index for layout and clicks (`MeterManager.cs:8908-8916`, `:9404-9407`). 12 groups (`MAX_BITFIELD_GROUP = 12`, `ucOtherButtonsOptionsGrid.cs:523`), 384 slots (`MeterManager.cs:8301`); bit 31 of each group is never used (`:8303`, `:8902-8906`). Ids keep their NereusSDR names; only the numbers change. The Thetis caption, tooltip and section of each id are ported from that table.
- Old ordinals (`OtherButtonItem.h:83-91`) map to new ids: Power 0→0, Rx2 1→1, Mon/Tun/Mox/TwoTon/Dup/PsA 2-7→2-7, Play 8→10, Rec 9→9, Anf 10→37, Snb 11→41, Mnf 12→42, Avg 13→96, PeakHold 14→97, Ctun 15→98, Vac1 16→128, Vac2 17→129, Mute 18→130, Bin 19→132, SubRx 20→133, PanSwap 21→134, Xpa 22→8, Spectrum…DisplayOff 23-33→99-109, Macro i (64+i)→352+i for i = 0..30.
- A new OTHERBTNS item and each `control.*` shortcut show exactly what they show today (Thetis starts all off, `MeterManager.cs:8303-8307`).
- The only places that persist `ButtonId` integers are OTHERBTNS records and the `control.*` shortcuts (`OtherButtonItem.cpp`, `ButtonBoxItem.cpp`, `ContainerContentRegistry.cpp`, `ContainerControlCatalog.h`, `ContainerSettingsDialog.cpp`); `tests/visual/PreviewScenes.cpp` has none.

**Files:**
- Create: `src/gui/meters/OtherButtonCatalog.h`/`.cpp`, a port of `ucOtherButtonsOptionsGrid.cs` (its header byte for byte, including the Samphire dual-licence block, plus Modification history), holding the id table and the legacy map; registered in the root `CMakeLists.txt` beside `src/gui/meters/OtherButtonItem.cpp`; a `docs/attribution/THETIS-PROVENANCE.md` row
- Modify: `src/gui/meters/OtherButtonItem.h`/`.cpp` (enum values, 384 slots, visibility per id, map, serialize/deserialize)
- Modify: `src/gui/meters/ButtonBoxItem.h`/`.cpp` (lift the 32-button cap in `setVisibleBits`, `ButtonBoxItem.cpp:133-140`; the single-control layout test at about `:215-217` counts shown buttons)
- Modify: `src/gui/containers/ContainerControlCatalog.h` (`isSingleContainerControl`, `:31-39`, checks for exactly one visible id)
- Modify: `src/gui/containers/ContainerContentRegistry.cpp` (`control.*` creation, about `:335-339`)
- Modify: `src/gui/containers/meter_property_editors/OtherButtonItemEditor.cpp` (bit arithmetic at about `:33-36`, `:63`, `:79-80`)
- Modify: `src/gui/containers/ContainerButtonDispatcher.cpp`, `src/gui/MainWindow.cpp` only where they assume old values
- Create: `tests/tst_other_button_ids.cpp`, registered beside `tst_other_button_item`
- Update: `tests/tst_other_button_item.cpp`, `tests/tst_remote_gui_gating.cpp` (container cases near `:4962-5053`), `tests/tst_container_item_persistence.cpp` wherever they encode `1u << int(Id::X)` or exact OTHERBTNS strings

**Interfaces:**
- Consumes: the OTHERBTNS record at HEAD (Task 1, commit 7145b8506): `OTHERBTNS|x|y|w|h|binding|z|columns|visibleBits|border|margin|radius|heightRatio|fadeRx|fadeTx`, fields 0-14 (`OtherButtonItem.cpp:218-237`, grid style read from index 9).
- Produces:
  - `OtherButtonItem`: `kGroupCount = 12`, `kSlotCount = 384`, `setButtonVisible(ButtonId, bool)`, `isButtonVisible(ButtonId)`, `visibleGroups()`/`setVisibleGroups(const QVector<qint32>&)` (12 non-negative values, bit 31 clear), `setOnlyVisible(ButtonId)`, `buttonMap()`/`setButtonMap(const QVector<short>&)` (empty = identity), `resetButtonMap()`, `bool isOffered(ButtonId) const` (true when NereusSDR has an action).
  - `OtherButtonCatalog`: `struct OtherButtonInfo { int group; int bit; QString caption; QString tooltip; QString section; }`, `OtherButtonInfo info(OtherButtonItem::ButtonId)`, `OtherButtonItem::ButtonId legacyToButtonId(int oldOrdinal)`.
  - Record, appended after field 14: field 15 = `2` (id scheme); fields 16-27 = the 12 group bitfields in decimal (Thetis `buttonbox_other_buttons_bitfield_0..11`, `MeterManager.cs:28583`); field 28 = the map, empty for identity, else 384 comma-separated shorts (Thetis `buttonbox_button_map`, `:28579`). Field 8 still holds the old-numbering mask of the visible old ids 0..31, so an older build loads the same subset.

**Acceptance:**
- A record without field 15 loads through the legacy map with the same buttons in the same order: `OTHERBTNS|0.1|0.2|0.3|0.4|5|6|3|328708` shows MON, ANF, VAX 1 and MUTE only (old bits 2, 10, 16, 18).
- An old saved Power bit is still dropped.
- Old ids 32 and 33 count as visible on legacy load (the old `i < 32` loop never hid them).
- An old single control (`columns` 1, mask `1 << 4`) loads as the MOX single control; `isSingleContainerControl` is true and its geometry is unchanged.
- A new OTHERBTNS item and each `control.*` shortcut show exactly what they show today.
- Visibility in every group (including group 11) and a non-identity map survive serialize → deserialize and the registry capture/allocate path (the `tst_container_document` pattern).
- Group 1 set to `1 << 5` shows ANF (id 37); bit 31 is ignored.
- Field 8 holds the old-numbering mask of the visible old ids.
- An id with no NereusSDR action is never drawn and its saved visibility is kept; ids gated by `UnbuiltFeature` keep their gate (`OtherButtonItem.cpp:163-195`).
- No visible change to any grid.

**Verification:** data-format change with backward compatibility; legacy-record tests first. Build and run per Global Constraints: `tst_other_button_ids`, `tst_other_button_item`, `tst_container_item_persistence`, `tst_container_document`, `tst_container_persistence`, `tst_container_controls_refresh`, `tst_remote_gui_gating`. UI evidence per `ui-verification`: before/after captures of a default OTHERBTNS grid and a single control, which must match.

**Execution note (advisory):** opus. Prerequisite tasks: 1. No transmit path, no wire. Keep the shown-ordinal calculation off the per-button path (384 slots): cache it when visibility or the map changes.

- [ ] **Step 1:** Legacy, round-trip, single-control and group tests; see them fail.
- [ ] **Step 2:** Port the id table into `OtherButtonCatalog`, captions and tooltips verbatim (Task 59 rewrites any user tooltip naming VFOa or rx1).
- [ ] **Step 3:** Storage, map and record; lift the cap.
- [ ] **Step 4:** Catalog, registry, editor and the tests that encode old numbers.
- [ ] **Step 5:** Build, run, commit signed: `feat(containers): other buttons use Thetis button numbers and keep every button's visibility`.

## Task 59: Reorder other buttons, and an editor that lists every button

**Requirements:**
- Audit Part D rows "Reorder (alt-drag, shift-swap)", "Reset layout button" and "Visibility editor" (`container-item-options-parity-audit.md:1528-1530`). The buttons use NereusSDR's look; Thetis's lime/dark-red drag outline (`MeterManager.cs:39797-39815`) is not ported.
- Behaviour per `MeterManager.cs:9322-9390`: Alt held at press on a shown button starts a drag and emits no click (`:9331-9339`); the button under the pointer is the target (`MoveButton`, `:9546-9550`); on release over a different button, Shift held swaps the two (`:9359-9364`), otherwise the dragged button moves and the positions between shift by one (`:9366-9378`); the map covers all 384 positions; while Fade on RX/TX hides the grid, the press does nothing (`:9324-9325`).
- Input path: `ArrangeHoldFilter::eventFilter` returns false for mouse presses, and a Shift press while a button is down or with Alt held does not start an arranging hold (`ArrangeHoldFilter.cpp:137-151`), so an Alt press and a mid-drag Shift reach the item. If a test shows otherwise, make the filter pass an Alt press through.
- The new order is saved (field 28) and loads the same.
- Editor: the "Single control" combo stays (NereusSDR-only). "Visible controls" lists every id with `isOffered` true, under Thetis's section headings, with "VFOs" shown as "Tuning" (ruling D2). Each row has the Thetis caption and a tooltip with no "VFO A/B", "VFOa" or "RX1/RX2". Toggling a row shows or hides that button. "Reset layout" sets the identity map (Thetis `btnOtherButtons_reset_layout`, `setup.cs:35082-35086`; a null map becomes identity, `MeterManager.cs:8918-8947`).
- Ids that Tasks 60-62 mark offered appear in the editor with no editor change.

**Files:**
- Modify: `src/gui/meters/OtherButtonItem.h`/`.cpp` (press, move and release for dragging; a drag face in NereusSDR colours)
- Modify: `src/gui/meters/ButtonBoxItem.h`/`.cpp` only if a protected hook is needed for the drag position or face
- Modify: `src/gui/containers/meter_property_editors/OtherButtonItemEditor.h`/`.cpp`
- Create: `tests/tst_other_button_reorder.cpp`, registered

**Interfaces:**
- Consumes: Task 58 (`buttonMap`, `resetButtonMap`, `isOffered`, `OtherButtonCatalog::info`); Task 2 (transmit routing, which makes the fade guard meaningful).
- Produces: no public API beyond Task 58.

**Acceptance:**
- Alt-press then release over another button moves it and emits no click; with Shift, the two swap.
- The order survives save and load; Reset layout restores identity.
- A press on a faded grid does nothing.
- The editor lists every offered id under its heading, with "Tuning" for "VFOs", and no row or tooltip contains "VFO A", "VFO B", "VFOa", "RX1" or "RX2".

**Verification:** GUI interaction; unit tests with synthetic mouse events carrying Alt and Shift (copy `clickShownButton`, `tests/tst_other_button_item.cpp:126-141`). Build and run per Global Constraints: `tst_other_button_reorder`, `tst_other_button_item`. UI evidence: the editor, and a grid mid-drag.

**Execution note (advisory):** opus. Prerequisite tasks: 2, 58. No transmit path.

- [ ] **Step 1:** A test that an Alt press inside a container reaches the item.
- [ ] **Step 2:** Move, swap, no-click, persistence and Reset layout tests; see them fail.
- [ ] **Step 3:** The drag, then the editor.
- [ ] **Step 4:** Build, run, commit signed: `feat(containers): reorder other buttons and list every button in the editor`.

## Task 60: Receive buttons act on the entry's slice

**Requirements:**
- Ruling D6, and the ruling to ship all 78 other buttons: Tasks 60-62 add 63 buttons (31 here, 20 in Task 61, 12 in Task 62) to the 15 existing. These Thetis ids get a NereusSDR action on the entry's slice through `ContainerButtonDispatcher::sliceFor` (Slice A-D or Active slice). A slice not open, or a refused change, shows the existing reason. They change receive settings through existing setters only; none keys.

| Thetis id (number) | Label | Click | Lit when | Thetis cite |
|---|---|---|---|---|
| NR (32) | NR, NR2, NR3, NR4 | `SliceModel::setActiveNr`, cycling Off→NR1→NR2→NR3→NR4→Off; a NereusSDR-only slot (DFNR, BNR, MNR, NNR) cycles to Off and the label shows that slot's name | `activeNr()` not Off | console.cs:51391, :43327-43339; MeterManager.cs:8693-8702 |
| NR1-NR4 (33-36) | NR1-NR4 | `setActiveNr(n)`, or Off when already n | equal | console.cs:51425-51444; MeterManager.cs:8704-8718 |
| NB (38) | NB, NB2 | `setNbMode(cycleNbMode(nbMode()))` (`src/core/NbFamily.h`) | not Off | console.cs:51393-51400; MeterManager.cs:8721-8729 |
| NB1 (39), NB2 (40) | NB1, NB2 | `setNbMode` to that mode, or Off when already set | equal | console.cs:51445-51454; MeterManager.cs:8731-8737 |
| MNF_PLUS (43) | MNF+ | `RadioModel::addTnfForSlice(slice)` | never | console.cs:51403, TNFAdd :40313-40331 |
| RIT (70), XIT (72) | RIT, XIT | `setRitEnabled`/`setXitEnabled` toggle | enabled | console.cs:51503-51504, :52896-52901 |
| RIT0 (71), XIT0 (73) | RIT0, XIT0 | `setRitHz(0)`/`setXitHz(0)` | never | console.cs:51505-51506, :52902-52907 |
| LOCK_A (75) | Lock | `setLocked(!locked())` | `locked()` | console.cs:51611 |
| TUNE_STEP_U/D (77, 78) | TS+, TS- | `changeTuneStepUp()`/`changeTuneStepDown()` | never | console.cs:51613-51614 |
| MUTE_ALL (131) | Mute All | `setMuted(!allMuted)` on every slice this window may change | all those muted | console.cs:51421; GetMute :52208-52229 |
| SQL_VSQL (137) | Voice SQL | `setSsqlEnabled` toggle | `ssqlEnabled()` | console.cs:51510; MeterManager.cs:8661-8691 |
| AF_P5/M5 (151, 152) | AF+, AF- | `setAfGain(afGain() ± 5)`, clamped to `kAfGainMin..kAfGainMax` (`ControlRanges.h`) | never | console.cs:51556-51557 |
| BAL_P5/M5 (153, 154) | BAL+, BAL- | `setAudioPan(audioPan() ± 0.1)`, clamped to -1..1 (Thetis 0..100; `RxApplet.cpp:1082` maps (v-50)/50) | never | console.cs:51558-51559 |
| AGC_FIXED…AGC_CUSTOM (192-197) | FIX, LONG, SLOW, MED, FAST, CUST | `setAgcMode(Off/Long/Slow/Med/Fast/Custom)` (Thetis FIXD = 0 matches `AGCMode::Off`, `enums.cs:152-162`) | equal | console.cs:51477-51482; MeterManager.cs:8600-8640 |
| AGC_AUTO (198) | AUTO | `setAutoAgcEnabled` toggle | enabled | console.cs:51483; MeterManager.cs:8641 |
| AGC_P5/M5 (199, 200) | AGC+, AGC- | `setAgcThreshold` ± 5, clamped to `kAgcThresholdMinDb..kAgcThresholdMaxDb` | never | console.cs:51572-51573; :51803-51815 |

- Divergences kept: MNF+ uses the entry's slice (Thetis always RX1, "// no rx2"); Lock, RIT, XIT and tune step are per slice (console-wide in Thetis).

**Files:**
- Modify: `src/gui/containers/ContainerButtonDispatcher.h`/`.cpp` (step constants carry Thetis cites per `docs/attribution/HOW-TO-PORT.md`; if the inline-tag check flags the file, give it the console.cs header)
- Modify: `src/gui/meters/OtherButtonItem.cpp` (`isOffered`, dynamic NR/NB labels)
- Modify: `src/gui/MainWindow.cpp` (`watchSlicesForContainers` adds `activeNrChanged`, `nbModeChanged`, `ritEnabledChanged`, `xitEnabledChanged`, `lockedChanged`, `agcModeChanged`, `autoAgcEnabledChanged`, `ssqlEnabledChanged`)
- Create: `tests/tst_other_button_actions.cpp`, registered (copy the fixture in `tests/tst_other_button_item.cpp:150-189`)

**Interfaces:**
- Consumes: Task 58 ids; the `SliceModel` and `RadioModel` setters above.
- Produces: `ContainerButtonDispatcher` `stateOf` and `click` handle every id in the table.

**Acceptance:**
- For every row, with Slice A on the container and Slice B active, a click changes Slice A only and lights as the row says.
- NR3 refused changes nothing and shows the slice's `nrSelectionRefused` reason.
- Steps clamp at the ends.
- A container on a slice that is not open shows every row unavailable with `noSliceReason`.
- Mute All skips slices this window may not change.
- No new connection to the Core or the radio.

**Verification:** GUI routing to existing setters; unit tests on a local `RadioModel`. Build and run per Global Constraints: `tst_other_button_actions`, `tst_other_button_item`. UI evidence: one capture of a grid showing all rows.

**Execution note (advisory):** opus. Prerequisite tasks: 58.
**RX-path flag:** these buttons change receive DSP settings (NR, NB, AGC, squelch, AF, pan) on the RX path, through existing setters only. None keys.

- [ ] **Step 1:** Per-row tests; see them fail.
- [ ] **Step 2:** Cases, labels and refresh signals.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): receive buttons on the container's slice`.

## Task 61: Transmit setting buttons

**Requirements:**
- Ruling D6, for the Thetis transmit-setting ids NereusSDR has. Each click calls the setter or request the existing applet control uses. In a remote window each button follows that control's permission gate (computed at `MainWindow.cpp:17824-17900`), shows the gate's reason, and the Core still decides. No new command types.
- Existing MOX, TUN and 2TON (ids 3-5) are unchanged.

| Thetis id (number) | Label | Action (existing control) | Gate | Lit | Thetis cite |
|---|---|---|---|---|---|
| COMP (141) | COMP | `TransmitModel::setCpdrOn` (`PhoneCwApplet.cpp:1007`) | chain, v2 | `cpdrOn()` | console.cs:51486, :52848-52850 |
| VOX (142) | VOX | `setVoxEnabled` (`TxApplet.cpp:3278`) | VOX line, then chain (`MainWindow.cpp:17841-17851`) | `voxEnabled()` | console.cs:51487, :52851-52853 |
| DEXP (143) | DEXP | `setDexpEnabled` (`PhoneCwApplet.cpp:1114`) | chain | `dexpEnabled()` | console.cs:51488, :52854-52856 |
| TX_EQ (145) | TX EQ | `setTxEqEnabled` (`TxApplet.cpp:2851`) | chain | `txEqEnabled()` | console.cs:51490, :52860-52862 |
| CFC (147) | CFC | `setCfcEnabled` (`TxApplet.cpp:2884`) | chain | `cfcEnabled()` | console.cs:51492, :52866-52868 |
| CFC_EQ (148) | CFC EQ | `setCfcPostEqEnabled` (`DspSetupPages.cpp:2656`; Thetis chkCFCPeqEnable, setup.cs:4365-4370) | processing, v4 | `cfcPostEqEnabled()` | console.cs:51493, :52869-52871 |
| LEVELER (167) | Leveler | `setTxLevelerOn` (`TxApplet.cpp:2834`) | chain | `txLevelerOn()` | console.cs:51494, :52872-52874 |
| PHASE_ROT (168) | PhaRot | `setPhaseRotatorEnabled` (`DspSetupPages.cpp:2554`) | processing | `phaseRotatorEnabled()` | console.cs:51495, :52875-52877 |
| MIC_P1/M1 (160, 161) | MIC+, MIC- | `setMicGainDb` ± 1 (`PhoneCwApplet.cpp:963`), clamped to that slider's range | chain | never | console.cs:51566-51567 |
| COMP_P1/M1 (163, 164) | COMP+, COMP- | `setCpdrLevelDb` ± 1 (`PhoneCwApplet.cpp:1034`) | chain | never | console.cs:51568-51569 |
| VOX_P1/M1 (165, 166) | VOX+, VOX- | `setVoxThresholdDb` ± 1 (`TxApplet.cpp:3309`) | chain | never | console.cs:51570-51571 |
| DRIVE_P5/M5 (256, 257), DRIVE_0 (260) | DRI+, DRI-, DRI-0 | `setPower` ± 5 or 0, as the RF Power slider (`TxApplet.cpp:2641-2644`) | the slider's gate | never | console.cs:51574-51575, :51578 |
| TUNE_P5/M5 (258, 259), TUN_0 (261) | TUN+, TUN-, TUN-0 | local `setTunePowerForTxBand` (`TxApplet.cpp:2697`); remote `IStationLink::requestTunePowerForTxBand` (`TxApplet.cpp:2218-2228`) | chain | never | console.cs:51576-51577, :51579 |

- VOX± steps one NereusSDR `voxThresholdDb` dB per click for Thetis's one VOX sensitivity step. DRI± uses whichever gate the TX applet applies to its RF Power slider.

**Files:**
- Modify: `src/gui/containers/ContainerButtonDispatcher.h`/`.cpp` (two new `Hooks` members, each returning an empty string when allowed; the existing `transmitSettingsPermitted`/`transmitSettingsReason` stay)
- Modify: `src/gui/MainWindow.cpp` (hook setup with the `settingsAt`/`settingsReasonAt` holder rule, about `:17859-17866`; the global refresh block adds `cpdrOnChanged`, `voxEnabledChanged`, `dexpEnabledChanged`, `txEqEnabledChanged`, `cfcEnabledChanged`, `cfcPostEqEnabledChanged`, `txLevelerOnChanged`, `phaseRotatorEnabledChanged`)
- Modify: `src/gui/meters/OtherButtonItem.cpp` (`isOffered`)
- Test: extend `tests/tst_other_button_actions.cpp`; the remote tune-power case uses the loopback `Session` pattern of `tests/tst_transmit_model_properties.cpp:805-830`

**Interfaces:**
- Consumes: Task 58 ids; Task 60's dispatcher cases and test file; `IStationLink::requestTunePowerForTxBand`.
- Produces: `ContainerButtonDispatcher::Hooks` members `std::function<QString(int version)> transmitSettingsRefusal` and `std::function<QString()> voxRefusal`.

**Acceptance:**
- Local window: each row's click sets the model as the row says, steps clamp, and the lit state follows the model.
- Remote window with the gate refused: every row is unavailable with that gate's reason and a click changes nothing.
- Remote TUN+: the request reaches the Core and the Core's tune power for the TX band rises by 5.
- VOX in a remote window with no microphone line shows the VOX line reason.
- No button keys MOX or TUNE: the tests assert `isTransmitting()` stays false for every row.

**Verification:** transmit settings (no audio path code); unit tests first, plus the loopback remote case. Build and run per Global Constraints: `tst_other_button_actions`, `tst_transmit_model_properties`.

**Execution note (advisory):** opus. Prerequisite tasks: 60.
**TX-path flag:** VOX arms voice-operated keying; DRI± and TUN± change output power on the air; the others change the transmit audio chain. No button keys; no wire or audio path code changes.

- [ ] **Step 1:** Local, remote-refused and remote tune-power tests; see them fail.
- [ ] **Step 2:** Hooks, cases and refresh signals.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): transmit setting buttons follow the TX applet's gates`.

## Task 62: Display and window buttons

**Requirements:**
- Ruling D6. Display toggles act on the panadapter that shows the entry's slice (`ContainerButtonDispatcher::Hooks::spectrumFor`), as Peak and CTUN do, through the setter the overlay pill or Setup row calls. Console-wide in Thetis, per panadapter in NereusSDR. None keys.

| Thetis id (number) | Label | Action | Lit | Thetis cite |
|---|---|---|---|---|
| NF (53) | NF | `SpectrumWidget::setShowNoiseFloor` (`SpectrumOverlayPanel.cpp:3309-3313`) | `showNoiseFloor()` | console.cs:51618, :51760-51787 |
| PEAK_BLOBS (111) | Peak Blobs | `setPeakBlobsEnabled` on that pan, saved as the pan's own value through the per-pan path `SpectrumPeaksPage::bindDefaultToggle` uses for a selected pan (`src/gui/setup/SpectrumPeaksPage.cpp:205-207`), not the master | `peakBlobsEnabled()` | console.cs:51498, :52881-52883 |
| CURSOR_INFO (112) | Cur Info | `setCursorFreqVisible` (`SpectrumOverlayPanel.cpp:3325-3328`) | `cursorFreqVisible()` | console.cs:51499, :52884-52886 |
| SPOTS (113) | Spots | save `IsSpotsEnabled` as `SpotHubDialog.cpp:2833` does, then `loadSpotDisplaySettings()` on every pan | `showSpots()` | console.cs:51500, :52887-52889 |
| FILL_SPECTRUM (114) | Fill | `setPanFillEnabled` (`SpectrumOverlayPanel.cpp:3178-3181`) | `panFillEnabled()` | console.cs:51502, :52893-52895 |
| ZTB (118) | ZTB | `SpectrumWidget::zoomToBand()` | never | console.cs:51548 |
| ACTITVE_PEAK (123) | A-Peak | `setActivePeakHoldEnabled`, same per-pan path as Peak Blobs (`SpectrumPeaksPage.cpp:195-197`) | `activePeakHoldEnabled()` | console.cs:51501, :52890-52892 |
| MAF_P5/M5 (149, 150) | MAF+, MAF- | master volume ± 0.05 through `MasterOutputWidget`'s own slider path (`src/gui/widgets/MasterOutputWidget.cpp:425-437`: engine, `audio/Master/Volume`, echo), clamped to 0..1 | never | console.cs:51554-51555 |
| FORM_SETUP (288) | Setup | `MainWindow::createSetupDialog()`, then show (pattern at `MainWindow.cpp:15417-15425`) | never | console.cs:51581 |
| FORM_DIVERSITY (296) | Diversity | `MainWindow::openDiversityDialog()` | never | console.cs:51594 |
| FORM_LINEARITY (297) | Linearity | `MainWindow::openPureSignalDialog()` | never | console.cs:51595 |

- Display toggles light at once after a click (through the refresh in `MainWindow::onContainerOtherButtonClicked`) or on the next refresh, as Peak does; a change made elsewhere may lag until then.

**Files:**
- Modify: `src/gui/containers/ContainerButtonDispatcher.h`/`.cpp` (new `Hooks` members: `std::function<bool()> spotsShown`, `std::function<void(bool)> setSpotsShown`, `std::function<void(double delta)> stepMasterVolume`, `std::function<void()> openSetup`, `openDiversity`, `openLinearity`)
- Modify: `src/gui/MainWindow.cpp` (supplies the hooks)
- Modify: `src/gui/widgets/MasterOutputWidget.h`/`.cpp` only to expose a step that runs its existing slider path
- Modify: `src/gui/meters/OtherButtonItem.cpp` (`isOffered`)
- Test: extend `tests/tst_other_button_actions.cpp`

**Interfaces:**
- Consumes: Task 58 ids; Task 61 (shared dispatcher and test file); `Hooks::spectrumFor`.
- Produces: the `Hooks` members listed under Files; `void MasterOutputWidget::stepVolume(double delta)`.

**Acceptance:**
- Each toggle changes the panadapter of the container's slice and lights from it at once after a click.
- With no panadapter showing the slice, the toggles show "No panadapter shows this slice."
- Spots persists and every pan follows.
- MAF± changes the master volume and saves it like the slider, clamped to 0..1.
- Setup, Diversity and Linearity open the same windows as their menu actions, with their own remote gates.

**Verification:** GUI routing; unit tests with `SpectrumWidget` instances as the Peak test does (`tests/tst_other_button_item.cpp:635-669`). Build and run per Global Constraints: `tst_other_button_actions`, `tst_other_button_item`. UI evidence: one grid capture.

**Execution note (advisory):** opus. Prerequisite tasks: 61. Display and window only; no transmit.

- [ ] **Step 1:** Write the tests; see them fail.
- [ ] **Step 2:** Hooks and cases.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): display and window buttons`.

## Task 63: Frequency readout's second line picks a slice; migrate the legacy `direction` key on load

**Requirements:**
- Rulings D9 and D2; audit Part E multislice notes for longpath.frequency and open questions 4 and 8.
- Second line: today `otherSlice()` is the first other slice in `RadioModel::slices()` (`LongpathContent.cpp:222-229`, used at `:260-277`). New property "Second line" (key `secondSlice`) on both frequency types:
  - Transmit slice (default; key absent): the slice whose `SliceModel::isTxSlice()` is true (the arbiter's binding; `setTxSlice` emits `txSliceChanged`), when it is not the item's own slice, else hidden;
  - None: hidden;
  - Slice A to Slice D: that slice, hidden when not open or the item's own slice.
- The line follows `txSliceChanged` and slice add/remove (`SliceFollower::trackRefresh`); `setActiveIsThis` (`:271`) compares with the slice shown.
- The Custom type's toggles keep their keys (`kShowOwnKey`, `kShowOtherKey`, `LongpathContent.h:86-87`); labels become "Show this slice's line" and "Show the second line". Hiding through the toggle wins over the picker.
- `direction` migration: `direction` (`LongpathContent.h:78`) is read only by `primaryBinding` (`LongpathContent.cpp:451-458`) and has no UI. On load, a `longpath.needle` or `longpath.bar` entry with `direction` and no `binding` gets `binding = LongpathContent::primaryBinding(typeId, config)` and loses `direction`; an entry with both keeps `binding` and loses `direction`. `primaryBinding`'s `direction` branch stays for in-memory entries. Load means every decode path (workspace store, profiles, imports, duplicates), all through `decodeEntry` (`ContainerDocumentCodec.cpp:350`).

**Files:**
- Modify: `src/gui/instruments/LongpathContent.cpp` (`makeFrequency` refresh, `frequencyProperties`, the two Custom labels), `src/gui/instruments/LongpathContent.h` (`kSecondSliceKey = "secondSlice"`)
- Create: `src/gui/containers/ContentEntryMigration.h`/`.cpp` (NereusSDR-original), registered in the root `CMakeLists.txt` beside `src/gui/containers/ContainerDocumentCodec.cpp` (`:1993`)
- Modify: `src/gui/containers/ContainerDocumentCodec.cpp` (`decodeEntry` calls `migrateEntryOnLoad` after a successful decode)
- Test: extend `tests/tst_longpath_instruments.cpp` (update `frequencyRowUsesSliceLetters` and the Custom tests for the new default), `tests/tst_container_document.cpp`

**Interfaces:**
- Consumes: `SliceModel::isTxSlice()`, `txSliceChanged(bool)` (`SliceModel.h:954`, `:1699`); `LongpathContent::primaryBinding`.
- Produces: `void NereusSDR::migrateEntryOnLoad(ContentEntry& entry)`: runs on every decoded entry, idempotent, leaves unknown types untouched; Task 66 extends it.

**Acceptance:**
- Slices A and B, item on B, A transmitting: the second line shows A; when B becomes the transmit slice the line hides; with no transmit slice it is hidden.
- "Slice C" with C open shows C; closed, hidden; the item's own letter hides the line. "None" hides it; Custom "Show the second line" off hides it whatever the picker says.
- No label in either frequency editor contains "A/B", "VFO A", "VFO B", "RX1" or "RX2".
- Decoding a needle with `{"direction":"tx"}` gives `{"binding":100}` and no `direction`; `{"direction":"rx"}` gives `binding` 0; a bar with `direction` and no `binding` gives 102 (`TxSwr`); decoding twice gives the same result; a needle with `binding` keeps it.
- Every other entry decodes byte-identical (existing `tst_container_document` cases pass).

**Verification:** data migration on load (decode tests first) plus GUI behaviour. Build and run per Global Constraints: `tst_longpath_instruments`, `tst_container_document`.

**Execution note (advisory):** opus. Prerequisite tasks: none. `ContentEntryMigration.cpp` includes `gui/instruments/LongpathContent.h` (same GUI library target); keep that include in this one file.

- [ ] **Step 1:** Decode-migration and second-line tests; see them fail.
- [ ] **Step 2:** `ContentEntryMigration` and the `decodeEntry` call.
- [ ] **Step 3:** Second-line choice, follow rules, Custom labels.
- [ ] **Step 4:** Build, run, commit signed: `feat(longpath): second frequency line picks a slice; migrate needle direction on load`.

## Task 64: TX section items show, hide and reorder their controls

**Requirements:**
- Ruling D10: keep the row look; add per-entry show/hide of each row's controls and their order, persisted and editable. Audit Part E "Which buttons show" and "Reorder buttons" and open question 6. Builds on Task 3.
- Reuse the TX Keying pattern: a config key read by the factory, applied by a section setter, declared as a Toggle (`TxSectionContent.cpp:45-55`, `:131-142`; `TxKeyingSection.cpp:221-228`). Dynamic visibility goes through the same AND (`TxApplet.cpp:2174`, `:2194`).
- Parts: a line part sits in a shared line; a column part owns whole items of the section's column. A part's hide key is `hide` plus its id with the first letter uppercased; existing keys keep their names. Toggles are labelled "Hide <part label>".

| Type | Line parts | Column parts | Section source |
|---|---|---|---|
| tx.monitor | line 1: `mon` "MON button", `speakers` "Speakers button", `phones` "Phones button" | `notice` "Silent-monitor notice" (dynamic, `TxMonitorSection.cpp:250`), `volume` "Monitor volume" | `TxMonitorSection.cpp:127-222` |
| tx.power | none | `forwardGauge` "Forward power gauge", `swrGauge` "SWR gauge", `rfPower` "RF power", `tunePower` "Tune power" | `TxPowerSection.cpp:130-215` |
| tx.filter | line 1: `caption` "TX BW label", `low` "Low cutoff", `high` "High cutoff" | `status` "Filter status" | `TxFilterSection.cpp:120-179` |
| tx.chain | line 1: `lev` "LEV button", `eq` "EQ button", `cfc` "CFC button" | `profile` "TX profile" | `TxChainSection.cpp:135-209` |
| tx.vox | line 1: `vox` "VOX button", `threshold` "Threshold and level", `hold` "Hold" | none | `TxVoxSection.cpp:118-185` |
| tx.processing | line 1: `twoTone` "2-Tone button", `psa` "PS-A button" (dynamic, `TxApplet.cpp:2247`, `:3692`), `dup` "DUP button" | `swrProtection` "SWR protection" | `TxProcessingSection.cpp:120-207` |
| tx.keying | line 1: `tune` "TUNE button", `mox` "MOX button" | `sliceIndicators` (existing key `hideSliceIndicators`: letters, holder label and Take transmit together, as `setSliceIndicatorsHidden` does) | `TxKeyingSection.cpp:119-180` |

- Order: config key `order`, an array of part ids. The column follows the list; a shared line takes the position of its first listed part and moves as a unit; within a line, parts follow the list; unknown ids are ignored; unlisted parts keep their built position after the listed ones; an order equal to the built order is not written.
- Editor: new `ContentProperty::Kind::Order`, a reorderable list of part labels with Move up and Move down (object name `property_order`); its tooltip says controls on one line stay together. Existing declared properties keep their positions; new ones are appended in part order, then "Order".
- Advanced bindings for `tx.*` stays disabled (`ContentPropertyEditor.cpp:392`), with the tooltip "These controls follow the TX panel; they have no source of their own." The factories never read `entry.context`.

**Files:**
- Create: `src/gui/applets/SectionParts.h`/`.cpp` (NereusSDR-original), registered in the root `CMakeLists.txt` beside `src/gui/applets/TxSectionContent.cpp` (`:2213`)
- Modify: `src/gui/containers/ContainerContentRegistry.h` (`Kind::Order` in `ContentProperty::Kind`, `:46`), `src/gui/containers/ContentPropertyEditor.cpp` (`declaredControl` builds the Order control; the Advanced bindings rule)
- Modify: `src/gui/applets/TxMonitorSection.cpp`/`.h`, `TxPowerSection.cpp`/`.h`, `TxFilterSection.cpp`/`.h`, `TxChainSection.cpp`/`.h`, `TxVoxSection.cpp`/`.h`, `TxProcessingSection.cpp`/`.h`, `TxKeyingSection.cpp`/`.h` (register parts; `parts()` accessor)
- Modify: `src/gui/applets/TxSectionContent.cpp` (factories apply the config; types declare toggles and Order)
- Modify: `src/gui/applets/TxApplet.cpp` (`:2174`, `:2194`, `:2247`, `:3692` through `setDynamicVisible`)
- Test: extend `tests/tst_tx_section_content.cpp` (`keyingCanHideTheSliceIndicators` changes its size check from 1 to 4 and keeps index 0 `hideSliceIndicators`), `tests/tst_container_document.cpp`

**Interfaces:**
- Consumes: Task 3 (no "Unsupported configuration" for known widget types); `ContainerContentRegistry::declaredProperties`.
- Produces: `class SectionParts { void add(const QString& id, const QString& label, int line, QList<QWidget*> widgets); void apply(const QJsonObject& config); bool isHidden(const QString& id) const; void setDynamicVisible(const QString& id, bool allowed); static QVector<ContentProperty> properties(const SectionParts&); }` (`line` -1 = column part); `ContentProperty::Kind::Order`.

**Acceptance:**
- Default look: with no new keys every `tx.*` copy is geometry-identical, at a fixed width, to one built before this task; the TX applet's own panel is untouched (`apply` runs only in the container factories).
- Each hide key hides exactly its part; `hideSliceIndicators` behaves as today.
- A hidden part stays hidden when applet logic would show it (PS-A on a PureSignal board, the monitor notice with phones missing, the holder line and Take transmit on a remote window).
- `order: ["mox","tune"]` on tx.keying puts MOX left of TUNE; `["sliceIndicators","tune","mox"]` puts the letters above the buttons; `["hold","vox","threshold"]` on tx.vox reorders the line.
- The editor shows the toggles and the Order list for every `tx.*` type and writes the keys; a default order writes no `order`; no "Unsupported configuration"; Advanced bindings is disabled with the tooltip.
- The config survives a round trip through the workspace store.
- Hiding every part leaves an empty row with no crash; the entry stays listed and editable.

**Verification:** GUI layout on live TX controls; geometry tests. Build and run per Global Constraints: `tst_tx_section_content`, `tst_container_document`. UI evidence per `ui-verification`: a container with a reordered tx.keying and a hidden tx.monitor part, light and dark, at the end-of-branch UI pass.

**Execution note (advisory):** opus. Prerequisite tasks: 3. Tests never toggle MOX or TUNE; use the `RadioModel` + `TxApplet` fixture already in `tst_tx_section_content`. Keep each widget's stretch factor when re-added to its line.
**TX-path flag:** this rearranges and hides keying-adjacent controls (MOX, TUNE, VOX, PS-A, the slice indicators and Take transmit) in container copies. It changes no keying logic and never keys.

- [ ] **Step 1:** Default-geometry test for all seven types; hide, order and editor tests.
- [ ] **Step 2:** `SectionParts` and `Kind::Order` with the editor control.
- [ ] **Step 3:** Register parts in the seven sections; route the dynamic calls; declare the properties.
- [ ] **Step 4:** Build, run, commit signed: `feat(containers): TX section items show, hide and order their controls`.

## Task 65: RX section items show, hide and reorder their controls

**Requirements:**
- Ruling D10, the Task 64 contract for `src/gui/applets/RxSectionContent.cpp`. The RX filter's keys `hideTop` and `hideBottom` (`RxSectionContent.cpp:55-88`; applied `RxControlSections.cpp:350-353`) become the hide keys of parts `top` and `bottom`, with names and labels unchanged. `rx.mode` has one control (`RxControlSections.cpp:277-288`), gets no parts and stays "nothing to configure" (Task 3).

| Type | Line parts | Column parts | Section source |
|---|---|---|---|
| rx.antennas | line 1: `rxAntenna` "RX antenna button", `txAntenna` "TX antenna button" (dynamic, `RxApplet.cpp:2040-2041`, `:3144-3145`) | none | `RxControlSections.cpp:257-275` |
| rx.step | line 1: `caption` "STEP label", `down` "Step down", `value` "Step size", `up` "Step up" | none | `:290-312` |
| rx.filter | none | `top` "Width label" (key `hideTop`), `presets` "Preset buttons" (new `hidePresets`), `bottom` "Passband" (key `hideBottom`) | `:314-353` |
| rx.pan | line 1: `left` "L label", `slider` "Pan slider", `right` "R label" | none | `:390-409` |
| rx.squelch | line 1: `button` "SQL button", `slider` "Squelch level" | none | `:411-430` |
| rx.attenuator | line 1: `caption` "ATT label", `control` "Attenuator control" | `preamp` "Preamp check box" (dynamic, `RxApplet.cpp:2076`, `:3204`) | `:432-466` |
| rx.agc | line 1: `mode` "AGC mode", `caption` "AGC-T label", `value` "Threshold value", `auto` "AUTO button" | `slider` "Threshold slider", `info` "AGC info" (dynamic, `RxApplet.cpp:3228`) | `:468-516` |
| rx.rit | line 1: `on` "RIT button", `zero` "Zero button", `minus` "Step down", `offset` "Offset", `plus` "Step up" | none | `:518-547` |
| rx.xit | line 1: `on` "XIT button", `zero` "Zero button", `minus` "Step down", `offset` "Offset", `plus` "Step up" | none | `:549-578` |

- Advanced bindings for `rx.*` is disabled with the tooltip "These controls follow the RX panel; they have no source of their own." The RX factories never read `entry.context` (no `context` reference in `RxSectionContent.cpp`), so the Source combo `rx.filter` shows today has no effect and goes.
- The row text "RX1 preamp" (`RxControlSections.cpp:458`) stays, because D10 keeps the row look; only the editor label ("Preamp check box") avoids it (appendix item).

**Files:**
- Modify: `src/gui/applets/RxControlSections.cpp`/`.h` (register parts), `src/gui/applets/RxSectionContent.cpp` (factories apply config; types declare toggles and Order), `src/gui/applets/RxApplet.cpp` (`:2040-2041`, `:2076`, `:3144-3145`, `:3204`, `:3228` through `setDynamicVisible`; `createFilterSection` keeps calling `applyConfig`)
- Modify: `src/gui/containers/ContentPropertyEditor.cpp` (Advanced bindings rule extended to `rx.`)
- Test: extend `tests/tst_rx_section_content.cpp` (the size checks at about `:240-252` and `:518-519` go from 8 to 10; indices 0-7 unchanged)

**Interfaces:**
- Consumes: Task 64 (`SectionParts`, `Kind::Order`).
- Produces: nothing new.

**Acceptance:**
- With no new keys every `rx.*` copy is geometry-identical to before; the RX applet's own panel is untouched.
- `hideTop` and `hideBottom` behave as today; `hidePresets` hides the preset grid.
- A hidden antenna button stays hidden when antennas become available; a hidden preamp check box stays hidden on a board with a per-ADC preamp.
- `order: ["plus","offset","minus","zero","on"]` on rx.rit reverses the line; `["bottom","presets","top"]` on rx.filter puts the passband first.
- The editor writes the keys for each type with parts; rx.mode shows no item group and no "Unsupported configuration"; no editor label says RX1 or RX2.
- The config survives a round trip through the store.

**Verification:** GUI layout on live RX controls; geometry tests. Build and run per Global Constraints: `tst_rx_section_content`. Include `rx.*` in the end-of-branch UI capture.

**Execution note (advisory):** opus. Prerequisite tasks: 64.
**TX-path flag:** the `txAntenna` part hides or reorders the TX antenna button in container copies; it changes no antenna logic and never keys.

- [ ] **Step 1:** Default-geometry, hide, order and editor tests.
- [ ] **Step 2:** Register parts; route the dynamic calls; declare the properties; the Advanced rule.
- [ ] **Step 3:** Build, run, commit signed: `feat(containers): RX section items show, hide and order their controls`.

## Task 66: Container picker names and headings

**Requirements:**
- JJ's naming rulings: full maker and model names; headings instead of suffixes; plain words for building blocks; applet labels match their window titles; readable bar names; one "S-Meter" casing; no "VFO A/B" and no RX1/RX2; never change a typeId.
- Inventory: `/Users/j.j.boyd/NereusSDR-crew/container-item-parity/naming-inventory.md`, rechecked at HEAD. The live picker is the flat Available list (`ContainerSettingsDialog.cpp:682-692`, edit-session branch). Labels come from `ContainerContentRegistry::descriptors()` (`ContainerContentRegistry.cpp:243-326`), `registerWidgetFactory` calls and `supportedContainerControls()` (`ContainerControlCatalog.h:19`). The legacy grouped list (no edit session, `ContainerSettingsDialog.cpp:694-804`) is not changed.
- Headings, in order: Ready-made meters, Building blocks, Buttons, Panels, Panel sections, Longpath instruments. Within a heading entries keep `descriptors()` order. Heading rows are bold and non-selectable (`Qt::NoItemFlags`), styled as the legacy `addHeader`, and carry no `Qt::UserRole` data; `onAddFromAvailable` ignores an empty typeId. " — Move" on singletons stays.
- Saved entries: on load, an entry whose stored `name` equals the old default label of its typeId takes the new label; any other name is kept. This is a {typeId, old, new} table in `migrateEntryOnLoad` (Task 63).
- Label table (old → new; every typeId unchanged; rows not listed keep their label):

| typeId | Old label | New label | Heading |
|---|---|---|---|
| meter.radeMicWaveform, meter.radeSpeechWaveform, meter.powerSwr, meter.ananMulti, meter.crossNeedle, meter.magicEye, meter.signalText, meter.historyGraph, meter.vfoDisplay, meter.clock, meter.contest, meter.spacer, meter.leveler, meter.customBar | (unchanged) | keep | Ready-made meters |
| meter.sMeter | S-meter bar (additional) | S-Meter bar | Ready-made meters |
| meter.comp | Comp bar | Compression bar | Ready-made meters |
| meter.eq | Eq bar | EQ bar | Ready-made meters |
| meter.cfc | Cfc bar | CFC bar | Ready-made meters |
| meter.cfcGain | CfcGain bar | CFC gain bar | Ready-made meters |
| meter.levelerGain | LevelerGain bar | Leveler gain bar | Ready-made meters |
| meter.alcGain | AlcGain bar | ALC gain bar | Ready-made meters |
| meter.alcGroup | AlcGroup bar | ALC group bar | Ready-made meters |
| meter.agc | Agc bar | AGC bar | Ready-made meters |
| meter.agcGain | AgcGain bar | AGC gain bar | Ready-made meters |
| meter.signal | Signal bar | Signal peak bar | Ready-made meters |
| meter.signalAvg | SignalAvg bar | Signal average bar | Ready-made meters |
| meter.signalMaxBin | SignalMaxBin bar | Signal max bin bar | Ready-made meters |
| meter.adc | Adc bar | ADC bar | Ready-made meters |
| meter.adcMax | AdcMax bar | ADC maximum bar | Ready-made meters |
| meter.pbSnr | PbSnr bar | PB SNR bar | Ready-made meters |
| meter.mic | Mic | Mic bar | Ready-made meters |
| meter.alc | ALC | ALC bar | Ready-made meters |
| meter.pwr, meter.reversePwr, meter.swr (Task 14) | (new) | Power bar, Reverse power bar, SWR bar | Ready-made meters |
| sMeter.dial | S-Meter dial (additional) | S-Meter dial | Ready-made meters |
| BAR, IMAGE, SCALE, TEXT, NEEDLE, SPACER, LED, DIAL, ROTATOR, CLOCK | (unchanged) | keep | Building blocks |
| SOLID | SolidColour | Solid color | Building blocks |
| FADECOVER | FadeCover | Fade cover | Building blocks |
| HISTORY | HistoryGraph | History graph | Building blocks |
| MAGICEYE | MagicEye | Magic eye | Building blocks |
| NEEDLESCALEPWR | NeedleScalePwr | Power scale | Building blocks |
| SIGNALTEXT | SignalText | Signal text | Building blocks |
| TEXTOVERLAY | TextOverlay | Text overlay | Building blocks |
| WEBIMAGE | WebImage | Web image | Building blocks |
| FILTERDISPLAY | FilterDisplay | Filter display | Building blocks |
| VFO | VfoDisplay | VFO display | Building blocks |
| CLICKBOX | ClickBox | Click box | Building blocks |
| DATAOUT | DataOut | Data output | Building blocks |
| BANDBTNS | BandButton | Band buttons | Buttons |
| NEREUSBAND | NereusBandButton | Band buttons (panel style) | Buttons |
| MODEBTNS | ModeButton | Mode buttons | Buttons |
| FILTERBTNS | FilterButton | Filter buttons | Buttons |
| ANTENNABTNS | AntennaButton | Antenna buttons | Buttons |
| TUNESTEPBTNS | TuneStepButton | Tune step buttons | Buttons |
| OTHERBTNS | OtherButton | Other buttons | Buttons |
| control.mox … control.duplex (15) | MOX, Tune, Monitor, 2Tone, PureSignal, ANF, SNB, MNF, Peak, CTUN, VAX 1, VAX 2, Mute, Binaural, Duplex | keep | Buttons |
| VOICERECPLAY | VoiceRecordPlay | Voice record and play | Buttons |
| DISCORDBTNS | Discord control (removed) | keep | Buttons |
| applet:rx, applet:Display, applet:TX, applet:PHCW, applet:RADE, applet:vax, applet:pure_signal, applet:cat, applet:rotor, applet:s_meter | (unchanged) | keep | Panels |
| applet:mod_monitor | Mod Monitor | AM Mod Monitor | Panels |
| applet:tci | TCI | TCI Server | Panels |
| applet:tci_clients | Client Chain | TCI Clients | Panels |
| applet:amp | Amplifier | 4O3A Power Genius | Panels |
| applet:tuner | Tuner | 4O3A Tuner Genius | Panels |
| applet:RfKit | RF Kit | RF-Kit RF2K-S | Panels |
| tx.monitor … tx.keying (7), rx.antennas … rx.xit (10) | (unchanged) | keep | Panel sections |
| longpath.frequency | Frequency (Longpath) | Frequency | Longpath instruments |
| longpath.frequencyCustom | Frequency (Custom) | Custom frequency | Longpath instruments |
| longpath.needle | Needle meter (Longpath) | Needle meter | Longpath instruments |
| longpath.bar | Bar meter (Longpath) | Bar meter | Longpath instruments |
| longpath.filter | Filter passband (Longpath) | Filter passband | Longpath instruments |

- Window titles confirmed: AM Mod Monitor (`ModMonitorApplet.h:79`), TCI Server (`TciApplet.h:71`), TCI Clients (`ClientChainApplet.h:74`), RF-Kit RF2K-S (`Rf2ksApplet.h:69`). The amp and tuner take JJ's "4O3A …" names, not their window titles.
- Control shortcuts keep their labels: they are saved as OTHERBTNS, so a name rule keyed by typeId cannot tell them apart.

**Files:**
- Modify: `src/gui/containers/ContainerContentRegistry.h` (`enum class ContentGroup { ReadyMadeMeters, BuildingBlocks, Buttons, Panels, PanelSections, LongpathInstruments }`; `ContentDescriptor` gains `ContentGroup group` as its last member, defaulted; `static QString groupTitle(ContentGroup)`)
- Modify: `src/gui/containers/ContainerContentRegistry.cpp` (labels; bar variant titles from a readable-name map instead of `variant + " bar"`; a group per descriptor; widget-factory descriptors take their group from the typeId prefix `tx.`/`rx.`/`longpath.`/`sMeter.`)
- Modify: `src/gui/instruments/LongpathContent.cpp` (its registered labels), `src/gui/instruments/SMeterDialContent.cpp` (label and comment at about `:334-337`)
- Modify: `src/gui/containers/ContainerSettingsDialog.cpp` (`populateAvailableList` edit-session branch; `onAddFromAvailable`)
- Modify: `src/gui/containers/ContentEntryMigration.cpp` (the name rule)
- Create: `tests/tst_container_picker_catalog.cpp`, registered beside `tst_container_document`
- Modify tests: `tests/tst_longpath_instruments.cpp` and `tests/tst_smeter_dial_content.cpp` (label expectations); `tests/visual/PreviewScenes.cpp` (find the first row whose `Qt::UserRole` starts with `longpath.` instead of the text "(Longpath)"; the text "S-Meter dial"); `tests/visual/build_gallery.py` (caption wording)

**Interfaces:**
- Consumes: Task 63 (`migrateEntryOnLoad`); Task 14 (the three new bar typeIds); Task 58 (control catalog). Tasks 59-65 edit the same test files, so this task runs after them.
- Produces: `ContentDescriptor::group`, `ContainerContentRegistry::groupTitle(ContentGroup)`.

**Acceptance:**
- With workspace preview on and every widget type registered, each descriptor's title and group equal the table. No title contains "(Longpath)", "(Custom)", "(additional)", "VFO A", "VFO B", "RX1" or "RX2"; no two titles under one heading are equal.
- In an edit session the Available list shows the six headings in order, each followed only by its entries; headings cannot be selected and carry no typeId; Add or double-click on a heading adds nothing. Existing picker tests that search by `Qt::UserRole` (`tst_container_settings_workflow`, `tst_free_canvas`, `tst_container_preview`, `tst_tx_section_content`) pass unchanged.
- Every table row's new label equals the registry's title for that typeId.
- Load renaming: an entry with the old label decodes with the new one; a user name (for example "Shack amp" on `applet:amp`) is kept; an entry already on the new label is unchanged; decoding twice is idempotent.
- No typeId string changes: a diff of all typeId literals before and after is empty.

**Verification:** user-visible strings plus a load-time rename; rename tests first. Build and run per Global Constraints, one at a time: `tst_container_picker_catalog`, `tst_container_document`, `tst_longpath_instruments`, `tst_smeter_dial_content`, `tst_container_settings_workflow`, `tst_tx_section_content`, `tst_rx_section_content`. Build `tst_workspace_colour_sweep` to prove `PreviewScenes.cpp` compiles; running it belongs to the end-of-branch UI pass with a capture of the grouped Available list (`ui-verification`).

**Execution note (advisory):** opus. Prerequisite tasks: 14, 58, 59, 60, 61, 62, 63, 64, 65. Never edit a typeId.

- [ ] **Step 1:** Catalog and load-rename tests; see them fail.
- [ ] **Step 2:** Labels, groups, the bar title map, Longpath and dial labels.
- [ ] **Step 3:** Grouped Available list and the empty-typeId guard.
- [ ] **Step 4:** The rename table in `migrateEntryOnLoad`; update the existing tests and scenes.
- [ ] **Step 5:** Build, run, commit signed: `feat(containers): plain picker names under headings`.

## Appendix: follow-up plan (not in this build)

Each item below is out of this build's scope, with the reason. None blocks a task above.

### Thetis Other buttons not shipped here

| Thetis ids (numbers) | Why not here |
|---|---|
| POWER (0) | Omitted by maintainer decision (`OtherButtonItem.h:80-82`); not a follow-up. |
| RX_2 (1), SUBRX (133), PAN_SWAP (134), SAF± (155-156), SBAL± (157-158) | Two-receiver layout (`UnbuiltFeature::TwoReceiverLayout`). |
| XPA (8) | `UnbuiltFeature::OcExtras`. |
| REC (9), PLAY (10), WAVE_RECORD (169) | Voice record and play is a separate design study (ruling). PLAY can key: Thetis MoxOnPlayback defaults to true (`clsAudioRecordPlayback.cs:218`, `:548`). |
| ATT_STEP … ATT- (44-52) | Per ADC. With the step attenuator off, Thetis steps per-board preamp presets (`console.cs:53207` onward); needs a rule beside `StepAttenuatorFacade::setAttenuationDbForSlice`. |
| SPLT, A>B, 0 Beat, A<B, IF>V, A<>B, SYNC, LockB (64-69, 74, 76) | No VFO pair; split was removed by design (`RadioModel::split(int)` always false). Needs a slice-pair rule. |
| STACK± (79-80) | `UnbuiltFeature::BandStack`. |
| AVG (96) | `UnbuiltFeature::DisplayAveraging`. |
| Display modes (99-109) | `UnbuiltFeature::DisplayMode`. |
| PAUSE (110) | No display pause control. |
| PAN+/-/Centre (115-117), 0.5x/1x/2x/4x (119-122) | Zoom is continuous per pan (`zoomBy`); needs a preset rule. |
| SQL (135), Reg SQL (136), SQL± (138-139) | NereusSDR's SQL is voice squelch; AM and FM squelch are per mode and need a mapping. |
| MIC (140) | No desktop control drives `TransmitModel::setMicMute`; remote reach unverified. |
| RX_EQ (144), FORM_EQ (292) | `UnbuiltFeature::Equalizer`. |
| TX_FILTER (146) | `setTxFilterVisible` exists but nothing saves `DisplayDrawTxFilter`; needs a persistence decision. |
| DITHER, RANDOM (224-225) | No user setting. |
| SR 48k-1536k (226-231) | Sample rate is per stream and station-gated (`RadioModel::setStreamSampleRate`); needs a rule for which stream. |
| FORM_DBMAN (289), FORM_WB (298) | No matching window action. |
| FORM_MEMORY (290), FORM_XVTR (293) | `UnbuiltFeature::Memories`, `UnbuiltFeature::Transverters`. |
| FORM_AMPVIEW (291) | AmpViewWindow opens only from PsForm (`PsForm.cpp:1267`). |
| FORM_CWX (294), CWX KEY/STOP/F1-F9 (320-330) | `UnbuiltFeature::Cwx`. CWX KEY keys the transmitter. |
| Macros M1-M31 (352-382) and the macro editor | `UnbuiltFeature::MacroButtons`; settings not serialized, no editor (`OtherButtonItem.h:93-101`); the CAT macro type needs `UnbuiltFeature::Cat`. Thetis editor: `ucOtherButtonsOptionsGrid.cs:260-520`. |
| Right click opens a Setup tab (`console.cs:51623` onward); ZTB right click stores (`:51548`) | Needs a mapping to NereusSDR Setup pages. |
| Button icons (icon_on/off) | Not planned: NereusSDR's look uses text. |

### Other follow-ups

- `meter.adcMax` provider: parse ADC_MAX_MAG from Protocol 2 high-priority bytes 35-38 (`console.cs:47057`, `:47221`; `netInterface.c:159-166`; `network.c:721-722`) in the core and feed the bar (Task 15 shows the reason until then).
- LED conditions: full C# expressions (Thetis compiles with Roslyn, `clsMeterScriptEngine.cs:325-381`) and CAT variables (Task 26 ports a defined subset).
- S9 frequency setting: Thetis lets the operator pick 30 or 144 MHz (`setup.cs:29395-29405`, `console.cs:48112-48118`); this build fixes 30 MHz (Task 8).
- Image dark and size variants as shipped assets (Task 16 looks them up when present); Thetis rotator skin images (`MeterManager.cs:16507-16521`, not in the Thetis repository).
- TX filter match: `MatchTXFilterToRXFilter`/`SetTXFilter`/`UpdateTXLowHighFilterForMode` (`console.cs:34901-34921`, `:17839-17870`); filter grid clicks set the RX filter only (Task 51). Upstream `SetTXFilter` indexes RX2 filters by `_rx1_dsp_mode`, a bug to avoid if ported.
- VFO split and sync (Tasks 32 and 36 draw them greyed); band stack from the VFO long press and the band grid right click (Tasks 35 and 36).
- Thetis item-level import: `fromThetisDisplayMode` (Task 32) and `translateThetisOverlayText` (Task 39) exist with no caller; `LegacyContainerImporter` reads only container metadata.
- Overlay tokens: QSO timer, CAT and Discord (Task 39).
- The "RX1 preamp" row text in `rx.attenuator` (`RxControlSections.cpp:458`), kept by ruling D10 (Task 65).
- The VOICERECPLAY item's options, with the voice record and play study.
- Container Lock in the workspace look; enabled or hidden by macro.
- Thetis button-box styling (on/off/fill/hover/click colours, fonts, indicator styles): not planned, by ruling.
