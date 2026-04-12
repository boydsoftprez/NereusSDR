# Changelog

## [Unreleased]

### Phase 3G-8 — RX1 Display Parity (complete)

**Status:** Complete. Branch `feature/phase3g8-rx1-display-parity`
off `feature/phase3g7-polish`. 9 GPG-signed code commits plus
three doc-amend prep commits. Brings the Display → Spectrum
Defaults / Waterfall Defaults / Grid & Scales pages from "every
control disabled with NYI tooltip" to feature parity with Thetis
for RX1.

See [`docs/architecture/phase3g8-rx1-display-parity-plan.md`](docs/architecture/phase3g8-rx1-display-parity-plan.md)
for the design (plan §13 open questions resolved in commit
`0308b1b`, §5.3 architectural correction in `b8045cc`).

**What landed:**

- **Commit 1** — `ColorSwatchButton` reusable widget. Replaces
  the dead `makeColorSwatch` placeholder with a real
  `QPushButton + QColorDialog` pair. Used by 9 call sites across
  this phase and available to future TX Display / skin editor
  work.
- **Commit 2** — per-band grid storage on `PanadapterModel`.
  New `Band` enum (14 bands: 160m–6m + GEN + WWV + XVTR), new
  `BandGridSettings { dbMax, dbMin }` struct, per-band hash, 28
  persistence keys, `bandChanged` signal, and `setCenterFrequency`
  auto-derive. `BandButtonItem` expanded 12 → 14 buttons.
  Initialised to Thetis uniform -40 / -140 per plan §13 Q4.
- **Commits 3–5** — `SpectrumWidget` + `FFTEngine` renderer
  additions: averaging mode (None/Weighted/Log/TimeWindow),
  peak hold + decay, trace line width, fill + alpha, gradient,
  cal offset, waterfall AGC, reverse scroll, opacity, update
  period rate limiting, waterfall averaging, use-spectrum-min/max,
  filter/zero-line overlays, timestamp overlay, 3 new colour
  schemes (LinLog / LinRad / Custom, total now 7), configurable
  grid / grid-fine / h-grid / text / zero-line / band-edge
  colours, 5-mode frequency label alignment, FPS overlay. GPU
  line width and gradient shader wire-up deferred to a future
  polish pass.
- **Commits 6–8** — wire each Display setup page to the new
  state:
  - Spectrum Defaults: 17 controls including FFT size / window
    (routed to FFTEngine), FPS, averaging + time + decimation,
    fill + alpha, line width, gradient, cal offset, peak hold
    + delay, 3 colour pickers, thread priority.
  - Waterfall Defaults: 17 controls including high / low
    thresholds, AGC, low colour, use-spectrum-min/max, update
    period, reverse scroll, opacity, 7-scheme colour combo,
    WF averaging, 4 filter / zero-line overlay toggles,
    timestamp position + mode.
  - Grid & Scales: 13 controls including show grid, live
    per-band dB Max / Min with "Editing band: N" label that
    updates on PanadapterModel::bandChanged, global dB Step,
    5-mode frequency label align, show zero line, show FPS,
    6 colour pickers (grid / grid-fine / h-grid / text /
    zero-line / band-edge).
- **Commit 9** — this CHANGELOG entry + verification matrix
  checklist at
  [`docs/architecture/phase3g8-verification/README.md`](docs/architecture/phase3g8-verification/README.md).

**Architectural additions:**

- `RadioModel::spectrumWidget()` / `fftEngine()` non-owning
  view hooks, wired by `MainWindow` during construction, so
  setup pages can reach the renderer without depending on
  `MainWindow` directly.
- `src/models/Band.h` — first-class 14-band enum with label,
  key-name, frequency lookup (IARU Region 2), and UI-index
  mapping.

**Authorized divergences from Thetis (plan §10):**

- New per-band grid slots initialise to Thetis uniform
  -40 / -140 rather than NereusSDR's existing -20 / -160,
  per user decision 2026-04-12. Existing users see the grid
  shift on first launch after upgrade.
- Source-first protocol (`CLAUDE.md`) stays as written. This
  phase is a one-off exception, not a precedent.

**Known deferrals (tracked for future phases):**

