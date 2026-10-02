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
