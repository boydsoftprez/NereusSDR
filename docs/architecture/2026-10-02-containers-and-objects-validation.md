# Containers and objects validation

## Task 1 — 2026-10-02

Base: `dd53da5af65127840e2e6e145c7104286ef5453f`, isolated
`codex/containers-and-objects` worktree. Dedicated `build-containers`:
RelWithDebInfo, tests ON, GPU ON, LTO OFF, `/opt/homebrew` dependencies,
ccache. No DSP/protocol changes, hardware operations, or operator settings I/O.
Every test uses the repository sandbox or explicit temporary AppSettings paths.

Reference choice: existing `ContainerWidget.h` owns the saved DockMode/AxisLock
numeric values. They are extracted unchanged into a QWidget-independent GUI
header. AxisLock is the already-ported Thetis `ucMeter.cs:49–59` enum, verified
at `v2.10.3.15`; its verbatim upstream notice and provenance row accompany the
extraction. Document/codec/store logic is NereusSDR-original, built on the
existing `AppSettings::save()` atomic XML boundary rather than introducing a
second persistence engine. Core and models acquire no GUI include.

Wire decisions for downstream tasks:

- Schema version 1. Extension objects hold unknown members at their owning
  JSON object's level, including return locations; they are re-emitted there.
  Known-field extension collisions are rejected. Names are labels and may repeat;
  container IDs and global content IDs must each be unique.
- Geometry/canvas rectangles use `[x,y,width,height]`; mode enums use existing
  numeric values. Ordered content vectors and independent paint order both persist.
- Revisions use decimal JSON strings to retain all 64 bits. Safe nonnegative
  numeric JSON revisions are accepted on read. The store allocates revisions;
  it does not trust draft revisions and cannot wrap around.
- Constructor/load is read-only. Absent documents produce an empty valid snapshot
  (legacy import is Task 2). Malformed/future documents keep their source key and
  the previous snapshot unchanged; commits are blocked until a successful reload.
- First successful replacement keeps the exact structured source bytes in
  `ContainerWorkspaceBackup` if no backup already exists. An importer-seeded
  backup remains untouched. Existing legacy keys remain untouched.
- CAS compares both live revision and the loaded source payload, detecting another
  store's commit or a direct local settings edit even at the same revision.
  Failed persistence restores only the two owned keys and emits no commit signal.

Commands and evidence (logs under `.crew/2026-10-02-containers-and-objects-plan/`):

| Check | Command | Exit | Evidence |
| --- | --- | --- | --- |
| Configure | `cmake -S . -B build-containers -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON -DNEREUS_GPU_SPECTRUM=ON -DNEREUSSDR_ENABLE_LTO=OFF -DCMAKE_PREFIX_PATH=/opt/homebrew` | 0 | `task-1-configure.log` |
| Current-main baseline build | `cmake --build build-containers --target tst_container_persistence tst_settings_scope tst_core_has_no_gui_includes -j 6` | 0 | `task-1-baseline-build.log` |
| Current-main baseline tests | `ctest --test-dir build-containers -R '^tst_(container_persistence\|settings_scope\|core_has_no_gui_includes)$' --output-on-failure` | 0; 3/3 | `task-1-baseline-tests.log` |
| Missing interfaces | `cmake --build build-containers --target tst_container_document tst_container_workspace_store -j 6` | 1 (expected missing headers) | `task-1-red-build.log` |
| Behavioral red | Same new-target build, then `ctest --test-dir build-containers -R '^tst_(container_document\|container_workspace_store)$' --output-on-failure` | build 0, tests 8; assertions failed against stubs | `task-1-red-safe-build.log`, `task-1-red-safe-tests.log` |
| Reviewed build | `cmake --build build-containers --target tst_container_document tst_container_workspace_store tst_settings_scope tst_core_has_no_gui_includes tst_container_persistence -j 6` | 0 | `task-1-reviewed-build.log` |
| Reviewed tests | `ctest --test-dir build-containers -R '^tst_(container_document\|container_workspace_store\|settings_scope\|core_has_no_gui_includes\|container_persistence)$' --output-on-failure` | 0; 5/5 | `task-1-reviewed-tests.log` |

Intermediate evidence is retained: initial stub runs exposed two unsafe fixture
indexes, fixed with assertions before a clean behavioral red; first green build
used an unavailable QScopedValueRollback method, corrected before the passing
build. Neither failure was suppressed. Final tests cover names, context/MMIO,
unknown JSON at every value level, geometry/order, duplicates, malformed/future
schemas, save/rollback with absent/new/existing backups, concurrent unrelated
settings edits, stale drafts/multiple stores, failed reloads, revision exhaustion,
local scope and zero station-backend writes. The committed signal checks a newly
loaded disk document and the published snapshot.

Native rendering and hardware checks do not apply to this value/storage layer.
The legacy persistence regression passes with the extracted enums. Full-suite and
cross-platform integration gates remain with the controller for the combined plan.


## Task 2 — registry and legacy import (2026-10-02)

Implemented one current meter factory in `ContainerContentRegistry`, used by
MeterWidget decoding, the retained ItemGroup adapter, and dialog defaults/clones.
The registry captures prior IDs/names/raw/extension data, binding/MMIO, visibility,
paint order, rect and runtime stack coordinates. `config.legacyRecord` remains
unchanged; edited known serialized fields live in the indexed string map
`config.overrides`. Current feature gates determine availability: Voice and
ClickBox are unbuilt; ContainerFilterDisplay is explicitly built in the current
`UnbuiltFeatureList.cpp`. Retired Discord never has a factory. S-meter has the
new singleton descriptor `applet:s_meter`, with no invented legacy float key.

The importer is read-only and uses deterministic UUID v5 IDs from container ID
and content occurrence. Original settings values are kept in
`extensions.legacySettings`; file bytes in `extensions.legacyPayloadBase64`;
Base64 clipboard input additionally in `extensions.legacyClipboardText`.
Container metadata and complete item payloads remain alongside parsed supported
placement/policy fields. It preserves the actual `MeterDisplay_<id>_Geometry`
keys, `ContainerData_*`, `ContainerItems_*`, ID/count/splitter keys, and explicit
case-sensitive applet visibility/floating aliases. The first restored container
is the main fallback; PANEL records replace that identity, matching current main.