- GPU path line width and gradient shader wiring. QPainter
  fallback path fully implemented.
- FFT decimation (S16) — UI is scaffolded; FFTEngine has no
  decimation setter yet.
- W12 / W14 TX filter / zero-line overlays — renderer call
  path in place, activates once the TX state model provides
  a TX VFO/filter pair (post-3I-1).
- Data Line Color / Data Fill Color share `m_fillColor` — UX
  polish to split these is deferred until verification
  screenshots show whether users actually need them separate.
- W10 Waterfall Low Color — persisted; runtime gradient
  rebuild waits for the Custom-scheme AppSettings parser.

### Phase 3G-7 — Polish (complete)

**Status:** Complete. Branch `feature/phase3g7-polish` off
`feature/phase3g6-oneshot`. 4 GPG-signed commits, all polish
landed in a tighter scope than the original handoff proposed.
See [`docs/architecture/phase3g7-polish-handoff.md`](docs/architecture/phase3g7-polish-handoff.md).

**Items shipped:**

- **B — MeterItem accessor gap fills** (`25a7819`). Five
  subclasses (TextOverlayItem, RotatorItem, FilterDisplayItem,
  ClockItem, VfoDisplayItem) had setters with no matching
  getters, so each item's property editor `setItem()`
  populated only a fraction of its widgets. Added 42 trivial
  inline getters and wired each editor to populate from them.
  Investigation showed the handoff's other "phantom field"
  candidates (`SignalText::showMarker`, `LEDItem::condition`,
  `HistoryGraph::keepFor`/`fadeRx`, `MagicEye::darkMode`/`eyeScale`,
  `DialItem::vfoClickBehavior`) don't exist on either the items
  or the editors today — they're future feature ports, not
  gap fills, and have been moved to deferred-features list.
- **A — MMIO binding clone-path side-channel** (`8774b7c`).
  The 3G-6 runtime bug — meter items losing their MMIO binding
  on dialog Apply — turned out to be a 4-site clone leak in
  `ContainerSettingsDialog.cpp`, not a 30-subclass serialize
  sweep. `populateItemList`, `applyToContainer`,
  `takeSnapshot`/`revertFromSnapshot`, and the preset clone
  loop now copy `(mmioGuid, mmioVariable)` directly around the
  text round-trip via a parallel
  `QVector<QPair<QUuid, QString>>` snapshot. ~50 LOC in one
  file, no subclass changes. Disk persistence remains
  deliberately deferred (block 5 design).
- **C — `NeedleItemEditor` `QGroupBox` grouping** (`41c7031`).
  The 17 needle-specific fields plus the calibration table now
  live in 5 group boxes (Needle / Geometry / History / Power /
  Calibration) instead of stacking flat under header labels.
  Member pointers and connect lambdas unchanged; layout-only.
- **F — MMIO smoke test** — *deferred*. User skipped at scope
  decision; the runtime fix in item A is the proof MMIO works
  end-to-end, smoke test doc tracked as separate future work.
- **D — Editor widget width sweep** — *deferred to future*.
  Cosmetic, block 4b's scroll area solved the load-bearing
  reachability problem.
- **E — `ButtonBox` per-button `ButtonState` sub-editor** —
  *deferred to future*. Architectural addition; no current
  workflow blocked on it.

**Investigation note:** Both items A and B turned out to be
narrower than the handoff proposed. Item B's "phantom fields"
mostly didn't exist on either side; item A's bug was a 4-site
clone leak in one dialog file rather than a 30-subclass
serialize sweep. The handoff's pessimistic scope estimate cost
zero session time to disprove and saved a lot of mechanical
edits.

### Phase 3G-6 (One-Shot) — Container Settings Dialog + Full Thetis Parity + MMIO

**Status:** Complete. Branch `feature/phase3g6-oneshot`, PR #2.
Plan: [`docs/architecture/phase3g6a-plan.md`](docs/architecture/phase3g6a-plan.md).

40 GPG-signed commits across 7 execution blocks landed the
biggest single phase in NereusSDR's history, covering the
complete user-facing surface for the meter system.

#### Block 1 — rendering plumbing + Thetis filter rule
- Meter PNGs registered as Qt resources (`:/meters/ananMM.png`,
  CrossNeedle stopgap placeholders).
