# Phase 3G-10 Handoff — 2026-04-15 (Stage 1 partial cut)

## TL;DR

Phase 3G-10 Stage 1 was cut at the **S1.2–S1.7 line**. The branch `feature/phase3g10-rx-dsp-flag` delivers the widget library + SliceModel stub surface + VfoWidget S-meter integration as a single reviewable PR. Tasks **S1.8, S1.9, S1.10, S1.11** are deferred to a follow-up branch, to be opened after this PR is reviewed and decisions are made on the tab-area rewrite.

- **Branch:** `feature/phase3g10-rx-dsp-flag`
- **Worktree:** `/Users/j.j.boyd/NereusSDR/.worktrees/phase3g10-rx-dsp-flag`
- **Plan:** `docs/architecture/phase3g10-rx-dsp-flag-plan.md` (§S1.8-§S1.11 intact, deferred)
- **Design:** `docs/architecture/2026-04-15-phase3g10-rx-dsp-flag-design.md`
- **Skill used:** `superpowers:subagent-driven-development`

## What shipped on this branch

### Setup commits (pre-implementation)

| SHA | Subject |
|---|---|
| `89cdcf8` | phase3g10(design): add RX DSP parity + AetherSDR flag port design |
| `06d02dd` | phase3g10(plan): add two-stage implementation plan |
| `30d2eb8` | phase3g10(style): add VfoStyles.h with verbatim AetherSDR style constants |
| `53111b2` | phase3g10(docs): fold S1.1 review findings + Thetis pre-gate resolution |
| `9e41664` | phase3g10(docs): add handoff document for Stage 1 resume *(superseded by this doc)* |

### Stage 1 implementation commits

**S1.2 — VfoLevelBar widget + unit test** (3 commits)

| SHA | Subject |
|---|---|
| `ab0db70` | phase3g10(meter): add VfoLevelBar widget + unit test |
| `20941b5` | phase3g10(meter): use kS9Dbm constant in isAboveS9 |
| `8bc58a8` | phase3g10(meter): VfoLevelBar review fixups — braces + min width |