No legacy primitive group is promoted while the new composite representation is
absent. GROUP records remain named unavailable compositions; supported flattened
primitives retain customization and overlapping geometry. Historical
`15de6fbecf:src/gui/meters/presets/BarPresetItem.cpp:700–716` writes compact JSON
with `kind=BarPreset`, flavor, x/y/w/h, bindingId, minValue/maxValue, label,
bar/backdrop colors and redThreshold. Customized JSON records are preserved as
named unavailable objects, with confidently parsed geometry. A schema-bearing
workspace always takes the strict codec path, never this item fallback. This is
format/reference study only; no historical composite implementation was ported.

Validation checks all six standardized numeric base fields before individual
legacy deserializers can silently coerce malformed values to zero: four finite
float geometry fields and two valid integer binding/paint fields. Unknown tails
are preserved without treating them as current numeric fields. Unknown,
malformed, unbuilt and retired siblings survive widget/group serialization;
opaque-only fresh restores survive; malformed-only replacement retains existing
live content. Dialog Apply and legacy ItemGroup transfer retain inert siblings.
Replacement accepts reused owned objects by detaching them before clearing.

Preview/Validation hydration disables WebImage fetching and periodic refresh;
Live remains the default and fetches normally. Non-live items have signals
blocked, and are not wired to radio/session actions. A loopback HTTP regression
asserts URL retention, zero non-live connections and one Live connection. Store
load/constructor does not mutate settings. The first successful commit atomically
stores original legacy backup and the document; failed saves roll back only owned
keys. Legacy changes after load cause Conflict before replacement.

Evidence in `.crew/2026-10-02-containers-and-objects-plan/`:

- `task-2-red.log`: missing importer-interface build failure, exit 1. The wrapper
  initially used zsh's reserved `status` variable; the actual build failure and
  corrected exit marker are retained.
- `task-2-adapter-red.log`: dialog Apply discarded unknown siblings and Base64
  clipboard was misclassified, exit 8.
- `task-2-numeric-red.log`: malformed CLOCK base values were silently accepted,
  exit 8.
- `task-2-group-red.log` and `task-2-transfer-red.log`: unknown group records were
  lost on serialization / installation, exit 8.
- `task-2-opaque-red.log`: opaque-only restore was rejected and reused-pointer
  replacement reproduced SIGSEGV, exit 8. Both now have passing regressions.
- `task-2-historical-red.log`: historical kind-bearing JSON was wrongly treated
  as a workspace, exit 8.
- `task-2-build-final.log`: rebuilt `tst_container_legacy_import`,
  `tst_container_persistence`, `tst_meter_presets`,
  `tst_core_has_no_gui_includes`, `tst_container_workspace_store`, exit 0.
- `task-2-tests-final.log`: ctest matching those five targets passed 5/5, exit 0.
- `task-2-registration.log`: test-registration verifier passed, exit 0.

Fixtures are synthesized non-default records from current serialization and the
historical formats, not operator settings captures. They cover calibrated bars,
custom needles/labels, overlapping rects, absent tails, future tails, malformed
middle siblings, GROUP, compact historical JSON and retired Discord. Runtime MMIO,
stack and durable-name recovery is covered separately because main's legacy
serialize does not persist MMIO or stack metadata.

Native rendering, platform drag/GPU behavior, hardware and full-suite gates remain
with later tasks/integration. This task does not restore composite faces, expose
unavailable rows in the old dialog, or replace MainWindow/ContainerManager legacy
writers; the document host and transactional editor tasks own that integration.


## Task 3 — Composite samples, source replay and shared cadence (2026-10-02)

Implemented in the isolated `codex/containers-and-objects` worktree from Task2
revision `017ddf67e316f4cf2cf3cbc533fa3d1d2726bd79`. The primitive adapters and
registry/raw legacy retention remain; no Core, model, wire or DSP change occurs.

`MeterItem` now exposes binding sets, per-binding pushes, frame advancement,
TX-transition reset and PA-scale hooks. `MeterWidget` caches raw samples to seed
new items before repeated-input shortcuts, delivers equal samples for primitive
smoothing/history, and invalidates only relevant changed items. Complete faces
with static/dynamic pipelines preserve Background and OverlayStatic during
reading/frame updates; Background-only dynamic items retain a fallback. A logical
invalidation counter and GPU cache-flag seam verify this without claiming native
QRhi rendering. Explicit per-item channel availability supports partial faces;
a composite is veiled only if every channel is unavailable. Available primitive
TX -400 readings retain their old semantics; explicit unavailability renders no
reading and new source-clamped peak IDs can recognize absence safely.

The six GUI-only additions preserve all old IDs: MicPeak=113, AlcPeak=114,
CompPeak=115, EqPeak=116, LevelerPeak=117, CfcPeak=118. Their existing model
transforms use -195 dB floors for Mic/ALC peaks and -30 dB for the other four.
They use the shared TxChannel cached/lane API and existing thetisTxReading;
independent injected raw peak/average values verify each mapping. Remote DTOs
have no independent peak fields, so all six remain explicitly unavailable even
at stage version3. Useful average/gain channels remain visible.

MeterPoller caches RX by canonical JSON source context and binding, and shared
TX/HW values within the owning poller/window. The GUI adapter supplies cached
slice/channel data once per context/binding per existing frame, including with
no default RxChannel. Source/setter changes, destroyed sources, active-slice
changes and disconnects invalidate affected replay and publish absence; adapters
are not called when a remote source/snapshot is absent. A scope guard advances
every existing poll exactly once, including all early returns, using QElapsedTimer
or an injected monotonic test source. No new timer/reader/subscription per face.

