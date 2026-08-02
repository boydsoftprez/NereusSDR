# Bottom Banner Cleanup + AetherSDR-Shaped Pan Menu: Design Spec

Status: **Approved** (JJ / KG4VCF, 2026-08-02)
Branch: `claude/bottom-banner-cleanup-10b4fe`
Supersedes the right-strip drop-priority section of
`2026-04-30-shell-chrome-redesign-design.md` §278.5.

Interactive mockups (private artifacts):

* Bottom banner, with a live fold ladder:
  <https://claude.ai/code/artifact/cf8d811f-25e6-4f20-bd68-9f7c962f9dc9>
* Pan menu, with per-board gating:
  <https://claude.ai/code/artifact/66da69c0-35ec-4bfc-ba19-6a7b19d808cd>

---

## 1. Goal

Two related pieces of shell chrome, both reached from the bottom banner.

**Part A.** The bottom banner overlaps itself at every window width an
operator actually uses, and the machinery meant to prevent that oscillates
instead. Fix the width budget and replace three competing responsive systems
with one.

**Part B.** The `+PAN` button opens a text-only `QMenu` that lists layout ids
as bare strings (`12h`, `2x2`). AetherSDR opens a painted thumbnail grid.
Port that shape, gate it on our own per-board slice capacity, and move the two
per-pan actions out of it.

## 2. Non-goals

* No change to what any control *does*. This is placement, width, and
  affordance only, apart from the slice-binding defect in §4.2 and the
  per-pan routing fix in §8.4.
* No new DSP, no protocol change, no change to `maxSlices` values.
* No skin or theme work. Existing `StyleConstants` palette throughout.
* The six inert placeholder items (Band Stack, TNF, CWX, DVK, FDX, and the
  CAT tile) are explicitly **retained**. Their removal was proposed and
  rejected.

---

## 3. Evidence

### 3.1 The banner does not fit

`buildStatusBar()` (`src/gui/MainWindow.cpp:6064`) packs 33 widgets into a
single `QHBoxLayout` with two stretch spacers. Summing natural widths gives a
required width of roughly **1740 px** with no alarms showing.

The development and bench machine is a 3024 x 1964 Retina display, which is
**1512 logical points**. The banner is therefore about 228 px over budget
when maximised, in the ordinary case, on the machine it is tested on. This is
not a narrow-window edge case; it is the everyday state, and it is the
observed overlap.

### 3.2 Three responsive systems, each with a hysteresis patch

| System | Location | Patch it needed |
| --- | --- | --- |
| `RxDashboard` three-stage ladder | `src/gui/widgets/RxDashboard.h:76` | `m_lastDecisionBudget` / `m_settled` deadband |
| Right-strip drop priority | `src/gui/MainWindow.cpp:7749` | 30 px `kDeadbandPx` gate |
| Qt's own squeeze | left section, no policy at all | none; this is the overlap |

Both existing patches carry comments describing the loop they suppress.
`RxDashboard.h:76` records the wide-to-medium boundary alternating until the
user saw "both BadgePair orientations painted alternately".

The shared root cause: each system **measures, mutates itself, then
measures again**, so its own output feeds its next input. Hysteresis damps
that; it does not remove it.

The left section is the one with no policy, which is why the visible failure
appears there. Once the two stretch spacers collapse, `QLabel` text runs past
its box and paints over its neighbour.

### 3.3 Safety indicators move when they matter most

`AdcOverloadBadge` is inserted between the PA badge and the TX badge
(`MainWindow.cpp:6621`) and made visible on overload. Its appearance widens
the run, so the TX indicator shifts sideways **at the moment an alarm
fires**. The same applies to `TX INHIBIT` at `:6578`.

### 3.4 Reachability audit

Every banner item was audited against the menu bar to establish what an
operator can still reach if the item is not on screen.

