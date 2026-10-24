# Container item options parity: audit

Date: 2026-10-24. Compares every NereusSDR container item's options and controls with
Thetis v2.10.3.15 (`3759d09`, still the newest tag; the five commits after it touch no
meter code). NereusSDR at main `b85e4127d`. Read-only audit by five scouts; Thetis cites
are file:line in the v2.10.3.15 tree. Rulings and the plan come after JJ's decisions.

| Part | Items | Missing | Partial |
|---|---|---|---|
| A | Ready-made meters (`meter.*`) | 32 | 62 |
| B | Building blocks 1 (BAR, SOLID, IMAGE, SCALE, TEXT, NEEDLE, SPACER, FADECOVER, LED, HISTORY, MAGICEYE, NEEDLESCALEPWR, SIGNALTEXT) | 47 | 36 |
| C | Building blocks 2 (CLOCK, VFO, DIAL, TEXTOVERLAY, WEBIMAGE, FILTERDISPLAY, ROTATOR, CLICKBOX, DATAOUT, VOICERECPLAY) | 100 | 60 |
| D | Button grids and container options | 37 | 35 |
| E | Longpath instruments, S-meter dial, TX section | 106 (+77 shared rows) | 35 |

Labels and picker grouping are out of scope (container naming cleanup). `RxSectionContent`
and `NereusBandButton` belong to PR 635 and are not touched here.


---

## Part: Audit A: ready-made meters (`meter.*` presets)

Scout: read-only. Thetis tree v2.10.3.15. NereusSDR worktree `gracious-dirac-71a651` at `b85e4127d`.

Paths, abbreviated:

* `MM` = `../Thetis/Project Files/Source/Console/MeterManager.cs`
* `SU` = `../Thetis/Project Files/Source/Console/setup.cs`
* `SUD` = `../Thetis/Project Files/Source/Console/setup.designer.cs`
* `CCR` = `src/gui/containers/ContainerContentRegistry.cpp`
* `CPI` = `src/gui/meters/presets/CompositePresetItem.cpp`
* `BPI` = `src/gui/meters/presets/BarPresetItem.cpp`
* `CPE` = `src/gui/containers/ContentPropertyEditor.cpp`
* `CSD` = `src/gui/containers/ContainerSettingsDialog.cpp`

### How NereusSDR builds the presets and which editor each one uses

* Expansion: `CCR:72-123` (`allocate()`).
  * `meter.radeMicWaveform` / `meter.radeSpeechWaveform` build `WaveformMeterItem` (`CCR:73-74`).
  * `meter.powerSwr`, `ananMulti`, `crossNeedle`, `magicEye`, `signalText`, `historyGraph`, `vfoDisplay`, `clock` and `contest` build thin adapters (`CCR:75-83`). Each adapter is about 5 lines over `CompositePresetItem(Face::...)` in `src/gui/meters/presets/*PresetItem.cpp`.
  * `meter.sMeter` builds `SMeterPresetItem`, a `BarPresetItem` with `configureVariant("SignalAvg")` (`CCR:84`).
  * `meter.spacer` builds the `SpacerItem` building block (`CCR:85`) and is stored as `SPACER` (`CCR:381`).
  * `meter.<variant>` calls `BarPresetItem::configureVariant` (`CCR:86`).
  * `meter.mic`, `meter.alc` and `meter.customBar` are covered at `CCR:87-91`.
  * Descriptors are at `CCR:240-257`. `meter.adcMax` is marked unavailable at `CCR:254` ("ADC maximum magnitude has no sanctioned reading provider").
* Editors:
  * Every `meter.*` preset except `meter.spacer` is edited in `ContentPropertyEditor`:
    * composite path: about `CPE:340-352`, driven by `CompositePresetItem::editableFields` (`CPI:553-567`) and `editableChannelFields` (`CPI:543-551`).
    * bar path: about `CPE:353-362`.
    * waveform path: title, showTitle, viewWindowMs, faceHeight, traceColor, sourceStage.
  * `meter.spacer` uses the building-block `SpacerItemEditor`.
  * Every entry also gets an "Advanced bindings" group in `CPE`: binding ids, a Source combo (container default, Slice A-D, Follow selected RX), Radio session, MMIO source and variable. This group is disabled for clock.
* Thetis setup: one item-group editor, `grpMeterItemSettings`, serves every bar, needle and eye type. It is saved by `updateMeterType()` (`SU:24999-25570`), loaded at `SU:26316-26456` and shown by `setupMMSettingsGroupBoxes` (`SU:26695-26860`). Special group boxes serve HISTORY, SIGNAL_TEXT, VFO_DISPLAY, CLOCK and SPACER.
  * Storage is the `clsIGSettings` bag (`MM:1433`, properties `MM:1878-1923`).
  * The settings are applied by `ApplySettingsForMeterGroup` (`MM:26908`).

