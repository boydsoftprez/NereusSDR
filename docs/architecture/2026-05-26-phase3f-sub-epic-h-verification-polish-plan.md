# Phase 3F Sub-Epic H: Bench Verification + Polish — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** After Sub-Epics A-G have all landed, exercise the full Phase 3F surface area on real hardware (G2 + HL2 minimum, ideally also G2E and HermesII), triage bugs surfaced by bench, polish tooltips and status strings, and update operator-facing documentation. After this plan lands, Phase 3F is shippable in a release.

**Architecture:** No new code by default. Bug fixes scoped to whatever the bench matrix surfaces. Polish = tooltip text from Thetis verbatim, error message consistency, status string review, release notes drafting.

**Tech Stack:** Bench hardware + Qt6 app + git + gh CLI for issue tracking.

**Parent design:** [docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md](2026-05-26-phase3f-multi-pan-multi-slice-design.md) §13 (Bench Verification Matrix)

**Prereqs:** Sub-Epics A-G complete.

**Estimated effort:** 5 working days. Variable based on bench bug count.

---

## File Structure

### Files to create

| File | Purpose |
|---|---|
| `docs/architecture/2026-05-26-phase3f-verification/README.md` | Per-SKU × per-feature verification matrix (40 rows × 5 SKUs) |
| `docs/architecture/2026-05-26-phase3f-verification/g2-results.md` | ANAN-G2 bench results, per row |
| `docs/architecture/2026-05-26-phase3f-verification/hl2-results.md` | HL2 bench results |
| `docs/architecture/2026-05-26-phase3f-verification/g2e-results.md` | ANAN-G2E results (pending hardware) |
| `docs/architecture/2026-05-26-phase3f-verification/hermesII-results.md` | HermesII results (if available) |

### Files to modify

| File | Purpose |
|---|---|
| `CHANGELOG.md` | Phase 3F section under unreleased / next-minor |
| `README.md` | Update feature list if appropriate |
| `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md` | Final retrospective with bench-discovered issues |

---

## Task 1: Build the bench verification matrix template

**Files:** Create `docs/architecture/2026-05-26-phase3f-verification/README.md`

- [ ] **Step 1: Create matrix scaffold**

