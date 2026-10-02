# EQ and CFC Editor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver the approved native EQ/CFC layout with dragging, exact entry, reliable undo, and controls that reach the audio engine and saved profiles.

**Architecture:** Keep the shared `ParametricEqWidget` and the existing two dialogs. Add a small session-history helper and a GUI-independent CFC profile codec; route CFC profile changes through TransmitModel and RadioModel to the existing TxChannel setter. Keep TX EQ's established DSP and persistence paths.

**Tech Stack:** C++20, Qt6 Widgets/Core/Test, existing WDSP and zlib, CMake/Ninja.

**Spec:** [EQ and CFC editor design](2026-10-02-eq-cfc-ui-design.md).

## Global Constraints

- Native C++20 and Qt6; no new runtime dependencies or web view.
- Use `StyleConstants.h`, Qt ownership, existing model guards, and `QSignalBlocker`; no new audio-thread locks.
- Preserve existing TX EQ DSP conversion, defaults, and profile formats. In particular, its 5/18-band curves are still sampled into the existing ten-band WDSP EQ profile.
- Preserve existing independent legacy/parametric settings and profile-bank ownership in TxApplet/Setup. Do not create another profile selector or bank.
- All new user-facing strings use `tr()`. Dialog buttons have `autoDefault(false)`. Provide accessible labels, keyboard focus, exact entry, and Undo/Redo shortcuts scoped to the dialog; text-field undo takes precedence while editing text.
- No changes to the core RX path, WDSP algorithms, radio protocol, or release/version.
- Preserve upstream license headers and inline tags; new ports include provenance entries and cites pinned to v2.10.3.15. Follow `docs/attribution/HOW-TO-PORT.md`.
- The maintainer explicitly approved connecting all CFC band counts and Q to audio and saved profiles.

## Review Focus

1. Profile replacement during a gesture: abort the stale gesture, apply the new profile, and rebase history without a writeback (Tasks 3–5).
2. Width at zero/negative gain, extreme Q, or a clipped edge: remain operable without changing frequency or gain (Task 2).
3. Frequencies crossing with 18 bands: retain stable selection and matching CFC frequency/value pairing (Tasks 2 and 5).
4. Malformed/one-sided CFC blobs: use the ten-band fallback, preserve original storage, and never apply half a profile (Task 3).
5. Independent modes/graphs and nonzero globals: reset/undo must preserve unrelated settings and never include measured bars (Tasks 4–5).

## Execution setup and dependency order

Read `CLAUDE.md`, `CONTRIBUTING.md`, the design, and `docs/development/fast-test-loop.md`. Inspect attached worktrees before creating an isolated `codex/eq-cfc-ui` branch using the worktree skill. Carry these two design/plan files into it; leave the preexisting untracked `docs/manual/` alone. Configure `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo` with the repository's existing local dependency options. Every new source and test must be registered explicitly in the relevant CMake file.

Tasks 1–2 precede both dialog integrations. Task 3 precedes Task 5. Finish with Task 6. Use signed commits (`git commit -S`); never bypass hooks/signing. Commit only the task's files.

### Task 1: Exact session-history transactions

**Files:** Create `src/gui/widgets/EqEditHistory.h`, `src/gui/widgets/EqEditHistory.cpp`, `tests/tst_eq_edit_history.cpp`; modify `CMakeLists.txt`, `tests/CMakeLists.txt`.

**Interfaces:** `EqEditHistory : QObject`, parent-owned; `void reset(const QByteArray& state)`, `void beginEdit(const QByteArray& before)`, `void commitEdit(const QByteArray& after)`, `void cancelEdit()`, `std::optional<QByteArray> undo()`, `std::optional<QByteArray> redo()`, `bool canUndo() const`, `bool canRedo() const`; signal `availabilityChanged(bool canUndo, bool canRedo)`. Store opaque immutable audio-state snapshots, with a maximum of 100 completed edits. `beginEdit` is idempotent within an active transaction; commit without begin uses the current baseline. `reset` replaces baseline and clears history/pending edits.

