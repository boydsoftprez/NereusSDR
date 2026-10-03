# Setup description versions 1–24

The Core sends the desktop's built Setup pages as JSON strings on the read-only
`setup` mirror object (`SetupDescription`). It has one string property per
category and a `revision` that advances when any published description
changes. A category string is empty until that category is published. The Core
sends its schema, object and later deltas only to a session whose client hello
declares `setupDescription: 1`; that session receives
`setupDescriptionVersion: 1` in capabilities. Older clients see neither.

Each category file is embedded in NereusCore at `:/setup/<id>.json`:

```json
{
  "version": 1,
  "category": {"id": "test", "title": "Test", "where": "station"},
  "pages": [{
    "id": "test.twoToneImd", "title": "Two-Tone IMD", "where": "station",
    "sections": [{"title": "Mode", "controls": [{
      "id": "test.twoToneImd.invert", "label": "Invert for LS Modes",
      "tooltip": "Swap F1 and F2 for lower side band modes",
      "kind": "toggle",
      "binding": {"property": {"object": "transmit", "name": "twoToneInvert"}},
      "applies": "live", "gate": {"transmit": true}
    }]}]
  }]
}
```

The `pages`, `sections` and `controls` arrays preserve the desktop order.
Category and page `where` values are `station`, `phone` or `mixed`; the owner
of a control is determined by its binding. A page or control the desktop has
not built is absent. The desktop-only exceptions are Remote Access, Skins and
Collapsible Display. A desktop control has the dynamic QObject property
`nereusSetupId` equal to the description's control ID; no ID is reused.
An optional `coverage` field on a category or page is `partial` or a more
specific pending scope. The renderer may show described controls but must not
infer that omitted desktop controls are present. A category with no ready
pages is sent as an empty string, not a page with empty controls. Diagnostics
publishes only its Settings Validation panel to version-3 peers; versions 1
and 2 still receive an empty Diagnostics string. Its local file and log
actions remain undescribed.

Control `kind` is one of `toggle`, `integer`, `decimal`, `slider`, `choice`,
`text`, `colour`, `button`, `readout` or `table`; version 3 also has the one
closed `settingsHygiene` panel described below. Numeric controls carry
`min`, `max`, `step` and, where shown, `unit`. Choices have an ordered `choices`
array. A table has `rows`, `columns` and a cell kind. `tooltip` is the
desktop's exact text, including an empty string when the desktop has none.

Every control has exactly one `binding`: `setting` (an AppSettings key routed
by `classifySettingsKey`), `property` (a mirrored object and property),
`command` (a station verb), or `phone` (a closed phone-owned dispatch identity). The
version-3 Settings Validation panel and version-6 antenna tables have their
own closed `settingsHygiene` and `antennaRows` bindings; neither extends
those generic binding rules. A
station binding has `applies: "live"`; the only other value,
`subscription`, is for display keys a client puts into its endpoint
subscription. A `toggle` backed by a station `setting` requires exactly
`"valueEncoding":{"true":"True","false":"False"}`. The five
setting-backed toggles in `resources/setup/general.json` are the canonical
fixture. The renderer may read a native boolean or a case-insensitive exact
match of either mapped string from its current live settings value. A
missing, stale or malformed value disables the control with a plain reason.
An edit sends the exact mapped string to the settings proxy, never a JSON
boolean or a value from another epoch. Property-backed toggles carry no
`valueEncoding` and retain their mirrored boolean wire kind. Optional gates
are capability name/minimum version, transmit
permission, and a BoardCapabilities flag. A false board flag removes its
control; a permission gate disables its control with the Core's reason.
`availability: {"enabled": false, "reason": "…"}` keeps a built but currently
unavailable desktop control visible with the same plain reason. General's
Extended control uses this pending its migration policy; no edit is sent while
unavailable. Region requires transmitSettingsVersion 9 and the offAir gate.
General > Options' RX2 Attenuation (`general.options.rx2StepAtt`, integer,
`stepAtt.rx2AttenuationDb`, the board attenuator's range) is the attenuator of
the receive input slice A is not on, for the slices on that input (R-R3-46,
R-R3-11). It gates on `adcAttenuatorVersion:1` and on the board flag
`attenuator.secondAdc` (a step attenuator and a second receive ADC); the Core
removes it on a one-ADC radio. RX2's own step attenuator enable
(`general.options.rx2StepAttEnable`, toggle, `stepAtt.rx2StepAttEnabled`) and
the RX2 Auto Attenuate section (`general.options.rx2AutoAttEnable`,
`stepAtt.rx2AutoAttEnabled`; `general.options.rx2AutoAttUndo`,
`stepAtt.rx2AutoAttUndo`) carry the same gate; the section goes with its
controls on a one-ADC radio. As for RX1, the hold time is not described.
Phone note: a phone that has not declared
`adcAttenuators` 1 receives no `adcAttenuatorVersion`, so it shows the control
disabled; one that has shows and sets it, and shows each slice the value
`stepAtt.rx2SliceMask` names for it.

Appearance > Colors & Theme currently publishes a partial phone-owned page of
ten built spectrum swatches. Each is `kind:"colour"`, `applies:"live"`, and has
a literal PascalCase `binding.phone`; the phone owns per-pan persistence and
never sends these values to the Core settings proxy. Each `default` and each
edited value at this boundary is an eight-digit `#RRGGBBAA` string, including
the final alpha byte. This is ColorSwatchButton's phone-facing format; the
desktop's own AppSettings uses Qt `HexArgb` (`#AARRGGBB`) and is not copied to
the phone. Core accepts only the ten named IDs/phone keys and exact default
colors. The hidden Waterfall Low Color row remains undescribed (it is an
unbuilt feature on the desktop); version 12 describes the Reset Colors action
(below). Appearance's source category is V7; its RGBA defaults are sent
only to V4+ peers, while older projections retain all ten
color controls without `default`. No station settings permission is needed.

Version 7 also publishes the built Appearance > Meter Styles > S-Meter page,
with exactly three phone-owned live controls. `appearance.meterStyles.face`
is a `choice` bound to `SMeter_FaceStyle` with integer options 0–6 in native
order: Aged Cream, VU Amber, Collins White, Blackface, Carbon, Ice, and
Classic (flat); default 0. The phone maps them to its existing typed face
cases `agedCream`, `vuAmber`, `collinsWhite`, `blackface`, `carbon`, `ice`,
and `classic`. `appearance.meterStyles.peakHold` is a `toggle` bound to
`PeakHoldEnabled`, default `true`. `appearance.meterStyles.peakDecay` is a
`choice` bound to `PeakDecayRate`, with integer options 0 Fast (20 dB/s),
1 Medium (10 dB/s), and 2 Slow (5 dB/s); default 1. The phone maps these to
its existing `fast`, `medium`, and `slow` cases. These binding strings are
dispatch identities for existing phone model actions, not independent
phone storage keys or Core settings writes. Each control has
`requiresDescriptionVersion:7`; options use only the closed numeric
`[{"value":<integer>,"label":<native text>}]` shape. The Core accepts only
these three IDs, bindings, kinds, exact options/defaults, labels and tooltips.
V1–V3 receive the old ten swatches without defaults, V4–V6 receive the old
version-4 Appearance shape with ten defaults, and V7+ receive the new page.
These controls carry no off-air gate: only Setup controls carrying the
offAir gate lock while the Core is keyed (G-61), and receive and display
controls such as these stay live on air. The existing session freshness
rule still applies; the description adds no permission or capability gate.
VFO Small Filter, Skins, and unused controls remain omitted. Phone rendering is owned by the
phone implementation and is not established by this Core publication.

An optional `decimals` field on a `kind:readout` control is an integer from 0
through 6. It formats a finite numeric mirrored value with that many decimal
places; the existing `unit` string follows the number (empty means no unit).
Missing, nonfinite, wrong-type, or stale-session values display unavailable,
never zero or a cached value from another session. Readouts send no edits,
including when the source property happens to be writable. A readout without
`decimals` keeps the renderer's prior behavior. The PA Values partial page
uses five `txState` scalars with `txReadingsVersion:1` and the selected Drive
setpoint from `transmit.power` (Int64) with `transmitSettingsVersion:1`. Drive
is a readout even though its mirrored source is writable; it sends no write.
Three additional readouts use the Core's existing PA scaling of its current
board's raw samples: `forwardRawPowerWatts` (W), `forwardAdcVolts` (V), and
`reflectedAdcVolts` (V), all Float64 from `txState` with
`txReadingsVersion:2`. They have two decimals and no transmit or off-air
gate. Their IDs are `pa.values.forwardRawPower`, `pa.values.forwardVoltage`,
and `pa.values.reflectedVoltage`. All Setup versions can carry these standard
property readouts; the independent capability gate makes them unavailable
with an older Core. There is no client formula, peak/min tracker, or reset
action in these descriptions.
`setup.pa` is appended after `setup.revision` in the fixed mirror schema and
is empty on a board without an integrated PA or on an RX-only SKU. Its nine
readouts remain visible while the radio transmits; they require neither
transmit permission nor an off-air gate. A peer without the negotiated
Setup-description feature receives no `setup` mirror object.