```markdown
# Phase 3F Bench Verification Matrix

| # | Feature | G2 | HL2 | G2E | HermesII | Notes |
|---|---------|----|----|-----|----------|-------|
| 1 | Slice creation up to maxSlices | ☐ | ☐ | ☐ | ☐ | G2=5, HL2=1, G2E=5, HermesII=2 |
| 2 | Slice removal restores chain BPF correctly | ☐ | n/a (1-slice) | ☐ | ☐ | |
| 3 | Per-slice sample rate change — 48 kHz | ☐ | ☐ | ☐ | ☐ | |
| 4 | Per-slice sample rate change — 96 kHz | ☐ | ☐ | ☐ | ☐ | |
| 5 | Per-slice sample rate change — 192 kHz | ☐ | ☐ | ☐ | ☐ | |
| 6 | Per-slice sample rate change — 384 kHz | ☐ | ☐ | ☐ | n/a (P1 192 cap) | |
| 7 | Per-slice sample rate change — 768 kHz | ☐ | n/a | ☐ | n/a | |
| 8 | Per-slice sample rate change — 1536 kHz | ☐ | n/a | ☐ | n/a | |
| 9 | Slice band change (per-band memory load) | ☐ | ☐ | ☐ | ☐ | |
| 10 | AetherSDR overlay (same-band slices on one pan) | ☐ | n/a | ☐ | ☐ | |
| 11 | AetherSDR overlay (cross-band slices, flag migration) | ☐ | n/a | ☐ | ☐ | |
| 12 | Antenna routing — single-antenna | ☐ | ☐ | ☐ | ☐ | |
| 13 | Antenna routing — 2-antenna 2-ADC auto-distribute | ☐ | n/a (1-ADC) | ☐ | n/a (1-ADC) | |
| 14 | Antenna routing — 3+ antenna conflict dialog | ☐ | n/a | ☐ | n/a | |
| 15 | TxSliceArbiter handoff with MOX-drop guard | ☐ | n/a | ☐ | ☐ | |
| 16 | Wideband extension via zoom-out gesture | ☐ | deferred (3F-W) | ☐ | deferred | |
| 17 | Wideband extension auto-bypasses Alex BPF | ☐ | deferred | ☐ | deferred | |
| 18 | Click-in-wing retunes DDC | ☐ | deferred | ☐ | deferred | |
| 19 | Click-in-listenable-island retunes slice | ☐ | ☐ | ☐ | ☐ | |
| 20 | Alex hybrid policy: same-band no badge | ☐ | ☐ | ☐ | ☐ | |
| 21 | Alex hybrid policy: multi-band auto-bypass + WIDE badge | ☐ | ☐ | ☐ | ☐ | |
| 22 | Alex hybrid policy: ForceBand override | ☐ | ☐ | ☐ | ☐ | |
| 23 | Alex hybrid policy: ForceBypass override | ☐ | ☐ | ☐ | ☐ | |
| 24 | Alex policy popup from WIDE badge click | ☐ | ☐ | ☐ | ☐ | |
| 25 | Alex policy popup from CH tag click | ☐ | n/a | ☐ | ☐ | |
| 26 | Diversity enable/disable on Slice A | ☐ | n/a | ☐ | n/a (1-ADC) | |
| 27 | Diversity phase + gain controls | ☐ | n/a | ☐ | n/a | |
| 28 | Diversity radar widget renders + updates | ☐ | n/a | ☐ | n/a | |
| 29 | Diversity 8 memory slots (store + recall) | ☐ | n/a | ☐ | n/a | |
| 30 | Diversity quick-nudge buttons (7 of them) | ☐ | n/a | ☐ | n/a | |
| 31 | Diversity Sync Slice A → Slice B | ☐ | n/a | ☐ | n/a | |
| 32 | Diversity Link ATT | ☐ | n/a | ☐ | n/a | |
| 33 | Diversity direction-finding math (visual check) | ☐ | n/a | ☐ | n/a | |
| 34 | PS during MOX with multiple slices (pause + auto-resume) | ☐ | ☐ | ☐ | ☐ | |
| 35 | PS during MOX with diversity active | ☐ | n/a | ☐ | n/a | |
| 36 | Layout switch at runtime (1 → 2v → 12h → 2x2 → 1) | ☐ | ☐ | ☐ | ☐ | |
| 37 | Floating pan window detach + dock-back | ☐ | ☐ | ☐ | ☐ | |
| 38 | Pan layout persistence across launches | ☐ | ☐ | ☐ | ☐ | |
| 39 | +PAN dropdown menu functional | ☐ | ☐ | ☐ | ☐ | |
| 40 | +RX button on overlay panel + cap-reject toast | ☐ | ☐ | ☐ | ☐ | |
| 41 | Right-click VFO flag context menu | ☐ | ☐ | ☐ | ☐ | |
| 42 | Antenna auto-switch toast with Undo | ☐ | n/a | ☐ | n/a | |
| 43 | TX-bound re-route confirmation dialog | ☐ | n/a | ☐ | n/a | |
| 44 | Bottom-bar CH state indicators update live | ☐ | ☐ | ☐ | ☐ | |
| 45 | Settings restore: TxBoundSliceIndex | ☐ | ☐ | ☐ | ☐ | |
| 46 | Settings restore: per-slice per-band sample rate | ☐ | ☐ | ☐ | ☐ | |
| 47 | Settings restore: diversity state + memories | ☐ | n/a | ☐ | n/a | |

Total: 47 rows × 4 SKUs (some N/A) = ~150 actual checks.

## How to use

1. Plug in target SKU, launch app
2. For each row, perform the action, observe expected outcome
3. Tick the box for that SKU column
4. Note any deviation in `<sku>-results.md`
5. If a row reveals a bug: open a GitHub issue, link from the deviation note

## Status legend

- ☐ Not tested
- ✅ Tested, behaves as designed
- ⚠️ Tested with deviation (link issue)
- ❌ Broken (link issue)
- n/a Not applicable on this SKU (capability-gated)
- deferred Documented gap; not blocking 3F ship
```

