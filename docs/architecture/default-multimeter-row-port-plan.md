# Meter Subsystem Parity Port Plan

**Branch:** `feat/default-multimeter-row-port`
**Base:** `feature/phase3g7-polish` @ `73bb826` (post PR #9 merge)
**Scope source:** `docs/architecture/meter-parity-audit.md` (this worktree)
**Strategy:** one PR, frequent commits, TDD throughout
**Status:** APPROVED — implementation in progress

---

## Goal

Close the `BarItem` / `ScaleItem` / `NeedleItem` / `SignalTextItem` / `MagicEyeItem`
parity gaps between NereusSDR and Thetis identified by the meter-parity audit,
then rebuild the broken presets that depend on those fields, then add the
default multimeter bar-row factories. Verify every touched preset both via
unit tests AND a live app screenshot.

## Ground rules

- **READ → SHOW → TRANSLATE** for every Thetis-backed commit. Commit body
  must quote the Thetis line range it ports from.
- All new serialized fields **append-only** with tail-tolerant parsing.
  Legacy saved layouts MUST continue to load without error.
- One GPG-signed commit per logical unit. Push often.
- Every RED test must fail on the parent commit before GREEN is written
  (per `superpowers:test-driven-development`).
- No code change without a preceding test, except pure refactors.

---

## Commit sequence

### Phase A — BarItem API expansion (the foundation)

**A1. `BarItem` ShowValue / ShowPeakValue numeric labels**
Thetis source: `MeterManager.cs:19939-19940, 20145+` (clsBarItem properties + renderHBar text pass), `MeterManager.cs:21541, 21706-21707` (example call sites in `addSMeterBar` / `AddSMeterBarText`).
Add `m_showValue`, `m_showPeakValue`, `m_peakValue`, `m_fontColour`, setters, `setValue()` tracks peak. `paint()` draws top-left current value and top-right peak text. Serialize tail append order: `showValue` → `showPeakValue` → `fontColour`.
RED: `BarItem_ShowValue_and_ShowPeakValue_render_text_corners`.

**A2. `BarItem` ShowHistory / HistoryColour / HistoryDuration**
Thetis source: `MeterManager.cs:19938, 19881, 19945-19946, 20040-20053` (history ring buffer + render), `:21539-21540, 21545` (4000ms default, Red(128) colour).
Add `m_showHistory`, `m_historyColour`, `m_historyDurationMs`, ring buffer sized by duration × update interval, render history polyline underneath main bar. Serialize append: `showHistory` → `historyColour` → `historyDurationMs`.
RED: `BarItem_ShowHistory_draws_trailing_fill`.

**A3. `BarItem` ShowMarker / MarkerColour / PeakHoldMarkerColour**
Thetis source: `MeterManager.cs:21543-21544` (ShowMarker + PeakHoldMarkerColour = Red), renderHBar marker draw lines.
Add `m_showMarker`, `m_markerColour`, `m_peakHoldMarkerColour`. Draw live value marker + separate peak-hold marker with independent decay. Serialize append.
RED: `BarItem_ShowMarker_draws_live_and_peakhold_lines`.

**A4. `BarItem` Line style + ScaleCalibration non-linear map**
Thetis source: `MeterManager.cs:21546-21549` (`BarStyle.Line` + 3-point calibration), `:21638-21640` (ADC 3-point), `renderHBar` Line branch.
Add `BarStyle::Line` enum value, `QMap<double, float> m_scaleCalibration` (value → X position 0-1), `setScaleCalibration(double, float)`, `clearScaleCalibration()`. When non-empty, `paint()` uses piecewise-linear interpolation between calibration points instead of linear min/max mapping. Serialize append: `barStyle` (if already present, extend enum), `scaleCalibration` (semicolon-joined key=pos pairs).
RED: `BarItem_ScaleCalibration_interpolates_nonlinearly` (assert S0=-133 → x=0, S9=-73 → x=0.5, S9+60=-13 → x=0.99).

**A5. Editor surface for new BarItem fields**
Wire the new booleans + colours into `BarItemEditor.cpp` as QGroupBox-grouped controls, following the 3G-7 NeedleItemEditor pattern. No new tests — editor is covered by existing persistence round-trip.

### Phase B — ScaleItem + ReadingName

**B1. `readingName(int bindingId)` map**
Thetis source: `MeterManager.cs:2258-2318` (`ReadingName()` switch).
Add free function `QString readingName(int bindingId)` in `MeterItem.h/cpp`. Exhaustive switch over every `MeterBinding::*` constant, returning the Thetis label exactly as spelled.
RED: `readingName_returns_Thetis_labels_verbatim` — one assert per binding.

**B2. `ScaleItem::setShowType` + centered title render**
Thetis source: `MeterManager.cs:14827` (ShowType property), `:31879-31886` (title render pass), `:21563, 21653` (call sites).
Add `m_showType`, setter/getter, paint pass that resolves `readingName(bindingId())` and draws centered in the top strip. Serialize append: `showType`.
RED: `ScaleItem_showType_renders_title_in_top_strip`.

**B3. `ScaleItem::generalScale` layout parity**
Thetis source: `MeterManager.cs:32338+` (`generalScale()` helper — major/minor tick math, two-tone, baseline at `y + h*0.85`).
Adjust existing `paint()` tick math to match. Keep existing colors.
RED: `ScaleItem_tick_layout_matches_generalScale`.

**B4. Editor surface for ScaleItem `ShowType`**
Wire checkbox into `ScaleItemEditor.cpp`.

### Phase C — Persistence

**C1. Tail-tolerant round-trip test**
Assemble a BarItem + ScaleItem with every new field set, serialize, deserialize, assert equality. Second case: parse a legacy pre-A1 payload, assert no error + defaults applied. Third case: parse a payload with EXTRA trailing fields (future-compat), assert no error.
RED: `BarItem_ScaleItem_tail_tolerant_roundtrip`.

### Phase D — Preset rebuilds (the visible work)

**D1. `createSMeterPreset` rebuilt from `AddSMeterBarSignal`**
Thetis source: `MeterManager.cs:21499-21616` (`addSMeterBar`).
Replace the current bare-NeedleItem body with the 4-item Thetis composition: SolidColour bg at `y - h*0.75` (z=1), BarItem with Line style + calibration [-133, -73, -13] at [0, 0.5, 0.99] + ShowHistory + ShowValue + ShowMarker + PeakHoldMarker(Red) + HistoryColour Red(128) + FontColour Yellow + attack 0.8 / decay 0.2 (z=2), ScaleItem with ShowType (z=3), ClickBoxItem covering the SolidColour (z≈4). Binding = `SignalPeak`.
RED: `createSMeterPreset_has_5_items_with_Thetis_calibration`.
**CHECKPOINT:** build + kill/relaunch app, visual diff against reference.

**D2. `createSignalTextPreset` rebuilt from `AddSMeterBarText`**
Thetis source: `MeterManager.cs:21678-21738` (`AddSMeterBarText`).
Replace current body with SolidColour bg at `y - h*0.75` (z=1) + SignalTextItem with FontSize 56, FontColour Yellow, ShowValue + ShowPeakValue, HistoryDuration 4000, attack 0.8 / decay 0.2 (z=2). Binding = `SignalAvg`. FadeCover optional.
RED: `createSignalTextPreset_matches_AddSMeterBarText`.

**D3. `createAdcMaxMagPreset` parity with `AddADCMaxMag`**
Thetis source: `MeterManager.cs:21617-21677` (`AddADCMaxMag`).
The audit listed this as NereusSDR-original; it actually has a Thetis counterpart. Retrofit with calibration [0, 25000, 32768] at [0, 0.8333, 0.99], MarkerColour Orange, HistoryColour CornflowerBlue(128), BarStyle::Line, attack 0.2 / decay 0.05. Binding = `AdcPeak`.
RED: `createAdcMaxMagPreset_matches_AddADCMaxMag`.

**D4. `createAdcBarPreset` parity with `AddADCBar`**
Thetis source: `MeterManager.cs:21740+` (`AddADCBar`).
Similar retrofit — read source first, port faithfully.
RED: `createAdcBarPreset_matches_AddADCBar`.

### Phase E — Default multimeter bar rows

**E1. Canonical `createAlcBarRowPreset`**
Thetis source: `MeterManager.cs:23326-23411` (`AddALCBar`).
First bar-row-style factory. 5 items: SolidColour bg (z=1), BarItem PEAK with ShowValue + ShowHistory + HistoryColour LemonChiffon(128) + yellow marker (z=2, Primary), BarItem AVG with dark gray marker (z=3), ScaleItem with ShowType + generalScale layout (z=4), FadeCover (z=5). Reading = `TxAlc` / `TxAlc_PK`.
RED: `createAlcBarRowPreset_has_5_items_at_expected_z`.
**CHECKPOINT:** visual diff against reference screenshot.

**E2-En. Remaining bar-row factories**
For each Thetis `Add*Bar` method (`AddEQBar`, `AddCompBar`, `AddLevelerBar`, `AddLevelerGainBar`, `AddCFCBar`, `AddCFCGainBar`, `AddMicBar`, `AddAGCBar`, `AddAGCGainBar`, `AddSignalBar`, `AddSignalAvgBar`, `AddMaxBinBar`, `AddPBSNRBar`, `AddALCGroupBar`, `AddALCGainBar`, `AddADCBar`, `AddADCMaxMag`) — one RED test + one GREEN commit per factory. Each commit body cites the Thetis line range.
After all are green, **refactor**: extract shared `buildBarRow(reading_pk, reading_avg, minVal, maxVal, parent)` helper.

### Phase F — Other P1 gaps

**F1. `NeedleItem::setPeakHold` + decay**
Thetis source: `MeterManager.cs:20585+` (clsNeedleItem PeakHold).
Add `m_peakHoldEnabled`, `m_peakHeldValue`, `m_peakDecayRatio`. Render a second thinner needle at peak position. Serialize append.
RED: `NeedleItem_PeakHold_holds_and_decays`.

**F2. `SignalTextItem` dBm / S-unit / µV unit switch**
Thetis source: `MeterManager.cs:20430+` (clsSignalText unit switching).
Add `Units` enum (`DBM`, `SUnits`, `MicroVolt`), setter, `formatValue()` helper that converts dBm → S-unit string (S0=-127dBm, S9=-73dBm, >S9 in 6dB steps → "S9+NNdB"). Serialize append: `units`.
RED: `SignalTextItem_formats_dBm_and_SUnits_and_uV`.

**F3. `createMagicEyePreset` bezel + FadeCover**
Thetis source: `MeterManager.cs:22249-22295` (`AddMagicEye`, specifically 22274-22290 for background ImageItem + FadeCover wrap).
Add optional background ImageItem at z=3 and FadeCover at z=10. ImageItem asset path stays empty by default (user can supply skin texture).
RED: `createMagicEyePreset_has_bezel_image_and_fadecover`.

### Phase G — Final verification

**G1. Full `tst_container_persistence` green**
All existing cases still pass. New cases from C1 green.

**G2. Visual verification pass**
Build, kill+relaunch app, screenshot each touched preset, compare against Thetis reference screenshots. Record any residual visual drift as follow-up issues (not blockers for this PR).

**G3. `simplify` skill sweep**
Review changed code for reuse, quality, efficiency. Fix anything flagged.

**G4. `requesting-code-review` skill**
Run before opening the PR.

---

## Files expected to change

- `src/gui/meters/MeterItem.h` — BarItem / ScaleItem / NeedleItem / SignalTextItem API, `readingName()` decl
- `src/gui/meters/MeterItem.cpp` — paint() extensions, serialize/deserialize tail, `readingName()` switch
- `src/gui/meters/ItemGroup.h` — new bar-row factory declarations
- `src/gui/meters/ItemGroup.cpp` — rewritten SMeter / SignalText / AdcMaxMag / AdcBar / MagicEye / all bar-row factories
- `src/gui/containers/meter_property_editors/BarItemEditor.{h,cpp}` — surface new booleans + colours
- `src/gui/containers/meter_property_editors/ScaleItemEditor.{h,cpp}` — surface ShowType
- `src/gui/containers/meter_property_editors/NeedleItemEditor.{h,cpp}` — surface PeakHold
- `src/gui/containers/meter_property_editors/SignalTextItemEditor.{h,cpp}` — surface Units
- `tests/tst_container_persistence.cpp` — new round-trip cases
- `tests/tst_meter_factories.cpp` (new, if needed) — factory composition assertions

Files NOT touched: `MeterWidget`, `MeterPoller`, `ContainerWidget`, `ContainerManager`, `Band`, any RadioModel / SliceModel / connection code.

---

## Review checkpoints (pause for maintainer)

1. **After A4** — BarItem core API complete. Verify signatures before touching editors/presets.
2. **After D1** — first rebuilt preset live in app. Visual sanity check vs Thetis S-meter.
3. **After E1** — first bar-row factory green + visually verified. Mechanical work from here.
4. **Before opening PR** — G1/G2/G3/G4 all green.

---

## Standing user rules applied

- GPG-sign every commit (`commit.gpgsign = true` — never `--no-gpg-sign`)
- `Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>` trailer on every commit
- Plan lives under `docs/architecture/` (this file)
- Diff-style code reviews in chat, not unchanged blocks
- Auto kill+relaunch after every successful build (osascript quit)
- GitHub posts: draft → approve → post, signature `JJ Boyd ~KG4VCF` + Claude co-author line
- READ→SHOW→TRANSLATE on every commit body that touches Thetis-backed code