- `onlyWhenRx` / `onlyWhenTx` / `displayGroup` moved to
  `MeterItem` base, default 0 per Thetis sentinel.
- Thetis filter rule (MeterManager.cs:31366-31368) ported into
  `MeterWidget::shouldRender()`, wired into all 5 paint paths.
- ANANMM `NeedleScalePwrItem` pinned to power/SWR group to fix
  RX label overlap.

#### Block 2 — container surface + NeedleItem calibration
- `m_titleBarVisible`, `m_minimised`, `m_highlighted` on
  `ContainerWidget` (the latter painting a 2px accent outline).
- `ContainerManager::duplicateContainer`, `containerTitleChanged`
  signal chain.
- `FloatingContainer` collapses on `minimisedChanged(true)`.
- `NeedleItem` rewritten for calibration-driven painting:
  serializes the full factory state (16-point calibration map,
  needle color, geometry, smoothing); paint routines early-return
  when calibration is non-empty so the background ImageItem
  shows through; emitVertices honors `m_radiusRatio` /
  `m_lengthFactor` and reads needle color from `m_needleColor`.

#### Block 3 — `ContainerSettingsDialog` rewrite
- Live preview panel removed.
- Thetis 3-column layout: Available / In-use / Properties.
- Container-switch dropdown populated from
  `ContainerManager::allContainers()` (auto-commit on switch
  landed in block 7).
- Snapshot + revert on Cancel.
- Categorized Available list (RX / TX / Special).
- Two-row container property bar with all 8 plan controls plus
  Duplicate / Delete buttons.
- Footer: Save / Load / MMIO Variables (last became real in
  block 5).

#### Block 4 — per-item property editors (parallel agents)
- `BaseItemEditor` shared base class with 9 common form rows.
- 30 subclass editors built in parallel by 4 subagents:
  primitives (5), needles (3 — `NeedleItemEditor` includes a
  fully-editable calibration `QTableWidget`), text/signal/history
  (6), interactive + button-box (17). 62 files, ~155 fields,
  zero manual fixups.
- Dialog factory wires the right editor per item type tag and
  pushes property changes live via
  `propertyChanged → applyToContainer`.
- Block 4b polish: editor pages wrapped in `QScrollArea`,
  dialog grown to 1100×750, splitter rebalanced, minimum field
  widths in `BaseItemEditor` helpers.

#### Block 5 — MMIO subsystem
- Started with a docs commit (`f090dcf`) that rewrote section
  6 of the plan after a Thetis Explore agent found the
  original draft's per-variable parse-rule taxonomy was
  imaginary. Real Thetis is endpoint-centric with format-driven
  (JSON / XML / RAW) discovery.
- Core scaffold: `ExternalVariableEngine` singleton on its own
  worker `QThread`, `MmioEndpoint` value-object + per-endpoint
  `QHash<QString, QVariant>` cache behind a `QReadWriteLock`,
  `FormatParser` free functions, `ITransportWorker` abstract
  base, `lcMmio` logging category.
- Four parallel-built transport workers:
  `UdpEndpointWorker`, `TcpListenerEndpointWorker`,
  `TcpClientEndpointWorker`, `SerialEndpointWorker` (the last
  gated on `HAVE_SERIALPORT`). All Qt event-driven instead of
  Thetis's 50ms polling loops.
- XML persistence under `AppSettings/MmioEndpoints/<guid>/*`
  instead of Thetis's opaque Base64 binary blob.
- `MmioEndpointsDialog` 3-column manager (endpoint list +
  property form + discovered-variables tree).
- `MmioVariablePickerPopup` tree-of-endpoints picker for the
  per-item editor's new "Variable…" button.
- `MeterItem::m_mmioGuid` / `m_mmioVariable` in-memory binding
  + `MeterPoller` branch that reads bound values and pushes
  them into items at 10 fps.
- App startup wires `ExternalVariableEngine::instance().init()`.

#### Block 6 — `Containers → Edit Container` submenu
- Replaces the static "Container Settings…" action with a
  dynamic submenu populated from
  `ContainerManager::allContainers()`, alphabetized by notes.