Reconstruction replays MOX, units, PA scale, per-channel availability and current
same-source raw samples. TX transitions evict all TX IDs100–118 (PA power and
SWR included), preserving independent HW200–202. Until an actual subsequent PA
callback arrives, a new face receives absence rather than a fabricated 0W/1:1.
Primitive bars retain their current TX-off smoothing reset. Each complete face
owns its source-calibrated minima/history reset. First-view replay seeds raw input;
MeterDynamics smoothing still starts at the configured family minimum and follows
the next scheduled update, separately from replay delivery.

MeterDynamics ports MeterManager.cs21323–21458 at Thetis v2.10.3.15 with that
source's verbatim header, inline comments and same-commit provenance. It preserves
per-update rise/release recurrence, bounded smoothed history, ignore-sample policy
and history extrema. Timestamp expiry removes old samples after missed GUI frames;
missed frames do not synthesize readings or elapsed-frame samples. Peak hold is
history maximum, with no fabricated independent decay. Configurable update/history/
ignore values come from later faces; main's 100ms/default settings stay unchanged.

The existing MMIO endpoint cache remains the source. Samples/replay are keyed by
GUID+variable, with a separate identity-bearing `mmioReadingUpdated` GUI signal.
Radio samples and radio availability never overwrite a bound MMIO item. Missing
endpoints, absent/non-numeric variables and non-finite values push absence with an
item-specific reason and clear stale display/history. Replay checks the current
existing endpoint cache to prevent stale reconstruction between shared frames.
The test seam supplies an in-memory MmioEndpoint; no endpoint is registered, worker
or transport opened, or external write performed. No extra preview poll target.

A destruction regression matching MainWindow's raw `QObject::destroyed` callback
reproduced SIGSEGV. LLDB proved QWidget can emit destroyed after derived members
are gone while its QPointer still appears live. MeterWidget now emits
aboutToDestroy before its members die; the poller captures/removes the target
there and subsequent destroyed callbacks use identity only. No bypass of the
existing MainWindow lifecycle is required.

Task4/6/8 handoff: complete faces implement readingBindings/pushBindingValue,
advanceMeter, resetForTxTransition, setPowerScale; inspect per-channel
bindingUnavailableReason or override its setter. Use OverlayDynamic/Geometry for
moving parts with participatesIn/paintForLayer. Task6 should drive poller
setUnitMode/rescalePowerMeters as shared settings (they survive zero live targets),
and install a cached-source GUI adapter via setRxReadingSource/setTargetContext.
Legacy per-widget settings are captured while targets are live. Read-only previews
call replayReadings, listen to readingUpdated+frameAdvanced and
bindingAvailabilityChanged, and use mmioReadingUpdated GUID+variable for MMIO;
none of these registers a target or creates a source subscription.

Evidence: `.crew/2026-10-02-containers-and-objects-plan/task-3-*.log`.
Missing-interface RED (`task-3-red.log`) exit1; initial compile failure from two
incorrect RadioStatus getter names (`task-3-build-initial.log`) exit1, corrected to
existing getters. `task-3-tests-final.log` exit8 reproduces the lifetime SIGSEGV;
`task-3-lifetime-backtrace.log` contains its LLDB stack. `task-3-tests-complete.log`
exit8 records a test-fixture mistake: MmioEndpoint defaults to a null GUID; the
fixture now assigns an explicit UUID. Subsequent final acceptance evidence is
recorded in the Task3 report. Test-registration and diff-whitespace gates run too.

Native CPU/GPU captures, platform interaction, performance benchmark, hardware
and full-suite integration remain owned by later tasks/controller. These tests
verify contracts/dynamics, not whole-face rendering or cross-platform parity.

Final Task3 build: `task-3-build-handoff.log` exit0 rebuilt all13 relevant targets.
That run's PA assertion failed because the fixture had omitted MainWindow's
continuing `setRadioStatus(&model.radioStatus())` source connection
(`task-3-tests-handoff.log`, exit8). The assertion remains50W, with production
source wiring restored. `task-3-build-delivery.log` exit0 rebuilds the affected
test; `task-3-tests-delivery.log` passes13/13, exit0, including both new targets,
eight required existing meter/TX regressions, R1, and the affected Task2 importer/
preset regressions. `task-3-registration-final.log` exit0; `git diff --check` exit0.
Normal signed/DCO commit hooks and signature evidence are in the Task3 report.


## Task 4 — complete Mic/ALC faces (2026-10-02)

Native source-based faces replace the recovered cached-background/generic2% peak
implementation. Independent trace/calibration and actual-renderer pixel checks
cover motion, history expiry, peak hold, channel absence and minimum width.
`config.properties` named JSON overlays preserve original legacyRecord/unknown
nested data separately from primitive field-index overrides. See meter-reference
matrix Task4 appendix for exact API/config and intentional presentation departures.

The first compile failed on a missing parent argument in test factory calls
(task-4-build-initial.log exit1); corrected to the exact registry signature.
Native pixel RED (task-4-native-gpu-corrected.log exit8) exposed double-alpha
composition in the existing QRhi overlay: premultiplied QImage + SrcAlpha. Correct
One source blend makes the independent128-alpha color oracle pass on both native
CPU and Metal. A delivery run (task-4-tests-final.log exit8) also caught float
rounding of preserved historical canvas geometry and a synthetic fixture that
retained a new-face explicit properties overlay when replacing its raw record.
Exact unchanged document geometry is retained; the historical fixture now models
an imported record with no explicit overlay. Assertions were preserved.

CPU/GPU captures: `.crew/task-4-captures/{cpu,gpu}/face-{260,434,640}-NNN.png`
and `mic-alc.gif`.65 frames/100ms, rows72, cocoaDPI72/DPR2. GPUactual Metal API2,
frameSubmitted + grabFramebuffer, no skips. Controller inspected accepted
minimum434/larger/falling/unavailable frames and animation. Scale cache is
face-local and DPR-aware; styles/threshold colors have independent pixel checks.
Detailed final scoped evidence and exit statuses are recorded in the Task4 report.
No transmitter operation, Core/model/DSP/wire edit or Windows/Linux runtime claim.