- [ ] Add `noOpDoesNotCreateEntry`, `gestureIsOneEntry`, `newEditDropsRedo`, `resetDropsPendingAndHistory`, `historyRetainsExactBytes`, and `boundedHistory` tests. Assert undo of multiple intermediate updates returns the exact before bytes; identical before/after gives no entry; the 101st edit keeps the newest 100; cancel leaves committed state unchanged.
- [ ] Register/build `tst_eq_edit_history`; run `ctest --test-dir build -R '^tst_eq_edit_history$' --output-on-failure` and confirm the new assertions fail before implementation.
- [ ] Implement the opaque timeline and pending baseline, emitting availability only when it changes. Keep capture/restore responsibility in callers; no models, JSON interpretation, or timers here.
- [ ] Rebuild/run that target and confirm every assertion passes; commit the helper and test registrations.

### Task 2: Shared graph styling, width handles, and gesture boundaries

**Files:** Modify `src/gui/widgets/ParametricEqWidget.h`, `.cpp`, `tests/tst_parametric_eq_widget_interaction.cpp`, `tests/tst_parametric_eq_widget_axis.cpp`, `tests/tst_parametric_eq_widget_paint.cpp`.

**Interfaces:** Add `void setEditorPresentationEnabled(bool enabled)` (default false), `void setMinimumPlotGutters(int leftPx, int rightPx)` (default 0/0), and `QRectF plotRect() const`. Add signals `editStarted()` before the first mutation and `editFinished()` after final existing change signals. Preserve all existing signal signatures and public point/JSON APIs. Presentation flag enables numbered points, selected-only shading, theme tokens, and width handles; callers outside these dialogs retain current visuals.

- [ ] Add interaction tests `widthDragChangesOnlyQ`, `widthHandleAtZeroAndNegativeGain`, `clippedHandleDoesNotMutateQ`, `widthDisabledWithoutQ`, `gestureSignalsBracketFinalCommit`, `crossingRetainsBandId`, and `externalLoadCancelsDrag`. Data rows include Q=0.2/20, 5/10/18 bands, both scales, zero-Hz endpoint, and high/low clipping. Assert no point movement on selection-only click; no dirty final commit without movement; wheel still respects Shift gain/Ctrl frequency/plain Q.
- [ ] Add axis assertions that two widgets with different dB scales and identical minimum gutters have equal plot left/right bounds. Update paint assertions only for opt-in presentation; retain existing default-mode paint expectations and response tests.
- [ ] Build the three targets, run `ctest --test-dir build -R '^tst_parametric_eq_widget_(interaction|axis|paint)$' --output-on-failure`, and observe failures for the new interfaces/behavior.
- [ ] Implement the opt-in paint/hit-testing and selected width gesture. Derive FWHM from the existing `responseDbAtFrequency` formula, including its minimum; convert handle movement through `freqFromX`, then `Q = span / (6 * abs(handleHz - centerHz))`, clamped to existing limits. Treat clipped drawing positions separately from stored Q; prioritize point hit before width handles and use separate hit targets when handles overlap. Use stable `bandId` across ordering; keep existing endpoint locks, curve mathematics, wheel increments, and global-gain drag behavior.
- [ ] Implement gesture signals for point/global/width drag and wheel events, including cancellation on authoritative state replacement. Preserve final `pointsChanged(false)` / `pointDataChanged(..., false)` semantics and include width dragging in `isDraggingNow()`.
- [ ] Rebuild/run the three targets plus `tst_parametric_eq_widget_skeleton` and `tst_parametric_eq_widget_json`; commit after passing. Verify new cites and modification notes preserve existing provenance.

### Task 3: Functional CFC profile plumbing and compatibility

**Files:** Create `src/core/CfcProfile.h`, `.cpp`, `tests/tst_cfc_profile.cpp`, `tests/tst_radio_model_cfc_profile_wiring.cpp`; modify `src/models/TransmitModel.h`, `.cpp`, `src/models/RadioModel.cpp`, `src/core/MicProfileManager.cpp`, `src/core/TxChannel.h`, `.cpp`, `tests/tst_mic_profile_manager_cfc_round_trip.cpp`, `tests/tst_mic_profile_manager_cfc_live_path.cpp`, `CMakeLists.txt`, `tests/CMakeLists.txt`, `docs/attribution/THETIS-PROVENANCE.md`.