- Rebuild on `containerAdded` / `containerRemoved` /
  `containerTitleChanged`.
- `Reset Default Layout` finally functional (was disabled NYI).

#### Block 7 — polish + docs
- Dead-code cleanup: 720 lines of legacy
  `buildXItemEditor`/`buildCommonPropsPage`/etc. methods
  removed from `ContainerSettingsDialog.cpp`.
- `lcMmio` registered in `LogManager` runtime metadata.
- Copy / Paste item settings (plan commit 35).
- Container-dropdown auto-commit on switch (plan commit 13's
  deferred half).
- This CHANGELOG entry + phase-table flip + debug-handoff
  marked Resolved.

### Known limitations after 3G-7

Resolved in 3G-7:
- ~~MMIO bindings dropped on dialog Apply~~ — fixed via the
  4-site clone-path side-channel in `ContainerSettingsDialog.cpp`.
- ~~`NeedleItemEditor` flat field stack~~ — now wrapped in 5
  `QGroupBox`es.
- ~~Five item subclasses missing accessor getters~~ —
  TextOverlay, Rotator, FilterDisplay, Clock, VfoDisplay
  populate fully on dialog open.

Still outstanding:
- **MMIO binding disk persistence.** Bindings survive Apply
  and Cancel within a session, but `serialize()`/`deserialize()`
  on each item still does not include them. Save-to-file +
  reload, or app restart, drops bindings. Block 5's design
  explicitly deferred this; the path forward is the original
  handoff's "append `|guid|var` to every concrete subclass
  format" sweep.
- **MMIO end-to-end smoke test.** No external doc walking a
  user through "spin up a UDP listener, send JSON, see the
  meter move." Useful first-real-proof artifact when MMIO
  bindings get exercised in anger.
- **Editor `QLineEdit` / `QComboBox` / `QPushButton` width
  sweep.** Block 4b's `BaseItemEditor` helpers set
  `setMinimumWidth(140)` but the 30 subagent-produced editors
  hand-roll some controls that still shrink-wrap. Cosmetic.
- **`ButtonBoxItem` per-button `ButtonState` sub-editor.**
  Per-button color/style/font fields live on a `ButtonState`
  struct array; needs a per-button-index spinner UI in
  `ButtonBoxItemEditor`. Untested code path because no
  workflow currently exposes per-button customization.
- **Phantom fields not yet ported from Thetis** (would expand
  scope, not gap-fill). Each is a standalone Thetis port:
  - `LEDItem` — custom amber/red zone colors (fields exist,
    no setter); string-expression `condition` mode
  - `SignalTextItem::showMarker` — bar-style marker visibility
    toggle
  - `HistoryGraphItem` — `keepFor` (time-based retention),
    `ignoreHistory` (pause), manual axis range, `fadeRx/Tx`
  - `MagicEyeItem` — `darkMode`, `eyeScale`, `eyeBezelScale`
  - `DialItem::vfoClickBehavior` — quadrant click action remap
- **Real CrossNeedle PNG artwork** — current files are
  byte-identical copies of `ananMM.png`. Waiting on user.
- **TX wiring** from `RadioModel` / `TransmitModel` into
  `MeterWidget::setMox()` — phase 3I-1 territory.

### Added — Phase 3G-5: Interactive Meter Items

**Mouse event infrastructure:**
- MeterItem base class gains virtual `hitTest()`, `handleMousePress()`, `handleMouseRelease()`, `handleMouseMove()`, `handleWheel()` with default no-op implementations
- MeterWidget forwards mouse events to items in reverse z-order (top-first hit dispatch)

**ButtonBoxItem shared base class** (from Thetis clsButtonBox, MeterManager.cs:12307+):
- Configurable grid layout (rows/columns), rounded rectangle buttons
- 13 indicator types: Ring, Bar (L/R/B/T), Dot (L/R/B/T/TL/TR/BL/BR), TextIconColour
- Three-state colors per button (fill/hover/click), on/off state
- 100ms click highlight timer (from Thetis `setupClick` pattern)
- FadeOnRx/FadeOnTx visibility guards, visibility bitmask

