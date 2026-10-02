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