**Interfaces:**

```cpp
struct CfcCurveState {
    QVector<double> frequenciesHz, gainsDb, q;
    double globalGainDb, frequencyMinHz, frequencyMaxHz;
    bool useQ;
};
struct CfcProfile { CfcCurveState compression, postEq; };
bool isValidCfcProfile(const CfcProfile& profile);
QString encodeCfcProfile(const CfcProfile& profile);
std::optional<CfcProfile> decodeCfcProfile(const QString& blob);
// TransmitModel additions:
CfcProfile effectiveCfcProfile() const;
bool setCfcProfile(const CfcProfile& profile);
void beginCfcProfileUpdate();
void endCfcProfileUpdate();
// signal: void cfcProfileChanged();
// TxChannel test seams, following existing last*ForTest accessors:
std::array<std::vector<double>, 5> lastCfcProfileForTest() const; // F,G,E,Qg,Qe
quint64 cfcProfileApplyCountForTest() const;
double lastCfcPrecompDbForTest() const;
double lastCfcPostEqGainDbForTest() const;
```

The codec has no widget dependency. Validate both graphs before returning: finite values; 5/10/18 matching band counts; equal, ordered frequencies and equal bounds; valid existing dialog ranges. Do not repair mismatched halves silently. `effectiveCfcProfile()` returns the full-precision active typed state, a decoded valid profile, or the existing ten-band fallback; fallback uses both Use-Q flags false. The nested begin/end API defers aggregate notification until the outer batch ends; use a scope guard to guarantee balance. Profile/settings loads invalidate the runtime typed cache before applying saved fields so loading the same serialized blob still restores its saved values.

- [ ] Add codec data tests `thetisPascalCaseLoads`, `nereusSnakeCaseLoads`, `allCountsRoundTrip`, `badHalfFallsBackWithoutOverwrite`, and `mismatchedFrequenciesRejected`. Assert counts 5/10/18, decimals 125.125 Hz/3.5 dB/Q=1.25, independent Q vectors, and gzip `<SEP>` payload compatible with `ParaEqEnvelope`. Invalid data never changes the model's original opaque blob.
- [ ] Add wiring tests `configuredBandsAndQReachChannel`, `useQRequiresBothGraphs`, `emptyBlobUsesLegacyTenBands`, `typedEditPublishesOneCompleteProfile`, `legacyEditOverridesStaleVariableProfile`, `fullPrecisionReachesChannelBeforeSave`, and `reconnectAndProfileSwitchRestoreWithoutDialog`. Assert a typed Q=1.2345 reaches DSP unchanged, while its saved/reloaded Q uses 1.23; same-blob profile reload restores saved values. Use existing RadioModel channel injection and the narrow TxChannel test-observation pattern to assert complete F/G/E/Qg/Qe vectors, not merely setter signal counts.
- [ ] Build/register/run the new targets and profile round-trip/live-path targets; confirm the missing routing/codec assertions fail.
- [ ] Implement the codec from verified Thetis `frmCFCConfig.cs:333-392,492-575` and `ucParametricEq.cs:1460-1486` at v2.10.3.15. Preserve the gzip envelope and original TX serializer. Add verbatim headers, inline comments/tags and a provenance row to new port files. Read the attribution guide before copying logic.
- [ ] Implement typed TransmitModel apply: validate first, retain full-precision active state, update the serialized blob and globals coherently, mirror integer legacy arrays when count=10, emit existing property signals for changed fields, then one aggregate `cfcProfileChanged()`. Suppress aggregate emission during this typed batch. Existing scalar setters update active/blob globals when valid; existing per-band setters update a valid ten-band profile or invalidate a valid 5/18 profile and fall back to ten bands. Keep `setCfcParaEqData` opaque pass-through for unknown blobs, invalidate the typed cache on authoritative replacement, and emit aggregate change on replacement. Add scoped begin/end batches around CFC application in MicProfileManager and TransmitModel settings loading so replacement exposes one final CFC state. Preserve existing idempotent public property signals.
- [ ] Route RadioModel CFC profile/scalar reconstruction from the aggregate change and the existing connect/reconnect/profile-activation path. Send both Q vectors only when both flags are true. Remove redundant per-property CFC profile rebuild connections; retain enabled gates and unrelated processing. Do not add a dialog-only direct DSP path. Add the stated TxChannel test accessors/caches at the existing successful WDSP setter boundaries, recording accepted arguments rather than rejected calls; no new audio-thread locks or signals. Verify double-valued configured globals reach channel setters while legacy fallback retains integer values.
- [ ] Rebuild/run the new targets plus `tst_tx_channel_cfc_cpdr_cessb_setters`, `tst_mic_profile_manager_cfc_round_trip`, `tst_mic_profile_manager_cfc_live_path`, and `tst_radio_model_eq_lev_alc_wiring`; commit after passing.