Delivers: `src/gui/widgets/VfoLevelBar.{h,cpp}` + `tests/tst_vfo_level_bar.cpp` (6/6 pass). S-meter bar with S-unit tick strip above, cyan below S9 / green at/above. `minimumSizeHint {150, 22}` (raised from plan's {120, 22} to avoid S1 label clipping). Uses `kS9Dbm` constant instead of inline `-73.0f`.

**S1.3 — ScrollableLabel widget + unit test** (2 commits)

| SHA | Subject |
|---|---|
| `1a11a2f` | phase3g10(widget): add native ScrollableLabel widget + unit test |
| `44fb643` | phase3g10(widget): ScrollableLabel review fixups — setRange + filter lifecycle |

Delivers: `src/gui/widgets/ScrollableLabel.{h,cpp}` + `tests/tst_scrollable_label.cpp` (4/4 pass). **NereusSDR native composite widget** (QStackedWidget with QLabel + QLineEdit), richer than AetherSDR's trivial ~20-line wheel-emitter. Wheel step with range clamp, double-click inline edit with Enter commit / Esc cancel, custom format callback. First widget task to use the user-approved "source-first exception for control surfaces" — Thetis source-first governs DSP behavior; Qt widgets are NereusSDR native. See `feedback_source_first_ui_vs_dsp.md` in user memory. `setRange` silently clamps without emitting; event filter installed once in ctor (not per edit cycle).

**S1.4 — Small utility widgets** (1 commit)

| SHA | Subject |
|---|---|
| `3c4be4a` | phase3g10(widget): add GuardedSlider, ResetSlider, CenterMarkSlider, TriBtn |

Delivers 4 header-only widgets in `src/gui/widgets/`: `GuardedSlider.h` (with embedded `ControlsLock` singleton), `ResetSlider.h`, `CenterMarkSlider.h`, `TriBtn.h`. Verbatim ports from AetherSDR `GuardedSlider.h:13-47` and `VfoWidget.cpp:68-129`, with the one permitted deviation of adding braces to `GuardedSlider::wheelEvent`'s `if (delta != 0)` for CLAUDE.md compliance. AetherSDR issue numbers (#570, #745, #1026) rewritten to preserve rationale but drop numbers. `GuardedComboBox` was deferred from this commit and landed in S1.5.

**S1.6 — SliceModel stub setters** (4 commits, lands before S1.5 per reorder)

| SHA | Subject |
|---|---|
| `9d2d50f` | phase3g10(model): extend SliceModel with stub setters for new DSP state |
| `6393c54` | phase3g10(model): reconcile SliceModel defaults against Thetis |
| `5fda1de` | phase3g10(model): S1.6 review fixups — qFuzzyIsNull + plan + fmsq comment |
| `c753836` | phase3g10(model): expand SliceModel API to match Thetis for S1.5 containers |

Delivers 31 new `Q_PROPERTY`s on `SliceModel` for the Stage 2 DSP NYI list, plus 5 new `enum class`es in `src/core/WdspTypes.h` (`NrMode`, `NbMode`, `SquelchMode`, `AgcHangMode`, `FmTxMode`). Every setter body is pure state-and-emit; no `RxChannel` forwarding. Stage 2 replaces each body with WDSP wiring.

**Thetis source-first reconciliations applied during S1.6:**
- `m_fmCtcssValueHz`: 88.5 → 100.0 (Thetis `console.cs:40500`, `radio.cs:2899`)
- `m_rttyMarkHz`: 2125 → 2295 (Thetis `setup.designer.cs:40635`/:40637 — plan's 2125 was the SPACE tone misfiled as MARK)
- `m_agcHang`: 0 → 250 ms (Thetis `radio.cs:1057`)
- `digOffsetHz` split into `diglOffsetHz` + `diguOffsetHz` (Thetis `console.cs:14672` DIGLClickTuneOffset, `:14637` DIGUClickTuneOffset)
- `fmSimplex{bool}` replaced with `fmTxMode{FmTxMode}` enum (Thetis `console.cs:20873`, `enums.cs:380`)
- `FmTxMode` ordering: `{ High=0, Simplex=1, Low=2 }` per Thetis `enums.cs:380` ("order chosen carefully for memory form")

**Review-finding fixups:**
- `setAudioPan` and 4 other new double setters: replaced `qFuzzyCompare` with `qFuzzyIsNull(lhs - rhs)` to handle the `setAudioPan(0.0)` case (Qt contract: `qFuzzyCompare` is undefined when either arg is 0.0). Pre-existing `setFrequency` has the same latent issue — out of scope, flagged for a future sweep.
- `m_fmsqThresh` comment rewritten: was self-contradictory (cited Thetis `fm_squelch_threshold = 1.0f` but stored `-150.0`); now explicitly documents the scale-domain mismatch and defers reconciliation to the Stage 2 gate.

Thetis citation stats: **8 real Thetis citations / 20 neutral defaults / 3 pending-gate placeholders** (`m_agcThreshold`, `m_agcAttack`, `m_fmsqThresh` scale).

**S1.5 — VfoModeContainers (FM/DIG/RTTY)** (1 commit)

| SHA | Subject |
|---|---|
| `45574c8` | phase3g10(widget): add VfoModeContainers (FM/DIG/RTTY) + GuardedComboBox |

Delivers `src/gui/widgets/VfoModeContainers.{h,cpp}` (three containers) + `src/gui/widgets/GuardedComboBox.h` (deferred from S1.4) + `tests/tst_vfo_mode_containers.cpp` (9/9 pass). **NereusSDR native widgets informed by AetherSDR patterns**, not verbatim ports — binds to the S1.6 SliceModel API.

- **`FmOptContainer`**: CTCSS mode + value combos, FM repeater offset QSpinBox (kHz → Hz conversion), 3-way TX direction (Low/Simplex/High mutually exclusive) + independent Reverse toggle. Reimagines AetherSDR's 4-state direction as the Thetis 3-state enum + separate reverse flag.
- **`DigOffsetContainer`**: single offset control routed by `m_slice->dspMode()` → `diglOffsetHz` or `diguOffsetHz`. Test proves DIGL write leaves DIGU unchanged and vice versa.
- **`RttyMarkShiftContainer`**: Mark (2295 default) and Shift (170 default) scrollable labels with nudge buttons (step 25 Hz / 5 Hz).

**`CwAutotuneContainer` was dropped from Stage 1 scope.** Neither AetherSDR (closest: dynamic button insertion in `updateTabsForMode`, not a container) nor Thetis (only `EnableApolloAutoTune` matches, which is Apollo antenna tuner, not CW matched-filter) has a CW-autotune container. Deferred to a future phase with explicit native design.

**S1.7 — VfoWidget top-row surgical edit** (1 commit)

| SHA | Subject |
|---|---|
| `0cd1008` | phase3g10(flag): VfoWidget S-meter → VfoLevelBar + split badge |

Delivers surgical edits to `src/gui/widgets/VfoWidget.{h,cpp}`:
- Replaced dead `m_smeterLabel` stub with `VfoLevelBar* m_levelBar`
- Added hidden `QLabel* m_splitBadge` for split-mode indication (Stage 2 wires)
- `setSmeter(double)` now forwards to `m_levelBar->setValue(float(dbm))` with null guard
- `m_nbBtn`/`m_nrBtn`/`m_anfBtn` **intentionally preserved** — S1.8 drops them when it replaces the DSP grid, keeping the intermediate commit buildable

Plan §S1.7 was amended to reflect the tightened scope: MeterPoller has no signals (`smeterChanged(double)` does not exist in NereusSDR), and the current VfoWidget already has most expected top-row members. S1.7 did NOT add MeterPoller wiring — Stage 2 builds the meter pipeline. **`setSmeter` still has zero external callers** (same as pre-S1.7 state); the widget is a visual shell.

### Test suite status

| Test binary | Cases | Status |
|---|---:|---|
| `tst_vfo_level_bar` | 6 | ✅ pass |
| `tst_scrollable_label` | 4 | ✅ pass |
| `tst_vfo_mode_containers` | 9 | ✅ pass |

All other existing tests still pass except `tst_p1_loopback_connection` which is a **pre-existing failure** (confirmed against the S1.4 baseline) — requires a live radio response, not a code regression.

## What did NOT ship — deferred tasks

| Task | Plan § | Reason for deferral |
|---|---|---|
| S1.8 | §S1.8 | Large tab-area rewrite (~600-750 lines of new/changed code, 300+ deleted, 2-3 sub-commits). Existing VfoWidget has 3 tabs with decoupled AetherSDR-pattern signal wiring to MainWindow; plan's direct-SliceModel coupling needs adjustment. Current `m_nbBtn`/`m_nrBtn`/`m_anfBtn` get dropped here when the DSP grid replaces them. |
| S1.9 | §S1.9 | Mode-container visibility rules — blocked by S1.8 (containers must be embedded in a tab first). |
| S1.10 | §S1.10 | Floating lock/close/rec/play buttons — existing VfoWidget has `m_closeBtn`/`m_lockBtn`/`m_recBtn`/`m_playBtn` already; task needs re-scoping to verify what's actually missing. |
| S1.11 | §S1.11 | Tooltip coverage test — needs the full tab surface from S1.8 to be meaningful. |

**Stage 1 is complete enough for a partial PR**: all widget library pieces, the SliceModel surface, and the first integration toehold ship. The visual flag gets its real tab rewrite on a follow-up branch.

## Known open items & stubs

1. **`m_splitBadge` hidden and unwired** — lives in VfoWidget header row, `setVisible(false)` default. Stage 2 (or a follow-up branch) wires split semantics.
2. **`VfoWidget::setSmeter(double)` has zero external callers** — the meter display is connected internally but never driven. Stage 2 (MeterPoller pipeline) is the real meter wiring commit.
3. **`fmCtcssMode` as int-enum** (values 0-3 for Off/Enc/Dec/Enc+Dec) while Thetis uses `bool ctcss_on`. Stage 2 collapses the multi-state to the Thetis bool at WDSP wiring time; DCS variant is deferred further.
4. **Three `citation pending Stage 2 gate` defaults** in SliceModel.h: `m_agcThreshold`, `m_agcAttack`, `m_fmsqThresh` (scale mismatch acknowledged). The Stage 2 source-first gate enforces replacement before feature-slice commits.
5. **Pre-existing `setFrequency` `qFuzzyCompare` latent issue** — same class as the S1.6 `setAudioPan` fix but out of S1.6 scope. Sweep in a future phase.
6. **`tst_p1_loopback_connection` failure** — pre-existing, unrelated to 3G-10.

## Decisions locked (unchanged from original handoff)

1. **Scope A**: Phase 3G-10 finishes every RX-side DSP NYI before 3M-1 TX.
2. **Flag styling**: faithful AetherSDR port — flat transparent header, underline tabs, 26px `#c8d8e8` freq label, green-checked DSP toggles, blue-checked mode toggles, round `#c8d8e8` slider knobs.
3. **S-meter choice C**: gradient bar with S-unit ticks rendered *above* it (S1/3/5/7/9/+20/+40), cyan below S9 turning green above. **Delivered in S1.2.**
4. **Tooltips choice B**: Thetis-first with NereusSDR-voice fallback for native-only controls; tooltip coverage enforced by `tst_vfo_tooltip_coverage`. **Deferred to S1.11 follow-up branch.**
5. **Mode containers A**: full AetherSDR parity — FM OPT, DIG offset, RTTY mark/shift inside the DSP tab. **CW autotune dropped from Stage 1** (no Thetis source). Partial delivery in S1.5 (widgets exist as standalone; embedding in DSP tab happens in S1.8 follow-up).
6. **Persistence B**: per-slice-per-band bandstack. DSP-flavored state under `Slice<N>/Band<KEY>/*`; session-state under `Slice<N>/*`. Not yet started — Stage 2 work.
7. **Approach 3 with immediate backfill**: Stage 1 ships the flag visual shell first, Stage 2 fills WDSP wiring. **Stage 1 is now split across two branches** as of this cut.

## How to resume — follow-up branch instructions

After this PR is reviewed and merged (or while it's in review, off the same base), open a new branch for the S1.8-S1.11 continuation:

```
git checkout main
git pull origin main
git checkout -b feature/phase3g10-stage1-tabs
```

**Before dispatching subagents for S1.8:**

1. Re-read plan §S1.8 and **amend it** to reflect the current VfoWidget reality:
   - VfoWidget has 3 tabs currently, not 4. S1.8 adds XRitTab.
   - Existing signal wiring is VfoWidget→MainWindow→SliceModel (decoupled AetherSDR pattern), not direct VfoWidget→SliceModel.
   - MainWindow has no `wireVfoWidget()` method — connects are inline starting around `MainWindow.cpp:1623`.
   - NyiOverlay exists and uses `NyiOverlay::markNyi(widget, QStringLiteral("tag"))` pattern.
2. Consider breaking S1.8 into sub-commits for reviewability:
   - **S1.8a**: Tab bar 3→4 + add XRitTab + inline MainWindow signal stubs (all new surface, no existing-tab touches)
   - **S1.8b**: DspTab rewrite — 4×2 grid + APF tune slider + embed `FmOptContainer`/`DigOffsetContainer`/`RttyMarkShiftContainer`, drop `m_nbBtn`/`m_nrBtn`/`m_anfBtn` members (this is the commit that finally drops them, per the S1.7 baton-pass)
   - **S1.8c**: AudioTab rewrite — add pan/mute/bin/squelch/agc-T, replace `m_agcCmb` with 5-button row, preserve afGain + existing agcMode behavior
3. S1.9 depends on S1.8b landing (containers must be embedded first).
4. S1.10: verify what `m_closeBtn`/`m_lockBtn`/`m_recBtn`/`m_playBtn` already do in the current VfoWidget before treating it as a from-scratch build.
5. S1.11: tooltip coverage test only makes sense after the full tab surface exists.

**Resume prompt for a fresh session:**

> I'm continuing Phase 3G-10 Stage 1 on a follow-up branch after the S1.2-S1.7 partial PR landed. Read `docs/architecture/phase3g10-handoff.md` (this file) for context, then pick up at **Task S1.8 — VfoWidget tab bar + four tab panes**. Re-read plan §S1.8 and apply the amendment notes from this handoff's "How to resume" section before dispatching. Use the `superpowers:subagent-driven-development` skill. Work in a new worktree off `main` (post-merge) at `feature/phase3g10-stage1-tabs`. Dispatch S1.8 as 3 sub-commits (S1.8a/S1.8b/S1.8c), then continue with S1.9/S1.10/S1.11. Stop at the end of Stage 1 to open a second draft PR.

## Known non-critical drifts to fix opportunistically

1. **`CLAUDE.md` references `Project Files/Source/Console/wdsp.cs`** — actually `dsp.cs`. Design doc is fixed; CLAUDE.md is stale.
2. **`CLAUDE.md` "Current Phase" table lists `3I-TX: Next`** — renumbered to `3M-1` per MASTER-PLAN.
3. **`README.md` roadmap table** — does not list 3G-10 (only MASTER-PLAN does). Reconcile when convenient.

None block progress.