## Task 5 complete palette (2026-10-02)

Native macOS cocoa verification used build-cpu (GPU OFF) and build-containers
(GPU ON, Metal API2), Qt DPR2, synthetic GUI readings only. Required targets were
built explicitly (EXCLUDE_FROM_ALL). No Core/model/DSP/wire changes or hardware
operations. The10-test focused set comprises meter_composite_presets,
meter_presets, vfo_mode_containers, smeter_widget_scale,
smeter_widget_peak_hold, meter_bar_face, meter_composite_dispatch,
meter_item_no_reading, container_legacy_import and other_button_item.

Evidence in `.crew/2026-10-02-containers-and-objects-plan/`:
- task-5-build-cpu-final.log and task-5-build-gpu-final.log: exit0.
- task-5-tests-cpu-final.log and task-5-tests-gpu-final.log: other9 passed;
  composite failed an added static scale comparison while its RX scale was
  hidden in TX. Transitioning the fixture to RX preserves the assertion.
- task-5-build-{cpu,gpu}-native-final-2.log and
  task-5-tests-{cpu,gpu}-native-final-2.log: exit0; affected composite rerun passes.
- task-5-native-{cpu,gpu}-final.log: saved complete native output,10 QtTest
  passes,0 failures,0 skips; GPU frameSubmitted and grabFramebuffer, API2.

Earlier failed evidence is retained. A real GPU configured-backdrop mismatch
(task-5-tests-gpu-verified.log exit8) led to the consume-once static presentation
hook. The first hook compile failed on GPU-only members in CPU; the corrected
hook reuses existing invalidateReadingLayers(true). Effective configuration,
PA scaling, ANAN display groups/S9 calibration, Bar title and stable-data VFO
setters now alter native pixels; equal state does not continuously dirty static
presentation. Primitive and composite clocks repaint with no radio samples.
Independent numeric/color checks include source-calibrated marker96.75000009216W
and alpha128 red over gray32 yielding144/16/16. Family tests cover customized
round trips, unknown fields, atomic rejection, bindings/units, remote channel
absence, poller replay/TX eviction/disconnect, numeric MMIO-400, QRP1–500W,
source smoothing/history/angular geometry and actual existing control routes.

Captures `.crew/task-5-captures/{cpu,gpu}/<family>-{360,640}-{0,11,customized}.png`
cover Power/SWR, Cross, ANAN, Eye, SignalText, History, VFO, Clock, Contest and EQ.
CPU controller inspection accepted corrected Contest sizing, separated ANAN
labels and human-readable titles. Durable copies are in the task5 subdirectory
of `/Users/j.j.boyd/.codex/visualizations/2026/10/02/01a0fe3c-4f4b-7c10-b84c-16c14521e8dd`.
See matrix for exact editor/descendant invalidation interfaces and source versus
local composition boundaries. Full historical whole-face schema promotion,
Task6 live descendant wiring, ADC magnitude provider, remote independent peaks,
Windows/Linux/hardware verification remain outside this scoped completion.


Final visual inspection exposed a separate background DPR allocation defect:
logical-resolution background textures blurred static labels/arcs at DPR2 while
dynamic overlay remained sharp. Background image/texture now use physical pixel
size and image DPR, recreated on size or DPR changes with minimum64 pixels;
logical drawing coordinates are unchanged. Native title pixels compare against
an independent DPR-painted reference (channel tolerance2, under1% mismatches).
`task-5-build-{cpu,gpu}-dpr-final.log` and
`task-5-tests-{cpu,gpu}-dpr-final.log`: exit0, directly affected composite+bar2/2.
Full raw native output is saved as `task-5-native-{cpu,gpu}-dpr-final.log`.
Earlier other8 focused targets remain passing; no broader rerun was needed.

## Task 6 — 2026-10-02

Integration base `b37b57d2abdf995ab2f0a3b67f10992b6270eee3`: the original
five container commits plus the controller's accepted, signed fourteen-component
CoreSettings carry. Mutable Settings work in the primary checkout was excluded.
Only the assigned worktree was edited; no Core/model/DSP/protocol implementation,
operator profile, radio connection, hardware command or dependency installation
belongs to this task.

