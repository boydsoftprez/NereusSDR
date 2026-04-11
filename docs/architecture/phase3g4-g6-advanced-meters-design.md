# Phase 3G-4 / 3G-5 / 3G-6 — Advanced Meters, Interactive Items & Container Settings

Design spec for the three phases completing the NereusSDR meter/container system.

---

## Phase 3G-4: Advanced Meter Items

Four new passive `MeterItem` subclasses. All data-bound, no mouse interaction.

### HistoryGraphItem — Scrolling Time-Series Graph

**Ported from:** Thetis `clsHistoryItem` (MeterManager.cs:16149+)

- Fixed-size ring buffer (`std::vector<float>`) — not Thetis's List+time-cleanup
- Capacity options: 100 / 300 / 600 / 3000 samples (at 100ms poll = 10s / 30s / 60s / 5m)
- Default: 300 samples (30s window)
- QPainter line graph in `OverlayDynamic` layer — connects consecutive points left-to-right
- Auto-scaling Y-axis from running min/max (from Thetis `addReading()` lines 16468-16499)
- Subtle grid lines + Y-axis labels in `OverlayStatic` layer
- Colors: cyan line `#00b4d8`, dark grid `#203040`, label text `#8090a0`
- Serialization tag: `HISTORY`

**Properties:**
- `capacity` (int) — ring buffer size
- `lineColor` (QColor)
- `showGrid` (bool, default true)

### MagicEyeItem — Vacuum Tube Magic Eye

**New NereusSDR feature** — no Thetis equivalent. Inspired by vintage receiver EM84/6E5 tubes.

- Green phosphor arc that opens/closes with signal strength
- `OverlayDynamic` layer — QPainter with radial gradient + arc geometry
- Closed (narrow shadow) = strong signal (S9+), fully open = no signal (S0)
- Reuses NeedleItem's `dbmToFraction()` mapping for S-meter scale
- Green glow: `#00ff88` center fading to `#004400` edge, dark center shadow
- Purely decorative — no scale labels
- Smoothed value (same alpha as NeedleItem: `kSmoothAlpha = 0.3f`)
- Serialization tag: `MAGICEYE`

**Properties:**
- `glowColor` (QColor, default `#00ff88`)

### DialItem — Circular Dial Meter (270-degree arc)

**Inspired by:** Thetis `clsNeedleItem` (MeterManager.cs:20580+), simplified.

- 270-degree sweep arc with tick marks and labels around circumference
- Needle pointer from center to arc edge
- Attack/decay smoothing (same as BarItem: `m_attackRatio` / `m_decayRatio`)
- Multi-layer participant (like NeedleItem):
  - `OverlayStatic`: arc, tick marks, labels
  - `Geometry`: needle quad (vertex-colored)
  - `OverlayDynamic`: digital readout at center-bottom
- Configurable range — reusable for S-meter, power, SWR, etc.
- Colors: arc `#405060`, ticks `#c8d8e8`, labels `#8090a0`, needle `#ff4444`
- Serialization tag: `DIAL`

**Properties:**
- `minVal`, `maxVal` (double)
- `arcColor`, `needleColor`, `tickColor`, `labelColor` (QColor)
- `sweepDegrees` (float, default 270.0)
- `suffix` (QString, e.g. " dBm")
- `decimals` (int)
- `attackRatio`, `decayRatio` (float)

### LEDItem — Colored LED Indicator Dot

**Ported from:** Thetis `clsLed` (MeterManager.cs:19448+), flat style only.

- Four states: off (grey `#404040`), green (`#00ff88`), amber (`#ffb800`), red (`#ff4444`)
- `OverlayDynamic` layer — QPainter filled circle with 1px darker border
- Value thresholds determine state (configurable):
  - `< greenThreshold` → off
  - `>= greenThreshold` and `< amberThreshold` → green
  - `>= amberThreshold` and `< redThreshold` → amber
  - `>= redThreshold` → red
- No blink/pulsate animations (future enhancement if needed)
- Serialization tag: `LED`