Version 3 adds one closed `kind:settingsHygiene` panel in Diagnostics >
Settings Validation, with ID `diagnostics.settingsValidation.health`. Its
`binding` is exactly `{"settingsHygiene":{"version":1}}` and its gate is
`settingsHygieneVersion:1`. It describes the existing validation issue list
and three desktop actions in order: Re-validate, Repair Invalid Settings
(its own gate `settingsHygieneVersion:2`, paired-device and off-air like
Forget, disabled with the Core's reason below that gate; G-38), and Forget
This Radio. It is not a generic
command or result binding. The actual peer must separately declare
`settingsHygiene:1` and receive that capability; a descriptor alone grants
nothing. A V3 peer still receives the older controls with their existing
semantics. The Core filters every control above the peer's negotiated
description version, drops empty sections and pages, and caps an unknown
future declaration at version 24. PA has a version-20 ceiling (version 14
for V14–V19, version 13 for V13, version 5 for V5–V12) and Hardware a version-23 ceiling
(version 18 for V18–V22, version 17
for V17, version 16 for V16, version 13
for V13–V15, version 6 for V6–V12; see Versions 16, 17, 18 and 23); Display a version-12 ceiling, and
Appearance a version-12 ceiling with its prior version-4 projection for
V4–V6 and version-7 projection for V7–V11. DSP is version 22 to a V22 or
later peer (see Version 22), version 19 to a V19 to V21 peer (see Version 19)
and version 15 to a V15 to V18 peer; Audio is version 24 to a V24 or later
peer (see Version 24) and version 15 to a V15 to V23 peer; Transmit and
Diagnostics are version 15 to a V15 or later peer (see Version 15); CAT & Network is version
21 to a V21 or later peer (see Version 21) and version 15 to a V15 to V20
peer; Transmit is version 13 to a V13 or V14 peer and version 3 to
a V3 to V12 peer (with the Power page's earlier coverage text), and DSP,
Audio, Diagnostics and CAT & Network are version 3 to a V3 to V14 peer.
General and Test retain version 3.
No mirror field or ordinal changes.

Version 6 adds exactly two closed `kind: "table"` controls to the partial
Hardware Config > Antenna / ALEX page: `hardware.antenna.txRows` and
`hardware.antenna.rxRows`. Each has `binding.antennaRows` with
`object: "alexAntennas"` and mode `tx` or `rx`, and requires
`radioAntennaRowsVersion: 1`. The TX table also requires a fresh off-air
`txState`; the RX table has no added transmit-permission gate. The Core
omits both tables for a peer without the row feature and omits the whole
partial Hardware page on a board without Alex filters. V1–V5 peers retain
the scalar controls only. A description never grants an edit by itself.
For a peer that declared the row feature, the supported board's static table
shape remains described while its radio is disconnected. The live row
capability is then absent; current-session capability, radio identity, and
row-command checks must all allow an edit before a cell can be changed.

Each table has one row per antenna-list entry, in the lists' order: the
14 `Band` rows from 160m through XVTR, in enum order, then 2 m (band 27,
R-IOS-26) for a peer that declared `band2m` 1. Each row carries its band
number, so a row's list entry is its position, not its band number. A peer
without `band2m` is sent the 14 rows (station link section 6.1).
The TX columns are Ant 1/2/3 (`field: "tx"`). RX has three RX1 columns
(`field: "rx"`, labels 1/2/3) and three RX-only columns
(`field: "rxOnly"`, labels from the current Core SKU). The RX table's
required `columnGroups` are exactly RX1 over rx1/rx2/rx3 and RX-only over
rxOnly1/rxOnly2/rxOnly3; TX has no column groups. Each row's `cells` carry
the native button tooltips in column order. These are fixed display and
source facts, not a command-template language. The source is the existing
three 15-integer CSV mirrors `txAntennas`, `rxAntennas`, and
`rxOnlyAntennas` (14 for a peer without `band2m`, 2 m last); RX-only value 0 means no button selected. The blocked
TX-port mirrors disable columns 2 and 3. A phone checks all values,
capability, canonical connected MAC and session freshness together, then
uses only the existing `setAlexTxAntennaForRadio` or
`setAlexRxAntennaForRadio` typed verb for a clicked band/port. The Core
rechecks identity and authority and sends the accepted mirror or refusal.
The Setup description revision is not a per-row state revision.

Version 5 adds only two optional PA Values telemetry readouts. The closed
bindings are `{"telemetry":{"object":"radio","name":"paCurrentAmps"}}`
for `PA Current:` (two decimals, A) and
`{"telemetry":{"object":"radio","name":"supplyVolts"}}` for
`DC Voltage:` (one decimal, V). No other telemetry object or field is a
Setup binding.
Both require `stationTelemetryVersion:4`, send no writes, and have no
transmit-permission or off-air gate. The Core projects each row away when its
board lacks the corresponding amps or volts telemetry; the whole PA category
still requires an integrated PA and a non-RX-only SKU. Missing, malformed,
nonfinite, disconnected, or stale-session values are unavailable, while a
present zero is rendered as zero. The phone renderer must apply that freshness
rule to the existing optional station telemetry wire. V1–V4 projections omit
both rows; PA alone has a version-5 ceiling. This adds no peak/min/reset,
temperature conversion, derived formula, or Core command.

Version 4 publishes the partial Display category. Its station-scoped controls
cover Spectrum Defaults FFT/window/Hz-per-bin/FPS, Multimeter polling delay,
and TX Display FFT/window/panadapter/waterfall analyzer settings. The RX quartet
uses `applies: "subscription"`: it writes through the current session's settings
proxy and the existing endpoint subscription path picks up the changed values.
The meter and TX analyzer controls use `applies: "live"`; Core reloads their
running objects, and these keys are not RX subscription fields. All TX Display
controls require `txDisplayVersion: 2`. They have no transmit-permission or
off-air gate because the desktop changes display processing while on air.
Absent, malformed, stale, or unavailable settings disable their controls.

Version 8 appends seven phone-owned RX subscription controls to Display: four
in Spectrum Defaults > Rendering (Spectrum Detector, Spectrum Averaging,
Spectrum Avg Time, Decimation) and three in the new partial Waterfall Defaults
> Display page (WF Detector, WF Averaging, WF Avg Time). The new controls use
only closed `binding.phone` dispatch identities for the phone's existing
per-pan typed settings. Six identities match the desktop's existing display
names; Decimation uses the literal `decimation` field, which has no desktop
settings key. These descriptors do not create Core or phone settings keys or
grant permission to write Core settings. Detector options are numbered 0–4
for spectrum (Peak, Rosenfell, Average, Sample, RMS) and 0–3 for waterfall
(no RMS), both default 0. Averaging options are numbered 0–3 (None,
Recursive, Time Window, Log Recursive), default 3 for spectrum and 0 for
waterfall. Both averaging times span 10–9999 ms in steps of 10, default 30
and 120 ms. Decimation spans 1–16 in steps of 1, default 1. Choices use
exact integer `{value,label}` options. Each control has
`requiresDescriptionVersion:8` and `applies:"subscription"`; detectors gate
on `remoteMediaVersion:1`, averaging and times on `displayExtrasVersion:1`,
and Decimation on `spectrumGrantVersion:2`. These capability gates are
necessary but do not replace the phone's current-session media availability
checks. Versions 1–7 retain their original 11/14-control Display shape and
version numbers (1–3, then 4), and omit the new Waterfall page. Native rendering
behavior is unchanged; phone
parsing and rendering require their own implementation.

Version 9 appends eight phone-owned rendering controls to the existing partial
Display pages, in native order after the V8 RX rows: Spectrum Defaults >
Rendering has Fill under trace, Fill Alpha, Trace gradient, Peak hold, and Peak
Delay; Waterfall Defaults > Display has Update Period, Stop on TX, and Opacity.
Their closed `binding.phone` identities are the desktop's existing local keys:
`DisplayPanFill`, `DisplayFftFillAlpha`, `DisplayGradientEnabled`,
`DisplayPeakHoldEnabled`, `DisplayPeakHoldResetMs`, `DisplayWfUpdatePeriodMs`,
`WaterfallStopOnTx`, and `DisplayWfOpacity`. These are phone-owned per-pan
values, not Core settings writes or new persistence keys. The four toggles
have native defaults true, false, false, false in that order. Fill Alpha is
an integer percent 0–100, step 1, default 70; the phone's existing typed
Double stores that percent divided by 100. Peak Delay is 100–10000 ms, step
100, default 2000. Update Period is 10–500 ms, step 1, default 30; this is
the one row with `applies:"subscription"` because the phone's existing
subscriber derives frames per line from it. Opacity is integer percent
0–100, step 1, default 100. The other seven have `applies:"live"` and
affect phone rendering only. No new media field, setting, authority, or TX
verb is defined. All eight require description version 9. V1–V8 projection
retains its prior control counts and version, and V9 has 29 Display controls
on the same four partial pages. The desktop renderer and native UI behavior
remain unchanged. Phone generic V9 parsing and dispatch require separate
phone work; its existing typed fields do not by themselves consume this
description.

Version 10 appends four phone-owned Waterfall Defaults > Overlays toggles in
native order: Show RX filter on waterfall, Show TX filter on RX waterfall,
Show RX zero line on waterfall, and Show TX zero line on waterfall. Their
`binding.phone` identities are `DisplayShowRxFilterOnWaterfall`,
`DisplayShowTxFilterOnRxWaterfall`, `DisplayShowRxZeroLine`, and
`DisplayShowTxZeroLine`. All four use `applies:"live"` and require description
version 10. Their defaults are false, true, false, false; the TX filter's
true default comes from the desktop renderer's persisted-load path, not its
pre-load C++ member initializer. They remain per-pan operator-local renderer
preferences, with no Core setting write, RX subscription field or new wire
verb. V1–V9 projections retain their previous version numbers and 11/14/21/29
control counts; V10 has 33 controls on the same four partial pages. The
phone's typed settings and render paths already exist, but V10 descriptor
parsing/dispatch and its TX-filter default correction are separately owned.

Version 11 adds the Spectrum Peaks page (`display.spectrumPeaks`,
`where:"phone"`, `coverage:"partial"`) between Spectrum Defaults and
Waterfall Defaults, with all fifteen of its rows in native order. Active
Peak Hold has Enable per-bin peak trace with decay (`activePeakHold`,
`DisplayActivePeakHoldEnabled`, false), Hold duration (`activePeakHoldTime`,
`DisplayActivePeakHoldDurationMs`, integer 100–60000 ms, step 100, default
2000), Drop rate (`activePeakHoldDropRate`,
`DisplayActivePeakHoldDropDbPerSec`, integer 1–60 dB/s, step 1, default 6),
Fill area between peak trace and current trace (`activePeakHoldFill`,
`DisplayActivePeakHoldFill`, false), Update during TX (`activePeakHoldOnTx`,
`DisplayActivePeakHoldOnTx`, false) and Trace color (`activePeakHoldColor`,
`DisplayActivePeakHoldColor`, `#FFD700FF`). Peak Blobs has Show top-N peak
markers (`peakBlobs`, `DisplayPeakBlobsEnabled`, false), Number of peaks
(`peakBlobCount`, `DisplayPeakBlobsCount`, integer 1–20, step 1, default 3),
Only show peaks inside the RX filter passband (`peakBlobInsideFilter`,
`DisplayPeakBlobsInsideFilterOnly`, false), Hold peaks before decay
(`peakBlobHold`, `DisplayPeakBlobsHoldEnabled`, false), Hold duration
(`peakBlobHoldTime`, `DisplayPeakBlobsHoldMs`, integer 100–60000 ms, step
100, default 500), Decay after hold (`peakBlobHoldDrop`,
`DisplayPeakBlobsHoldDrop`, false), Fall rate (`peakBlobFallRate`,
`DisplayPeakBlobsFallDbPerSec`, integer 1–60 dB/s, step 1, default 6), Blob
color (`peakBlobColor`, `DisplayPeakBlobColor`, `#FF4500FF`) and Text color
(`peakBlobTextColor`, `DisplayPeakBlobTextColor`, `#7FFF00FF`). Each id is
prefixed `display.spectrumPeaks.`; each `binding.phone` is the desktop's own
key. The desktop stores these keys once for every pan, and a Setup change
reaches every pan at once. The desktop draws the peak hold trace and the
blobs from the frames it already has; the phone gets the same computation
from the Core's display extras `activePeakHold` and `peakBlobs`
subscription fields. The eleven rows that feed those fields use
`applies:"subscription"`; the fill and the three colors only change the
phone's drawing and use `applies:"live"`. Every row requires
`displayExtrasVersion:1` except Hold duration and Update during TX, which
require 3: the Core that holds each peak for `holdMs` and honors
`activePeakHold.onTx`. The blob hold, drop and fall rows reach the Core as
the existing `holdMs` (0 when the hold is off) and `fallDbPerSec` (0 when
decay after hold is off) numbers, which the Core turns back into the
desktop's switches. Colors are eight-digit `#RRGGBBAA` strings as in
Appearance. No new Core setting or wire verb is defined. V1–V10
projections retain their prior version numbers, page lists and
11/14/21/29/33 control counts; V11 has 48 Display controls on five partial
pages. Phone parsing and dispatch of V11 are separately owned.

Version 12 describes the rest of Setup > Display after Spectrum Peaks, and
Appearance's Reset all colors. Every new row has
`requiresDescriptionVersion:12` and a `binding.phone` that is the desktop's
own key or, for a button, an action identity; the Core accepts each row only
as the exact closed object it publishes. V1–V11 projections keep their
versions, page lists and control counts (Display V11: 48 controls on five
pages; Appearance V7: 13 controls); V12 has 100 Display controls on seven
pages and 14 Appearance controls.

Display pages, in the desktop's order: Spectrum Defaults gains a Profile
section (Reset to Smooth Defaults, Enable Clarity) before Fast Fourier
Transform and a Spectrum Overlays section after Rendering (Show cursor
frequency, Show bin width, Show noise floor, NF shift, NF line width, NF line,
text and fast-attack colours, Normalize trace, Show peak value overlay, its position and
refresh, Get Monitor Hz). Waterfall Defaults gains Levels (High and Low
Threshold, AGC, Use spectrum min/max, Copy spectrum min/max) and Waterfall
NF-AGC (Enable, NF offset) before Display, Color Scheme at the end of
Display, and Rewind history (Depth) and Time (Timestamp Position and Mode)
after Overlays. The new Grid & Scales page (`display.gridScales`) has Grid
(Show grid, Show dBm scale strip, dB Max and dB Min per band, dB Step),
Labels (Freq Label Align, Show zero line, Show FPS overlay), Noise-Floor
Tracking (Adjust grid min, NF offset, Maintain grid range) and Copy (Copy
waterfall thresholds). Multimeter gains Show decimal point, Signal Units
(Display units) and Signal History (History duration). TX Display gains
Waterfall Amplitude Scale (Low and High Level, Palette, Low Color). The new
3D View page (`display.threeD`) has one 3D VIEW section: Reset 3D, Spectrum
(2D or 3D), 3D Floor per band, 3D Gain, Span, Angle and Slice Shadow. The two
new pages describe every control they have and carry no `coverage`. The
desktop's defaults, ranges and labels are the published ones; the ids, keys,
defaults and ranges are listed in `resources/setup/display.json`.

Rows that feed the display extras subscription use `applies:"subscription"`
and gate on `displayExtrasVersion:1`: Enable Clarity, the waterfall
thresholds, AGC, Use spectrum min/max, NF-AGC and its offset (together the
`waterfallLevels` field: `clarity` when Clarity is on, else `noiseFloorAgc`
when NF-AGC is on, else `agc` when AGC is on, else `manual`; low and high are
the thresholds, or the pan's spectrum bottom and top with Use spectrum
min/max; offset is the NF offset), Show noise floor and NF shift (the
`noiseFloor` field) and Normalize trace (`normalize`, sent true only while
the spectrum detector is Average, Sample or RMS; the Core applies it only
then too). The NF line width and line and text colours draw the extras'
noise floor and gate on the same capability; the fast-attack colour needs
the noise floor state (`noiseFloor.fastAttack`) and gates on
`displayExtrasVersion:4`. The 3D Spectrum choice sets the subscription's
`wideSpanFactor` and gates on `remoteMediaVersion:1`. The Grid & Scales
noise-floor tracking rows follow the pan's display noise floor as Thetis
does (every 500 ms, not while transmitting, the extras' noise floor while
its state is not fast attack), or Clarity's estimate from the Core's
`noise-floor` operation while Clarity is on; they gate on
`displayExtrasVersion:4`. The TX
Display waterfall rows colour the transmit display and gate on
`txDisplayVersion:1`. Every other row changes only the phone's drawing,
`applies:"live"`, with no gate.

Three closed fields are new in version 12. `enabledWhen:{"phone":<key>,
"oneOf":[<values>]}` enables a row only while the named row of this
description holds one of the values (Normalize: spectrum detector 2, 3 or 4;
the waterfall thresholds, AGC and NF-AGC rows: Use spectrum min/max false;
the grid NF offset and Maintain grid range: Adjust grid min true); the
desktop disables the same rows. `perBand:{"label":<template>}` marks a row
the desktop keeps once per band: the value is stored under
`<binding.phone>_<band>` for the pan's current band (160m, 80m, 60m, 40m,
30m, 20m, 17m, 15m, 12m, 10m, 6m, GEN, WWV or XVTR; the band of the pan's
centre frequency as `Band::bandFromFrequency` finds it), shared by every pan,
and the label shows the band where the template has `%1`. `confirm` is the
desktop's exact question before a destructive action: the renderer asks it
with Yes and No, No the default, and acts only on Yes. On a `kind:"toggle"`
row (version 13), `confirm` comes with `confirmWhen`, the value the desktop
asks before setting: the renderer asks the question with Yes and No, No the
default, only when the operator sets the toggle to `confirmWhen`; on Yes it
writes, on No the toggle stays as it was and nothing is written. Setting it
the other way writes at once. A peer that knows no `confirmWhen` writes at
once, as before.

A `kind:"button"` row with a `binding.phone` is a phone action. It has no
value or default and acts on the pan the page is editing, exactly as the
desktop's button acts on its active pan, in a local or a remote window:
`smoothDefaults` sets Color Scheme to Clarity Blue (7), Spectrum Averaging to
Log Recursive (3), Trace & Fill Color to `#FFFFFFE6`, Fill under trace off,
waterfall AGC on and Update Period 30 ms; `getMonitorHz` sets FPS to the
screen's refresh rate rounded and held to 10–60, as an edit of the FPS row
(the same availability); `copySpectrumMinMax` sets High Threshold to the
pan's spectrum top and Low Threshold to its bottom; `copyWaterfallThresholds`
sets the current band's dB Max and dB Min to the waterfall High and Low
Threshold, rounded; `reset3d` sets the six 3D rows to their defaults (3D
Floor for the current band); `resetColors` sets the ten Colors & Theme
swatches to their defaults. No action writes a Core setting except
`getMonitorHz` through the FPS row.

Not described, with the reason: Line Width (the phone has its own line
width, D75), Cal Offset and Display Thread Priority (the Core calibrates and
schedules the display; a remote window disables both), the noise floor text
position (disabled on the desktop, no effect), the TX Custom Gradient (no
gradient editor kind), the Waterfall Low Level Color (unbuilt) and the
Multimeter peak hold, text hold, averaging window, digital delay and history
enable (unbuilt). Derived readouts (bin width, delay, effective rewind) and
the cross-links are not controls.

Version 13 describes the rest of Setup > PA's Watt Meter and PA Values
pages, three built Hardware Config tabs, the Alex-1 and Alex-2 Filters tabs'
receive rows, Radio Info's sample rate and Transmit > Power's Disable HF PA
(R-R3-46, R-R3-49, R-IOS-18). Every new row
has `requiresDescriptionVersion:13` and the Core accepts it only as the exact
closed object its resource carries; V1–V12 projections keep their versions,
page lists and controls (PA V5–V12: version 5, two pages on the ANAN-G2E, one
elsewhere; Hardware V6–V12: version 6, Antenna / ALEX only, and no Hardware
category at all on a board without ALEX filters).

PA pages, in the desktop's order: PA Gain (unchanged), Watt Meter
(`pa.wattMeter`, new, `where:"mixed"`, every control described), PA Values.
The Watt Meter's PA Forward Power Calibration section has the board class's
ten points (`pa.wattMeter.calPoint1` to `calPoint10`), then a PA Values
section with Show PA Values page and Reset PA Values. PA Values gains PA
Temperature after PA Current, ADC Overload after REV Voltage, and a Reset
section with Reset Peak/Min.

A calibration point is `kind:"decimal"` with the closed binding
`{"radioSetting":"paCalibration/calPoint<N>"}`. The Core projects each for
its radio's model: the label is the point's factory value (`"10 W"`), which is
also its `default`; `min` 0, the point's own `max` (Thetis's box for that
point: ANAN-10 class and the HL2 10 W except 11 and 12 W for points 9 and 10,
ANAN-100 class 100 W except 110 and 120 W, ANAN-8000 class 100, 100, 100,
120, 140, 200, 200, 200, 220 and 240 W), `step` 0.1, `decimals` 1, `unit`
`W`, and `boardClass` (1 ANAN-10, 2 ANAN-100, 3 ANAN-8000). A model without a
class removes the ten points. The gate is `transmitSettingsVersion:6`, the
desktop's own gate for this table, with no `offAir`: Thetis gives the table
no transmit rule (it corrects the forward-power reading only, and its boxes
are read live with no MOX check).

