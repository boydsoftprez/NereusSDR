# PR #293 Full-Range Stabilization Design

**Date:** 2026-07-29
**PR:** `boydsoftprez/NereusSDR#293`
**Base:** `102f2d7c95d15104c44e52723bdaf3fe3ee3fee5`
**Fresh head at scope freeze:** `40cc91907870f6811ac8ac2a661f6f862795f2ad`
**Range at scope freeze:** 323 commits, 292 changed files

## Purpose

Stabilize the complete Phase 3F pull-request range before it is merged. This is
not a patch against the formerly reviewed tip alone. The work covers:

1. every confirmed finding from the fresh full-range review;
2. sibling call sites with the same violated invariant;
3. the complete current commit range, including commits that landed while the
   review was in progress;
4. actionable GitHub conversation feedback and any inline review threads; and
5. the failing cross-platform and attribution checks.

The final result stays on the existing PR branch. Each behavioral change is
developed test-first, followed by focused verification, the complete practical
local suite, a clean application build, and a short isolated-config launch
smoke test before signed commits are pushed.

## Scope Inventory

The original full-range review was performed at
`df0d9a4126c98d639af4a9fb958e5a43339b15a9`. A fresh fetch during design
advanced the PR to `40cc91907870f6811ac8ac2a661f6f862795f2ad` with four
additional commits:

- `f678e9fe` seeds the P2 per-DDC ADC map;
- `7cc35f20` carries antenna-driven ADC routing to the ANAN-G2 codecs;
- `2f4c4f6a` makes Alex analysis follow the DDC-to-ADC assignment; and
- `40cc9190` updates the pan-chain tests to seed the real assignment path.

Those commits are inputs, not work to repeat. Their claims and regression
coverage must be verified against the remaining DDC/ADC findings.

The thread-aware GitHub inventory at this head reports no review submissions
and no inline review threads. Two external conversation comments remain
substantive:

- Per-ADC DDC assignment must drive the physical Alex filter chain, with
  bypass selected independently when one chain spans incompatible filter
  ranges.
- Diversity must consume a synchronized even/odd DDC pair on ADC0/ADC1 and the
  DDC model must expose one authoritative assignment to all transports.

The optional sample-rate-budget idea in the second comment is not treated as a
defect to invent inside this stabilization pass. The concrete transactional
sample-rate bug is in scope; a new hardware/transport/CPU capacity policy would
need its own product and hardware design.

Before implementation begins, refresh the PR head again. If it has moved,
audit every new commit and amend this inventory before editing overlapping
code.

### Confirmed Defect Ledger

The implementation plan must map a failing regression or static assertion to
each ledger entry:

1. missing direct Qt meta-object includes break non-macOS compilation;
2. External Diversity calls WDSP state before engine ID 0 is created;
3. the legacy PureSignal update can overwrite the full seven-DDC P2 map;
4. TX mode follows the active slice rather than the TX-bound slice;
5. the MOX band-plan guard validates the first/active slice;
6. Alex TX routing follows the active slice's band;
7. audio MOX gating and withdrawal follow the active slice;
8. `sliceAdded` handlers use a stable ID as a list position;
9. VFO TX handoff and TX badges mix stable IDs with positions;
10. mixer withdrawal leaves queued audio eligible for summation;
11. layout teardown can leave a floating window holding a freed applet;
12. the codec's DDC-to-ADC choice is not published consistently to all model,
    Alex, and wideband consumers;
13. wideband and wing-click callbacks are captured by the startup pan and can
    drive the wrong co-hosted slice;
14. stream-rate narrowing commits before every migration is known to succeed;
15. inactive-slice AGC edits mutate the active slice;
16. per-pan overlays/settings hard-code Slice A or pan 0, including persisted
    Extended-view `false`;
17. TCI enumerates existing slices by position; and
18. RF2KS disconnect/failure handling accepts stale replies and skips initial
    backoff.

The same-root audit also includes antenna-picker identity, RADE pan/slice
routing, diversity Source A removal, effective PureSignal state, Hermes
suspension, TX-bound VOX/TGXL/WDSP channel selection, and physical-filter
compatibility in Alex Auto mode. Alex compatibility is determined by the
actual filter selected for each frequency, not merely by unequal amateur-band
enums; two bands that share one physical BPF do not require bypass.