- [ ] **Step 2: Commit**

```bash
mkdir -p docs/architecture/2026-05-26-phase3f-verification
git add docs/architecture/2026-05-26-phase3f-verification/README.md
git commit -m "docs(arch): Phase 3F bench verification matrix template (47 rows × 4 SKUs)"
```

---

## Task 2: G2 bench run

**Files:** Create `docs/architecture/2026-05-26-phase3f-verification/g2-results.md`

- [ ] **Step 1: Connect to G2, run through the matrix**

Build a fresh release: `cmake --build build --config RelWithDebInfo`

For each of the 47 rows:
1. Perform the action (e.g. "Slice creation up to maxSlices=5"): create slices A through E via +PAN menu
2. Observe expected outcome (5 slices appear, 6th attempt shows reject toast)
3. Record result in `g2-results.md`

- [ ] **Step 2: For each ⚠️ or ❌ row, open a GitHub issue**

```bash
gh issue create --title "Phase 3F bench: G2 row XX <feature> deviates" --body "..."
```

- [ ] **Step 3: Commit results doc**

```bash
git add docs/architecture/2026-05-26-phase3f-verification/g2-results.md
git commit -m "docs(arch): Phase 3F G2 bench results"
```

---

## Task 3: HL2 bench run

**Files:** Create `docs/architecture/2026-05-26-phase3f-verification/hl2-results.md`

- [ ] **Step 1: Run matrix on HL2**

HL2 has maxSlices=1, so many rows are n/a. Focus on:
- Single-slice operation unchanged (rows 1, 3-5, 9, 19, 20-24, 36, 38-41, 44, 45-46)
- Wideband deferred (n/a per design)
- Diversity not supported (n/a)
- PS during MOX still works on single slice

- [ ] **Step 2: Commit**

```bash
git add docs/architecture/2026-05-26-phase3f-verification/hl2-results.md
git commit -m "docs(arch): Phase 3F HL2 bench results"
```

---

## Task 4: G2E + HermesII bench runs (when hardware available)

**Files:** Create remaining results docs

- [ ] **Step 1: G2E results** (pending v0.5.2 G2E hardware availability)

If not yet available, mark all G2E rows as "deferred" with note "Pending live G2E hardware per v0.5.2 status".

- [ ] **Step 2: HermesII results**

If available, similar to HL2 but with 2-slice cap.

---

## Task 5: Bug triage + fix pass

**Files:** Variable, depends on bench results

- [ ] **Step 1: For each bug surfaced in benches, file a fix branch**

```bash
git checkout -b fix/phase3f-bench-<short-description>
```

- [ ] **Step 2: Fix root cause (not symptom)**

Follow project memory `feedback_no_minimal_fixes_for_ports.md`: if the bug surfaces in code ported from Thetis, re-port the full upstream function, don't patch only the branch the user noticed.

- [ ] **Step 3: Add regression test for each fix**

- [ ] **Step 4: Open PR per fix branch**

```bash
gh pr create --title "fix(3f): <description>" --body "..."
```

- [ ] **Step 5: Update bench result rows from ⚠️/❌ → ✅ as fixes land**

---

## Task 6: Tooltip + status string polish pass

**Files:** Variable, every new widget added in Sub-Epics D-G

- [ ] **Step 1: Audit all new widgets for missing/wrong tooltips**

```bash
grep -rn "setToolTip" src/gui/widgets/ src/gui/dialogs/ src/gui/setup/HardwareDdcRoutingPage.* 2>/dev/null
```