**Properties:**
- `greenThreshold`, `amberThreshold`, `redThreshold` (double)
- `offColor`, `greenColor`, `amberColor`, `redColor` (QColor)

---

## Phase 3G-5: Interactive Meter Items

Five new `MeterItem` subclasses that respond to mouse events, plus MeterWidget
mouse forwarding infrastructure.

### MeterWidget Mouse Event Forwarding

MeterWidget gains `mousePressEvent`, `mouseReleaseEvent`, `mouseMoveEvent`,
`wheelEvent` overrides:

1. Iterate items in reverse z-order (top-first)
2. Call `item->hitTest(localPos, widgetW, widgetH)` — base impl checks `pixelRect().contains()`
3. Dispatch to virtual methods on the hit item:
   - `handleMousePress(QMouseEvent*, int widgetW, int widgetH)`
   - `handleMouseRelease(QMouseEvent*, int widgetW, int widgetH)`
   - `handleWheel(QWheelEvent*, int widgetW, int widgetH)`
4. Base MeterItem provides default no-op implementations
5. Return value: `true` if handled (stops propagation), `false` to continue

### BandButtonItem — Band Selection Grid

**Ported from:** Thetis `clsBandButtonBox` (MeterManager.cs:11482+)

- Grid of band buttons: 160m, 80m, 60m, 40m, 30m, 20m, 17m, 15m, 12m, 10m, 6m, GEN
- `OverlayDynamic` layer — QPainter rounded rects with text
- Active band highlighted (tracks SliceModel via value binding)
- Configurable column count (default 4), rows auto-calculated
- Colors: base `#1a2a3a`, active `#0070c0`/`#ffffff`, hover `#204060`, border `#205070`

**Mouse interaction:**
- Left-click → emits `bandClicked(int bandIndex)` signal
- Right-click → emits `bandStackRequested(int bandIndex)` signal
  (Thetis `PopupBandstack`, MeterManager.cs:11896)

**Properties:**
- `columns` (int, default 4)
- `visibleBands` (bitmask or list — which bands to show)

### ModeButtonItem — Mode Selection Buttons

**Ported from:** Thetis `clsModeButtonBox` (MeterManager.cs:9951+)

- Buttons: LSB, USB, DSB, CWL, CWU, FM, AM, SAM, DIGL, DIGU
- Same visual pattern as BandButtonItem (rounded rect grid)
- Active mode highlighted via value binding

**Mouse interaction:**
- Left-click → emits `modeClicked(int modeIndex)` signal
- No right-click action (matches Thetis)

**Properties:**
- `columns` (int, default 5)

### FilterButtonItem — Filter Preset Buttons

**Ported from:** Thetis `clsFilterButtonBox` (MeterManager.cs:7674+)

- Buttons: Filter 1-10 + Var1, Var2
- Active filter highlighted via value binding
- Filter labels are mode-dependent (set externally when mode changes):
  e.g., CW shows "200", "500"; SSB shows "2.4k", "2.7k"

**Mouse interaction:**
- Left-click → emits `filterClicked(int filterIndex)` signal
- Right-click → emits `filterContextRequested(int filterIndex)` signal
  (Thetis `PopupFilterContextMenu`, MeterManager.cs:7917)

**Properties:**
- `columns` (int, default 4)
- `filterLabels` (QStringList — set by mode change, not serialized)

### VfoDisplayItem — Frequency Display with Wheel-to-Tune

**Ported from:** Thetis `clsVfoDisplay` (MeterManager.cs:12881+)

- Displays frequency in `XX.XXX.XXX` MHz.kHz.Hz format with digit grouping
- `OverlayDynamic` layer — updates every poll cycle
- Below frequency: mode label, filter label, band label (from Thetis VFO render states)
- Each digit is a hot zone for mouse wheel tuning (Thetis pattern)

**Mouse interaction:**
- Mouse wheel over digit → emits `frequencyChangeRequested(int64_t deltaHz)` signal
  (delta = digit place value × wheel direction)