The attribution verifier previously identified these exact candidate sites,
which must each be source-compared rather than mechanically tagged:

- `src/models/RadioModel.cpp`;
- `src/core/RadioConnection.h`;
- `src/core/P2RadioConnection.cpp`;
- `src/core/codec/CodecContext.h`;
- `src/core/codec/P2CodecSaturn.cpp`;
- `src/core/codec/P1CodecAnvelinaPro3.cpp`;
- `src/core/codec/P2CodecOrionMkII.cpp`;
- `tests/tst_alex_per_adc_bpf_wire.cpp`; and
- `tests/tst_codec_5_slice_assignment.cpp`.

## Design Invariants

### 1. Stable Slice Identity

Slice IDs are persistent identities, not positions in `RadioModel::slices()`.
Every signal, widget callback, protocol command, and persisted binding that
carries a slice token resolves it with `sliceById()`. Positional APIs remain
internal only where list order is explicitly the contract.

This invariant covers:

- `sliceAdded` consumers in `MainWindow`;
- TX handoff and TX badge state;
- antenna-picker callbacks;
- TCI startup enumeration;
- diversity Source A selection and removal behavior;
- inactive-slice AGC edits; and
- remove-middle/re-add sequences.

Tests construct non-contiguous list order, remove a middle slice, re-add it,
and assert that the intended object, channel, and UI surface receive each
operation.

### 2. TX-Bound Authority

The active slice means keyboard/UI focus. The TX-bound slice is the sole
authority for transmit behavior.

The following consumers use `txBoundSlice()` consistently:

- transmit mode and TX DSP mode;
- band-plan MOX guard;
- Alex TX low-pass and antenna routing;
- audio MOX gate and withdrawal;
- VOX mode gating;
- TGXL transmit-side band/autotune decisions; and
- the WDSP receiver channel disabled or reconfigured for transmit.

Regression tests deliberately make active Slice A and TX-bound Slice B differ
in frequency, band, and mode. Every TX consumer must follow B while UI focus
continues to follow A.

### 3. Per-Pan Ownership

Each `PanadapterApplet` owns a pan index, a `SpectrumWidget`, its settings
namespace, its overlay controls, and its slice-selection context. A single
`MainWindow` helper wires every applet, whether it exists at startup or is
created later.

The helper covers:

- wideband and wing-click routing;
- extended-view persistence;
- overlay mode/filter controls;
- pan-specific active-slice resolution;
- RADE synchronization and frequency-offset routing; and
- load/save of the correct pan settings.

No callback may capture the startup spectrum widget or hard-code Slice A.

Before layout teardown, all floating windows are closed or re-docked and their
tracking entries removed. A floating window never retains a pointer to an
applet that a new layout may delete.

### 4. One Protocol Assignment Authority

`DdcAssignment` is the complete authoritative assignment for Protocol 2.
Effective MOX, PureSignal, diversity, rate, DDC, ADC, sync, enable, and
suspension state enter one codec context and produce one full assignment.

`PsDdcConfig` remains only where Protocol 1 requires the legacy state machine.
It must not overwrite DDC3-DDC6 after the full P2 assignment has been emitted.

The effective PureSignal state, not merely the auto-calibration preference,
feeds the codec context. Hermes-class PureSignal collapse explicitly marks
unavailable user streams suspended. Assignment changes remain frozen where
the hardware protocol requires a MOX barrier.

`publishDdcAssignment()` propagates both:

- hardware DDC selection for every stream; and
- the ADC selected for that DDC.

`ReceiverManager`, `SliceModel`, Alex analysis, wideband routing, and any
transport adapter read that published result rather than reconstructing or
defaulting it.

The four commits that landed after the initial review are validated through
this same end-to-end path. Their ADC seed and Alex-chain mapping remain if they
satisfy the unified authority; they are adjusted only where they still create
multiple writers or incomplete publication.

### 5. Diversity Lifecycle and Topology

External Diversity engine ID 0 has a complete lifetime:

1. create when a valid diversity pair becomes active;
2. feed the synchronized ADC0/ADC1 pair;
3. apply gain, phase, receiver number, and output configuration;
4. enable run only after creation and valid input binding;
5. stop before rebinding or teardown; and
6. destroy on disable, disconnect, or owner destruction.

