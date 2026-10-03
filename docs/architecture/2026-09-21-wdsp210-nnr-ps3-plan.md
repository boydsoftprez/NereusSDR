# WDSP 2.10, NNR, and PureSignal 3 Implementation Plan

> Execution: yonder-cost-aware-execution. Requirements and acceptance criteria
> are binding; test order and review effort follow the Yonder risk-based policy.
> Native Qt evidence applies here; Yonder web-page tooling does not.

**Status:** Implementation is complete at the macOS software boundary: desktop,
Core/daemon and 716 test targets build, and the final integrated suite passed
716/716. Native sanitizer evidence and signed local integration are recorded in
[the execution ledger](wdsp210-verification/README.md). Remaining platform,
full-application/operator and RF acceptance stays explicitly unchecked below.
No RF operation has been performed by this task.

**Goal:** Upgrade the shared desktop/Core DSP to WDSP 2.10, deliver complete NNR
and PS3 controls with durable settings, and prepare remote PS3 for R4 activation.

**Architecture:** The radio-owning process owns DSP configuration, execution,
assets, and accepted state. The GUI uses the existing model/session boundary
and owns presentation preferences. Extend ordinary mirrored properties for
configuration; use explicit commands for actions and bounded value snapshots
for diagnostics and AmpView.

**Stack:** C++20, Qt 6, C/WDSP, FFTW, AppSettings XML, existing authenticated
station transport, CMake/CTest and QtTest. No new UI or persistence framework.

**Spec:** [Approved design](2026-09-21-wdsp210-nnr-ps3-design.md), including
R-PERSIST-01 through R-PERSIST-04 and section 9.1.

## Global constraints

- Pin TAPR `wdsp 2.10/Source` to
  `b02d5bac675dd2f33ec2bab2b339f79a597c47dd`; keep required compatibility
  changes explicit in provenance. Desktop and `nereusd` share that baseline.
- Target the active `codex/integrate-r2-main` combined build. This planning
  checkout starts at `aed2278fb7033c4b5f6e36207bc6764f6bd1bad9`; the integration
  branch advanced to `d6ce05a5` during planning. Reconcile its latest accepted
  state at execution start; never overwrite another task's uncommitted work.
- Preserve all existing NR enum values, NR implementations, Qg/Qe CFC profile
  behavior, board/feedback routing, attribution, and Windows/macOS/Linux ARM64
  support. New unrelated EQ/FM/phase-rotator editors are outside this plan.
- Persist accepted settings using AppSettings and the existing radio/slice
  identity hierarchy. Core owns remote DSP settings; GUI owns view preferences.
  No QSettings, desktop-path assumptions on Core, or save-on-dialog-close-only
  implementation. Disabled features retain their tuning.
- Persist preferences, never one-shot actions or live status. NNR diagnostic
  overrides reset to Network/Q-zero for a fresh station session. Remote PS3
  actuation stays refused while `txPermitted=false`; off/reset remains a stop.
- Preserve local automatic-calibration intent through the existing readiness-
  gated coordinator path. Separate that intent from remote session arming,
  single-calibration requests, correction restore, tone generation, and PTT.
- Follow CLAUDE.md, CONTRIBUTING.md, attribution/sync requirements, and
  `docs/development/fast-test-loop.md`. Preserve notices and inline author tags;
  new presentation is attributed honestly. Use GPG-signed commits and hooks.
- Keep DSP code and internal buffers out of GUI/session code. No blocking
  allocations, file parsing, or GUI work in the sample-processing callbacks.
- Hardware and operator acceptance remain pending until observed. Software
  success cannot stand in for Rock 5C capacity or measured PS3 RF improvement.

## Review focus

1. Settings load before channel creation, plus channel rebuild after an edit:
   Task 3 checks actual applied NNR values, not only XML/model values.
2. A stale/reconnecting GUI overwrites newer Core settings or arms PS3:
   Tasks 4 and 6 cover authority, saved intent, refusal, and replay.
3. An unavailable model or corrupt asset crashes an advanced setter, or a
   rejected edit becomes persistent: Tasks 3, 5, and 6 cover those boundaries.
4. PS3 ABI/count/phase changes look plausible but corrupt memory or plots:
   Tasks 2, 4, and 8 verify signatures, allocation bounds, and known snapshots.
5. Off/reset while calibration processing is suspended fails to stop correction,
   or disconnect leaves workers alive: Task 4 covers state/teardown ordering.

## Shared NNR interface and persistence contract

Append `NrSlot::NNR = 8`; do not insert before any existing value. Add
`src/core/dsp/NnrSettings.h` for a value type shared by the model-to-DSP boundary.
Its fields and SliceModel properties use this exact mapping:

| Value field | SliceModel property | AppSettings leaf | Domain/default |
| --- | --- | --- | --- |
| `modelSlot` (`int`) | `nnrModelSlot` | `NnrModelSlot` | 0 Standard / 1 Premium; 0 |
| `maskFloorDb` (`double`) | `nnrMaskFloorDb` | `NnrMaskFloorDb` | operator range -50..-10 dB; -25 |
| `position` (`NrPosition`) | `nnrPosition` | `NnrPosition` | pre/post AGC; post |
| `alpha` (`double`) | `nnrAlpha` | `NnrAlpha` | 0..4; 1 |
| `alphaKneeDb` (`double`) | `nnrAlphaKneeDb` | `NnrAlphaKneeDb` | 0..40 dB; 10 |
| `tauSeconds` (`double`) | `nnrTauSeconds` | `NnrTauSeconds` | 0.05..30 seconds; 2 |
| `maxGainDb` (`double`) | `nnrMaxGainDb` | `NnrMaxGainDb` | 0..24 dB; 12 |
| `attackMs` (`double`) | `nnrAttackMs` | `NnrAttackMs` | 0..500 ms; 0 |
| `releaseMs` (`double`) | `nnrReleaseMs` | `NnrReleaseMs` | 0..500 ms; 0 |

Normal properties follow existing getter/setter/NOTIFY conventions. Persist the
listed leaves plus `NrActive` at
`hardware/<normalized-mac>/slices/<stable-slice-id>/nnr/`. Existing
`Slice<N>/...` settings are station-scoped and cannot by themselves isolate two
radios in one profile. Reuse AppSettings' hardware namespace and stable slice
IDs; do not migrate unrelated slice settings or use tab/vector positions.
NR selection changes, including switching away from NNR, update `NrActive` in
this namespace so restart cannot resurrect an older NNR choice.