Reference choice: study current MainWindow construction, Qt ownership, existing
ContainerButtonDispatcher policy and SliceModel calibrated caches. Implement a
NereusSDR-original committed projection rather than translate a second upstream
container or polling engine. Existing upstream notices and inline tags remain;
changed files record J.J. Boyd / KG4VCF and OpenAI Codex modification history.
The new host/registry/source glue is explicitly original. No new upstream
provenance derivation is introduced. Current SpectrumWidget native-island policy
and [Qt 6.11 QWidget source](https://github.com/qt/qtbase/blob/v6.11.0/src/widgets/kernel/qwidget.cpp)
were studied to diagnose native sibling promotion; public Qt attributes and
construction order are used, with no copied private Qt implementation.

A VerticalStack partitions contiguous meter entries by effective source identity.
Face height and maximum required minimum width determine each run; hidden and
unavailable records retain ordered IDs. A legacy canvas remains one surface and
retains exact canvas rectangles. If entries request different sources in that
canvas, mismatched entries explain their unavailability, accept no wrong-source
readings or control actions, and retain original data for later arrangement.
No runtime row projection or live readout is written by geometry persistence.

The registry is QApplication-free for metadata/import use. It borrows each actual
Main-constructed singleton, parks it only when necessary, and preserves its
construction owner. S-meter attachment detaches the old header before wrapper
teardown; AppletPanelWidget lookup and MeterPoller's pointer survive moves.
Qt connections and dispatcher ownership/listening/held/desktop-hosting gates are
retained. First document placement claims a singleton deterministically; duplicate
records remain inert and ordinary visibility toggles change only the claimed
record. Late attach, destroyed-view replacement, preference/capability changes,
and metadata-only updates preserve unaffected meter pointers and populated
history. Changed/native-reparented hosts synchronously unregister/delete every old
meter before creating replacements in the final host; targets are seeded once.

Main starts the existing shared MeterPoller cadence offline and keeps it running
across disconnects; no additional face timer or DSP subscription is created.
Clock frames advance while radio readings remain unavailable. Source contexts
resolve stable sliceById IDs, legacy rxSource-1 or inherited windowRxSlice.
Explicit absent/foreign sources never fall back. A nonempty sessionId matches the
current local radio MAC or remote station identity fingerprint (hex); unknown
identities remain deferred. The adapter uses only calibrated SliceModel caches,
handshake/version gates and the existing per-pan MaxBin resolver; PBSNR remains
truthfully unavailable where no sanctioned producer exists. All runs and composite
children participate in Main readout/control/mini refresh and GPU invalidation.

Production adoption is one committed document operation. Old floating applet
flags/geometry become document-owned shells with return metadata; later startups
respect moved/hidden entries and leave legacy keys untouched. Structured visibility
saves before live mutations. A real atomic-save StorageError, separately from
future-schema rejection, leaves controller/menu/widget/document/raw settings and
last saved bytes unchanged. Future/malformed workspaces stay read-only without a
default overwrite. A small transitional dialog Apply adapter commits its edited
run and container fields while retaining other mixed rows; full draft/exchange and
settings UI remain Tasks 8–9.

Native diagnosis preserved failing logs: initial sibling meters were RasterSurface
while the first was MetalSurface, causing Qt Metal beginOffscreenFrame failure.
Configure each meter before attaching it to the final parent and prevent ancestor
promotion; every native leaf then has MetalSurface and renders. A subsequent
shutdown failure came from returning parked native applets during QWidget child
teardown. Main now stops cadence, clears only its own model adapter pointers and
retires manager/registry while the window and construction owners are still alive.
The surviving-model regression verifies no dangling MeterPoller/ContainerManager.

GPU focused build and thirteen test owners pass, exit 0, in
`.crew/2026-10-02-containers-and-objects-plan/task-6-build-gpu-verified.log`,
`task-6-build-editor-preservation.log` and `task-6-tests-gpu-verified.log`.
The final added borrowed stack-widget lifetime test also passes in
`task-6-build-stack-lifetime.log` / `task-6-tests-stack-lifetime.log`.
Native Cocoa/Metal frameSubmitted plus grabFramebuffer captures are in
`task-6-captures/gpu/{offline-clock,slice-b-contest,move-0-clock,move-1-clock,move-2-clock}.png`.
Contest capture inspection confirms Slice B frequency/mode/band, functioning
buttons and shared local/UTC clock. Functional evidence covers source B actions,
missing/unsupported refusal, all four targets through float/panel/overlay, no
layout-induced frequency/mode change, shutdown, floating adoption/reopen and
read-only future store. Offscreen GUI tests make no GPU-rendering claim.

Final CPU eleven-owner checks pass, exit 0, in `task-6-build-cpu-final.log`
and `task-6-tests-cpu-final.log`. Final GPU five-owner affected checks pass,
exit 0, in `task-6-build-gpu-final.log` and `task-6-tests-gpu-final.log`; the
other previously passing owners retain the thirteen-owner evidence above.
The final repairs dispose failed hydration before emitting an empty run and
adopt legacy floating-container top-level geometry once, after which structured
forms never consult the legacy geometry key. The malformed known-entry test
first failed with two surfaces instead of one (`task-6-tests-malformed-before.log`),
then passed with raw entry preservation and the correct following source run.
Latest GPU Contest and CPU offline-clock captures were visually inspected.
Normal-hook signed commit evidence is recorded in the Task 6 report. Windows GPU, Linux native drag and attached
hardware remain unobserved. Controller reports inherited carry omissions in the
empty-primary-pan-key MaxBin resolver and remote/TX frame-stamped DSS path;
mutable corrections have not been imported. Adapter routing is tested, and a
missing source result remains unavailable; final carry acceptance and broad gates
belong to Task 10 after the controller integrates a signed correction packet.


## Task 7 — arrangement and reserved chrome (2026-10-02)

Implemented from BASE `6030de86d39dae1cef81aae4e35df8cbc4de769f` on
`codex/containers-and-objects`, by gpt-6.1-sol / high, with sole product/build/Git
ownership and no additional agents. Token usage and original task elapsed time
are unknown. No attached hardware, radio discovery/connection, private face timer,
DSP/model/Core behavior, operator profile, push or deployment was used.

The controller changes a complete working document, validates it, then uses one
revision CAS; only committed snapshots reconcile. Per-live-controller identity,
revision and stable entry ID reject stale/foreign MIME even with shared settings.
IgnoreAction/Escape is cancellation; explicit menu Pop Out creates shells. Stable
neighbor returns preserve live survivor order and linked historical groups; stale,
self/missing/cyclic anchors recover deterministically with diagnostics. Complete
ReturnLocation and opaque config/extensions survive. Metadata-free new shell rows
return in their current shell order. Normal floating close hides even when locked;
locked structural returns/moves/resize are rejected. Singleton views remain borrowed,
while supported primitive and complete-face duplicates get new entry identity.

Ordinary Move retains explicit entry context and lets inherited source context use
the destination defaults. New pop-out shells copy source default axes
(sessionId/sliceId/rxSource), preserving an inherited Slice B source without
rewriting the entry's explicit/inherited choice. Shell hops retain normal-home
metadata. The legacy one-time applet migration now records predecessor in beforeId
and successor in afterId.

Grips occupy a reserved left gutter beside native meter leaves. Host entry order
and canvas paint order remain separate; legacy grip boundaries use actual item
rectangles and legacy paint lists are ordered by retained paintOrder/stable identity.
Reveal and Hidden reserve header extent; hover/focus/Shift/menu recovery never
moves content, and same-mode commits preserve explicit Hidden recovery. Old
noControls migrates to canonical Hidden, retaining original raw/config; subsequent
Always/Reveal edits govern recovery. Shell toolbar Return closes through the atomic
controller; Hide retains placement. AppletPanel managed float/dock routes use the
same controller. Final geometry persists after gestures; screen clamp, content
minima, auto-height, overlay anchors and read-only RX/TX visibility are covered.
The transitional settings Apply keeps Reveal when its legacy hide checkbox is
unchanged; the full editor remains Task 8.

Reference decision: studied Aether AppletPanel.cpp at approved 5766bb13 (drag
threshold, scroll insertion line, explicit drag-only title area). The new mixed
host/transaction/gutter implementation is original Nereus code, with stricter
workspace identity/revision and singleton/native-surface ownership. No new upstream
code was copied; existing notices/inline tags and provenance remain intact.

Dedicated graphs retain Ninja, RelWithDebInfo, Qt /opt/homebrew, ccache, both LTO
switches OFF; GPU ON in build-containers, OFF in build-cpu. Actual CPU capacity is
18 logical CPUs; this task used -j4 and serialized each graph/native run. Explicit
named targets were built before every ctest run. Required and affected targets:
`tst_container_arrangement`, `tst_container_chrome`, `tst_applet_float_dock`,
`tst_container_persistence`, `tst_native_cursors`, `tst_container_content_host`,
`tst_container_content_host_native`, `tst_mini_display_composition`,
`tst_container_legacy_import`. Final accepted build/test logs and actual exit
statuses are in the ignored crew report directory, `task-7-*-accepted.log`.

Meaningful coverage includes 17 return-order/failure boundaries, same-number/shared
store foreign drags, stale/invalid positions, locked structural commands, complete
primitive/face duplication, shell hops, deleted sources, multiple destinations,
opaque/hidden entries, metadata-free shell order, failed saved bytes/singleton
parents, zero fake station settings requests, managed applet float/dock, native
explicit control connections, exact live poll target counts, synchronous native
surface retirement, actual Metal frameSubmitted/readback, reserved geometry,
Hidden same-mode recovery, lock resize events, auto-height, screen/pane minima,
anchors and no-save RX/TX visibility. Task 6 native Main shutdown and surviving-model
regressions and mini descendants remain meaningful and pass in the affected run.

Native images under `.crew/2026-10-02-containers-and-objects-plan/task-7-captures/gpu`
show 01 mixed grips (full window plus direct container grab), 02 insertion line,
03 reordered (full/direct), 04 popped applet, 05 compact mixed pop-out, 06 returned,
07 Reveal focus, 08 Hidden, 09 recovered Hidden and 10 custom legacy canvas.
Matching CPU images are separate. Native QWidget captures plus Metal readback
were visually inspected; the gutter does not cover scales or explicit controls.
These are implemented native artifacts, not mockups.

Initial `task-7-build-before.log` failed on the missing controller interface;
subsequent compile failures and native focus/auto-height stages are retained in
`task-7-build-controller.log`, `task-7-build-chrome.log`, `task-7-tests-chrome.log`
and `task-7-tests-focus.log`. They were repaired without weakening valid assertions.
Focus-state tests establish Qt's active-window state explicitly because native
activation is unavailable on this locked desktop. `cua.getState` reports the Mac
locked and automatic unlock unavailable. Delivered native Qt drag/drop/menu events
are verified; physical QDrag pointer/Escape, Windows GPU/native interactions,
Linux native interactions and physical monitor-removal benches remain pending.
No cross-platform/hardware or full-suite success is claimed. Final broad gates and
the controller's pending signed Settings correction packet remain Task 10 work.

Final focused results: GPU 9/9, exit 0, 14.35 seconds; CPU 9/9, exit 0,
8.79 seconds. The subsequent GPU arrangement-only data-driven saved-write,
external-revision and invalid-external-document failure boundaries also pass
(1/1, exit 0, 0.56 seconds); product source is unchanged from the GPU nine-target
run. Both accepted chrome binaries include the retained custom caption fixture.
R1 GUI access and test-registration checks pass with exit 0. Broad gates remain
Task 10.


## Task 8 — draft transactions and inert native preview

Implemented against signed freshness merge `7acd357f4f749dfaae948b5abad5b39183d58b86`
(Task 7 `de317e7370a062d077037831e8d710c23c15883b` plus accepted Core
Settings/logger `5d425542048fb4c2348553c99a3ec79ab4929829`). No mutable Core
source was copied; the accepted actual-slice pan resolver and DSS fold are retained.

ContainerEditSession reloads the latest Store before conflict comparison and final
raw/revision CAS. Stable entry identity catches competing moves, deletion,
configuration changes and return-container references. Unrelated live containers,
workspace extensions and main identity merge without overwrite. Partial Reload
adopts only selected containers and linked entry placements, retaining unrelated
names/configuration. Workspace extension conflicts belong to the main container;
reloading it resolves that conflict without losing another container's rename.
Save failure/invalid future bytes retain saved state, real views and draft.

The production settings dialog edits independent draft snapshots across selection,
property changes, reorder/add/delete/duplicate and legacy load. Apply publishes one
complete Saved Store transaction; the existing Manager committed connection performs
one reconciliation. Cancel/window close perform no saved/live ownership mutation.
Unchanged hydration and edit-undo preserve raw opaque fields and original entry
positions. Task 9 retains final Contents/property editor and portable exchange work;
the intermediate composite row still uses the old serialized label.

Prominent native ContainerPreviewWidget uses real Preview meter factories and
explicit inert existing-applet presentation tiles/snapshots. It never invokes a live
singleton factory, reparents a singleton, registers another poll target, creates a
model/subscription/endpoint transport, or starts a timer. Parentless Metal leaves
attach before painting. Valid unpolled slices seed directly from sanctioned GUI
caches. Continuing shared frameAdvanced samples preserve dynamics/history; changes
reconstruct only affected native leaves. GUID+variable MMIO stays independent.
Explicit foreign sessions refuse window-global TX/PA/hardware at initial replay,
live push and preview sampling; matching/inherited sessions retain global readings
with missing RX slices. Session changes cannot relabel old cache before the next
tick or first new producer callback. Offline clocks remain independent.

Final post-transition focused checks: GPU 11/11, exit 0, 23.87 seconds; CPU 11/11,
exit 0, 24.95 seconds. Each raw Qt record contains 125 passes, zero failures/skips.
Named EXCLUDE_FROM_ALL targets were rebuilt first in both graphs, exit 0; Ninja,
RelWithDebInfo, Qt /opt/homebrew, ccache, both LTO flags OFF, GPU ON/OFF respectively.
Final logs are `.crew/2026-10-02-containers-and-objects-plan/task-8-{build,tests}-{gpu,cpu}-final-session.log`,
with raw `task-8-qt-{gpu,cpu}-final-session.log`. Included targets: edit_session,
preview, workspace_store, content_host, content_host_native, composite_dispatch,
pan_actions_per_pan, persistence, mini_display_composition, meter_poller_tx_bindings
and multimeter_unit_conversion. R1 GUI/DSP access and test-registration checks pass.

Meaningful red regressions are retained: foreign-session live cached power was
50 W instead of no-reading (`task-8-tests-session-red.log`, exit 1); switching
session before a first tick replayed the same old 50 W
(`task-8-tests-session-transition-red.log`, exit 1). Both repaired boundaries pass,
including matching/inherited live+preview TX/PA/volts, fresh 80 W/24 V after identity
change and independent foreign-session clock/MMIO. Earlier fixture/source build
failures are retained with their repairs in the Task 8 report, without assertion
weakening. The initial successful build wrapper used zsh's readonly `status`
variable; subsequent wrappers record explicit EXIT_STATUS correctly.

Actual native screenshots are in `task-8-captures/{gpu,cpu}/`: main-slice-b-preview,
meter-preview, mixed-preview, settings-preview and existing host fixture captures.
GPU frameSubmitted/Metal framebuffer readback and CPU QWidget captures pass.
Visual inspection shows the prominent 7.074100 MHz / LSB / 2.9k / 40m Slice B
preview with clock and inert selected band/mode controls. This is actual product
rendering against an isolated fake model/profile, with no radio command/connection.
Physical pointer interaction is still unobserved on the locked desktop. Windows,
Linux native/GPU and physical monitor-removal evidence and broad final gates remain
Task 10. No cross-platform or full-suite success is claimed.

Reference study: immutable Aether `5766bb13:src/gui/AppletPanel.cpp` lines 340–398
uses a live single-meter editor/settings snapshot in its container. This change
uses original Nereus whole-workspace drafts and read-only preview projection with
stricter singleton/native ownership; no upstream editor code was copied. Existing
upstream notices/inline tags remain, with Nereus modification histories and
THETIS-PROVENANCE updated.

## Task 9 — effective settings and portable content (2026-10-02)

Available/Contents/Properties now operate on complete document entry identities,
with readable registry names, explicit unavailable reasons, singleton Move-only
choices and a prominent native preview. Contextual controls use actual typed
renderer fields, calibrated scale locks, validated heights/history/timing and
color pickers. Primitive adapters retain legacy originals and known overlays.
Renderer edit-undo restores sparse/raw config independently of name/source edits;
advanced MMIO remains intact through subsequent appearance edits and reload.
Global signal units are explained; unsupported applet/clock sources and stacked
geometry are disabled with their actual controller/layout reasons.

All operations remain draft-only until Apply. Pending status follows full draft
comparison, including edit-undo after Apply. Header Always/Reveal/Hidden, layout,
placement, anchor, lock, background and RX/TX visibility use the typed document and
existing Manager behavior. Container removal uses Task 7's shared pure return merge:
remembered destinations/neighbors, fallback warnings and locked atomic refusal.
Singleton import keeps the existing identity/config/context, stages a move from its
old home, remaps imported neighbor references and retains imported metadata.

Portable `nereus.container` / `nereus.entries` schemaVersion 1 envelopes share the
existing document codec. Legacy raw/file/Base64 readers remain; malformed/future
structured payloads fail without fallback or draft mutation. Imported IDs and
internal return links regenerate together. Tagged collision-safe recovery slots
retain the exact first source bytes and unknown envelope/workspace extensions on
each entry, surviving selection/append/Apply/restart. Repeated generated exchange
cycles preserve that immutable original rather than nesting entire prior workspaces;
new unknown envelope metadata is deduplicated. Foreign same-name extensions survive.

Final source-matched GPU/CPU builds and 16-target matrices are recorded under
`.crew/2026-10-02-containers-and-objects-plan/task-9-*`. Builds use Ninja,
RelWithDebInfo, Qt /opt/homebrew, ccache, GPU ON/OFF and both LTO flags OFF.
GPU passed 16/16 in 15.80 s; CPU passed 16/16 in 20.38 s, both exit 0.
Each raw Qt record has 192 passes, zero failures/skips. R1 and registration exit 0.
The final report gives exact log provenance; earlier intermediate
and failure logs remain independently named. Registration and normal attribution
hooks run before the signed checkpoint. The new duplicate-error fixture originally
looked up a nonexistent status QLabel; retained LLDB evidence locates the null
QLabel::text() call, and the repaired assertion inspects actual diagnostic labels.

`task-9-captures/{gpu,cpu}` contains settings-properties, settings-mixed-native,
settings-legacy-properties and settings-unavailable. The real Cocoa fixture has a
cached -4 dB microphone face/average marker, inert snapshot of the existing RX
applet, customized cyan legacy bar and retained retired record. GPU Metal frame
submission/framebuffer readback and CPU QWidget rendering are asserted. No operator
profile, connected hardware, duplicate singleton, extra poll target or action is used.
Physical pointer/QDragEscape/monitor tests remain pending locked-Mac access;
Windows/Linux and broad all_tests/RT/end-to-end benches belong to Task 10.

## Task 10 — integration and final recovery evidence (2026-10-03)

`tests/tst_container_workspace_integration.cpp` follows actual legacy settings
migration into a two-container draft, Apply, move/Pop Out, AppSettings disk reload
and shell Return. Names, MMIO GUID/variable/context, opaque raw entries, original
legacy backup, stable order and late singleton identity survive. Its station
backend spy specifically proves zero station-setting writes. An unavailable-control
case retains exact Voice/Discord records in the committed workspace through Apply,
float/dock and reload, with no live host items. A cached-source case uses
a cached Remote RadioModel/SliceModel, actual ContainerSourceAdapter, MeterPoller
and TransmitState: disconnect/rebuild/reconnect, stale-cache refusal, explicit
foreign-session TX refusal, matching-session recovery and TX-off reset. No
transport, RF, local DSP or operator profile is started.

`tests/tst_container_meter_native.cpp` uses parentless native meter construction
before host attachment and destroys surfaces before owners. Matching CPU/Metal
frames cover ten families, dark/light surrounds and width260/434/640 with declared
family minima respected (composite minima are360 where required). Existing
Task4/5 numeric dynamics/channel and DPR reference oracles remain in the final
combined graph. New traces exercise native dynamic redraw, static configuration
invalidation and leaf reconstruction. Actual Retina DPR2 is observed; an additional
DPR1 fixture covers four selected rows in both products, without claiming a physical
monitor move. Final captures are in task-10-captures/{cpu,gpu}-delivery and
{cpu,gpu}-dpr1-delivery (60 DPR2 rows and four DPR1 rows per product).

The real TxApplet fixture counts sanctioned MOX/Tune/two-tone handlers and watches
TransmitModel power/mic/filter/key-state signals after setup. Repeated move,
Pop Out/Return and floating/docking preserve its exact widget pointer and emit no
requests or parameter changes. The actual visible grip must have nonempty geometry
and remain outside the TX controls. The native TX/microphone capture includes the
real applet, not a control mock. Qt fixture events do not prove physical pointer
QDrag/Escape behavior on the locked desktop.

Native QA exposed an actual ANAN selector defect: paintNeedles painted the group
label in both static and dynamic layers with different fonts, causing overlapping
Power/SWR text. A transparent dynamic-layer image regression first failed; the
selector now paints once in the static background. Current corrected native
frames and layer test replace early images. Upstream geometry/calibration and
comments are preserved, with modification history/provenance updated.

The first complete 1099-test ordinary run exposed five failing targets. Product
repairs restore the closed-slice letter, narrow frequency-only refresh to preserve
unrelated mode/filter button state, and use plain operator wording. Persistence
fixtures now commit entries through the workspace API and check inert raw retention
instead of bypassing ownership or deleting retired records. The remote MON fixture
keeps all availability, state and on-air checks using real committed host items;
its old bare widget could become stale during geometry reconciliation.

### Reference and recovery decisions

| Reference | Final decision | Evidence |
| --- | --- | --- |
| Main dd53 primitive meter/poller APIs | Unmodified common benchmark fixture against actual product libraries | Paired metadata/result artifacts; never emulate main with current binary |
| Accepted Thetis v2.10.3.15 families, Task4/5 source audits | Keep calibrated face geometry/channels; fix only duplicate Nereus selector layer | Native layer regression and matching current CPU/Metal frames |
| Accepted Aether 5766bb13 grip/placement study | Explicit separate grip gutter and atomic document arrangement | Actual TX grip bounds, existing chrome/drop/controller native tests |
| Old15de/April50c recovery material | Keep original raw payload/unknown metadata; no wholesale source transplant | Full migration→Apply→restart→Return test plus exchange/legacy gates |
| Core5d accepted Settings/logger/pan carries | Retain actual slice-host MaxBin and session gating; no mutable peer copy | Final combined pan/source/session tests |

Focused help is `resources/help/containers.md`, linked from getting-started. It
documents actual arrange/menu actions, Apply/Cancel/conflict Reload, singleton
Move, Return/Hide/remove, Hidden recovery/lock, source availability and portable
exchange/recovery. The unfinished global Help action is not enabled.

Final combined build/tests, paired timings, raw exits/counts and source/binary
identities are recorded in `.crew/2026-10-02-containers-and-objects-plan/task-10-report.md`.
Physical pointer/QDragEscape, monitor removal and Windows/Linux native/GPU benches
remain pending. Root owns whole-branch independent review after the signed
Task10 checkpoint; this appendix does not label that review passed.

### Integrated-review correction wave

The frozen integrated review identified three bounded GUI defects. HistoryGraph
now measures retained-sample cadence independently of faster shared frames; its
rendered-line regression covers 250 ms samples on 100 ms ticks, exact expiry,
absence, capacity and TX/configuration resets. A presentation clock rollback
drops future trace points without changing channel dynamics. Matching native
HistoryGraph captures use that slower cadence and a 1.2 s visible window.

Settings-created object and whole-container copies clear active remembered homes
and neighbor anchors, matching live duplicate semantics. Original home/config,
unknown fields and MMIO bindings remain in immutable portable recovery. Actual
Settings Duplicate → Apply → shell-close checks send the original between its
previous neighbors and the copy to main, with stable real singleton identity and
no frequency actions. Singleton-containing container duplication remains refused;
portable singleton imports retain their separate Move semantics.

Minimizes and Hide when RX unused remain unavailable and are disabled with plain
explanations. Structured and legacy Apply skip these controls, retaining boolean
and nonstandard raw values through selection, save/reload and portable recovery.
No minimize/RX-unused lifecycle implementation was added. Red/green receipts and
exact corrected products are tracked in correction-report.md; final full ordinary,
paired/new-only timing, realtime gates and scoped re-review remain separate gates.

Controller freshness check: official AetherSDR main was fetched and checked at
`732b99a90e9f6ad8154872bc1ab11471d04d2569`, sixteen commits after the studied
`5766bb13` pin. The selected AppletPanel/FramelessResizer grip, placement and
resizer patterns and TxApplet remain unchanged; the catalog delta adds CTR2Proxy
only. The studied immutable attribution pin remains authoritative; no CTR2 scope
or new port was adopted. NereusSDR published main remains exact `dd53da5a`.
