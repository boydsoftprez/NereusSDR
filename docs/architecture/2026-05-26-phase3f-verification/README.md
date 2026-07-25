# Phase 3F Bench Verification Matrix

Per-SKU per-feature verification matrix for the Phase 3F multi-pan + multi-slice epic. Covers Sub-Epics A through G as shipped to `feature/phase3f-sub-epic-a-foundation`.

## Status as of 2026-05-27 (Phase 3F build)

This matrix scaffold ships with the bench-verification plan; per-SKU results documents (`g2-results.md`, `hl2-results.md`, `g2e-results.md`, `hermesII-results.md`) get filled in as bench sessions happen (Sub-Epic H Tasks 2-4).

Important per-row caveats from what actually shipped vs the original plan assumptions:

- **Sub-Epic G shipped bench-MINIMUM only** (4 of 25 plan tasks landed). Rows 28-33 (radar widget, 8-memory slots, quick-nudge, Sync A-to-B, Link ATT, direction-finding) are marked `deferred` per the Sub-Epic G retrospective. What ships in G is: Enable + Phase slider + Gain slider via Tools > Diversity Dialog (Ctrl+Shift+D), wired to WDSP External Diversity on Slice A only via DivId 0.
- **Sub-Epic F shipped data-path only.** Rows 16-18 (wideband zoom + auto-bypass + click-in-wing) work data-side end-to-end (packet decode -> accumulator -> FFT -> bins -> SpectrumWidget) but visual rendering of wideband bins as background fill behind the DDC island with dashed boundary indicators is deferred. Marked with "data path live; visual polish post-bench" annotations.
- **Row 14 (3+ antenna conflict dialog):** `TxBoundConfirmDialog` widget shipped but the consumer wire-up (the call site that actually opens it on a 3+ antenna conflict event) is deferred. Marked "widget exists; consumer wire-up deferred".
- **Row 42 (Antenna auto-switch toast):** `AntennaSwitchToast` widget shipped but the upstream `RadioModel::antennaAutoSwitched` signal is pending. Marked `deferred`.

The per-SKU columns use empty `[ ]` checkboxes for testers to tick as rows pass. Avoid unicode box characters that may render oddly across viewers.

## Matrix