`binding.radioSetting` is new in version 13: a key under the connected
radio. The renderer reads and writes `hardware/<MAC>/<radioSetting>` through
the current session's settings proxy, where `<MAC>` is the canonical MAC of
the Core's connected radio in the current session (the MAC antenna rows and
Settings Validation use). A missing, stale or other-radio MAC disables the
control; the Core refuses another radio's key. Values are the stored strings
(decimals as numbers written with `.`, toggles with their `valueEncoding`).
A point reads as its stored `calPoint<N>` while
`hardware/<MAC>/paCalibration/boardClass` reads as the row's `boardClass`,
and as its `default` while that key is absent or `0`; another class disables
the point with a plain reason (the table was saved for another model). An
edit first writes `boardClass` when it is absent or `0`, then the point. The
Core takes a point on or off the air; a point written while the radio
transmits reaches the meter once the radio is back on receive, as a remote
desktop window's does. A value outside the row's range, a
non-number, or a `boardClass` other than the Core's radio's is refused whole
with the range in plain words (for example "Choose a calibration point from
0 to 10 W."), and the Core hands back its value; nothing is clamped.

`pa.values.paTemperature` is a readout of the optional station telemetry
`{"telemetry":{"object":"radio","name":"paTemperatureCelsius"}}`, one decimal,
`unit` `°C`, gate `stationTelemetryVersion:4`, with the closed field
`"temperatureUnit":"PaTempUnit"`: the renderer shows it in the viewer's own
PA temperature unit (the desktop's C/F toggle, `PaTempUnit` `C` or `F`,
default `C`), converting °F = °C × 9 / 5 + 32. `pa.values.adcOverload` has the
closed binding `{"adcOverload":{"object":"stepAtt"}}` and gate
`radioHardwareVersion:1`: it reads the `stepAtt` mirror's `overloadAdc0` and
`overloadAdc1` (0 none, 1 or 2 overloaded) and shows `Yes (ADC 0)` while ADC 0
is overloaded, else `Yes (ADC 1)` while ADC 1 is, else `No`, as a remote
desktop window does; a missing or stale object is unavailable. It has no
`decimals`.

Reset Peak/Min (`pa.values.resetPeakMin`) and Reset PA Values
(`pa.wattMeter.resetPaValues`) are the one phone action `resetPaValues`.
While a version 13 renderer shows PA Values it tracks, from the first change
after the page opens, the running peak and minimum of Forward (calibrated),
Reflected, SWR, PA Current, PA Temperature and DC Voltage, and shows a row as
the desktop does: the value, then `  (P <peak> / M <minimum>)` at the row's
decimals once the two differ (temperature in the viewer's unit). The action
restarts each tracker at its current value. It is the viewer's own and
reaches no other device. Show PA Values page (`pa.wattMeter.showPaValues`)
is a phone toggle, `binding.phone` `display/showPaValuesPage`, default true:
off hides the PA Values page from the viewer's own Setup, as the desktop
hides its PA Values page.

Hardware Config pages, in the desktop's order: Radio Info
(`hardware.radioInfo`, new, `where:"mixed"`, partial), Antenna / ALEX
(unchanged, only with ALEX filters), Calibration (`hardware.calibration`,
new, partial) and HL2 I/O (`hardware.hl2Io`, new, partial, only on the HL2's
I/O board). Radio Info's Board Identity section has seven readouts with the
closed binding `{"radioInfo":<field>}` (`board`, `protocol`, `adcCount`,
`maxRx`, `firmware`, `mac`, `ip`) and a `value` the Core fills with the
desktop tab's exact text for its current radio (a dash, U+2014, for a value
the radio has not reported); the description changes, and its revision
advances, when the Core's radio does. Its Support section has Copy Support
Info to Clipboard, phone action `copySupportInfo`, whose `copyText` the Core
fills with the tab's exact clipboard text; the action copies it on the
viewer's own device. Calibration's TX Display Cal section has Offset
(`hardware.calibration.txDisplayOffset`, `radioSetting` `cal/txDisplayOffset`,
-100 to 100 dB, step 0.1, one decimal, default 0), gate
`transmitSettingsVersion:8` and no off-air gate: the Core takes it on the air
too, as Thetis changes it while transmitting. HL2 I/O's Configuration section
has Enable N2ADR Filter board (`hardware.hl2Io.n2adrFilter`, `radioSetting`
`hl2IoBoard/n2adrFilter`, `True`/`False`, default true), gate
`transmitSettingsVersion:8`; the Core applies its whole preset once the
radio is back on receive.

Radio Info gains an Operating Parameters section between Board Identity and
Support with Sample rate (`hardware.radioInfo.sampleRate`, `kind:"choice"`).
Its binding is the command `setRadioSampleRate {rateHz: $controlValue}`
with `valueProperty` `slice:active`'s `sampleRateHz`, gate
`radioHardwareVersion:9` plus `offAir:true`: the whole radio's rate, every
receiver at once, as the desktop's box changes it. The Core fills `options`
with the rates the desktop's box lists for its radio (`value` the rate in
hertz, `label` its digits); with none it adds `availability` disabled with
"The radio is not connected, so its sample rate cannot change." The Core
refuses the verb from a device signed in with the pairing token, while the
radio is on the air, and for a rate outside the list.