**8 interactive button grid items:**
- `BandButtonItem` — 12 band buttons (160m-6m, GEN), left-click select + right-click bandstack popup (from Thetis clsBandButtonBox)
- `ModeButtonItem` — 10 mode buttons (LSB-DIGU), left-click select (from Thetis clsModeButtonBox)
- `FilterButtonItem` — 12 filter buttons (F1-F10, Var1/2), mode-dependent labels, right-click context menu (from Thetis clsFilterButtonBox)
- `AntennaButtonItem` — 10 color-coded buttons: Rx (green), Aux (orange), Tx (red), Toggle (yellow) (from Thetis clsAntennaButtonBox)
- `TuneStepButtonItem` — 7 step buttons (1Hz-1MHz) with active highlighting (from Thetis clsTunestepButtons)
- `OtherButtonItem` — 34 core control buttons + 31 macro slots with Toggle/CAT/ContainerVis modes (from Thetis clsOtherButtons)
- `VoiceRecordPlayItem` — 5 DVK buttons (REC/PLAY/STOP/NEXT/PREV) (from Thetis clsVoiceRecordPlay)
- `DiscordButtonItem` — 12 Discord Rich Presence buttons (from Thetis clsDiscordButtonBox)

**3 standalone interactive items:**
- `VfoDisplayItem` — frequency display in XX.XXX.XXX format with per-digit mouse wheel tuning, mode/filter/band labels, RX/TX indicator, split indicator (from Thetis clsVfoDisplay)
- `ClockItem` — dual Local + UTC time display with 1s auto-refresh, 24h/12h toggle (from Thetis clsClock)
- `ClickBoxItem` — invisible hit region for unit cycling overlays (from Thetis clsClickBox)

**1 data item:**
- `DataOutItem` — MMIO external data bridge with JSON/XML/RAW output formats and UDP/TCP/Serial transport (from Thetis clsDataOut)

**Signal-based interaction:** all interactive items emit Qt signals, never touch RadioModel directly. Signal chain: MeterItem → MeterWidget → ContainerWidget → MainWindow.

**ContainerWidget signal forwarding:** `wireInteractiveItem()` connects band/mode/filter/antenna/tuneStep/other/macro/voice/discord/VFO signals from MeterItems to container-level signals.

**ItemGroup extensions:** 13 new type tags in deserialize registry. 3 new presets: VFO Display, Clock, Contest (VFO + band buttons + mode buttons + clock).

### Added — Phase 3G-4: Advanced Meter Items

**12 new MeterItem types (passive/display):**
- `SpacerItem` — layout spacer with gradient fill (from Thetis clsSpacerItem)
- `FadeCoverItem` — RX/TX transition overlay (from Thetis clsFadeCover)
- `LEDItem` — LED indicator: 3 shapes (Square/Round/Triangle), 2 styles (Flat/ThreeD), blink/pulsate (from Thetis clsLed)
- `HistoryGraphItem` — scrolling time-series with dual-axis ring buffer, auto-scaling (from Thetis clsHistoryItem)
- `MagicEyeItem` — vacuum tube magic eye with green phosphor arc (from Thetis clsMagicEyeItem)
- `NeedleScalePwrItem` — non-linear power scale labels for needle arcs (from Thetis clsNeedleScalePwrItem)
- `SignalTextItem` — S-units/dBm/uV signal display with peak hold and bar styles (from Thetis clsSignalText)
- `DialItem` — circular dial with VFO/ACCEL/LOCK quadrant buttons (from Thetis clsDialDisplay)
- `TextOverlayItem` — two-line text with %VARIABLE% substitution parser (from Thetis clsTextOverlay)
- `WebImageItem` — async web-fetched image with periodic refresh (from Thetis clsWebImage)
- `FilterDisplayItem` — mini passband spectrum/waterfall with filter edges and notch overlays (from Thetis clsFilterItem)
- `RotatorItem` — antenna rotator compass dial with AZ/ELE/BOTH modes (from Thetis clsRotatorItem)

**Complex composite presets:**
- ANANMM 7-needle multi-meter (signal, volts, amps, power, SWR, compression, ALC) with exact Thetis calibration points
- CrossNeedle dual fwd/rev power meter with mirrored CounterClockwise geometry