| Item | Alternate surface |
| --- | --- |
| TNF | DSP > TNF (`MainWindow.cpp:5459`) |
| CWX | Tools > CWX… (`:5922`) |
| Band Stack | Band > Band Stacking… (`:5650`) |
| CAT / TCI | Tools > CAT Control… (`:5933`) / TCI Server… (`:5939`) |
| RX pills | DSP menu NR / NB / ANF / SNB / APF / BIN / AGC (`:5343`-`:5471`), plus the VFO flag |
| CH chain tags | `SpectrumStatusOverlay`, per pan |
| Clock | OS menu bar |
| StationBlock | Radio > Connect / Disconnect / Manage Radios (`:5024`-`:5091`) |
| `+PAN` | **none.** Add-slice and float-pan exist only in its own popup |
| `☰` | **none.** No menu action toggles the container panel |
| TX INHIBIT / PA / ADC OVL / TX | **none.** No menu, no dialog, no applet |

This audit is the source of the fold order in §6. It is deliberately
evidence-based rather than a judgement about which readings feel important.

---

## 4. Part A decisions

### 4.1 Merge radio identity into StationBlock

Radio identity currently renders twice: a stacked model-over-firmware pair in
the left section (`MainWindow.cpp:6225`), and the radio name in the centred
`StationBlock`. The left pair has no click affordance and sits in the
unprotected section, so its width changes are what shove its neighbours.

`StationBlock` gains a second row:

```
┌─────────────────┐
│    Nereus G2    │   radio name (existing)
│  ANAN-G2 · v27  │   model · firmware (new)
└─────────────────┘
```

The left-section pair and its trailing separator are deleted.
`m_connStatusLabel`'s legacy alias to `m_radioModelLabel` (`:6236`) must be
repointed or retired.

`StationBlock` stays between two `flex:1` spacers, so it remains the centred
anchor per layout rule §278.4 and its growth is symmetric.

**Saving: about 110 px.**

### 4.2 RX pills stay, densified, and bound to the active slice

The `RxDashboard` pills are **retained**. A proposal to evacuate them as
per-slice state was rejected.

Two changes:

**Densify.** Each of the seven values currently wears its own bordered pill,
which is a large amount of chrome for a three-character string. Render as a
single row without per-pill borders:

```
[A]  USB 2.7k │ MED │ NR2 NB2 APF SQL
```

No reading is dropped. **268 px becomes 186 px.**