- Right-click on band text → emits `bandStackRequested(int bandIndex)`
  (Thetis MeterManager.cs:13273)
- Right-click on filter text → emits `filterContextRequested(int filterIndex)`
  (Thetis MeterManager.cs:13287)

**Properties:**
- `vfoSelect` (enum: VfoA, VfoB)
- Colors: frequency `#c8d8e8`, frequency small digits `#8090a0`, mode `#00b4d8`,
  band `#8090a0`, RX indicator `#00ff88`, TX indicator `#ff4444`

### ClockItem — UTC/Local Time Display

**Ported from:** Thetis `clsClock` (MeterManager.cs:14075+)

- Dual display: Local time (left, 48% width) + UTC (right, 48% width)
- Time format: `HH:mm:ss` (24h) or `hh:mm:ss AP` (12h)
- Date line: `ddd d MMM yyyy` format
- `OverlayDynamic` layer with internal `QTimer` (1s interval, connected to `update()`)
- No mouse interaction (matches Thetis)
- Colors: time `#c8d8e8`, date `#8090a0`, type title `#708090`

**Properties:**
- `show24Hour` (bool, default true)
- `showDate` (bool, default true)
- `timeColor`, `dateColor`, `titleColor` (QColor)

### Signal Wiring Pattern

All interactive item signals propagate:
```
MeterItem (emits signal)
  → MeterWidget (re-emits or forwards)
    → ContainerWidget (re-emits)
      → ContainerManager (routes to MainWindow)
        → MainWindow (calls model setters)
```

Items never reference RadioModel or SliceModel directly.

---

## Phase 3G-6: Container Settings Dialog

User-facing UI for composing container contents and editing item properties.

### ContainerSettingsDialog

Modal `QDialog`, opened from container title bar settings button
(`ContainerWidget::settingsRequested()` signal, already wired).

**Three-panel layout:**

```
+------------------+---------------------------+------------------+
| Content List     | Property Editor           | Live Preview     |
| (200px fixed)    | (stretch)                 | (200px fixed)    |
|                  |                           |                  |
| [icon] S-Meter   | Range: [-140] to [0]     | +--------------+ |
| [icon] BarItem   | Color: [  #00b4d8  ]     | | Live meter   | |
| [icon] TextItem  | Red threshold: [___]     | | rendering    | |
|                  | Attack: [0.8]             | |              | |
|  [+ Add] [- Del] | Decay: [0.2]             | +--------------+ |
+------------------+---------------------------+------------------+
```

**Left panel — Content list:**
- Ordered list of current items (icon + type name + binding label)
- Drag handles for reorder (changes z-order and vertical stacking)
- Add/Remove buttons at bottom
- "Add" opens categorized picker popup

**Add Item picker categories:**
- **Meters:** BarItem, NeedleItem, DialItem, HistoryGraphItem, MagicEyeItem
- **Indicators:** LEDItem, TextItem, ClockItem
- **Controls:** BandButtonItem, ModeButtonItem, FilterButtonItem, VfoDisplayItem
- **Layout:** SolidColourItem, ImageItem

Each entry: small icon + name + one-line description. New item inserted after
current selection with sensible defaults.

**Center panel — Property editor:**
Switches form based on selected item type. All items share common properties
(position x/y/w/h sliders, z-order spinner, binding ID dropdown). Type-specific
properties listed in the Phase 3G-4 and 3G-5 sections above.

**Right panel — Live preview:**
A `MeterWidget` instance rendering the current item configuration in real-time.
Updates as properties change in the editor.

**Container-level properties** (tab or section above item list):
- Background color picker
- Border toggle
- Title text
- RX source dropdown (RX1, RX2, etc.)
- Show on RX / Show on TX toggles

### Import/Export

- **Export:** Serializes container item list + container properties via existing
  `MeterWidget::serializeItems()` + `ContainerWidget::serialize()`.
  Base64-encoded for clipboard safety. "Copy to Clipboard" button.
- **Import:** "Paste from Clipboard" button → Base64 decode → validate →
  confirmation dialog → replace current content.