### Item-group-level options Thetis offers every type (reference for all tables below)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Add / remove / reorder items in a meter | `btnAddMeterItem` `SU:24813`, Remove `SU:24855`, Up `SU:24873`, Down `SU:24892` | Container settings dialog content list (`CSD`), z-order in `BaseItemEditor` | match (out of this scout's depth; container scout owns it) |
| Copy / paste item settings | `btnMeterCopySettings` about `SU:26969`. Paste about `SU:26988-27030` keeps the target's Unit and SubIndicators | `CSD:661-671`, enabled at `CSD:2055-2066` only when the type matches. Hidden in draft edit sessions (`CSD:503`) | partial (same-type only; hidden while editing drafts) |
| Update interval | `nudMeterItemUpdateRate` 15-5000 ms, default 100 (`SUD`), `igs.UpdateInterval` `SU:25515` | `updateIntervalMs` >= 1, up to 1e6 (`CPE`, `CPI:579-617`) | partial (range differs) |
| Attack / Decay | 0-1, step .05, 3 decimal places (`SU:25516-25517`) | `attack` and `decay` 0-1 (`CPI:579-617`, `BPI` keys) | match |
| History duration | 50-30000 ms (`SU:25521`) | `historyMs` >= 100 composite; bars accept larger values (`CPE`) | partial (range) |
| Ignore history / peak | "Ignore History/Peak (ms)" 0-4000 (`SU:25522`) | `ignoreHistoryMs` up to 1e6 (`CPE`) | partial (range) |
| Show history + history colour with alpha trackbar | `SU:25518-25519`, trackbar 0-255 | `showHistory`, `historyColor` (#AARRGGBB picker with alpha) | match |
| Shadow | `SU:25520`. Enabled only for ANANMM and CROSS (`SU:26316-26456`) | per needle channel `shadow` (`CPI:543-551`). Not offered on bars | match for needles |
| Peak hold + peak-hold marker colour | `SU:25534-25535` | `peakHold` exists. Colour: composites use `highColor`; bars hard-code `Qt::red` (`BPI` paint) | partial |
| Peak value + peak value colour | `SU:25541-25542` | `showPeakValue` exists. Bars hard-code the colour red; composites have no separate colour | partial |
| Low / High scale colours | `SU:25508-25509` (`LowColor`, `HighColor`) | composites: `lowColor` and `highColor`. Bars: hard-coded white/red | partial |
| Indicator colour + Show | `SU:25510`, `SU:25512` (`MarkerColour`, `ShowMarker`) | bars: `barColor`, no Show toggle. Composites: per-channel `color` | partial |
| Sub indicator colour + Show | `SU:25511`, `SU:25513`. Enabled only when `igs.SubIndicators` | bars: average marker hard-coded `QColor(169,169,169)`, no toggle | missing on bars |
| Background colour | `SU:25514` (`Colour`) | `backdropColor` | match |
| Bar style Line / Solid / Segmented | `SU:25524-25529` | `style` Line / Solid / Segments (`BPI`) | match |
| Segmented/Solid low and high colours | `SU:25531-25532` | missing (bars fill with `barColor` and red) | missing |
| Fade on RX / Fade on TX | `SU:25537-25538`. `fade()` dims to 48/255 over 0.5 s (`MM:33153-33177`), cover drawn at `MM:37826` | composites: `fadeRx`/`fadeTx`, instant `setOpacity(.25)` (`CPI:1181`). Bars: missing | partial / missing |
| Meter title Show + title colour | `ShowType` `SU:25539`, `TitleColor` `SU:25540` | composites: `showTitle`, `titleColor`. Bars: `label` text and `titleColor`, but no show toggle | partial |
| Power limit | `MaxPower` 0.1-5000 (`SU:25545`). Enabled for ANANMM, CROSS, PWR and REVERSE_PWR | missing per item. A global power scale is set through `setPowerScale` | missing |
| RX source of the whole meter | container RX1/RX2 assignment (`ucMeter`) | per entry: Source combo with Slice A-D / Follow selected RX, and Radio session (`CPE` Advanced bindings) | Nereus-only granularity |
| MMIO variables | `%` buttons, enabled by `MeterVariables` (`MM:22684-22735`, `SU:25607-25645`) | one MMIO source/variable pair per entry (`CPE`) | partial (see per-type notes) |
| Only when RX / only when TX (hide) | not found as a hide option; Thetis fades instead | `onlyWhenRx`/`onlyWhenTx` (`CCR:404-405`, `540-541`) | Nereus-only |

Multislice notes:

* Thetis binds a whole meter container to RX1 or RX2 (`ucMeter`). Every reading in the item group follows that one receiver. NereusSDR lets each entry pick Slice A-D, Follow selected RX, or the container default, plus a Radio session for the remote Core (`CPE` Advanced bindings). This already covers the need.
* Fade on RX/TX in Thetis keys off the single radio's MOX. NereusSDR's fade keys off the context's RX/TX state (`CCR:404-405`). Open question: with a Core session, which radio's TX state drives fade? (See Summary.)

---

### meter.radeMicWaveform: none (NereusSDR-original WaveformMeterItem, Microphone)

Thetis has no RADE waveform meter. `MeterType` enum `MM:178-232` has none. `WAVE_RECORD` is the recorder, a different feature.

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Title + Show title | n/a, no Thetis type (enum `MM:178-232`) | `title`, `showTitle` (`src/gui/meters/WaveformMeterItem.cpp:20-21`) | Nereus-only |
| View window 5 s / 1 s / 100 ms | n/a | `viewWindowMs` (`WaveformMeterItem.cpp:22`, validated `:51-53`) | Nereus-only |
| Face height 180-4000 | n/a | `faceHeight` (`:23`, `:61-64`) | Nereus-only |
| Trace colour | n/a | `traceColor` `#7fae91` (`:24`) | Nereus-only |
| Grid / text colour | n/a | `gridColor`, `textColor` accepted by config (`:65`, used `:148-170`) but not in `CPE` | Nereus-only (hidden keys) |
| Source stage Conditioned / RawMic | n/a | `sourceStage` (`:25`, `:55-57`) | Nereus-only |
| Fade RX/TX, update, attack/decay | n/a | missing | n/a (no Thetis type) |

Multislice notes:

* The mic is a TX-side source and there is one TX path. No slice assumption applies. With a remote Core, the waveform should come from the Core's TX chain. I did not verify which session it reads today.

### meter.radeSpeechWaveform: none (NereusSDR-original WaveformMeterItem, DecodedSpeech)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Title + Show title | n/a (enum `MM:178-232`) | `WaveformMeterItem.cpp:20-21` | Nereus-only |
| View window | n/a | `:22` | Nereus-only |
| Face height | n/a | `:23` | Nereus-only |
| Trace colour | n/a | `:24` | Nereus-only |
| Source stage | n/a | forced to `Conditioned` for decoded speech (`:58-60`); `CPE` hides the combo | Nereus-only |

Multislice notes:

* Decoded RADE speech belongs to whichever slice runs the RADE decoder. The face has no slice selector in `CPE`. If two slices decode RADE, which one feeds this meter is unspecified. Needs a ruling.

---

### meter.powerSwr: no single Thetis type; Thetis offers PWR (`AddPWRBar` `MM:25177/25185`) + SWR (`AddSWRBar` `MM:25313`) as separate bars

NereusSDR builds one composite with 2 bar channels, Power (normalized) and SWR (`CPI:267-271`).

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Item structure | two separate bar items, PWR and SWR, each with its own `clsIGSettings` | one composite, two channels (`CPI:267-271`) | partial (different shape) |
| Update / attack / decay / ignore history | group-wide (`SU:25515-25522`) | per channel (`CPI:543-551`) | Nereus-only granularity (superset) |
| History + show history + history colour | `SU:25518-25521` | per channel `historyMs`, `showHistory`, `historyColor` | match (per channel) |
| Peak hold + colour | `SU:25534-25535` | per channel `peakHold`. Colour = face `highColor` (`CPI:620-700`) | partial |
| Peak value + colour | `SU:25541-25542` | `showPeakValue`, no colour | partial |
| Low / High colours | `SU:25508-25509` | `lowColor`, `highColor` | match |
| Indicator colour | `MarkerColour` `SU:25510` | per channel `color` | match |
| Sub indicator colour + Show | `SU:25511`, `SU:25513` | missing | missing |
| Bar style | `SU:25524-25529` | missing (composite bar face fixed) | missing |
| Seg/Solid low/high colours | `SU:25531-25532` | missing | missing |
| Background | `SU:25514` | `backdropColor` | match |
| Title show + colour | `SU:25539-25540` | `showTitle`, `titleColor` | match |
| Power limit | `MaxPower` for PWR/REVERSE_PWR (`SU:25545`, applied `MM:28292`) | missing per item; global power scale | missing |
| Fade RX/TX | `SU:25537-25538`, animated | `fadeRx/fadeTx`, instant 25% (`CPI:1181`) | partial |
| Channel name / units / visible | n/a (fixed by type) | `name`, `units`, `visible` per channel | Nereus-only |
| Font size, face height, show readout | n/a | `fontSize`, `faceHeight`, `showReadout` | Nereus-only |
| MMIO | PWR/SWR `MeterVariables` = 1 (`MM:22684-22735`) | one pair per entry, not per channel | partial |
| Legal marker | not found in `AddPWRBar` | drawn in `paintBar` (`CPI:620-700`) | Nereus-only |

Multislice notes:

* Forward/reverse power and SWR are TX readings from the one radio in Thetis (`AddPWRBar` `MM:25177`). In NereusSDR they come from the active radio session. With a Core, the entry's Radio session picks the radio. No slice is involved; the Source combo is meaningless here but still offered.

### meter.ananMulti: ANANMM (`AddAnanMM` `MM:23784-24138`)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| 7 needles with display groups ALL / PWR/SWR / Comp / ALC / Volts/Amps | `MM:23784-24138`, default group 1 (`MM:24131-24136`) | 7 channels, per-channel `displayGroup`, `onlyWhenRx/Tx` (`CPI:281-298`). Face `displayGroup` (`CPI:553-567`). Click in TX cycles the group %5 (`CPI:1241`) | match |
| Update / attack / decay | group-wide to all needles (`MM:27920-27988`) | per channel | Nereus-only granularity |
| History + show history + colour | primary needle and the SWR needle only (`MM:27920-27988`) | per channel | Nereus-only granularity |
| Ignore history | `SU:25522` | per channel `ignoreHistoryMs` | match |
| Peak hold | signal and PWR needles (`MM:27920-27961`) | per channel `peakHold`, peak needle drawn in `highColor` | partial (colour) |
| Peak-hold colour | `PeakHoldMarkerColor` `SU:25535` | uses face `highColor` | partial |
| Primary needle colour | `MarkerColour` (`MM:27920-27961`) | per channel `color` | match |
| Non-primary needle colour | `SubMarkerColour` (`MM:27987`) | per channel `color` | match (finer) |
| Shadow | `SU:25520`, enabled for ANANMM | per channel `shadow` | match |
| Dark Mode (image swap) | `SU:25565`, applied `MM:27997` | missing | missing |
| Signal type Peak / Avg / MaxBin | selector `SU:25548-25564`, applied from `Average`/`MaxBin` | per-channel reading combo (`CPE`) binds any reading | partial (more general, but not the Thetis 3-way control) |
| Power limit | `MaxPower` `SU:25545` | missing per item; global scale | missing |
| Power scale colour | `PowerScaleColour` `SU:25546` (load-enabled for ANANMM/CROSS) | missing | missing |
| Background, title, low/high, seg/solid, peak value | disabled for ANANMM (`SU:26316-26456`) | `backdropColor`, `showTitle`, `titleColor`, `lowColor`, `highColor`, `showPeakValue` offered | Nereus-only |
| Fade RX/TX | `SU:25537-25538` | `fadeRx/fadeTx` instant | partial |
| Needle geometry (offset, radius, length, stroke) | n/a (fixed calibration in `AddAnanMM`) | per channel (`CPI:543-551`) | Nereus-only |
| Images ananMM, ananMM-bg, ananMM-bg-tx | `MM:23784-24138` | drawn faces; I did not verify image assets | partial (not verified) |
| MMIO | `MeterVariables` ANANMM = 7, but buttons disabled ("TODO ananmm not done yet" `MM:27906`) | one pair per entry | Nereus-only (Thetis disabled) |

Multislice notes:

* The signal needle reads the container's RX (RX1/RX2) in Thetis. NereusSDR lets each channel bind any reading plus the entry's Slice A-D / Follow selected RX. That covers it.
* The Volts/Amps and PWR/SWR/ALC/Comp needles are one-radio TX readings. With a Core, they follow the entry's Radio session.
* The S-meter calibration above S9 uses `IsAboveS9Frequency` for the container RX in Thetis. NereusSDR pushes `setAboveS9Frequency` to the preset. I did not verify which slice's frequency it uses when Source is Slice B-D.

### meter.crossNeedle: CROSS (`AddCrossNeedle` `MM:24140-24326`)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Forward and reverse needles + scales | `MM:24140-24326`, NeedleScalePwr items | channels at `CPI:272-280` | match |
| Update / attack / decay / fade / shadow | group-wide to both needles (`MM:27920-27988`) | per channel | Nereus-only granularity |
| History | primary needle and REVERSE_PWR (`MM:27962-27988`) | per channel | match |
| Peak hold | disabled for CROSS (`SU:26316-26456`) | per channel `peakHold` offered | Nereus-only |
| Needle colours (Marker / SubMarker) | `MM:27920-27988` | per channel `color` | match |
| Shadow | enabled for CROSS | per channel `shadow` | match |
| Dark Mode | `SU:25565`, `MM:27997`, scale `MM:28015` | missing | missing |
| Power limit | `MaxPower` `SU:25545`, scale `MM:28014` | missing per item; global scale | missing |
| Power scale colour | `PowerScaleColour` `SU:25546`, scale `LowColour` `MM:28016` | missing | missing |
| Background, title, low/high, seg/solid, peak value | disabled for CROSS | offered (face fields) | Nereus-only |
| MMIO | 2 variables, CROSS only (`MM:27906`, `MM:22684-22735`) | one pair per entry | partial |
| Needle geometry | fixed | per channel | Nereus-only |
| Fade RX/TX | animated | instant | partial |

Multislice notes:

* TX-only readings from the one radio in Thetis. With a Core, the entry's Radio session decides the radio. No slice dimension.

### meter.magicEye: MAGIC_EYE (`AddMagicEye` `MM:23572`)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Eye colour | `MarkerColour` (`MM:27847`) | channel `color` (`CPI:299-304`) | match |
| Signal type Avg / Peak / MaxBin | selector `SU:25548-25564`, applied `MM:27849-27856` | channel reading combo, default SignalAvg | partial |
| Eye Size | `EyeScale` 0.01-1 (`SU:25543`, `MM:27807-27812`) | missing | missing |
| Bezel Size | `EyeBezelScale` 0.01-1 (`SU:25544`, `MM:27868-27869`) | missing | missing |
| Bezel image "eye-bezel" | `MM:23572` builder | missing (`paintEye` about `CPI:1110` draws no bezel) | missing |
| Update / attack / decay | defaults attack .2, decay .05 (`MM:23572`), `SU:25515-25517` | base channel fields | match |
| History / peak hold | disabled for MAGIC_EYE on load (`SU:26316-26456`) | not offered (Eye not in the history channel list `CPI:543-551`) | n/a (disabled in Thetis too) |
| Title, low/high, seg/solid, peak value, background | disabled for MAGIC_EYE | `title`, `showTitle`, `titleColor`, `lowColor`, `backdropColor`, `showReadout` offered | Nereus-only |
| Fade RX/TX | `SU:25537-25538` | instant | partial |
| MMIO | `MeterVariables` = 1 | one pair | match |

Multislice notes:

* `IsAboveS9Frequency(_rx)` (`MM:23594`) uses the container RX. NereusSDR's Source combo selects Slice A-D. Not verified that the S9 frequency follows the chosen slice.

### meter.signalText: SIGNAL_TEXT (`AddSMeterBarText` `MM:23001`)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Reading text colour | `FontColour` = `MarkerColour` (`MM:28167-28190`) | `lowColor` drives the text (`CPI:553-567`) | partial (named differently) |
| µV / dBm sub readings + colour + Show | `ShowSubMarker`, `SubMarkerColour` (`SU:25287-25317`, render about `MM:40540-40620`) | missing | missing |
| Signal type glyph PK / AVG / FFT | `ShowType` (`MM:28167-28190`) | missing | missing |
| Signal type selector | enabled on load (`SU:26020-26112`) | channel reading combo | partial |
| Peak value + colour | `SU:25287-25317`, `MM:28167-28190` | `showPeakValue`, no colour | partial |
| Update / attack / decay / history / ignore | defaults attack .8, decay .2, history 4000 (`MM:23001`) | channel fields incl. `historyMs`, `peakHold`, `historyColor` (`CPI:543-551`) | match |
| Low / High / Shadow / Peak hold | disabled on load (`SU:26020-26112`) | `peakHold` offered | Nereus-only |
| Background | `Colour` | `backdropColor` | match |
| Title text | not found as an editable title in the SIGNAL_TEXT group (`SU:25287-25317`) | `title`, `showTitle`, `titleColor` | Nereus-only |
| Font size | fixed 56 (`MM:23001`) | `fontSize` >= 8 | Nereus-only |
| Units / decimals | sub readings in fixed units | `CPE` note: "Signal units and decimal precision follow the global multimeter preferences." | partial |
| Fade RX/TX | `SU:25287-25317` | instant | partial |
| MMIO | `MM:28156-28165`, 1 variable | one pair | match |

Multislice notes:

* The render uses `IsAboveS9Frequency(_rx)` for the container RX (about `MM:40540-40620`). Same open point as the magic eye: confirm the S9 frequency follows the entry's slice.

### meter.historyGraph: HISTORY (`AddHistory` `MM:25585`)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Two traces (left and right axis) | `grpHistoryItem`, reading 0/1 (`SU:25018-25054`, load `SU:25676-25717`, apply `MM:26924-26990`) | single trace bound to SignalAvg (`CPI:299-304`, `paintHistory` `CPI:1155-1167`) | missing (second trace) |
| Per-axis reading | left/right reading combos | one channel reading combo | partial |
| Per-axis auto scale, min, max (-40000..40000) | `SU:25018-25054` | one `autoScale`, `min`, `max` (default -140/0) | partial |
| Show right axis | `SU:25018-25054` | missing | missing |
| Axis colours (red / yellow defaults) | `SU:25018-25054` | missing (one channel `color`) | partial |
| Lines colour, time colour | `SU:25018-25054` | grid uses `lowColor`; no time axis colour | partial |
| Background | `SU:25018-25054` | `backdropColor` | match |
| Vertical ratio 0.13-1 | `SU:25018-25054` | `faceHeight` | partial (different unit) |
| Update 50-10000 ms | `SU:25018-25054` | channel `updateIntervalMs` >= 1 | partial (range) |
| Keep for 1-1800 s | `SU:25018-25054` | `historyMs` default 60000, plus `capacity` 300 (`CPI:260-262`) | partial |
| Units | not found in `grpHistoryItem` | `units` | Nereus-only |
| Two MMIO buttons | `SU:25018-25054` | one pair | partial |
| Fade RX/TX | `SU:25018-25054` | instant | partial |
| Title | not found in `grpHistoryItem` | `title`, `showTitle`, `titleColor` | Nereus-only |

Multislice notes:

* Each Thetis axis reads from the container RX. NereusSDR's one trace follows the entry Source. A two-trace port would want each trace to pick its own slice, for example Slice A signal on the left and Slice B on the right. Needs a ruling.

### meter.vfoDisplay: VFO_DISPLAY (`AddVFODisplay` `MM:25751`); building block `VfoDisplayItem`

The preset is a composite with a `VfoDisplayItem` child (`CPI:305-307`). It does NOT use `VfoDisplayItemEditor`. The building block itself is audited by another scout.

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Display mode VFO A / VFO B / Both | stored in `HistoryDuration` (`SU:25318-25349`), applied `MM:28072` | building-block editor has it (`VfoDisplayItemEditor.cpp`); preset: missing (`CPI:553-567` exposes `vfo` only) | partial (the preset hides the mode control) |
| Frequency / mode / filter / band colours | reused fields (`MM:28052-28061`) | preset exposes all 4 | match |
| RX / TX label colours | `MM:28052-28070` | editor has them; preset missing | missing in preset |
| Digit highlight, split, band text, small digits, lock, sync colours | `MM:28063-28070` | missing in preset and (per the editor list) in the building block | missing |
| Background | `SU:25318-25349` | not exposed (`CPI:553-567` excludes backdrop for Vfo) | missing |
| Title | `SU:25318-25349` | not exposed | missing |
| Fade RX/TX | `SU:25318-25349` | `fadeRx/fadeTx` | partial (instant) |

Multislice notes:

* Thetis display mode is VFO A / VFO B / Both (`MM:28072`), meaning RX1/RX2. NereusSDR needs Slice A-D. The building-block editor still says VFO A/B. The preset shows "No live slice reading" when no slice feeds it (`CPI:306`, `CPI:420`). Needs: a slice picker (or the entry Source combo) wired to this face, and a ruling on what "Both" means with 4 slices.

### meter.clock: CLOCK (`AddClock` `MM:25815`); building block `ClockItem`

The preset is a composite with a `ClockItem` child (`CPI:312`). It does NOT use `ClockItemEditor`, but it exposes everything that editor has plus more.

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| 12 hr / 24 hr | `ShowMarker` reused (`SU:25357`, load `SU:26147`, apply `MM:28143`) | `show24Hour` | partial (12 h format "hh:mm:ss AP" vs Thetis "h:mm:ss" with a small am/pm, `MM:40474-40537`) |
| Time colour | `MarkerColour` (`SU:25350-25358`) | time colour | match |
| Date colour | `SubMarkerColour` | date colour | match |
| Title colour + show | `TitleColor`, `ShowType` | title colour, `showTitle` | match |
| Background | `Colour` | `backdropColor` | match |
| Local + UTC side by side | always both, titles "local"/"utc" (`MM:40474-40537`) | `clockMode` Both / UTC / Local, titles "Local"/"UTC" (`CPI:1220-1236`) | Nereus-only (modes) |
| Date format | "ddd d MMM yyyy", always shown | "yyyy-MM-dd", `showDate` toggle | partial (format) / Nereus-only (toggle) |
| Font size, face height | n/a | `fontSize`, `faceHeight` | Nereus-only |
| Fade RX/TX | none in CLOCK group (`SU:25350-25358`) | `fadeRx/fadeTx` | Nereus-only |
| Advanced bindings | n/a | disabled for clock (`CPE`) | match |

Multislice notes:

* None. The clock has no RX dependency.

### meter.contest: none (NereusSDR composite: VFO + band buttons + mode buttons + clock)

`THETIS-PROVENANCE.md:340` calls this a local adaptation. No `MeterType` exists for it (`MM:178-232`).

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| VFO child | n/a | `VfoDisplayItem` (`CPI:305-307`) | Nereus-only |
| Band / mode buttons | n/a | `bands`, `modes` (`CPI:308-311`, `CPI:553-567`) | Nereus-only |
| Clock child + clock fields | n/a | `CPI:312`, clock fields | Nereus-only |
| Background, show title, font size, face height, fade | n/a | base and non-Vfo fields (`CPI:553-567`) | Nereus-only |

Multislice notes:

* The band and mode buttons are disabled until a slice is attached (`CPI:309-310`). Which slice they act on follows the entry Source. Confirm that "Follow selected RX" is the intended default for a contest face.

### meter.sMeter: AVG signal bar (`AddSMeterBarSignal`/Avg `MM:22826`)

`SMeterPresetItem` = `BarPresetItem` with `configureVariant("SignalAvg")`. The table is the same as the bar family below, with these extras:

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Sub indicator (average marker under peak) | `SubIndicators` true for bars (`MM:29339`), colour + Show `SU:25511-25513` | average marker hard-coded `QColor(169,169,169)`, no toggle (`BPI` paint) | missing |
| Above-S9 scale | `IsAboveS9Frequency` | `setAboveS9Frequency` pushed to the preset | match (not verified per slice) |

Multislice notes:

* Same as the bar family. The S9 frequency must follow the entry's slice.

### meter.<variant> (BarPresetItem variants): bar MeterTypes

Mapping (variants listed at `BPI:58`, configured at `BPI:59-92`):

| NereusSDR variant | Thetis MeterType | Builder |
|---|---|---|
| Comp | COMP | `MM:25004` |
| Eq | EQ | `MM:24414` |
| Leveler | LEVELER | `MM:24502` |
| Cfc | CFC | `MM:24857` |
| CfcGain | CFC_GAIN | `MM:24943` |
| LevelerGain | LEVELER_GAIN | `MM:24588` |
| AlcGain | ALC_GAIN | `MM:24735` |
| AlcGroup | ALC_GROUP | `MM:24796` |
| Agc | AGC | `MM:23284` |
| AgcGain | AGC_GAIN | `MM:23222` |
| Signal | SIGNAL_STRENGTH | `MM:22822` |
| SignalAvg | AVG | `MM:22826` |
| SignalMaxBin | SIGNAL_MAX_BIN | `MM:22830` |
| Adc | ADC | `MM:23063` |
| AdcMax | ACG_MAX_MAG (sic) | `MM:22940`; unavailable in Nereus (`CCR:254`) |
| PbSnr | ESTIMATED_PBSNR | `MM:23151` |
| (none) | PWR | `MM:25177/25185`; no standalone variant |
| (none) | REVERSE_PWR | `MM:25181`; no standalone variant |
| (none) | SWR | `MM:25313`; no standalone variant |

The options table is the same for every variant. Thetis applies the default bar branch (`MM:28221-28336`).

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Indicator colour | `MarkerColour` (`MM:28271`) | `barColor` | match |
| Indicator Show | `ShowMarker` (`MM:28272`) | missing | missing |
| Sub indicator colour + Show | `MM:28281-28291` | hard-coded grey, no toggle | missing |
| Low / High scale colours | `MM:28303-28326` | hard-coded white / red | missing |
| Bar style | `MM:28270` | `style` | match |
| Seg/Solid low and high colours | `MM:28275-28276` | missing | missing |
| Solid colour | `Colour` (`MM:28294-28302`) | `backdropColor` (background only) | partial |
| Peak hold + colour | `MM:28273-28274` | `peakHold`; colour hard-coded red | partial |
| Peak value + colour | `MM:28277-28278` | `showPeakValue`; colour hard-coded red | partial |
| Show history + history colour | `SU:25518-25519` | `showHistory`, `historyColor` | match |
| Update / attack / decay / history / ignore | `SU:25515-25522` | same keys (`BPI`) | partial (ranges, see group table) |
| Title show + colour | `MM:28303-28326` | `label`, `titleColor`; no show toggle | partial |
| Unit | `MM:28279` | fixed per variant | n/a (Thetis fixes it per type too; only Custom edits it) |
| Fade RX/TX | `FadeCover` `MM:28327-28334` | missing on bars | missing |
| Shadow | disabled for bars | not offered | match |
| Row height, show value | n/a | `rowHeight` >= 72, `showValue` | Nereus-only |
| MMIO | `MeterVariables` 2 for MIC/ALC/EQ/LEVELER/COMP/CFC/ADC/AGC, else 1 (`MM:22684-22735`) | one pair per entry | partial |
| Secondary binding | the sub indicator reading | `secondaryBindingId` | match |

Multislice notes:

* Signal, SignalAvg, SignalMaxBin, Adc, Agc, AgcGain and PbSnr are RX readings from the container RX in Thetis. NereusSDR uses the entry Source (Slice A-D / Follow selected RX). ADC needs a ruling: an ADC is shared by slices on the same DDC, so "slice" is the wrong key. It may want "ADC of this slice".
* Comp, Eq, Leveler, Cfc, Alc* are TX readings of the single TX path. With a Core, the entry's Radio session decides. The Source combo is meaningless for them.

### meter.mic: MIC (`AddMicBar` `MM:24327`)

Same bar table as above. Specific defaults:

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Defaults | `AddMicBar` `MM:24327` | `configureAsMic` `BPI:43-48`: attack .8, release .1, history 2000, yellow marker, white low fill, red/128 history | partial (I did not confirm every Thetis default value line by line) |
| MMIO | 2 variables | one pair | partial |
| All other options | as the bar table | as the bar table | as the bar table |

Multislice notes:

* TX path only. One per radio session.

### meter.alc: ALC (`AddALCBar` `MM:24649`)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Defaults | `MM:24649` | `configureAsAlc` `BPI:49-53` | partial (same caveat as Mic) |
| MMIO | 2 variables | one pair | partial |
| All other options | as the bar table | as the bar table | as the bar table |

Multislice notes:

* TX path only.

### meter.customBar: CUSTOM_METER_BAR (`AddCustomBar` `MM:23374-23460`)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Readings | CUSTOM_PK / CUSTOM_AV from MMIO variables 0 and 1 (`MM:23374-23460`) | `bindingId`, `secondaryBindingId`, one MMIO pair | partial |
| Min / Max / High | -5000..5000, load defaults 0 / 10 / 7.5 (`SU:26323-26328`, save `SU:25498-25505`, apply `MM:28251-28258`) | `minValue`, `maxValue`, `redThreshold`; defaults -30 / 12 (constructor) | partial (defaults) |
| Units | default "?" | `units` default "dB" | partial (default) |
| Title | default "Title" | `label` default "Custom" | partial (default) |
| Attack / decay / history | .8 / .2 / 4000 (`MM:23374-23460`) | history 2000 | partial (default) |
| Colours | DarkCyan indicator, violet history | `m_marker` fill, red history | partial (defaults) |
| Settings / Custom radios | switch between preset and custom range (`SUD`) | not needed (always custom) | n/a |
| All other options | as the bar table | as the bar table | as the bar table |

Multislice notes:

* MMIO is client-wide in Thetis. NereusSDR's MMIO source/variable is per entry and can carry a Radio session. Nothing slice-specific.

### meter.spacer: SPACER; building block `SpacerItem`

The preset builds the building block directly (`CCR:85`) and uses `SpacerItemEditor`.

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Background RX | `Colour` (`SU:25436-25443`, `MM:27622`), shown in RX (`MM:36544-36576`) | "Color 1" (`SpacerItemEditor.cpp`), the gradient start (`SpacerItem.cpp:76-95`) | partial (gradient, not RX colour) |
| Background TX | `MarkerColour` (`MM:27623`), shown in TX with a cross-fade (`MM:36575`) | "Color 2", the gradient end | partial (no RX/TX swap; comment in `SpacerItem.cpp` claims Thetis RX/TX switching, but the code paints a gradient) |
| Vertical padding 0.001-10 | `SpacerPadding` (`MM:27624`) | Padding 0-1, 3 decimal places | partial (range) |
| Fade on RX / TX | `SU:25436-25443` | missing; only the Only-when-RX/TX hide | missing |
| Exposes the whole building-block editor | n/a | yes (it is the editor) | match |

Multislice notes:

* RX/TX colour swap keys off the one radio's MOX. With a Core, the radio session's TX state should drive it.

---

### Summary

Counts (missing / partial) per item. Nereus-only, match and n/a rows are not counted.

| Item | Missing | Partial |
|---|---|---|
| Item-group level (shared) | 3 | 11 |
| meter.radeMicWaveform | 0 | 0 |
| meter.radeSpeechWaveform | 0 | 0 |
| meter.powerSwr | 5 | 5 |
| meter.ananMulti | 3 | 4 |
| meter.crossNeedle | 3 | 2 |
| meter.magicEye | 3 | 2 |
| meter.signalText | 2 | 4 |
| meter.historyGraph | 2 | 9 |
| meter.vfoDisplay | 4 | 2 |
| meter.clock | 0 | 2 |
| meter.contest | 0 | 0 |
| meter.sMeter (extras) | 1 | 0 |
| meter.<variant> bars (each) | 5 | 7 |
| meter.mic | 0 | 2 |
| meter.alc | 0 | 2 |
| meter.customBar | 0 | 7 |
| meter.spacer | 1 | 3 |
| **Total** | **32** | **62** |

The five biggest gaps:

1. **History graph** is a single trace on SignalAvg. Thetis HISTORY has two traces with left/right axes, per-axis reading, scale and colour, a right-axis toggle, lines/time colours, keep-for and two MMIO slots.
2. **Bar colour set and fade.** Every bar (including sMeter, mic, alc, custom) lacks the Low/High scale colours, the sub indicator colour and Show toggle, the indicator Show toggle, peak hold and peak value colours, the Seg/Solid low/high colours, and Fade on RX/TX. Several of these colours are hard-coded in `BarPresetItem`.
3. **ANAN and Cross needles** lack Dark Mode, a per-item Power limit and the Power scale colour. Power scale is global only.
4. **Magic eye** lacks Eye Size, Bezel Size and the bezel image.
5. **Signal text and VFO.** Signal text lacks the µV/dBm sub readings and the PK/AVG/FFT glyph. The VFO preset hides the display-mode control and lacks RX/TX label, split, lock, sync, band text, background and title colours.

Open questions a human must rule on:

1. Keep per-channel needle settings (update, attack, decay, shadow, geometry) as a NereusSDR superset, or collapse to the Thetis group-wide model?
2. Keep the clock's NereusSDR extras (Both/UTC/Local modes, date toggle, ISO date, fade), or match Thetis's always-both "ddd d MMM yyyy" layout?
3. Add standalone PWR, REVERSE_PWR and SWR bar variants, or is the `meter.powerSwr` composite the intended replacement?
4. Spacer: keep the gradient, or port the Thetis RX/TX colour swap with cross-fade (and fix the misleading comment in `SpacerItem.cpp`)?
5. Fade: port the animated 0.5 s dim to 48/255, or keep the instant 25% opacity?
6. Multislice: what does VFO "Both" mean with Slice A-D, and should a two-trace history graph allow a different slice per trace?
7. ADC bar: should its source be the ADC behind the chosen slice rather than the slice itself?
8. Custom bar defaults: adopt Thetis 0 / 10 / 7.5, "?", "Title", DarkCyan, or keep -30 / 12 / "dB" / "Custom"?

Not verified (looked but did not confirm): image assets for ANAN/Cross faces; every Mic/ALC default value line by line; whether `setAboveS9Frequency` follows a non-default slice Source.

---

## Part: Audit B: building blocks, part 1

Scope: NereusSDR typeIds BAR, SOLID, IMAGE, SCALE, TEXT, NEEDLE, SPACER, FADECOVER, LED, HISTORY, MAGICEYE, NEEDLESCALEPWR, SIGNALTEXT (catalog at `src/gui/containers/ContainerContentRegistry.cpp:258-270`).

Thetis tree: `../Thetis/Project Files/Source/Console/` at v2.10.3.15. Short names below: `MM` = `MeterManager.cs`, `setup` = `setup.cs`, `designer` = `setup.designer.cs`, `common` = `common.cs`.

NereusSDR tree: this worktree. Short names: `ED/` = `src/gui/containers/meter_property_editors/`, `MI` = `src/gui/meters/MeterItem.{h,cpp}`, `CPE` = `src/gui/containers/ContentPropertyEditor.cpp`.

### How the two models differ (read this first)

* Thetis has no free-standing building blocks. The user adds a MeterType group (clsItemGroup) that a builder fills with clsMeterItems. One settings bag per group (`clsIGSettings`, MM:1433, properties MM:1878-1923) holds the user's choices. `setup.updateMeterType` (setup:24998) writes the bag and `ApplySettingsForMeterGroup` (MM:26908) pushes it onto the items. Which panel the user sees depends on the group type: `setupMMSettingsGroupBoxes` (setup:26691-26860) shows grpMeterItemSettings by default, grpMeterItemSpacerSettings for SPACER, grpLedIndicator for LED and grpHistoryItem for HISTORY.
* Many bag fields are reused under other meanings. Each reuse is noted in the rows below (LED is the worst case: offsets live in EyeScale and EyeBezelScale, sizes in AttackRatio and DecayRatio, condition text in Text1, and so on).
* NereusSDR places each block as its own entry. Each entry gets BaseItemEditor plus a type editor (dispatch at `src/gui/containers/ContainerSettingsDialog.cpp:2673-2725`), and CPE adds an "Advanced bindings" box per entry (CPE:397-413).
* Consequence: many Thetis controls are group-level and apply to "the primary bar" or "the sub needle". In NereusSDR the same effect needs a per-item control on whichever block plays that role. Rows below compare at that per-item level.

### Shared base (applies to all 13 typeIds)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Reading source | Fixed per MeterType by the builder; user picks only the signal flavor (ucMeterItemSignalType, MaxBin/Average) for ANANMM, MAGIC_EYE and SIGNAL_TEXT (setup:25520-25578, MM:27805-27893, MM:28147-28219). Reading enum MM:78-170: SIGNAL_STRENGTH 0 … ESTIMATED_PBSNR 7, MIC 8 … COMP_PK 21, CFC_PK 24, CFC_AV, CFC_G 26, REVERSE_PWR 27, SWR 28, DRIVE_FWD_ADC 29 … FWD_VOLT 37, VOLTS 38, AMPS 39, AZ 40, ELE 41, CUSTOM_PK 62, CUSTOM_AV 63, SIGNAL_MAX_BIN 72, ADC_MAX_MAG 73 | Free Reading combo on every block, `ED/BaseItemEditor.cpp:201`, list at `ED/BaseItemEditor.cpp:260-311` (27 bindings). Not listed: MIC_PK, ALC_PK, EQ_PK, LEVELER_PK, COMP_PK, CFC_PK (ids 113-118 exist in `src/gui/meters/MeterPoller.h:155-210` but are not offered), DRIVE_FWD_ADC … FWD_VOLT, CUSTOM_PK/AV, ADC_MAX_MAG. HwTemperature is Nereus-only | partial (PK, PA-internal, custom and ADC max mag readings not offered) |
| MMIO variable binding | Per item MMIOGuid / MMIOVariable / MMIOVariableIndex (MM:6625 base), bag arrays MM:1976-1990, buttons btnMMIO_variable / _2 (setup:25606-25670) | "Choose a value…" button `ED/BaseItemEditor.cpp:179`; MMIO source UUID and variable line edits CPE:412-413 | match |
| RX/TX source selection | Container-level only: radContainer_rx1_data / rx2_data (setup:24716-24727, 34480-34512) call SetContainerRX (MM:2837). Items read `getReading(rx, …)` (MM:5447) | Per-entry Source combo: Container default / Slice A-D / Follow selected RX (CPE:397-402) plus Radio session (CPE:412-413); container rxSource 1..4 = slice A-D (`src/gui/containers/ContainerWidget.h:242-249`) | Nereus-only (per-entry source is a superset) |
| Only when RX / Only when TX | clsMeterItem OnlyWhenRx / OnlyWhenTx MM:6709-6710, accessors MM:7116-7122; set by builders | `ED/BaseItemEditor.cpp:205-206`, honored in `src/gui/meters/MeterWidget.cpp:840-851` | match |
| Display group | clsMeterItem DisplayGroup (MM:6625 base) | `ED/BaseItemEditor.cpp:207` (0-15), persisted `src/gui/containers/ContainerContentRegistry.cpp:540-542` | match |
| Update interval | Per item UpdateInterval (default 5000, MM:6625 base); user value nudMeterItemUpdateRate 15-5000, default 100 (designer:55675) pushed to the primary item (MM:28220-28340) | missing (one global poll rate) | missing |
| Fade on RX / Fade on TX | Per item FadeOnRx / FadeOnTx (MM:6625 base) from chkMeterItemFadeOnRx/Tx (setup:25520-25578). `fade()` (MM:33153-33177) dims to 48 over 500 ms, and also dims when `mi.Disabled` (MM:6840) is set because the feature is off | missing on every block except FADECOVER (see below). MeterItem base has no FadeOnRx/Tx and no Disabled (MI.h:192-428) | missing |
| Position, size, Z-order | Fixed by the MeterType builder; not user-editable | X/Y/W/H and Z-order `ED/BaseItemEditor.cpp:149-152, 203` | Nereus-only |

Multislice notes:
* Thetis stores readings per rx 1/2 only (`getReading(rx, …)` MM:5447) and a container follows RX1 or RX2 (setup:24716-24727). NereusSDR needs slice A-D. Today: per-entry Source with Slice A-D or Follow selected RX (CPE:397-402), falling back to the container rxSource 1..4 (ContainerWidget.h:242-249). This part is ahead of Thetis.
* Thetis sends TX readings to whichever rx is under MOX (MM:5468-5540) and PA readings (DRIVE_FWD_ADC … CAL_FWD_PWR) to rx1 only (MM:5524-5533). NereusSDR needs TX readings tied to the transmitting slice and PA readings tied to the radio (Radio session / remote Core). Today the TX and PA binding ids carry no slice (MeterPoller.h:155-210); the Source combo has no effect on them.
* Thetis fade uses the one radio's MOX state (MM:33153-33177). NereusSDR needs "TX" to mean the radio that owns the entry's session, not any radio in the window. Today: no fade exists.

### BAR: Thetis clsBarItem (MM:21237), default bar meters

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Reading | Fixed by MeterType; see shared base | Base Reading combo | partial (shared row) |
| Update rate | nudMeterItemUpdateRate (designer:55675) to the primary bar (MM:28220-28340) | missing | missing |
| Attack ratio | nudMeterItemAttack 0-1, step 0.05, 3 decimals (designer:55634) | `ED/BarItemEditor.cpp:121` 0-1, step 0.01 | match |
| Decay ratio | nudMeterItemDecay (designer:55593) | `ED/BarItemEditor.cpp:131` | match |
| Bar style | BarStyle None / Line / SolidFilled / GradientFilled / Segments (MM:21237). UI: chkMeterItemSegmented gives Segments, chkMeterItemSolid gives SolidFilled, else Line (setup:25520-25578), stored in igs.BarStyle | Combo offers Filled and Edge only (`ED/BarItemEditor.cpp:145-149`); model enum Filled / Edge / Line (MI.h:471-695) | partial (no Segments, no Solid vs Line choice as Thetis means it, no Gradient; Edge is Nereus-only) |
| Low colour | clrbtnMeterItemSegSolidLow, igs.SegmentedSolidLowColour, to bar Colour (MM:28220-28340) | "Bar color" `ED/BarItemEditor.cpp:177` | match |
| High colour | clrbtnMeterItemSegSolidHigh, igs.SegmentedSolidHighColour, to bar ColourHigh (MM:28220-28340); switch point comes from the meter type's calibration (or the high value on CUSTOM_METER_BAR) | "Bar red color" `ED/BarItemEditor.cpp:196` plus a free "Red threshold" `:110` | partial (Nereus switch point is a user threshold) |
| Min / max | Fixed by calibration per MeterType; only CUSTOM_METER_BAR takes min/max/high (MM:28220-28340) | `ED/BarItemEditor.cpp:89, 99` | Nereus-only |
| Orientation | Not a user option (not found in setup:25520-25578) | `ED/BarItemEditor.cpp:77` | Nereus-only |
| Show indicator + indicator colour | chkMeterItemShowIndicator, clrbtnMeterItemIndicator to ShowMarker / MarkerColour (setup:25520-25578, MM:28220-28340) | Model holds marker fields, serialized MI.cpp:848-890; no editor row | partial (not editable) |
| Sub indicator | Sub bar takes MarkerColour = igs.SubMarkerColour and ShowMarker = igs.ShowSubMarker (MM:28220-28340) | No sub-bar role; a second BAR could stand in but has no marker editor | partial |
| History (show, colour with alpha, duration) | chkMeterItemHistory, clrbtnMeterItemHistory + alpha, nudMeterItemHistoryDuration 50-30000, default 2000 (designer:55001) | Model history fields serialized MI.cpp:848-890; no editor row | partial (not editable) |
| Ignore history duration | nudMeterItemIgnoreHistory 0-4000 (designer:55140) | missing | missing |
| Peak hold + marker colour | chkMeterItemPeakHold, clrbtnMeterItemPeakHold to PeakHold / PeakHoldMarkerColour | Model has peakHoldDecayRatio (not a bool) and marker colour; no editor row | partial |
| Peak value + colour | chkMeterItemPeakValue, clrbtnMeterItemPeakValue to ShowPeakValue / PeakValueColour (MM:28220-28340) | Model showPeakValue serialized MI.cpp:848-890; no editor row | partial (not editable) |
| Show value text | clsBarItem ShowValue, rendered MM:37608-37670; set by builders, no setup control found (setup:25520-25578) | Model showValue / fontColour, no editor row | n/a (neither side exposes it) |
| Units | clsBarItem Unit DBM / S_UNTS / U_V; never set from the UI (MM:28220-28340 copies igs.Unit, which no control writes) | Global multimeter units | n/a |
| Power limit | nudMeterItemPowerLimit 0.1-5000 (designer:55302) to MaxPower for PWR / REVERSE_PWR (MM:28220-28340) | `setPowerScale` acts only on objects named PowerBar / PowerScale (MI.h:305-320); no editor row | partial |
| Edge bg / low / high / avg colours | none | `ED/BarItemEditor.cpp:217-271` | Nereus-only |
| S-unit value text above S9 on VHF | `IsAboveS9Frequency(_rx)` (MM:5426-5429) in the value render (MM:37608-37670) | `MeterItem::formatValue` fixes S9 at -73 dBm, no VHF branch (MI.cpp:250-280) | partial |
| Fade on RX / TX | Primary bar fade (MM:28220-28340) | missing | missing |

Multislice notes:
* `IsAboveS9Frequency(_rx)` reads `_rx1VHForAbove` / `_rx2VHForAbove` (MM:5426-5429). NereusSDR needs the VHF test from the bound slice's frequency. Today: none (MI.cpp:250-280).
* Power limit for PWR / REVERSE_PWR assumes one transmitter (PA readings rx1 only, MM:5524-5533). NereusSDR needs it per radio session. Today: name-matched PowerBar only (MI.h:305-320).
* Bar signal readings use `getReading(rx, …)` (MM:5447) with rx from the container radio. NereusSDR: per-entry Source works today.

### SOLID: Thetis clsSolidColour (MM:7657)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Colour | clrbtnMeterItemHBackground to igs.Colour to SOLID Colour (MM:28220-28340); class default Black (MM:7657) | `ED/SolidColourItemEditor.cpp:48`; default #0f0f1a (MI.h:429) | partial (default differs) |
| Fade on RX / TX | SOLID takes fade in ANANMM / CROSS groups (MM:27895-28035) | missing | missing |

Multislice notes:
* No RX1/RX2 assumption beyond fade, which uses the one radio's MOX (MM:33153-33177). NereusSDR needs the entry's radio. Today: no fade.

### IMAGE: Thetis clsImage (MM:16025)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Image | ImageName set by builders (for example "eye-bezel" in AddMagicEye MM:23572-23615); no free path control found in updateMeterType (setup:24998-25578) | Path + Browse `ED/ImageItemEditor.cpp:47` | Nereus-only |
| Dark mode | chkMeterItemDarkMode for ANANMM / CROSS (setup:25520-25578) to the primary image DarkMode (MM:27895-28035); render loads the "-dark" file (MM:40629-40680) | missing | missing |
| Size variants | Render picks "-small" (diagonal 450 or less) and "-large" (1200 or more) files (MM:40629-40680) | missing | missing |
| Clip rectangle / ellipse | ClipTopLeft, ClipSize, Clipped, ClipEllipse (MM:16025); builder-set only | missing | n/a (not user-editable; only matters for ported faces) |
| Fade on RX / TX | Image fades with the group (MM:27895-28035) | missing | missing |

Multislice notes:
* None. Image items read no radio state except fade (one MOX, MM:33153-33177).

### SCALE: Thetis clsScaleItem (MM:16105)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Low colour / high colour | clrbtnMeterItemLow / High to igs.LowColor / HighColor to Scale Low/HighColour and FontColourLow/High (MM:28220-28340) | Model has low/high colours (MI.h:696); editor shows Tick color and Label color instead (`ED/ScaleItemEditor.cpp:204, 221`) | partial |
| Show type title | igs.ShowType to ShowType (MM:28220-28340) | `ED/ScaleItemEditor.cpp:238` | match |
| Title colour | clrbtnMeterItemTitle, igs.TitleColor to FontColourType (MM:28220-28340) | `ED/ScaleItemEditor.cpp:265` | match |
| Min / max / ticks | Fixed by meter type calibration; CUSTOM_METER_BAR adds min, max, high, units and title (MM:28220-28340) | Min/Max `ED/ScaleItemEditor.cpp:148, 158`, major/minor ticks `:169, 179` | Nereus-only (Thetis only edits these on Custom bar) |
| Custom bar units and title text | CUSTOM_METER_BAR takes units and title from the bag (MM:28220-28340) | missing on SCALE (covered by meter.customBar, out of this group) | n/a (see audit A) |
| Orientation | Not a user option | `ED/ScaleItemEditor.cpp:145` | Nereus-only |
| Font family / style / size | FontFamily, FontStyle, FontSize 20 (MM:16105); builder-set, no setup control found | Font size `ED/ScaleItemEditor.cpp:224` | Nereus-only |
| Fade on RX / TX | Not found in the SCALE apply path (MM:28220-28340) | missing | n/a |

Multislice notes:
* Signal scales do not switch labels for VHF in the item itself (no IsAboveS9Frequency use found in clsScaleItem, MM:16105-16207). No slice assumption.

### TEXT: Thetis clsText (MM:22251)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Text with tokens | Text supports %value%, %valueWithType%, %group% (render MM:37357-37387) | Label + suffix + decimals (`ED/TextItemEditor.cpp:47, 94, 104`); no tokens | partial |
| Colour | Colour (MM:22251) | `ED/TextItemEditor.cpp:70` | match |
| Font family | FontFamily (MM:22251) | missing | missing |
| Font style | Style (MM:22251) | Bold only `ED/TextItemEditor.cpp:83` | partial |
| Font size | FontSize, default 9 (MM:22251) | `ED/TextItemEditor.cpp:73` (4-128) | match |
| Centre | Centre, default false (MM:22251) | Always centered (MI.cpp:1389-1417) | partial |
| Update interval | int.MaxValue (MM:22251) | n/a | n/a |
| Idle text, min valid | none | `ED/TextItemEditor.cpp:115, 125` | Nereus-only |
| Fade on RX / TX | TEXT takes fade in ANANMM / CROSS (MM:27895-28035) | missing | missing |

Note: Thetis builders set text items; the user-facing text comes from igs.Text1 / Text2 for specific types (MM:1878-1923). No free TEXT block exists in Thetis.

Multislice notes:
* %value% resolves against the item's reading for its rx (MM:5447). NereusSDR has no token so nothing to scope. If tokens are added they must use the entry's Source.

### NEEDLE: Thetis clsNeedleItem (MM:21900), ANANMM / CROSS needles

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Reading | Fixed by builder; ANANMM signal needle takes ucMeterItemSignalType (setup:25520-25578) | Base Reading combo + Source label `ED/NeedleItemEditor.cpp:291` | partial (shared row) |
| Update rate | to primary needle (MM:27895-28035) | missing | missing |
| Attack / decay | to primary needle (MM:27895-28035) | `ED/NeedleItemEditor.cpp:321, 333` | match |
| Needle colour | igs.MarkerColour (Indicator) to primary needle Colour (MM:27895-28035) | `ED/NeedleItemEditor.cpp:309` | match |
| Sub needle colour | igs.SubMarkerColour to sub needles (MM:27895-28035) | No sub role; each NEEDLE has its own colour | partial |
| History (show, duration, colour) | chkMeterItemHistory, nudMeterItemHistoryDuration, history colour to primary needle (MM:27895-28035) | Editor rows `ED/NeedleItemEditor.cpp:446-476`; serialized MI.cpp:2046-2128 but never painted | partial (stored, not drawn) |
| Ignore history | nudMeterItemIgnoreHistory (designer:55140) | missing | missing |
| Shadow | chkMeterItemShadow to Shadow, default true (MM:21900, MM:27895-28035) | Hard-coded shadow (MI.cpp:1670) | missing (no toggle) |
| Peak hold + colour | ANANMM signal and PWR needles only (MM:27895-28035); disabled for CROSS (setup:26300-26430) | Peak marker hard-coded (MI.cpp:1823, 1864) | missing (no toggle) |
| Power limit | MaxPower (MM:27895-28035) | `ED/NeedleItemEditor.cpp:505`; stored, not used in paint (MI.cpp:2046-2128) | partial |
| Normalise to 100 W | NormaliseTo100W (MM:6625 base), builder-set | `ED/NeedleItemEditor.cpp:493` | match |
| Direction, offset, radius, length, stroke | NeedleDirection, Offset, RadiusRatio, LengthFactor, StrokeWidth (MM:21900); builder-set | `ED/NeedleItemEditor.cpp:351-429` | Nereus-only (editable) |
| Placement, Style, ScaleStrokeWidth, PeakNeedleShadowFade | MM:21900; builder-set | missing | n/a (not user-editable) |
| Calibration table | builder-set | `ED/NeedleItemEditor.cpp:511` | Nereus-only (editable) |
| Fade on RX / TX | primary needle fade (MM:27895-28035) | missing | missing |

Multislice notes:
* PWR needle MaxPower and PA readings assume one transmitter on rx1 (MM:5524-5533). NereusSDR needs the radio session's PA. Today: power fields not used.
* Signal needle reads `getReading(rx, …)` (MM:5447). Today: per-entry Source works.

### SPACER: Thetis clsSpacerItem (MM:17436)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| RX colour | clrbtnMeterItemHBackgroundSpacerRX to igs.Colour to Colour1 (setup:25438-25445, MM:27608-27650), default 32,32,32 (MM:17436) | Color 1 `ED/SpacerItemEditor.cpp:55`, default 0x20 (`src/gui/meters/SpacerItem.h:84`) | partial (painted as gradient top, not as the RX colour) |
| TX colour | clrbtnMeterItemHBackgroundSpacerTX to igs.MarkerColour to Colour2 (same cites) | Color 2 `ED/SpacerItemEditor.cpp:74` | partial (gradient bottom, not TX) |
| RX/TX switch | renderSpacer crossfades Colour1 to Colour2 on MOX (MM:36544) | Vertical gradient when colours differ (`src/gui/meters/SpacerItem.cpp` paint) | missing |
| Padding | nudMeterItemSpacerPadding 0.001-10 (designer:63168) to igs.SpacerPadding to Padding; sets the item height (MM:27608-27650) | `ED/SpacerItemEditor.cpp:77` 0-1; stored, not used (SpacerItem.h:87) | partial |
| Fade on RX / TX | chkMeterItemFadeOnRxSpacer / TxSpacer (setup:25438-25445) | missing | missing |

Multislice notes:
* RX/TX switch uses the one radio's MOX (MM:36544). NereusSDR needs the MOX of the radio that owns the entry. Today: no switch.

### FADECOVER: Thetis clsFadeCover (MM:7691)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Placement | Added on top of a group, ZOrder int.MaxValue (MM:7691); no own properties | User-placed block (allocation `ContainerContentRegistry.cpp:100`) | Nereus-only (placement) |
| Fade on RX / TX | Driven by the group's FadeOnRx/Tx through fade() (MM:33153-33177) | `ED/FadeCoverItemEditor.cpp:42, 51`. BUG: `FadeCoverItem::setTxState` (`src/gui/meters/FadeCoverItem.cpp:84`) has no caller in src, and m_active defaults to false, so a fade cover never paints | partial (stored, never drawn) |
| Colour and alpha | Fills with the container BackgroundColour at alpha 255 minus fade (MM:37826-37846) | Color 1 / Color 2 / Alpha `ED/FadeCoverItemEditor.cpp:77, 96, 99` | Nereus-only |
| 500 ms ramp to 48 | fade() (MM:33153-33177) | missing | missing |
| Dims when feature disabled | fade() also dims on mi.Disabled (MM:6840, MM:33153-33177) | missing | missing |
| Inflate 1%, minimum 2 px | renderFadeCover (MM:37826-37846) | missing | missing |

Multislice notes:
* fade() uses the one radio's RX/TX state (MM:33153-33177). NereusSDR needs the entry's radio session. Today: no caller feeds TX state at all (FadeCoverItem.cpp:84).

### LED: Thetis clsLed (MM:20768), grpLedIndicator

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Condition | txtLedIndicator_condition, stored in igs.Text1 (setup:25359-25393, MM:27421-27512); evaluated by MeterScriptEngine (MM:20768) | Green / amber / red thresholds `ED/LedItemEditor.cpp:195-213` | missing (threshold model instead) |
| Variable picker, valid label | btnLedIndicatorVarPicker, lblLed_Valid (grpLedIndicator, designer) | missing | missing |
| True colour | clrbtnLedIndicator_true, igs.Colour (MM:27421-27512) | `ED/LedItemEditor.cpp:102` | match |
| False colour | clrbtnLedIndicator_false, igs.MarkerColour | `ED/LedItemEditor.cpp:118` | match |
| Copy true/false colours | btnLedIndicator_copy_truefalse_colours | missing | missing |
| Panel back RX | clrbtnLedIndicator_PanelBackground, igs.TitleColor to PanelBackColour1 | `ED/LedItemEditor.cpp:144` | match |
| Panel back TX | clrbtnLedIndicator_PanelBackgroundTX, igs.HistoryColor to PanelBackColour2 | `ED/LedItemEditor.cpp:160` | match |
| Show panel | chkLedIndicator_ShowPanel, igs.ShowSubMarker | `ED/LedItemEditor.cpp:121` | match |
| Offset X / Y | nudLedIndicator_xOffset / yOffset, -10 to 10 step 0.001 (designer:63950), stored in igs.EyeScale / EyeBezelScale | missing | missing |
| Size X / Y + copy X to Y | nudLedIndicator_xSize / ySize (designer:63888), stored in igs.AttackRatio / DecayRatio; btnLedIndicator_copy_sizex_to_y | missing | missing |
| Update interval | nudLedIndicator_UpdateInterval 50-10000 (designer:63730) | missing | missing |
| Padding | nudLedIndicator_PanelPadding 0.001-10 (designer:63981), igs.SpacerPadding | `ED/LedItemEditor.cpp:183` 0-50 | partial (units and range differ) |
| Light mode | radLed_light_on_off / blink / pulsate, stored as igs.IgnoreHistoryDuration 0/1/2 (MM:27421-27512) | Blink and Pulsate checkboxes `ED/LedItemEditor.cpp:165, 174`; both can be on; frame-counter timing | partial |
| Show true / show false | chkLed_show_true / false, stored in igs.PeakHold / ShowMarker | missing | missing |
| No TX when true / false | chkLed_notx_true / false, keys led_notx_true / led_notx_false; Update calls StopAllTx when matched (MM:21177-21205) | missing | missing |
| Process when hidden | chkLed_process_when_hidden, key led_process_when_hidden (UpdateAlways) | missing | missing |
| 4-char id + copy | txtLedIndicator_4char, key led_4char (setup:26151-26194), btnLedIndicator_4char_copy | missing | missing |
| Fade on RX / TX | chkLedIndicator_FadeOnRX / TX (setup:25359-25393) | missing | missing |
| Shape / style | Fields exist (SQUARE / ROUND / TRIANGLE, FLAT / THREE_D) but are never set by setup or apply and never drawn; renderLed always uses FillRectangle (MM:34010-34130) | `ED/LedItemEditor.cpp:62-78`, rendered | Nereus-only |

Multislice notes:
* NoTx calls StopAllTx on the one radio (MM:21177-21205). NereusSDR needs it to stop the transmitting slice's radio, or the entry's radio session on a remote Core. Today: not implemented.
* Panel back RX/TX switches on the one radio's MOX (MM:34010-34130). Today: Nereus panel colours exist; which radio's TX drives them was not traced.
* Condition variables come from MeterScriptEngine; whether they name rx1/rx2 readings was not traced (looked at MM:20768-21236 only).

### HISTORY: Thetis clsHistoryItem (MM:17469), grpHistoryItem

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Reading 0 | history_reading_0 combo built by initComboHistoryReadings0: readings 0..73 minus 22, 23 and 29-71, sorted by name (setup:32239-32261) | Base Reading combo (`ED/BaseItemEditor.cpp:260-311`) | partial (list differs) |
| Reading 1 | history_reading_1, same list (MM:26924-26990) | "Axis 1 shows", 15 entries (`ED/HistoryGraphItemEditor.cpp:138-176`) | partial |
| MMIO per axis | btnMMIO_variable_history / _2, Axis0/1 MMIO (MM:26924-26990) | One MMIO per entry (CPE:412-413) | partial (axis 1 has none) |
| Vertical ratio | history_vertical_ratio 0.130-1 (designer:62565) | missing | missing |
| Background colour | history_background_colour | missing | missing |
| Update interval | history_update 50-10000 (designer:62535) | missing | missing |
| Keep for | history_keep_for, UI 1-1800 s (designer:62505), apply clamps 1-86400 (MM:26924-26990) | Capacity 10-3000 samples `ED/HistoryGraphItemEditor.cpp:52`; duration is global (`src/gui/setup/pages/MultimeterPage.cpp:296, 454`) | partial |
| Auto scale 0 / 1 | history_auto_scale_0 / 1 | `ED/HistoryGraphItemEditor.cpp:104, 113` | match |
| Manual min / max per axis | history_min/max_0/1, UI -40000 to 200 (designer:62474), apply ±40000 | missing | missing |
| Show scale 1 | history_show_scale_1 | `ED/HistoryGraphItemEditor.cpp:131` (plus axis 0 at `:122`, Nereus-only) | match |
| Line colours 0 / 1 | history_colour_0 / 1 | `ED/HistoryGraphItemEditor.cpp:76, 92` | match |
| Grid lines colour, time colour | history_colour_lines / time | missing | missing |
| Show grid | none | `ED/HistoryGraphItemEditor.cpp:95` | Nereus-only |
| Fade on RX / TX | chkHistory_fade_rx / tx | missing | missing |

Multislice notes:
* Both axes read `getReading(rx, …)` for the container rx (MM:5447). NereusSDR needs each axis to follow a slice, possibly different ones (for example slice A signal vs slice B signal). Today: axis 0 follows the entry Source; axis 1's binding has no source of its own.

### MAGICEYE: Thetis clsMagicEyeItem (MM:17175)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Signal type | ucMeterItemSignalType: MaxBin / Average pick the reading (MM:27805-27893), default AVG_SIGNAL_STRENGTH (MM:23572-23615) | Base Reading combo (includes "RX: Signal Max Bin", `ED/BaseItemEditor.cpp:284`) | match |
| Eye colour | igs.MarkerColour (Indicator) to Colour, default Lime (MM:27805-27893) | Glow color `ED/MagicEyeItemEditor.cpp:53` | match |
| Eye scale | nudMeterItemEyeScale 0.01-1 (designer:55512) | missing | missing |
| Bezel scale | nudMeterItemEyeBezelScale 0.01-1 (designer:55444) | missing | missing |
| Bezel image | Fixed "eye-bezel" (MM:23572-23615) | Path `ED/MagicEyeItemEditor.cpp:68` | Nereus-only |
| Update / attack / decay | Attack 0.2, decay 0.05 (MM:23572-23615), user-editable in grpMeterItemSettings | Fixed symmetric alpha 0.3, no editor | partial (defaults differ, not editable) |
| History | ShowHistory, history 500 ms, ignore 2000 (MM:17175) | missing | missing |
| Calibration | -127 dBm to 0, -73 to 0.85, -13 to 1 (MM:23572-23615) | Linear -127 to -13 dBm | partial |
| VHF offset | -20 dB VHF calibration shift (MM:23572-23615) | missing | missing |
| Fade on RX / TX | from bag | missing | missing |

Multislice notes:
* VHF shift depends on the rx's band (MM:5426-5429). NereusSDR needs it from the bound slice's frequency. Today: none.

### NEEDLESCALEPWR: Thetis clsNeedleScalePwrItem (MM:16208)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Scale colour | clrbtnMeterItemPowerScaleColour, igs.PowerScaleColour to LowColour (MM:27895-28035) | Low color `ED/NeedleScalePwrItemEditor.cpp:228` | match |
| High colour | HighColour (MM:16208); builder-set | `ED/NeedleScalePwrItemEditor.cpp:246` | Nereus-only (editable) |
| Max power | nudMeterItemPowerLimit 0.1-5000 (designer:55302) to MaxPower | `ED/NeedleScalePwrItemEditor.cpp:309`, 1-10000 | partial (range) |
| Dark mode | chkMeterItemDarkMode (MM:27895-28035) | `ED/NeedleScalePwrItemEditor.cpp:320` | match |
| Font family / size / bold | FontFamily, FontStyle, FontSize (MM:16208); builder-set | `ED/NeedleScalePwrItemEditor.cpp:260-274` | Nereus-only (editable) |
| Marks, show markers | Marks, ShowMarkers (MM:16208); builder-set | `ED/NeedleScalePwrItemEditor.cpp:286, 297` | Nereus-only (editable) |
| Direction, offset, length factor, calibration | Direction, NeedleOffset, LengthFactor (MM:16208); builder-set | `ED/NeedleScalePwrItemEditor.cpp:347-452` | Nereus-only (editable) |
| Radius ratio, placement | RadiusRatio, Placement (MM:16208); builder-set | missing from model (`src/gui/meters/NeedleScalePwrItem.h:72-139`) | n/a (not user-editable) |

Multislice notes:
* MaxPower and the PWR scale assume one transmitter (PA readings rx1 only, MM:5524-5533). NereusSDR needs the radio session's PA ceiling. Today: one value per item, no session link.

### SIGNALTEXT: Thetis clsSignalText (MM:21606)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Signal type | ucMeterItemSignalType, MaxBin / Average (setup:25287-25319, MM:28147-28219) | Base Reading combo | match |
| Background colour | clrbtnMeterItemHBackground, igs.Colour to the group's solid background (MM:28147-28219) | missing (a separate SOLID can stand in) | missing |
| Main text colour | Indicator, igs.MarkerColour to FontColour (MM:28147-28219) | Color `ED/SignalTextItemEditor.cpp:136` | match |
| Type icon + colour | ShowType + TitleColor to TypeColour; draws PK / AVG / FFT (render MM:40538) | Show type `ED/SignalTextItemEditor.cpp:103` draws a units label (`src/gui/meters/SignalTextItem.cpp` ~314-330); no type colour | partial |
| Sub line (µV and dBm) + colour | ShowSubMarker + SubMarkerColour to HistoryColour (MM:28147-28219, render MM:40538) | missing ("Marker color" `:168` has another use) | missing |
| Peak value + colour | PeakValue / PeakValueColour (setup:25287-25319) | `ED/SignalTextItemEditor.cpp:94, 152`; shown only when Peak hold is also on | partial |
| Peak hold | PeakHold field (MM:21606), not set by the SIGNAL_TEXT apply block (MM:28147-28219) | `ED/SignalTextItemEditor.cpp:112` | Nereus-only |
| Update / attack / decay / history / ignore | history and ignore saved (setup:25287-25319) and applied (MM:28147-28219) | missing | missing |
| Units | S-units text via SMeterFromDBM_Spaceless (common:834), +dB over S9 | Units combo `ED/SignalTextItemEditor.cpp:75` plus global units note (CPE:351) | Nereus-only (combo) |
| Font, size | Font fixed by builder | `ED/SignalTextItemEditor.cpp:172, 181` | Nereus-only |
| Bar style | Render draws no bar (MM:40538) | `ED/SignalTextItemEditor.cpp:192-197` | Nereus-only |
| S9 on VHF | IsAboveS9Frequency(_rx) in render (MM:40538, MM:5426-5429) | fixed -73 (MI.cpp:250-280) | partial |
| Fade on RX / TX | from bag | missing | missing |

Multislice notes:
* S-unit text uses `IsAboveS9Frequency(_rx)` from `_rx1VHForAbove` / `_rx2VHForAbove` (MM:5426-5429). NereusSDR needs the bound slice's frequency. Today: none.
* Reading follows the container rx (MM:5447). Today: per-entry Source works.

### Summary

Counts (rows marked missing / partial):

| typeId | missing | partial |
| --- | --- | --- |
| Shared base | 2 | 1 |
| BAR | 3 | 10 |
| SOLID | 1 | 1 |
| IMAGE | 3 | 0 |
| SCALE | 0 | 1 |
| TEXT | 2 | 3 |
| NEEDLE | 5 | 4 |
| SPACER | 2 | 3 |
| FADECOVER | 3 | 1 |
| LED | 11 | 2 |
| HISTORY | 6 | 4 |
| MAGICEYE | 5 | 2 |
| NEEDLESCALEPWR | 0 | 1 |
| SIGNALTEXT | 4 | 3 |
| Total | 47 | 36 |

Five biggest gaps:
1. Fade is broken or absent. FADECOVER never paints because `FadeCoverItem::setTxState` (FadeCoverItem.cpp:84) has no caller, and no other block has Fade on RX/TX, the 500 ms ramp or the feature-disabled dimming (MM:33153-33177).
2. LED uses thresholds instead of the Thetis condition script. Missing: condition and variable picker, No TX true/false (StopAllTx, MM:21177-21205), show true/false, offset and size, update interval, 4-char id, process when hidden.
3. The BAR editor hides most of the bar model: show marker, sub marker, history, peak hold, peak value and power limit. Segments and Gradient styles are missing, as are ignore-history and per-item update rate.
4. HISTORY lacks manual axis min/max, background / lines / time colours, vertical ratio, per-item keep-for and update rate, axis 1 MMIO, and the full reading list (setup:32239-32261).
5. S-unit readouts lack the per-RX VHF S9 rule (MM:5426-5429), and render semantics drift: needle history and power are stored but not drawn, magic eye calibration and smoothing differ, and signal text lacks the µV/dBm sub line and the PK/AVG/FFT type icon. The reading list also lacks the *_PK readings.

Open questions for a human:
1. Keep or drop the Nereus-only extras: LED shape, style and thresholds; history grid; signal text units combo, peak hold and bar style; fade cover colours and alpha; free image path?
2. Should Thetis group-level options (sub marker colour, sub needle colour, fade, update rate) become per-block controls on each NereusSDR entry, or a container-level settings bag like clsIGSettings?
3. Where should the per-slice VHF S9 rule live: in `MeterItem::formatValue` using the entry's Source slice frequency, or in the poller that supplies the reading?
4. LED No TX: should it stop only the radio that owns the entry's session (including a remote Core), or every transmitting radio?
5. Fade cover in Thetis is automatic per group; should NereusSDR keep it as a user-placed block or add it automatically on top of each container?

---

## Part: Audit C: Building blocks, part 2

Scope: NereusSDR typeIds CLOCK, VFO, DIAL, TEXTOVERLAY, WEBIMAGE, FILTERDISPLAY, ROTATOR, CLICKBOX, DATAOUT, VOICERECPLAY.

Read-only audit. Thetis tree `../Thetis` at v2.10.3.15 (`3759d096`). Thetis cites are relative to `Project Files/Source/Console/`. NereusSDR cites are relative to the worktree root (`src/...`).

### Facts that apply to the whole group

How Thetis wires these items:

* Every item has a settings group box in Setup > Appearance > Multimeter. `setupMMSettingsGroupBoxes` (setup.cs:26691) shows the right box per MeterType (clock case :26724, VFO :26719, text overlay :26739, data out :26744, dial :26759, web image :26764, rotator :26750).
* The controls are saved into a shared settings bag, `clsIGSettings` (igs), in `updateMeterType` (setup.cs:~25012). They are loaded back in `updateItemSettingsControlsForSelected` (setup.cs:~25596).
* `MeterManager.ApplySettingsForMeterGroup` (MeterManager.cs:~27500-28200) pushes igs onto items. `GetSettingsForMeterGroup` (~28700-29200) reads it back.
* igs fields are reused per type, so the user-visible meaning of each field is given per item below.

How NereusSDR edits and saves these legacy typeIds:

* They are edited through `ContentPropertyEditor` (src/gui/containers/ContentPropertyEditor.cpp:362-370), which embeds the item's `BaseItemEditor` subclass.
* Common rows come from BaseItemEditor (src/gui/containers/meter_property_editors/BaseItemEditor.cpp:147-207): X/Y/W/H, "Choose a value…" reading binding, Z-order, "Only when RX", "Only when TX", "Display group" 0-15.
* Advanced bindings (ContentPropertyEditor.cpp:395-420): Source combo (container default, Slice A-D, "Follow selected RX"), "Radio session", "MMIO source UUID" and "MMIO variable" as free text.
* Saving goes through `ContainerContentRegistry::captureMeterItem` (src/gui/containers/ContainerContentRegistry.cpp:500-525), which stores only what `item->serialize()` writes. **Any property that serialize() leaves out is lost on save and reload.** This causes three real data-loss bugs (CLOCK, VFO, TEXTOVERLAY), flagged below.

Fade vs "Only when":

* Thetis "Fade on RX / Fade on TX" draws a `clsFadeCover` over the item. It paints the container background colour at alpha `255 - fade(...)`, so the item is dimmed, not removed (renderFadeCover, MeterManager.cs:37826-37841).
* NereusSDR "Only when RX / Only when TX" hides the item (src/gui/meters/MeterWidget.cpp:846-848). That ports Thetis `OnlyWhenRX/OnlyWhenTX` (MeterManager.cs:32741), which is a different property and is not in the Setup UI for these items.
* So none of the legacy NereusSDR items has a Thetis-style fade. The composite presets do: CompositePresetItem uses opacity 0.25 (src/gui/meters/presets/CompositePresetItem.cpp:1181).

MMIO identity:

* Thetis refers to MMIO nodes by a 4-character ID (`MultiMeterIO.GuidfromFourChar`, for example setup.cs:25261).
* NereusSDR has no 4-char concept. A grep for "FourChar"/"fourChar" in src/ hits only src/core/cat/CatGlobalCommands.cpp. The user types a raw UUID instead.

Unbuilt gating (ContainerContentRegistry.cpp:307-311; MeterWidget.cpp:821-835):

* FILTERDISPLAY → `UnbuiltFeature::ContainerFilterDisplay`. `isBuilt()` returns true unconditionally for it (src/core/UnbuiltFeatureList.cpp:185; header comment "Feed implemented; gate awaits loaded FFT startup acceptance", UnbuiltFeatureList.h:111). **So it is effectively built.**
* CLICKBOX → `ContainerClickBox`: unbuilt ("Container click box has no host action routing", UnbuiltFeatureList.cpp:155-156).
* VOICERECPLAY → `Voice`: unbuilt.

Overlap: audit E covers `longpath.frequency` (VFO) and `longpath.filter` (filter display) against the same Thetis classes. This file covers the legacy `VFO` and `FILTERDISPLAY` typeIds.

---

### CLOCK: MeterType.CLOCK / clsClock

Thetis Setup group:

* `grpMeterItemClockSettings` is declared at setup.designer.cs:4199, laid out at :66950-66966, Text "Clock Settings".
* It holds exactly ten controls (:66952-66961): lblMMClockBackground, clrbtnMMClockBackground, labelTS164 "Date:", clrbtnMMDate, labelTS162 "Time:", clrbtnMMTime, clrbtnMMClockTitle, chkMMClockTitle, radMM24Clock, radMM12Clock.
* There are no fade, font or update-rate controls.

Thetis wiring:

* Save: setup.cs:25350-25358. Load: :26140-26150. Handlers: :26865-26899, all of which call updateMeterType.
* Item group built by AddClock (MeterManager.cs:25815-25859): two `clsSolidColour` halves at (32,32,32), one `clsClock` spanning the full width, and a fade cover.
* Apply: :28117-28145. Get: :29178-29205. Render: renderClock :40474-40528.

NereusSDR has two clocks:

* The legacy typeId `CLOCK`: src/gui/meters/ClockItem.{h,cpp} and src/gui/containers/meter_property_editors/ClockItemEditor.cpp.
* The composite preset `meter.clock`: CompositePresetItem Face::Clock, src/gui/meters/presets/CompositePresetItem.cpp.

The table covers both.

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Background colour | "Background:" clrbtnMMClockBackground (designer :63052, designer default LimeGreen, tooltip "Background colour"). Saved to **igs.Colour** (setup.cs:25352). Applied to both clsSolidColour halves (MeterManager.cs:28126-28128). AddClock creates the two halves at (32,32,32) (:25821-25833). | Legacy CLOCK: no background fill and no control (ClockItem.cpp:77-114). Composite: `backdropColor` #ff202020 (CompositePresetItem.cpp:260), editable. | partial (composite only; legacy CLOCK has none) |
| Time colour | "Time:" clrbtnMMTime (:63082, tooltip "Time"). Saved to **igs.MarkerColour** (setup.cs:25355). Applied as clock.TimeColour (MeterManager.cs:28139). clsClock default Orange (:15395-15484). | Legacy: "Time color" (ClockItemEditor.cpp:73), default #c8d8e8 (ClockItem.h:88). **Not written by serialize()** (ClockItem.cpp:116-121), so it is lost on reload. Composite: `clockTimeColor` is saved (CompositePresetItem.cpp:575, 617). | partial (legacy loses it on reload; default colour differs) |
| Date colour | "Date:" clrbtnMMDate (:63067). Saved to **igs.SubMarkerColour** (setup.cs:25356). Applied as DateColour (:28140). Default Gray. | Legacy: "Date color" (ClockItemEditor.cpp:84), default #8090a0 (ClockItem.h:89), not serialized. Composite: `clockDateColor` saved. | partial (same as time colour) |
| Title colour | clrbtnMMClockTitle (:63097, tooltip "Meter title colour"). Saved to **igs.TitleColor** (setup.cs:25354). Applied as TypeTitleColour (:28142). Default Gray. | Legacy: "Title color" (ClockItemEditor.cpp:95), default #708090 (ClockItem.h:90), not serialized. Composite: `clockTitleColor` saved. | partial |
| Title colour enabled only when title is shown | updateTitleControlsClock (setup.cs:26468) enables clrbtnMMClockTitle only while chkMMClockTitle is checked. | Legacy editor always enables the title colour button. | missing |
| Meter Title on/off | chkMMClockTitle "Meter Title" (:63117, tooltip "Show meter title"). Saved to **igs.ShowType** (setup.cs:25353). renderClock draws lowercase "local" at x+1% and "utc" at x+52%, top-left, at FontSize (:40474-40528). | Legacy: "Show UTC/local label" (ClockItemEditor.cpp:53), serialized. Draws capitalized "Local"/"UTC", centered above the time (ClockItem.cpp:107, 112-113). Composite: `showTitle`, also "Local"/"UTC". | partial (wording case and position differ) |
| 24 hr / 12 hr | radMM24Clock "24 hr" (:63131) and radMM12Clock "12 hr" (:63145). Saved to **igs.ShowMarker** = radMM24Clock.Checked, with comment "use the show marker bool for this" (setup.cs:25357). Applied as Show24HourCLock (:28143). | Legacy: "24-hour format" checkbox (ClockItemEditor.cpp:44), serialized (ClockItem.cpp:121). Composite: `show24Hour`. | match (control) |
| 12 hr when neither radio is set | On load, checks 12 hr if neither radio is checked (setup.cs:26140-26150). | One checkbox, so there is no indeterminate state. Default is 24 hr (ClockItem.h:86), the same as clsClock `_24HourClock = true`. | n/a (UI guard for a radio pair) |
| Time text format | 24 h "HH:mm:ss". 12 h "h:mm:ss" (no leading zero) with a separate lowercase "am"/"pm" at 0.8x size, plus a pad when the string is 7 characters. Time at 1.9x font size (renderClock :40474-40528). | Legacy and composite: 12 h is "hh:mm:ss AP" (leading zero, uppercase AM/PM inline) (ClockItem.cpp:90; CompositePresetItem.cpp:1231). Time is drawn bold. | partial |
| Date text format | "ddd d MMM yyyy" at 0.9x font, at y+60%. | Legacy: "ddd d MMM yyyy" (ClockItem.cpp:91). Composite: "yyyy-MM-dd" (CompositePresetItem.cpp:1239). | partial (legacy matches; composite differs) |
| Local and UTC side by side | Always both. Local on the left half, UTC on the right. | Legacy: same order, 48% / 4% gap / 48% (ClockItem.cpp:82-85). | match |
| clockMode Both / UTC / Local, and showDate | Not in Thetis. renderClock always draws both clocks and both dates. | Composite only: `clockMode` (CompositePresetItem.cpp:262, 1241; combo ContentPropertyEditor.cpp:234) and `showDate` (:1232). | Nereus-only |
| Font | clsClock "Trebuchet MS", Regular, 18f (:15395-15484). Not user editable. | Legacy: default UI font, fitted to item height (ClockItem.cpp:93-109). Composite: `fontSize` field (default 18). | partial |
| Update rate | clsClock UpdateInterval = 250, fixed. | Legacy repaints when the second changes (ClockItem.cpp:69-75). | match (same visible result) |
| Fade on RX / TX | No control in grpMeterItemClockSettings. Apply copies igs.FadeOnRx/Tx onto the solid colours and the clock (:28126-28127, :28137-28138), but the clock save never writes them. | Legacy: none; "Only when RX/TX" hides instead. Composite: `fadeRx`/`fadeTx` dim to 25% (CompositePresetItem.cpp:1181). | n/a (not user editable in Thetis); the composite's fade is Nereus-only |
| Save and reload of every setting | All six igs fields saved (setup.cs:25350-25358) and restored (:26140-26150). | Legacy serialize writes only `CLOCK|x|y|w|h|binding|z|24h|showType` (ClockItem.cpp:116-121). The three colours are not saved. | partial (**data-loss bug: colour edits vanish on reload**) |
| Clock readings for other items | The clock publishes TIME_UTC (am/pm appended in 12 h), DATE_UTC, TIME_LOC and DATE_LOC readings, using the 24 h setting (MeterManager.cs:31065-31099). | None. A grep for time_utc/TIME_UTC in src/ finds nothing. | missing |
| Reading / source bindings | The clock has no reading source. | Advanced bindings are disabled for CLOCK and meter.clock (ContentPropertyEditor.cpp:388-389). BaseItemEditor still shows "Choose a value…", which has no effect on a clock. | match (advanced); the stray binding row is Nereus-only |

Note for porting: the Thetis apply block contains commented lines `//vfo.UpdateInterval = igs.UpdateInterval;` and `//clk.Show24HourCLock = igs.Clock24Hr;` (MeterManager.cs:28135-28136). If that block is ported, the inline-comment rule requires keeping them.

Multislice notes:
- The clock does not depend on the radio. Thetis publishes the clock readings into the owning meter's readings (MeterManager.cs:31065-31099), so a text overlay on an RX2 meter reads the RX2 copy. NereusSDR should publish clock values once, globally, not per slice. Today it publishes none.
- In remote Core use, "Local" means the client machine's time zone in both Thetis and NereusSDR (QDateTime::currentDateTime in ClockItem.cpp:87). No change needed. Worth confirming that is the wanted behavior.

---

### VFO: MeterType.VFO_DISPLAY / clsVfoDisplay

Thetis Setup group: `grpMeterItemVfoDisplaySettings`. Labels and controls:

| Label / control | Cite |
|---|---|
| "Sync:" | 68112 |
| "Lock:" | 68122 |
| "Small #'s:" | 68132 |
| "Show Band Text" | 68142 |
| VFO B / VFO A / Both radios | 68155 / 68168 / 68181 |
| "VfoHighlight:" | 68193 |
| "Titles" | 68203 |
| "Background:" | 68213 |
| "Frequency:" | 68223 |
| "Band:" | 68233 |
| "Filter:" | 68243 |
| "TX:" | 68253 |
| "RX:" | 68263 |
| "Split:" | 68273 |
| "Split Back:" | 68283 |
| "Mode:" | 68293 |

Thetis wiring:

* Save: setup.cs:25320-25348. Load: :26101-26139. Apply: MeterManager.cs:28036-28115.
* Class: clsVfoDisplay :14201-15395. Render: renderVfoDisplay :39857-40474.

NereusSDR: src/gui/meters/VfoDisplayItem.{h,cpp}, VfoDisplayItemEditor.cpp. Fed by MainWindow.cpp:15030-15146. The composite `meter.vfoDisplay` wraps the same item and saves four colours (CompositePresetItem.cpp:563, 573, 616).

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Display mode Both / VFO A / VFO B | radMultiMeter_vfo_display_*. Saved to **igs.HistoryDuration** (VFO_BOTH=0, VFO_A=1, VFO_B=2) (setup.cs:25344-25348). Both shows two VFOs; the second background is visible only in Both (MeterManager.cs:28036-28115). | Combo "VFO A / VFO B / Both" (VfoDisplayItemEditor.cpp:61-67), serialized. Enum order is `{VfoA, VfoB, VfoBoth}` (VfoDisplayItem.h:71), which is a different numbering from Thetis. paint draws only `m_frequencyHz`. `setFrequencyB` (h:77) is never called, so VFO B and Both never show a second VFO. | partial |
| Background | "Background:". Saved to **igs.Colour**. | Hard-coded #0a0a18 (VfoDisplayItem.cpp:117). No control. | missing |
| Titles colour and titles | "Titles". Saved to **igs.TitleColor**. Draws "VFO A" / "VFO Sub" / "VFO B" titles (~40248-40262). | No titles, no control. | missing |
| Frequency colour | **igs.MarkerColour**. Default Orange (:14304-14347). | "Frequency" (VfoDisplayItemEditor.cpp:99). Legacy serialize omits it (VfoDisplayItem.cpp:204-209), so it is lost on reload. Composite saves `vfoFrequencyColor`. | partial (**legacy data-loss bug**) |
| Small #'s colour, and "=" copy button | **vfo_frequency_small_numbers_colour** (named setting), default Orange. Button btnVFOCopyColourFromMainNumbers, tooltip "Copy the colour from the main #'s" (designer 64104-64105; handler setup.cs:32175). | missing | missing |
| Mode colour | **igs.SubMarkerColour**, default Gray. | "Mode" (editor :100), not serialized in legacy. | partial |
| Filter colour | **igs.HistoryColor**, default Gray. Draws the filter name. | "Filter" (editor :101), not serialized in legacy. Shows filter width text from MainWindow. | partial |
| Band colour | **igs.SegmentedSolidLowColour**, default White. | "Band" (editor :102), not serialized in legacy. | partial |
| RX colour | **igs.PeakValueColour**, default LimeGreen. Colours the RX box (40339-40346). | "RX label" (editor :103), not serialized. Used as a 3 px bar at the left edge (VfoDisplayItem.cpp:122-123). | partial |
| TX colour | **igs.PeakHoldMarkerColor**, default Red. Colours the TX box. | "TX label" (editor :104), not serialized. `setTransmitting` (h:86) is never called, so the TX colour never shows. | partial |
| Split colour | **igs.HighColor**, default Orange. | No editor row. "SPLIT" is drawn in m_splitColour (VfoDisplayItem.cpp:150-155), but `setSplit` (h:88) is never called. | missing |
| Split Back colour | **igs.LowColor**, default (64,64,64). | missing | missing |
| VfoHighlight (digit highlight) | **igs.PowerScaleColour**, default (128,128,128). Highlights the digit under the mouse (render ~39895-40160). | missing | missing |
| Show Band Text, and its colour | chkMultiMeter_vfo_show_bandtext saved to **vfo_showbandtext**, with **vfo_showbandtext_colour** (default LimeGreen). The colour button is enabled only when on (updateVfoShowBandtextColour setup.cs:32141). Pad 0.03 (apply). Render 40404-40417. | missing | missing |
| Lock colour and lock icon | **vfo_lock_colour** (default LimeGreen). Lock icon (40422-40457). | missing | missing |
| Sync colour and sync icon | **vfo_sync_colour** (default LimeGreen). Sync icon (40422-40457). | missing | missing |
| Font | Trebuchet MS 18 (:14304-14347). | Fitted text (VfoDisplayItem.cpp:145-148). | partial |
| Update rate | UpdateInterval 50, fixed. | Event driven from the slice (MainWindow.cpp:15030-15146). | n/a (different update model, same visible result) |
| Mouse wheel on a digit | Steps by that digit's weight. Hover boxes per digit (MouseWheel ~14555; render ~39895-40160). | Nine digit weights picked from the x fraction (VfoDisplayItem.cpp:101-108). Emits `frequencyChangeRequested` (:185), which sets the container slice frequency (MainWindow.cpp:15333-15344). No hover boxes. | partial |
| Keyboard digit entry | KeyDown (~14521). | missing | missing |
| Click the VFO to step, and the screen state machine | MouseUp (~14610-14800): left/right click step. Opens BAND, FILTER, MODE and TUNE_STEP screens with selection grids. | missing | missing |
| BAND screen | Band grid with HF / VHF / GEN panels and WWV (setBand logic). | Right-click on the band third emits `bandStackRequested` (VfoDisplayItem.cpp:199). MainWindow answers with the "band stack not built" reason (MainWindow.cpp:14777-14782). | missing |
| Band stack popup | PopBandStack, with comment "only for rx1, vfoa right now". | Same "not built" reason as above. | missing |
| FILTER screen / long-press filter menu | PopFilterMenu on a 1000 ms long press. Filter grid screen. | Right-click on the filter third emits `filterContextRequested` (VfoDisplayItem.cpp:200), which opens the container filter context (MainWindow.cpp:14773-14776). | partial |
| MODE screen | Mode grid. | missing | missing |
| TUNE_STEP screen | Uses console.TuneStepList. | missing | missing |
| SPLIT toggle | Split button, VFO A only. | missing | missing |
| TX toggle | TX box toggles the transmit VFO. | missing | missing |
| LOCK toggle | Toggles the VFO lock. | missing. SliceModel only refuses frequency steps on a locked slice. | missing |
| VFO_SYNC toggle | Toggles VFO sync. | missing | missing |
| Screen timeout | A screen goes back to the VFO view after more than 5 s (15282, 15295). | n/a, there are no screens. | missing |
| Inactive VFO faded | VFO A or B is drawn faded (alpha 24) depending on RX / RX2 (~39882-39888). | missing | missing |
| VFO readings for other items | Publishes VFOA_FREQ and related readings (MeterManager.cs:31100-31118). | missing | missing |
| "Slice not open" text | Not in Thetis. | `setUnavailableText` (VfoDisplayItem.h:92; paint :128-135). | Nereus-only |
| Slice source | Not in Thetis. | Container source: Slice A-D or "Follow selected RX" (ContentPropertyEditor.cpp:397-399). | Nereus-only |

Multislice notes:
- Thetis getVfo/setVfo/abortForLockedVFO/adjustVfo (MeterManager.cs:14404-14520) branch on the owning meter's RX (1 or 2), console.RX2Enabled, MultiRxEnabled/Split and VfoSub. NereusSDR needs one model instead:
  - "VFO A" means the container's slice (A-D);
  - "VFO B" means that slice's split/TX partner, or a second chosen slice;
  - "VFO Sub" has no NereusSDR equivalent until multi-RX on one slice exists.
  
  Today the feed handles only one frequency per slice (MainWindow.cpp:15030-15146).
- The Thetis TX box and SPLIT act on console-wide MOX and split state. NereusSDR needs them to act on the TX-owner slice of the right panadapter, routed through RadioModel so that a remote Core gets the command.
- PopBandStack is "only for rx1, vfoa right now" in Thetis. In NereusSDR it should use the container slice's band stack once band stacking is built.
- The tune-step screen reads console.TuneStepList (global). NereusSDR should read the container slice's step list, if steps are per slice.
- The faded-VFO logic (~39882-39888) keys off which RX the meter belongs to. NereusSDR should key off whether the container's slice is the active/selected one.

---

### DIAL: MeterType.DIAL_DISPLAY / clsDialDisplay

Thetis Setup group: `grpDialDisplay` (designer ~69267). Save setup.cs:25188-25214, load :25906-25932. The setting keys use the prefix `dialdisplay_`, including the misspelled key `dialdisplay_cirlce`.

Thetis class: clsDialDisplay MeterManager.cs:16719-17366, constructor :16764, UpdateInterval 1000/60. Render: renderDialDisplay :35284.

NereusSDR: src/gui/meters/DialItem.{h,cpp}, DialItemEditor.cpp. Paint only; nothing outside the registry references it.

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Fade on RX / TX | chkDialDisplay_fade_rx/_tx saved to igs.FadeOnRx/Tx (setup.cs:25190-25191). | Only the hide-style "Only when RX/TX". | missing |
| Vertical ratio | 0.15-1, default 1 (load clamps to 0.01-1). Height of the dial vs width. | missing | missing |
| Font scale | 0.01-1.1, default 1. | missing | missing |
| Always show VFOs | When off, RX2 forces VFO B and RX1 with RX2 enabled forces VFO A (MeterManager.cs:17032-17072). | missing | missing |
| Align with tune step | Default true. Rounds the frequency to the tune step (Degrees setter :16907). | missing | missing |
| Accelerate-up threshold | 90-720, default 540 (Accelerate :16989-17014, ChangeTuneStepUp). | missing | missing |
| Accelerate-down threshold | 90-720, default 360 (ChangeTuneStepDown). | missing | missing |
| Interval | 1-10 s, default 2. Acceleration sampling window. | missing | missing |
| Max increments | 1-30, default 4. Maximum tune-step raises. | missing | missing |
| Degrees for change | 1-90, default 5. Degrees of rotation per tune step (Degrees setter :16907; MouseWheel :17157). | missing | missing |
| Text colour | Default White. | "Text color" (DialItemEditor.cpp:70), default #c8d8e8 (DialItem.h:131). | partial (default differs) |
| Circle colour | Default Black. Key `dialdisplay_cirlce`. | "Circle color" (:72), default #1a2a3a. | partial (default differs) |
| Pad colour | Default Blue. | "Pad color" (:74), default #152030. | partial (default differs) |
| Pad pressed colour | Default Orange. | missing | missing |
| Ring colour | Default Gray. | "Ring color" (:76). | partial (default differs) |
| Button on / off / highlight | CornflowerBlue / Black / Gray. | "Button on/off/highlight" (:80-84), different defaults. | partial |
| Slow / hold / fast colours | Blue / Green / Red. | "Slow/Hold/Fast color" (:88-92), defaults green/amber/red (DialItem.h:138-140). | partial (defaults differ) |
| Tuning by drag and wheel | Rotating the dial steps the VFO by CurrentTuneStepHz each degrees_per_change (:16907, :17157). | DialItem has no mouse or wheel handlers. | missing |
| Acceleration and undo on release | Accelerate (:16989-17014); undoTuneStep on release (:17094-17099). | Only the paint state `setSpeedState` (DialItem.h:107). Nothing sets it. | missing |
| VFOA / VFOB / ACCEL / LOCK buttons | MouseUp :17134 toggles the VFO choice and ACCEL, and toggles VFOALock/VFOBLock. Labels " VFOA ", "VFOB", " LOCK ", " ACCEL " (render :35284; highlight depends on RX :35335-35338). | Labels are drawn (DialItem.cpp:147-159). `setAccelEnabled`/`setLockEnabled` exist (h:110-114) but nothing calls them. No clicks. | partial (drawn only) |
| Save and reload | All of the above saved as named settings. | serialize saves the ten colours (DialItem.cpp:264-285). | partial (only the colours exist to save) |

Multislice notes:
- VFO selection (MeterManager.cs:17032-17072) assumes RX1/RX2 and VFO A/B. LOCK toggles console.VFOALock/VFOBLock. NereusSDR needs the dial to tune the container's slice (A-D, or "Follow selected RX"), lock that slice, and route through RadioModel for a remote Core.
- CurrentTuneStepHz and ChangeTuneStepUp/Down are console-wide in Thetis. NereusSDR needs to decide whether acceleration changes the slice's tune step or a global one.
- Audit E covers the S-meter dial applet. This item is the tuning knob, which is a separate thing.

---

### TEXTOVERLAY: MeterType.TEXT_OVERLAY / clsTextOverlay

Thetis Setup group: `grpTextOverlay`.

Thetis wiring:

* Save: setup.cs:25394-25431. Load: :26195-26231. Apply: MeterManager.cs:27514-27570, where the panel height is set from padding and the item is rebuilt.
* Class: clsTextOverlay :20066-21605. Update :20529-20559, parseText :20587-20716, render renderTextOverlay :34135-34290.

NereusSDR: src/gui/meters/TextOverlayItem.{h,cpp}, TextOverlayItemEditor.cpp (rows :61-280).

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| RX text / TX text, one at a time | Saved to **igs.Text1** / **igs.Text2**. Text1 shows in RX and Text2 in MOX, each with its own font, colour, offsets and back (render :34135-34290). | "Text 1" / "Text 2" (editor :64, :146). **Both lines are always drawn**, line 1 in the top half and line 2 in the bottom half (TextOverlayItem.cpp:153-175). There is no MOX swap. | partial |
| Text colour 1 / 2 | **igs.Colour** / **igs.MarkerColour**. | "Color 1" / "Color 2" (editor :87, :169), serialized (TextOverlayItem.cpp:289-290). | match |
| Text back colour 1 / 2 | **igs.SubMarkerColour** / **igs.PeakValueColour**. A box behind the text. | "Back color 1/2" (editor :103, :185). **Not serialized** (TextOverlayItem.cpp:272-303), so it is lost on reload. | partial (**data-loss bug**) |
| Show text back 1 / 2 | chkTextOverlay_textback1/2 "Back", saved to **igs.ShowMarker** / **igs.ShowType**. | "Show back 1/2" (editor :105, :187). **Not serialized.** | partial (**data-loss bug**) |
| Panel background RX / TX | **igs.TitleColor** (RX) / **igs.HistoryColor** (TX). Panel colour follows MOX, with a 0.5 s fade when the two colours differ. | "Panel back 1/2" (editor :250, :266). Panel 1 colours the top half and panel 2 the bottom half. No MOX swap, no transition. | partial |
| Show Panel | **igs.ShowSubMarker**. | "Show panel" (editor :227), serialized. | match |
| Panel padding | 0.001-10, saved to **igs.SpacerPadding**. Sets the panel height on apply (27514-27570). | "Padding" 0-1, a fraction of height (editor :280). | partial (different range and meaning) |
| Font 1 / Font 2 | FontDialog, TrueType only (setup.cs:29602). Family, full style and size, saved to FontFamily/Style/Size 1 and 2. Size × 2.1 fudge in render. | Font combo, size 4-200 and "Bold" per line (editor :115-133, :197-215). No italic, underline or strikeout. Size scales with the item. | partial |
| Copy fonts "=" | btnTextOverlay_copyfonts copies TX font, colour, back colour and back flag from RX (setup.cs:31291-31298). | missing | missing |
| RX x / y offset | -10..10, step 0.001, saved to **igs.EyeScale** / **igs.EyeBezelScale**. | missing | missing |
| TX x / y offset | Saved to **igs.AttackRatio** / **igs.DecayRatio**. | missing | missing |
| Copy offsets "=" | Copies the RX offsets to TX (setup.cs:29690). | missing | missing |
| RX on Led / TX on Led | Checkboxes with a 4-char box, saved to **textoverlay_rx_ledlogic / tx_ledlogic / rx_4char / tx_4char**. Tooltip "Show RX text only when this Led Indicator is true" (designer 62773-62774, 62749-62750). Evaluated in Update (:20552-20559). | missing | missing |
| Variable picker "%" | Copies `%var%` to the clipboard (setup.cs:35280). | "Choose a value…" binds the item's one reading. There is no token picker. | missing |
| %precis=N% | 0-20 (parseText :20587-20716). | `PRECIS=` (TextOverlayItem.cpp:110). | match |
| %nl% | New line. | `NL` (TextOverlayItem.cpp:107). | match |
| Reading tokens (%SIGNAL_STRENGTH% etc.) | Any Reading enum name. | Only `%VALUE%` for the bound reading, "--" when unbound (TextOverlayItem.cpp:118). | partial |
| Custom placeholders | From _all_readings_text (MeterManager.cs ~788-840), values from GetReading (999-1260): time_utc, time_utc_int, time_loc, time_loc_int, date_utc, date_utc_int, date_loc, date_loc_int, vfoa, vfob, vfoasub, vfoa_double, vfob_double, vfoasub_double, band_vfoa, band_vfob, band_vfoasub, mode_vfoa, mode_vfob, subrx, filter_vfoa, filter_vfob, filter_vfoa_name, filter_vfob_name, split, qso_time, qso_time_short, qso_time_int, tb_qso_time, tb_qso_time_short, tb_qso_time_int, mox, cfc, comp, lev, rx2, rxn, nr, tx_eq, bandtext_vfoa, bandtext_vfob, nf, tune_step, pa_profile, bw_vfoa, bw_vfob, bw_tx, on, discord_general, discord_bot. | A `setVariable` map exists (TextOverlayItem.h:129) but nothing in src/ calls it. | missing |
| MMIO variables | Every MMIO node's variables are available as tokens. | missing | missing |
| CAT variables | _cat_variables (MeterManager.cs:327). | missing | missing |
| Auto scroll | When the text is wider than the panel, it scrolls at a fixed -0.15 (:20154; a ScrollX setter exists at :20334-20338 but Setup does not use it). It resets when the text changes (:20539, :20549) or on hover. | "Scroll X" -10..10, default 0 = off (editor :271; TextOverlayItem.h:173). The user picks the speed. There is no automatic "scroll only when too wide", and no reset on hover. | partial |
| Fade on RX / TX | chkTextOverlay_FadeOnRX/TX, saved to igs.FadeOnRx/Tx (setup.cs:25396-25397). | Only the hide-style "Only when RX/TX". | missing |

Thetis behavior to rule on: Update tests `if (_rx_led_logic)` twice (MeterManager.cs:20552-20559). The second test reads `_tx_four_char` into `_show_tx` but is gated by the RX flag. So "TX on Led" only works when "RX on Led" is also ticked. This looks like an upstream bug.

Multislice notes:
- Values come from `ReadingsCustom(_owningMeter.RX)` (MeterManager.cs:20535, 20545, 20367), with the RX limited to 1 or 2 (:20589). NereusSDR needs the tokens resolved against the container's slice (A-D, or "Follow selected RX").
- Tokens named vfoa, vfob, vfoasub, band_vfoa/b, mode_vfoa/b, filter_vfoa/b, bw_vfoa/b, rx2, rxn and subrx encode the Thetis RX1/RX2 and VFO A/B/Sub model. NereusSDR must decide what each means: for example vfoa = this container's slice, vfob = its split partner, rx2 = "is a second slice open". It might also add per-slice tokens (slice_a_freq, ...).
- The RX/TX swap uses the meter's MOX, which is console-wide. NereusSDR should use MOX for the transmitting slice that this container's slice belongs to, which is the same radio in remote Core use.
- discord_* and cat variables assume one Thetis instance. NereusSDR's CAT is per radio, so CAT tokens need a radio scope.

---

### WEBIMAGE: MeterType.WEB_IMAGE / clsWebImage

Thetis Setup group: `grpWebImage`.

Thetis wiring:

* Save: setup.cs:25216-25227. Load: :25934-25948.
* Class: clsWebImage MeterManager.cs:15485-16361. Render: renderWebImage :36578-36650.

NereusSDR: src/gui/meters/WebImageItem.{h,cpp}, WebImageItemEditor.cpp.

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Url | "Url:" text box. | "URL" row with a "Reload" button (WebImageItemEditor.cpp:57-61), serialized. | match (Reload is Nereus-only) |
| Update interval | "Update (s):" 30-7200, default 60. | "Refresh (s)" 1-3600 (editor :78), default 300 (WebImageItem.h:99). | partial (range and default differ) |
| Known-host lock | Typing a hamqsl, bsdworld or kc2g URL forces the interval to 600 and turns bypass cache off (setup.cs:31492-31531). fixBSDWorldUrls adds -light/-dark. | missing | missing |
| Width scale | "Width Scale:" 0.001-1, saved to **igs.EyeScale**. | missing | missing |
| Bypass cache | "Bypass Cache", saved to **igs.DarkMode**. Appends a unique id to each request. | missing | missing |
| State label | idle / ok / url issue / bad image / no image / waiting (setup.cs:31607). | missing | missing |
| 4Char ID and copy | Read-only "4Char ID:" (webimage_4char, loaded :25944; KeyPress blocked) plus a copy button (setup.cs:33207). | missing | missing |
| Use as spectral area background | chkWebImage_background (designer 66552). Raised via the ShowWebImageBackground event (clsWebImage). | missing | missing |
| Background chain: "After N s goto 4Char ID", Next ID, ">" go-to | 5-3600 s, a next 4-char ID, and a go-to button (setup.cs:33242). Enable logic in updateWebImageBackground (:33227). | missing | missing |
| Presets | Combos for HamQsl (_hamqsl_urls :31706), BsdWorld (:31734, plus a "Dark Mode" checkbox), NASA (:31764), NOAA (:31776). Setup :31559-31595. Donate and visit buttons. | missing | missing |
| Fade on RX / TX | chkWebImage_fade_rx/_tx (setup.cs:25220-25221). Fades on the MOX transition (render :36578-36650). | Only the hide-style "Only when RX/TX". | missing |
| file:// URLs | Special-cased (MeterManager.cs:15865-15900). | No special case. Uses a plain QNetworkAccessManager GET (WebImageItem.cpp:155-188). Not verified at runtime. | partial |
| Draw | Keeps the aspect ratio, clipped. Shows "No Image" text when there is no image (:36578-36650). | Stretches to the rect, losing aspect ratio. Fills the fallback colour when there is no image (WebImageItem.cpp:196-203). | partial |
| Fallback colour | Not in Thetis. | "Fallback color" (editor :102). | Nereus-only |
| No fetch in previews | Not in Thetis. | Previews never fetch (WebImageItem.cpp:121). | Nereus-only |

Multislice notes:
- The only radio coupling is fade on MOX. NereusSDR should follow the transmitting radio of the container's slice.
- "Use as spectral area background" draws behind the Thetis console spectrum. That is effectively RX1's display, or whichever display the console shows. NereusSDR has several panadapters, so this option would need a panadapter picker.

---

### FILTERDISPLAY: MeterType.FILTER_DISPLAY / clsFilterItem

Gating: gated by `ContainerFilterDisplay`, but `isBuilt()` returns true for it (UnbuiltFeatureList.cpp:185). So it is live and fed by MainWindow.cpp:~14700-14750.

Thetis wiring:

* Save: setup.cs:25446-25503. Load: :26241-26300. Apply: MeterManager.cs:27651-27700. Get: :28860-28890.
* Class: clsFilterItem :18172 (FIWaterfallPalette :18174, FIDisplayMode :18185). Render: renderFilterDisplay :35466.

NereusSDR: src/gui/meters/FilterDisplayItem.{h,cpp}, FilterDisplayItemEditor.cpp.

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Back colour | clrbtnFilterDisplay_backcolour, saved to **igs.Colour** (setup.cs:25448). Item-group background. | One "Background" (= meter back) only (editor :163). | partial |
| Fade on RX / TX | chkFilterDisplay_fadeonrx/tx (setup.cs:25449-25450). | Only the hide-style "Only when RX/TX". | missing |
| Vertical ratio | filterdisplay_vertical_ratio, 0.15-1, default 1 (designer); apply default 0.2. | missing | missing |
| Show filter limits | filterdisplay_show_filter_limits, default true. | missing | missing |
| Fixed RX zoom and level | Flag plus level 1-10. | missing | missing |
| Fixed TX zoom and level | Flag plus level 1-20. | missing | missing |
| Sidebands / CW / others scale | 0-10 each. | missing | missing |
| Display mode | PANADAPTOR / WATERFALL / PANAFALL / NONE, saved to `filterdisplay_others_displaymode`. Default PANAFALL on apply (MeterManager.cs:27678). | "Display mode" combo in the same order (editor :70-78), serialized. Default Panafall. | match |
| Font scale | 0.01-4. | missing | missing |
| Fill spectrum | filterdisplay_fill_spec, default true. | "Fill spectrum" (editor :110). | match |
| Data line / data fill colours | Default LimeGreen. | "Data line" / "Data fill" (editor :158-159), serialized. | match |
| Waterfall palette | ENHANCED, SPECTRAN, BLACKWHITE, LINLOG, LINRAD, LINAUTO, CUSTOM. Default ENHANCED. | "WF palette" in the same order (editor :92-99), serialized. | match |
| Waterfall low colour | filterdisplay_wflow_colour, default Black. | `setWaterfallLowColour` is fed from global settings by MainWindow. No editor row, not serialized. | partial |
| Text colour | filterdisplay_text_colour, default White. | "Text" (editor :164). Used only for "Spectrum unavailable" (FilterDisplayItem.cpp:249-251). **Not serialized** (:586-607). | partial |
| Number highlight colour | Default DarkRed. | missing | missing |
| Edges RX / TX colours | Yellow / Red. | "RX edges" / "TX edges" (editor :160-161), serialized. | match |
| Edge highlight colour | Default White. | missing | missing |
| Meter back colour | Default Black. | "Background" (editor :163), serialized. | match |
| Notch / notch highlight colours | OrangeRed / LimeGreen. | "Notch" only (editor :162; default Orange, h:`m_notchColour`). | partial |
| Extents colour and extents text | Default Gray. Low/high extent labels (render :35801-35802). | missing | missing |
| Sideband mode | Default false. | missing | missing |
| Waterfall frame update | 1-1000, default 4. | `m_waterfallFrameInterval` defaults to 4 and is serialized (FilterDisplayItem.cpp:607). No editor row. | partial |
| Grey outside passband | filterdisplay_use_grey, default true. | missing | missing |
| Snap line / setting-on / button highlight colours | Gray / CornflowerBlue / Gray. | missing | missing |
| Filter characteristic | filterdisplay_characteristic, plus a low limit of -300..-50 (default -250). | A high-resolution filter curve is applied from DSP Options for every item (DspOptionsPage::applyPersistedHighResFilterTo, MainWindow.cpp:14860). Not a per-item setting. | partial |
| Labels | Mode string, filter name, low/high offset numbers, bandwidth, centre "0" (render :35809-35814, :36145, :36240-36245). | missing (FilterDisplayItem draws no labels). | missing |
| MNF, +MNF, SNAP, FIT buttons | Drawn at :36195-36198. MNF toggles TNF and +MNF adds a notch (clsFilterItem MouseUp :18994, :19001). | missing | missing |
| Drag filter edges and shift | Edits the RX1/RX2 var filter, or the TX filter in MOX (:19201, :19223-19250; Shift :19378). | missing (no mouse handlers). | missing |
| Notch editing | Right-click opens the notch popup (:18930). Ctrl+right-click adds a notch (:18958). Wheel changes notch width (:19013). | missing | missing |
| Spectrum min / max dB | Not in Thetis Setup. | "Min dB" / "Max dB" (editor :130-140). | Nereus-only |

Multislice notes:
- Filter edits branch on `_owningmeter.RX` to UpdateRX1Filters or UpdateRX2Filters (MeterManager.cs:19223-19250, ~19400). Notches are added with `_console.AddNotch(freq, _owningmeter.RX)` (:18958). Waterfall AGC reads `Display.*RX2` (:19854-19870).
- NereusSDR already feeds the item from the container's slice (MainWindow.cpp:~14700-14750) and asks RadioModel for the response by station (`filterResponseForStation`), which covers the remote Core.
- Any interaction port needs slice-scoped filter and notch setters routed through RadioModel. The TX branch should use the TX-owner slice, not "MOX on the console".

---

### ROTATOR: MeterType.ROTATOR / clsRotatorItem

Thetis Setup group: `grpMeterItemRotator`, Text "Rotator" (designer 68355), 43 controls.

Thetis wiring:

* Save: setup.cs:25229-25271. Load: :25950-26003. Handlers: :31207-31342. MMIO variable buttons: :25647-25660 and :31282-31290.
* Class: clsRotatorItem MeterManager.cs:16362-16718.
* AddRotator ~:23470-23560: AZ item on MMIO variable slot 0, ELE item on slot 1, a grid image (z 3), a map image (z 2, clipped ellipse) and a (32,32,32) solid background.
* Apply: :27298-27390. Render: renderRotator :36704-37225.

NereusSDR: src/gui/meters/RotatorItem.{h,cpp}, RotatorItemEditor.cpp. Control is wired to the window's rotor model (MainWindow.cpp:14861-14866).

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Show AZ / ELE / Both | radMeterItemRotator_show_*, saved to **igs.HistoryDuration** (AZ=0, ELE=1, BOTH=2) (setup.cs:25247-25252). Apply clamps bad values to AZ (MeterManager.cs:27320-27321). | "Mode" combo Azimuth/Elevation/Both (RotatorItemEditor.cpp:76-81), same order, serialized. Default Both. | match |
| Update rate | nudMeterItemUpdateRateRotator, 15-5000 ms, default 100, saved to **igs.UpdateInterval**. | Fixed 100 ms feed (kFeedIntervalMs, RotatorItem.cpp:94). | partial |
| Background | clrbtnMeterItemHBackgroundRotator, saved to **igs.Colour**. Fills the whole item with a clsSolidColour (MeterManager.cs:23554). | "Background" (editor :145), serialized. Fills only the compass disc (RotatorItem.cpp:325-328). | partial |
| Arrow colour | **igs.TitleColor**, default White. | "Arrow" (editor :143), serialized. | match |
| Large dot colour | **igs.MarkerColour**, default Red. | "Large dot" (editor :140). | match |
| Small dot colour | **igs.SubMarkerColour**, default White. | "Small dot" (editor :141). | match |
| Text colour | clrbtnMeterItemRotatorText, saved to **igs.HighColor** (OuterTextColour, default (128,128,128)). Every cardinal, including "N", is drawn in it (MeterManager.cs:36809). | "Outer text" (editor :142). Draws "N" in the large-dot colour and bold, citing "MeterManager.cs:35275" (RotatorItem.cpp:390-395). Thetis does not do this, and the cite points into renderDialDisplay. | partial |
| Cardinals | chkMeterItemRotatorCardinals "Cardinals", saved to **igs.ShowHistory**. 16-point readout (convertDegreesToCardinal :36671-36696). Ring labels N..NW at 45° (:36760-36810). | "Show cardinals" (editor :31), serialized. Ring at RotatorItem.cpp:348-410. | match |
| Show beam width | chkMeterItemRotatorShowBeamWidth "Show", saved to **igs.ShowMarker**. Enables the colour, degrees and alpha controls (setup.cs:31258-31270). | "Show beam width" (editor :40). The beam-width controls are never disabled. | partial |
| Beam width | 0-180, step 0.05, 1 dp, saved to **igs.AttackRatio**. Class default 30. | "Beam width (deg)" 0-360, step 1 (editor :49). | partial (range differs) |
| Beam width colour | **igs.LowColor**, default (64,64,64). | "Beam width" (editor :144). | match |
| Beam width alpha | rotator_beamwidth_alpha, 0-1, default 0.6 (setup.cs:25271, 26003). | "Beam alpha" 0-1 (editor :59), default 0.6. | match |
| Padding | 0.001-1, saved to **igs.EyeScale**. Sets face size in AZ-only or ELE-only mode (MeterManager.cs:27335-27346) and the text scale (:36742-36743). | "Padding" 0-2 (editor :78), serialized. **Never used in paint** (only RotatorItem.cpp:1208, 1269). | partial (no effect) |
| Dark Mode | chkMeterItemDarkModeRotator, saved to **igs.DarkMode**. Applied to the grid and map images (MeterManager.cs:27370, 27393). | "Dark mode" (editor :69), serialized. **No effect on paint.** | partial (no effect) |
| Fade on RX / TX | chkMeterItemFadeOnRx/TxRotator (setup.cs:25240-25241). Applied to the rotator and its images. | Only the hide-style "Only when RX/TX". | missing |
| Grid image and world map | Built-in images rotator_az-bg / rotator_ele-bg / rotator_both-bg (ImageName :16507-16521) and rotator_map-bg as a clipped ellipse (MapName :16523; AddRotator :23525, :23543). | "BG image" file picker (editor :158-168), clipped to the disc (RotatorItem.cpp:331-337). No built-in grid or map. | partial |
| Show value | `_showValue` defaults true (:16403-16440). Not exposed in Setup. Draws "f1°" plus "azimuth"/"elevation" captions and the cardinal (:36897-36911, :37097-37104). | "Show value" checkbox (editor :22). Captions drawn (RotatorItem.cpp:626-679, 737-788). | Nereus-only (the toggle) |
| AZ source | MMIO variable slot 0 via btnMMIO_variable_rotator (setup.cs:25649-25658, :31282). Falls back to Reading.AZ (MeterManager.cs Update :16564-16620). | Azimuth from RotorLink::RotorModel (RotatorItem.cpp:884-896). The generic MMIO source fields in ContentPropertyEditor feed only the item's main value. | partial |
| ELE source | MMIO variable slot 1 via btnMMIO_variable_2_rotator. | From the rotor model only (RotatorItem.cpp:891-893). There is no MMIO path. | missing |
| Smoothing | Moves 0.2 × the shortest-path difference per update. ELE clamped to 0-90 (Update :16564-16620). | Same constant (RotatorItem.cpp:119). | match |
| Allow control | chkMeterItemRotatorAllowControl "Allow control", saved to **igs.ShowType**. Enables the command and colour controls (updateRotatorControlControls setup.cs:31321-31335). | No toggle. Control is on whenever a rotor is connected (rotorControllable, RotatorItem.cpp:852-857). | partial |
| Control colour | clrbtnMeterItemRotatorControlColour, saved to **igs.HistoryColor**, default LimeGreen. | Fixed Style::kAmberText. The comment says "Thetis draws these in the control colour (LimeGreen); the plan has them amber" (RotatorItem.cpp:1119-1127). | missing |
| AZ command | txtMeterItemRotatorAZcommand, saved to **igs.Text1**. Default `<PST><AZIMUTH>%AZ%</AZIMUTH></PST>`. %AZ% formatted "000" (SendRotatorMessage :16475-16505). | Replaced by RotorCommandSink::requestRotorTarget (RotatorItem.cpp:1081-1100). | missing (by design) |
| ELE command | txtMeterItemRotatorELEcommand, saved to **igs.Text2**. Default `<PST><ELEVATION>%ELE%</ELEVATION></PST>`. %ELE% formatted "00". | Same replacement as AZ. | missing (by design) |
| STOP command | txtMeterItemRotatorSTOPcommand, saved to **igs.FontFamily1**. Default `<PST><STOP>1</STOP></PST>`. | `sendStop` goes to the rotor link (RotatorItem.cpp:1069-1079). | missing (by design) |
| Escape handling in commands | `\n`, `\r`, `\0` in the command text become control characters (:16501-16503). | n/a | missing |
| "pstRotator" defaults button | bntMultiMeterItemRotator_default_pstRotator fills the three PST strings (setup.cs:31337-31342; designer 64438). Info icon tooltip "%AZ%\r\n%ELE%" (designer 64451). | missing | missing |
| Rotator output 4Char ID | txtRotator_4charID, saved to MMIO GUID slot 2 (setup.cs:25261-25269; load :25996-26000). Commands are enqueued to that node (SendRotatorMessage :16475-16505). | missing (the native rotor link replaces it). | missing |
| Drag to aim, send on release | Drag on the face; on release, send the AZ or ELE command (render :36937-36983, :37126-37172, :37205-37220). The update rate is raised while dragging. | handleMousePress/Move/Release (RotatorItem.cpp:990-1060), then sendTarget. | match |
| Stop circle | Red circle at the centre while hovering. A click sends STOP (MouseUp/Down :16700-16716; render :36930-36937). | sendStop, red circle (RotatorItem.cpp:1069-1079, 1174). | match |
| Native rotor link, local or remote Core | Not in Thetis. | RadioModel routes commands to the local rotor or the remote Core (MainWindow.cpp:14861-14866). | Nereus-only |
| Target markers | Not in Thetis. | targetAzimuthMarker / targetElevationMarker (RotatorItem.h:158-159). | Nereus-only |
| Font | Trebuchet MS 18 (:16403-16440). | Sized from face height (RotatorItem.cpp:390-392). | partial |

Multislice notes:
- The Thetis rotator is not tied to RX1/RX2. It reads MMIO variables and writes to one MMIO node. NereusSDR binds each rotator item to the window's `RadioModel::rotorModel()` (MainWindow.cpp:14864-14866), so a second radio window gets its own rotor. That fits multi-radio use better than Thetis does.
- The Thetis drag state (`_dragging_rotator_degrees`, `_rotator_was_dragging`, MeterManager.cs:36697-36700) lives on the renderer, so it is shared by every rotator in one container. NereusSDR keeps it per item. No action needed.

---

### CLICKBOX: no MeterType; internal helper clsClickBox

Gating: unbuilt in NereusSDR (`UnbuiltFeature::ContainerClickBox`, "Container click box has no host action routing", UnbuiltFeatureList.cpp:155-156). The item is not drawn or hit-tested (MeterWidget.cpp:829-831), and the picker gates it (ContainerContentRegistry.cpp:309).

Thetis:

* clsClickBox (MeterManager.cs:7597-7652) is not a MeterType and has no Setup UI.
* It is created only inside other items: AddSMeterBarMaxBin (:22898), AddPBSNRBar (:23199) and AddAnanMM (:23901, OnlyWhenTX, left button only).
* Clicks are handled in MeterManager MouseUp (:29814-29848) and hit-tested in the container loop (:32737-32745).

NereusSDR: src/gui/meters/ClickBoxItem.{h,cpp}, ClickBoxItemEditor.cpp (no type rows).

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Button mask | Button, default Left \| Right \| Middle (:7607). | Left click only for `clicked()`. The wheel is always handled (ClickBoxItem.cpp:57-69). | partial |
| GotoGroup | Clicking jumps to that display group (:29816-29819). | missing | missing |
| ChangesGroup | Default true. Left click goes to the next display group, right click to the previous one (:29822-29828). | `clicked()` has no receiver. MeterWidget has `setDisplayGroup` (MeterWidget.cpp:866), but nothing cycles groups. | missing |
| PerformsIncDec and RelatedMeterItem | Left click calls RelatedMeterItem.HandleIncrement, right click calls HandleDecrement (:29829-29845, :31175-31184). Used by the S-meter max-bin and PBSNR bars. | `wheelIncrement`/`wheelDecrement` signals (wheel, not click) have no receiver. There is no related-item link. | missing |
| OnlyWhenTX / OnlyWhenRX gating | Checked before hit-testing (:32741). | Base "Only when RX/TX" (MeterWidget.cpp:846-848). | match |
| Invisible | Not drawn. | paint() is empty (ClickBoxItem.cpp:52-55). | match |
| User-addable item with an editor | Not possible in Thetis. | Offered in the registry (ContainerContentRegistry.cpp:120, 288) with BaseItemEditor rows only. | Nereus-only |
| Save | StoreSettings = false (:7612). Rebuilt by the parent item. | serialize saves the base fields (ClickBoxItem.cpp:71-75). | n/a |

Multislice notes:
- Display groups are per container in both Thetis (per meter) and NereusSDR. There is no RX coupling.
- The related items it drives (max-bin hold, PBSNR) are per-RX readings in Thetis. When ported, the click target should follow the parent item's slice binding.

---

### DATAOUT: MeterType.DATA_OUT / clsDataOut

Thetis Setup group: `grpMeterItemDataOutNode`. Save setup.cs:25273-25285, load :26005-26018.

Thetis class: clsDataOut MeterManager.cs:17367-17431.

NereusSDR: src/gui/meters/DataOutItem.{h,cpp}, DataOutItemEditor.cpp.

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Output node | txtDataOutNode_4charID: a 4-char ID turned into a GUID by GuidfromFourChar, saved with SetMMIOGuid(0) (setup.cs:25273-25285). | "Data source ID" free-text UUID (DataOutItemEditor.cpp:98-99), serialized (DataOutItem.h:70-71). | partial (UUID, not 4-char) |
| Send interval | nudDataOutNode_sendinterval: 100-5000 ms, step 10, designer default 1000. Saved to **igs.UpdateInterval**, or 500 when there is no GUID (setup.cs:25273-25285). | missing | missing |
| Output format JSON / XML / RAW | Belongs to the MMIO node (FormatOut), not the item. JSON: dict with "rx" plus lowercase reading names, values formatted "0.0#####" (:17387). XML: `<data><rx/>...` (:17402). RAW: `rx:N:name:value:` (:17417). | "Format" combo on the item (DataOutItemEditor.cpp:56-62), serialized. | partial (setting is on the item instead of the node) |
| Transport | Belongs to the MMIO node (UDP / TCP / serial and so on). | "Connection type" UDP / TCP / Serial / TCP Client on the item (DataOutItemEditor.cpp:75-83). | partial (setting is on the item instead of the node) |
| Value name | Not in Thetis. DataOut sends every reading of the meter. | "Value name" (DataOutItemEditor.cpp:110-111). | Nereus-only |
| Sending | Every interval, sends all of the meter's readings to the node via MultiMeterIO.SendDataMMIO (:17367-17431). | paint() is empty and nothing sends. The only reference outside the item is the registry (ContainerContentRegistry.cpp:121). src/core/mmio has the workers (ExternalVariableEngine, MmioEndpoint, UDP/TCP/serial), but DataOutItem does not use them. | missing |

Multislice notes:
- Every payload carries `rx` = the owning meter's RX, 1 or 2 (MeterManager.cs:17387, 17402, 17417). NereusSDR needs a slice identifier (A-D) and, for remote Core use, which station or radio. A downstream consumer written for Thetis expects "1"/"2". Mapping slice A→1 and B→2, with C and D as 3 and 4, would be a design decision.
- Which readings get sent depends on the meter's RX. In NereusSDR it should follow the container's source (Slice A-D or "Follow selected RX").

---

### VOICERECPLAY: MeterType.VOICE_RECORD_PLAY_BUTTONS / clsVoiceRecordPlay (subclass of clsButtonBox)

Gating: unbuilt in NereusSDR (`UnbuiltFeature::Voice`, MeterWidget.cpp:823-824; ContainerContentRegistry.cpp:310; the context menu hides it, ContainerSettingsDialog.cpp:1462-1466).

Thetis wiring:

* Save: setup.cs:25081-25103. Load: :25750-25760.
* Panel `pnlVoiceRecordPlayback`. The common button-box settings belong to audit D.
* Class: clsVoiceRecordPlay MeterManager.cs:10342-~11600.

NereusSDR: src/gui/meters/VoiceRecordPlayItem.{h,cpp}, VoiceRecordPlayItemEditor.cpp. It is a fixed set of five buttons: REC, PLAY, STOP, NEXT, PREV (VoiceRecordPlayItem.cpp:57).

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Slot count | nudVoiceRecordingPlayback_slots, saved to buttonbox_recordplayback_slots, 1-64, default 8 (setup.cs:25085, 25750). Columns capped at the slot count. | Fixed five transport buttons (kButtonCount, VoiceRecordPlayItem.h). | missing (different model) |
| Per-slot label | buttonbox_recordplayback_label_N, default "Slot N" (setup.cs:25088; MeterManager.cs:10465). | missing | missing |
| Slot locked | ..._locked_N. A locked slot cannot be recorded or deleted (:11415, :11461). | missing | missing |
| Keybind | ..._useskeybind_N and ..._keybind_N (KeyUp :10597, KeyDown :10641). | missing | missing |
| Repeat | ..._canrepeat_N and ..._repeatdelay_N, 2-60 s. Long hold (750 ms, :11342; keybind :10629) starts repeat (setupRepeatTimer :10798). | missing | missing |
| TX gain adjust | ..._txgainadjust_N, -70..70. | missing | missing |
| Ignore temp changes | ..._ignoreplaytempchanges_N and ..._ignorerecordtempchanges_N. | missing | missing |
| Record / Playback mode button | Top-row RECORD_PLAYBACK_MODE toggles `_mode_record` (:11350-11355). Label "Record"/"Playback" (:39125). | A separate REC button. | missing |
| WDSP / PC button | Top-row RECORD_PLAYBACK_WDSPPC toggles `_wdsp` (:11356-11361). Label "PC" (:39098). WDSP records the radio audio; PC records a PC input device (:11483-11487). | missing | missing |
| Click semantics | Click a slot to play or record it. Click again to stop. Shift-click while in playback mode quick-records. Right-click in record mode deletes (handleClicked ~11386-11560). | `voiceAction(index)` emitted on left click (VoiceRecordPlayItem.cpp:68-69). ContainerWidget re-emits it (ContainerWidget.cpp:1952-1953), but nothing receives `ContainerWidget::voiceAction`. | missing |
| Export / load wav, open folder, 4-char copy | pnlVoiceRecordPlayback buttons. The LED-indicator 4-char ID is made with Common.FourChar (:10407). | missing | missing |
| Drag to reorder slots | Dragable (:11142), MoveButton (:11207). | missing | missing |
| Fade blocks clicks | MouseDown/Up return early while faded (:11268-11269, :11297-11298). | n/a (no fade) | missing |
| Columns | Button-box columns. | Serialized (VoiceRecordPlayItem.cpp:72-76). | partial |

Multislice notes:
- WDSP recording and playback pick the stream with `_owningmeter.RX == 1 ? 0 : 1` (MeterManager.cs:11483, 11538), so only RX1 or RX2. NereusSDR needs:
  - the record source as a slice (A-D, or the container's source);
  - playback to go to the TX of the radio that owns that slice;
  - a decision about where files live for a remote Core (client disk or Core disk) and which side mixes the audio into TX.
- The wav list player uses `"wavelistplayer_" + _owningmeter.RX` (:12607). It needs the same slice mapping.
- MOX state (`MOX` override :10528) is console-wide in Thetis. NereusSDR should use the TX state of the slice's radio.

---

### Summary

Counts per item (rows in each table; "partial" includes the data-loss rows):

| Item | missing | partial | match | Nereus-only | n/a |
|---|---|---|---|---|---|
| CLOCK | 2 | 9 | 4 | 1 | 2 |
| VFO | 22 | 10 | 0 | 2 | 1 |
| DIAL | 13 | 8 | 0 | 0 | 0 |
| TEXTOVERLAY | 10 | 8 | 4 | 0 | 0 |
| WEBIMAGE | 9 | 3 | 1 | 2 | 0 |
| FILTERDISPLAY | 17 | 6 | 6 | 1 | 0 |
| ROTATOR | 9 | 11 | 10 | 3 | 0 |
| CLICKBOX | 3 | 1 | 2 | 1 | 1 |
| DATAOUT | 2 | 3 | 0 | 1 | 0 |
| VOICERECPLAY | 13 | 1 | 0 | 0 | 0 |
| **Total** | **100** | **60** | **27** | **11** | **4** |

ROTATOR "missing" includes three rows marked "missing (by design)", where the native rotor link replaces the Thetis MMIO command strings.

Five biggest gaps:
1. **The VFO display is mostly missing.**
   - No VFO B or Both, titles, band text, lock or sync icons.
   - None of the click, keyboard or screen interactions (band, mode, filter, tune step, split, TX, lock, sync).
   - TX and split states are never fed.
   - Legacy colour edits are lost on reload.
2. **Text overlay misses its core behavior.**
   - It draws both lines at once instead of swapping RX and TX text on MOX.
   - No placeholder variables (clock, VFO, MMIO, CAT), no LED gating, no offsets.
   - Text-back colours and flags are lost on reload.
3. **Data out sends nothing.** There is no send interval, and format and transport sit on the item instead of the MMIO node. The 4-char node IDs are raw UUIDs.
4. **The dial is paint-only.** No tuning, acceleration, VFO select or lock, and none of its eight behavior settings.
5. **The clock (named by JJ) is incomplete.**
   - The legacy CLOCK loses its colours on reload.
   - It has no background and the title colour is not disabled with the title.
   - 12 h format and label case differ from Thetis, and the composite uses a different date format.
   - It publishes no clock readings, so the time and date overlay tokens cannot work.

Runners-up: filter display interaction and labels (drag edges, notches, MNF/SNAP/FIT, zoom); the web image background chain and presets; the rotator Dark Mode and Padding settings that are stored but do nothing; voice record/play as a slot model.

Bugs noticed (one-line fixes, in scope for whoever owns these items):
- `ClockItem::serialize` drops time, date and title colours (ClockItem.cpp:116-121).
- `VfoDisplayItem::serialize` drops all six colours (VfoDisplayItem.cpp:204-209).
- `TextOverlayItem::serialize` drops textBackColour1/2 and showTextBack1/2 (TextOverlayItem.cpp:272-303).
- `FilterDisplayItem::serialize` drops the text colour (FilterDisplayItem.cpp:586-607).
- RotatorItem.cpp:394 cites "MeterManager.cs:35275" for a red "N" that Thetis does not draw (Thetis :36809 uses OuterTextColour).

Open questions for a human:
1. MMIO identity: should NereusSDR adopt Thetis 4-char IDs for MMIO nodes (data out, rotator, web image chain, text overlay LED logic), or keep raw UUIDs with a picker? Recommendation: add 4-char IDs, since Thetis users' setups and the overlay tokens depend on them.
2. Rotator: keep only the native rotor link, or also port the Thetis MMIO command strings (pstRotator, the "Allow control" toggle, LimeGreen control colour)? Recommendation: keep the native link as the default and port the MMIO path as an option for parity.
3. Fade: should legacy items get Thetis dim-fade (the composite already dims to 25%), separate from the hide-style "Only when RX/TX"? Recommendation: yes, add "Fade on RX/TX" as Thetis has it.
4. Thetis text overlay TX-LED gating bug (MeterManager.cs:20552-20559): port it faithfully or fix it? Recommendation: fix, and cite the upstream line.
5. Default colours: Thetis defaults (clock Orange/Gray/Gray, dial White/Black/Blue and so on) or the current NereusSDR palette? This is a visual design call.
6. VFO display mode numbering: Thetis BOTH=0, A=1, B=2 vs NereusSDR VfoA=0, VfoB=1, VfoBoth=2. Any Thetis import must map them. Should the NereusSDR enum be renumbered to match?
7. Slice meaning of Thetis tokens and VFO labels: in a Slice C container, what do "VFO A", "VFO B", "VFO Sub", %vfoa%, %vfob% and %rx2% mean? Recommendation: A = the container slice, B = its split/TX partner, Sub = not shown until supported.
8. Voice record/play: adopt the Thetis slot model (1-64 slots, labels, keybinds, repeat, WDSP/PC), or keep the five-button DVK transport? Recommendation: Thetis slot model.
9. Click box: keep it user-addable (Nereus-only), or make it internal as in Thetis?
10. meter.clock composite: change the date format "yyyy-MM-dd" to Thetis "ddd d MMM yyyy" and the 12 h format to Thetis "h:mm:ss" plus a small am/pm? Keep clockMode/showDate as Nereus extras?

---

## Part: Audit D: button grids and container-level options

Scope: BANDBTNS, MODEBTNS, FILTERBTNS, ANTENNABTNS, TUNESTEPBTNS, OTHERBTNS (with the
`control.*` shortcuts), DISCORDBTNS (retirement check) and the container-level options.

Pins: Thetis v2.10.3.15 (`3759d09`), paths relative to `Project Files/Source/Console/`.
NereusSDR origin/main `b85e4127d`, paths relative to `src/`.

Read-only audit. Nothing in the repo was edited, built or tested.

Persistence fact used throughout: a NereusSDR grid persists only what its `serialize()`
writes into `ContentEntry.config["legacyRecord"]` (gui/containers/ContainerContentRegistry.cpp:336,
:345, :500-516). Overrides are diffs of those same fields. For all six grids that is
x, y, w, h, bindingId, zOrder, columns, active index (not Antenna or Other) and a 32-bit
visibleBits (gui/meters/BandButtonItem.cpp:114-120, OtherButtonItem.cpp:214-220, and the same
pattern in the other four). Anything else the editor changes is lost on reload.

---

### Button grid common options: clsButtonBox (shared by all six grids)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Columns | `buttonbox_columns`, clamped per type: band 1-15, mode 1-12, filter 12 or 9, antenna and tunestep and other = visible count (setup.cs:25815-25870) | Editor spin 1-32, no per-type clamp (gui/containers/meter_property_editors/ButtonBoxItemEditor.cpp:48); persisted | partial |
| Border width | `buttonbox_border`, 0-1, default 0.05 (setup.cs save :25077-25187, load :25784-25900) | Editor 0-0.1 (ButtonBoxItemEditor.cpp:58), item default 0.005; **not serialized** | partial |
| Margin | `buttonbox_margin`, 0-1, default 0 (setup.cs:25784-25900) | Editor 0-0.5 (ButtonBoxItemEditor.cpp:68); **not serialized** | partial |
| Corner radius | `buttonbox_radius`, 0-2, default 0 (setup.cs:25784-25900) | Editor 0-20 px (ButtonBoxItemEditor.cpp:78), default 3.0; **not serialized** | partial |
| Height ratio | `buttonbox_height_ratio`, 0.01-2, default 0.5 (setup.cs:25784-25900) | Editor 0.1-5 (ButtonBoxItemEditor.cpp:88), default 1.0; **not serialized** | partial |
| Use indicator | `buttonbox_use_indicator`, default false (setup.cs:25784-25900) | missing; indicator always paints when on (gui/meters/ButtonBoxItem.cpp:327) | missing |
| Indicator border width | `buttonbox_indicator_border`, default 0.05 | per-button indicatorWidth 0.005, no editor, not serialized | missing |
| Indicator type | `buttonbox_indicator_type` (RING .. TEXT_ICON_COLOUR), default RING (MeterManager.cs:13627-13900) | Enum matches (ButtonBoxItem.cpp:336), no editor, not serialized | partial |
| On / off colors | `buttonbox_on_colour` CornflowerBlue, `buttonbox_off_colour` LightGray (setup.cs:25784-25900) | Fixed STYLEGUIDE colors per button, no editor, not serialized | missing |
| Use off color | `buttonbox_use_off_colour`, default false | missing | missing |
| Fill / hover / border / click colors | `buttonbox_fill_colour` Black, `hover` LightGray, `border` White, `click` Orange | Fixed defaults (#1a2a3a, #204060, #205070, #00b4d8), no editor | missing |
| Font family / style / color | FontFamily1/FontStyle1 + `buttonbox_font_colour` White; font color hidden for ANTENNA (setup.cs:26779-26850) | Bold system font, 40% of button height, 11 px minimum (ButtonBoxItem.cpp:312-319); no options | missing |
| Font scale / shift X / shift Y | `buttonbox_font_scale` 0.01-2 default 1, shifts ±0.25 | missing | missing |
| Fix text size | `buttonbox_fix_text_size`, default false | missing | missing |
| Use icons | `buttonbox_use_icons`, default true, shown only for OTHER (setup.cs:26779-26850) | missing (no icons at all) | missing |
| Fade on RX / TX | igs.FadeOnRx/FadeOnTx; MouseUp returns while faded (MeterManager.cs:9348-9349, :13144-13208) | Click and paint guards match (ButtonBoxItem.cpp:265-267, :390-391); editor rows (ButtonBoxItemEditor.cpp:100, :109); **not serialized** | partial |
| Copy / paste item settings | igs copy block carries button keys (setup.cs:27020-27073) | Container dialog Copy/Paste settings disabled (gui/containers/ContainerSettingsDialog.cpp:661-664) | missing |
| Single-control size and stack layout | n/a | Nereus layout exceptions (ButtonBoxItem.cpp:192-243) | Nereus-only |

Multislice notes:
- None of these options is per-receiver in Thetis. No slice work needed here.
- The real problem is persistence: four of the editor's six rows do nothing after a reload.

---

### BANDBTNS: BAND_BUTTONS / clsBandButtonBox

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Button set | 15 buttons, three layouts by band group (MeterManager.cs:12925+) | 15 fixed labels 160m..10m, 6m, GEN, WWV, XVTR, 2m (gui/meters/BandButtonItem.cpp:68) | partial |
| Band groups GEN / HF / VHF | Group buttons switch the layout via SetBandPanel; RX1 follows the console panel, RX2 its own (MeterManager.cs:12802-13300, enum :234-240) | missing | missing |
| HF layout extras: SWL, VHF jump, blank slot | HF layout has VHF at 12, WWV 13, SWL 14 (MeterManager.cs:12925+) | WWV only; no SWL, no VHF jump | missing |
| GEN layout (BLMF..B11M) | Yellow GEN band buttons plus HF (MeterManager.cs:12925+) | missing | missing |
| VHF layout (VHF0..13 transverter names) | Uses console GetVHFEnabled/GetVHFText (MeterManager.cs:12925+) | single XVTR button, hidden until Transverters built | missing |
| 2m button | not a Thetis band button | 2m, disabled through stationLink reason (gui/containers/ContainerButtonDispatcher.cpp:519) | Nereus-only |
| Auto height from rows | ig height sized from row count (MeterManager.cs:12925+) | fixed heightRatio grid | partial |
| Left click sets band | setBand: BandPreChangeHandlers (RX1) or SetupRX2Band (RX2) (MeterManager.cs:13224) | clickBand -> RadioModel::onBandButtonClicked(slice, band) (ContainerButtonDispatcher.cpp:535, models/RadioModel.cpp:18499) | match |
| Locked VFO refusal | abortForLockedVFO, VFOALock/VFOBLock (MeterManager.cs:13226, :13244) | slice->locked() check (RadioModel.cpp:~18540) | match |
| Right click bandstack popup | PopupBandstack(RX, band) (MeterManager.cs:13216) | signal exists (BandButtonItem.cpp:108); handler says "Band stacking is not ready." (gui/MainWindow.cpp:14779-14781, :15232) | missing |
| Active band lighting | BandChanged / VHFUpdate / BandPanelsChanged (MeterManager.cs:12802-13300) | applyBand lights from slice frequency (ContainerButtonDispatcher.cpp:519) | match |
| Visible buttons | via layout only, no bitfield | 32-bit visibleBits, no editor beyond common rows | Nereus-only |

Multislice notes:
- Thetis RX==2 guard: no change when RX2 is off (MeterManager.cs:13154, comment "prevent any change when rx2 is disabled, but this container is for rx2"). NereusSDR replaces this with sliceFor(rxSource) plus the slice refusal reasons (ContainerButtonDispatcher.cpp:153). That works for slices A-D.
- Band group (GEN/HF/VHF) is per RX in Thetis. A port would need a per-slice or per-container band group state.
- **PR 635 overlap:** RxSectionContent and NereusBandButton also render band buttons. Any shared band list or group work must line up with that PR. No edits proposed there.

---

### MODEBTNS: MODE_BUTTONS / clsModeButtonBox

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Button set | 12 in DSPMode order: LSB, USB, DSB, CWL, CWU, FM, AM, DIGU, SPEC, DIGL, SAM, DRM (MeterManager.cs:10071+, enums.cs:253-271) | 10: LSB, USB, DSB, CWL, CWU, FM, AM, SAM, DIGL, DIGU (gui/meters/ModeButtonItem.cpp:60) | partial |
| SPEC, DRM | present | missing | missing |
| Order | DSPMode enum order | different order (SAM before DIGL/DIGU) | partial |
| Column limit | 1-12 (setup.cs:25815-25870) | 1-32, default 5 (ModeButtonItem.cpp:68) | partial |
| Left click sets mode | setMode: RX1DSPMode or RX2DSPMode (MeterManager.cs:10275) | onContainerModeClicked (MainWindow.cpp:15204) | match |
| Locked VFO refusal | abortForLockedVFO (MeterManager.cs:10262, :10277) | returns if slice->locked() (MainWindow.cpp:15204+) | match |
| Active mode lighting | from ModeVfoA/ModeVfoB | refreshContainerMeter per slice (MainWindow.cpp:14959) | match |

Multislice notes:
- Thetis RX2 guard at MeterManager.cs:10256. NereusSDR uses sliceFor(rxSource), so slices C and D work.

---

### FILTERBTNS: FILTER_BUTTONS / clsFilterButtonBox

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Button set | RX1: F1-F10, VAR1, VAR2 (12). RX2: F1-F7, VAR1, VAR2 (9) (MeterManager.cs:7700+) | 12 for every slice (gui/meters/FilterButtonItem.cpp:61) | partial |
| Column limit | max 12, or 9 when RX==2, comment "rx2 only has 9 filter buttons" (setup.cs:25826) | 1-32, default 4 | partial |
| VAR1 / VAR2 | live variable filters | hidden until VariableFilters built | missing |
| Labels from filter names | FilterChanged sets text, not in FM/DRM/SPEC (MeterManager.cs:7700-8023) | FilterPresetStore presetsForMode, 50 Hz tolerance (MainWindow.cpp:14959+) | match |
| Left click | setFilter sets RX1Filter / RX2Filter (MeterManager.cs:7956) | onContainerFilterClicked (MainWindow.cpp:15216) | match |
| Match TX filter to RX | MatchTXFilterToRXFilter() after set (MeterManager.cs:7966, :7974) | not called | missing |
| Right click context menu | PopupFilterContextMenu(RX) (MeterManager.cs:7943) | Edit/reset preset menu (MainWindow.cpp:15241) | partial |
| Lock refusal | none in Thetis setFilter | none | match |

Multislice notes:
- RX2 guard at MeterManager.cs:7935. The 9-button RX2 limit is a Thetis RX2 hardware-era rule. NereusSDR gives every slice 12. Keep 12 for slices B-D, or apply 9 to slice B only? Open question.
- MatchTXFilterToRXFilter needs a "which slice is TX" rule in NereusSDR (txBoundSlice exists).

---

### ANTENNABTNS: ANTENNA_BUTTONS / clsAntennaButtonBox

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Button set | 10: Rx Ant 1-3, Aux 0-2 (BYP, EXT1, XVTR names), Tx Ant 1-3, Rx/Tx Ant toggle (MeterManager.cs:9669-9800) | 10: Rx1-3, Aux1, Aux2, XVTR, Tx1-3, Rx/Tx (gui/meters/AntennaButtonItem.cpp:64) | match |
| Aux labels from radio | GetRXAuxName(n) (MeterManager.cs:9732-9740) | setHpsdrSku relabels 3-5 from SkuUiProfile (AntennaButtonItem.cpp:129) | match |
| Per-button visibility toggles | `buttonbox_rx1, rx2, rx3, tx1, tx2, tx3, byp, ext1, xvtr, rxtxant`, all default true (setup.cs:25800-25811, panel pnlButtonBox_antenna_toggles :26779-26850) | visibleBits 1023 default (AntennaButtonItem.cpp:75); no editor for toggles | partial |
| Column limit | getTotalColumnsNeededForAntennaButtons (setup.cs:25834, :32113) | 1-32, default 5 | partial |
| Auto height | rows from visible count (MeterManager.cs:9803-9817) | fixed heightRatio | partial |
| On-state logic | XVTR / BYP / EXT1 rules, no-TX antenna checks (MeterManager.cs:9700-9790) | per-slice lighting in refreshContainerMeter (MainWindow.cpp:14959) | partial |
| RX antenna click | SetRXAntenna(n, rx1_band) (MeterManager.cs:9975) | setRxAntenna("ANTn") (MainWindow.cpp:15291) | match |
| Aux click | SetAuxAntenna(n, band, byp, ext1) (MeterManager.cs:9978) | sku rxOnlyLabels for 3-4 (MainWindow.cpp:15291+) | partial |
| TX antenna click | SetTXAntenna(n, tx_band), comment "[2.10.3.9]MW0LGE fix, was using _rx1_band" (MeterManager.cs:9981) | setTxAntenna on txBoundSlice or this slice (MainWindow.cpp:15291+) | match |
| Rx/Tx toggle | console.ToggleRxTxAnt() (MeterManager.cs:9986-9992) | no counterpart, documented (MainWindow.cpp:15301-15304) | missing |
| No-Alex boards | n/a | setHasAlex disables clicks (AntennaButtonItem.cpp:142) | Nereus-only |
| Font color option | hidden for ANTENNA (setup.cs:26779-26850) | n/a (no font color anywhere) | n/a |

Multislice notes:
- Thetis antenna grid always uses VFO A's band (`_rx1_band = BandVfoA`, MeterManager.cs:9640) and the TX band. It ignores the container's RX. NereusSDR follows the container slice, which is a deliberate multislice change.
- Antennas are per band on the radio, not per slice. Two slices on one band share one antenna. Need a rule for that.

---

### TUNESTEPBTNS: TUNESTEP_BUTTONS / clsTunestepButtons

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Step list | console.TuneStepList, 26 steps 1 Hz .. 10 MHz (console.cs:1955-1980) | 7 fixed steps 1, 10, 100, 1k, 10k, 100k, 1M (gui/meters/TuneStepButtonItem.cpp:60, :82) | partial |
| Visible steps | `buttonbox_tunestep_bitfield`, default 1023 (first 10) (setup.cs:25077-25187) | visibleBits over 7 steps | partial |
| Visibility editor | ucTunestepOptionsGrid initialised from TuneStepList (setup.cs:26779-26850) | missing | missing |
| Column limit | checked count (setup.cs:25815-25870) | 1-32, default 4 | partial |
| Label | name with "Hz" stripped | short labels | match |
| Click | sets global console TuneStepIndex (MeterManager.cs:8242) | slice->setStepHz (MainWindow.cpp:15323) | partial |
| Auto height | ig height from rows | fixed heightRatio | partial |

Multislice notes:
- Thetis tune step is global, not per RX. NereusSDR sets it per slice. Not wrong for multislice, but it is a behavior difference. Flag for the maintainer.

---

### OTHERBTNS: OTHER_BUTTONS / clsOtherButtons (plus `control.*` shortcuts)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Button catalog | 12 bit groups x 31: General, Noise/ATT, VFOs, Display, Audio/DSP, mic/comp steps, AGC, Hardware, RF Power, Forms, CWX, Macros (ucOtherButtonsOptionsGrid.cs:529-700) | 34 core ids + 31 macros (gui/meters/OtherButtonItem.cpp:73) | partial |
| Missing ids | NR, NR1-4, NB, NB1-2, MNF+, ATT steps, NF, SPLT, A>B, 0 Beat, A<B, IF>V, A<>B, RIT/XIT/0, SYNC, LockA/B, TS+/-, STACK+/-, Pause, Peak Blobs, Cur Info, Spots, Fill, PAN +/-/Centre, ZTB, zooms, A-Peak, Mute All, SQL group, MIC, COMP, VOX, DEXP, RX/TX EQ, TX Filter, CFC, AF/BAL/SAF/SBAL steps, MIC/COMP/VOX steps, Leveler, PhaRot, Wav Record, AGC modes and +/-, Dither, Random, SR 48k..1536k, Drive/Tune steps, Forms, CWX (same cite) | missing | missing |
| Id numbering | bit_group*32 + bit (ucOtherButtonsOptionsGrid.cs:529+) | own enum order, Macro0 = 64 (gui/meters/OtherButtonItem.h:90) | partial |
| Visible bits storage | `buttonbox_other_buttons_bitfield_0..11` (setup.cs:25077-25187) | single uint32; setVisibleBits stops at i<32 (gui/meters/ButtonBoxItem.cpp:133-136) | partial |
| Visibility editor | full grid with captions and tooltips (setup.cs:26779-26850) | 15 catalog checkboxes only (meter_property_editors/OtherButtonItemEditor.cpp:70) | partial |
| Reorder (alt-drag, shift-swap) | `_map` persisted as `buttonbox_button_map` (MeterManager.cs:9353-9385) | missing | missing |
| Reset layout button | btnOtherButtons_reset_layout (setup.cs:26779-26850) | missing | missing |
| Macro buttons M1-M31 | handleMacroButtonPress (MeterManager.cs:9409-9413, :9428) | ids exist, hidden until MacroButtons built | missing |
| Macro settings | type OFF/ON/TOGGLE/LED/CONT_VIS/CAT, on/off text, notes, open/close containers, MMIO, LED, CAT macro, run on visible (ucOtherButtonsOptionsGrid.cs:260-520) | struct exists (OtherButtonItem.h:93-120), not serialized, no editor | missing |
| Icons | icon_on / icon_off per id (ucOtherButtonsOptionsGrid.cs:529+) | missing | missing |
| Right click | opens related Setup tab (console.cs:51623+) | missing | missing |
| Power | POWER toggles PowerOn (console.cs:51353) | omitted by maintainer decision | n/a |
| SPLT label | "QSPL" when RX2 off and quick split on (MeterManager.cs:8741-8780) | no SPLT | missing |
| Action routing | console.DoOtherButtonAction(RX, id, button) (MeterManager.cs:9424, console.cs:51337) | ContainerButtonDispatcher::click (gui/containers/ContainerButtonDispatcher.cpp:417) | partial |
| `control.*` single-button shortcuts | n/a | 15 shortcuts mapping to one OtherButton id (gui/containers/ContainerControlCatalog.h:21-29, ContainerContentRegistry.cpp:282, :330-336) | Nereus-only |

Routed ids and their scope (NereusSDR ContainerButtonDispatcher.cpp:223, :417):

| Id | Thetis scope (console.cs) | NereusSDR scope | Status |
| --- | --- | --- | --- |
| MON, TUN, MOX, 2TON, PS-A | global (:51355-51360) | global | match |
| DUP | global, chkRX2SR (:51359) | display duplex hook | match |
| ANF, SNB | per rx (:51392, :51401) | sliceFor(rxSource) | match |
| MNF | global, "// no rx2" (:51402) | global notch model | match |
| AVG | per rx (:51415) | DisplayAveraging not built | missing |
| Peak, CTUN | per rx (:51416, :51417) | pan showing the slice (ContainerButtonDispatcher.cpp:218) | match |
| VAC1 / VAC2 | global (:51418-51419) | AppSettings audio/VaxN/Enabled | match |
| Mute, Bin | per rx (:51420, :51422) | per slice | match |
| SubRX | per rx (:51423) | TwoReceiverLayout not built | missing |
| PanSwap | global, "// no rx2" (:51424) | not built | missing |
| RX2 | toggles RX2Enabled (:51354) | not built | missing |
| Rec, Play | wave quick record (:51361-51382) | Voice not built | missing |
| xPA | OC override (:51455) | OcExtras not built | missing |
| Display modes | per rx SetDisplayMode (:51465-51475) | DisplayMode not built | missing |

Multislice notes:
- Thetis passes `_owningmeter.RX` (1 or 2) to every action. NereusSDR passes rxSource 1..4 and resolves the slice. Good basis.
- For future ids: NR, NB, AGC, SQL, AF, ATT, RIT/XIT and display mode are per RX in Thetis. They should be per slice. ATT is per ADC, so slice-to-ADC mapping is needed.
- VFO A/B ids (A>B, A<B, SWAP, SPLT) are "// no rx2" in Thetis. Slices A-D need a rule for which pair they act on.
- Zoom, pan and ZTB act on the console display in Thetis. Per-pan in NereusSDR.

---

### DISCORDBTNS: DISCORD_BUTTONS / clsDiscordButtonBox

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Discord grid | clsDiscordButtonBox (MeterManager.cs:13303), MeterType DISCORD_BUTTONS (:224), column max 12 (setup.cs:25815-25870) | Listed as "Discord control (removed)", unavailable, reason "This control was retired" (gui/containers/ContainerContentRegistry.cpp:319) | n/a |

Multislice notes:
- Retired. Confirmed. No slice work.

---

### Container: ucMeter / setup.cs container section

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
| --- | --- | --- | --- |
| Add RX1 / RX2 container | btnAddRX1Container / btnAddRX2Container -> AddMeterContainer(1 or 2) (setup.cs:24464, :24473) | New container; source picked later | partial |
| RX assignment | radContainer_rx1_data / rx2_data -> SetContainerRX (setup.cs:34484, :34498; MeterManager.cs:2837) | Slice combo, Slice A-D (gui/containers/ContainerSettingsDialog.cpp:1048-1053) | match |
| Follow the selected RX | n/a | "Active slice" in the source chip (gui/containers/ContainerWidget.cpp:763-783), rxSourceMode followSelectedRx (gui/containers/ContainerManager.cpp:1816); not in the dialog's Slice combo | Nereus-only |
| Show on RX / Show on TX | ShowContainerOnRX/TX (setup.cs:24754, :24763; MeterManager.cs:2946) | Show RX / Show TX (ContainerSettingsDialog.cpp:1063-1068); hiddenByOwnFlags (ContainerManager.cpp:666-672) | match |
| Hide when RX not used | hide if RX==2 and RX2 off; comment "atm this is only a consideration for rx2" (setup.cs:34512; MeterManager.cs:2784-2801, :5910, :5923, :6005) | checkbox disabled, "unavailable. Saved values are retained." (ContainerSettingsDialog.cpp:1125-1130); stored (ContainerManager.cpp:1005) but nothing hides | missing |
| Minimizes with console | floating form hides on console minimize (setup.cs:31663; MeterManager.cs:2745; frmMeterDisplay.cs:114-139) | checkbox disabled (ContainerSettingsDialog.cpp:1121-1128); FloatingContainer::onConsoleWindowStateChanged ported (gui/containers/FloatingContainer.cpp:167-185) but has no caller | missing |
| Title bar off | chkContainerNoTitle -> NoTitle (setup.cs:27526; MeterManager.cs:2705) | Header combo Shown/Reveal/Hidden, or Hide title (ContainerSettingsDialog.cpp:1119-1145) | match |
| No controls | ucMeter NoControls, serialize field 13 (ucMeter.cs:894, :1104-1105) | noControls in config (ContainerManager.cpp:991-1007), no dialog control found | partial |
| Border | chkContainerBorder (setup.cs:26609; ucMeter.cs:642) | Border checkbox (ContainerSettingsDialog.cpp:950-1200) | match |
| Background color | clrbtnContainerBackground (setup.cs:26619; MeterManager.cs:3218) | BG color button (ContainerSettingsDialog.cpp:1017-1028) | match |
| Lock | chkLockContainer; also disables recover, delete, add/remove/move item (setup.cs:32381-32395) | Lock check (ContainerSettingsDialog.cpp:1116), hidden in the workspace look | partial |
| Auto height | chkMultiMeter_auto_container_height (setup.cs:31878; ucMeter.cs:903) | autoHeight in ContainerDocument; no dialog control found | partial |
| Notes / title text | txtContainerNotes; first line shows in title; "|" stripped (setup.cs:24772; ucMeter.cs:625-639, :886-889) | Title field = notes (ContainerSettingsDialog.cpp:1005-1009); title "Slice X" + first line (ContainerWidget.cpp:979) | match |
| Highlight | chkContainerHighlight (setup.cs:24738; MeterManager.cs:3243) | Highlight check; disabled in drafts (ContainerSettingsDialog.cpp:504) | match |
| Pin on top | ucMeter PinOnTop, title button (ucMeter.cs:791, :969) | pinOnTop config, title button | match |
| Enabled / hidden by macro | enableContainer, driven by Show RX/TX and macros (MeterManager.cs:2984-3066) | enabled, hiddenByMacro in config (ContainerManager.cpp:666-671); no macros drive it | partial |
| Delete | btnContainerDelete (setup.cs:24673) | Delete, "Remove / return contents" | match |
| Duplicate | btnContainer_dupe (setup.cs:35199) | Duplicate (ContainerSettingsDialog.cpp:1175-1216) | match |
| Save / load container file | btnContainer_save / load (setup.cs:35095, :35120) | Save Container (.nscontainer) | match |
| Recover container | btnRecoverContainer -> RecoverContainer (setup.cs:35267; MeterManager.cs:6539) | showHiddenContainer (ContainerManager.cpp:3014-3050) | match |
| Placement / anchor | floating or docked only | Panel / Overlay / Floating + 8 anchors (ContainerSettingsDialog.cpp:1162-1164) | Nereus-only |
| Layout mode | n/a | Canvas / Vertical stack / Saved legacy canvas | Nereus-only |
| Fade | per item only (FadeOnRx/Tx) | per item | n/a |

Multislice notes:
- **How NereusSDR binds a container today:** ContainerDocument.config holds `rxSource` (1..4) or `sliceId`, or `rxSourceMode = followSelectedRx`. ContainerSourceAdapter resolves in order: session, follow, sliceId, rxSource, inherited (gui/containers/ContainerSourceAdapter.cpp). Buttons go through ContainerButtonDispatcher::sliceFor(rxSource) with ownership refusals (ContainerButtonDispatcher.cpp:153). The Thetis importer maps RX1/RX2 to slice A/B (gui/containers/LegacyContainerImporter.cpp:40-48).
- **RX1/RX2 assumptions:**
  - "Hide when RX not used" only ever applies to RX2 in Thetis (MeterManager.cs:2801).
  - Minimize-with-console code only knows sources 1 and 2 (FloatingContainer.cpp:174-180, ported from frmMeterDisplay.cs:126-139). Sources 3 and 4 never re-show.
- **Per-slice "RX not used":** NereusSDR needs a rule for slices A-D. Proposal: the slice does not exist or is not active. A container following the selected slice never hides.
- **Per-pan:** containers bind to slices, not pans. Peak and CTUN reach the pan through the slice.

---

### Summary

Missing / partial counts per item:

| Item | Missing | Partial |
| --- | --- | --- |
| Common clsButtonBox options | 10 | 7 |
| BANDBTNS | 5 | 2 |
| MODEBTNS | 1 | 3 |
| FILTERBTNS | 2 | 3 |
| ANTENNABTNS | 1 | 5 |
| TUNESTEPBTNS | 1 | 5 |
| OTHERBTNS (main table) | 8 | 5 |
| OTHERBTNS (routed id scope) | 7 | 0 |
| DISCORDBTNS | 0 | 0 |
| Container | 2 | 5 |
| **Total** | **37** | **35** |

Five biggest gaps:
1. **Grid styling does not persist.** serialize() writes only columns, active index and visibleBits. Border, margin, radius, height ratio and fade from the editor are lost on reload. All Thetis color, font, indicator and icon options are missing.
2. **OTHERBTNS catalog is a fraction of Thetis.** About 120 ids are missing (NR/NB, VFO ops, AGC, SQL, ATT, steps, forms, CWX). A 32-bit visibleBits cannot hold the 12 Thetis bitfield groups. There is no macro editor or persistence and no reorder map.
3. **Band groups are missing.** No GEN/HF/VHF panels, SWL or VHF transverter names. Right-click bandstack is not built.
4. **"Hide when RX not used" and "Minimizes" are dead.** The checkboxes are disabled, the values are stored, and nothing reads them. The ported minimize handler has no caller and only knows sources 1 and 2.
5. **Tune step and filter gaps.** 7 steps instead of 26, with no visibility editor. MatchTXFilterToRXFilter is never called. VAR1/VAR2 are hidden. Mode lacks SPEC and DRM.

Open questions:
1. Filters: give slices B-D 12 buttons, or apply the Thetis 9-button limit (setup.cs:25826) to slice B?
2. Tune step: global (Thetis) or per slice (NereusSDR today)?
3. "RX not used" for slices A-D: slice absent, inactive, or not shown on any pan?
4. Antenna grid: Thetis uses VFO A's band only (MeterManager.cs:9640). Keep NereusSDR's follow-the-slice? What happens when two slices share a band?
5. OTHERBTNS ids: adopt the Thetis bit_group*32+bit numbering so Thetis bitfields and macros import directly?
6. Should the dialog's Slice combo offer "Active slice", like the header chip?
7. Band grid overlap with PR 635 (RxSectionContent / NereusBandButton): should the band list and groups be shared? This audit proposes no edits there.

---

## Part: Audit E: Longpath instruments, S-meter dial, TX section items

Scope: everything `LongpathContent::registerAll` registers (`longpath.frequency`,
`longpath.frequencyCustom`, `longpath.needle`, `longpath.bar`, `longpath.filter`),
the S-meter dial (`sMeter.dial` from SMeterDialContent.cpp, and the `applet:s_meter`
singleton), and the seven TX section items from TxSectionContent.cpp (`tx.monitor`,
`tx.power`, `tx.filter`, `tx.chain`, `tx.vox`, `tx.processing`, `tx.keying`).

Trees: NereusSDR worktree at b85e4127d; Thetis `Project Files/Source/Console` at tag
v2.10.3.15. Every NereusSDR path below is relative to the worktree; every Thetis path is
relative to `Thetis/Project Files/Source/Console`.

### Facts that apply to the whole group

- **Workspace preview only.** All 13 types are registered only inside
  `if (WorkspacePreview::enabled())` (src/gui/MainWindow.cpp:11686-11703;
  registerAll :11697, registerDial :11698, registerTxSectionContent :11702). Outside
  the workspace preview none of them exist; `applet:s_meter` is always registered.
- **What Longpath is.** Longpath (github.com/oe5sos/Longpath, Martin Fischer OE5SOS) is
  a GPLv3 fork of NereusSDR. NereusSDR ports its instruments at pin 383f0589 / @551576e
  (docs/attribution/LONGPATH-PROVENANCE.md:14-25, rows :95-114). The instruments are
  Longpath designs, not Thetis ports. Only some values come from Thetis (S-scale
  -127/-73/-13 dBm, ALC scale; src/gui/instruments/ReadingSource.cpp:238-246, :452-466).
  So most "Thetis equivalent" rows below compare against the nearest Thetis item, not
  against a source the code was ported from.
- **How settings are edited.** The widget-factory types have no MeterItem. Their only
  settings UI is the declared `ContentProperty` schema, which ContentPropertyEditor shows
  as "Appearance and behavior" (src/gui/containers/ContentPropertyEditor.cpp:306-321).
  Reading-kind properties appear under "Advanced bindings" (:384-386). Every declared
  type also gets a per-entry **Source** combo: Container default, Slice A to D, or
  Follow selected RX (:394-407). It also gets a **Radio session** field (:408-416).
  The property kinds are only Reading, Choice, Toggle, Text, Integer and Color
  (src/gui/containers/ContainerContentRegistry.h:46). There is no floating-point kind,
  so Thetis's fractional options (attack ratio, decay ratio, scales) cannot be declared
  today without adding one.
- **No common per-item RX/TX behavior.** Thetis gives nearly every item Fade on RX and
  Fade on TX (for example setup.cs:25548-25549, applied MeterManager.cs:28264-28265).
  NereusSDR's only-when-RX and only-when-TX switches reach MeterItems only
  (src/gui/containers/ContainerContentRegistry.cpp:404-405). None of the 13 widget types
  here has fade, only-when-RX or only-when-TX.
- **Thetis binds a source per container, not per item.** Setup has "RX1 data" and
  "RX2 data" radios per container (setup.cs:24716-24726, `MeterManager.GetContainerRX`)
  plus "hide when RX not used" (setup.cs:24714). NereusSDR resolves a per-entry context
  (src/gui/containers/ContainerContentHost.cpp:244-269) through
  `ContainerSourceAdapter::slice` (src/gui/containers/ContainerSourceAdapter.cpp:26-36)
  to a slice id, Follow selected RX (`MainWindow::windowRxSlice`,
  src/gui/MainWindow.cpp:2312-2341), or nothing for another session's context.

---

### longpath.frequency and longpath.frequencyCustom: Thetis VFO_DISPLAY / clsVfoDisplay

NereusSDR: `makeFrequency` (src/gui/instruments/LongpathContent.cpp:201-251).
`longpath.frequency` declares only Form (:342-353). `longpath.frequencyCustom` adds font
size, fit, color, centering and two line toggles (:355-400), applied by
`applyFrequencyDisplay` (:253-273). Thetis: `AddVFODisplay` (MeterManager.cs:25751),
class `clsVfoDisplay` (:14201), setup save block (setup.cs:25320-25348), apply
(MeterManager.cs:28036-28115). Thetis stores several colors in reused igs fields; each row
names the field.

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Form: number only, band strip, flat arc | No equivalent. Nearest is "show band text" (`vfo_showbandtext`, setup.cs:25336, MeterManager.cs:28063) | Form choice (LongpathContent.cpp:342-353, applied :204-206) | Nereus-only |
| Which VFOs show: both, A only, B only | Radio buttons stored in **igs.HistoryDuration** (setup.cs:25343-25348; MeterManager.cs:28072-28074; layout :28076-28086) | Custom: "A/B line: this slice" and "A/B line: other slice" toggles (LongpathContent.cpp:387-398, :268-272). Plain Frequency: no control, both lines always shown | partial |
| Background color | igs.Colour (setup.cs:25322 → solid colour MeterManager.cs:28092) | missing | missing |
| Title / VFO-label color | igs.TitleColor (setup.cs:25323 → TypeColour MeterManager.cs:28052) | missing | missing |
| Frequency color | **igs.MarkerColour** (setup.cs:25326 → FrequencyColour MeterManager.cs:28053) | Custom only: "Font color" (LongpathContent.cpp:374-378, :258-264). Plain: missing | partial (Custom) / missing (plain) |
| Small-digit frequency color | `vfo_frequency_small_numbers_colour` (setup.cs:25338 → MeterManager.cs:28067) | missing (one color for all digits) | missing |
| Mode text color | **igs.SubMarkerColour** (setup.cs:25327 → MeterManager.cs:28054) | missing (no mode is shown) | missing |
| Split background / split color | **igs.LowColor / igs.HighColor** (setup.cs:25328-25329 → MeterManager.cs:28055-28056) | missing | missing |
| RX / TX indicator colors | **igs.PeakValueColour / igs.PeakHoldMarkerColor** (setup.cs:25330-25331 → MeterManager.cs:28057-28058) | missing | missing |
| Filter text color | **igs.HistoryColor** (setup.cs:25332 → MeterManager.cs:28059) | missing (no filter shown) | missing |
| Band text color | **igs.SegmentedSolidLowColour** (setup.cs:25333 → MeterManager.cs:28060) | missing | missing |
| Digit highlight color (hovered digit) | **igs.PowerScaleColour** (setup.cs:25334 → MeterManager.cs:28061) | missing | missing |
| Show band text and its color | `vfo_showbandtext`, `vfo_showbandtext_colour` (setup.cs:25336-25337 → MeterManager.cs:28063-28066) | Band strip form shows the band (no color option) | partial |
| Lock / sync indicator colors | `vfo_lock_colour`, `vfo_sync_colour` (setup.cs:25340-25341 → MeterManager.cs:28069-28070) | missing | missing |
| Fade on RX / TX | Applied (MeterManager.cs:28049-28050); no control found in the VFO setup block (setup.cs:25320-25348) | missing | missing |
| Font size / fit to width | Fixed font, Trebuchet MS 18 (MeterManager.cs:14308-14310) | Custom: Font size 12-160 px, Fit font to the width (LongpathContent.cpp:358-372) | Nereus-only |
| Center in the box | none | Custom: toggle (LongpathContent.cpp:380-385) | Nereus-only |
| Mouse-wheel tuning per digit | `MouseWheel` + digit step adjust (MeterManager.cs:14572, :14495-14525) | Per-digit wheel → `frequencyEdited` → `SliceModel::setFrequency` (FrequencyInstrument.h:196-200; LongpathContent.cpp:216-222) | match |
| Typed frequency entry | `KeyDown` (MeterManager.cs:14527) | Entry field parsed by `parseUserFrequency` (FrequencyInstrument.h:166-184) | partial (different mechanism) |
| Band screen / band pick | BAND_SCREEN, BAND (MeterManager.cs:14656, :14691) | missing | missing |
| Mode screen / mode pick | MODE_SCREEN, MODE (MeterManager.cs:14681, :14710) | missing | missing |
| Filter screen / filter pick | FILTER_SCREEN, FILTER (MeterManager.cs:14673, :14728) | missing | missing |
| Split toggle | SPLIT → toggleSplit (MeterManager.cs:14746, :14933-14947) | missing | missing |
| Choose TX VFO | TX → setTX (MeterManager.cs:14754, :14905-14931) | missing | missing |
| VFO lock toggle, and edits refused while locked | LOCK → setLock (MeterManager.cs:14766, :14879-14895); abortForLockedVFO (:14462-14494) | missing as a control. Whether `SliceModel::setFrequency` refuses a locked slice: not checked | missing |
| VFO sync | VFO_SYNC → setVfoSync (MeterManager.cs:14778, :14897-14903) | missing | missing |
| Tune-step screen | TUNE_STEP (MeterManager.cs:14784; sets console.TuneStepIndex :14871-14873) | missing | missing |
| Band-stack popup | PopBandStack (MeterManager.cs:14584-14597) | missing | missing |
| Filter context menu | PopFilterMenu (MeterManager.cs:14599-14611) | missing | missing |
| Which VFO is active | RX/TX colors on the active VFO (above) | `setActiveIsThis` dims the inactive line (LongpathContent.cpp:234) | partial |
| Source | Container RX1/RX2 (setup.cs:24716-24726) | Per-entry Source combo (ContentPropertyEditor.cpp:394-407) | Nereus-only |

Multislice notes:
- Thetis maps the item to VFO A/B and RX1/RX2 by the container's RX, RX2Enabled,
  MultiRxEnabled and Split (MeterManager.cs:14400-14460 for the get/set frequency
  paths). NereusSDR binds the item to one slice through the entry context
  (LongpathContent.cpp:128-183 `SliceFollower`, rebinding on slice add/remove and on
  activeSliceChanged :141-143). That is already slice A-D aware.
- The second line is `otherSlice()`: the first slice in `RadioModel::slices()` that is
  not this one (LongpathContent.cpp:185-192). With three or four slices the second line
  is arbitrary. Thetis's second line is always VFO B or the sub-RX
  (MeterManager.cs:14409-14425). NereusSDR needs a rule or a choice: the split partner,
  the TX slice, or a picked slice.
- Thetis split, TX-VFO and lock buttons assume one TX VFO out of A/B. In NereusSDR they
  would mean "make this slice the transmit slice", as the keying row already does
  (src/gui/applets/TxKeyingSection.cpp:145-152).
- Remote Core: edits go through `SliceModel::setFrequency`, the same path as the VFO
  flag (LongpathContent.cpp:216-222). A foreign `sessionId` context resolves to no slice
  (ContainerSourceAdapter.cpp:29-31), and the instrument then shows 0.

---

### longpath.needle: Thetis ANANMM and CROSS (clsNeedleItem)

NereusSDR: `meterFactory` (LongpathContent.cpp:461-562), properties `meterProperties`
(:313-340). Reading choices are only the readings that have a scale
(`readingsWithScale`, :302-311; ReadingSource.cpp:598-606). Those are Signal Peak, Avg
and Max Bin (:256-258), Noise Floor and RADE SNR (both disabled, :313-315, :367-369),
TX Forward Power (:398), TX SWR (:428) and TX ALC (:451). Peak hold is always on with a
fixed 3 s hold (PeakHold.h:66-125; no setter is reached from content). There is no
smoothing: `onReading` stores the raw value (NeedleInstrument.cpp:230-243).

Thetis has no stand-alone needle MeterType. Needles live inside ANANMM (`AddAnanMM`,
MeterManager.cs:23784) and CROSS (`AddCrossNeedle`, :24140). Both use
grpMeterItemSettings (setup.cs:26852-26855). The save block is setup.cs:25517-25577 and
the apply block is MeterManager.cs:27895-28034.

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Primary reading | Fixed per type. ANANMM: AVG_SIGNAL (23798), VOLTS (23841), AMPS (23869), PWR (23938), SWR (24001), ALC_G (24033), ALC_GROUP (24065). CROSS: PWR (24151), REVERSE_PWR (24221) | "Reading" with 8 choices, 2 of them disabled (LongpathContent.cpp:314-322) | partial: volts, amps, reverse power, ALC gain and ALC group cannot be picked, although MeterPoller has them (src/gui/meters/MeterPoller.h:171-204) |
| Signal type: peak, average, max bin | ucMeterItemSignalType → igs.Average / igs.MaxBin (setup.cs:25559-25575; apply MeterManager.cs:27940-27951; enabled ANANMM and Magic Eye only, setup.cs:26417) | The three signal readings are in the Reading list | match |
| Second needle | Fixed sub-needles (for example CROSS reverse power, MeterManager.cs:24217-24221) | "Second reading (second pointer)", any reading (LongpathContent.cpp:323-329) | partial (free choice in NereusSDR, fixed in Thetis) |
| RX needle in receive, TX needles in transmit | OnlyWhenRX on the signal needle (23797); OnlyWhenTX on amps/PWR/SWR/ALC needles (23868, 23936, 24000, 24032, 24064) | missing. One reading shows in both states; the legacy `direction` key only picks the default (LongpathContent.cpp:414-420), with no UI | missing |
| TX display group cycle: all, PWR/SWR, Comp, ALC, Volts/Amps | Click box plus `%group%` text (MeterManager.cs:23901-23928); groups (:24130-24135) | missing | missing |
| Update interval | igs.UpdateInterval (setup.cs:25526 → MeterManager.cs:27923) | missing (poller cadence) | missing |
| Attack ratio | igs.AttackRatio (setup.cs:25527 → 27924) | missing (no smoothing) | missing |
| Decay ratio | igs.DecayRatio (setup.cs:25528 → 27925) | missing | missing |
| History trail, color and alpha | igs.ShowHistory, igs.HistoryColor (setup.cs:25529-25530 → 27928-27929) | missing | missing |
| History duration / ignore duration | setup.cs:25532-25533 → 27926-27927; SWR needle :27975-27977; CROSS reverse :27982-27984 | missing | missing |
| Primary needle color | igs.MarkerColour → ni.Colour (setup.cs:25521 → 27932) | missing (palette) | missing |
| Sub-needle color | igs.SubMarkerColour (setup.cs:25522 → 27987) | missing | missing |
| Shadow | igs.Shadow (setup.cs:25531 → 27937, 27971; enabled ANANMM/CROSS setup.cs:26382) | missing | missing |
| Peak hold and marker color | igs.PeakHold, igs.PeakHoldMarkerColor (setup.cs:25545-25546 → 27942-27943, 27956-27957; disabled for CROSS setup.cs:26380-26381) | Always on, 3 s hold, no toggle, no color (PeakHold.h:115-121) | partial |
| Fade on RX / TX | setup.cs:25548-25549 → 27930-27931, 27968-27969, images 27995-27996 | missing | missing |
| Dark mode | igs.DarkMode (setup.cs:25577 → image 27997, scale 28015; enabled ANANMM/CROSS setup.cs:26384) | Follows the workspace palette, with no per-item switch (InstrumentPainter, LONGPATH-PROVENANCE.md:104) | partial |
| Power limit (scale top) | igs.MaxPower (setup.cs:25556 → 27960, 27985, 28014; enabled ANANMM/CROSS/PWR/REV_PWR setup.cs:26389) | Global `MainPowerMeter` scale for TX Power only (LongpathContent.cpp:514-526) | partial |
| Power scale color | igs.PowerScaleColour (setup.cs:25557 → 28016) | missing | missing |
| MMIO variable | CROSS only; ANANMM is a Thetis TODO (MeterManager.cs:27906-27917) | missing: declared types skip MMIO (ContentPropertyEditor.cpp:409-410) | missing |
| VHF S9 shift (S9 = -93 dBm at or above 30 MHz) | `IsAboveS9Frequency(_rx)` (MeterManager.cs:23833, :5426-5429, :3342-3343) | missing; recorded as a known gap (ReadingSource.h:203-208) | missing |
| Offline state when a reading is unavailable | none | Needle goes offline when the poller or Core has no source (LongpathContent.cpp:497-509, :540-551) | Nereus-only |
| Source | Container RX | Per-entry Source (ContentPropertyEditor.cpp:394-407) | Nereus-only |

Multislice notes:
- Thetis reads the container's RX (`_rx`, MeterManager.cs:738-742; setup.cs:24716-24726).
  NereusSDR reads `MeterPoller` readings keyed by the entry's effective context
  (LongpathContent.cpp:532-561). RX readings follow slice A-D or Follow selected RX.
  TX readings are radio-wide.
- A source change clears the value and peak (`rxSourceChanged`, LongpathContent.cpp:552-558).
- The VHF S9 shift is keyed by RX1/RX2 frequency in Thetis
  (`_rx1VHForAbove`/`_rx2VHForAbove`, MeterManager.cs:3342-3343). In NereusSDR it would
  need to key on the bound slice's frequency.
- Remote Core: the poller's unavailable reason blanks the needle
  (`bindingUnavailableReason`, LongpathContent.cpp:540-551).

---

### longpath.bar: Thetis bar MeterTypes (clsBarItem: SIGNAL_STRENGTH, PWR, SWR, ALC, ...)

NereusSDR: same factory and schema as the needle, plus "Compact (no footer)"
(LongpathContent.cpp:330-338, applied :478). `BarInstrument::setSegmented` and
`setTube` exist but nothing reaches them (BarInstrument.h:162, :179). Thetis: one
MeterType per reading (MeterManager.cs:178-205), for example `AddSMeterBarSignal`
(:22822), `AddPWRBar` (:25177), `AddSWRBar` (:25313) and `AddALCBar` (:24649). Save
block setup.cs:25517-25557; apply MeterManager.cs:28221-28331.

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Reading | A choice of MeterType (MeterManager.cs:178-205): signal peak/avg/max bin, ADC, AGC, AGC gain, PB SNR, MIC, EQ, Leveler, Leveler gain, ALC, ALC gain, ALC group, CFC, CFC gain, COMP, PWR, REV PWR, SWR | 8 scaled readings (LongpathContent.cpp:302-322) | partial: no ADC, AGC, PB SNR, mic, EQ, leveler, CFC, comp, reverse power or gains |
| Second bar | Non-primary H_BAR, for example average under peak (MeterManager.cs:28281-28290) | "Second reading (second trough)" (LongpathContent.cpp:323-329) | partial |
| Show sub-marker and its color | igs.ShowSubMarker, igs.SubMarkerColour (setup.cs:25522, 25524 → 28288-28289) | missing | missing |
| Compact (no footer) | none | Toggle (LongpathContent.cpp:330-338) | Nereus-only |
| Scale low / high colors | igs.LowColor, igs.HighColor (setup.cs:25519-25520 → 28310-28315) | missing | missing |
| Indicator (marker) color and show marker | igs.MarkerColour, igs.ShowMarker (setup.cs:25521, 25523 → 28271-28272) | missing | missing |
| Background color | igs.Colour (setup.cs:25525 → 28301) | missing | missing |
| Update interval | setup.cs:25526 → 28261 | missing | missing |
| Attack ratio | setup.cs:25527 → 28262 | missing | missing |
| Decay ratio | setup.cs:25528 → 28263 | missing | missing |
| History trail and color | setup.cs:25529-25530 → 28268-28269 | missing | missing |
| History duration / ignore | setup.cs:25532-25533 → 28266-28267 | missing | missing |
| Bar style: line, segments, solid | igs.BarStyle (setup.cs:25534-25540 → 28270) | missing (setSegmented/setTube exist but are not exposed, BarInstrument.h:162, :179) | missing |
| Segmented / solid low and high colors | setup.cs:25542-25543 → 28275-28276 | missing | missing |
| Peak hold and its color | setup.cs:25545-25546 → 28273-28274 | Always on, 3 s (PeakHold.h) | partial |
| Show peak value and its color | igs.PeakValue, igs.PeakValueColour (setup.cs:25552-25553 → 28277-28278) | missing | missing |
| Title and its color | igs.ShowType, igs.TitleColor (setup.cs:25550-25551 → 28312-28313) | The footer shows the reading name when not compact (InstrumentFooter) | partial |
| Fade on RX / TX | setup.cs:25548-25549 → 28264-28265 | missing | missing |
| Power limit | igs.MaxPower for PWR/REV_PWR (setup.cs:25556 → 28292, 28316) | Global MainPowerMeter scale (LongpathContent.cpp:514-526) | partial |
| MMIO variable | MeterManager.cs:28237-28246 | missing | missing |
| VHF S9 shift | MeterManager.cs:22875 | missing (ReadingSource.h:203-208) | missing |
| Custom min / max / high / units / title | CUSTOM_METER_BAR only (setup.cs:25511-25515 → 28253-28257) | n/a: a separate type; NereusSDR has `meter.customBar` | n/a |
| Source | Container RX | Per-entry Source | Nereus-only |

Multislice notes: as for longpath.needle. The bar has the same context, readings,
power scale and remote-Core handling (LongpathContent.cpp:476-491, :514-561).

---

### longpath.filter: Thetis FILTER_DISPLAY (clsFilterItem)

NereusSDR: factory at LongpathContent.cpp:570-680. Its only property is "Label"
(:402-410). The pane is the Longpath `BandwidthFilterPane` (src/gui/widgets/
BandwidthFilterPane.h). It supports dragging the edges and the body
(`filterChanged`, `filterCentreChanged` → `SliceModel::setFilter`,
LongpathContent.cpp:598-605). Its trace comes from the slice's pan every 50 ms
(:613-677). The accent is the slice color (:587). The span is fixed at the 10 kHz
default, because `setSpan` is never called (BandwidthFilterPane.h:208).

Thetis: `AddFilterDisplay` (MeterManager.cs:25722), class `clsFilterItem` (:18172),
setup save (setup.cs:25446-25504), apply (MeterManager.cs:27651-27731).

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Label | none | Text, default "RX <letter>" (LongpathContent.cpp:402-410, :582) | Nereus-only |
| Background color | igs.Colour (setup.cs:25448 → 27665) | missing | missing |
| Fade on RX / TX | setup.cs:25449-25450 → 27663-27664 | missing | missing |
| Height ratio | `filterdisplay_vertical_ratio` (setup.cs:25452 → 27666) | Container geometry only | partial |
| Show filter limits | `filterdisplay_show_filter_limits` (setup.cs:25454 → 27668) | missing | missing |
| Fixed RX zoom and level | setup.cs:25455, 25457 → 27669, 27671 | Fixed 10 kHz span, no control | partial |
| Fixed TX zoom and level | setup.cs:25456, 25458 → 27670, 27672 | missing | missing |
| Per-mode scales: sidebands, CW, others | setup.cs:25460-25462 → 27674-27676 | missing | missing |
| Display mode: panadapter, waterfall, panafall, none | `filterdisplay_others_displaymode` (setup.cs:25464-25479 → 27678) | Peak trace only (panadapter-like), `traceFromRendered` (LongpathContent.cpp:422-445) | partial |
| Font scale | setup.cs:25481 → 27680 | missing | missing |
| Fill spectrum, data line and fill colors | setup.cs:25483-25485 → 27682-27684 | missing | missing |
| Waterfall palette, low color, frame update | setup.cs:25486-25487, 25498 → 27685-27686, 27697 | missing | missing |
| Text and number-highlight colors | setup.cs:25488-25489 → 27687-27688 | missing | missing |
| Edge colors (RX, TX) and edge highlight | setup.cs:25490-25492 → 27689-27691 | Slice accent color only (LongpathContent.cpp:587) | partial |
| Meter background color | setup.cs:25493 → 27692 | missing | missing |
| Notch and notch-highlight colors | setup.cs:25494-25495 → 27693-27694 | missing | missing |
| Extents color | setup.cs:25496 → 27695 | missing | missing |
| Sideband mode | setup.cs:25497 → 27696 | missing | missing |
| Grey outside the passband | `filterdisplay_use_grey` (setup.cs:25499 → 27698) | missing as an option | missing |
| Snap lines and their color | `filterdisplay_snap_lines` (apply 27699, toggled on the item MeterManager.cs:19004); color setup.cs:25500 → 27701 | missing | missing |
| Auto zoom | `filterdisplay_autozoom` (apply 27700; toggled on the item :19008) | missing | missing |
| Setting-on and button-highlight colors | setup.cs:25501-25502 → 27702-27703 | missing | missing |
| Filter characteristic and its low level | setup.cs:25503-25504 → 27704-27705 | missing | missing |
| Edge and shift adjust | Drag, plus left/right click steps on low, high and shift (MeterManager.cs:18968-18987); constrained by ConstrainFilter (:19399) | Drag edges and body (LongpathContent.cpp:598-605); constraint in SliceModel (BandwidthFilterPane.h:44 comment) | partial (no click steps) |
| Right-click reset to the mode's default filter | MeterManager.cs:18871-18915 | missing | missing |
| Notches: add (Ctrl+right-click), move, popup | MeterManager.cs:18930, :18949-18958, :19374 | missing | missing |
| TNF on/off and TNF+ | MeterManager.cs:18990-19001 | missing | missing |
| TX filter shown and edited while transmitting | MeterManager.cs:19201-19205, :19295-19299; TX edge color 27690 | missing | missing |
| Source and pan | Container RX | Per-entry Source; the trace comes from `spectrumForSlice(slice)` (LongpathContent.cpp:623) | Nereus-only |

Multislice notes:
- Thetis hard-codes RX1/RX2. `_showVfoA = _owningmeter.RX == 1` (MeterManager.cs:18368).
  The spectrum id is `RX == 1 ? 0 : 1` (:18445, :18545, :18614, :19618). Filter writes
  branch RX1 vs `UpdateRX2Filters` (:19221-19250, :19315-19344, :19400-19408). RX2
  waterfall thresholds come from RX2 globals (:19854-19870).
- NereusSDR already binds to one slice and reads the pan that shows it, so any
  panadapter works. If the slice's pan is not open in this window, the trace is empty
  (LongpathContent.cpp:623-624). On a remote window it depends on the local pan's
  rendered pixels.
- Thetis edits the TX filter while MOX. NereusSDR would need the TX slice's filter, or the
  TransmitModel TX filter (the tx.filter row), while transmitting.

---

### sMeter.dial and applet:s_meter: closest Thetis items ANANMM (and CROSS for TX power)

NereusSDR: `SMeterWidget` (ported from AetherSDR, src/gui/SMeterWidget.cpp header). The
copies (`sMeter.dial`) declare TX mode, RX mode, Peak hold, Peak decay and Meter face
(src/gui/instruments/SMeterDialContent.cpp:129-172, applied :88-111). The right-click
menu writes back to the entry (:296-315; menu SMeterWidget.cpp:1499-1580). The
`applet:s_meter` singleton has no declared properties. Its settings are the global
AppSettings keys SMeter_TxSelect, SMeter_RxSelect, PeakHoldEnabled and PeakDecayRate,
set from its right-click menu (SMeterWidget.cpp header, lines 37-45). Its face comes
from Setup (src/gui/setup/AppearanceSetupPages.cpp:383-422). Its Source is honored
(`setSMeterContext`, src/gui/MainWindow.cpp:8921). The dial is not a Thetis port, and
Thetis's DIAL_DISPLAY is a tuning dial, not a meter. The direct ANANMM and CROSS ports
are `meter.ananMulti` and `meter.crossNeedle`, which belong to another audit group.

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| TX reading: Power, SWR, Level (mic), Compression | ANANMM shows PWR, SWR, ALC_G, ALC_GROUP, Volts and Amps needles together, filtered by display group (MeterManager.cs:23931-24089, :24130-24135) | TX mode choice (SMeterDialContent.cpp:136-141; SMeterWidget::setTxMode :403) | partial: one reading at a time; no ALC, ALC group, volts, amps or "all" |
| RX reading: Signal, Sig Avg, Signal Peak, Max Bin | Signal type peak/avg/max bin (setup.cs:25559-25575 → MeterManager.cs:27940-27951) | RX mode choice (SMeterDialContent.cpp:142-147; `rxBindingForMode` :319-324). "Signal Peak" shows the held peak (SMeterWidget.cpp:610) | match (Signal Peak display is Nereus-only) |
| Peak hold on/off | igs.PeakHold (setup.cs:25545 → 27942, 27956) | Toggle (SMeterDialContent.cpp:148-152; SMeterWidget.cpp:1408) | match |
| Peak marker color | igs.PeakHoldMarkerColor (setup.cs:25546 → 27943) | missing | missing |
| Peak decay: fast 20, medium 10, slow 5 dB/s | No decay-rate control in grpMeterItemSettings (save block setup.cs:25517-25577) | Choice (SMeterDialContent.cpp:153-158; SMeterWidget.cpp:1433-1460) | Nereus-only |
| Meter face (7 faces or "Same as Setup") | none | Choice (SMeterDialContent.cpp:159-171) | Nereus-only |
| Dark mode | igs.DarkMode (setup.cs:25577 → 27997, 28015) | Only through the face choice (Blackface, Carbon) | partial |
| Update interval | setup.cs:25526 → 27923 | Fixed 33 ms animation (SMeterWidget.h:511) | missing |
| Attack ratio | setup.cs:25527 → 27924 | Fixed 0.045 s (SMeterWidget.h:512) | missing |
| Decay ratio | setup.cs:25528 → 27925 | Fixed 0.180 s (SMeterWidget.h:513) | missing |
| History trail and color | setup.cs:25529-25530 → 27928-27929 | missing | missing |
| History duration / ignore | setup.cs:25532-25533 → 27926-27927 | missing (peak resets every 10 s, SMeterWidget.cpp:200-202) | missing |
| Needle and sub-needle colors | setup.cs:25521-25522 → 27932, 27987 | Fixed per face | missing |
| Shadow | setup.cs:25531 → 27937 | missing | missing |
| Fade on RX / TX | setup.cs:25548-25549 → 27930-27931 | missing | missing |
| Power limit | igs.MaxPower (setup.cs:25556 → 27960, 28014) | Global MainPowerMeter legal marker and scale top (SMeterDialContent.cpp:175-190; SMeterWidget.cpp:1398-1401) | partial |
| Power scale color | setup.cs:25557 → 28016 | missing | missing |
| MMIO variable | ANANMM: Thetis TODO (MeterManager.cs:27906) | none | n/a |
| VHF S9 shift | MeterManager.cs:23833 | missing | missing |
| Volts / amps needles | ANANMM ni2, ni3 (MeterManager.cs:23837-23898) | missing. HwVolts and HwAmps exist (MeterPoller.h:203-204) but the dial registers neither (SMeterDialContent.cpp:326-330) | missing |
| Source | Container RX | `sMeter.dial`: per-entry Source. `applet:s_meter`: Source honored (MainWindow.cpp:8921) | Nereus-only |
| `applet:s_meter` settings in the container editor | n/a | No declared properties; the editor shows a note only (ContainerContentRegistry.cpp:491); settings are global, by right-click and Setup | partial |

Multislice notes:
- RX readings follow the entry context (SMeterDialContent.cpp:261-292). A source change
  clears the level and peak (:283-287). TX readings are radio-wide.
- The TX power scale is the radio-wide `MainPowerMeter` (SMeterDialContent.cpp:175-190).
  Thetis's per-item MaxPower would become a per-copy override.
- The VHF S9 shift would key on the bound slice's frequency (Thetis keys on RX1/RX2,
  MeterManager.cs:3342-3343).
- Remote Core: TX modes with no reading are disabled with the poller's reason
  (`setTxModeUnavailable`, SMeterDialContent.cpp:246-252, :270-280). Without a poller,
  readings come from MainPowerMeter directly (:183-189).

---

### tx.monitor, tx.power, tx.filter, tx.chain, tx.vox, tx.processing, tx.keying: Thetis OTHER_BUTTONS (clsOtherButtons) and console front-panel controls

NereusSDR: each type calls `TxApplet::create*Section`, which builds a copy of a TX
applet row wired to the same model (src/gui/applets/TxSectionContent.cpp:64-125;
src/gui/applets/TxApplet.cpp:2469-3070). **No type declares properties.** With no
MeterItem, no `applet:` prefix and no schema, ContentPropertyEditor shows "Unsupported
configuration. Original data is retained; this object can be removed explicitly."
(ContentPropertyEditor.cpp:306-309). It also disables Advanced bindings with
"Unsupported content retains its stored bindings." (:392). The content works, but the
settings pane wrongly says it is unsupported.

Thetis puts TX controls in containers through OTHER_BUTTONS (`AddOtherButtons`
MeterManager.cs:25686; `clsOtherButtons` :8251; ids
ucOtherButtonsOptionsGrid.cs:54-260; actions console.cs:51340-51600). Thetis has no
container slider, combo or spin box for TX, so several NereusSDR rows have only a
front-panel or Setup equivalent.

### Common to all seven tx.* items: the Thetis button-box settings (grpButtonBox)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Columns | `buttonbox_columns` (setup.cs:25150) | Fixed row layout | missing |
| Border, margin, corner radius, height ratio | setup.cs:25151-25154 | missing | missing |
| Indicator: use, type, border, on and off colors | setup.cs:25156-25159, 25170 | Fixed green-checked style | missing |
| Fill, hover, border, click and font colors; use off color | setup.cs:25161-25168 | missing | missing |
| Font family and style, scale, x/y shift, fixed text size | setup.cs:25172-25182 | missing | missing |
| Use icons | `buttonbox_use_icons` (setup.cs:25177; OTHER_BUTTONS only, setup.cs:26778) | missing | missing |
| Fade on RX / TX | setup.cs:25185-25186 | missing | missing |
| Which buttons show | Per-button visibility bitfields (setup.cs:25110) | Fixed set per row | missing |
| Reorder buttons (Alt+drag) | `buttonbox_button_map` (setup.cs:25105; tooltip :26812) | missing | missing |
| Macro buttons | `buttonbox_other_buttons_macro_settings_N` (setup.cs:25115) | missing | missing |
| Settings pane | grpButtonBox (setup.cs:26768-26846) | Shows "Unsupported configuration" (ContentPropertyEditor.cpp:309) | missing (a NereusSDR bug) |

### tx.monitor (TxSectionContent.cpp:64-71; TxMonitorSection.cpp:127-215)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| MON toggle | OtherButtonId.MON (ucOtherButtonsOptionsGrid.cs:58; console.cs:51355); chkMON (console.Designer.cs:1878) | MON button (TxMonitorSection.cpp:130) | match |
| Speakers / Phones output pair | none | TxMonitorSection.cpp:158-180 | Nereus-only |
| Why MON is silent notice | none | TxMonitorSection.cpp:183, :248 | Nereus-only |
| Monitor volume slider | TXAF (console.cs:18496); no Other Button for it (enum ucOtherButtonsOptionsGrid.cs:54-260) | Mon Vol slider (TxMonitorSection.cpp:195-215) | Nereus-only (as a container item) |

### tx.power (TxSectionContent.cpp:73-80; TxPowerSection.cpp:123-205)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| Forward-power gauge | PWR bar MeterType (`AddPWRBar` MeterManager.cs:25177) with the full bar options and MaxPower | Slim bar with per-model scale (TxPowerSection.cpp:130-134) | partial: no bar options |
| SWR gauge | SWR bar MeterType (`AddSWRBar` MeterManager.cs:25313) | Slim bar (TxPowerSection.cpp:137-140) | partial |
| RF power | ptbPWR (console.Designer.cs:3690); DRIVE_P5, DRIVE_M5, DRIVE_0 buttons (ucOtherButtonsOptionsGrid.cs:181-185; console.cs:51574-51575, :51578) | RF Power slider (TxPowerSection.cpp:145-170) | partial: no ±5 or zero buttons |
| Tune power | ptbTune (console.Designer.cs:3946); TUNE_P5, TUNE_M5, TUN_0 (ucOtherButtonsOptionsGrid.cs:183-186; console.cs:51576-51577, :51579) | Tune Pwr slider (TxPowerSection.cpp:183-205) | partial |

### tx.filter (TxSectionContent.cpp:82-89; TxFilterSection.cpp:120-175)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| TX filter low / high | Setup udTXFilterLow/High (setup.cs:3042), console TXFilterHigh/Low (console.cs:19733, :19749). In containers it is edited only by dragging the filter display while MOX (MeterManager.cs:19201-19205, :19295-19299) | Lo / Hi spin boxes (TxFilterSection.cpp:139-170) | partial |
| Filter status label | none | TxFilterSection.cpp:172 | Nereus-only |
| Show the TX filter on the display | TX_FILTER button → ShowTXFilter (ucOtherButtonsOptionsGrid.cs:123; console.cs:51491, :52687-52688) | missing | missing |

### tx.chain (TxSectionContent.cpp:91-98; TxChainSection.cpp:135-200)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| LEV (leveler) | LEVELER (ucOtherButtonsOptionsGrid.cs:126; console.cs:51494) | LEV button (TxChainSection.cpp:141) | match |
| EQ on/off, and opening the EQ | TX_EQ (:122; console.cs:51490); FORM_EQ (:191; console.cs:51588-51591); chkTXEQ MouseDown (console.Designer.cs:2055) | EQ button; right-click opens TxEqDialog (TxApplet.cpp:2843-2860) | match |
| CFC | CFC (:124; console.cs:51492) | CFC button (TxChainSection.cpp:167); right-click opens TxCfcDialog (TxApplet.cpp:1896-1960) | match |
| CFC EQ | CFC_EQ (:125; console.cs:51493) | missing | missing |
| TX profile | comboTXProfile (console.Designer.cs:2104); no Other Button | Profile combo (TxChainSection.cpp:195) | Nereus-only (as a container item) |

### tx.vox (TxSectionContent.cpp:100-107; TxVoxSection.cpp:118-183)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| VOX on/off | VOX (ucOtherButtonsOptionsGrid.cs:119; console.cs:51487); chkVOX (console.Designer.cs:2082) | VOX button (TxVoxSection.cpp:121) | match |
| VOX threshold | ptbVOX (console.Designer.cs:6020); VOX_P1, VOX_M1 (:177-178; console.cs:51570-51571) | Threshold slider (TxVoxSection.cpp:143) | partial: no ±1 buttons |
| VOX hold | Setup udDEXPHold / VOXHangTime (setup.cs:3648, :4898); no Other Button | Hold slider (TxVoxSection.cpp:168) | Nereus-only (as a container item) |
| VOX level meter | No VOX or DEXP reading in MeterType (MeterManager.cs:178-230) | DexpPeakMeter (TxVoxSection.cpp:154) | Nereus-only |
| DEXP (noise gate) on/off | DEXP (ucOtherButtonsOptionsGrid.cs:120; console.cs:51488); chkNoiseGate (console.Designer.cs:2092) | missing from this row | missing |

### tx.processing (TxSectionContent.cpp:109-116; TxProcessingSection.cpp:120-200)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| 2-Tone | TWOTON (ucOtherButtonsOptionsGrid.cs:61; console.cs:51358); chk2TONE (console.Designer.cs:1930) | TxProcessingSection.cpp:129 | match |
| PS-A | PS_A (:63; console.cs:51360); chkFWCATUBypass, text "PS-A" (console.Designer.cs:1941; console.resx:2253-2254) | TxProcessingSection.cpp:145 | match |
| DUP | DUP → chkRX2SR (:62; console.cs:51359; console.Designer.cs:1899) | TxProcessingSection.cpp:162 | match |
| SWR protection LED | SWRProtection (console.cs:19051); no dedicated item. Whether a Thetis LED item (`AddLed` MeterManager.cs:23756) can read SWR protection: not checked | TxProcessingSection.cpp:198 | Nereus-only |

### tx.keying (TxSectionContent.cpp:118-125; TxKeyingSection.cpp:117-170)

| Option / control | Thetis (what it does, file:line) | NereusSDR (file:line or "missing") | Status |
|---|---|---|---|
| TUNE | TUN (ucOtherButtonsOptionsGrid.cs:59; console.cs:51356); chkTUN (console.Designer.cs:1919) | TxKeyingSection.cpp:124 | match |
| MOX | MOX (:60; console.cs:51357); chkMOX (console.Designer.cs:1909) | TxKeyingSection.cpp:134 | match |
| Transmit slice | VFO A/B TX in the VFO display (MeterManager.cs:14754-14762, :14905-14931) | One letter per controlled slice (TxKeyingSection.cpp:145-152, :187-212) | partial (Thetis has A/B only; NereusSDR's row is broader) |
| Who holds transmit / Take transmit | none | TxKeyingSection.cpp:155-180 | Nereus-only |

Multislice notes (all tx.* items):
- Thetis TX is per radio. Other-button general settings are read per container RX
  (`GetGeneralSetting(m.RX, ...)`, MeterManager.cs:5078-5086). VOX, CFC, leveler and TX
  EQ are radio-wide states anyway (console.cs:52060-52080).
- NereusSDR rows read the radio-wide TransmitModel through the one TxApplet
  (TxApplet.cpp:2819-2860). The Source combo is disabled for these types
  (ContentPropertyEditor.cpp:392). Only tx.keying is slice-aware: it lists slices A-D this
  window controls.
- Remote Core: tx.keying shows the transmit holder and Take transmit
  (TxKeyingSection.cpp:155-180). Other rows follow the TxApplet permission gates
  (TxApplet.cpp:2816-2817 comment).

---

### Summary

Counts (missing / partial):

| Item | missing | partial |
|---|---|---|
| longpath.frequency | 22 | 4 |
| longpath.frequencyCustom | 21 | 5 |
| longpath.needle | 14 | 5 |
| longpath.bar | 15 | 5 |
| longpath.filter | 22 | 5 |
| sMeter.dial / applet:s_meter | 12 | 4 |
| tx.monitor | 11 (common) + 0 | 0 |
| tx.power | 11 + 0 | 4 |
| tx.filter | 11 + 1 | 1 |
| tx.chain | 11 + 1 | 0 |
| tx.vox | 11 + 1 | 1 |
| tx.processing | 11 + 0 | 0 |
| tx.keying | 11 + 0 | 1 |

Totals: 106 item-specific missing (183 counting the 11 common button-box rows once per
tx item), 35 partial.

The 5 biggest gaps:
1. **No common item options on any of these 13 types.** None has fade on RX/TX,
   update rate, attack, decay, history, colors or shadow, all of which Thetis gives every
   meter (setup.cs:25517-25557). The ContentProperty schema has no floating-point kind,
   so attack and decay ratios cannot be declared yet (ContainerContentRegistry.h:46).
2. **Longpath needle and bar are not Thetis meters in behavior.** They do not swap RX and
   TX readings or offer ANANMM display groups (MeterManager.cs:23797-24135). Their
   Reading list has 8 entries, missing reverse power, mic, comp, ALC gain and group,
   leveler, CFC, volts, amps, ADC and AGC. MeterPoller already produces these.
3. **The filter passband has only a Label.** It lacks nearly all of Thetis's
   FILTER_DISPLAY: notches and TNF, editing the TX filter while MOX, RX/TX zoom,
   waterfall, reset to the mode default, and 20+ colors (MeterManager.cs:18858-19410,
   :27651-27705).
4. **The Frequency instrument has none of Thetis VFO display's controls.** Band, mode and
   filter screens, split, TX VFO, lock, sync, tune step and band stack are all absent
   (MeterManager.cs:14584-14947). Its "other slice" line is arbitrary with 3 or 4 slices
   (LongpathContent.cpp:185-192).
5. **The TX section items have no settings, and the editor calls them "Unsupported
   configuration"** (ContentPropertyEditor.cpp:306-309). They also lack Thetis button-box
   styling, button visibility and order (setup.cs:25105-25186). Separately, the VHF S9
   shift (-93 dBm at or above 30 MHz) is missing from the dial, needle and bar
   (MeterManager.cs:23833, :22875; ReadingSource.h:203-208).

Open questions for a human:
1. Scope: these types exist only in the workspace preview (MainWindow.cpp:11686). Should
   parity work target them now, or wait until they leave the preview?
2. Should the Longpath needle and bar gain Thetis's common meter options (fade,
   attack/decay, history, colors)? Or are they meant to keep Longpath's fixed look, with
   `meter.ananMulti`/`meter.crossNeedle` and the Thetis bar types as the parity path?
3. Should the needle and bar Reading list include every MeterPoller reading (that needs
   scales from Thetis for each), or only the 8 scaled ones?
4. The Frequency instrument's second line: with 3 or 4 slices, which slice should it show?
   The TX slice, the split partner, or a user-picked slice?
5. Should the dial's TX mode gain ANANMM's ALC, ALC group and Volts/Amps groups, and a
   per-copy power limit in place of the global MainPowerMeter scale?
6. TX section items: should they adopt Thetis button-box styling and visibility options,
   or keep the applet-row look? Either way, the "Unsupported configuration" note looks
   like a bug to fix (for example, treat a widget type with an empty schema as having no
   options).
7. VHF S9 shift: adopt Thetis's per-receiver 30 MHz rule, keyed per slice? Longpath
   recorded it as a known, deliberate gap (ReadingSource.h:203-208).
8. The legacy `direction` config key on Longpath needles (LongpathContent.cpp:418) has no
   UI. Should it be kept, migrated or dropped?