### Task 4: Native dual-mode TX EQ editor

**Files:** Modify `src/gui/applets/TxEqDialog.h`, `.cpp`, `tests/tst_tx_eq_dialog.cpp`.

**Interfaces:** Consume Task 1 history and Task 2 graph APIs. Replace `legacyToggle()`'s checkbox UI accessor with `bool usingLegacyEq() const` and `QButtonGroup* modeSelector() const`; update test callers. Retain `onLegacyToggled(bool)` as the existing mode-application seam. Add private `QByteArray captureEditState(bool legacy) const`, `void restoreEditState(bool legacy, const QByteArray& state)`, and `void rebaseEditHistory()`; one history per mode. Runtime snapshots retain unrounded point values, global, range, count, Use-Q and mode-specific controls; selection is restored separately and excluded from equality.

- [ ] Extend `tst_tx_eq_dialog` with `modesKeepIndependentValues`, `legacyAllInputsRemainEditable`, `selectedEditorMatchesGraph`, `widthSliderAndEntryAgree`, `oneDragOneUndo`, `directEntryCommitsOnce`, `countApplyCancelAndUndo`, `liveOffCommitsOnRelease`, and `profileSwitchDuringDragRebasesHistory`. Preserve tests for model/DSP round trip and update assertions tied to intentionally replaced control types.
- [ ] Build/run `tst_tx_eq_dialog` and confirm new behavior assertions fail.
- [ ] Build Graphic/Parametric selector and layouts from the design. Keep existing legacy object names/ranges and direct frequency entries. Move the parametric editor below the graph; add logarithmic Q slider mapping over 0.2–20 with “Wider” / “Narrower” labels. Keep exact Q spin box synchronized under blockers. Show numbered band selection and 5/10/18 controls; retain all advanced algorithm/range/log/live/guide controls in a collapsible section.
- [ ] Wire history around graph gesture signals, slider press/release, spin edit sessions (baseline captured before first value mutation; `editingFinished` commits), wheel events, resets and range changes. Undo/Redo restore exact state under blockers and push once through the existing active-mode commit path. Defer text-field Undo/Redo to the focused editor. Keep histories across hide/reopen; authoritative external model/profile changes cancel/rebase, excluding guarded self-updates.
- [ ] Implement inline count-reset Apply/Cancel; cancelled changes produce no mutation or audio update. Preserve current count-reset/default behavior when applied. Preserve `UsingLegacyEQ` and both independent data sets; do not add hidden obsolete controls just to pass tests.
- [ ] Rebuild/run `tst_tx_eq_dialog`, `tst_tx_applet_lev_eq_cfc`, `tst_tx_channel_eq_setters`, `tst_para_eq_envelope`, and `tst_mic_profile_manager_para_eq_round_trip`; commit after passing.

### Task 5: Matched CFC editor and paired transactions

**Files:** Modify `src/gui/applets/TxCfcDialog.h`, `.cpp`, `tests/tst_tx_cfc_dialog.cpp`.

**Interfaces:** Consume Tasks 1–3. Preserve existing graph/control accessors where controls remain. Add private `CfcProfile captureProfile() const`, `void restoreProfile(const CfcProfile& profile)`, `QByteArray captureEditState() const`, `void restoreEditState(const QByteArray& state)`, and `void rebaseEditHistory()`. Profile commits call `TransmitModel::setCfcProfile` once, never write a variable-band graph into selected integer-array indices. History restores both graphs before committing either.