Set the slice's settings radio identity before restore/save. Seed the new
namespace once from legacy NR selection only when it is absent, the saved
`radios/lastConnected` MAC matches, and `NnrMigrationComplete` for that radio is
absent. Preserve all existing enum values and legacy keys, and mark migration
complete. Do not copy a first radio's legacy values to another radio; retain
unattributable old data for recovery and make any fallback visible. NNR TestMode
and output-layout overrides are session-only and absent from normal save/load.

The DSP boundary exposes `RxChannel::setNnrTuning(const NnrSettings&)`, a
`NnrSettings` accepted-value readback, and a bounded `NnrDiagnostics` snapshot
(actual model, availability/source, run/bypass, rate, latency when supported,
diagnostic modes, and optional profiling availability). Return model refusal
and partially loaded state explicitly. GUI code never reads internal NNR
objects. The model records accepted configuration only; offline desired values
remain distinguishable from unavailable runtime state.

PureSignalSettings stores normal configuration under
`hardware/<normalized-mac>/pureSignal/`. Preserve the existing
`autoCalEnabled` leaf for desired automatic-calibration intent. New leaves are
`RunCalibrationProcessing`, `AutoAttenuate`, `QuickAttenuate`,
`MoxDelaySeconds`, `LoopDelaySeconds`, `RequestedTxDelayNs`,
`HardwarePeakOverrideEnabled`, and `HardwarePeakOverride`. Corresponding
properties use lower camel case. The default intent is false, run processing
true, auto attenuation true, quick attenuation false, loop delay 0 seconds,
and MOX delay 0.1 seconds. Retain the existing requested amplifier delay and
source/board-backed peak default; the override is a positive finite double
when explicitly enabled. Keep requested delay separate from actual applied
delay. Both local and remote clients consume this single settings object.

## Verification conventions