Two pages follow Antenna / ALEX, both `where:"station"` and partial:
Alex-1 Filters (`hardware.alex1Filters`, on every board with ALEX filters)
and Alex-2 Filters (`hardware.alex2Filters`, only where the board has
Alex-2). Alex-1 Filters has one bank section: the bank the Core programs
for the board (`codec::alex::usesBpf1Preselector`, the selector
`computeRxPreselector` uses, Thetis setAlex1HPF at console.cs:6827-6837
[v2.10.3.15]), the desktop's own gate. That is Saturn BPF1 Bands on the
OrionMKII, Saturn, Saturn MkII and HermesC10 boards (ANAN-7000DLE, 8000DLE,
Anvelina Pro 3, Red Pitaya, plain ORION MKII, ANAN-G2E, G2, G2-1K), and Alex
HPF Bands on every other board. It matches Thetis's panel list
(setup.cs:6336-6360) on every model but the plain ORION MKII, where Thetis
programs BPF1 yet shows the HPF panel; the page shows BPF1 there, the rows
that take effect. Alex-2
Filters has Alex-2 HPF Bands, whose first row is ByPass / 55 MHz BPF
(master) (`hardware.alex2Filters.bypass55MhzBpf`, `radioSetting`
`alex2/master/bypass55MhzBpf`). Each bank has six rows in the desktop's
order, each row three controls with ids `<page>.<bank>.<slug>.bypass`,
`.start` and `.end` (bank `hpf` or `bpf1`; slugs `1_5MHz`, `6_5MHz`,
`9_5MHz`, `13MHz`, `20MHz`, `6mBP`), labelled with the desktop's row label
and Bypass, Start or End, and bound to `radioSetting`
`<prefix>/<slug>/enabled|start|end` (prefix `alex/hpf`, `alex/bpf1` or
`alex2/hpf`). Bypass boxes are `True`/`False` toggles, default false; the
edges are decimals from 0 to 200 MHz, step 0.001, six decimals, unit `MHz`,
defaulting to Thetis's spinner values. Every row has the gate
`radioHardwareVersion:8` and no off-air rule: the Core applies a change to
its radio at once, on or off the air, as Thetis's setters do.

The shown bank section opens with the tab's five switches above its rows, in the
desktop's order: HPF Bypass (master), HPF Bypass on TX, HPF Bypass on
PureSignal feedback, Disable 6m LNA on TX and Disable 6m LNA on RX (ids
`hardware.alex1Filters.hpfBypass`, `.hpfBypassOnTx`, `.hpfBypassOnPs`,
`.disable6mLnaOnTx`, `.disable6mLnaOnRx`; `radioSetting`
`alex/master/<the same name>`). Each is a `True`/`False` toggle with an
empty tooltip, defaulting as the desktop does (on PureSignal feedback and
6 m LNA on TX checked, the other three clear), gate
`radioHardwareVersion:8` and no off-air rule. The three the Core counts as
transmit hardware (on TX, on PureSignal feedback, 6 m LNA on TX) add
`transmit:true`: with remote transmit allowed, the Core takes a write of them
only from a session permitted to transmit. The desktop asks before clearing
HPF Bypass on PureSignal feedback (the IMD warning, Thetis setup.cs
`chkDisableHPFonPS_CheckedChanged`), and so does the description: that row
carries the desktop's warning as `confirm` with `confirmWhen:false`, so a
peer asks the same question before clearing it and leaves it set on No.

Transmit > Power gains a PA Control section last with Disable HF PA
(`transmit.power.DisableHfPa`, `setting` `DisableHfPa`, `True`/`False`,
default false, gate `transmitSettingsVersion:11` plus `transmit:true` and
no off-air rule: the Core applies it on and off the air, as Thetis does).
On the Hermes and the Atlas/Metis kit, which have no such switch, the row
carries `availability` disabled with "This radio cannot switch off its HF
PA from here."