For widgets with no tooltip set, add one. Source text from Thetis where applicable (verbatim per `feedback_thetis_attribution_rules.md`).

- [ ] **Step 2: Audit status bar messages and toast strings for consistency**

- [ ] **Step 3: Commit polish pass**

```bash
git commit -m "polish(3f): tooltip + status string pass"
```

---

## Task 7: CHANGELOG.md update

**Files:** Modify `CHANGELOG.md`

- [ ] **Step 1: Add Phase 3F section under unreleased / next-minor (likely v0.6.0)**

```markdown
## [Unreleased] / v0.6.0

### Phase 3F: Multi-Panadapter + Multi-Slice + Wideband + Full Diversity

#### Added

- **Multi-slice operation** up to per-SKU hardware cap (HL2=1, HermesII=2, Metis=3, Hermes=4, G2-class=5). Slices A through E with cyan/magenta/green/yellow color coding per AetherSDR convention.
- **5 pan layout templates**: Single, Stacked, Side-by-Side, Wide+2, Grid. Plus floating windows for multi-monitor.
- **AetherSDR overlay model**: any slice whose frequency falls within a pan's visible range overlays as a flag. Same-band slices share pan real estate.
- **TxSliceArbiter** enforces single-TX invariant with MOX-drop guard for RF-safe handoff. Click any slice's TX badge to transfer TX binding.
- **Per-slice sample rate**: operator-owned per-slice per-band. Full P2 ladder up to 1536 kHz on supported boards (G2, G2E, OrionMkII, Saturn, Andromeda, AnvelinaPro3, RedPitaya-P2). HL2 supports 384 kHz P1 via mi0bot extension.
- **Antenna-driven ADC chain assignment** on 2-ADC boards: slice's antenna preference auto-routes to the appropriate ADC chain.
- **Hybrid Alex filter policy**: auto-distribute → bypass-on-conflict → operator override. WIDE badge per pan on multi-band shared chains.
- **Antenna conflict policies**: Auto-switch (RX safe), Always ask, Auto-switch any. Toast notification with Undo for silent re-routes. Modal confirmation for TX-bound re-routes.
- **Wideband extended pan**: zoom any DDC pan out beyond its native 1.5 MHz to see wideband data fill the wings while a listenable I/Q island stays in the center. Click in wing retunes DDC; click in island retunes slice.
- **Full Thetis Diversity port** on 2-ADC SKUs: radar widget, direction finding, 8 memory slots per band, phase + gain + fine null sliders, 7 quick-nudge buttons, cross-fire, lock angle, auto-find null.
- **+PAN button** activated in bottom status bar with dropdown for add slice / change layout / float pan.
- **+RX button** on per-pan SpectrumOverlayPanel for one-click slice creation.
- **SpectrumStatusOverlay** widget in top-right of each pan: slice badge + freq + mode + CH tag + TX / WIDE / DIV / PS HOLD pills.
- **Right-click VFO flag** context menu: Make TX, Antenna ▶, Sample rate ▶, Move pan, Move DDC, Diversity ▶, Filter policy…, Remove.
- **Setup → Hardware → DDC Routing** page for power users (auto routing handles 99% of cases; this is the override panel).
- **Setup → Antenna Control → Conflict policy** group.
- **View menu**: Pan Layout… (Ctrl+L), Add slice on active pan (Ctrl+R), Float active pan.
- **Tools menu**: Diversity Dialog… (Ctrl+Shift+D, capability-gated to 2-ADC SKUs).
- **Per-chain bottom-bar indicators** (CH 0 / CH 1) showing live BPF state.

#### Changed

- `RadioModel::setSplit()` stub removed. AetherSDR-faithful: no VFO B / split concept. Use XIT (±10 kHz) for offsets within the slice; create a second slice for full-band TX retune.
- `BoardCapabilities` gains `maxSlices` (user-facing cap, distinct from existing `maxReceivers`/DDC count) and `widebandAdcs` fields. Schema v6 migration.
- `SliceModel` gains 7 new Q_PROPERTYs: sliceLetter, chainIndex, ddcIndex, sampleRateHz, diversityEnabled, widebandExtensionRequested, psPaused. Plus per-band diversity state + 8 memory slots for Slice A.
- `AlexController` gains per-ADC state machine (`AlexAdcState` struct, `BpfMode` enum, `recomputeBpf()`).
- `PanadapterStack` replaces single `SpectrumWidget` in MainWindow.

#### Fixed

- (List bench-discovered fixes here as they land)

#### Deferred to 3F-W follow-on

- HL2 wideband bandscope (P1 protocol mechanism differs from P2; needs separate research)
- HermesII PS+diversity combined (hardware limit — only 2 DDCs total)
```