**NeedleItem extensions:** scaleCalibration, needleOffset, radiusRatio, lengthFactor, direction (CW/CCW), onlyWhenRx/Tx, displayGroup, historyEnabled/Color, normaliseTo100W

**Edge meter display mode:** BarItem gains Filled/Edge style with 3-line indicator (from Thetis Edge meter)

**15+ bar preset factories:** Signal, AvgSignal, MaxBin, ADC, AGC, PBSNR, EQ, Leveler, ALC variants, CFC variants

**MeterBinding extensions:** SignalMaxBin(7), PbSnr(8), TxEq-TxCfcGain(106-112), HwVolts/Amps/Temp(200-202), RotatorAz/Ele(300-301)

### Fixed — Applet Panel Layout
- RxApplet: fix STEP row overflow (138px content in 99px column), change filter grid
  from 2×5 to 3-column layout matching AetherSDR, remove fixedWidth from RIT/XIT
  labels so they flex with panel width, add Expanding sizePolicy to RIT/XIT toggles
- TxApplet: add outer→body wrapping pattern, merge MOX/TUNE/ATU/MEM into single
  4-button row matching AetherSDR, fix value label styles and slider label colors
- PhoneCwApplet: add missing applyComboStyle() to mic combos, set Ignored sizePolicy
  on CW QSK/Iambic/FW Keyer buttons for even spacing, flex CW pitch label
- EqApplet: add outer→body wrapping, reduce scale/spacer widths for 10-band fit,
  reduce band label font to 9px, add applyComboStyle() to preset combo

### Added — Phase 3-UI: Full UI Skeleton

**Applets (12 total, 150+ control widgets):**
- `src/gui/applets/RxApplet.h/.cpp` — RX controls: mode, AGC, AF/RF gain, filter presets; Tier 1 wired to SliceModel
- `src/gui/applets/TxApplet.h/.cpp` — TX controls: mic gain, drive, tune power, compression, DEX
- `src/gui/applets/PhoneCwApplet.h/.cpp` — Voice/CW macro keys, VOX, CW speed/weight
- `src/gui/applets/EqApplet.h/.cpp` — 10-band RX/TX graphic equalizer
- `src/gui/applets/FmApplet.h/.cpp` — FM deviation, sub-tone, repeater offset
- `src/gui/applets/DigitalApplet.h/.cpp` — digital mode settings (baud, shift, encoding)
- `src/gui/applets/PureSignalApplet.h/.cpp` — PureSignal calibrate, feedback level indicator
- `src/gui/applets/DiversityApplet.h/.cpp` — diversity RX phase/gain controls
- `src/gui/applets/CwxApplet.h/.cpp` — CW message memory keyer
- `src/gui/applets/DvkApplet.h/.cpp` — digital voice keyer playback controls
- `src/gui/applets/CatApplet.h/.cpp` — CAT/rigctld port configuration
- `src/gui/applets/TunerApplet.h/.cpp` — antenna tuner control (tune, bypass, memory)

**Spectrum Overlay Panel:**
- `src/gui/SpectrumOverlayPanel.h/.cpp` — 10-button overlay panel with 5 flyout sub-panels (display, filter, noise, spots, tools); auto-close on outside click

**Menu Bar (9 menus, ~60 items):**
- File, Radio, View, DSP, Band, Mode, Containers, Tools, Help menus wired to MainWindow

**Status Bar:**
- Double-height status bar (46px): UTC clock, radio info, TX/RX indicators, signal level

**Setup Dialog:**
- `src/gui/SetupDialog.h/.cpp` — 47 pages across 10 categories (Radio, Audio, DSP, Display, CW, Digital, TX, Logging, Network, Misc); all pages have real controls

**Applet Panel:**
- `src/gui/AppletPanelWidget.h/.cpp` — fixed header (S-Meter) + scrollable applet body for Container #0

**Infrastructure:**
- `src/gui/StyleConstants.h` — shared color palette, font sizes, widget style constants
- `src/gui/widgets/HGauge.h/.cpp` — horizontal bar gauge widget
- `src/gui/widgets/ComboStyle.h/.cpp` — styled combo box shared across applets