- Format: existing pipe-delimited text, Base64-wrapped.

### Preset Browser

Built-in named presets via `ItemGroup` factory methods (extending existing pattern):

| Preset | Contents |
|--------|----------|
| S-Meter Only | NeedleItem S-meter, full container |
| S-Meter + Bar | NeedleItem (top 60%) + BarItem (bottom 40%) |
| Full Panel | S-Meter + Power/SWR + ALC + Mic + Comp (stacked) |
| TX Monitor | Power/SWR + ALC + Comp + LEDs (clip, SWR alarm) |
| Contest | VfoDisplayItem + BandButtons + ModeButtons + ClockItem |
| Retro | MagicEyeItem + DialItem |
| History | HistoryGraphItem + TextItem readout |

Preset picker: dropdown at top of dialog + "Load Preset" button.
Loading replaces all current items (confirmation prompt).

### Persistence

All configuration persists through existing `AppSettings` XML system.
`ContainerWidget::serialize()` / `deserialize()` already handle container + item
state. This dialog provides the UI to edit what's serialized.

---

## Serialization Registry

The `ItemGroup::deserialize()` method needs a type tag registry for the new types.
Current tags: `BAR`, `SOLID`, `IMAGE`, `SCALE`, `TEXT`, `NEEDLE`.

New tags added in 3G-4 / 3G-5:

| Tag | Class |
|-----|-------|
| `HISTORY` | HistoryGraphItem |
| `MAGICEYE` | MagicEyeItem |
| `DIAL` | DialItem |
| `LED` | LEDItem |
| `BANDBTNS` | BandButtonItem |
| `MODEBTNS` | ModeButtonItem |
| `FILTERBTNS` | FilterButtonItem |
| `VFO` | VfoDisplayItem |
| `CLOCK` | ClockItem |

Each class implements `serialize()` / `deserialize()` following the existing
`TAG|x|y|w|h|bindingId|zOrder|...typeSpecificFields` pipe-delimited format.

---

## Files Modified / Created

### New files:
- `src/gui/meters/HistoryGraphItem.h/.cpp`
- `src/gui/meters/MagicEyeItem.h/.cpp`
- `src/gui/meters/DialItem.h/.cpp`
- `src/gui/meters/LEDItem.h/.cpp`
- `src/gui/meters/BandButtonItem.h/.cpp`
- `src/gui/meters/ModeButtonItem.h/.cpp`
- `src/gui/meters/FilterButtonItem.h/.cpp`
- `src/gui/meters/VfoDisplayItem.h/.cpp`
- `src/gui/meters/ClockItem.h/.cpp`
- `src/gui/containers/ContainerSettingsDialog.h/.cpp`

### Modified files:
- `src/gui/meters/MeterItem.h` — add virtual `hitTest()`, `handleMousePress()`,
  `handleMouseRelease()`, `handleWheel()` to base class
- `src/gui/meters/MeterWidget.h/.cpp` — add mouse event overrides + forwarding
- `src/gui/meters/ItemGroup.h/.cpp` — add new preset factories, update deserialize registry
- `src/gui/containers/ContainerWidget.h/.cpp` — wire settings button to dialog,
  forward interactive item signals
- `CMakeLists.txt` — add new source files

---

## Design Decisions

1. **Ring buffer over List+cleanup** for HistoryGraphItem — O(1) insert vs Thetis's
   O(n) removeAt(0). Fixed capacity is simpler and more predictable.

2. **Separate files per item type** rather than adding to MeterItem.h/.cpp —
   MeterItem.cpp is already 12k+ tokens. Each new type gets its own .h/.cpp pair.

3. **Signal-based interaction** — interactive items emit Qt signals, never touch
   RadioModel directly. Wiring happens at ContainerManager/MainWindow level.

4. **Flat LED style only** — Thetis supports flat + 3D + blink + pulsate.
   Starting simple; 3D/animations can be added later if requested.

5. **No per-item context menus on passive items** — matches Thetis pattern where
   only functional items (band/filter/VFO) have right-click actions.
