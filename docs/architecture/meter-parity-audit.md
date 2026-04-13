# Meter Parity Audit: NereusSDR vs Thetis

**Date:** 2026-04-12
**Worktree:** `feat/default-multimeter-row-port` @ `73bb826`
**Scope:** 32 `ItemGroup` factories + 31 `MeterItem` subclasses vs `../Thetis/Project Files/Source/Console/MeterManager.cs`
**Mode:** Read-only reconnaissance — no code changed.

---

## Part 1: ItemGroup Factory Audit

| Factory | NereusSDR | Thetis source | Composition match | Known gaps | Blocking? |
|---|---|---|---|---|---|
| `createHBarPreset` | `ItemGroup.cpp:328-390` | AddMeter generic (composite) | SolidColour(0) → Bar(5) → Scale(10) + 2×Text(10) | Inherits BarItem/ScaleItem subclass gaps (see Part 2) | Minor |
| `createCompactHBarPreset` | `ItemGroup.cpp:398-448` | AddMeter compact variant | SolidColour(0) → Bar(5) + label/readout Text | Same as HBar | Minor |
| `createSMeterPreset` | `ItemGroup.cpp:476-489` | AetherSDR pattern — no Thetis AddSMeter located | Single NeedleItem; no calibration/ring | No labels or ring rendering | **Moderate** (S-meter is central UX) |
| `createPowerSwrPreset` | `ItemGroup.cpp:498-608` | `AddPWRBar:23862` + `AddSWRBar:23990` | Full parity — attack 0.8f / decay 0.1f match, red@100W / red@3:1 | **None** | **None** |
| `createAlcPreset` | `ItemGroup.cpp:616-620` | Thetis ALC scale (-30..0 dB) | Wrapper → CompactHBar | Inherits | Minor |
| `createMicPreset` | `:628-632` | Thetis MIC scale (-30..0 dB) | Wrapper → HBar | Inherits | Minor |
| `createCompPreset` | `:640-648` | Thetis COMP scale (-25..0 dB) | Wrapper → HBar | Inherits | Minor |
| `createSignalBarPreset` | `:650-654` | Inferred from AddMeter dispatch | Wrapper → HBar | Inherits | Minor |
| `createAvgSignalBarPreset` | `:656-660` | Inferred | Wrapper → HBar | Inherits | Minor |
| `createMaxBinBarPreset` | `:662-666` | Phase 3G-4 NereusSDR addition | Wrapper → HBar | NereusSDR-original | N/A |
| `createAdcBarPreset` | `:668-672` | No Thetis match | Wrapper → HBar | NereusSDR-original | N/A |
| `createAdcMaxMagPreset` | `:674-678` | No Thetis match | Wrapper → HBar | NereusSDR-original | N/A |
| `createAgcBarPreset` | `:680-684` | Inferred | Wrapper → HBar | Inherits | Minor |
| `createAgcGainBarPreset` | `:686-690` | Phase 3G-4 | Wrapper → HBar | Inherits | Minor |
| `createPbsnrBarPreset` | `:692-696` | Phase 3G-4 (no Thetis) | Wrapper → HBar | NereusSDR-original | N/A |
| `createEqBarPreset` | `:698-702` | Phase 3G-4 | Wrapper → HBar | Inherits | Minor |
| `createLevelerBarPreset` | `:704-708` | No direct Thetis | Wrapper → HBar | NereusSDR-original | N/A |
| `createLevelerGainBarPreset` | `:710-714` | NereusSDR-original | Wrapper → HBar | — | N/A |
| `createAlcGainBarPreset` | `:716-720` | Phase 3G-4 | Wrapper → HBar | Inherits | N/A |
| `createAlcGroupBarPreset` | `:722-726` | Phase 3G-4 | Wrapper → HBar | Inherits | N/A |
| `createCfcBarPreset` | `:728-732` | Phase 3G-4 | Wrapper → HBar | Inherits | N/A |
| `createCfcGainBarPreset` | `:734-738` | Phase 3G-4 | Wrapper → HBar | Inherits | N/A |
| `createCustomBarPreset` | `:740-744` | User-defined | Wrapper → HBar | Inherits | Minor |
| `createAnanMMPreset` | `:754-984` | `AddAnanMM:22461-22815` | **Full parity** — 7-needle calibrations, red 233/51/50, attack/decay all match | **None** | **None** |
| `createCrossNeedlePreset` | `:990-1150` | `AddCrossNeedle:22817-23002` | **Full parity** — 10-point fwd/rev calibrations, needle offsets, radius all match | **None** | **None** |
| `createMagicEyePreset` | `:1150-1161` | `AddMagicEye:22249-22295` | Single MagicEyeItem only | **Missing bezel ImageItem** (Thetis:22274-22290) and FadeCover | Minor visual |
| `createSignalTextPreset` | `:1167-1188` | `clsSignalText:20286+` — no Add* factory located | SolidColour + SignalTextItem (yellow #ffff00 matches Thetis:21708) | No dBm/µV unit switching (see Part 2) | Cosmetic |
| `createHistoryPreset` | `:1193-1225` | `AddHistory:24233-24261` | SolidColour + HistoryGraphItem(80%) + TextItem(20%), cap=300 | Possible button/event behavior on Thetis `clsHistoryItem` not ported | Cosmetic |
| `createSpacerPreset` | `:1229-1239` | `AddSpacer:22324-22347` | Single SpacerItem | **None** | **None** |
| `createVfoDisplayPreset` | `:1245-1257` | No Thetis equivalent | NereusSDR Phase 3G-5 original | — | N/A |
| `createClockPreset` | `:1260-1280` | No Thetis equivalent | NereusSDR Phase 3G-5 (0..0.15 strip, post-3G-7 layout fix) | — | N/A |
| `createContestPreset` | `:1282-1307` | No Thetis equivalent | NereusSDR Phase 3G-5 composite (VFO + Band + Mode + Clock) | — | N/A |

**Summary:** 3 factories achieve zero-gap parity (PowerSWR, AnanMM, CrossNeedle). 20 wrapper factories delegate to `createHBarPreset` / `createCompactHBarPreset` and inherit the subclass-level gaps in Part 2. 9 factories are NereusSDR-original (no Thetis source — Phase 3G-4/5 extensions).

---

## Part 2: MeterItem Subclass Parity

| Subclass | Thetis cls | API gaps | Render gaps | Serialization risk | Blocking? |
|---|---|---|---|---|---|
| SolidColourItem | `clsSolidColour:6725+` | — | — | — | None |
| ImageItem | `clsImage:14620+` | — | — | — | None |
| **BarItem** | `clsBarItem:19917-20278` | Missing `ShowValue(19939)`, `ShowPeakValue(19940)`, `ShowHistory(19938)`, `HistoryColour(19881,19945-19946)`, `MarkerColour`, `PeakHoldMarkerColour`, `Units` enum, Style variants (`SolidFilled`/`GradientFilled`/`Segments`) | No history overlay (20040-20053), no value/peak text (20145+), no peak-hold marker, no multi-segment Edge | History data not serialized (in-session only) | **Moderate** — affects every HBar/CompactHBar wrapper factory (~20 presets) |
| **ScaleItem** | `clsScaleItem:14785-15853` | Missing `ShowType(14827)` centered title, `Units` enum, `FontFamily`/`FontStyle` | No ShowType render (14945+), fixed font only | Full parity on existing fields | **Minor** — but touches every HBar wrapper |
| TextItem | Inferred from inline text in clsBarItem | NereusSDR adds `setIdleText` / `setMinValidValue` (improvements) | — | Full parity | None |
| **NeedleItem** | `clsNeedleItem:20580-21038` | Missing `PeakHold(20585+)`, needle trail, DisplayGroup filter semantics (NereusSDR uses OnlyWhenTx/Rx instead) | No peak-hold render, no trail | `NormaliseTo100W` is NereusSDR enhancement (fine) | **Minor** — affects S-Meter and needle presets |
| SpacerItem | `clsSpacerItem:14604-14620` | — | — | — | None |
| FadeCoverItem | `clsFadeCover` (via `getFadeCover():22289+`) | — | — | — | None |
| LEDItem | No Thetis equivalent | N/A | N/A | N/A | N/A |
| **HistoryGraphItem** | `clsHistoryItem:16149-16600` | — | Missing waterfall-style background gradient (16400+) | Capacity matches | Cosmetic |
| MagicEyeItem | `clsMagicEyeItem:15855-16148` | — | — | — | None |
| NeedleScalePwrItem | NereusSDR factoring of Thetis inline labels | — | — | — | None (improvement) |
| **SignalTextItem** | `clsSignalText:20286-20580` | Missing `Units` enum (DBM / S_UNTS / µV) — NereusSDR has S_UNTS only | Missing dBm↔S-unit conversion (20430+) | Unit state not persisted | Minor |
| DialItem | `clsDialDisplay:22296-22323` | — | — | — | None |
| TextOverlayItem | No Thetis equivalent | N/A | N/A | N/A | N/A |
| WebImageItem | No Thetis equivalent | N/A | N/A | N/A | N/A |
| FilterDisplayItem | No Thetis equivalent | N/A | N/A | N/A | N/A |
| RotatorItem | No Thetis equivalent | N/A | N/A | N/A | N/A |
| ButtonBoxItem (base) | `clsButtonBox:~23180+` inferred | NereusSDR refactor into base+subclasses; adds mouse forwarding (3G-5) | Interactive vs static divergence | Dynamic grid serialization | Minor |
| BandButtonItem | `clsBandButtonBox` inferred | 14-band enum added in 3G-8 | — | — | None |
| ModeButtonItem | `clsModeButtonBox` inferred | — | — | — | None |
| FilterButtonItem | `clsFilterButtonBox` inferred | — | — | — | None |
| AntennaButtonItem | `clsAntennaButtonBox:24262-24297` | — | — | — | None |
| TuneStepButtonItem | `clsTunestepButtons:24298-24331` | — | — | — | None |
| OtherButtonItem | NereusSDR catch-all | N/A | N/A | N/A | N/A |
| VoiceRecordPlayItem | Phase 3G-5 original | N/A | N/A | N/A | N/A |
| DiscordButtonItem | No Thetis equivalent | N/A | N/A | N/A | N/A |
| VfoDisplayItem | Phase 3G-5 original | N/A | N/A | N/A | N/A |
| ClockItem | Phase 3G-5 original | N/A | N/A | N/A | N/A |
| ClickBoxItem | NereusSDR hit-test | N/A | N/A | N/A | N/A |
| DataOutItem | Phase 3G-5 original | N/A | N/A | N/A | N/A |

---

## Part 3: Prioritized Gap List

| # | Gap | Impact radius | Effort | Priority |
|---|---|---|---|---|
| 1 | BarItem `ShowValue` top-left numeric | ~20 wrapper factories | S | **P0** |
| 2 | BarItem `ShowPeakValue` + peak marker | ~20 factories | S | **P0** |
| 3 | BarItem `ShowHistory` + `HistoryColour` | ~20 factories | M | **P0** |
| 4 | ScaleItem `ShowType` centered title | every HBar scale | S | **P0** |
| 5 | NeedleItem `PeakHold` + decay | S-Meter, ANAN MM, CrossNeedle | M | **P1** |
| 6 | SignalTextItem dBm↔S-unit toggle | SignalText preset | S | **P1** |
| 7 | MagicEye bezel ImageItem + FadeCover | MagicEye preset | S | **P1** |
| 8 | HistoryGraphItem waterfall background | History preset | S | **P2** |
| 9 | BarItem `GradientFilled` / `Segments` | cosmetic across bars | M | **P2** |
| 10 | NeedleItem trail history trace | cosmetic needle presets | M | **P2** |

Plus an independent **P1 item not in the bar/needle axis**: `createSMeterPreset` (`ItemGroup.cpp:476-489`) currently builds only a single bare `NeedleItem` with no calibration, ring, or labels — needs a proper S-meter composition with tick marks. No Thetis `AddSMeter` method was located by the audit; maintainer should confirm the source (likely an inline composition somewhere in `console.cs` or a Thetis user-skin file).

---

## Recommendation

**P0 cluster is internally coherent** — it's exactly the BarItem + ScaleItem work from the original bar-row port plan. Shipping those four fixes in one PR closes the gaps that propagate through ~20 wrapper factories, then the default multimeter row factories become a thin second PR built on top.

**P1 items are independent** — each targets a single preset and can land as small follow-up PRs after P0.

**P2 items are cosmetic** — defer to a future polish phase.
