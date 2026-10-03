# Containers and objects implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Nereus containers, meter objects and applets reliably editable,
arrangeable and recoverable, with the accepted meter appearance and motion.

**Architecture:** Keep ContainerManager and add a GUI-owned presentation document,
one content registry, and transactional persistence. Views reconcile committed
documents; editor previews use drafts. Recover selected composite meter classes,
then integrate their reading, timing and rendering contracts with current main.

**Tech Stack:** C++20, Qt 6, QtTest, CMake/Ninja, existing AppSettings, existing
CPU/QRhi meter renderers; no additional runtime dependency.

**Spec:** [Approved design](2026-10-02-containers-and-objects-design.md).
**Status:** Approved for crew and cost-aware implementation on 2026-10-02.
**Inspected base:** Nereus `origin/main` at `dd53da5af` (2026-10-02).

## Global Constraints

- Use current main's `CLAUDE.md`: **Study, then choose.** Record reference findings,
  architectural choices and deliberate departures; preserve attribution.
- No DSP or protocol behavior change is part of this design.
- Layout operations must not transmit, change radio settings, or create another
  DSP instance. Presentation state is client-authoritative.
- Nothing in `src/core/` or `src/models/` acquires a GUI dependency (rule R1).
- Use AppSettings, Qt ownership/RAII, logging and the project's thread rules.
- Preserve legacy geometry, complete configuration and opaque unavailable data.
- New mixed containers use an ordered vertical layout; arbitrary nesting and
  splitting individual controls out of applets are deferred.
- Header modes are Always visible, Reveal on hover/focus, and Hidden with an
  explicit recovery gesture/menu. Reserve the header extent in this revision.
- The prototype's 50 ms synthetic sampling is not an approved production default.
- Actual Windows GPU reparenting and macOS/Linux native drag evidence is required
  before claiming cross-platform completion.

## Review Focus

1. Truncated, delimiter-ambiguous, customized or future-version saved data must
   survive attempted migration without destructive partial loading (Tasks 1–2).
2. Hidden/unavailable singleton applets, late session attachment and mismatched
   legacy IDs must keep ownership and order without duplicate views (Tasks 2, 6).
3. Canceled moves, deleted return destinations and mixed pop-out shells must
   transfer every object atomically without keying the radio (Tasks 6–7).
4. External changes while settings is open, failed saves and live applet previews
   must leave committed state/connections intact (Tasks 1, 8–9).
5. Unchanged readings, missing channels, TX transitions and GPU reconstruction
   must seed and animate the correct face without stale state (Tasks 3–5, 10).

## Execution preparation and evidence

Read both documents and current `CLAUDE.md`, `docs/development/fast-test-loop.md`,
and `docs/attribution/HOW-TO-PORT.md`. Select the execution method before coding.
At execution, fetch origin, inspect changes after `dd53da5af`, and use an isolated
`codex/` branch/worktree from latest main under the worktree workflow. Carry these
two documents into it. Preserve the primary checkout's unrelated EQ/CFC/manual
work and all other worktrees. Do not merge the old refactor branch wholesale.

Historical recovery material: `15de6fbecf`, April `50c5246cdb`; interaction study:
Aether `5766bb13ef9ec8df3cf714f2c2fae2e17a7465ab`; meter reference: Thetis
`v2.10.3.15`. Source lines below refer to these snapshots, not the stale primary
checkout. Read source at the executor's actual base before editing it.

