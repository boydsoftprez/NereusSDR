# Phase 3M-3a-ii Follow-up — Full `ucParametricEq` Widget Port (CFC + EQ Dialog Parity)

**Status:** Hand-off design brief — _no implementation has started_.
**Audience:** A future Claude session whose first job is to brainstorm,
design, and then write a `superpowers:writing-plans`-grade implementation
plan that another (or the same) session can later execute via
`superpowers:subagent-driven-development`.
**Author:** Claude Opus 4.7 (1M ctx) drafting on JJ's instruction
2026-04-30.

---

## 1. Why this exists

3M-3a-ii (`feature/phase3m-3-tx-processing`, PR #TBD) shipped CFC + CPDR +
CESSB + Phase Rotator under the radio plumbing, and gave the user a working
`TxCfcDialog`. But the dialog landed **scalar-complete and spartan** — a
profile combo, two global spinboxes, and a 10×3 spinbox grid for
F/COMP/POST-EQ. JJ's verdict, verbatim:

> "the UI inteface landed without much grace. it is a very frugal and
> spartan interpratation of what thetis has we need to run a pass over that
> and get it to true parity of thetis with all the sliders and the
> representation of what the cfc is doing."

In Thetis, both the CFC config dialog (`frmCFCConfig.cs`, 609 + 814 lines
designer) and the TX EQ config dialog (`eqform.cs`, 3626 lines) wrap a
single, shared, very rich `UserControl`:

> `Project Files/Source/Console/ucParametricEq.cs` — **3396 lines**, sole
> author Richard Samphire MW0LGE, GPLv2 + Samphire dual-licensed.

Until we port `ucParametricEq` natively to Qt6, **both** dialogs in
NereusSDR are second-class. The CFC dialog visually omits the per-band Q
sliders and the live curve overlay; the EQ dialog (`TxEqDialog`,
`src/gui/applets/TxEqDialog.{h,cpp}`) omits the same visual handles and
filter response curve. So this work delivers **two improvements for one
porting effort.**

It is also the natural foundation for any future TX-side "what does my
chain sound like" visualizer (CFC compression curves, ALC/Lev/CPDR
multi-stage envelopes), because all of those layer onto the same
band-handle drag interaction model.

---

## 2. Source-of-truth files

All paths relative to `../Thetis/Project Files/Source/Console/`:

| File | Lines | Role |
| --- | ---: | --- |
| `ucParametricEq.cs` | 3396 | The control itself — drag, paint, JSON state, public API |
| `frmCFCConfig.cs` | 609 | CFC dialog wrapping ucParametricEq + per-band CFC controls |
| `frmCFCConfig.Designer.cs` | 814 | Layout for above (read this for spinbox arrangement / labels) |
| `eqform.cs` | 3626 | EQ dialog (TX + RX EQ) wrapping ucParametricEq |
| (no `eqform.Designer.cs`) | — | EQ form layout is hand-rolled in the .cs |

Capture the Thetis version stamp at the moment of porting:

```sh
git -C ../Thetis describe --tags          # release tag, e.g. v2.10.3.13
git -C ../Thetis rev-parse --short HEAD   # for between-release commits
```

Every inline `// From Thetis ucParametricEq.cs:N [vX.Y.Z.W]` cite must
carry that stamp; CI's `verify-inline-tag-preservation.py` will fire if
any author tag (`//MW0LGE`, `//[2.10.3.13]MW0LGE`, etc.) is dropped.

GPL header rules (see `CLAUDE.md` §License-preservation rule + §Byte-for-byte
headers): copy the entire header block from `ucParametricEq.cs:1-40` —
including the **Samphire dual-licensing statement**, since the file is
100% Samphire-authored — into the Qt port file's header verbatim.

---

## 3. What `ucParametricEq` actually does

Read these structural waypoints in `ucParametricEq.cs` to internalize the
control before designing the port:

| Construct | Purpose |
| --- | --- |
| `class EqPoint` (line 54) | One band: `_band_id`, `_band_color`, `_frequency_hz`, `_gain_db`, `_q`, `_band_type` (peak/low-shelf/high-shelf/lpf/hpf), `_active` (bypass) |
| `class EqDraggingEventArgs` (line 107) | Live-drag event payload — fired during mouse drag |
| `class EqPointDataChangedEventArgs` (line 122) | Fired on any point property change (commit) |
| `class EqPointSelectionChangedEventArgs` (line 177) | Fired when the user clicks/tabs to a different band |
| `class EqJsonState` / `EqJsonPoint` (lines 220, 242) | Newtonsoft.Json round-trip blob (this is what NereusSDR's `cfcParaEqData` opaque blob is for) |
| `ucParametricEq()` ctor (line 360) | Default 5-band peak EQ at 100 / 250 / 800 / 2400 / 6000 Hz |
| `OnPaint` (line 1575) | Paints background grid, curve, handles, labels |
| `drawCurve` (line 2344) | Bilinear-transform biquad summation across freq axis (this is the math you must port) |
| `OnMouseDown` (line 1625) | Handle hit-test → start drag |
| `OnMouseMove` (line 1677) | Live drag → updates point + emits `Dragging` event |

Public API the dialogs depend on (as the Qt port API surface):

- `Points` — `ObservableCollection<EqPoint>` (add/remove/select)
- `SelectedPoint` — currently focused band
- `PlotMinDb` / `PlotMaxDb` / `PlotMinFreqHz` / `PlotMaxFreqHz` — axis range
- `LogFrequencyAxis` — bool, default true
- `JsonState` — get/set the full state blob (this round-trips via the
  `cfcParaEqData` field on `TransmitModel` already)
- Events: `PointDataChanged`, `Dragging`, `PointSelectionChanged`,
  `PointAdded`, `PointRemoved`

---

## 4. Where it slots into NereusSDR

| Thetis | NereusSDR |
| --- | --- |
| `ucParametricEq` (UserControl) | `ParametricEqWidget` — new `QWidget` subclass under `src/gui/widgets/` |
| `frmCFCConfig` (Form) | **Existing** `TxCfcDialog` (`src/gui/applets/TxCfcDialog.{h,cpp}`) — refactor, do not replace |
| `eqform` TX-EQ tab | **Existing** `TxEqDialog` (`src/gui/applets/TxEqDialog.{h,cpp}`) — refactor, do not replace |
| `eqform` RX-EQ tab | RX EQ is wired separately (`EqApplet`) — RX-side parity is **out of scope** for this PR; flag as future work |

`ParametricEqWidget` is a new file under `src/gui/widgets/` (not
`src/gui/applets/`) because it's a reusable widget, not an applet. Other
widgets in that directory (`ColorSwatchButton`, `HGauge`) already follow
this pattern.

The state plumbing is **already in place**:

- `TransmitModel::cfcParaEqData` (QByteArray) — opaque JSON blob, ready
- `TransmitModel::txEqParaEqData` (… needs check — see §8)
- `MicProfileManager` already round-trips `cfcParaEqData` since 3M-3a-ii
  Batch 4.5 (commit `66ff196`)

So this port is mostly **rendering + interaction**, not plumbing.

---

## 5. Hard constraints (non-negotiable)

These are the rules every line of this port must obey. They come from
project-wide policies in `CLAUDE.md` / `CONTRIBUTING.md` and from
session-level memories.

1. **Source-first.** Read the Thetis source before writing any logic.
   State `"Porting from ucParametricEq.cs:N — original C# logic:"` and
   quote/summarize before writing the C++.
2. **Byte-for-byte header.** `ucParametricEq.cs:1-40` (full GPL block +
   Samphire dual-licensing block) goes into the Qt port file's header
   verbatim.
3. **Inline tag preservation.** Every `//MW0LGE`, `//[2.10.3.13]MW0LGE`
   (or whatever appears) within ±5 source lines of a ported region must
   land within ±10 lines of the C++ port. CI enforces this.
4. **Cite versioning.** Every `// From Thetis ucParametricEq.cs:N` cite
   needs `[v2.10.3.13]` or `[@sha]` stamp.
5. **Constants verbatim.** Default 5-band frequencies, default Q,
   default plot ranges, color palette, biquad coefficients, freq/gain
   axis log/lin scaling — all sourced from the C# code, not invented.
6. **Atomic params for cross-thread DSP.** The widget itself is
   main-thread Qt; the DSP read of the resulting CFC band arrays is
   audio-thread. The existing `RadioModel::pushTxProcessingChain` lambda
   already covers this — do not introduce a mutex on the audio path.
7. **AppSettings + per-MAC persistence.** No `QSettings`. `cfcParaEqData`
   is already persisted via `MicProfileManager` bundles; widget-only
   prefs (e.g. axis ranges if exposed) go via `AppSettings` with
   PascalCase keys + `"True"`/`"False"` boolean strings.
8. **GPG-signed commits.** Never `--no-verify` / `--no-gpg-sign`.
9. **Survey UI before adding controls.** Per the
   `feedback_survey_before_adding_controls` memory: before adding any
   new spinbox / slider / button, grep `src/gui/` for existing widgets
   that serve the same purpose; wire to existing surfaces if found.
10. **Diff-style code in the design + implementation discussion.** Per
    `feedback_diff_style_code` — show diffs or full small functions, not
    large unchanged blocks.

---

## 6. The session contract

The future planning session should produce, in order:

1. **Brainstorm** (`superpowers:brainstorming` skill). Surface tradeoffs
   in interaction model: native QPainter+QWidget vs. QGraphicsView+items
   vs. QML. Discuss whether the curve renders on the CPU (cheap, every
   paint event) or via a small QRhi compute pass (overkill — reject
   unless the curve becomes a bottleneck on a 4K display).
2. **Spike or no spike.** Decide if a 1-day spike on
   QPainter-vs-QGraphicsView is needed before committing. JJ's posture
   is "objectively understand what you are doing" (per
   `feedback_options_with_recommendation`), so default to a brief
   evaluation rather than guessing.
3. **Plan** (`superpowers:writing-plans` skill). Output a plan file at
   `docs/architecture/phase3m-3a-ii-followup-parametriceq-plan.md`
   broken into batches that fit subagent-driven development. Suggested
   coarse partition (NOT prescriptive — the planner should refine):

   | Batch | Scope | Files |
   | --- | --- | --- |
   | 1 | `ParametricEqWidget` skeleton + EqPoint + JsonState | new `src/gui/widgets/ParametricEqWidget.{h,cpp}` |
   | 2 | Axis math (log freq, dB scale, hit-test, screen↔data mapping) | same |
   | 3 | OnPaint port — grid + axis labels + handle painting | same |
   | 4 | drawCurve port — biquad summation across freq axis | same |
   | 5 | Mouse interaction — drag freq/gain, scroll Q, double-click bypass | same |
   | 6 | Public Qt API + signals + JSON round-trip | same |
   | 7 | TxCfcDialog refactor — drop the spinbox grid, embed widget, add per-band Q sliders | `src/gui/applets/TxCfcDialog.{h,cpp}` |
   | 8 | TxEqDialog refactor — embed widget on TX EQ tab | `src/gui/applets/TxEqDialog.{h,cpp}` |
   | 9 | Tests — `tst_parametric_eq_widget` (axis math + JSON round-trip + signal emission) | new test file |

4. **Execute** via `superpowers:subagent-driven-development` — fresh
   subagent per batch + spec compliance review + code quality review,
   exactly as 3M-3a-ii ran.

---

## 7. Open design questions for the planner to resolve

These are explicitly **NOT** decided. The planning session must
brainstorm + recommend + cite Thetis source.

1. **Curve math:** does Thetis actually compute the biquad sum across N
   freq bins on every paint, or does it cache and only invalidate on
   point-data-change? Read `drawCurve` (line 2344) carefully and decide.
2. **Per-band Q control:** in Thetis CFC dialog, where does the Q slider
   live — on the widget itself (right-click popup? drag direction?) or in
   the surrounding form? Read `frmCFCConfig.Designer.cs` to find out.
3. **Handle drag axes:** does Thetis lock to freq-only on shift,
   gain-only on ctrl, or is it free 2D drag? Read `OnMouseMove` (1677).
4. **Snap-to behavior:** does the freq-handle snap to ISO 1/3-octave
   boundaries? Round to nearest 10 Hz? Free-form? Source-first.
5. **Color palette:** the 10 band colors NereusSDR will use — port
   verbatim from `ucParametricEq.cs` `EqPoint._band_color` defaults; do
   not pick from the NereusSDR `StyleConstants.h` palette (this is a
   Thetis-faithful port, not a re-skin).
6. **Add/remove band:** Thetis lets the user add/remove bands; the CFC
   spec is fixed at 10 bands (`cfcomp.c` per-band arrays size 10). Does
   the CFC dialog hide add/remove? Or limit the count? Read both.
7. **JSON schema compatibility:** the `cfcParaEqData` blob ports through
   `MicProfileManager` and into Thetis profile imports (someday). Decide
   whether to write a Newtonsoft-compatible JSON or our own and document
   trade-off.
8. **EQ dialog tab structure:** Thetis `eqform` has TX EQ + RX EQ + a
   "general" tab. NereusSDR currently has only `TxEqDialog`. Does the
   refactor add an RX EQ tab? Or stay TX-only and flag RX as future
   work? **Strong recommendation:** stay TX-only this PR; RX EQ is a
   separate sub-PR (the existing `EqApplet` covers RX EQ scalar
   controls).

---

## 8. Pre-flight checks the planner must run

Before writing the plan, the future session should do:

```sh
# 1. Confirm cfcParaEqData / txEqParaEqData fields exist on TransmitModel:
grep -n "cfcParaEqData\|txEqParaEqData\|paraEqData" src/models/TransmitModel.{h,cpp}

# 2. Confirm MicProfileManager round-trips both blobs:
grep -n "cfcParaEqData\|txEqParaEqData\|paraEqData" src/core/MicProfileManager.{h,cpp}

# 3. Verify TxCfcDialog / TxEqDialog signal hookups won't be broken:
grep -n "openCfcDialog\|cfcDialogRequested\|openTxEqDialog\|txEqDialogRequested" src/

# 4. Confirm Thetis source files are present:
ls "../Thetis/Project Files/Source/Console/ucParametricEq.cs" \
   "../Thetis/Project Files/Source/Console/frmCFCConfig."* \
   "../Thetis/Project Files/Source/Console/eqform.cs"

# 5. Run the WDSP-PROVENANCE / THETIS-PROVENANCE registration check (the
#    new ParametricEqWidget will introduce a new Thetis attribution event):
grep -l "ParametricEqWidget\|ucParametricEq" docs/attribution/THETIS-PROVENANCE.md
```

If (5) returns nothing, the port is a **new attribution event** and
`docs/attribution/HOW-TO-PORT.md` walkthrough applies — add the
PROVENANCE row in the same commit that introduces the file.

---

## 9. Risks the planner should call out

| Risk | Mitigation |
| --- | --- |
| **3396 lines is a lot of code to port** | Subagent-driven development with batch boundaries that fit a single subagent context (~1500 lines per batch max) — the partition in §6 is sized for that |
| **drawCurve biquad math regression** | Add a `tst_parametric_eq_widget` test that pins curve sample points at fixed freq/gain/Q against a hand-computed expected (from Octave/numpy) — independent of Thetis, so CI catches drift |
| **QPainter performance on 4K** | Profile in spike. If problematic, switch to QRhiWidget — but reject this preemptively without measurement |
| **Visual divergence from Thetis** | Side-by-side bench with Thetis running on Windows VM; capture screenshots into the PR |
| **JSON breaks `MicProfileManager` round-trip** | Round-trip test must run **before** subagents touch dialog refactor — i.e. Batch 1 includes the round-trip assertion |
| **Surveying-existing-controls violation** | Per the §5.9 constraint — the spinbox grid in `TxCfcDialog` must be **removed**, not duplicated. Right-click popups for per-band Q, not new sliders, unless `frmCFCConfig.Designer.cs` shows otherwise |

---

## 10. Out of scope (do not let this expand)

- RX EQ dialog (`EqApplet`-side) — separate sub-PR
- ALC/Lev curve overlay — separate future work
- TX-chain visualizer (multi-stage envelope) — separate future work
- Re-skinning to NereusSDR palette — verbatim port; aesthetic tweaks
  later if JJ asks
- Adding new DSP parameters — only port what `ucParametricEq` exposes

---

## 11. Definition of done

1. `src/gui/widgets/ParametricEqWidget.{h,cpp}` exists with full GPL +
   Samphire dual-license header byte-for-byte from `ucParametricEq.cs:1-40`.
2. `THETIS-PROVENANCE.md` row added; CI's `check-new-ports.py` green.
3. `tst_parametric_eq_widget` covers axis math + drawCurve sample points
   + JSON round-trip + signal emission.
4. `TxCfcDialog` refactored — spinbox grid removed; widget embedded;
   per-band Q control wired (location TBD by §7.2).
5. `TxEqDialog` refactored — sliders/spinbox grid removed; widget
   embedded.
6. Side-by-side screenshots captured (NereusSDR vs Thetis on a Windows
   VM) and attached to the PR.
7. JJ's bench-test sign-off on ANAN-G2: drag a CFC band, save profile,
   reload profile, confirm curve restores; same for TX EQ.
8. All commits GPG-signed.
9. CI green: lint + ctest + provenance + inline-tag verifiers.
10. CHANGELOG.md `[Unreleased]` entry under `Phase 3M-3a-ii Follow-up`
    or whatever sub-phase tag applies.

---

## 12. Hand-off prompt for the next session

> "I'm starting the design + planning session for the
> `ucParametricEq` widget Qt6 port. Read
> `docs/architecture/phase3m-3a-ii-cfc-eq-parametriceq-handoff.md` first
> — it's the brief I left for you. Then:
>
> 1. Run §8 pre-flight checks.
> 2. Read `ucParametricEq.cs` end-to-end (3396 lines — budget for it).
> 3. Skim `frmCFCConfig.Designer.cs` (814) and `eqform.cs` (3626) only
>    enough to answer §7.2 / §7.3 / §7.4 / §7.6 / §7.8.
> 4. Brainstorm with me on the open questions in §7.
> 5. Then write the plan at
>    `docs/architecture/phase3m-3a-ii-followup-parametriceq-plan.md`
>    using `superpowers:writing-plans`.
> 6. Hand off to the implementation session via
>    `superpowers:subagent-driven-development`.
>
> Hard constraints in §5 are non-negotiable. I'm KG4VCF / J.J. Boyd, and
> my standing prefs (GPG signing, no-redundant-callsign, plain English
> discussion, options-with-recommendation, etc.) are in
> `~/.claude/projects/-Users-j-j-boyd/memory/MEMORY.md`. Re-read the
> memories before we start."