**Bind to the active slice.** This is a correctness fix, not a layout one.
`RxDashboard` is bound at `MainWindow.cpp:2832` on `sliceAdded` only when
`sliceId == 0`, and is never rebound. Since Phase 3F multi-pan landed
(PR #312), an operator working Slice B sees Slice A's mode, filter, AGC and
noise reduction presented as current. The dashboard must follow the active
slice, and a cyan slice-letter tag (`A`, `B`, `C`, `D`) is prepended so the
reading is unambiguous about which slice it describes.

**Net: about 56 px saved**, after the 26 px slice tag.

### 4.3 Merge PA telemetry and CPU into one tile

The PA tile (`MainWindow.cpp:6404`) is already a two-row vertical stack, but
the rows are mutually exclusive in practice: row 1 (PA volts) fills only on
MKII-class boards, row 2 (PA temp) only on HL2. On any given radio one row is
always empty.

CPU moves into that row:

```
┌──────────────┐
│ PA   13.8V   │  volts on MKII, temp on HL2, hidden if the board has neither
│ CPU  19%     │  always
└──────────────┘
```

Rules:

* A board publishing **both** volts and temp puts both in row 1
  (`PA 13.8V 42°C`) rather than evicting CPU.
* A board publishing **neither** shows a CPU-only tile, not an empty one.
* The °C / °F click toggle and its `isPaTempToggle` event filter stay bound
  to whichever row is carrying temperature.
* The G2E `PSU` relabel path (`:6536`) is unaffected.
* The tile folds as one unit. Shedding the CPU row alone saves no width,
  because `PA 13.8V` is the wider row.

**Saving: about 100 px** (160 px of two tiles plus a separator becomes 60 px).

### 4.4 Relocate the UTC clock to the TitleBar

The clock moves to the TitleBar right side as a **single row of UTC**. The
date and local-time row is dropped.

Rationale is placement before pixels. Every desktop OS puts time top-right,
so bottom-right costs a cross-window eye movement, and more importantly it
squats in the one corner that should belong to alarms alone. Date is already
known; local time is duplicated in the menu bar. UTC is the one fact the OS
clock does not give an operator for logging.

`TitleBar` has slack between the app name and `MasterOutputWidget`
(`AetherSDR-derived layout, TitleBar.cpp:404-491` in NereusSDR). A single-row
UTC label in a fixed-width monospace face contributes zero layout variance.

**Saving: 130 px from the banner.**

### 4.5 Reserve the safety slots

The four safety indicators move into a dedicated right-hand group with
**permanently allocated fixed-width slots**:

```
│ [INH] [ PA ] [OVL] [ TX ]
```

Each slot is always present at a fixed 50 px. Only the badge *inside* it
changes appearance. An inactive slot renders its badge at low opacity rather
than collapsing.

This is the fix for §3.3: an alarm now lights up in a pixel the operator has
already learned, and nothing else on the bar moves when it does.

The group is separated from the rest of the strip by a 1 px `#203040` rule,
which is the section separator that
`2026-04-30-shell-chrome-redesign-design.md` §278.1 specified and that the
implementation never used (it used dot separators uniformly instead).

**Cost: about 26 px.** This is the one item in Part A that grows.

### 4.6 Width ledger

| | Today | Agreed |
| --- | ---: | ---: |
| RX state pills | 268 | 212 |
| Radio identity | 278 | 168 |
| PA telemetry + CPU | 160 | 60 |
| UTC clock | 130 | 0 |
| Safety corner | 174 | 200 |
| Everything else, unchanged | 730 | 646 |
| **Total** | **1740** | **1286** |

At 1512 logical points, the agreed banner has roughly 226 px of slack and
folds nothing.

---

## 5. Part A architecture: one layout authority

### 5.1 The rule

**Banner layout is a pure function of banner width.** One width always
produces exactly one visible set. This is what makes flicker impossible
rather than merely damped, and it is the property the current code cannot
satisfy.

Three supporting invariants make it hold:

1. **Nothing shrinks.** Every banner item is present at its natural width or
   absent. No item is permitted to compress below `sizeHint()`. This removes
   the overlap failure mode in §3.2 outright.
2. **Widths are cached once.** Natural widths are measured at construction
   (and on content change, which is explicit and rare) into a static table.
   The fold decision reads that table, never a live `sizeHint()` that the
   previous fold step just changed.
3. **Single pass.** On resize: walk the ladder from step 1, subtract each
   step's cost, stop as soon as the running total fits. No re-measure between
   steps, no second pass, no feedback.

### 5.2 What gets deleted

* `RxDashboard::reapplyDropPriority()` and its `m_lastDecisionBudget` /
  `m_settled` / `m_inReapplyDropPriority` state. The dashboard becomes a
  fixed-width dense row that the authority folds as a unit.
* `MainWindow::reapplyRightStripDropPriority()` and its `kDeadbandPx`
  hysteresis, `m_rightStripLastBudget`, `m_rightStripSettled`, and the
  `force=true` call sites at `:6473`, `:6642`, `:6686`, `:7127`.

`OverflowChip` is **retained** and becomes the authority's single output
surface for folded items.

### 5.3 New code

| Unit | Responsibility |
| --- | --- |
| `ChromeBarController` | Owns the item table and the ladder. One public entry point: `relayout(int barWidthPx)`. Pure and side-effect-free apart from `setVisible` calls. |
| `ChromeBarItem` | `{ QWidget* widget; QWidget* separator; int naturalWidth; int rung; QString overflowLabel; }` |
| `SystemTile` | The merged PA + CPU tile from §4.3. |

`ChromeBarController` is deliberately separable from `MainWindow` so the
ladder is unit-testable without constructing a GUI (see §9).

---

## 6. Part A fold ladder

Ordered by §3.4 reachability: an item may fold in proportion to how easily an
operator reaches it elsewhere.

| Rung | Folds | Reachable via |
| ---: | --- | --- |
| 1 | System tile (PA + CPU) | PA applet; informational |
| 2 | TGXL chip | TunerApplet |
| 3 | CAT + TCI, together | Tools menu, both |
| 4 | CH chain tags | `SpectrumStatusOverlay`, per pan and correct |
| 5 | RX pills, **one at a time**, right to left: SQL, then APF, then NB, then NR, then AGC | DSP menu and the VFO flag |
| 6 | Placeholder row: FDX, DVK, CWX, TNF, Band Stack | collapses to one `⋯ 5` chip |
| : | **Never folds:** `+PAN`, `☰`, StationBlock, the four safety slots, and the mode + filter pills | nothing else reaches them |

Notes:

* Rungs 5 and 6 mean live RX state yields before inert placeholders. That is
  a deliberate consequence of the "left buttons are last resort" call, and it
  is acceptable because §4.6 puts the fold point well below any width in
  normal use. The ladder is insurance, not everyday behaviour.
* Rung 5 is five sub-steps, not one. Each pill folds individually so the
  ladder degrades smoothly rather than dropping 90 px at a stroke. The slice
  tag stays with mode and filter and never folds.
* Rung 6 collapses the placeholders to a single chip rather than removing
  them, so the group keeps a position.
* CAT and TCI fold as a pair to avoid a "TCI but no CAT" half-state, matching
  the existing grouping at `MainWindow.cpp:7789`.
* Every folded item's label and current value go into `OverflowChip`'s
  popover.

**Floor.** With all six rungs folded, the banner requires about 746 px, so it
survives any window the rest of the UI does.

---

## 7. Part A degenerate cases

| Case | Behaviour |
| --- | --- |
| Board with no PA telemetry (Atlas, Hermes, Angelia, Orion) | System tile shows CPU only |
| Board publishing volts and temp | Both in row 1; CPU keeps row 2 |
| Single-ADC SKU | `CH 1` absent as today, gated on `rxFilterChainCount >= 2` |
| Disconnected | StationBlock shows its dashed "Click to connect" placeholder; RX pills empty; safety slots stay allocated and dim |
| No slices | Slice tag hidden, pills empty, dashboard keeps its slot |
| PS not armed | `PsaIndicatorWidget` hidden as today, per `updatePsaIndicatorVisibility()` |

`QStatusBar::showMessage` must continue not to be used. The existing warning
at `MainWindow.cpp:6810` stays; notices go through `showToast()`.

---

## 8. Part B: the pan menu

### 8.1 The button becomes an icon

AetherSDR paints a 36 x 28 pixmap: a jagged spectrum polyline with a plus
sign in the upper right (`MainWindow.cpp:4368-4396 [@c6481cb]`). The text
`+PAN` pill is replaced by that icon, drawn from the same point list.

An icon reads as a control; a text pill reads as a label. The spectrum trace
also says what kind of thing is being added.

### 8.2 The menu becomes a thumbnail grid

`showPanMenu()` (`MainWindow.cpp:8714`) is replaced by opening the existing
`PanLayoutDialog`, extended to AetherSDR's shape
(`PanLayoutDialog.cpp [@c6481cb]`):

* Three-column grid of layout tiles.
* Each tile paints the **actual cell geometry** at 120 x 90, with lettered
  pans (`A`..`E`), so `A / B|C` is recognised rather than decoded.
* Current layout highlighted: `#00607a` fill, 2 px `#00b4d8` border.
* Caption under each tile: `A / B|C` plus the pan count.
* Hover: `rgba(0,180,216,0.12)` fill with an accent border.

The dialog is **gated on connection**, matching
`MainWindow_Shortcuts.cpp:663 [@c6481cb]`. With no radio there is no slice
capacity to gate against, and applying a layout would build pans that cannot
receive slices.

AetherSDR's gate is a silent early return, which reads as a dead click. Ours
renders the icon dim while disconnected so the unavailability is visible
before the click, and the tooltip says "Connect a radio to change pan
layout". No dialog, no toast.

### 8.3 Nine layouts, and why not twelve

AetherSDR ships twelve layouts up to eight pans. Three of those are
unreachable for us.

`BoardCapabilities` caps `maxSlices` at **five on every supported board**:

| Board | maxReceivers | maxSlices | userDdcCount |
| --- | ---: | ---: | ---: |
| Saturn / ANAN-G2 / G2-1K / Andromeda | 7 | 5 | 5 |
| Angelia / Orion / OrionMkII / 7000D / 8000D / AnvelinaPro3 | 7 | 5 | 5 |
| HermesC10 / ANAN-G2E | 4 | 5 | 4 |
| Hermes Lite 2 | 4 | 5 | 2 |
| Hermes | 4 | 4 | 4 |
| Atlas / Metis | 3 | 3 | 3 |
| HermesII | 4 | 2 | 2 |

This is a **client-side allocation limit, not a hardware one.** The gateware
does eight receivers: `n1gp-Anvelina_PROIII Orion.v:958 [@8e86a61]` sets
`localparam NR = 8`, and the changelog at `Orion.v:632 [@8e86a61]` records
"NR=8 8 separate bands in CW SkimServ and SparkSDR @192K OK". The comment at
`Orion.v:956-957` notes the fabric fits fourteen and only the bootloader's
2 MB file cap holds it to ten. Revisiting `maxSlices` is out of scope here
and would be its own design.

AetherSDR's `2x3` (6 pans), `4h3` (7) and `2x4` (8) are therefore dropped.
Shipping them would mean a grid where a third of the tiles can never light
up on any radio we support.

The nine that ship, with implementation status:

| Id | Shape | Pans | `PanadapterStack` today |
| --- | --- | ---: | --- |
| `1` | Single | 1 | implemented |
| `2v` | A / B | 2 | implemented |
| `2h` | A \| B | 2 | implemented |
| `12h` | A / B\|C | 3 | implemented (`PanadapterStack.cpp:140`) |
| `2h1` | A\|B / C | 3 | **new** |
| `3v` | A / B / C | 3 | **new** |
| `2x2` | A\|B / C\|D | 4 | implemented (`PanadapterStack.cpp:158`) |
| `4v` | A/B/C/D | 4 | **new** |
| `3h2` | A\|B\|C / D\|E | 5 | **new** |

Four new layouts land in `PanadapterStack::applyLayout` alongside the
existing `12h` and `2x2` branches. `3h2` is the only one that exercises the
full five-slice cap, and is therefore the highest-value bench case for
DDC-allocation behaviour.

### 8.4 Hide, do not grey, and say why

AetherSDR greys an out-of-capacity tile and gives no reason, because it is
relaying a capacity number from a radio API it does not own.

NereusSDR **hides** unavailable tiles and prints a single footer line naming
what was hidden and why:

> `Hermes II allots 2 slices. 6 layouts need a radio with more.`

Rationale. A tile that can never be clicked is noise, and greying nine tiles
down to three on a Hermes II makes the dialog look broken. But silently
vanishing options are their own failure when an operator has read about a
layout elsewhere, so the footer converts "this app does not have that" into
"this radio does not have that". We can write that sentence because we
allocate DDCs locally; this is the deliberate divergence from AetherSDR's
shape, and it is the "port the shape, enhance the mechanics" rule applied.

Accepted cost: the grid reflows per radio, so tile positions are not stable
across radios on a multi-radio bench.

### 8.5 Per-pan actions leave the button

`showPanMenu()` carries two actions AetherSDR's does not, and both resolve
through `m_panStack->activePanId()`:

* Add slice on active pan (`MainWindow.cpp:8733`)
* Float active pan (`MainWindow.cpp:8769`)

Both move to the pan's own right-click menu, where they act on **that** pan:

```
Add slice on this pan
Float this pan
```

This removes the last `activePanId()` routing from the `+PAN` affordance and
leaves it doing exactly one job. It also brings the two actions in line with
the standing rule that a control drawn on a pan targets that pan.

---

## 9. Testing

### 9.1 Unit, no GUI and no radio

`ChromeBarController` takes a width table and returns a visible set, so the
whole ladder is testable as a pure function.

1. **Monotonicity.** Sweep 640 to 2560 px in 1 px steps. Assert the visible
   set only ever shrinks as width decreases. Any item that reappears at a
   narrower width is a ladder bug.
2. **No oscillation.** For every width, assert `relayout(w)` twice yields an
   identical visible set, and that `relayout(w)` after `relayout(w±1)` yields
   the same set as `relayout(w)` cold. This is the property the current code
   cannot pass and is the regression gate for this work.
3. **Ladder order.** Assert each rung folds before the next, and that the
   never-fold set is present at every width down to the 746 px floor.
4. **Alarm invariance.** Assert the four safety slots occupy identical
   geometry with alarms off and on, at every width.
5. **Board matrix.** For each `BoardCapabilities` row, assert the layout grid
   shows exactly the layouts with `panCount <= maxSlices` and the footer
   count matches the hidden count.

### 9.2 Bench

1. G2 maximised at 1512: nothing folded, no overlap anywhere on the strip.
2. Drag the window narrow slowly. Rungs fire in order, no flicker at any
   boundary, `OverflowChip` lists what went.
3. Trigger an ADC overload during TX. TX badge does not move.
4. HL2: system tile shows `PA T`, not `PA`; °C / °F toggle still works.
5. Multi-pan: activate Slice B, confirm the pills follow and the slice tag
   reads `B`.
6. Layout grid on G2, HL2, and a 2-slice SKU if one is on the bench.
   Verify hidden counts and footer text.
7. Apply each of the four new layouts; confirm slices land and DDCs allocate.

Per `docs/development/fast-test-loop.md`, iterate with `ctest -R` on the new
targets; the full suite runs once at the end.

---

## 10. Attribution

Pre-port checklist, per CLAUDE.md Ring 1.

**Upstream sources.** AetherSDR at `v26.6.1-512-gc6481cbf`, short `@c6481cb`:

* `src/gui/MainWindow.cpp:4368-4417`: the `+PAN` icon pixmap
* `src/gui/MainWindow_Shortcuts.cpp:662-688`: click handler and connection gate
* `src/gui/PanLayoutDialog.cpp`: the thumbnail grid, `LayoutThumbnail`, tile styling

Plus, for a fact-only citation, `n1gp-Anvelina_PROIII Orion.v:958,632,956-957
[@8e86a61]` in §8.3. That is PROVENANCE kind `reference`, not `port`; no
gateware logic is translated.

**NereusSDR files touched.** `src/gui/PanLayoutDialog.{h,cpp}`,
`src/gui/MainWindow.cpp`, `src/gui/PanadapterStack.{h,cpp}`,
`src/gui/PanadapterApplet.{h,cpp}`, `src/gui/TitleBar.{h,cpp}`, plus new
`ChromeBarController` and `SystemTile`.

**Provenance status.** `PanLayoutDialog.{h,cpp}` is already registered at
`docs/attribution/aethersdr-contributor-index.md:270` and carries a
port-citation header citing `@0cd4559`. `MainWindow.cpp` is registered at
`docs/attribution/aethersdr-reconciliation.md:1155`. **Neither is a new
attribution event.**

**Required actions.**

1. Bump the `PanLayoutDialog` cite stamp from `@0cd4559` to `@c6481cb` in the
   file header, since this re-ports from newer upstream.
2. Add an inline `// From AetherSDR MainWindow.cpp:4368 [@c6481cb]` cite at
   the icon-painting site, and equivalents at the grid and connection-gate
   sites.
3. Update the `aethersdr-contributor-index.md:270` row from "(pending Phase
   3F)" to the shipped state.