**Container / Persistence fixes:**
- Content restored on restart (applet panel state persisted)
- Pin-on-top state persists across sessions
- Floating container position persists correctly
- Container minimum width 260px enforced in all dock modes

**S-Meter enhancements:**
- Dynamic resize with aspect-ratio scaling and full-width arc
- SMeterWidget ported directly from AetherSDR for pixel-identical fidelity

### Added — Phase 3G-3: Core Meter Groups (in progress)
- NeedleItem: arc-style S-meter needle participating in all 4 GPU render pipelines
  - P1 Background: QPainter arc segments (white S0-S9, red S9+, blue inner TX arc)
  - P2 Geometry: uniform-width needle quad + shadow + amber peak marker triangle
  - P3 OverlayStatic: tick marks and labels (1,3,5,7,9,+20,+40) via needleDir vector
  - P3 OverlayDynamic: S-unit readout (cyan) + dBm readout (light steel)
- Multi-layer MeterItem infrastructure: participatesIn() + paintForLayer() virtuals
- Exponential smoothing (SMOOTH_ALPHA=0.3 from AetherSDR) + peak hold (Medium preset)
- S-unit scaling from Thetis: S0=-127dBm, S9=-73dBm, 6dB/S-unit
- Aspect-ratio-locked meterRect() helper (2:1 matching AetherSDR sizeHint 280x140)
- TX MeterBinding constants 100-105 (Power, ReversePower, SWR, Mic, Comp, ALC)
- Preset factories: createSMeterPreset, createPowerSwrPreset, createAlcPreset, createMicPreset, createCompPreset
- ItemGroup::installInto() for positioning groups within MeterWidget sub-regions
- Default Container #0 layout: S-Meter (55%) + Power/SWR (30%) + ALC (15%)
- NEEDLE serialization format + deserializer registration
- NOTE: S-meter will be re-implemented as dedicated SMeterWidget (direct AetherSDR port)
  for pixel-identical fidelity — the MeterItem/NeedleItem approach introduces rendering
  artifacts from GPU pipeline splitting and texture-based text

### Added — Phase 3G-2: MeterWidget GPU Renderer
- MeterWidget (QRhiWidget): 3-pipeline GPU rendering engine for meters inside ContainerWidget
  - Pipeline 1 (textured quad): cached background textures and images
  - Pipeline 2 (vertex-colored geometry): animated bar fills with per-frame vertex updates
  - Pipeline 3 (QPainter overlay, split static/dynamic): tick marks, labels, value readouts
- MeterItem base class with normalized 0-1 positioning and data binding
- Concrete item types: BarItem (H/V), TextItem, ScaleItem (H/V), SolidColourItem, ImageItem
- BarItem exponential smoothing (attack 0.8 / decay 0.2, from Thetis MeterManager.cs)
- ItemGroup composite with createHBarPreset() factory (AetherSDR visual style)
- MeterPoller: QTimer-based WDSP meter polling at 100ms via RxChannel::getMeter()
- MeterBinding constants mapping to RxMeterType (SignalPeak through AgcAvg)
- Pipe-delimited item serialization compatible with ContainerWidget persistence
- Container #0 pre-populated with live horizontal signal strength bar meter
- Shaders: meter_textured.vert/.frag (Pipelines 1 & 3), meter_geometry.vert/.frag (Pipeline 2)
- lcMeter logging category

### Added — Phase 3G-1: Container Infrastructure
- ContainerWidget: dock/float/resize container shell with title bar, axis-lock (8 positions), content slot
- FloatingContainer: top-level window wrapper with pin-on-top, minimize-with-console, geometry persist
- ContainerManager: singleton lifecycle — 3 dock modes (PanelDocked/OverlayDocked/Floating)
- QSplitter-based MainWindow layout: spectrum left, Container #0 right panel
- Axis-lock repositioning: docked containers track main window resize per anchor position
- 24-field pipe-delimited serialization (Thetis-compatible + DockMode extension)
- Container + splitter state persistence to AppSettings
- Hover-reveal title bar with dock-mode-aware buttons (float/dock, axis cycle, pin, settings)
- Wider hover detection zone (40px) for reliable title bar reveal on high-DPI displays
- Frameless floating containers on macOS (no native title bar, container provides its own)
- Ctrl-snap to 10px grid during drag and resize
- Phase roadmap: AetherSDR-style AppletPanel planned for Container #0 in Phase 3G-AP