Configure a dedicated test build using available Qt paths; do not reinstall
dependencies just to plan. Use `RelWithDebInfo`, tests ON, GPU ON, LTO OFF:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON -DNEREUS_GPU_SPECTRUM=ON -DNEREUSSDR_ENABLE_LTO=OFF
```

Every new target below is registered through `nereus_add_test` in
`tests/CMakeLists.txt`; source files enter `GUI_SOURCES` in root `CMakeLists.txt`.
Build matching test targets before each ctest run. First failing runs may fail
at compilation for the absent interface; subsequent assertions must expose the
behavioral defect. Tests use temporary AppSettings files and existing sandbox
initialization, never the operator's real settings or an attached transmitter.

Each task ends with an explicit commit of its named files after verification;
avoid `git add .`. Record test commands, reference decisions and visual artifacts
in `docs/architecture/2026-10-02-containers-and-objects-validation.md` as work runs.
That validation file is created by Task 1 and completed by Task 10.

## File and interface map

All new product files below live under `src/gui/`; the document is deliberately
independent of QWidget, not relocated into Core.

| Files | Responsibility |
| --- | --- |
| `containers/ContainerTypes.h`, `ContainerDocument.{h,cpp}` | Shared enums and value-only workspace, content and placement records |
| `containers/ContainerDocumentCodec.{h,cpp}` | Versioned lossless JSON, validation and extension preservation |
| `containers/ContainerWorkspaceStore.{h,cpp}` | Revisioned AppSettings load/commit with rollback on save failure |
| `containers/LegacyContainerImporter.{h,cpp}` | Legacy keys/files/clipboard conversion, raw backups and explicit aliases |
| `containers/ContainerContentRegistry.{h,cpp}` | Descriptors, one meter factory, singleton attachment, inert preview factories |
| `containers/ContainerContentHost.{h,cpp}` | Legacy meter canvas or mixed stack and view reconciliation |
| `containers/ContainerArrangeController.{h,cpp}` | Atomic order/move/pop-out/return/delete commands |
| `containers/ContainerEditSession.{h,cpp}`, `ContainerPreviewWidget.{h,cpp}` | Draft transactions, conflict detection and safe preview |
| `meters/presets/*`, `meters/MeterDynamics.{h,cpp}` | Recovered complete faces and deterministic presentation dynamics |
| Existing `MeterItem`, `MeterWidget`, `MeterPoller` | Binding fan-out, replay, cadence, reset, scale and paint invalidation |
| Existing `ContainerManager`, `ContainerWidget`, `AppletPanelWidget`, `MainWindow` | Committed presentation integration, existing applet/model connections |
| Existing `ContainerSettingsDialog`; new `containers/editors/ContentPropertyEditor.{h,cpp}` | Available/Contents/Properties and working contextual controls |

### Shared data contract (defined by Task 1)

`ContainerTypes.h` retains existing numeric `DockMode` and `AxisLock` values from
ContainerWidget; adds `HeaderMode { Always, Reveal, Hidden }` and
`ContentLayout { LegacyCanvas, VerticalStack }`.

`ContentEntry`: `QString id`, `typeId`, `name`; `QJsonObject context`, `config`,
`extensions`; `QRectF canvasRect`; `int paintOrder`; `bool visible`; optional
`ReturnLocation { QString containerId, beforeId, afterId; }`. Context preserves
existing RX source, slice/session association and MMIO. Config holds typed,
validated known properties; unknown fields remain alongside them unchanged.

`ContainerDocument`: `QString id`, `name`; ordered `QVector<ContentEntry> contents`;
`ContentLayout layout`; `DockMode dockMode`; `AxisLock anchor`; `HeaderMode header`;
`QRect geometry`; `bool visible`, `locked`, `autoHeight`, `popOutShell`;
`QJsonObject config`, `extensions` (including RX/TX policy and splitter state).

`WorkspaceDocument`: `int schemaVersion = 1`; `quint64 revision`;
`QString mainContainerId`; `QVector<ContainerDocument> containers`;
`QJsonObject extensions`. UUID strings identify new content; keep existing
container IDs. Ordered vectors are content order; `paintOrder` is separate.

Results use `DocumentResult { bool ok; WorkspaceDocument document; QString error; }`
and `CommitResult { CommitStatus status; quint64 revision; QString error; }`, with
`CommitStatus { Saved, Conflict, Invalid, StorageError }`. Future schema versions
are recoverable/read-only, never partially interpreted and overwritten.

## Implementation tasks

### Task 1: Lossless document and atomic client-local persistence

**Files:** Create the four document/types/codec/store units above; validation log;
`tests/tst_container_document.cpp`, `tests/tst_container_workspace_store.cpp`.
Modify `ContainerWidget.h` (shared enum include), both CMake files; extend
`tests/tst_settings_scope.cpp` without changing the Core classifier.

**Interfaces:** Produce
`ContainerDocumentCodec::decode(const QByteArray&) -> DocumentResult`,
`encode(const WorkspaceDocument&) -> QByteArray`,
`validate(const WorkspaceDocument&) -> QString` (empty means valid), and
`ContainerWorkspaceStore(AppSettings&, QObject* parent = nullptr)` with
`load() -> DocumentResult`, `snapshot() const -> WorkspaceDocument`,
`commit(const WorkspaceDocument&, quint64 expectedRevision) -> CommitResult`,
signal `committed(quint64 revision)`. Store owns the current document, not widgets.

- [x] Write round-trip/validation tests for duplicate IDs, names, context/MMIO,
  extensions at every level, geometry, order, malformed JSON and future versions:
  `QCOMPARE(decode(encode(doc)).document, doc);` and reject duplicate IDs before
  changing the live snapshot. Define equality on document value types.
- [x] Write store tests: saved revision advances once; stale expected revision
  returns Conflict; force a save failure through an unwritable destination and
  assert snapshot/owned AppSettings keys unchanged. Assert new keys classify as
  `SettingsScope::OperatorLocal`; fake remote backend receives zero layout writes.
- [x] Build/run `tst_container_document` and `tst_container_workspace_store`;
  expect failing assertions or missing interfaces before implementation.
- [x] Implement codec and store. Use `ContainerWorkspace` for structured JSON and
  `ContainerWorkspaceBackup` for the recoverable original. Preserve unknown JSON.
  Validate before writing; save through existing atomic `AppSettings::save`;
  roll back only these owned keys on failure; publish/emit only after success.
  Do not clear legacy keys. Never replace a corrupt newer document silently.
- [x] Build both targets plus `tst_settings_scope`, `tst_core_has_no_gui_includes`;
  run ctest matching `^tst_(container_document|container_workspace_store|settings_scope|core_has_no_gui_includes)$`.
  Expected: all pass with temporary settings, no remote station mutation.
- [x] Commit: `feat: add revisioned container presentation documents`.

### Task 2: One registry and non-destructive legacy import

**Files:** Create registry/importer units, `tests/tst_container_legacy_import.cpp`,
`tests/fixtures/containers/` fixtures; modify MeterWidget's legacy decode dispatch
and ContainerSettingsDialog's duplicated create/clone paths; wire importer into
ContainerWorkspaceStore's absent-document load path. Keep ItemGroup's
legacy adapter until all its consumers use the registry.

**Interfaces:** Consume Task 1 types. Produce
`LegacyContainerImporter::fromSettings(const AppSettings&) -> DocumentResult`,
`fromContainerFile(const QByteArray&) -> DocumentResult`,
`fromClipboard(const QString&) -> DocumentResult`;
`ContentDescriptor { QString typeId, title; bool singleton, available;
QString unavailableReason; }`;
`ContainerContentRegistry::descriptors() const -> QVector<ContentDescriptor>`,
`makeEntry(const QString& typeId) const -> ContentEntry` (fresh ID/default config),
`validateEntry(const ContentEntry&) const -> QString`,
`createMeterItem(const ContentEntry&, QObject* parent,
ContentRenderMode mode = ContentRenderMode::Live) const -> MeterItem*`, where
`ContentRenderMode { Live, Preview, Validation }` suppresses actions/fetching in
non-live hydration. Also produce `captureMeterItem(const MeterItem&,
const ContentEntry& prior = {}) const -> ContentEntry` for live defaults/clones:
retain prior identity/raw extensions and capture MMIO/stack/visibility context.
QObject parenting transfers ownership; registry owns no meter widgets.

- [x] Capture non-default main/historical fixtures: calibrated bars, needles,
  MMIO, names, overlapping custom objects, absent property tails, unknown records,
  malformed record between valid siblings, and retired DISCORDBTNS.
  Assert all original bytes are recoverable; idempotent second load keeps IDs,
  order and customization; unknown/retired controls never instantiate.
- [x] Add explicit alias tests for visibility ID → floating widget ID:
  `Rx→rx`, `Display→Display`, `Tx→TX`, `PhoneCw→PHCW`, `Rade→RADE`, `Vax→vax`,
  `PureSignal→pure_signal`, `ModMon→mod_monitor`, `Tci→tci`,
  `ClientChain→tci_clients`, `Amp→amp`, `Tuner→tuner`, `RfKit→RfKit`.
  Assert hidden/floating/geometry combinations remain distinct. Give S-meter an
  explicit new singleton descriptor; do not invent a legacy float key for it.
- [x] Build/run `tst_container_legacy_import`; expect failure for destructive
  parse, incorrect aliases or absent importer.
- [x] Implement raw-first import: legacy item lines are newline/pipe delimited,
  with no general escaping. Keep each exact record in `config.legacyRecord`,
  edited known properties in `config.overrides`, and original complete payload
  in backup. Factory applies overrides after legacy decoding. Unparseable entries
  stay named opaque unavailable objects. Retired records stay inert. Promote a
  group only on full unambiguous signature with every property representable.
  Preserve `ContainerData_*`, `ContainerItems_*`, ID list, splitter, float keys.
- [x] Register current built kinds only; registry becomes the single factory
  behind deserialize, clone and preset creation. Future-schema import rejects
  editing without clearing current state. Build/run new target and
  `tst_container_persistence`, `tst_meter_presets`; expected pass and byte-preserved
  source backups. Run the R1 test after introducing includes.
- [x] Commit: `feat: preserve legacy container content through one registry`.

### Task 3: Composite reading, replay and timing contracts

**Files:** Modify `MeterItem.{h,cpp}`, `MeterWidget.{h,cpp}`,
`MeterPoller.{h,cpp}`; create `MeterDynamics.{h,cpp}` and
`tests/tst_meter_composite_dispatch.cpp`, `tests/tst_meter_dynamics.cpp`.

**Interfaces:** Produce virtual MeterItem methods
`readingBindings() const -> QSet<int>`, `pushBindingValue(int, double) -> void`,
`advanceMeter(qint64 monotonicMs) -> bool` (visual change),
`resetForTxTransition(bool inTx) -> void`, `setPowerScale(int watts) -> void`.
Default adapters retain primitive behavior. Preserve existing
`MeterWidget::updateMeterValue(int, double)` and `rescalePowerMeters(int)`;
add `advanceMeters(qint64) -> void`, `resetForTxTransition(bool) -> void`.
MeterPoller produces `frameAdvanced(qint64 monotonicMs)` once per existing poll.
Add `MeterPoller::setTargetContext(MeterWidget*, const QJsonObject&) -> void`
and `setRxReadingSource(std::function<double(const QJsonObject&, int)>) -> void`;
the GUI source adapter resolves existing cached local channels or remote slice
snapshots for that context, not new DSP calls. Default context retains main's
active-source behavior. Cache/replay keys include source context and binding.
No new polling timer or DSP reader is created for each face/preview.
Expose `replayReadings(MeterWidget*, const QJsonObject&) const -> void` and signal
`readingUpdated(const QJsonObject& context, int bindingId, double value)` for
read-only GUI previews; these do not add a Core/DSP subscription or poll target.

- [x] Write a two-channel test face receiving distinct peak/average bindings;
  assert both channels dispatch, unchanged latest values seed a newly added
  face, unavailable bindings propagate, and replacing a target does not double
  target count. Two fake slice contexts with distinct readings never cross-feed;
  changing source invalidates that target's stale replay. An absent slice/source
  yields no reading. Keep association local to the correct window/session.
- [x] Write deterministic timestamp tests for rise/release, history expiry and
  peak hold under existing supported intervals; repeated equal values still
  advance time. TX reset excludes hardware telemetry. Missing reading uses
  current no-reading semantics, never a fabricated valid zero sample.
- [x] Build/run both new tests; expect missing contracts/current stale replay
  behavior to fail.
- [x] Implement fan-out and replay before unchanged-value shortcuts; replay
  availability, MOX, units and PA scale when constructing views. Emit cadence
  after the existing poll paths, using a monotonic clock/test seam. Recover
  `pushBindingValue(int,double)` selectively; remove primitive-only reset/scale
  assumptions. Dynamics implement cited Thetis algorithms per face, with injected
  timestamps; keep the production interval/settings unchanged.
  Cache latest readings in the poller by context as well as in MeterWidget;
  `addTarget` replays cached values immediately. Source changes/disconnects clear
  affected cache entries and advertise no reading before another view is seeded.
- [x] Build/run new targets plus `tst_meter_item_bar`, `tst_meter_item_no_reading`,
  `tst_meter_poller_tx_bindings`, `tst_remote_meter_poller`,
  `tst_multimeter_timing`, `tst_multimeter_unit_conversion`. Expected: all pass;
  verify local/remote version-gated bindings are still unavailable as appropriate.
- [x] Commit: `fix: fan out and replay composite meter readings`.

### Task 4: Mic/ALC complete faces with reference motion

**Files:** Recover/repair `meters/presets/BarPresetItem.{h,cpp}` and
`PresetGeometry.h`; modify registry, meter render layers;
create `tests/tst_meter_bar_face.cpp`, checked-in numeric trace fixtures and
`docs/architecture/2026-10-02-meter-reference-matrix.md`.

**Interfaces:** Consume Task 3 virtual contracts. Produce
`BarPresetItem::configureAsMic() -> void`, `configureAsAlc() -> void`,
and `configureAsCustom(int bindingId, double minV, double maxV,
const QString& label) -> void`; register `meter.mic`, `meter.alc`,
`meter.customBar` with complete structured configuration. Each exposes all
internal bindings through `readingBindings()`; composition has one content ID.

- [x] Write source-derived peak/average traces independently; assert calibration
  maps -30/0/+12 to 0/.665/.99, history tracks a recent minimum/maximum, and peak
  hold is independently configurable. Test history expiry while input is equal,
  clipped extrema, no reading, RX/TX changes and minimum supported face size.
- [x] Build/run `tst_meter_bar_face`; expect wrong old generic-decay/history
  behavior to fail. Reference Thetis MeterManager lines cited in the design;
  trace oracle must be independent of the C++ implementation under test.
- [x] Implement selected composition/dynamics from those references, not the old
  loosely matched 2% decay. Store title/color, history, peak hold, line/tattletale,
  units and sizing properties only when they affect rendering. Preserve current
  main's correct TX binding mappings/floors. Update background/geometry/overlay
  invalidation so dynamic faces actually repaint in both renderers.
- [x] Build/run `tst_meter_bar_face`, `tst_meter_item_scale`,
  `tst_compression_reading`, `tst_multimeter_timing`; expected pass. Capture native
  CPU and GPU frames/short animation at accepted study sizes plus minimum size;
  compare indicators and scale alignment to the accepted study, not just numbers.
- [x] Show the implemented Mic/ALC face and animation at this milestone. Record
  source matrix, any deliberate departures and required attribution/provenance.
  Commit: `feat: render complete Mic and ALC meter faces`.

### Task 5: Finish the existing meter-object palette

**Files:** Recover/repair these `.h`/`.cpp` pairs under `src/gui/meters/presets/`:
`SMeterPresetItem`, `PowerSwrPresetItem`, `AnanMultiMeterItem`, `CrossNeedleItem`,
`MagicEyePresetItem`, `SignalTextPresetItem`, `HistoryGraphPresetItem`,
`VfoDisplayPresetItem`, `ClockPresetItem`, `ContestPresetItem`;
modify BarPresetItem, ItemGroup preset adapters and registry.
Create `tests/tst_meter_composite_presets.cpp`; extend reference matrix.

**Interfaces:** All recovered faces implement Task 3 contracts and Task 2 factory.
Register stable IDs `meter.sMeter`, `meter.powerSwr`, `meter.ananMulti`,
`meter.crossNeedle`, `meter.magicEye`, `meter.signalText`, `meter.historyGraph`,
`meter.vfoDisplay`, `meter.clock`, `meter.contest`, `meter.spacer`.
Register existing bar variants through BarPresetItem for COMP, CFC/CFC gain,
Leveler/gain, AGC/gain, ALC gain/group, EQ, Signal/Avg/Max Bin, ADC/ADC max and
PBSNR; do not derive their scales from Mic/ALC.

- [ ] Add table-driven fixtures per existing palette entry: round-trip customized
  configuration, required bindings, units, TX gates, no-reading behavior and
  preview dimensions. Assert power scale handles QRP through normal PA maxima;
  needle and range/history trajectories use their own independently cited oracle.
- [ ] Build/run `tst_meter_composite_presets`; expect missing/incorrect recovered
  factories or primitive-only scale/reset to fail.
- [ ] Study each family's own Thetis implementation; fill matrix with source
  version/lines, channel list, calibration, cadence/reset and exposed properties.
  Recover selected code with attribution; make each offered setting effective.
  Keep custom/primitive objects editable without forced promotion. Preserve
  VFO/clock/model wiring through current main interfaces and GUI ownership.
- [ ] Build/run new target plus `tst_meter_presets`, `tst_vfo_mode_containers`,
  `tst_smeter_widget_scale`, `tst_smeter_widget_peak_hold`. Expected pass. Capture
  representative family faces in CPU/GPU and show the visual comparison before
  revising any appearance beyond the approved direction.
- [ ] Commit: `feat: restore complete configurable meter objects`.

### Task 6: Mixed hosts and singleton lifecycle on current main

**Files:** Create `ContainerContentHost.{h,cpp}` and
`tests/tst_container_content_host.cpp`; modify registry, ContainerManager,
AppletPanelWidget, AppletVisibilityController and MainWindow restore/wiring.

**Interfaces:** Extend registry with
`attachSingleton(const QString& typeId, QWidget* liveWidget) -> void`,
`singletonView(const QString& typeId) const -> QWidget*` (QPointer-backed).
Produce `ContainerContentHost::reconcile(const ContainerDocument&) -> void`,
`meterSurfaces() const -> QVector<MeterWidget*>`. ContainerManager consumes store
and registry and exposes `reconcileWorkspace(const WorkspaceDocument&) -> void`.
Views are owned by Qt hosts; live singleton construction stays in MainWindow.

- [ ] Test mixed ordering, hidden sibling positions, delayed widget attachment,
  unavailable capability, singleton ownership and source/model pointer identity.
  S-meter keeps the same MeterPoller pointer when moved; applets create no second
  model or subscription. Applet-panel menus find moved objects in every host.
- [ ] Build/run `tst_container_content_host`; expect absent host/registry behavior
  to fail. Extend existing float/dock regressions before replacing their routes.
- [ ] Register catalog descriptors before restore, attach live views as current
  MainWindow construction completes. Use the explicit alias map from Task 2;
  expose only active main applets and separately registered S-meter. Keep legacy
  canvas as one MeterWidget; mixed stacks use QWidget rows and one MeterWidget
  per contiguous meter run with matching source context, keeping face
  boundaries/IDs for arrangement.
- [ ] Reconcile from committed state only. Generalize main's GPU recreation
  workaround to every meter run; serialize through documents, remove old poll
  targets, create/seed/register replacements once. Reparent live applets while
  preserving connections; defer missing hosts/sessions without deleting entries.
  Pass each meter run's context to the poller/source adapter. Route manager save,
  geometry, splitter and visibility mutations through the store; disable the old
  destructive legacy-key writer after successful document adoption.
- [ ] Build/run new target plus `tst_applet_float_dock`, `tst_applet_panel_gutter`,
  `tst_applet_panel_set_visible`, `tst_applet_visibility_controller`,
  `tst_applet_visibility_menu_wiring`, `tst_container_persistence`.
  Expected: no duplicate/live orphan views, correct stable order and gates.
- [ ] Commit: `feat: host applets and meter objects in shared containers`.

### Task 7: Dotted-grip arrangement, return semantics and chrome

**Files:** Create arrange controller, `tests/tst_container_arrangement.cpp`,
`tests/tst_container_chrome.cpp`; modify ContainerWidget, content host,
AppletPanelWidget and ContainerManager lifecycle/menu paths.

**Interfaces:** Produce `ArrangeResult { bool ok; QString error; }` and controller
`move(const QString& entryId, const QString& destinationId, int insertionIndex)`,
`popOut(const QString& entryId)`, `returnEntry(const QString& entryId)`,
`closeContainer(const QString& containerId)`, `removeContainer(const QString&)`
all returning ArrangeResult. Commands consume latest store snapshot, validate,
then commit with its revision; reconciliation follows only Saved.
Also produce `duplicateEntry(const QString& entryId, const QString& destinationId,
int insertionIndex) -> ArrangeResult`; reject singleton entries and regenerate
meter identity while preserving complete config/extensions.

- [ ] Write tests asserting `[A,B,C]` → move A to end → `[B,C,A]` while internal
  paint order stays identical; invalid/canceled drop leaves encoded document
  unchanged. Move across containers updates both once; locked targets reject.
  Duplicate singleton rejected; duplicate meter gets new ID and equal config.
- [ ] Test a pop-out shell with objects from two destinations plus a newly created
  object: close returns each to its own remembered stable neighbors; missing
  destination/new object returns to main; shell removed only after successful
  commit. Normal floating close hides. Container deletion returns singleton
  views without destroying their connections. Fake session records zero keying
  or settings requests for every arranging command.
- [ ] Test Reveal mouse/focus cycles keep content geometry unchanged; Hidden has
  reachable recovery via menu/Shift; lock blocks resize/drag but not recovery.
  Clamp off-screen geometry on monitor removal; honor current minima 260×24 and
  content minima, anchors, auto-height and RX/TX visibility.
- [ ] Build/run both new targets; expect absent drag/chrome/atomic return to fail.
- [ ] Implement top-left dotted grip, Nereus MIME carrying stable entry ID and
  workspace revision, insertion indicator and equivalent menu commands. Reject
  stale/cross-workspace drops before mutation. Record return neighbors whenever
  an entry enters a pop-out shell, including subsequent moves. Preserve hidden
  positions. Route old floating applet actions through this controller.
- [ ] Implement reserved header extent, hover/focus reveal, hidden recovery and
  resize affordance; no essential face/control is covered. Build/run new tests,
  `tst_applet_float_dock`, `tst_container_persistence`, `tst_native_cursors`.
  Show native mixed-container reorder/pop-out/return and hover behavior; record
  native Windows/Linux checks still outstanding if unavailable locally.
- [ ] Commit: `feat: arrange and return container objects from dotted grips`.

### Task 8: Draft transactions and safe prominent preview

**Files:** Create edit session/preview units;
`tests/tst_container_edit_session.cpp`, `tests/tst_container_preview.cpp`;
modify ContainerSettingsDialog transaction/preview plumbing and registry.

**Interfaces:** Produce `ContainerEditSession(ContainerWorkspaceStore&)`,
`draft() const -> const WorkspaceDocument&`,
`setDraft(const WorkspaceDocument&) -> void`,
`apply() -> CommitResult`, `cancel() -> void`,
`conflictingContainers() const -> QStringList`,
`reloadContainers(const QStringList&) -> void`;
`ContainerPreviewWidget::setDocument(const ContainerDocument&) -> void`.
Its constructor consumes `ContainerContentRegistry&` and `MeterPoller&`, plus
`QWidget* parent`; it replays/listens to Task 3's read-only GUI feed.
Registry adds `createPreview(const ContentEntry&, QWidget* parent) const -> QWidget*`.

- [ ] Test rename/change/order/create/delete across two container drafts; switching
  selection retains edits; Cancel and window close leave store/settings/views
  unchanged; successful Apply commits all once. External move/delete/config
  change produces Conflict naming affected containers. Reload resolves only
  affected drafts; unrelated live changes merge without silent overwrite.
- [ ] Test live singleton stays parented to its existing host during preview;
  no factory creates a second singleton, no radio signal fires and no extra
  poll target/Core subscription is added. Meter preview receives read-only copies of
  existing cached readings; missing source shows the existing unavailable state.
- [ ] Build/run both new targets; expect current direct mutation/empty preview
  assumptions to fail.
- [ ] Implement base/draft snapshots and touched-container/reference comparison
  against current revision. Rebase unaffected edits onto current state; return
  Conflict before writing overlapping changes. Apply validates/commits, then
  reconciles; failed save leaves real views and draft intact. Preview uses the
  real meter factory and inert applet presentation adapters, never actual radio
  controls or reparented live widgets. Unknown content gets an explanatory tile.
- [ ] Build/run new tests and `tst_container_workspace_store`,
  `tst_container_content_host`; expected pass. Ensure preview reconstruction
  seeds unchanged readings and only invalidates affected surfaces.
- [ ] Commit: `feat: edit container drafts with safe live previews`.

### Task 9: Usable settings, property editors and portable content

**Files:** Refactor ContainerSettingsDialog; create property editor unit,
`tests/tst_container_settings_workflow.cpp`, `tests/tst_container_exchange.cpp`;
extend codec/importer for `.nscontainer` and clipboard consumers.

**Interfaces:** `ContentPropertyEditor::setEntry(const ContentEntry&) -> void`,
signal `entryEdited(const ContentEntry&)`; produce codec
`exportContainer(const ContainerDocument&) -> QByteArray`,
`importContainer(const QByteArray&) -> DocumentResult` (one-container workspace),
`exportEntries(const QVector<ContentEntry>&) -> QString`,
`importEntries(const QString&) -> DocumentResult` (one-container workspace).
New files/clipboard use schema envelopes; importer continues accepting old forms.

- [ ] Test select/rename/duplicate/remove, add from Available, move in dialog,
  contextual appearance/behavior/advanced binding controls, Apply/Cancel and
  explanatory unavailable rows. Singleton offers Move, never Duplicate. Every
  enabled property changes its intended renderer/behavior and survives reload.
- [ ] Test new/legacy file and Base64 clipboard import/export, opaque extensions,
  full MMIO/config, duplicate identity regeneration and invalid input leaving
  the draft unchanged. Import of a singleton resolves an existing view as a
  move only; never allocates a duplicate or steals it before Apply.
- [ ] Build/run both new targets; expect old separate clone/export/property
  paths to fail the new workflow assertions.
- [ ] Build accepted Available/Contents/Properties structure with prominent preview,
  durable names and contextual actions. Group properties into container,
  appearance/behavior and advanced bindings. Use meaningful composite names;
  disable unsupported properties with reasons. Return/hide/delete labels state
  their outcome. Route all operations through draft, registry and codec.
- [ ] Implement portable envelopes plus old-format readers; retain unknown data,
  validate before replacing draft, regenerate imported meter/container IDs and
  remap return links inside the imported set. Keep any original source bytes in
  the recoverable payload. Build/run new tests, `tst_container_legacy_import`,
  `tst_container_edit_session`; expected pass. Show implemented settings preview
  with a meter/applet mixture and customized legacy content.
- [ ] Commit: `feat: finish container settings and lossless content exchange`.

### Task 10: Integration evidence and recovery readiness

**Files:** Create `tests/tst_container_workspace_integration.cpp`,
`tests/tst_container_meter_native.cpp` (NATIVE_WINDOW where actual QRhi is needed);
finish validation log/reference matrix; update the project's container help.
Fix implementation defects in the owning files, not through test weakening.

**Interfaces:** Consume all prior contracts; add no alternate serializer,
poller, singleton factory or persistence path.

- [ ] Write integration tests for migration → edit → Apply → arrange/pop-out →
  restart → return, retaining names/config/MMIO/unknown entries and ordering.
  Cover remote disconnect/reconnect, late host, stale cache, TX transitions and
  singleton pointer stability; assert no layout action produces radio requests.
- [ ] Build/run integration and native meter targets; expect any remaining
  lifecycle/renderer defects to fail. Repair against the document contracts;
  rerun failed owners and integration after each fix.
- [ ] Configure `build-cpu` separately with GPU OFF; build/run meter-face and
  composite-preset tests there. Capture matching CPU/GPU native frames at minimum,
  study and normal sizes, light/dark surrounding themes and relevant DPI. Check
  rise/release/history/peak hold, all channels, invalidation and reparenting.
- [ ] Run native interactive acceptance: dotted grip only (controls remain usable),
  repeated reorder/move/float/dock, multi-source shell close, deleted destinations,
  hover/focus/Hidden recovery, lock, resize, monitor changes and editor conflicts.
  Measure mixed-host draw/poll load against main using the same document/sample
  trace; record surface/target counts and timing. No duplicate polling or runaway
  invalidation; investigate observed regressions before claiming completion.
- [ ] Build `all_tests` before final ctest. Run non-realtime suite first, realtime
  tests alone on a quiet machine, per fast-test-loop guidance. Run test-registration,
  GUI/DSP-boundary, slice-spectrum-wiring, image-cursor and attribution checks:
  `verify-test-registration.py`, `verify-no-gui-dsp-access.py`,
  `verify-no-captured-slice-spectrum-wiring.py`, `verify-no-image-cursors.py`,
  `verify-thetis-headers.py --all-kinds`, `check-new-ports.py`,
  `verify-inline-cites.py`, `verify-provenance-sync.py`,
  `verify-inline-tag-preservation.py`,
  `compliance-inventory.py --fail-on-unclassified` under `scripts/`.
- [ ] Record actual platform/build/snapshot evidence and outstanding benches;
  Windows GPU/macOS/Linux claims require those runs. Finish help for recovery,
  header modes, draft behavior and return/hide semantics. Show final behavior and
  face captures; obtain whole-branch review under the selected execution workflow.
  Commit: `test: verify container recovery and meter rendering end to end`.

## Plan review and handoff

Coverage: persistence/migration (1–2), composite fidelity (3–5), mixed ownership
(6), arrangement/chrome (7), drafts/preview (8), settings/exchange (9), integrated
runtime evidence (10). All five Review Focus conditions have owning tests.
No product code has been changed or tests claimed passing by writing this plan.

Selected execution: **Crew with cost-aware implementation**, as requested by the
maintainer. Use task-sized workers and one integrated review at the end; this
replaces the older per-task review boilerplate.
The tasks share document, registry and lifecycle interfaces closely; one executor
reduces repeated context/setup while keeping commits and visual milestones.
Execution is authorized. Continue the plan with targeted verification and visual
milestones without requesting approval at each task handoff.