4. HOW-TO-PORT.md rule 6 applies: AetherSDR carries no per-file headers, so
   project-level attribution only. There is no verbatim block to copy and
   none may be fabricated.
5. New NereusSDR-original files (`ChromeBarController`, `SystemTile`) get the
   `no-port-check: NereusSDR-original` marker.

---

## 11. Decision log

| # | Decision | Note |
| ---: | --- | --- |
| 1 | Six inert placeholders retained | removal proposed, rejected |
| 2 | Model + firmware fold into StationBlock | |
| 3 | RX pills retained | evacuation proposed, rejected |
| 4 | Pills densified and slice-tagged, bound to active slice | |
| 5 | CH chain tags retained | |
| 6 | PA and CPU merge into one tile | |
| 7 | UTC clock moves to TitleBar, single row | |
| 8 | Safety slots reserved, fixed width | |
| 9 | No target width; graceful folding | |
| 10 | Left buttons fold last | this derived the whole ladder |
| 11 | Ladder as written in §6 | |
| 12 | Pan grid: all nine reachable layouts, four to be built | |
| 13 | Hide unavailable tiles, with a footer line | diverges from AetherSDR |
| 14 | Add-slice and float move to per-pan right-click | |
| 15 | Grid gated on connection | follows AetherSDR |

## 12. Open

* `maxSlices` is five everywhere while the gateware does eight receivers.
  Worth its own look, out of scope here.
* Tile ordering puts Single first. AetherSDR puts it last; that ordering was
  not carried over.
* Whether the layout grid should be a modal dialog or a popup anchored under
  the button. Modal is specified, matching AetherSDR.