- [ ] **Step 2: Commit**

```bash
git add CHANGELOG.md
git commit -m "docs(changelog): Phase 3F release notes (multi-slice + multi-pan + wideband + diversity)"
```

---

## Task 8: Design doc final retrospective

**Files:** Modify `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md`

- [ ] **Step 1: Append "Phase 3F shipped" section**

```markdown
---

## Phase 3F shipped (YYYY-MM-DD)

All 8 sub-epics implemented per the per-sub-epic plans. Released in v0.6.0.

### Sub-epic landing chronology

- Sub-Epic A (Foundation): commit XXX (YYYY-MM-DD)
- Sub-Epic B (Codec + Chain): commit XXX
- Sub-Epic C (TxSliceArbiter + lifecycle): commit XXX
- Sub-Epic D (Pan layouts + multi-pan UI): commit XXX
- Sub-Epic E (UI atlas surfaces): commit XXX
- Sub-Epic F (Wideband extended pan): commit XXX
- Sub-Epic G (Full Diversity port): commit XXX
- Sub-Epic H (Bench verification + polish): commit XXX

### Bench-verified SKUs

- ANAN-G2 (Saturn) — primary bench
- HermesLite 2 (HL2) — single-slice operation
- (G2E, HermesII as available)

### Documented gaps / 3F-W follow-on items

- HL2 wideband (P1 mechanism)
- HermesII PS+diversity combined

### Lessons learned / design refinements during implementation

(Fill in based on bench discoveries)
```

- [ ] **Step 2: Commit**

```bash
git add docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md
git commit -m "docs(arch): Phase 3F shipped — final retrospective"
```

---

## Task 9: Open Phase 3F release PR

**Files:** none modified

- [ ] **Step 1: Cut a release branch**

```bash
git checkout -b release/v0.6.0
git push -u origin release/v0.6.0
```

- [ ] **Step 2: Open umbrella PR or use the `/release` skill**

If this is meant to land in next minor release, follow the project's standard release process (likely via the `release` skill).

```bash
gh pr create --title "release: v0.6.0 (Phase 3F multi-slice + multi-pan)" --body "..."
```

---

## Sub-Epic H Completion Criteria

- 47-row bench verification matrix populated for G2 + HL2 (minimum)
- All ⚠️ / ❌ rows have GitHub issues filed and either fixed or documented as deferred
- Tooltip + status string polish pass complete
- CHANGELOG.md Phase 3F section drafted
- Design doc has shipping retrospective
- Release PR opened (or release cut via `/release` skill)

Phase 3F is **DONE** when:
- ✅ All 8 sub-epic plans have landed
- ✅ Bench verification on G2 + HL2 shows ≥90% rows passing
- ✅ Documented gaps are listed in CHANGELOG with their 3F-W follow-on tracking
- ✅ No P0 / P1 bugs open against Phase 3F surface area

---

## References

- All 7 preceding sub-epic plans (A through G)
- Design doc §13 (bench matrix structure)
- Project memory: `feedback_no_minimal_fixes_for_ports.md`, `feedback_failed_tests_never_ignored.md`, `feedback_verify_release_vs_main.md`
- Release skill: `~/.claude/plugins/cache/.../skills/release/`
