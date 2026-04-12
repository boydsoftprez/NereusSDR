# Phase 3G-8 — RX1 Display Parity Plan

> **Status:** Open questions resolved 2026-04-12. Ready to execute commit 1.
> **Branch:** `feature/phase3g8-rx1-display-parity` (off `feature/phase3g7-polish`)
> **Drafted:** 2026-04-12
> **Predecessor:** Phase 3G-7 polish (shipped 2026-04-12, items A/B/C, items D/E/F deferred)
> **Successor:** Phase 3I-1 Basic SSB TX

## 1. Goal

Bring NereusSDR's `Setup → Display` category to feature parity with Thetis for **RX1 only**, before SSB TX work begins. Today every control on Spectrum Defaults, Waterfall Defaults, and Grid & Scales is `setEnabled(false)` with an "NYI" tooltip — this phase wires every one of them through to `AppSettings`, `SpectrumWidget`, `FFTEngine`, and the per-band grid storage, and adds ~20 missing Thetis Display fields plus 4 NereusSDR-original extensions.

RX2 Display, TX Display, and the spectrum overlay panel are explicitly **out of scope** and stay as-is until later phases.

## 2. Locked decisions (from interview, 2026-04-12)

| # | Decision | Choice |
|---|---|---|
| 1 | Wire depth | Tier (b) — persist + live-apply + missing renderer features implemented |
| 2 | Color picker | Build reusable `ColorSwatchButton` widget (lives in `src/gui/`) |
| 3 | Phase shape | One monolithic block, single branch, single PR |
| 4 | Thetis fidelity | Per-control citation with `file:line` references |
| 5 | Verification | Per-control before/after screenshot matrix (~48 pairs) |
| 6 | Per-band grid | Full Thetis parity, 14 bands × 3 values, BandStackManager wiring |
| 7 | NereusSDR-original controls | Keep all 4 (FFT Window, Peak Hold, Reverse Scroll, Timestamp), implement renderers |
| 8 | Missing Thetis fields | Add all ~20 in this phase |
| 9 | Defaults | Keep current NereusSDR defaults; new fields use Thetis defaults; divergence is intentional, **one-off exception** authorized by user 2026-04-12, documented in §10. Source-first protocol in CLAUDE.md remains the standing rule for all other work. |

## 3. Inventory — Spectrum Defaults page

### 3.1 Existing NereusSDR controls (10) — Thetis-cited, ranges to keep

| # | Control | Type | NereusSDR range/default | Thetis equivalent | Thetis location |
|---|---|---|---|---|---|
| S1 | FFT Size | combo (5 options) | 1024/2048/4096/8192/16384, default 4096 | `tbDisplayFFTSize` (TrackBar 0–6, value 5 → 4096×2⁵=131072) | `setup.designer.cs:35043`, handler `setup.cs:16142`, target `console.specRX.GetSpecRX(0).FFTSize` |
| S2 | FFT Window | combo (4 options) | Blackman-Harris/Hann/Hamming/Flat-Top | **NereusSDR extension** — Thetis has window in DSP tab (`comboDSPRxWindow setup.cs:2424`), not Display | n/a (extension) |
| S3 | FPS | slider | 10–60, default 30 | `udDisplayFPS` (1–144, default 60) | `setup.designer.cs:33834`, target `console.DisplayFPS` `setup.cs:7717` |
| S4 | Averaging | combo (3 options) | None/Weighted/Logarithmic | `comboDispPanAveraging` (None/Recursive/Time Window/Log Recursive) | `setup.designer.cs:34835`, target `console.specRX.GetSpecRX(0).AverageMode` `setup.cs:18055` |
| S5 | Fill toggle | checkbox | unchecked default | `chkDisplayPanFill` | `setup.designer.cs:33740` |
| S6 | Fill Alpha | slider | 0–100, default 30 | `tbDataFillAlpha` (0–255, default 128) | `setup.designer.cs:52733`, used in `display.cs:2145` `DataFillColor` alpha |
| S7 | Line Width | slider | 1–3, default 1 | `udDisplayLineWidth` (0.0–5.0 dec, default 1.0) | `setup.designer.cs:52886`, used `display.cs:10318` |
| S8 | Cal Offset | double spinbox | -30.0 to 30.0 dBm, default 0.0 | `Display.RX1DisplayCalOffset` (float dB) — real Thetis field, no setup-tab control but exposed programmatically | `display.cs:1372-1380` |
| S9 | Peak Hold | checkbox | unchecked | **NereusSDR extension** — Thetis has multimeter peak hold only (`udDisplayMultiPeakHoldTime`), no spectrum peak hold | n/a (extension) |
| S10 | Peak Hold Delay | spinbox | 100–10000 ms, default 2000 | **NereusSDR extension** (paired with S9) | n/a (extension) |