Run commands from the execution checkout, with its own `build` directory. On a
configured developer machine:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON
```

Build each named test target before invoking its CTest name. New test filenames
below also define the target names to register in `tests/CMakeLists.txt` using
the existing test helper and `TestSandboxInit.cpp` storage isolation. New helper
sources must be added to the correct Core/GUI library in `CMakeLists.txt`.
Use condition/signal assertions instead of sleeps. Consequential replay,
settings ownership, teardown, and parser invariants get regression coverage
before their behavior changes when reproducible.

## Task 1: Establish the integration and compatibility baseline

**Requirements:** R-WDSP-01/02, R-UI-PS-03, R-VERIFY-PS-01.
**Deliverable:** A reproducible upstream/delta inventory and retained-behavior
fixtures, attached to the execution revision. **Dependencies:** none.

**Owned files:** `docs/attribution/WDSP-PROVENANCE.md`, new
`docs/architecture/wdsp210-control-coverage.md`,
`docs/architecture/wdsp210-verification/README.md`, existing CFC/PS/NR tests and
new `tests/tst_wdsp210_cfc_compat.cpp`. Inventory changes to `third_party/wdsp/`
without overwriting the source until classification is complete.

**Interfaces:** Produces the source manifest, current exported-symbol/ABI
inventory, CFC characterization fixtures, and control-coverage ledger used by
Tasks 2-8. The ledger includes symbol/source, units/default/range, owner,
property/action, UI, settings key/scope/load-save path, and verification.

- [x] Reconcile the isolated implementation checkout with the current accepted
  integration branch. Record HEAD, dirty-state ownership, configure options,
  compiler/OS, and existing relevant test results. Keep hardware disconnected
  for software-only checks.
- [x] Obtain the pinned upstream source tree and hashes. Classify every current
  vendor deviation as retained Thetis extension, Nereus platform/lifecycle
  patch, superseded upstream change, or obsolete PS2 API. Record reasons rather
  than treating a successful link as compatibility evidence.
  Census new NNR/NURBS sources and generated model arrays, including
  `nnet`, `nnio`, `nnr`, `nnr_model_0/1`, `nurbs`, `nurbs_fit`,
  `nurbs_spline`, `extrapolate`, and new RXA/TXA dependencies. Preserve or
  deliberately retire current-only `FDnoiseIQ`, platform and stub files based
  on real references; the current source glob can silently compile additions.
- [x] Audit the pinned correction writer/reader and `ns_build` branch overlap/
  extension geometry. Record the maximum valid aggregate spline points before
  choosing a parser point cap; 1616 is only an initial estimate and must not
  reject a valid writer output. Census binary NNR models in both supported
  dtypes and saved PS3 fixtures against Task 5's product file-size ceilings.
- [x] Capture the existing CFC response/output for default and non-default
  Qg/Qe profiles and existing profile loading. Include the 1.29/2.10 same-symbol
  signature change in the manifest. Pin reference input/expected comparisons
  and their numerical tolerance to repeatable pre-upgrade observations.
- [x] Populate the coverage ledger from spec sections 6-7 and pinned sources,
  including aliases, read-only hardware fields, and reasoned implementation-
  owned exclusions. Record the real existing source of every retained default.

**Acceptance:** Every vendor difference has a disposition; every supported
NNR/PS3 runtime entry has a proposed surface; CFC fixtures exercise Qg/Qe
independently. Do not infer Rock 5C load or PS3 RF behavior from these fixtures.

**Verification:** Source inspection, hash manifest, existing NR/PS/CFC tests,
and the new linked CFC characterization target before import. Existing public
PS2 tests supply the baseline; expected PS3 API changes are documented, not
silently removed when they fail. No production changes precede this baseline.

**Execution advice:** Lead owns Git and shared ABI decisions. A `gpt-5.6-sol`
specialist at high effort suits the bounded vendor audit while the lead traces
session/UI integration; one inventory, no competing import branches.

## Task 2: Import WDSP 2.10 and reconcile the application ABI

**Requirements:** R-WDSP-01/02, R-PS3-01/04/07, R-VERIFY-PS-01.
**Deliverable:** Both binaries compile/link the pinned baseline with documented
compatibility and safe PS3 adapters. **Dependencies:** Task 1 dispositions and
CFC baseline. No UI advertises working NNR/PS3 before its later tasks pass.

**Owned files:** `third_party/wdsp/src/`, its build/include files,
`src/core/wdsp_api.h`, `src/core/TxChannel.{h,cpp}`,
`src/core/dsp/TxChannelState.h`, `src/core/WdspEngine.cpp`, root CMake sources
and provenance; new `src/core/dsp/Ps3DisplayAdapter.{h,cpp}` and
`Ps3Snapshot.h`; necessary ABI-call-site changes in `PureSignal.cpp` and
`AmpViewWindow.cpp`; relevant linked-WDSP/setter/CFC tests and new
`tests/tst_ps3_display_adapter.cpp`, `tests/tst_linux_port_wait.cpp`.
Keep display buffers in the Core adapter rather than in widgets.

**Interfaces:** `SetTXACFCOMPprofile` remains the application's seven-argument
compatibility export `(channel,n,F,G,E,Qg,Qe)`; declarations and definitions
must agree. Retain upstream split `SetTXACFCOMPGprofile` and
`SetTXACFCOMPEprofile` plus curve/weight support. The PS display wrapper returns
a bounded value snapshot with four sample arrays, four correction arrays,
counts and phase reference; Task 4 defines its lifecycle metadata.

- [x] Import all required pinned source/dependencies and embedded model data.
  Reapply classified NR3/NR4, VOX/DEXP, channel/meter/analyzer/diversity/cache,
  static-build and platform patches explicitly. Audit new RXA/TXA lifecycle
  and declarations instead of restoring whole old headers over new structs.
- [x] Add Qg/Qe compatibility to 2.10 `cfcomp.{c,h}`. Preserve the old Thetis
  Gaussian-tail evaluation exactly, with its constants, Q floor, and sorted
  frequency/value/Q tuples. Store independent Qg and Qe arrays/modes. A supplied
  Qg/Qe selects that band's retained calculation; null uses stock linear/NURBS
  behavior. The legacy setter updates both profiles; native split setters
  clear only their corresponding legacy Q array/mode before recalculation.
  Do not approximate Q with NURBS degree or weights. Update all prototypes so
  there is no silent five-versus-seven-argument collision.
- [x] Reconcile POSIX primitives used by the new PS3 source, including
  `CreateSemaphoreW`, `WAIT_OBJECT_0`, and wait-any
  `WaitForMultipleObjects(FALSE, INFINITE)`. Preserve macOS unique named
  semaphore behavior and correct initial counts. Implement the supported wait
  contract off the audio thread with explicit unsupported-mode/error handling;
  interruption cannot masquerade as a signaled semaphore.
- [x] Repair worker lifetime: request stop, await confirmed completion, close
  all owned semaphore/thread handles, then free calibration state. A 500 ms
  wait that times out cannot justify freeing a live worker's memory. Exercise
  pending calculation/save/restore and repeated create/destroy. macOS native
  evidence passed; the remaining platform runs are tracked in Task 9.
- [x] Reconcile exact PS3 signatures, remove obsolete PS2 wrapper/state calls,
  and update TX construction defaults from pinned TXA/board sources. Preserve
  valid saved timing values. Keep `SetPSTXDelay`'s returned applied value.
  Include all dependent call-site compile fixes in this deliverable: never
  fake removed controls with no-op wrappers or interpret new display memory
  as the old cubic format. Tasks 4 and 8 complete coordinator/UI behavior.
- [x] Preallocate `GetPSDisp` storage before calling it: four sample arrays of
  4096 doubles and four correction arrays of 512 doubles. These maxima come
  from pinned `calcc.c` collection geometry and `DISP_PTS`, not from returned
  counts. Validate counts and finite values after the call before publishing.
  Add source/compile guards so changing the vendor geometry forces review.
- [x] Update provenance, source/header inventory and diagnostics version.
  Verify fresh/existing wisdom and impulse-cache cases without using a moving
  upstream source or silently disabling existing optional NR implementations.

**Acceptance:** CFC characterization passes for both Q arrays, Qg only, Qe only,
and null-Q stock behavior; switching to native split profiles cannot retain old
Q state. New RX/TX channels create/process/destroy without leaks or callbacks
after free. All caller signatures match. Maximum-size PS display buffers pass
address/bounds checks; malformed returned counts never reach a plot/transport.

**Verification:** Build desktop/daemon plus `tst_tx_channel_ps_setters`,
`tst_wdsp_ps_smoke`, `tst_wdsp210_cfc_compat`, `tst_ps3_display_adapter`,
`tst_linux_port_wait` and impacted existing targets.
Exercise real linked WDSP and a sanitizer-enabled focused build where supported
for the buffer/lifetime boundary. Run platform build gates and source/attribution
checks. Synthetic feedback here proves software behavior, not RF acceptance.

**Execution advice:** `gpt-5.6-sol` high effort or lead ownership for this coupled
C ABI/platform work. Shared vendor/TxChannel edits are serialized; independent
downstream UI work starts only after these interfaces are settled.

## Task 3: Persist and apply complete NNR configuration

**Requirements:** R-NNR-01/03/04/07, R-WDSP-02, R-PERSIST-01..04.
**Deliverable:** NNR works through the local model/Core path and survives real
save/load and receiver recreation. **Dependencies:** Tasks 1-2.

**Owned files:** new `src/core/dsp/NnrSettings.h` and
`src/core/dsp/NnrAdapter.{h,cpp}` (including `NnrDiagnostics`);
`src/core/WdspTypes.h`, `RxChannel.{h,cpp}`, `WdspEngine.{h,cpp}`,
`src/core/dsp/RxChannelState.h`, `src/models/SliceModel.{h,cpp}`,
`RadioModel.{h,cpp}`, `src/core/AppSettings.{h,cpp}` for versioned narrow
migration; new `tests/tst_nnr_settings.cpp`,
`tests/tst_wdsp_nnr.cpp`; extend `tests/tst_rx_channel_rebuild.cpp` and
`tests/tst_slice_persistence_per_band.cpp`. Serialize shared model edits with
Tasks 4 and 6.

**Interfaces:** Implements the shared NNR contract above. Consumes Task 2's
linked NNR API, initially with bundled models and controlled test paths.
Task 5 subsequently connects asset-backed process model paths. Produces
accepted SliceModel properties and diagnostics consumed by Tasks 6-7.

- [x] Add the stable enum, properties, validation, exact AppSettings leaves,
  change notifications, and scheduled save wiring. Validate stored values as
  well as live edits; reject non-finite values. Keep source-backed defaults for
  absent keys without replacing valid legacy NR selection.
- [x] Add the radio identity/settings prefix and one-time migration described
  above. Test same-MAC migration, missing/mismatched MAC, idempotent reload,
  retained legacy values, and two radios with the same stable slice ID.
- [x] Remove reliance on saving only `m_activeSlice` for NNR edits. Capture
  dirty stable slice identities when changes are accepted; flush each affected
  slice before deletion or orderly shutdown. A popup bound to an inactive slice
  must save that slice, even if the active slice changes during the debounce.
- [x] Add the NNR branch to mutual-exclusion dispatch and old carry/run flags.
  Apply model selection, readiness checks, then tuning, and only then enable.
  Protect setters that upstream calls through a potentially absent model.
  Report an unavailable requested model without claiming it is active.
- [x] Apply loaded settings to every allocated slice, including slices created
  after connection. Extend capture/apply with the actual NR slot and NNR tuning;
  the current legacy `nrMode` carry alone is insufficient. Do not replace the
  established pointer-preserving sample-rate path with ad hoc wrapper rebuilds.
- [x] Reset tuning through the same setters/storage scope while preserving
  selection and global assets. Reset diagnostic modes on a fresh station
  session; leave normal tuning and disabled-feature values intact.
- [x] Add storage and linked-WDSP tests covering both models, unavailable model,
  finite processing, rate/enable/disable/recreate lifecycle, and two-slice
  isolation. Verify actual DSP readback after reload, not only model equality.

**Acceptance cases:** Store model 1, mask -31.25 dB, pre-AGC, alpha 1.75,
knee 12.5 dB, tau 2.75 s, maximum gain 13.5 dB, and smoothing 17.25/83.5 ms
while NNR is off. Save/reload into fresh objects; all values survive. Selecting
NNR applies those values before run. Recreate channels and repeat on slice B
with different settings without changing slice A. An invalid edit is neither
applied nor saved. Reset/restart restores only that slice's tuning defaults.
Edit slice B with slice A active, then switch active slices before the scheduled
save; after process restart both must retain their own values.
Old enum values round-trip unchanged. Identity/low-pass diagnostics cannot
survive a fresh session as hidden processing modes.

**Verification:** `tst_nnr_settings`, `tst_wdsp_nnr`,
`tst_rx_channel_rebuild`, `tst_slice_persistence_per_band`, and the retained NR
tests selected by the Task 1 inventory. Build named targets, then run their
anchored CTest selection. Live listening and Rock load remain Task 9 evidence.

**Execution advice:** `gpt-5.6-terra` at high effort can own bounded NNR model/
adapter work after the vendor ABI is fixed. RadioModel and lifecycle changes
remain serialized with the lead's settings/session work.

## Task 4: Implement PS3 state, correction lifecycle, and durable preferences

**Requirements:** R-PS3-01/02/03/04/05/07, R-PERSIST-01..04.
**Deliverable:** A Core-owned PS3 coordinator with correct status/actions,
safe lifecycle and persistent configuration distinct from operation.
**Dependencies:** Task 2; asset references integrate with Task 5.

**Owned files:** `src/core/PureSignal.{h,cpp}`, `PsccPump.{h,cpp}`,
`TxChannel.{h,cpp}`, `src/models/RadioModel.{h,cpp}`, and associated TX/MOX
ordering hooks; new `src/models/PureSignalSettings.{h,cpp}` and
`src/core/dsp/Ps3Snapshot.h`; extend `tests/tst_puresignal_coordinator.cpp`,
`tst_tx_channel_ps_setters.cpp`, `tst_wdsp_ps_smoke.cpp`,
`tst_radio_model_puresignal_run_wiring.cpp`; new
`tests/tst_ps3_settings_persistence.cpp` and `tst_ps3_lifecycle.cpp`.

**Interfaces:** PureSignalSettings is a Core-owned QObject of validated normal
configuration with Q_PROPERTY notifications. PureSignal remains the operation
coordinator. `Ps3Snapshot` contains channel/session generation, sequence/time,
16 raw status integers and decoded fields; AmpView data is a separate optional
bounded snapshot. The settings model cannot key TX or restore a correction by
being hydrated. Semantic actions are defined in Task 6.

- [x] Move normal retained configuration to PureSignalSettings: loop/MOX/
  requested TX delay, peak override/default selection, auto-/quick attenuation,
  desired automatic-calibration and run-calibration preferences. Retain existing
  per-MAC key identities, especially `pureSignal/autoCalEnabled`; add explicit
  new leaves through the same scope. Keep live correction, counters, MOX,
  measured peak, pending operations and one-shot requests out of save/load.
- [x] Implement non-actuating hydration, validation and accepted-value updates.
  Load board defaults then supported persisted overrides before operational
  initialization. Preserve local readiness-controlled automatic intent; remote
  session restoration never invokes startAutomaticCalibration. GUI geometry,
  On Top, indicator colors/visibility, and display preferences remain local.
  Split the current behavior into `initializeAutoCalPreference(bool)` (hydrate
  only) and `resumeAutomaticCalibrationPreference()` (local, readiness-gated).
  The first must never set `_autoON`, call WDSP control, or key TX.
- [x] Decode `info[5]` as successes and `info[7]` as attempts; drive automatic
  attenuation on new attempts, including failures. Decode compound bitfields
  (including overdrive bit in `info[6]`) and `info[12]` file status. Display raw
  reserved entries honestly instead of inventing meanings.
- [x] Implement coherent Off, Single, Automatic and ApplyCurrentCorrection
  transitions. `runCal=false` suspends processing but need not remove an
  existing correction. Off/reset must complete the real stop transition even
  from that state, and its result reflects correction-off acknowledgement.
  Persist run-processing intent independently. Applying true to resume a
  suspended engine must honor operational permission/readiness and must not
  release an old queued action from another session. Receive-only settings
  changes can record intent but cannot resume calibration implicitly.
- [x] Preserve board routing/aligned sample feed; deliver MOX-off before TXA
  shutdown. Retire pending operations/display subscriptions on disconnect or
  channel replacement and use Task 2's confirmed worker teardown.
- [x] Produce source-backed display snapshots and verify the transform below
  independently of widgets. Include count/finite checks, freshness and latest-
  value backpressure. Use at most the existing 100 ms PS polling cadence;
  acquire display snapshots only while a subscriber exists.

The transform derives from pinned `calcc.c` normalization/display generation:
`x` is normalized feedback magnitude; `ym*x` is normalized transmit magnitude;
`yc/ys` are cosine/sine correction components. For the retained chart axes:

| Series | Horizontal coordinate | Vertical coordinate |
| --- | --- | --- |
| Measured magnitude | `ym*x` | `x` |
| Measured gain | `ym*x` | `1/ym` (skip zero/non-finite divisor) |
| Measured phase | `ym*x` | `wrap180(atan2(ys,yc)*180/pi - phs_ref_deg)` |
| Magnitude correction | `xm_cor` | `ym_cor*xm_cor` |
| Gain correction | `xm_cor` | `ym_cor` |
| Phase correction | `xa_cor` | `ya_cor` (already unwrapped, referenced degrees) |

Implement `wrap180` as a bounded mathematical normalization with documented
endpoint convention, tested at +/-180 and crossings. Do not subtract phase
reference twice, invert atan2 arguments, or treat curve coordinates as cubic
coefficients. Derive the transform from WDSP's data contract and retained
Nereus axes; no external client's UI logic is ported by this plan.

**Acceptance:** A failure increments attempts and drives attenuation without a
false success; combined bits show overdrive alongside other failures. Saved
delay/peak/attenuation values survive restart and apply at initialization;
loading automatic intent produces no remote actuation. Off succeeds with
processing suspended. Disconnect during a file/calibration operation leaves
no live worker referencing freed state. Known phase components (including
quadrant crossings) and a nonzero reference generate expected plot values.

**Verification:** Run the six coordinator/settings/lifecycle targets listed
above and retained feedback/DDC tests from Task 1. Test real WDSP where possible
and inject status/permission/file failures at explicit seams; never overdrive
a transmitter merely to exercise a warning. Assert ordering and command counts
as well as final booleans. RF convergence/distortion remains Task 9.

**Execution advice:** Lead or `gpt-5.6-sol` high effort; serialize with Task 3's
RadioModel edits. Independent plot math/fixture work can run once snapshot
shape and source conventions are fixed.

## Task 5: Add station-owned models and correction assets

**Requirements:** R-NNR-04/06, R-PS3-05/06, R-PERSIST-01..04.
**Deliverable:** Validated, persistent station assets with explicit lifecycle
application and reliable correction file results. **Dependencies:** Tasks 2-4;
remote transport is connected in Task 6.

**Owned files:** new `src/core/dsp/DspAssetStore.{h,cpp}` and
`DspAssetValidation.{h,cpp}`; Core initialization and WdspEngine model-path
configuration, PureSignal correction workflow, settings classification;
new `tests/tst_dsp_asset_store.cpp`, `tst_nnr_model_assets.cpp`, and
`tst_ps3_correction_files.cpp`; provenance/coverage entries.

**Interfaces:** Asset IDs are opaque station identifiers, never client paths.
Each record has kind (`nnr-model`/`ps3-correction`), content hash, size, format/
version, compatibility metadata, user-visible label and validation state.
NNR slot choices distinguish requested asset, applied asset and pending apply.
PS saved selection is separate from the explicit restore action.

- [x] Store assets beneath the station profile's writable directory with
  generated short filenames and atomic publication. Separate model slot
  settings (process-wide) from per-radio correction metadata. Persist IDs and
  hashes, not a workstation-specific absolute path.
- [x] Preflight NNR `WDSPNN` version-1 files before invoking `nnio`: 32-byte
  header, 72-byte tensor descriptors, 40-byte names, rank 1..4, supported
  float32/float64 dtype, descriptor/data extents, checked multiplication and
  addition, finite weights and tensor shapes/names compatible with the pinned
  model architecture. Upstream's unchecked `int` products/total are not the
  import validator. Reject duplicate/conflicting tensors and truncated data.
- [x] Bound transfer and parser resources explicitly. Initial application
  policy: 64 MiB maximum model file, 1 MiB maximum correction file, and 64 KiB
  decoded transfer chunks. These are product resource limits, not algorithm
  maxima. Before enabling import, verify bundled models and a freshly saved
  PS3 fixture fit, plus float64 model representation; if a valid supported
  format exceeds a ceiling, adjust the named limit with fixture evidence.
  Do not allocate according to untrusted declared counts before validating.
- [x] Validate PS3 correction version/structure with a non-actuating parser
  derived from `nurbs_spline.c`. Version 2 text contains MAG/COS/SIN in order,
  six EMA metadata rows per curve, `curve_ema_pts 256` and 256 values, then
  branches with index/point-count/midpoint and x/y pairs. Verify exact keys,
  ordering, finite values, valid normalized coordinates and spline sizes,
  1..16 branches, Task 1's confirmed aggregate-point cap, and EOF. Preserve
  the checksum contract `abs(stored-sum) <= abs(sum)*1e-9 + 1e-9`.
  Harden the vendor reader as well as host preflight before allocation.
  Reject legacy PS2/version 1, incompatible radio/algorithm metadata, malformed
  counts and incomplete files without calling PSRestoreCorr. Preserve originals
  and report useful format/compatibility errors; no invented PS2 conversion.
- [x] Apply selected NNR paths before any receiver creation. An override chosen
  while receivers exist is persisted as pending and requires the explicit
  established reconnect/lifecycle operation. Default/bundled mode must not
  accidentally discover models through the process working directory.
  Require encoded NNR path plus terminator to fit the upstream 512-byte buffer;
  reject truncation rather than silently loading another path.
- [x] Serialize correction save/restore and identify each pending operation.
  Saving publishes a validated temporary file only after actual completion;
  restore is an actuating command with permission/interlock checks. Surface
  upstream asynchronous file errors and ignore stale completions after session
  replacement. Validate encoded path length against WDSP's 256-byte storage,
  including the terminator, before calling the fixed-size path API.
  `info[12]` failure bits alone cannot prove successful completion. Add a narrow
  `GetPSFileOperationStatus` compatibility readback under the source's update
  lock: operation kind, pending flag, completion generation and result.
  Advance generation on every worker completion path, successful or failed;
  serialize requests and compare the captured generation. Add checked vendor
  path copies. Host pending/completed/error state must follow this evidence.
  Declare the C compatibility readback in `wdsp_api.h` and its vendor header:
  `int GetPSFileOperationStatus(int channel, int kind, PSFileOperationStatus* out)`.
  `kind` is 0 save / 1 restore; return 1 for a valid copied snapshot and 0 for
  invalid channel/kind/output. The struct contains `uint64_t generation`,
  `int pending`, and `int result` (0 success, 1 failure, 2 cancelled); result is
  meaningful only for a completed generation. Copy under the update lock;
  never retain the caller's pointer or query after channel teardown.
- [x] Persist selected assets and visible fallback/pending state. A missing
  custom asset must not overwrite the saved preference with the bundled ID.
  Export yields the validated bytes; interrupted import publishes no asset.

**Acceptance:** Standard/Premium bundled files and valid custom replacements
round-trip. Oversized, overflowed, truncated, wrong-shape, bad-hash and non-finite
files fail without allocation spikes or changing active settings. Model changes
remain pending until controlled recreation. A valid PS3 save/restore round-trip
works on a software fixture; invalid PS2/PS3 files leave correction unchanged.
Remote restore with TX permission absent is refused before a WDSP call. Missing
asset and pending selections remain truthful after restart.

**Verification:** The three new asset/file targets use temporary station stores
and real source-format fixtures. Include parser bounds/fuzz corpus tests and
interruption/atomic-publish checks. A fixture restore verifies parsing/lifecycle,
not radio linearization. Source-format validation is a prerequisite to enabling
an import UI or transferring a file to WDSP.

**Execution advice:** `gpt-5.6-sol` high effort for the parser/worker boundary;
bounded storage/UI pieces can use `gpt-5.6-terra` after asset contracts settle.
No parallel edits to PureSignal lifecycle with Task 4.

## Task 6: Mirror accepted settings and expose bounded session services

**Requirements:** R-NNR-05/06/07, R-PS3-05/06/07, R-UI-PS-01,
R-PERSIST-01..04. **Deliverable:** Local/remote clients share one settings and
action contract; Core authority and receive-only gating survive replay.
**Dependencies:** Tasks 3-5 interfaces. Includes daemon persistence completion.

**Owned files:** `src/core/session/StationCapabilities.{h,cpp}`,
`MirrorPolicy.cpp`, `MirrorEnumDomain.cpp`, `StateMirror.{h,cpp}`,
`SessionMessages.{h,cpp}`, `SessionCommandDispatcher.{h,cpp}`,
`StationServer.{h,cpp}`, `StationClient.{h,cpp}`;
`src/core/settings/SettingsScope.cpp`, `SettingsProxyServer.{h,cpp}`;
`src/core/daemon/DaemonApp.{h,cpp}`, `src/server_main.cpp`, Core initialization;
new `src/core/session/PureSignalSessionFacade.{h,cpp}`;
new `tests/tst_nnr_session.cpp`, `tst_ps3_session.cpp`,
`tst_dsp_settings_restart.cpp`; extend existing session codec/mirror/capability,
settings scope/proxy and daemon profile tests.

**Shared interfaces:** Ordinary NNR and PureSignalSettings properties use the
existing StateMirror allowlist and Q_PROPERTY mechanism. Introduce a negotiated
`PropertyResult` carrying object ID, property, client sequence, accepted flag,
reason, and authoritative typed value. Keep the sequence scoped to session
generation. Validate before mutating/persisting; late replies cannot replace a
newer edit. SettingsProxy's raw storage writes are not a bypass around model
validation for these properties.
For batched writes, read back all requested fields after applying the whole
batch and return per-field outcomes with the same write ID. This avoids hiding
normalization/refusal behind StateMirror's current inbound-notify suppression.
Reuse the session codec's lossless integer conventions for IDs/sequences;
do not silently round 64-bit values through unrestricted JSON doubles.

`PureSignalSessionFacade` is a GUI-independent QObject used by all PS widgets.
It exposes the accepted `PureSignalSettings` model and `Ps3Snapshot` readback,
`requestAction(Ps3Action, QVariantMap)` returning an operation ID, and
`setAmpViewSubscribed(bool)`. Local requests call the coordinator through the
same validation/action contract; remote requests use the existing
CommandInvoke/CommandResult transport. Action results distinguish accepted,
pending, completed and failed using operation ID and session generation.

Add these verbs and argument contracts to the existing dispatcher:

| Verb/action | Arguments | Permission/behavior |
| --- | --- | --- |
| `nnr.setDiagnostics` | slice ID, test mode 0..2, output layout 0..1 | controlling session; transient, validated |
| `nnr.resetTuning` | slice ID | scoped normal-settings reset; persists |
| `nnr.applyModelSelection` | selected slot/asset revision | controlling session; controlled reconnect, never an implicit raw load |
| `ps3.off` / OffReset | none | controlling-session stop; available without TX permission |
| `ps3.single` / Single | none | TX permission/readiness required |
| `ps3.automatic` / StartAutomatic | none | TX permission/readiness required; saved intent alone is insufficient |
| `ps3.applyCurrent` / ApplyCurrentCorrection | none | TX permission/readiness required |
| `ps3.twoTone` / SetTwoTone | enabled boolean | enabling requires TX permission/interlocks; stopping remains allowed |
| `ps3.saveCorrection` / SaveCorrection | bounded asset label | controlling session; no keying, completed-file result |
| `ps3.restoreCorrection` / RestoreCorrection | validated asset ID | TX permission/readiness required |
| `ps3.subscribeDisplay` | enabled boolean | authorized read access, bounded latest snapshot |
| `dspAssets.list/beginImport/chunk/finishImport/export` | kind, asset/transfer ID, size/hash, offset and bounded data as applicable | authenticated existing station roles; bounded storage/transfer, no DSP activation |

`Ps3Action` contains exactly the named actions in the table. All commands reject
unknown fields/types/values and unknown capabilities. Asset transfer IDs and
operation IDs are station/session-owned; disconnect retires incomplete work.
Do not introduce a parallel text control protocol.

- [x] Advertise WDSP version/compatibility revision, NNR/model availability,
  PS algorithm/schema version, property-result support, display and asset
  features. Negotiate independently from `pureSignalPresent` hardware support.
  Older Core/GUI peers degrade explicitly without renumbering enums or writing
  unsupported cached values back on connect.
- [x] Register accepted property/status objects and real write validation.
  Normal settings pass through the same live model validators locally and
  remotely. Refused writes return accepted state and produce no saved change;
  echoed snapshots and SettingsProxy hydration do not call action setters.
- [x] Implement typed actions and bounded asset transfer over the authenticated
  channel. Gate at Core even if a client bypasses disabled widgets. Preserve
  controlling-session ownership; inspection rights never imply mutation rights.
- [x] Encode display snapshots with schema, channel/session identity, sequence,
  actual counts and freshness. Max payload is bounded by 4x4096+4x512 doubles
  (147456 data bytes) plus fixed metadata, capped at 160 KiB per frame. Use a
  dedicated bounded binary display message compatible
  with the existing transport framing, not an oversized generic JSON command.
  Integration clarification: preserve the transport’s 64 KiB per-message cap
  through negotiated bounded chunks, capped at 160 KiB per assembled snapshot
  with latest-only assembly; count every transmitted chunk in R35 telemetry.
  Publish at most once per 100 ms while subscribed, one latest pending frame;
  stale sequence/session data is dropped without changing the graph's owner.
- [x] Load Core's validated stored configuration before publishing the initial
  state snapshot; that snapshot wins on reconnect. Resolve pending model paths
  before channels exist. Flush every dirty slice and PS/model preference into
  AppSettings, then `AppSettings::save()` on orderly daemon shutdown after
  pending changes are captured. Handle file-save failure visibly in diagnostics.
- [x] Close raw SettingsProxy write/remove bypasses for the new authoritative
  DSP keys; either route through the same validator or reject with a typed
  reason. Keep GUI-only presentation settings local and preserve existing
  unrelated settings-proxy behavior.

**Acceptance:** A GUI reconnect with old local values adopts Core values.
Concurrent/delayed edits cannot roll a property backward. After accepted remote
edits, a fresh Core process restores the same tuning and the GUI shows it.
Rejected edits and fabricated raw settings writes cannot become next-start DSP
state. Snapshot replay emits zero action commands. Every actuating PS action is
rejected with `txPermitted=false`; off/reset and tone-stop still work. Closed
AmpView produces no ongoing display subscription or queue growth.

**Verification:** New session and real settings-file restart targets plus
existing codec/mirror, `tst_settings_scope`, `tst_settings_proxy`, and
`tst_daemon_settings_profile`. Assert actual dispatched WDSP calls/command counts
and saved files through temporary profiles. Cover old peers, malformed payload,
resource limits, disconnect/reconnect and session generation turnover.

**Execution advice:** Lead or `gpt-5.6-sol` high effort for cross-cutting
authority/replay. Schemas, daemon/model ownership and Git stay serialized. This
is a useful consolidated independent-review boundary before final UI wiring.

## Task 7: Deliver the complete NNR popup and Setup UI

**Requirements:** R-NNR-01..07, R-UI-PS-01..03, R-PERSIST-01..04.
**Deliverable:** Every NNR control is discoverable and bound to accepted local
or remote state. **Dependencies:** Tasks 3, 5, and 6 contracts working.

**Owned files:** `src/gui/widgets/VfoWidget.{h,cpp}`,
`DspParamPopup.{h,cpp}`, `RxDashboard.{h,cpp}`,
`src/gui/setup/DspSetupPages.{h,cpp}`, `src/gui/MainWindow.{h,cpp}`;
new `src/gui/widgets/NnrControls.{h,cpp}` shared by popup/Setup;
new `tests/tst_nnr_controls.cpp`; extend `tests/tst_vfo_tooltip_coverage.cpp`.
Core/model files are consumed through their public interfaces only.

**Interfaces:** NnrControls binds a guarded slice identity plus session
generation, Task 3 properties/diagnostics, and Task 6 model-asset actions.
Its host chooses compact or full layout. Add an NNR Setup deep link that carries
the opening slice identity, rather than resolving whichever slice is active
when a delayed callback runs.

- [x] Add NNR in the available fourth grid cell beside NR4/DFNR/MNR, the
  RxDashboard selector, and MainWindow's DSP/NR menu. Use enum data, not display
  order, for selection. Honor mode/backend capabilities.
- [x] Extend DspParamPopup only with reusable fractional numeric and expandable
  group support. Share bindings through NnrControls so Setup and popup expose
  identical accepted values and validation. Retain existing popup behavior.
- [x] Implement model/suppression quick controls; advanced position, alpha,
  knee, tau, gain, attack and release; Diagnostics/Models/More Settings links;
  and scoped Reset NNR tuning. Include source-backed help, units, defaults,
  keyboard context action, pending/refused states and diagnostic badge.
- [x] Show actual model availability and pending model-asset application.
  Require the explicit lifecycle action for an override; never load arbitrary
  GUI paths into live remote receivers. Retire callbacks on slice deletion or
  session replacement and keep normal settings editable while NNR is off.
- [x] Exercise each coverage row from a real control and inspect native Qt
  screenshots at minimum size, high DPI, expanded groups, and screen edges.
  Opening/right-clicking/deep-linking must not enable NNR or redirect to another
  slice when the active slice changes.

**Acceptance:** Edit fractional values in popup, observe Setup synchronization,
close/reopen and restart, then inspect the same accepted DSP values. Do this for
local and remote sessions and two slices. Right-click and keyboard context
menu emit zero enable commands. Refused model selection visibly returns the
accepted state and cannot alter next-start preferences. Reset survives restart.

**Verification:** Functional QtTest `tst_nnr_controls` for signal/state/action
behavior plus native Qt screenshot/interaction evidence for geometry and
readability. Existing popup/tooltip tests stay valid. Human listening/polish
feedback is recorded separately in Task 9, not presumed from widget presence.

**Execution advice:** `gpt-5.6-terra` at high effort; eligible alongside Task 8
after shared service contracts settle. MainWindow/CMake edits are lead-owned
integration points and cannot be concurrently modified by both workers.

## Task 8: Evolve the PureSignal dialog and AmpView

**Requirements:** R-PS3-02..06, R-UI-PS-01..03, R-PERSIST-01/03/04.
**Deliverable:** Complete PS3 control/status and accurate bounded AmpView with
durable presentation. **Dependencies:** Tasks 4-6.

**Owned files:** `src/gui/PsForm.{h,cpp}`, `AmpViewWindow.{h,cpp}`,
`AmpViewChart.{h,cpp}`, `src/gui/applets/PureSignalApplet.{h,cpp}`,
`src/gui/PsaIndicatorWidget.{h,cpp}` and MainWindow integration;
extend `tests/tst_psform.cpp`, `tst_ampview_window.cpp`,
`tst_applet_ps_wiring.cpp`; new `tests/tst_ps3_display_transform.cpp`.

**Interfaces:** Consumes the session-neutral PS settings/status/command surface
from Task 6 and bounded display data from Task 4. Widgets never call GetPSDisp
or retain TxChannel pointers. Presentation persists through GUI AppSettings.

- [x] Retitle and regroup the modeless dialog into quick actions, status,
  Calibration, Timing & Feedback, and Diagnostics. Remove obsolete live PS2
  PIN/MAP/STBL/PTOL/TINT controls/calls while preserving inactive migration data.
  Add run-calibration control, requested/applied delay, full status decoding,
  file outcomes and hardware-owned readbacks with explanations.
- [x] Route applet/indicator context menus to the same semantic actions and
  settings/AmpView/diagnostic pages. Separate saved automatic-calibration intent
  from operational arming. Gate controls by actual capability/permission; menu
  opening and model refresh generate no calibration or TX command.
- [x] Replace PS2 cubic reconstruction with Task 4's source-backed PS3 data
  transform. Preserve the reference line, measured/correction magnitude and
  phase series, Show Gain, Phase Zoom, Low Res, and On Top. Add per-series
  visibility and a readable legend; retain gain behavior around zero safely.
- [x] Bind display subscriptions to visibility and session identity. Bound
  update cadence/backpressure, unsubscribe on hide/close/disconnect, and mark
  stale/absent data explicitly. Graph updates cannot trigger correction state.
- [x] Persist geometry, On Top, expanded sections, all existing display choices
  and added series visibility. Restore under signal blockers before wiring
  actions. Keep legacy view keys so existing preferences survive the upgrade.
  Recover a window onto an available screen if the saved monitor disappeared.
- [x] Replace tests that assert removed PS2 widgets/defaults with PS3 contract
  assertions backed by the pinned source. Retain still-valid behavior checks;
  do not rewrite a test merely to bless an accidental UI change.

**Acceptance:** Known PS3 snapshots produce the expected samples/curves and
phase reference; zero inputs produce no division by zero or NaN chart values.
All runtime/readback inventory rows are reachable. A compound error bitfield
shows every relevant error. Save/restore reflect completion/failure. With
`txPermitted=false`, every UI entry refuses actuation and read-only status works.
Restart retains view choices and tuning without replaying an action.

**Verification:** Build/run the four named UI/display targets. Inspect the
actual Qt dialog and AmpView at minimum size (AmpView currently 440x380),
high DPI, expanded diagnostics, long errors, unavailable capability and session
replacement. Capture working local/remote control evidence against testable
backends. RF trace quality is pending until Task 9's arranged bench.

**Execution advice:** `gpt-5.6-terra` at high effort for presentation after the
math/ABI contract is established; escalate transform ambiguity to the lead.
May run beside Task 7 with separate files and serialized shared integration.

## Task 9: Verify the combined build, persistence, and operator workflows

**Requirements:** All design requirements, especially R-PERSIST-01/02/03/04 and
R-VERIFY-PS-01. **Deliverable:** Evidence tied to the final combined revision,
with explicit passed/pending hardware rows. **Dependencies:** Tasks 1-8.

**Owned files:** `docs/architecture/wdsp210-verification/README.md`, control
coverage ledger, affected regression fixtures, and migration/operator notes.
Tests belong beside the behavior they check; this task combines their evidence
and closes integration gaps rather than creating a second copy of each test.

- [x] Verify sandboxed settings/model restart boundaries through actual
  AppSettings files: two slices and two radio identities, feature-off tuning,
  fractional values, reset, model override pending/applied state and existing
  PS/AmpView migration. Session fixtures verify accepted remote readback; native
  channel/reconnect fixtures verify applied engine values.
- [ ] Complete the full desktop and headless-Core/remote-GUI process-relaunch
  matrix with an operator. The software fixtures above do not substitute for
  that application-level workflow.
- [x] Change accepted state on Core, reconnect a GUI holding stale settings,
  and recreate Core models from the saved file. The last accepted Core value wins. A refused remote edit
  must not appear in the saved file. Exercise orderly shutdown with a pending
  scheduled save through the same synchronous shutdown flush. Fresh settings
  and Core models retain the edit without action replay. Full application
  process-relaunch observation remains in the preceding unchecked row.
- [x] Exercise an older-Core handshake, missing model, unsupported property,
  interrupted asset transfer, channel/slice deletion, and session replacement
  with NNR popup, PS dialog, and AmpView open. Verify no stale target writes,
  accidental operation, hung shutdown, or unbounded snapshot queues.
- [x] Build desktop, `nereusd`, and all tests on the shared Core/GUI base plus
  this implementation, then run the full suite after focused corrections:
  716/716 passed on macOS arm64. Run source/attribution and GUI/DSP gates.
- [x] Complete serial integration with the Core/GUI owner: signed merge
  `fe9e0dc1` preserves the independent wideband/CMake changes and slice-preserving
  reconnect path. Fresh desktop, `nereusd` and all-tests build passed;
  **718/718** combined CTest executables passed. See the
  [combined source verification](2026-09-20-remote-daemon-r3-verification/combined-wdsp210.md)
  for scope, machine load, gates and outstanding hardware acceptance.
- [ ] Run the remaining Windows/Linux and Linux ARM64 platform matrix, including
  native worker lifecycle checks. Public CI remains on the existing publication
  hold; macOS results do not imply those platforms passed.
- [ ] Arrange an operator session for NNR listening and the three UI workflows:
  tune/reset/restart NNR; inspect and adjust PS3; view AmpView at minimum size
  and reconnect. Record operator, platform, revision, expected observations,
  and actual outcomes. Until observed, mark subjective usability pending.
- [ ] Arrange Rock 5C and desktop receive measurements on the same workload:
  NNR off, Standard, Premium, then multi-slice remote display/audio load.
  Record sample/buffer rates, CPU, memory, audio continuity/underruns, latency,
  duration and machine load. Compare with the baseline and existing R3
  sustained-session criteria; do not copy AMD manual figures as acceptance.
- [ ] Arrange an explicitly authorized local supported-radio PS3 bench with
  known feedback routing. Record board/protocol/band/power, calibration attempts
  and success, attenuation behavior, applied correction, AmpView and measured
  PS-off/on distortion. Keep untested families/protocols pending. R4 owns remote
  transmit authorization, watchdog, starvation, handoff and remote PS3 RF tests.

**Core final commands (after focused target checks):**

```sh
cmake --build build --target NereusSDR nereusd all_tests
ctest --test-dir build --output-on-failure
python3 scripts/verify-no-gui-dsp-access.py
python3 scripts/verify-no-captured-slice-spectrum-wiring.py
git diff --check
```

Run Linux GUI tests with the existing CI display setup and run the attribution
commands configured in `.github/workflows/ci.yml`/the pre-commit hook. Record
actual platform evidence; an unrun platform is pending, not assumed equivalent.

**Acceptance:** All 24 requirements have coverage; every supported control has
a working entry point and declared persistence scope; accepted settings survive
the matrix; remote actuation stays refused until R4. Preserve a truthful
separation between software-ready, operator-reviewed, and RF-accepted status.

**Execution advice:** Lead performs integration and one consolidated review at
the DSP/session/persistence boundary. An independent `gpt-6-astra` high-effort
review is warranted for this substantial change; move it earlier if ABI or
authorization risk can invalidate downstream work. Do not add routine per-task
reviewer loops. The executor rechecks available models and actual risk.

## Requirement coverage and delivery

| Requirements | Primary tasks |
| --- | --- |
| R-WDSP-01, R-WDSP-02 | 1, 2, 3, 4, 9 |
| R-NNR-01, R-NNR-02, R-NNR-03 | 3, 6, 7 |
| R-NNR-04, R-NNR-05, R-NNR-06, R-NNR-07 | 3, 5, 6, 7, 9 |
| R-PS3-01, R-PS3-03, R-PS3-07 | 2, 4, 9 |
| R-PS3-02, R-PS3-04 | 4, 6, 8 |
| R-PS3-05, R-PS3-06 | 4, 5, 6, 8, 9 |
| R-UI-PS-01, R-UI-PS-02, R-UI-PS-03 | 1, 6, 7, 8, 9 |
| R-PERSIST-01, R-PERSIST-02, R-PERSIST-03, R-PERSIST-04 | 3, 4, 5, 6, 7, 8, 9 |
| R-VERIFY-PS-01 | 1, 9 |

Keep one execution ledger in the verification document: task owner, revision,
checks/results, outstanding concerns and next action. Shared schemas, Git,
CMake, RadioModel, MainWindow and hardware remain serialized. Commit coherent
deliverables with provenance updates using repository signing and hooks.

Execution followed the approved design and Yonder risk-based policy. The
Core/GUI owner incorporates this feature at a signed, serial local Git boundary;
the existing public-push/PR-mutation hold remains in force. Software evidence
and remaining acceptance are recorded above and in the execution ledger. No
operator listening, Rock capacity result or RF acceptance is claimed.