- [ ] Add `plotsShareExactBounds`, `sharedSelectionSurvivesCrossing18Bands`, `numericAndDragPathsCommitSameProfile`, `independentWidthsReachAudio`, `undoRestoresBothGraphsAndGlobals`, `resetsPreserveOtherGraphAndFrequencies`, `rangeAndCountAreSingleTransactions`, `externalProfileInterruptsGesture`, and `barSamplesDoNotCreateUndo`. Test 5/10/18; both Live Update states; Q on/off; nonzero other-graph values and globals. Retain existing 50 ms timer/show/hide/null-channel tests.
- [ ] Build/run `tst_tx_cfc_dialog`; confirm new assertions fail.
- [ ] Arrange aligned Compression / EQ-after-compression graphs, one selected-band editor below, separate globals and reset buttons, visible count choices, and a collapsible Advanced section. Enable Task 2 presentation on both graphs; reserve common plot gutters from the larger required margins so plot rectangles actually align. Keep graph area usable within the laptop-height acceptance target.
- [ ] Replace array-only widget loading/pushing with effective typed profile loading and coherent typed commits. Synchronize selected band/frequency by stable identity before capturing paired vectors; rebuild matching order in both graphs when frequencies cross. Preserve independent amount/Q; restore loaded bounds/count/Use-Q and globals without writeback. Apply both Use-Q flags from the single existing checkbox. Reject malformed pairs through Task 3 fallback.
- [ ] Connect paired history and exact runtime snapshots. Implement reset semantics from the design and count-reset notice, avoiding signal-driven half-restores. Curve-range edits retain their existing frequency-rescaling semantics and 1000 Hz minimum spread; both graphs rescale together. Preserve measured overlay independently of snapshots, its slicing, and its timer lifecycle.
- [ ] Rebuild/run `tst_tx_cfc_dialog`, `tst_tx_channel_cfc_display`, `tst_cfc_setup_page`, both CFC profile-manager tests and Task 3 wiring tests; commit after passing.

### Task 6: Native verification, documentation, and branch review

**Files:** Create `docs/guides/tx-eq-cfc.md`, `docs/architecture/eq-cfc-ui/verification.md`, and native screenshots `graphic.png`, `parametric.png`, `cfc.png` in that verification directory. Do not modify the preexisting untracked `docs/manual/` files. Update this plan's checkboxes as work completes.

- [ ] Launch the actual Qt application from the execution worktree and capture Graphic EQ, Parametric EQ and CFC at normal and 125%/150% scaling on a 1280×800 display. Exercise all counts, graph/text/slider edits, mode switches, reset/cancel/undo, and open-dialog profile switching. Confirm units, focus, accessible labels, distinct live bars, aligned plots, and no clipped controls. Repair observed layout problems and repeat only affected checks.
- [ ] Document the Graphic/Parametric selector, point/width dragging, exact entry, count-reset notice, undo, CFC globals, and Advanced controls in `docs/guides/tx-eq-cfc.md`. State configured-curve versus measured-bar meaning and preserve the TX EQ ten-band conversion explanation.
- [ ] Rebuild the final changed targets and run their focused tests after any fixes. Then run `cmake --build build --target all_tests` followed by `ctest --test-dir build --output-on-failure`; record actual results. Run existing attribution/hook checks for edited port files. Never report tests as passing without executing the freshly built targets.
- [ ] Request one fresh independent whole-branch review covering the design, audio/profile compatibility, history, geometry, and verification evidence. Resolve actionable findings and rerun affected tests. Make signed final commits; prepare a reviewable branch/PR only within the user's requested delivery scope. Do not merge or release as part of this plan.

## Plan review and execution handoff

Selected execution: **cost-aware Crew subagents**, explicitly requested by the maintainer. Follow Crew's serial task implementers and one integrated independent review; do not stack older Superpowers per-task review loops. Sol 6.1 medium implements routine tasks, Sol 6.1 high handles consequential integration and integrated review. Controller checks each task's actual changes and raw evidence before accepting it.

The maintainer authorized implementation using Crew and cost-aware subagents. The functional CFC connection scope is approved. The Crew ledger records ownership, accepted commits, verification, and any necessary contract corrections.