Not described, with the reason: PA Gain's profile choice, New, Copy, Delete,
Reset Defaults, per-band gain, drive-step adjusts and max power (the profile
bank is serialized per profile and no closed profile state or command exists
on the wire yet); New Cal (hidden on the desktop, as in Thetis); the
auto-calibration sweep (it keys the radio from the desktop's own window);
the ANAN-8000DLE title bar volts/amps box (the desktop's title bar); the
Alex-1 Filters tab's LPF edges and 6m/ByPass on RX (described from version
17, below); OC Outputs; the rest of Calibration (frequency and level calibration,
6 m LNA offsets, the correction factors, Volts/Amps calibration and its log);
HL2 Options (described in version 16); and the rest of HL2 I/O (register, state machine, I2C and
bandwidth monitor views, probe and reset). No new wire field, verb or
capability value is defined.

Version 14 describes PA Gain's profiles (R-R3-49, R-IOS-18). The page
`pa.gain` now carries, on every radio with a PA, a Profile section (the
profile choice, New, Copy, Delete, Reset Defaults) and a PA Gain by Band
(dB) section with one table; the ANAN-G2E's bypass box stays last and is
still the G2E's only. Every new row has `requiresDescriptionVersion:14`,
the gate `{"capability":"paProfileVersion","min":1,"offAir":true}`, and is
accepted only as the exact closed object in `resources/setup/pa.json`.
V13 and older keep their projections (PA Gain appears there only on the
G2E, with the bypass box alone).

The data is the Core's read-only `paProfiles` object (link document,
paProfileVersion 1): `json` holds `names` (the profiles the desktop's combo
lists: this radio's factory profile and every user profile), `active`,
`factory` and `bands`, the active profile's 14 rows in Band order, each
`{band, gain, adjust[9], maxPower, useMax}` at one decimal place. The
closed bindings are:

- `{"paProfile":"active"}` on the `choice`: its options are `names`, its
  value `active`; a pick sends `paProfile.select {name}`.
- `{"paProfile":"new"}`, `"copy"`, `"delete"`, `"reset"` on the buttons.
  New and Copy carry `prompt` `{title, label, default}` (Copy's default is
  `"%1 (copy)"`, `%1` the active profile's name); the renderer asks for a
  name and sends `paProfile.new {name}` or `paProfile.copy {name}`. Delete
  and Reset Defaults carry the desktop's `confirm` (`%1` the active name)
  and send `paProfile.delete {name: active}` or `paProfile.reset {}` on
  Yes.
- `{"paProfileGrid":{"object":"paProfiles"}}` on the `table`: `rows` are
  the 14 bands (`band` 0..13 and label), `columns` the Gain (dB) column
  (38.8..100 dB), the nine drive-step adjusts (`driveStep` 0..8, labels
  10%..90%, -10..10 dB), Max W (0..1500 W), each a decimal of step 0.1 and
  one decimal place, and Use Max (a toggle). A column's `tooltip` has `%1`
  for the row's band. A cell edit sends `paProfile.setGain {band, value}`,
  `paProfile.setAdjust {band, step, value}`, `paProfile.setMaxPower {band,
  value}` or `paProfile.setUseMax {band, on}`.

The Core does what the desktop's page does: select (only a listed
profile), New (a profile seeded from this radio's factory row; names must
not start with "Default" or repeat one there), Copy (the active profile's
values under a new name), Delete (never the last one; the radio's factory
profile is then selected, as Thetis does), Reset Defaults (the active
profile's factory values for its model), and the cell edits, each rounded
to one decimal place and refused whole outside its column's range. While the
radio is on the air the Core follows Thetis: select, New, Copy, Delete,
Reset Defaults and every other band's cells are refused ("Can't change
while transmitting."), and the transmitting band's gain, adjusts, Max W
and Use Max are taken live from the device that holds transmit only
("Only the device that is transmitting can change this."); an Adjust edit
moves the drive to that step, as `setup.cs:24212-24222 [v2.10.3.15]`
does. Each verb also meets the gates the Core gives the desktop's own PA
profile writes: a receive-only Core takes it from a peer offered transmit
settings, and with remote transmit allowed only from a device that may
transmit. A peer must declare `paProfiles:1` to get the object, the
capability and the verbs.

Version 15 describes the rest of Setup > DSP, Transmit, Audio, Diagnostics
and CAT & Network (R-R3-49, R-IOS-18, R-IOS-27). Every new row has
`requiresDescriptionVersion:15`. A peer that declares 15 or higher receives
these five categories as version 15; a V3 to V14 peer receives each exactly
as before (Transmit as version 13 to a V13 or V14 peer, otherwise version
3; V1 and V2 keep their own), with the same pages, controls and coverage
words. The source files carry the older coverage and
a `coverageV15` value (on the category or a page) that replaces it for a V15
peer; an empty `coverageV15` removes the page's coverage, meaning the page is
complete. `coverageV15` itself is never sent. Versions 13 and 14 are PA,
Hardware Config and Disable HF PA's; these rows do not use them.

A version 15 row is not a copy the Core compares; the Core checks it against
its own sources (`SetupDescriptionV15::validateControl`). A `property` binding
names a mirrored object this peer receives (`slice:active`, `transmit`,
`stepAtt`, `dspAssets`, `radio`, `txState`, `amplifier`, `tuner`, `rfkit`,
`accessoryData`, `accessorySettings`) and a property its schema has; an
editable row needs a writable property the Core takes inbound, of the row's
kind (a toggle a Boolean, an integer or slider an integer, a decimal a
number, a choice an enum or integer, text a string). A `setting` binding
names a Station key the Core owns through the settings proxy. A `telemetry`
binding names a radio telemetry field and gates on the
`stationTelemetryVersion` that first sends it. A `command` binding names a
verb the Core runs, with its capability gate and argument types, as before.
A `phone` binding names a value the phone draws or chooses, or an action
it takes (below). Missing,
stale or malformed values are unavailable, never zero; a row whose gate the
renderer cannot evaluate is disabled.

New closed fields and sources in version 15:

- `applies:"staged"`: the row holds a value the operator is preparing; it
  starts from its `property` (or `setting`) and sends nothing. A button in
  the same section sends it with the argument source `{"$control":"<id>"}`.
  The desktop's Apply buttons (NNR Diagnostics, device network settings)
  work this way.
- `{"$selectedOwnedSliceId":true}` may fill any verb's `sliceId` (it was TNF
  Add's only), with the same checks: the selected slice, owned, this session
  and epoch.
- `{"$prompt":true}` fills a text argument with what the button's `prompt`
  asked for. `prompt` is `{title, label, initial, overwrite}`: the question's
  title and field label, `initial` a `$property` the field starts with, and
  `overwrite` `{title, question, namesFrom}`: when the trimmed name is already
  one of the JSON array of names `namesFrom` holds, ask `question` (its `%1`
  is the name) with Yes and No, No the default, and send only on Yes. An
  empty name sends nothing.
- `confirm` (the version 12 field) may carry `%1`: the value the command's
  first text argument resolves to (the profile name), or on Filter Presets
  the mode's label.
- `choicesFrom`: `{"jsonNames":{"object":"transmit","name":"txProfilesJson"}}`
  lists the names in that JSON array as the choices, each value its name;
  `{"dspAssets":"nr3"}` lists, after the row's `options`, the valid NR3
  models from the phone's `dspAssets.list` result (value the asset id, label
  its label, or a short id when the label is empty). The chosen value is
  sent as text. A value the list does not hold shows as "Missing model".
- `options` may carry text values (with `choicesFrom`) or `false`/`true` (a
  pair of radio buttons that set a Boolean, such as Mic In / Line In).
- `enabledWhen:{"property":{object,name},"oneOf":[...]}` enables a row only
  while that mirrored value is one of the values; the desktop disables the
  same rows.
- `valueOffset`: the row shows and edits the property plus this number
  (APF Center Freq is the CW pitch, 600 Hz, plus the slice's `apfTuneHz`);
  `min` and `max` are shown values.
- `rangeFrom:{"catalogueTransmit":"tunePower"|"micGainDb"}`: the range,
  step and shown values come from the Core's catalogue `board.transmit`
  entry of that name (catalog ranges), the same the desktop reads.
- `unsavedChanges` (TX Profile's choice): `{title, question, saveVerb,
  watch}`. While any `transmit` property in `watch` changed since the page
  opened or the active profile last changed, choosing another profile first
  asks `question` (its `%1` the current profile) with Yes, No and Cancel:
  Yes sends `saveVerb` with the current name and then the choice, No sends
  the choice, Cancel keeps the current profile.
- `format` on a readout says how the value shows, in the desktop's words:
  `nnrLimit` (0 hidden; 1 "Noise reduction is using the Standard model. The
  Core computer could not keep up with Premium."; 2 "Noise reduction was
  turned off. The Core computer could not keep up."), `nnrModelSlot` (0
  Standard, 1 Premium), `nnrStatus` (`nnrLastError`, else `nnrStatus`),
  `positiveOrNone` ("none" at 0 or below), `enabledOff` ("enabled" /
  "off"), `phaseRotator` ("ON · <freq> Hz · <stages> stages" / "OFF", from
  the transmit phase rotator values), `cfcBands` ("ON · 10 bands" / "OFF"),
  `cessb` ("ON"; "OFF" while CPDR is on; "OFF (gated on CPDR)"), `radioName`
  (`radio.name`, else `radio.model`, else "–"), `uptime` ("<m>m <ss>s", or
  "<h>h <mm>m <ss>s"; "–" when absent), `txMode` ("TX" / "RX (idle)"),
  `paTemperature` (Celsius in the viewer's own PA temperature unit, one
  decimal), `wattsWhileKeyed` ("<w> W" to one decimal while `txState.keyed`,
  else "– W"), `swrWhileKeyed` ("<swr>:1" to one decimal while keyed, else
  "1.0:1"), `kilobytesPerSecond` (bytes per second / 1024, one decimal,
  "KB/s"), `throttleActive` ("Active" / "None active"), `throttledOk`
  ("THROTTLED" / "ok"). A Boolean readout without a format shows Yes or No.
- `gate.micLine:true`: the row needs this device's own microphone line to
  the Core open (the desktop's VOX rule); the Core refuses VOX without it.
- `binding.phone` on a button is a phone action: `openTxEq` opens the phone's
  TX equalizer; `openSetupPage` with `target` (a page id) opens that Setup
  page. On another kind it names a value: `NotchVisualEnabled` (TNF's
  Visual Notch; the phone draws the dent itself, but the value is the
  Core-wide Station setting of that name, read with `settings.snapshot` and
  changed with `settings.write`, shared with every window) and
  `filterPresetsMode` (the Filter Presets page's mode, which the phone
  keeps).

DSP version 15 (10 pages). NR/ANF gains the RNNoise Model (Global) section
(the Core's NR3 model, `dspAssets.selectNr3Model`, `dspAssetVersion` 2, and
its status), the NNR limit (a readout and Try again, `nnr.tryAgain`, enabled
while a limit is in force) and an NNR Diagnostics section: fourteen readouts
of the selected slice's NNR state, the staged Test mode and Output mode, and
Apply until reconnect (`nnr.setDiagnostics`, enabled while NNR is ready;
the modes last until the app reconnects). CW gains APF Center Freq. TNF gains
Visual Notch (the Core-wide `NotchVisualEnabled` setting) and is complete. The new Filter Presets page has
the mode choice (LSB to DRM, the desktop's order, default USB), the Presets
table and the three resets; Options gains the high-resolution filter graph
setting (`dspInfoVersion` 1; the Core sends the curve through
`dsp.filterResponse`) and the time to the last change, and is complete.

The Filter Presets table (`binding.filterPresets.modeFrom` names the mode
row) is closed: its rows are the Core catalogue's `filterPresets` entry for
the chosen mode's label, one row per slot, in order, with columns # (the slot
plus one), Name (text, at most 32 characters), Low (Hz) and High (Hz)
(integers, -10000 to 10000), Width (Hz) (|high - low|) and Reorder (up on
every row but the first, down on every row but the last). An edit of a row
writes the slot's three Station settings together, as text:
`filters/<MODE>/<slot>/name`, `/low` and `/high` (an empty name is "F<slot
plus one>"). Moving a row swaps two slots and rewrites every slot of the mode
in its new order, after removing the mode's keys for slots 0 to 9. Reset
Selected Row removes the selected slot's three keys; Reset All Rows for This
Mode removes the mode's keys for slots 0 to 9; Reset Every Mode to Defaults
removes them for every mode. The Core's defaults (Thetis's) return for any
slot without all three keys; the catalogue follows every change.

Not described in DSP, with the reason: the NR3 and NNR model files (Models…
opens a manager that adds and saves files; the phone's own), the per-band CFC
editor (described in version 19 as `dsp.cfc.bands`; a V15 to V18 peer keeps
this coverage text), the Options warning
marks (worked out from the combos, not settings), the hidden unbuilt CW
keyer, CW timing, APF bandwidth and gain, SAM, AM squelch tail, FM deviation
and FM transmit, and the static notes.

Transmit version 15 (3 pages). Power gains ATT on TX, ATT on TX (dB) and
Force ATT on Tx to 31 when PS-A is off (the `stepAtt` object; the value's
range is the Core radio's attenuator minimum to 31, filled in with its
tooltip), a Tune section (Drive Source; Fixed Tune Power, enabled with Use
Fixed Drive, its range and shown values from the catalogue) and PA Control's
Disable HF PA is version 13's (above). The TX TUN Meter choice is not
described: it is wired to nothing on the desktop. The new Speech Processor
page shows the active profile and the state of each stage (TX EQ, Leveler,
Phase Rotator, CFC, CESSB, DEXP) with the buttons that open where each is set
up; ALC's row is the static "always-on" and is not a row. DEXP/VOX gains
Enable VOX (`transmit` `voxEnabled`, `remoteTxVersion` 1, transmit
permission and this device's microphone line) and is complete.

Audio version 15 (2 pages). The new TX Input page has Mic Gain
(`micGainDb`, catalogue range) and, on a radio with a microphone jack, its
family's Radio Mic section: Hermes / Atlas (Mic In or Line In, +20 dB Mic
Boost, Line In Gain -34 to 12 dB), Orion-MkII (Mic Tip-Ring, Mic Bias, Mic PTT
Disabled, +20 dB Mic Boost) or Saturn G2 (3.5 mm Jack or XLR, Mic PTT
Disabled, Mic Bias, +20 dB Mic Boost). They gate on `transmitSettingsVersion`
3 and carry no off-air rule (see the off-air sweep below). The microphone source and the
microphone device, buffer and test are each computer's own and are not
described (the source is not on the link). TX Profile gains the profile
choice (`txProfile.select`, with the unsaved-changes question), Save... (a
name prompt, then `txProfile.save`) and Delete (confirm, then
`txProfile.delete` of the active profile; the Core refuses the last
profile and says so), and is complete. Devices, VAX, TCI and Advanced are
each computer's own (Advanced's Core-side DSP group is hidden, unbuilt).

Diagnostics version 15 (3 pages). Radio Status (its status bar, PA Status,
Forward / Reflected / SWR and Connection Quality readouts) and Connection
Quality (its Live Counters) come before Settings Validation, the desktop's
order. PA Voltage reads `supplyVolts` on the ANAN-G2E and `paVolts`
elsewhere, as RadioModel::paRowVolts does. Not described: the PTT source card
(its history is not on the link), the Settings Hygiene card (the same panel
as Settings Validation) and the hidden 60 s history.

CAT & Network version 15 (5 pages: TCI Server, then 4O3A, PowerGenius XL and
Tuner Genius XL, the 4O3A page's three tabs, then RF-Kit). The rows are the
remote window's: its captions, and the Core's verbs, each gated on the
capability its verb or object names. 4O3A: the master switch
(`setFourO3AEnabled`), the FlexAPI listener's status, a Host IP, Port,
Connect, Disconnect and Status row set for each device (address edits send
`setTgxlAddress` / `setPgxlAddress` with the other field's current value;
Host, Port and Connect are enabled while the device is disabled, disconnected
or in error, Disconnect while it is on its way or connected; the desktop
shows one button whose caption follows), the Power Genius's band follow and
the TX interlock (Interlock Mode, Grace Period, Enable SWR Gate, Max SWR:
each edit sends `setTxInterlockPolicy` with its own new value and the other
three current values). PowerGenius XL: Nickname (`setPgxlName`), Fan Mode and
LED Intensity (`setPgxlHardware`, one field each), the soft power cap
(`setPgxlPowerCap`), Network (staged Use DHCP, IP Address, Netmask and
Gateway, then Apply Network Settings with its question, `setPgxlNetwork`),
Auto-pair on connect (`PGXL_PairAttempt`), eight Diagnostics readouts, Clear
All faults, Revert and Save & Reboot Amp (with its question). Tuner Genius
XL: Nickname, the three antenna labels (`TGXL_Ant<N>_Label`), Network,
Auto-recall tune memory (`TGXL_AutoTuneMemoryRecall`), eight Diagnostics
readouts, Revert and Save & Reboot Tuner. RF-Kit: the integration switch
(`setRfKitEnabled`), its status and band follow, Host and Port
(`setRfKitAddress`), Auto-reconnect and Poll interval (`RfKit_AutoReconnect`,
`RfKit_PollIntervalMs`), Connect (`configureRfKit` with the shown address),
Disconnect, Set amp to TCI mode (off the air, while connected), Reset amp
error state, the four antenna labels (`RfKit_Ant<N>_Label`, at most 12
characters) and six live diagnostics readouts. Setting rows write as they are
edited; the desktop's RF-Kit Save writes the same keys at once. More formats:
`connectionPhase` (0 "Disabled at the Core", 1 "Disconnected", 2
"Discovering at the Core", 3 "Connecting at the Core", 4 "Identifying
device", 5 "Retrying at the Core" (with ": <error>" when there is one), 6
"Connected: <model> <serial>", 7 "Error: <error>", from the same object's
`connectionError`, `deviceModel` and `deviceSerial`), `bandFollow` and
`rfkitBandFollow` (the desktop's band-follow sentences; RF-Kit's third names
`bandFollowAddress` and `bandFollowPort`), `fourO3AListener` ("Core listening
on TCP 4992", "Core listener error: <error>", "Core listener starting",
"Disabled at Core"), `sinceMs` (the time since that epoch, "<h>h <mm>m
<ss>s", "<m>m <ss>s" or "<s>s"; "--" at 0), `bytes` ("<n> B", or KB, MB, GB
to one decimal, by 1024), `rttAverage` ("<n> ms avg", "--" at 0) and
`clockTime` ("HH:mm:ss" local time of that epoch, "--" at 0). Not described:
Scan LAN (the scan result is a list the description has no kind for), the
fault history and tune memory tables and their clear buttons, the Core-side
Power Genius tab of a remote window (Operate and Standby are on the phone's
amplifier controls; connection settings keep their defaults), the Power
Genius's Bias Mode, TX Antenna and Follows slice (radio buttons without a
group the description can name; to be grouped on the desktop first) and the
static notes.

Version 16 describes the HL2 Options tab's Hermes Lite Options on the
Hermes Lite 2's HL2 I/O page (`hardware.hl2Io`), as a second section after
Configuration, after version 15. Every row has `requiresDescriptionVersion:16`, `applies:"live"`, the
gate `{"capability":"transmitSettingsVersion","min":8}` and a closed
`{"radioSetting":"hl2/<key>"}` binding, written as the desktop's own tab
writes `hardware/<mac>/hl2/<key>`, and is accepted only as the exact object
in `resources/setup/hardware.json`. The two transmit timing rows also carry
`"transmit":true` in their gate, since the Core treats those keys as
transmit hardware settings. In the desktop's order:

- `hardware.hl2Io.txLatency`, TX buffer latency, integer 0..70 ms, default 20.
- `hardware.hl2Io.pttHang`, PTT hang, integer 0..30 ms, default 12.
- `hardware.hl2Io.cl2Enable`, Enable CL2, toggle, default off.
- `hardware.hl2Io.cl2Freq`, CL2 frequency, 1..200 MHz, default 116;
  integer at 16, decimal from 18 (step 0.1, 3 decimals; below).
- `hardware.hl2Io.ext10MHz`, External 10 MHz reference, toggle, default off.
- `hardware.hl2Io.disconnectReset`, Reset on Ethernet disconnect, toggle.
- `hardware.hl2Io.psSync`, Disable power supply sync, toggle.
- `hardware.hl2Io.bandVolts`, Band Volts (PWM out 0–3.3 V), toggle.
- `hardware.hl2Io.swapAudioChannels`, Swap audio channels, toggle.

Toggles use `valueEncoding` True/False and default off. Three rows the
Core stores but does not send to the radio carry `availability
{enabled:false, reason}`, and the desktop tab, a remote window and the phone
show them disabled with that reason: Enable CL2, CL2 frequency and External
10 MHz reference ("NereusSDR does not change the radio's clock settings.").
Swap audio channels was a fourth such row until the Core sent its radio the
receive audio (the radio codec lane, radioHardwareVersion 13). It is now
open at every version from 16, with the tooltip "Swap the audio channels
sent to the HL2", and its shape is unchanged. A remote window enables it
against a Core at radioHardwareVersion 13 or later. A board without the HL2 I/O board has no
`hardware.hl2Io` page, so none of these rows. V13–V15 peers receive Hardware
at version 13 without the section. No new wire field, verb or capability
value is defined.

Version 17 describes the Alex-1 Filters tab's low-pass rows (R-R3-46,
R-R3-49). The Core applies them to its radio as Thetis's setAlexLPF selects
the transmit low-pass (console.cs:7177-7243 [v2.10.3.15]), with capability
`radioHardwareVersion` 10. A peer that declares 17 receives
Hardware as version 17; a V16 peer receives it as version 16, with no
low-pass row, a V13 to V15 peer exactly as version 13, and V6 to V12 keep
version 6. Every new row has `requiresDescriptionVersion:17`.

`hardware.alex1Filters` gains an Alex LPF Bands section after its bank
section, on every board that has the page. It holds seven rows in the
desktop's order, each two decimals with ids
`hardware.alex1Filters.lpf.<slug>.start` and `.end` (slugs `160m`, `80m`,
`40m`, `20m`, `15m`, `10m`, `6m`), labelled with the desktop's row label
(160m, 80m, 60/40m, 30/20m, 17/15m, 12/10m, 6m) and LPF Start or LPF End,
bound to `radioSetting` `alex/lpf/<slug>/start|end`: each edge limited to
its Thetis spinner's range (`codec::alex::kAlexLpfEdgeLimits`), step 0.001, six decimals, unit `MHz`, defaulting to Thetis's spinner
values (0 to 2.5, 2.500001 to 5, 5.000001 to 8, 8.000001 to 16.5, 16.500001
to 24, 24.000001 to 35.6, 35.600001 to 61.44). Their gate is
`radioHardwareVersion:10` plus `transmit:true` and no off-air rule: they
are the transmit low-pass table, so with remote transmit allowed the Core
takes a write only from a session permitted to transmit, and it stores the
edge on or off the air for the next selection, as Thetis's spinners do. The
section ends with 6m/ByPass on RX (`hardware.alex1Filters.lpfBypass`,
`radioSetting` `alex/master/lpfBypass`, a `True`/`False` toggle, default
false, tooltip "Selects the 6m LPF during receive regardless of
frequency.", gate `radioHardwareVersion:10` with no transmit flag and no
off-air rule). On the ANAN-8000DLE, ANAN-7000DLE, ANAN-G2, ANAN-G2 1K and
Anvelina Pro 3, where Thetis hides it, the row carries `availability`
disabled with "This radio does not have the 6m low-pass bypass on
receive." The low-pass in use is not a Setup row: a peer that declares
`alexLpf` 1 reads it from `radio`'s `alexLpfBits` (link document section
7.1). No other wire field changes.

Version 18 opens the three clock rows, which the Core now sends to a
Hermes Lite 2 (radioHardwareVersion 11). A peer that declares 18 or higher
receives Hardware as version 18. `hardware.hl2Io.cl2Enable`,
`hardware.hl2Io.cl2Freq` and `hardware.hl2Io.ext10MHz` carry
`requiresDescriptionVersion:18`, no `availability`, and the tooltips "Enable
frequency output on CL2", "Output frequency on CL2 output" and "Enable
external 10 MHz input on CL1". The frequency row becomes `kind:"decimal"`,
1..200 MHz, `step` 0.1, `decimals` 3, as the desktop's box holds three
decimal places; its value is the stored text, such as "116" or "24.576".
Labels, bindings, the other ranges, defaults and the gate are unchanged. The
frequency row also carries one closed dependency,
`"enabledWhen":{"radioSetting":"hl2/cl2Enable","oneOf":[true]}`: the row is
enabled only while the row of this description bound to `hl2/cl2Enable`
holds on, as the desktop disables the frequency box while Enable CL2 is off.
Hardware accepts `enabledWhen` only on that exact row. A peer declaring
V16 or V17 receives Hardware at version 16 or 17 with the three version 16
rows, closed with the old reason, in place of the version 18 rows; the six other rows
are unchanged. Its frequency row stays `kind:"integer"` and closed, so it
can show a value such as "24.576" set from the desktop or a version 18
peer, but never write one. No new wire field or verb is defined.

Version 19 describes DSP > CFC's per-band editor, the desktop's Configure
CFC bands… button (R-R3-49). DSP's root is version 19; the one new row is
the last control of the CFC page's CFC section, after Post-EQ Gain, and is
accepted only as the exact object in `resources/setup/dsp.json`
(`validateDspV19Control`):

- `id` `dsp.cfc.bands`, `label` "Configure CFC bands…", `kind` `table`,
  `applies` `live`, `requiresDescriptionVersion` 19.
- `binding` `{"cfcProfile":{"object":"transmit","name":"cfcProfile","command":"cfc.setProfile"}}`:
  the value is `transmit`'s read-only `cfcProfile` (the link document's
  "The CFC band editor"), and a change is sent whole with `cfc.setProfile`
  and that value's `revision` as `expectedRevision`. The Core refuses a
  stale revision, and the phone shows the refusal reason and the new value.
- `gate` `{"capability":"transmitSettingsVersion","min":15}`, with no
  off-air rule: Thetis's CFC dialog applies a change on the air
  (frmCFCConfig.cs:333-392 [v2.10.3.15] has no MOX check), and the Core
  takes `cfc.setProfile` on the air from a session permitted to change
  transmit settings (`transmitSettingsVersion` 13 and later), as the
  desktop does. Below `transmitSettingsVersion` 15 the row shows disabled
  with the Core's reason.
- `bandCounts` `[5,10,18]`: the band counts the editor offers (the
  desktop's 5-band, 10-band and 18-band). Changing the count respreads the
  bands evenly between Low and High, as the desktop's editor does.
- `minSpanHz` 1000: High must be at least 1000 Hz above Low.
- `fields`, the profile's own values, in order: `minHz` "Low" and `maxHz`
  "High" (decimal, 0..20000, step 1, no decimals, " Hz"), `parametric` "Use
  Q Factors" (toggle), `precompDb` "Pre-Comp" (decimal, 0..16, step 0.1,
  one decimal, " dB") and `postEqGainDb` "Post-EQ" (decimal, -24..24,
  step 0.1, one decimal, " dB").
- `columns`, one row per band: `frequencyHz` "Freq" (0..20000, step 1, no
  decimals, " Hz"), `compressionDb` "Comp" (0..16, step 0.1, one decimal,
  " dB"), `compressionQ` "Comp Q" (0.2..20, step 0.01, two decimals),
  `postEqGainDb` "Gain" (-24..24, step 0.1, one decimal, " dB") and
  `postEqQ` "EQ Q" (0.2..20, step 0.01, two decimals). The Q columns apply
  only while Use Q Factors is on.

The ranges are `CfcProfile`'s, each from Thetis frmCFCConfig.Designer.cs
[v2.10.3.15] (`udCFC_low` and `udCFC_high`, `nudCFC_precomp`,
`nudCFC_posteqgain`, `nudCFC_f`, `nudCFC_c`, `nudCFC_cq`, `nudCFC_gain` and
`nudCFC_q`; see `CfcProfile.h`), and so are the steps and decimals, with the
1000 Hz spread from frmCFCConfig.cs:120-140.
The first band sits on Low and the last on High; the Core keeps them there.

The CFC section's other rows and AGC/ALC's TX Leveler and TX ALC rows
(`dsp.agcAlc.txLevelerOn`, `txLevelerMaxGain`, `txLevelerDecay`,
`txAlcMaxGain`, `txAlcDecay`; `dsp.cfc.phaseRotatorEnabled`,
`phaseRotatorFreqHz`, `phaseRotatorStages`, `phaseReverseEnabled`,
`cfcEnabled`, `cfcPostEqEnabled`, `cfcPrecompDb`, `cfcPostEqGainDb`,
`cessbOn`) lose their off-air rule at every description version, for the
same reason: their gate is `{"capability":"transmitSettingsVersion","min":4}`
and the Core refuses one that carries `offAir`. The description is always
served by the Core that applies the edit. With a radio that Core reports
transmitSettingsVersion 15 and takes these settings on the air; without one it
reports 0, which fails the `min` and disables the rows anyway.

The off-air sweep applies the same rule to the other transmit rows the Core
has taken on the air since transmitSettingsVersion 13. These lose their
off-air rule at every description version, and the Core refuses one that
carries `offAir`: Audio's TX Input rows (Mic Gain and the twelve Radio Mic
rows across the three families), TX Profile's profile choice, Save, Delete,
Filter Low, Filter High and AM Carrier Level; DSP > Options' Filter Size TX
and Filter Type TX (Phone, FM, Digital); PA Gain's Bypass ANAN PA Settings;
Transmit > Power's thirteen rows (drive, ATT on TX, tune power, SWR
protection and External TX Inhibit); and Transmit > DEXP/VOX's sixteen rows.
Thetis disables none of these while MOX is on: its MOX setter
(setup.cs:5132-5161 [v2.10.3.15]) greys only the VAC controls and
`grpDSPBufferSize`. So the DSP > Options Buffer Size TX rows keep their
`offAir` rule (setup.cs:5159 [v2.10.3.15], `grpDSPBufferSize.Enabled =
!mox`), and the Core refuses one without it. The PA Watt Meter points never
carried the rule. No description version changes: a peer at any version reads
the rows without the lock, and the Core still applies its own on-air checks.

DSP's category coverage becomes "partial: the NR3 and NNR model files are
not described" and the CFC page carries no coverage (`coverageV19`). A V15
to V18 peer receives DSP at version 15 without the row, with version 15's
coverage text. A new transmit property (`cfcProfile`, ordinal 88), a new
verb (`cfc.setProfile`) and `transmitSettingsVersion` 15 are defined in the
link document; this version adds no other wire field.

Version 20 publishes that on-the-air lock per control (R-R3-49, R-IOS-18).
The table's gate becomes `{"capability":"paProfileVersion","min":1}` (no
`offAir`; its `requiresDescriptionVersion` stays 14), and the profile
choice and four buttons keep theirs. While the radio is on the air the
Core adds the existing `availability` object:

- the profile choice, New, Copy, Delete and Reset Defaults each carry
  `{"enabled":false,"reason":"Can't change while transmitting."}`;
- every table row (`{band, label}`) carries the same, except the row of
  the band the radio transmits on. For the device that holds transmit
  that row has no `availability` (it is live); every other peer gets
  `{"enabled":false,"reason":"Only the device that is transmitting can
  change this."}`. With no PA band for the transmit frequency every row
  is locked.

An absent `availability` means enabled, so off the air the rows carry
none. A renderer disables what `availability` disables and shows its
`reason`; it needs neither the transmit band nor the holder. Going on or
off the air, and a change of the device holding transmit while on the
air, advance `revision` and resend `pa` alone (`revision` and `pa` share
their own notify). V19 and older peers keep the exact closed version-14
rows, the table's `offAir` gate included.

Version 21 greys out CAT & Network > TCI Server's Forget row
(`catNetwork.tciServer.core.forgetRx2VfoBOnDisconnect`) while Duplicate
(`copyRx2VfobToVfoa`) is off, as the desktop does and as Thetis does
(Forget acts only while Duplicate is on). The row gains one closed
dependency, exactly
`"enabledWhen":{"property":{"object":"stationTci","name":"copyRx2VfobToVfoa"},"oneOf":[true]}`.
The renderer reads that value from the `stationTci` mirror object it already
reads for the row's own `valueProperty`; a missing or non-boolean value
disables Forget. The row keeps its `stationTciSettingsVersion` gate and has
no `requiresDescriptionVersion`: a V20 or older peer receives the same row
without `enabledWhen` (always enabled, as before), and CAT & Network at
version 15. The Core rejects `enabledWhen` on any other CAT & Network row
and any altered dependency. This version adds no mirror field, ordinal or
verb.

Version 22 locks DSP > Options' four RX Buffer Size rows (Phone, FM, CW,
Digital) while the radio is on the air. Thetis greys the whole buffer group
under MOX (setup.cs:5159 [v2.10.3.15], `grpDSPBufferSize.Enabled = !mox`).
Version 21 is CAT & Network's TCI Forget row (above). The rows keep their gate
(no `offAir`, since a peer that did not declare remote transmit receives no
`txState`); instead, while on the air, the Core adds the existing
`availability` object `{"enabled":false,"reason":"Can't change while
transmitting."}` to each of the four rows, and off the air they carry none.
Keying and unkeying advance `revision` and resend `dsp` (DSP has its own
notify). The Core refuses a write or removal of any of the four keys on the
air with the same reason, at every version. A V21 or older peer receives DSP
at its earlier version without the `availability` objects. The desktop Setup
greys the four RX rows and the three TX Buffer Size rows on the air with the
same reason.

Version 23 adds one row to Hardware Config > Calibration, in a new first
section titled `Level Cal`: `hardware.calibration.rx1_6mLna`, the desktop
Calibration tab's "Rx1 6m LNA:" box. It is closed as the version 13 rows are:
`kind:"decimal"`, `binding:{"radioSetting":"cal/rx1_6mLna"}`, `applies:"live"`,
gate `radioHardwareVersion` 1, `requiresDescriptionVersion:23`, `min` 0, `max`
25, `step` 1, `decimals` 1, `unit` `dB`, `default` 13, and an empty tooltip,
as the desktop box has none (Thetis ud6mLNAGainOffset,
setup.designer.cs:12089-12116 [v2.10.3.15]). The row is on every board, as
the tab is. A write goes through the per-radio settings path the desktop
uses: the Core refuses a value outside 0 to 25 dB with "Choose a 6 m LNA
offset from 0 to 25 dB." and hands back its own, takes it from a
receive-only Core's peers (it is a receive calibration), and applies it to
its receive calibration through the "cal" reload, once the radio is back on
receive if it is on the air. A V18 to V22 peer receives Hardware at version
18 without the row or its section. This version adds no mirror field,
ordinal or verb; the Core caps a declaration at 23.

Version 24 changes Audio > TX Input's radio microphone groups (the radio
codec lane). Two rows are closed as the version 23 row is:

- `audio.txInput.hermesLineInGain` moves in the 1.5 dB steps of Thetis's
  udLineInBoost with one decimal (setup.designer.cs:47006-47034
  [v2.10.3.15]): `kind:"decimal"`, `min` -34.5, `max` 12, `step` 1.5,
  `decimals` 1, `unit` `dB`, bound to transmit's `lineInBoost`, gate
  `transmitSettingsVersion` 3 with `transmit`, `requiresDescriptionVersion:24`.
  Before, the row said whole decibels from -34, which are not the radio's
  steps. A V15 to V23 peer keeps the version 15 row: `min` -34, `step` 1, no
  `decimals`.
- `audio.txInput.saturnMicTipRing`, "Mic Tip-Ring (Tip is Mic)", a toggle
  second in the Radio Mic (Saturn G2) section, bound to transmit's
  `micTipRing` as the Orion group's row is, with the same gate. Thetis
  enables the ORION Tip / Ring panel on the G2 and G2-1K (setup.cs:20292,
  20343). A V15 to V23 peer does not receive it.

Two changes follow the connected radio and reach every peer from version
15, which already reads `availability` and the section's rows:

- On the Red Pitaya the four Radio Mic (Orion-MkII) rows carry
  `availability:{enabled:false, reason:"These mic settings do not apply to
  the Red Pitaya."}`: Thetis greys out the ORION mic panel there
  (setup.cs:20440-20445, //DH1KLM). Elsewhere they carry none.
- On the Hermes Lite 2 the Radio Mic (Hermes / Atlas) section is sent,
  titled `Radio Mic (Hermes Lite 2)`, each of its three rows with the
  tooltip "Needs the Hermes Lite 2 audio add-on board. A stock Hermes Lite
  2 sends no mic audio." The HL2 takes those settings through its AK4951
  audio add-on board, which its gateware cannot report; mi0bot leaves them
  open on every model. The HL2 receive-only kit has no such section.

A V15 to V23 peer receives Audio at version 15. This version adds no mirror
field, ordinal or verb; the Core caps a declaration at 24.

V4 adds `default` metadata to these exact Display and Appearance controls.
Display toggles use JSON booleans; its numeric controls use JSON numbers,
with choice defaults as integer ordinals and FFT option defaults as their
actual integer values. Appearance color defaults are eight-digit
`#RRGGBBAA` strings. Display's Hz/bin `kind:"decimal"` has `decimals:2` in
V4. For V1–V3 peers, the Core strips `default` from all Display and Appearance
controls and strips `decimals` from `kind:"decimal"`; it retains the controls,
their older bindings and gates, and all existing `kind:"readout"` decimals in
other categories. This is a closed extension of those two published
categories, not permission to add arbitrary default or precision fields.

The two FFT-size controls are the only V4 `kind: "slider"` controls with an
`options` array. Each option is exactly `{ "value": 4096 * 2^index,
"label": "<that decimal value>" }` for indices 0 through 6, in ascending
order. Their `default` is the stored FFT size (RX 4096; TX 32768), not a
slider index. They have no `min`, `max`, or `step`. A view selects an option by
index and writes its decimal `value` through the current settings proxy.
Malformed, reordered, duplicate, unexpected, or empty options are rejected by
Core validation. V1–V3 projections omit both FFT-size controls.

TX Panadapter Normalize is also V4-only. Its one closed dependency is
`"enabledWhen":{"setting":"DisplayTxPanDetector","oneOf":["2","3","4"]}`.
The renderer reads that detector value from the same current-session settings
proxy; missing, malformed, stale, or other values disable Normalize. A change
to Average, Sample, or RMS enables it, and a change back to Peak or Rosenfell
disables it. Core rejects any altered or extended dependency envelope.
Normalize is stored with the existing exact `True`/`False` setting encoding.
V1–V3 omit it. The other Display controls retain their existing numeric ranges
or ordinal string choices. The category and its pages remain partial;
local colors, palettes, per-band tables, derived readouts, and actions are not
described by the V4 slice. V9 adds the eight renderer rows described above.

The phone uses a current-session settings snapshot and the current canonical
radio MAC. Re-validate sends only that MAC to `station.validateSettings`;
Forget sends only that MAC to `station.forgetSettings`. Replies must match
the request ID, session and MAC. `SettingsHygieneWire` accepts exactly two
typed values (`mac`, `issuesJson`), up to 32 issues and 64 KiB of compact
JSON with bounded fields. Render severity, summary and detail as plain text;
missing, malformed, stale or refused replies are unavailable, never an empty
healthy list. Revalidation is coalesced or serialized. Forget requires a
paired-device key, is disabled while on air or busy, and starts with a
default-cancel confirmation. After confirmation the client rechecks its
session, MAC, pairing and on-air state; the Core checks authority again on
dispatch. `fixActionId` is diagnostic text and never auto-executed. Repair sends
only that MAC to `station.repairSettings` under Forget's rules (paired-device
key, off air, default-cancel confirmation, rechecked after confirmation) and
is offered only at `settingsHygieneVersion` 2. The
panel does not describe Export / Import or Logs.

On the ANAN-G2E only, the partial `PA Gain` page also describes the existing
`transmit.paSettingsBypass` Boolean toggle. The Core projects that page away
unless its board capabilities include both an integrated PA and
`showsBypassPaSettingsUi`, and the SKU is not RX-only. Its gate is
`transmitSettingsVersion:6`, with no `transmit:true` gate and no `offAir`:
the Core permits negotiated transmit *settings* on a receive-only station,
and takes this one on the air (see the off-air sweep below). The Core still applies its own property-write
authority and on-air checks. This one toggle does not describe the PA profile
grid, calibration, or auto-calibration sweep.

`gate.offAir: true` requires a live txState with keyed, tuning, twoTone and
txEnding all false. Missing or stale state disables the control; a renderer
that cannot evaluate a gate disables that control rather than ignoring it.
This is independent of holding transmit: another idle holder can still trigger
the Core's existing shared confirmation. The Core rechecks the actual on-air
state when applying an edit and when proceeding with a confirmation.

For a command-backed control, `binding.command` contains `verb`, `arguments`
and optionally `valueProperty`. `valueProperty` names a mirrored property
that supplies the current control value; it is required when the command
does not have a writable property of its own. Every argument is either a
primitive literal or one declarative source object:

```json
{
  "binding": {"command": {
    "verb": "setStationTciOptions",
    "valueProperty": {"object": "stationTci", "name": "emulateExpertSdr3"},
    "arguments": {
      "emulateExpertSdr3": {"$controlValue": true},
      "emulateSunSdr2Pro": {"$property": {"object": "stationTci", "name": "emulateSunSdr2Pro"}},
      "cwluBecomesCw": {"$property": {"object": "stationTci", "name": "cwluBecomesCw"}},
      "sendInitialState": {"$property": {"object": "stationTci", "name": "sendInitialState"}}
    }
  }}
}
```

`{"$controlValue":true}` supplies the edited value with the control's
declared type. `{"$property":{"object":"…","name":"…"}}` supplies
the named mirrored property's current value. A source object has exactly
one marker; it cannot contain an expression or a fallback. The renderer
resolves every property from the same live session and epoch immediately
before dispatch. It checks the named capability and argument types. If a
reference is missing, stale, or has the wrong type, it disables the control
or refuses the edit with a plain reason; it never reuses an old value. The
station's existing command remains atomic. The Core validates the static
description shape, while each client resolves the runtime references.

For a per-receiver property, `object: "slice:active"` is the plan's dynamic
alias. The client resolves it to its currently selected, owned slice in the
current session and epoch at the start of an interaction, then writes that
concrete `slice:<id>` mirror object. If selection changes during a gesture,
the client cancels the pending edit instead of retargeting it. A missing,
retired, or no-longer-owned selected slice disables the control and refuses
the edit; there is no slice-zero or station-global active-slice fallback. The
Core continues to enforce ownership on every inbound write. The same alias
may be used in a command argument's `$property` reference, with the same
session and selection checks.

The Thetis-derived text and ranges in JSON are GPL material. Their upstream
header is preserved in `resources/setup/HEADERS.md`, with each file listed in
`docs/attribution/THETIS-PROVENANCE.md`.