### 3.2 Missing Thetis Spectrum fields to ADD (7)

| # | Add | Type | Thetis name | Source | Default |
|---|---|---|---|---|---|
| S11 | Data Line Color | ColorSwatchButton | `clrbtnDataLine` | `display.cs:2186 DataLineColor` | Thetis default (TBD from designer) |
| S12 | Data Line Alpha | slider 0–255 | `tbDataLineAlpha` | `setup.designer.cs:52721`, default 128 | 128 |
| S13 | Data Fill Color | ColorSwatchButton | `clrbtnDataFill` | `display.cs:2145 DataFillColor` | Thetis default |
| S14 | Gradient Enabled | checkbox | `chkDataLineGradient` | `setup.designer.cs:3308` | unchecked |
| S15 | Spectrum Averaging Time | spinbox ms | `udDisplayAVGTime` | `setup.cs:2334`, used in `console.specRX.GetSpecRX(0).AvTau` | Thetis default |
| S16 | Spectrum Decimation | spinbox | `udDisplayDecimation` | `setup.designer.cs:2040` | Thetis default |
| S17 | Display Thread Priority | combo | `comboDisplayThreadPriority` | `setup.designer.cs:2008` | Normal |

**Total Spectrum Defaults page after phase: 17 controls** (10 existing + 7 added).

## 4. Inventory — Waterfall Defaults page

### 4.1 Existing NereusSDR controls (9)

| # | Control | NereusSDR range/default | Thetis equivalent | Thetis location |
|---|---|---|---|---|
| W1 | High Threshold | -200 to 0, default -40 | `udDisplayWaterfallHighLevel` (-200–0, default -80) | `setup.designer.cs:34237`, `Display.WaterfallHighThreshold display.cs:2523` |
| W2 | Low Threshold | -200 to 0, default -130 | `udDisplayWaterfallLowLevel` (-200–0, default -130) | `setup.designer.cs:34197`, `Display.WaterfallLowThreshold display.cs:2537` |
| W3 | AGC | unchecked | `chkRX1WaterfallAGC` (default checked) | `setup.designer.cs:34058` |
| W4 | Update Period | 10–500 ms, default 50 | `udDisplayWaterfallUpdatePeriod` (1–1000 ms, default 2) | `setup.designer.cs:34123` |
| W5 | Reverse Scroll | unchecked | **NereusSDR extension** | n/a |
| W6 | Opacity | 0–100, default 100 | `tbRX1WaterfallOpacity` (0–100, default 100) | `setup.designer.cs:34013`, handler `setup.cs:2374` |
| W7 | Color Scheme | combo (3: Enhanced/Grayscale/Spectrum) | `comboColorPalette` (7 schemes) | `setup.designer.cs:34094`, handler `setup.cs:11898` |
| W8 | Timestamp Position | combo (None/Left/Right) | **NereusSDR extension** | n/a |
| W9 | Timestamp Mode | combo (UTC/Local) | **NereusSDR extension** | n/a |

### 4.2 Missing Thetis Waterfall fields to ADD (8)

| # | Add | Type | Thetis name | Source | Default |
|---|---|---|---|---|---|
| W10 | Waterfall Low Color | ColorSwatchButton | `clrbtnWaterfallLow` | `display.cs:2513`, default `Color.Black` | Black |
| W11 | Show RX Filter on Waterfall | checkbox | `chkShowRXFilterOnWaterfall` | `setup.cs:1048 Display.ShowRXFilterOnWaterfall` | Thetis default |
| W12 | Show TX Filter on RX Waterfall | checkbox | `chkShowTXFilterOnRXWaterfall` | `setup.cs:1052` | Thetis default |
| W13 | Show RX Zero Line on Waterfall | checkbox | `chkShowRXZeroLineOnWaterfall` | `setup.cs:1050` | Thetis default |
| W14 | Show TX Zero Line on Waterfall | checkbox | `chkShowTXZeroLineOnWaterfall` | `setup.cs:1051` | Thetis default |
| W15 | Waterfall Use Spectrum Min/Max | checkbox | `chkWaterfallUseRX1SpectrumMinMax` | `setup.cs:7801 console.WaterfallUseRX1SpectrumMinMax` | unchecked |
| W16 | Waterfall Averaging | combo | `comboDispWFAveraging` | `setup.designer.cs:34428`, target `AverageModeWF` `setup.cs:18069` | Log Recursive |
| W17 | Waterfall Color Schemes (expand 3→7) | (expansion of W7) | adds Spectran/LinLog/LinRad/LinAuto/Custom — `Default/Enhanced/Spectran/BlackWhite` already exist in `WfColorScheme enum src/gui/SpectrumWidget.h:32` | `setup.cs:11904-11939 ColorScheme enum` | Enhanced |