The topology must follow the Thetis/WDSP reference for the single supported
engine: synchronized DDC0/DDC1, DDC0 on ADC0, DDC1 on ADC1, with compatible
physical filter-chain decisions. A null guard around `SetEXTDIV*` is not an
implementation.

Current UI semantics expose one diversity source pair. Supporting multiple
simultaneous diversity receivers is not silently invented here; the assignment
and model must nevertheless be explicit enough that a future XDMA transport
can consume the same authoritative topology.

### 6. Transactional Data Plane

A stream sample-rate change is prepared before it is committed:

1. calculate the proposed geometry;
2. preflight every affected slice placement/migration;
3. apply migrations;
4. update the stream rate and DSP geometry; and
5. emit one authoritative assignment.

If any placement cannot be satisfied, the old rate, bindings, offsets, and
assignment remain intact. Tests cover a shared stream where one slice cannot
migrate after narrowing.

Mixer withdrawal atomically makes the slice non-contributing and clears its
queued frames. Summation includes only currently streaming/enrolled sources.
A withdrawn source can never contribute one stale block.

### 7. Integration Lifecycles

RF2KS requests carry a connection generation, or equivalent tracked-reply
ownership. Disconnect aborts outstanding replies and invalidates late
callbacks. Initial failures enter the same bounded retry/backoff path as later
failures and publish a disconnected/failure state rather than continuing the
normal polling cadence.

TCI startup enumeration uses stable slice IDs. Existing and subsequently
created slices therefore share the same identity contract.

### 8. Cross-Platform and Attribution Integrity

`MainWindow.cpp` directly includes the Qt meta-object headers for every
instantiated complete type. The Linux, Windows, and CodeQL builds must not
depend on macOS-only transitive includes.

Every reported missing attribution tag is rechecked against the exact upstream
revision used by CI. Tags are added only where the source comparison supports
them; original NereusSDR code remains untagged. The same source-first check
also applies to any production code added during this pass.

Before reading Thetis implementation source for a port or behavioral decision,
record:

1. the exact Thetis file and revision;
2. the NereusSDR files being changed;
3. the current entry in `docs/attribution/THETIS-PROVENANCE.md`; and
4. the attribution update required if the source is not already recorded.

## Test Strategy

Every production fix follows red-green-refactor:

1. add the smallest regression that demonstrates the current failure;
2. run only that test and confirm it fails for the intended reason;
3. implement the minimal invariant-level fix;
4. rerun the focused test and neighboring suites; and
5. refactor only while the tests stay green.

The final verification ladder is:

1. focused test executables for each cluster;
2. existing Phase 3F codec, allocator, pan, audio, mixer, and integration
   suites;
3. static guards and `git diff --check`;
4. the repository attribution verifier with the same upstream revisions as
   CI;
5. all practical local tests;
6. a clean macOS application build; and
7. a short application launch using an isolated temporary configuration,
   confirming that the process remains alive without an immediate crash.

No hardware claim is made by the local launch smoke. Hardware-only rows remain
identified as bench verification.

## Commit Structure

The existing 323-commit history is preserved. New work is split into focused,
GPG-signed commits:

1. design and implementation plan;
2. cross-platform build and attribution fixes;
3. stable identity and per-pan ownership;
4. TX-bound authority and RF/audio safety;
5. authoritative DDC/ADC/PureSignal/diversity lifecycle;
6. transactional mixer and sample-rate behavior;
7. RF2KS and TCI lifecycle fixes; and
8. any final test/documentation adjustment that cannot honestly belong to an
   earlier behavioral commit.

Each commit includes its regression tests when practical. Pre-commit hooks are
not bypassed. The branch is pushed only after the post-fix smoke build and
launch pass.

## Acceptance Criteria

- Every confirmed P1 and P2 finding is covered by a regression test or an
  explicit static/compliance assertion.
- Every same-root sibling call site listed above follows the new invariant.
- All commits added after the original review are included and validated.
- GitHub thread inventory is refreshed before handoff; no actionable inline
  thread is silently omitted.
- Focused suites, practical full local tests, attribution checks, clean build,
  and isolated-config launch smoke pass.
- All new commits are GPG-signed and pushed to
  `feature/phase3f-sub-epic-a-foundation`, updating PR #293.