| # | Feature | G2 | HL2 | G2E | HermesII | Notes |
|---|---------|----|----|-----|----------|-------|
| 1 | Slice creation up to maxSlices | [ ] | [ ] | [ ] | [ ] | G2=5, HL2=1, G2E=5, HermesII=2 |
| 2 | Slice removal restores chain BPF correctly | [ ] | n/a (1-slice) | [ ] | [ ] | |
| 3 | Per-slice sample rate change - 48 kHz | [ ] | [ ] | [ ] | [ ] | |
| 4 | Per-slice sample rate change - 96 kHz | [ ] | [ ] | [ ] | [ ] | |
| 5 | Per-slice sample rate change - 192 kHz | [ ] | [ ] | [ ] | [ ] | |
| 6 | Per-slice sample rate change - 384 kHz | [ ] | [ ] | [ ] | n/a (P1 192 cap) | |
| 7 | Per-slice sample rate change - 768 kHz | [ ] | n/a | [ ] | n/a | |
| 8 | Per-slice sample rate change - 1536 kHz | [ ] | n/a | [ ] | n/a | |
| 9 | Slice band change (per-band memory load) | [ ] | [ ] | [ ] | [ ] | |
| 10 | AetherSDR overlay (same-band slices on one pan) | [ ] | n/a | [ ] | [ ] | |
| 11 | AetherSDR overlay (cross-band slices, flag migration) | [ ] | n/a | [ ] | [ ] | |
| 12 | Antenna routing - single-antenna | [ ] | [ ] | [ ] | [ ] | |
| 13 | Antenna routing - 2-antenna 2-ADC auto-distribute | [ ] | n/a (1-ADC) | [ ] | n/a (1-ADC) | |
| 14 | Antenna routing - 3+ antenna conflict dialog | [ ] | n/a | [ ] | n/a | widget exists; consumer wire-up deferred |
| 15 | TxSliceArbiter handoff with MOX-drop guard | [ ] | n/a | [ ] | [ ] | |
| 16 | Wideband extension via zoom-out gesture | [ ] | deferred (3F-W) | [ ] | deferred | data path live; visual polish post-bench |
| 17 | Wideband extension auto-bypasses Alex BPF | [ ] | deferred | [ ] | deferred | data path live; visual polish post-bench |
| 18 | Click-in-wing retunes DDC | [ ] | deferred | [ ] | deferred | data path live; visual polish post-bench |
| 19 | Click-in-listenable-island retunes slice | [ ] | [ ] | [ ] | [ ] | |
| 20 | Alex hybrid policy: same-band no badge | [ ] | [ ] | [ ] | [ ] | |
| 21 | Alex hybrid policy: multi-band auto-bypass + WIDE badge | [ ] | [ ] | [ ] | [ ] | |
| 22 | Alex hybrid policy: ForceBand override | [ ] | [ ] | [ ] | [ ] | |
| 23 | Alex hybrid policy: ForceBypass override | [ ] | [ ] | [ ] | [ ] | |
| 24 | Alex policy popup from WIDE badge click | [ ] | [ ] | [ ] | [ ] | |
| 25 | Alex policy popup from CH tag click | [ ] | n/a | [ ] | [ ] | |
| 26 | Diversity enable/disable on Slice A | [ ] | n/a | [ ] | n/a (1-ADC) | |
| 27 | Diversity phase + gain controls | [ ] | n/a | [ ] | n/a | |
| 28 | Diversity radar widget renders + updates | deferred | n/a | deferred | n/a | Sub-Epic G T5 (DiversityRadarWidget) deferred |
| 29 | Diversity 8 memory slots (store + recall) | deferred | n/a | deferred | n/a | Sub-Epic G T3 (8-memory) deferred |
| 30 | Diversity quick-nudge buttons (7 of them) | deferred | n/a | deferred | n/a | Sub-Epic G quick-nudge deferred |
| 31 | Diversity Sync Slice A -> Slice B | deferred | n/a | deferred | n/a | Sub-Epic G Sync A-to-B deferred |
| 32 | Diversity Link ATT | deferred | n/a | deferred | n/a | Sub-Epic G Link ATT deferred |
| 33 | Diversity direction-finding math (visual check) | deferred | n/a | deferred | n/a | Sub-Epic G T11 (direction finding) deferred |
| 34 | PS during MOX with multiple slices (pause + auto-resume) | [ ] | [ ] | [ ] | [ ] | |
| 35 | PS during MOX with diversity active | [ ] | n/a | [ ] | n/a | |
| 36 | Layout switch at runtime (1 -> 2v -> 12h -> 2x2 -> 1) | [ ] | [ ] | [ ] | [ ] | |
| 37 | Floating pan window detach + dock-back | [ ] | [ ] | [ ] | [ ] | |
| 38 | Pan layout persistence across launches | [ ] | [ ] | [ ] | [ ] | |
| 39 | +PAN dropdown menu functional | [ ] | [ ] | [ ] | [ ] | |
| 40 | +RX button on overlay panel + cap-reject toast | [ ] | [ ] | [ ] | [ ] | |
| 41 | Right-click VFO flag context menu | [ ] | [ ] | [ ] | [ ] | |
| 42 | Antenna auto-switch toast with Undo | deferred | n/a | deferred | n/a | widget exists; RadioModel::antennaAutoSwitched signal pending |
| 43 | TX-bound re-route confirmation dialog | [ ] | n/a | [ ] | n/a | widget exists; consumer wire-up deferred (see row 14) |
| 44 | Bottom-bar CH state indicators update live | [ ] | [ ] | [ ] | [ ] | |
| 45 | Settings restore: TxBoundSliceIndex | [ ] | [ ] | [ ] | [ ] | |
| 46 | Settings restore: per-slice per-band sample rate | [ ] | [ ] | [ ] | [ ] | |
| 47 | Settings restore: diversity state + memories | [ ] | n/a | [ ] | n/a | memories deferred per Sub-Epic G; diversity Enable/Phase/Gain do restore |
| 48 | Two same-band slices share one DDC (one active DDC reported) | [ ] | n/a (1-slice) | [ ] | [ ] | Sub-Epic I |
| 49 | Four slices (A-D) share one DDC on one band | [ ] | n/a | [ ] | n/a (2-slice cap) | Sub-Epic I |
| 50 | Co-hosted slices keep independent mode / filter / AGC / audio | [ ] | n/a | [ ] | [ ] | Sub-Epic I |
| 51 | Co-hosted slices share one noise blanker (expected, not a bug) | [ ] | n/a | [ ] | [ ] | per cmaster.h `_rcvr.panb`; UI greying deferred |
| 52 | Slice retuned out of window claims a second DDC | [ ] | n/a | [ ] | [ ] | Sub-Epic I |
| 53 | Sole-occupant slice retunes its own DDC rather than claiming one | [ ] | [ ] | [ ] | [ ] | CTUN off; CTUN on must pin the DDC instead |
| 54 | Widening a DDC rate re-admits an out-of-window slice | [ ] | [ ] | [ ] | n/a (192 cap) | Sub-Epic I Task 10 |
| 55 | Narrowing a DDC rate evicts a slice to its own DDC | [ ] | [ ] | [ ] | n/a | Sub-Epic I Task 10 |
| 56 | DDC exhaustion rejects with a message naming the limit | [ ] | [ ] | [ ] | [ ] | not a silent failure |
| 57 | All `userDdcCount` DDCs usable simultaneously | [ ] | n/a (1) | [ ] | [ ] | G2 = 5 |
| 58 | P1 rate change applies to every active receiver | n/a | [ ] | n/a | [ ] | P1 carries one rate in C&C bank 0 |
| 59 | Second pan animates independently of the first | [ ] | n/a | [ ] | [ ] | the headline Sub-Epic I fix |
| 60 | Hermes-class board keeps RX after the first VFO turn | n/a | n/a | [ ] | [ ] | G2E / ANAN-10E regression guard, Task 7c |

Total: 60 rows x 4 SKUs (with a generous count of `n/a` and `deferred`) = approximately 130 actual ticks expected across the matrix.

Rows 48-60 were added by Sub-Epic I (data-plane completion). Rows 1-15 became
testable for the first time with that sub-epic; before it, slices B+ were UI
objects with no receiver, no WDSP channel and no DDC. See
`docs/architecture/2026-07-24-phase3f-sub-epic-i-data-plane-plan.md`.

## How to use

1. Plug in target SKU, launch app
2. For each row, perform the action, observe expected outcome
3. Tick the box for that SKU column
4. Note any deviation in `<sku>-results.md`
5. If a row reveals a bug: open a GitHub issue, link from the deviation note

## Status legend

- `[ ]` Not tested
- `[x]` Tested, behaves as designed (or use `PASS`)
- `WARN` Tested with deviation (link issue)
- `FAIL` Broken (link issue)
- `n/a` Not applicable on this SKU (capability-gated)
- `deferred` Documented gap; not blocking 3F ship (see Status section above)