**Total Waterfall Defaults page after phase: 17 controls** (9 existing + 8 added; W17 is an in-place expansion of W7's combo).

## 5. Inventory — Grid & Scales page

### 5.1 Existing NereusSDR controls (8)

| # | Control | NereusSDR range/default | Thetis equivalent | Thetis location |
|---|---|---|---|---|
| G1 | Show Grid | checked | `chkGridControl` (unchecked default) | `setup.designer.cs:52816` |
| G2 | dB Max | -200 to 0, default -20 | `udDisplayGridMax` (-200–0, default -40) **per-band: 14 separate** | `setup.designer.cs:34723`, `Display.SpectrumGridMax display.cs:1744` |
| G3 | dB Min | -200 to 0, default -160 | `udDisplayGridMin` (default -140) **per-band** | `setup.designer.cs:34692`, `display.cs:1755` |
| G4 | dB Step | 1–40, default 10 | `udDisplayGridStep` (default 2) **per-band** | `setup.designer.cs:34661`, `display.cs:1766` |
| G5 | Freq Label Align | combo (Left/Center) | `comboDisplayLabelAlign` (5: Left/Cntr/Right/Auto/Off) | `setup.designer.cs:34635`, `display.cs:2614 DisplayLabelAlignment` |
| G6 | Band Edge Color | dead swatch | `clrbtnBandEdge` (default `Color.Red`) | `display.cs:1941 BandEdgeColor` |
| G7 | Show Zero Line | unchecked | `chkShowZeroLine` (unchecked default) | `setup.designer.cs:52804` |
| G8 | Show FPS Overlay | unchecked | `chkShowFPS` (checked default) | `setup.designer.cs:2009`, `Display.ShowFPS setup.cs:19282` |

### 5.2 Missing Thetis Grid fields to ADD (5)

| # | Add | Type | Thetis name | Source | Default |
|---|---|---|---|---|---|
| G9 | Grid Color | ColorSwatchButton | `clrbtnGrid` | `setup.cs:1040 Display.GridColor` | Thetis default |
| G10 | Grid Fine (Dark) Color | ColorSwatchButton | `clrbtnGridFine` | `setup.cs:1041 Display.GridPenDark` | Thetis default |
| G11 | Horizontal Grid Color | ColorSwatchButton | `clrbtnHGridColor` | `setup.cs:1042 Display.HGridColor` | Thetis default |
| G12 | Grid Text Color | ColorSwatchButton | `clrbtnText` | `setup.cs:1044 Display.GridTextColor` | Thetis default |
| G13 | Zero Line Color | ColorSwatchButton | `clrbtnZeroLine` | `setup.cs:1043 Display.GridZeroColor` (default `Color.Red`) `display.cs:2037` | Red |

**Total Grid & Scales page after phase: 13 controls** (8 existing + 5 added).

### 5.3 Per-band grid expansion (G2/G3 only)

Thetis stores `DisplayGridMax<band>m` and `DisplayGridMin<band>m` for each of: 160m, 80m, 60m, 40m, 30m, 20m, 17m, 15m, 12m, 10m, 6m, WWV, GEN, XVTR — **14 bands × 2 values = 28 per-band values**. Grid **Step** is NOT per-band in Thetis; it's a single global value (`udDisplayGridStep`). Verified `console.cs:14242–14436` — no `DisplayGridStep<band>m` field exists. NereusSDR currently has one global pair.

**Thetis per-band defaults are uniform:** every band defaults to `Max = -40.0F, Min = -140.0F` (`console.cs:14242–14436`). Thetis stores per-band so users can *customize* per band; it does not ship hand-tuned per-band values.

**Architectural correction (2026-04-12).** Pre-flight investigation of commit 2 found two gaps between the original plan wording and the actual codebase:

- **Wrong model home.** The original plan said "per-band grid on `SliceModel`". Reality: NereusSDR's grid scale (`dBmFloor`, `dBmCeiling`) already lives on **`PanadapterModel`**, not `SliceModel`. `SliceModel` tracks VFO state only (frequency/mode/filter/AGC/gains/antennas) and has no grid properties. Per-band storage therefore goes on `PanadapterModel` to preserve single-source-of-truth for the dBm range.
- **No `Band` type exists.** No `Band` enum, no `frequency→band` lookup, no `bandChanged` signal existed anywhere in the codebase. Commit 2 introduces them.
- **UI band count is 12, not 14.** `BandButtonItem::kBandCount = 12` (160m–6m + GEN) omits WWV and XVTR. Per user decision 2026-04-12, commit 2 expands `BandButtonItem` 12 → 14 alongside the model work so the band enum and the UI stay in sync. `bandClicked(int)` is re-emitted by `ContainerWidget` but not consumed at top level, so the two new indices are purely additive and cause no downstream regressions.

**Implementation:**

1. New `src/models/Band.h`: `enum class Band { Band160m, Band80m, Band60m, Band40m, Band30m, Band20m, Band17m, Band15m, Band12m, Band10m, Band6m, GEN, WWV, XVTR }`. Static helpers `bandLabel(Band) → QString`, `bandKeyName(Band) → QString` (for AppSettings keys), `bandFromFrequency(double hz) → Band` (Thetis-sourced range table), `bandFromUiIndex(int)` / `uiIndexFromBand(Band)` for the 14-button UI order.
2. Add `BandGridSettings` struct to `PanadapterModel.h`: `{int dbMax; int dbMin;}` per band (Step stays global).
3. Add `QHash<Band, BandGridSettings> m_perBandGrid` to `PanadapterModel`.
4. Persistence keys: `"DisplayGridMax_160m"`, `"DisplayGridMin_160m"`, …, `"DisplayGridMax_XVTR"`, `"DisplayGridMin_XVTR"` (**28 per-band keys** + 1 global `DisplayGridStep`). Loaded in `PanadapterModel` constructor, written on every `setPerBandGrid*()`.
5. Add `band()`/`setBand(Band)` + `bandChanged(Band)` signal on `PanadapterModel`. `setCenterFrequency()` auto-derives band via `bandFromFrequency()` and calls `setBand()` on boundary crossing. Direct `setBand()` is also callable by UI (commit 8).
6. On `bandChanged`, `PanadapterModel` looks up the band's stored `BandGridSettings` and updates `m_dBmFloor`/`m_dBmCeiling`, emitting `levelChanged()` so existing `SpectrumWidget` wiring reacts with no new connections needed.
7. Expand `BandButtonItem::kBandCount` 12 → 14; add `"WWV"` and `"XVTR"` to `kBandLabels`.
8. Grid & Scales page (commit 8, not commit 2): dbMax/dbMin controls show "Editing band: 20m" label above them and read/write the currently active band's slot. dbStep stays a single global control with no band label.
9. **Default initialization (resolved per Q4):** `PanadapterModel` constructor initializes all 14 slots to Thetis uniform defaults `Max = -40, Min = -140`. Existing users will see the grid shift on first launch after upgrade; this is the authorized one-off divergence under decision #9's §10 exception scope, extended to cover per-band grid initial values (user decision 2026-04-12).

**Files affected:**
- `src/models/Band.h` (new) — enum, labels, frequency lookup, UI index mapping
- `src/models/Band.cpp` (new, if helpers need a translation unit) — lookup tables
- `src/models/PanadapterModel.h/.cpp` — `BandGridSettings` struct, per-band hash, `band`/`setBand`/`bandChanged`, constructor default init + AppSettings load, setCenterFrequency auto-derive
- `src/gui/meters/BandButtonItem.h/.cpp` — `kBandCount` 12 → 14, `kBandLabels` += `"WWV"`, `"XVTR"`
- `src/core/AppSettings.cpp` — no registration step needed (NereusSDR AppSettings uses free-form string keys)
- `src/gui/setup/DisplaySetupPages.cpp` — deferred to commit 8
- `src/gui/SpectrumWidget.cpp` — no change needed (existing `levelChanged` wiring reacts automatically)
- `CMakeLists.txt` — add `src/models/Band.cpp` if created

## 6. New widget — `ColorSwatchButton`

Lives at `src/gui/ColorSwatchButton.h/.cpp`. Reusable across this phase, TX Display, meter editors (replaces the dead `makeColorSwatch` helper in `DisplaySetupPages.cpp:54`), and future skin editor work.

**Spec:**

```cpp
class ColorSwatchButton : public QPushButton {
    Q_OBJECT
public:
    explicit ColorSwatchButton(const QColor& initial, QWidget* parent = nullptr);
    QColor color() const;
    void setColor(const QColor& c);  // emits colorChanged
signals:
    void colorChanged(const QColor& c);
private slots:
    void openPicker();  // QColorDialog::getColor with ShowAlphaChannel
private:
    QColor m_color;
    void updateSwatchStyle();  // sets button background to current color
};
```

**Persistence helper** (file-local in `ColorSwatchButton.cpp` or in `AppSettings`):
- `static QString colorToHex(const QColor&)` → `"#RRGGBBAA"`
- `static QColor colorFromHex(const QString&)`

**Used by:** S11 Data Line Color, S13 Data Fill Color, W10 Waterfall Low Color, G6 Band Edge Color, G9–G13 Grid colors. **9 instances** in this phase.

## 7. Renderer additions required

Tier (b) means every control does something visible. The following renderer features don't exist today and must be added:

| Need | Where | Backing existing code? |
|---|---|---|
| Spectrum averaging modes (None/Weighted/Logarithmic) | `FFTEngine` or `SpectrumWidget::updateSpectrum()` | Partial — `m_smoothed` exists |
| Spectrum peak hold + decay | `SpectrumWidget` (new `m_peakHold` array, decay timer) | None — new |
| Spectrum trace fill | `SpectrumWidget` paint path | None — new |
| Trace line width | `SpectrumWidget` pen width | Trivial |
| Trace gradient | `SpectrumWidget` paint path | None — new |
| Display calibration offset (dBm) | `SpectrumWidget::setDbmRange` adjustment | Trivial — additive |
| Waterfall AGC | `SpectrumWidget::pushWaterfallRow` (auto level) | None — new |
| Waterfall reverse scroll | `SpectrumWidget` waterfall texture write order | Small |
| Waterfall opacity | QRhi shader uniform or QPainter alpha | Trivial |
| Waterfall timestamp overlay | New `QPainter` text layer in `paintEvent` | None — new |
| Color scheme expansion (3→7) | `wfSchemeStops()` in `SpectrumWidget.cpp` — add LinLog/LinRad/LinAuto/Custom gradient stops | Existing function, add cases |
| FFT window switching | `FFTEngine` — store window choice, regenerate window coefficients | None — new (FFTEngine currently uses one window) |
| Show RX/TX filter on waterfall | `SpectrumWidget` waterfall paint — vertical filter band overlay | None — new |
| Show RX/TX zero line on waterfall | Vertical line at VFO frequency on waterfall | None — new |
| Grid/grid-fine/horizontal-grid colors live-applied | `SpectrumWidget::paintEvent` grid drawing | Currently hardcoded |
| Zero line color | `SpectrumWidget::paintEvent` zero line | Currently hardcoded |
| Grid text color | `SpectrumWidget::paintEvent` axis labels | Currently hardcoded |
| Frequency label alignment (5 modes) | `SpectrumWidget::paintEvent` frequency scale rendering | Existing, expand from 2→5 modes |
| FPS overlay text | `SpectrumWidget::paintEvent` corner overlay | None — new |

**Estimated renderer LOC additions: 600–900 lines across `SpectrumWidget.cpp` and `FFTEngine.cpp`.**

## 8. Verification matrix

For each control listed in §3, §4, §5, capture **two screenshots**: dialog open with control at default, and dialog open after changing the control + clicking Apply, with the spectrum/waterfall visibly reflecting the change.

| # | Control | Action | Expected visible change |
|---|---|---|---|
| S1 | FFT Size | 4096 → 16384 | Spectrum trace finer resolution, more bins per pixel |
| S2 | FFT Window | Blackman-Harris → Flat-Top | Spectrum sidelobes change shape |
| S3 | FPS | 30 → 60 | Spectrum frame rate visibly doubles |
| S4 | Averaging | None → Logarithmic | Spectrum trace smooths over time |
| S5 | Fill toggle | off → on | Area under trace fills |
| S6 | Fill Alpha | 30 → 80 | Fill opacity increases |
| ... | (all 47 controls) | ... | ... |
| G13 | Zero Line Color | Red → Yellow | Zero dB line color changes |

Captured screenshots go in `docs/architecture/phase3g8-verification/` and are referenced from the PR description. Total: ~94 screenshots (47 before + 47 after).

## 9. Commit shape (single monolithic block)

Per the locked decision, this is one branch with one PR. Commits within the branch are still small and focused — the "monolithic" refers to scope grouping, not commit count.

1. `feat(gui): ColorSwatchButton reusable color picker widget` — widget + unit-style smoke test
2. `feat(models): per-band grid settings on SliceModel` — struct, hash, signal, persistence keys
3. `feat(spectrum): renderer additions (averaging/peak/fill/gradient/trace cal)` — Spectrum Defaults backing
4. `feat(spectrum): renderer additions (waterfall AGC/reverse/timestamp/opacity/filter overlay)` — Waterfall backing
5. `feat(spectrum): renderer additions (color schemes expansion + grid/text colors + label align + FPS overlay)` — Grid & Scales backing
6. `feat(setup): wire Spectrum Defaults page to model + renderer (17 controls)`
7. `feat(setup): wire Waterfall Defaults page to model + renderer (17 controls)`
8. `feat(setup): wire Grid & Scales page to model + per-band grid (13 controls)`
9. `docs(3G-8): verification screenshots + CHANGELOG entry`

**~9 GPG-signed commits.** If any single commit grows past ~400 lines of diff, split it.

## 10. Documented divergences from Thetis

Per locked decision #9, NereusSDR keeps its current default values rather than adopting Thetis's. The plan records each divergence so future readers understand it's intentional.

| Control | NereusSDR | Thetis | Reason |
|---|---|---|---|
| FPS default | 30 | 60 | NereusSDR target hardware varies; 30 is conservative |
| FPS max | 60 | 144 | 144 FPS on a spectrum is wasteful |
| Update Period default | 50 ms | 2 ms | 2 ms is unnecessarily aggressive for the waterfall |
| Update Period min | 10 ms | 1 ms | Same |
| Grid Max default (global, pre-3G-8) | -20 | -40 | NereusSDR shows hotter signals better at -20. **Superseded** once per-band storage lands — see row below. |
| Grid Min default (global, pre-3G-8) | -160 | -140 | More headroom for weak signals. **Superseded** — see row below. |
| Grid Step default | 10 | 2 | Less visual clutter. Step remains a single global value in both Thetis and NereusSDR. |
| Waterfall High default | -40 | -80 | Matches NereusSDR's grid max better |
| Color Schemes count | 3 (now expanding to 7) | 7 | Now matches |
| FFT Size widget | combo | TrackBar | Combo is friendlier than a 7-position slider |
| Grid Max/Min initial per-band values | -40 / -140 (Thetis uniform) | -40 / -140 (Thetis uniform) | **New divergence from §10 row above:** per Q4 resolution, new per-band slots initialize to Thetis defaults, not NereusSDR -20/-160. Existing users will see grid shift on first launch. Authorized 2026-04-12. |
| Peak Hold (S9/S10) | exists | n/a | NereusSDR extension |
| Reverse Scroll (W5) | exists | n/a | NereusSDR extension |
| Timestamp (W8/W9) | exists | n/a | NereusSDR extension |
| FFT Window (S2) | Display tab | DSP tab | NereusSDR keeps it on Display for ergonomics |

**Standing rule preserved.** **Do NOT update CLAUDE.md.** The source-first protocol stands as written. This phase is an authorized exception, not a precedent. Future Display work — and all other porting work — defaults back to Thetis-exact constants unless the user grants another express, per-phase exception.

## 11. Files affected (estimated)

**New:**
- `src/gui/ColorSwatchButton.h`
- `src/gui/ColorSwatchButton.cpp`
- `docs/architecture/phase3g8-rx1-display-parity-plan.md` (this file)
- `docs/architecture/phase3g8-verification/` (screenshots)

**Modified:**
- `src/gui/setup/DisplaySetupPages.h` — add ~20 new control members across 3 page classes
- `src/gui/setup/DisplaySetupPages.cpp` — full rewrite of `buildUI()` for all 3 pages, all controls enabled, all wired to model
- `src/gui/SpectrumWidget.h/.cpp` — renderer additions (~600 LOC), `WfColorScheme` enum expansion
- `src/core/FFTEngine.h/.cpp` — averaging modes, FFT window switching
- `src/models/SliceModel.h/.cpp` — per-band grid struct, signal, persistence
- `src/core/AppSettings.cpp` — ~56 new keys (28 NereusSDR controls + 20 added + 28 per-band grid [14 × Max/Min] - overlap). Grid Step is a single global key, not per-band.
- `src/gui/SpectrumOverlayPanel.cpp` — verify Display sub-panel still works after live values
- `CHANGELOG.md` — phase entry

**~10–12 files modified, 2 new.**

## 12. Branch management

- Parent branch is `feature/phase3g7-polish` (3G-7 not yet merged to main as of 2026-04-12). When 3G-7 lands on main, rebase this branch with `git rebase main` — clean rebase expected (no overlapping files).
- New branch: `feature/phase3g8-rx1-display-parity` (created 2026-04-12)
- All commits GPG-signed (no `--no-gpg-sign`, no `--no-verify`)
- Pause for user review at each commit boundary per standing preference
- Show PR description as draft before posting (no auto-post)

## 13. Open questions — RESOLVED 2026-04-12

1. **Cal Offset (S8)** — **Resolved:** it IS a real Thetis field. `Display.RX1DisplayCalOffset` at `display.cs:1372-1380` (float, dB). Not a NereusSDR extension. §3.1 S8 row and §10 divergence table corrected accordingly. NereusSDR keeps its existing UI range (-30..30 dB, default 0.0).

2. **FPS overlay (G8)** — **Resolved:** `QPainter` text overlay in `SpectrumWidget::paintEvent`, top-right corner. Frame count + `QElapsedTimer` for rolling average. ~15 LOC. No new dependencies.

3. **Display Thread Priority (S17)** — **Resolved:** Thetis combo is 5 items (`setup.cs:34022` default "Above Normal"), not 7. 1:1 map to `QThread::Priority` on the FFT worker thread:

   | Thetis | QThread |
   |---|---|
   | Lowest | `QThread::LowestPriority` |
   | Below Normal | `QThread::LowPriority` |
   | Normal | `QThread::NormalPriority` |
   | Above Normal | `QThread::HighPriority` *(default)* |
   | Highest | `QThread::HighestPriority` |

   Apply via `FFTEngine` worker's `QThread::setPriority()`. Qt abstracts the OS priority class per platform.

4. **Per-band grid initial values** — **Resolved:** use Thetis uniform defaults (`Max = -40, Min = -140`) for all 14 band slots. Finding during resolution: Thetis ships every band with identical defaults (`console.cs:14242–14436`), so there's no hand-tuned per-band aesthetic to preserve — this just means adopting Thetis's -40/-140 over NereusSDR's current -20/-160 as the first-launch state. Existing users will see the grid shift on upgrade. This is the authorized one-off divergence expansion documented in §10. Also: **Grid Step is NOT per-band in Thetis** — it remains a single global value. Plan §5.3 corrected from "42 values" to "28 per-band values + 1 global Step".

## 14. How to begin (next session checklist)

1. Read this file end to end.
2. Re-read `CLAUDE.md` and `CONTRIBUTING.md` per standing preference.
3. Verify 3G-7 merge state: `git checkout main && git pull && git log --oneline -5`. If 3G-7 not merged, work off `feature/phase3g7-polish` and rebase later.
4. Resolve the four open questions in §13 (mostly quick reads).
5. `git checkout feature/phase3g8-rx1-display-parity` (branch already exists)
6. Execute commits in §9 order. Pause for review at each.
7. Build + auto-launch + screenshot at each commit per standing preference.
8. After commit 8 (last code commit), capture full verification matrix.
9. Show PR description draft to user before posting.

## 15. Out of scope (explicit deferrals)

- RX2 Display page — defer to phase that adds RX2 receiver support
- TX Display page — defer to phase 3I-1 / 3I-3 (TX work)
- Spectrum Overlay panel sub-panels (Display flyout) — separate work; those are runtime quick toggles, not Setup defaults
- Multimeter peak hold (`udDisplayMultiPeakHoldTime`) — meter system, already covered in 3G-6/3G-7
- Skin system color overrides — phase 3H
- Per-VFO display state — may need revisiting during 3F multi-panadapter phase