### Added — Phase 3E: VFO & Controls + CTUN Panadapter
- Floating VFO flag widget (AetherSDR pattern): frequency display, mode/filter/AGC tabs, antenna buttons
- SliceModel: DSPMode enum, AGCMode, per-mode filter defaults from Thetis, tuning step, AF/RF gain
- SmartSDR-style CTUN panadapter: independent pan center and VFO, WDSP shift offsets
- Off-screen VFO indicator with double-click to recenter
- I/Q pipeline rewired through ReceiverManager with DDC-aware routing
- Alex HPF/LPF/BPF filter registers ported from Thetis (frequency-based selection)
- Full-spectrum FFT output (all N bins, FFT-shift + mirror for correct sideband orientation)
- VFO settings persistence (frequency, mode, filter, AGC, step, gains, antennas)
- AetherSDR-style VFO control buttons and green toggle filter presets
- CTUN zoom: frequency scale bar drag and Ctrl+scroll zoom into FFT bin subsets
- Hybrid zoom: smooth bin subsetting during drag, FFT replan on mouse release
- visibleBinRange() maps display window to DDC bin indices (GPU + CPU + waterfall)
- DDC center frequency tracking (m_ddcCenterHz) for correct bin-to-frequency mapping
- Waterfall preserves existing rows across zoom changes (new rows at current scale)

### Added — Phase 3D: GPU Spectrum & Waterfall
- FFTEngine: FFTW3 float-precision on dedicated spectrum worker thread
- SpectrumWidget: QRhiWidget GPU rendering via Metal (macOS) / Vulkan / D3D12
- 3 GPU pipelines: waterfall (ring-buffer texture), spectrum (vertex-colored), overlay (QPainter→texture)
- 6 GLSL 4.40 shaders compiled to QSB via qt_add_shaders()
- 4096-point FFT, Blackman-Harris 4-term window, 30 FPS rate limiting
- VFO marker, filter passband overlay, cursor frequency readout
- Mouse interaction: scroll zoom, drag ref level, click-to-tune
- Right-click SpectrumOverlayMenu with display settings
- AetherSDR default + Thetis Enhanced/Spectran/BlackWhite color schemes

### Added — Phase 3C: macOS Build
- WDSP linux_port.h cross-platform compat (stdlib.h, string.h, fcntl.h, LPCRITICAL_SECTION)
- ARM64 flush-to-zero guard, AVRT stubs
- Fixed use-after-free crash: wisdom poll timer accessing deleted QThread

### Added — Phase 3B: WDSP Integration + Audio Pipeline
- WDSP v1.29 built as static library in third_party/wdsp/
- RxChannel wrapper: I/Q accumulation (238→1024), fexchange2(), NB1/NB2
- AudioEngine: QAudioSink 48kHz stereo Int16, 10ms timer drain
- FFTW wisdom generation with progress dialog, cached for subsequent launches
- Audio device selection and persistence via AppSettings

### Added — Phase 3A: Radio Connection (Protocol 2)
- P2RadioConnection faithfully ported from Thetis ChannelMaster/network.c
- P2 discovery (60-byte packet, byte[4]=0x02) on all network interfaces
- CmdGeneral/CmdRx/CmdTx/CmdHighPriority byte-for-byte from Thetis
- I/Q data streaming: DDC2, 1444 bytes/packet, 238 samples at 48kHz
- ConnectionPanel dialog with discovered radio list, connect/disconnect
- LogManager with runtime category toggles, AppSettings persistence
- SupportDialog with log viewer, category checkboxes, support bundle creation
- Auto-reconnect to last connected radio via saved MAC

## [0.1.0] - 2026-04-08

### Added
- Initial project scaffolding
- CMake build system with Qt6 and C++20
- Stub source files (AppSettings, RadioDiscovery, RadioConnection, WdspEngine, RadioModel, MainWindow)
- Documentation (README, CLAUDE.md, CONTRIBUTING, STYLEGUIDE)
- CI workflow (GitHub Actions)
- Phase 1 analysis document stubs
