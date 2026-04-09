# Phase 3D: GPU Spectrum & Waterfall — Implementation Plan

## Context

NereusSDR Phase 3B is complete — ANAN-G2 I/Q streams through WDSP to audio output. The next milestone is **live spectrum display and waterfall** from the raw I/Q data. Unlike AetherSDR (where the radio sends pre-computed FFT bins), NereusSDR must compute FFTs client-side via FFTW3 and render them with Qt RHI GPU acceleration.

**Design sources:**
- `docs/architecture/gpu-waterfall.md` — 1570-line architecture spec (FFTEngine, SpectrumWidget, shaders, overlays)
- AetherSDR `SpectrumWidget` — structural template (QRhiWidget, 3 GPU pipelines, ring-buffer waterfall)
- Thetis `display.cs` — authoritative display logic, color schemes, all "knobs"

**Key decision:** Default coloration follows AetherSDR/SmartSDR style (dark background, cyan spectrum fill, thermal waterfall gradient). All Thetis display knobs will be exposed for full configurability. Tooltips adapted thoughtfully from Thetis `setup.designer.cs`.

---

## Step 1: FFTEngine (spectrum worker thread)

**New files:** `src/core/FFTEngine.h`, `src/core/FFTEngine.cpp`

Port from: Thetis `display.cs` data flow + `console.cs` spectrum integration.
Structure from: `docs/architecture/gpu-waterfall.md` lines 48-161.

- FFTW3 `fftwf_plan_dft_1d` (float precision, `FFTW_MEASURE`)
- I/Q accumulation buffer → window function → FFT → 10*log10(I²+Q²) → dBm bins
- Window functions: BlackmanHarris4 (default, -92dB sidelobes), Hanning, Hamming, BlackmanHarris7, Kaiser, Flat
  - From Thetis: these match the WDSP `SetAnalyzer` window options
  - Constants from gpu-waterfall.md lines 184-216
- Averaging: Exponential IIR (default alpha=0.35, matches AetherSDR `SMOOTH_ALPHA`) and Linear (N-frame ring buffer, 2-100)
- Rate limiting: target FPS (default 30, range 1-60)
- FFT sizes: 1024, 2048, 4096 (default), 8192, 16384
  - From Thetis `display.cs:215`: `BUFFER_SIZE = 16384`
- Signal: `fftReady(int receiverId, const QVector<float>& binsDbm)` — auto-queued to main thread
- Thread-safe parameter updates via `std::atomic` for settings changed from main thread

**I/Q tap point:** `RadioModel::wireConnectionSignals()` at `RadioModel.cpp:207`. Fork the raw interleaved I/Q samples to FFTEngine BEFORE deinterleaving. FFTEngine handles its own deinterleave + accumulation to FFT-sized buffers independently from the WDSP 1024-sample path.

```
iqDataReceived signal
  ├── existing: accumulate → WDSP fexchange2 → audio
  └── NEW: FFTEngine::feedIQ() → FFT → fftReady signal → SpectrumWidget
```

---

## Step 2: SpectrumWidget (CPU fallback first, then GPU)

**New files:** `src/gui/SpectrumWidget.h`, `src/gui/SpectrumWidget.cpp`

Structure from: AetherSDR `SpectrumWidget` (QRhiWidget pattern).
Logic from: Thetis `display.cs` DrawPanadapterDX2D (line 4970) + DrawWaterfallDX2D (line 6469).

### Phase A — CPU fallback (QPainter, get something visible fast)

- Inherit from `QWidget` (no `#ifdef NEREUS_GPU_SPECTRUM` initially)
- Layout: 40% spectrum (top), 60% waterfall (bottom), 20px freq scale, 36px dBm strip, 4px divider
  - Constants from gpu-waterfall.md: `kFreqScaleH=20`, `kDividerH=4`, `kDbmStripW=36`
- `updateSpectrum(QVector<float> binsDbm)` — exponential smoothing (alpha=0.35), push waterfall row
- Waterfall: `QImage` ring buffer (`Format_RGB32`), draw via `QPainter::drawImage()`
- Spectrum: `QPainter::drawPolyline()` for trace, `QPainter::drawPolygon()` for fill
- Grid: frequency lines (vertical) + dBm lines (horizontal)
- Frequency/dBm scale labels
- Draggable spectrum/waterfall divider

### Phase B — GPU rendering (QRhiWidget)

- Switch base class with `#ifdef NEREUS_GPU_SPECTRUM` → `QRhiWidget`
- 3 GPU pipelines matching AetherSDR exactly:
  1. **Waterfall** — textured quad, ring-buffer texture, `fract(uv.y + rowOffset)` in fragment shader
  2. **Spectrum** — LineStrip (trace) + TriangleStrip (fill), per-vertex RGBA, alpha blending
  3. **Overlay** — textured quad, QPainter-rendered static overlay as texture
- Render order: waterfall → spectrum fill → spectrum line → overlay
- Partial texture upload (single row per frame, not full texture)
- On macOS: `setApi(QRhiWidget::Api::Metal)` per AetherSDR pattern

---

## Step 3: Shaders

**New files:** `resources/shaders/` — 6 files copied from AetherSDR with NereusSDR naming

| Shader | Source | Purpose |
|--------|--------|---------|
| `waterfall.vert` | AetherSDR `texturedquad.vert` | Fullscreen quad positions + UV |
| `waterfall.frag` | AetherSDR `texturedquad.frag` | Ring-buffer `fract(uv.y + rowOffset)` sampling |
| `spectrum.vert` | AetherSDR `spectrum.vert` | Per-vertex position + color passthrough |
| `spectrum.frag` | AetherSDR `spectrum.frag` | Per-vertex color output |
| `overlay.vert` | AetherSDR `overlay.vert` | Textured quad positions |
| `overlay.frag` | AetherSDR `overlay.frag` | RGBA texture sampling |

All GLSL 4.40. Compiled via `qt_add_shaders()` to `.qsb` format.

---

## Step 4: Waterfall Color Schemes

Port from: Thetis `display.cs` lines 6800-7700 (all 9 color schemes) + `enums.cs:68-79`.
Default style from: AetherSDR gradient (lines 43-51 in SpectrumWidget.cpp).

**Color schemes to implement:**

| Scheme | Thetis Name | Source |
|--------|-------------|--------|
| Default | (AetherSDR style) | AetherSDR gradient: black→dark blue→blue→cyan→green→yellow→red |
| Enhanced | `enhanced` | Thetis display.cs:6864-6954 (9-band progression) |
| Spectran | `SPECTRAN` | Thetis display.cs:6956-7036 |
| BlackWhite | `BLACKWHITE` | Thetis display.cs:7038-7075 |
| LinLog | `LinLog` | Thetis display.cs:7077-7288 |
| LinRad | `LinRad` | Thetis display.cs:7289-7490 |
| LinAuto | `LinAuto` | Thetis display.cs:7494-7700 (histogram-equalized) |
| Custom | `Custom` | User-defined 101-element gradient array |

**Default gradient stops (AetherSDR):**
```
{0.00,   0,   0,   0},    // black
{0.15,   0,   0, 128},    // dark blue
{0.30,   0,  64, 255},    // blue
{0.45,   0, 200, 255},    // cyan
{0.60,   0, 220,   0},    // green
{0.80, 255, 255,   0},    // yellow
{1.00, 255,   0,   0},    // red
```

Color mapping: `dbmToRgb()` with adjustable color gain (0-100, default 50) and black level (0-125, default 15), matching AetherSDR formula from `SpectrumWidget.cpp:1700`.

---

## Step 5: Display Settings ("Knobs")

All settings from Thetis `display.cs` + `setup.designer.cs`, persisted via AppSettings per-pan.

### Spectrum Settings
| Setting | Key | Default | Thetis Source | Tooltip |
|---------|-----|---------|---------------|---------|
| FFT Size | `DisplayFftSize` | 4096 | console.cs specRX.FFTSize | "FFT size for spectrum computation (larger = finer resolution, more CPU)" |
| Window Function | `DisplayWindowFunc` | BlackmanHarris4 | WDSP SetAnalyzer | "Window function applied before FFT computation" |
| Averaging Mode | `DisplayAvgMode` | Exponential | display.cs averaging | "Spectrum averaging mode for noise reduction" |
| Averaging Time | `DisplayAvgTime` | 30 | setup: udDisplayAVGTime "When averaging, use this number of buffers to calculate the average." | "Number of milliseconds to average spectrum data" |
| FPS | `DisplayFftFps` | 30 | AetherSDR default 25 | "Target display refresh rate (higher = smoother, more CPU)" |
| Fill Alpha | `DisplayFftFillAlpha` | 0.70 | AetherSDR default | "Transparency of the spectrum fill area (0=invisible, 1=opaque)" |
| Fill Color | `DisplayFftFillColor` | #00e5ff (cyan) | AetherSDR default | "Color of the spectrum trace and fill" |
| Heat Map | `DisplayFftHeatMap` | true | AetherSDR default | "Color spectrum trace by signal strength instead of solid color" |
| Line Width | `DisplayLineWidth` | 1.0 | Thetis display.cs:2585 `_display_line_width` | "Width of the spectrum trace line" |
| Pan Fill | `DisplayPanFill` | true | Thetis setup: chkDisplayPanFill "Check to fill the panadapter display line below the data." | "Fill the area below the spectrum trace" |
| Grid Max | `DisplayGridMax` | -40 | Thetis display.cs:1743 "Signal level at top of display in dB." | "Maximum signal level shown at top of display (dBm)" |
| Grid Min | `DisplayGridMin` | -140 | Thetis display.cs:1754 "Signal Level at bottom of display in dB." | "Minimum signal level shown at bottom of display (dBm)" |
| Grid Step | `DisplayGridStep` | 5 | Thetis display.cs:1765 "Horizontal Grid Step Size in dB." | "Grid line spacing in dB" |

### Waterfall Settings
| Setting | Key | Default | Thetis Source | Tooltip |
|---------|-----|---------|---------------|---------|
| Color Gain | `DisplayWfColorGain` | 50 | AetherSDR default | "Waterfall color intensity — higher values increase contrast" |
| Black Level | `DisplayWfBlackLevel` | 15 | AetherSDR default | "Signal level mapped to black — adjusts noise floor visibility" |
| Auto Black | `DisplayWfAutoBlack` | true | AetherSDR default | "Automatically adjust black level based on noise floor" |
| Line Duration | `DisplayWfLineDuration` | 100 | AetherSDR default | "Time each waterfall row is visible (ms) — lower = faster scroll" |
| Color Scheme | `DisplayWfColorScheme` | 0 (Default) | Thetis enums.cs:68 | "Waterfall color palette" |
| High Threshold | `DisplayWfHighLevel` | -80.0 | Thetis display.cs:2522 | "Signal level mapped to maximum color intensity (dBm)" |
| Low Threshold | `DisplayWfLowLevel` | -130.0 | Thetis display.cs:2536 | "Signal level mapped to minimum color / black (dBm)" |
| Update Period | `DisplayWfUpdatePeriod` | 2 | Thetis display.cs:3038 "How often to update (scroll another pixel line). Note that this is tamed by the FPS setting." | "Waterfall scroll rate — update every N frames" |
| WF Average Time | `DisplayWfAvgTime` | 120 | setup: udDisplayAVTimeWF | "Waterfall averaging time in milliseconds" |
| Show RX Filter | `DisplayWfShowRxFilter` | false | Thetis display.cs:2425 | "Show receiver filter passband overlay on waterfall" |
| Show TX Filter | `DisplayWfShowTxFilter` | false | Thetis display.cs:2416 | "Show transmit filter overlay on waterfall" |
| Low Color | `DisplayWfLowColor` | Black | Thetis display.cs:2510 "The Color to use when the signal level is at or below the low level set above." | "Color for signals at or below the low threshold" |

### Grid/Color Settings
| Setting | Key | Default | Thetis Source |
|---------|-----|---------|---------------|
| Grid Color | `DisplayGridColor` | rgba(65,255,255,255) | Thetis display.cs:2069 |
| H-Grid Color | `DisplayHGridColor` | White | Thetis display.cs:2102 |
| Grid Text Color | `DisplayGridTextColor` | Yellow | Thetis display.cs:2003 |
| Zero Line Color | `DisplayZeroLineColor` | Red | Thetis display.cs:2037 |
| Background Color | `DisplayBgColor` | #0f0f1a | NereusSDR STYLEGUIDE (dark theme) |
| Data Line Color | `DisplayDataLineColor` | White | Thetis display.cs:2184 |
| Spectrum Split | `DisplaySpectrumFrac` | 0.40 | gpu-waterfall.md:590 |

### Per-Pan Persistence
Following AetherSDR pattern: `settingsKey(base)` appends `_N` suffix for pan index > 0.
- Pan 0: `DisplayFftSize`
- Pan 1: `DisplayFftSize_1`

---

## Step 6: Wire into MainWindow

**Modify:** `src/gui/MainWindow.h/.cpp`

- Replace the placeholder QLabel central widget with SpectrumWidget
- Create FFTEngine on a worker thread, wire `iqDataReceived` → `FFTEngine::feedIQ()`
- Wire `FFTEngine::fftReady()` → `SpectrumWidget::updateSpectrum()`
- Set initial frequency range from RadioModel (center freq, bandwidth)

**Modify:** `src/models/RadioModel.h/.cpp`

- Add signal: `rawIqData(const QVector<float>& samples)` emitted from the iqDataReceived lambda
- This allows MainWindow to route raw I/Q to FFTEngine without RadioModel knowing about FFT

---

## Step 7: CMakeLists.txt Updates

**Modify:** `CMakeLists.txt`

- Add new source files to `SOURCES`
- Add `qt_add_shaders()` call for the 6 shader files (when GPU path is ready)
- Shader prefix: `/shaders`

```cmake
if(NEREUS_GPU_SPECTRUM AND Qt6ShaderTools_FOUND)
    qt_add_shaders(NereusSDR "nereus_shaders"
        PREFIX "/shaders"
        FILES
            resources/shaders/waterfall.vert
            resources/shaders/waterfall.frag
            resources/shaders/spectrum.vert
            resources/shaders/spectrum.frag
            resources/shaders/overlay.vert
            resources/shaders/overlay.frag
    )
endif()
```

---

## Step 8: Overlays (grid, VFO markers, filter passband)

Port from: Thetis `display.cs` DrawPanadapterDX2D grid section + AetherSDR overlay system.

**Static overlay (QPainter → QImage → GPU texture):**
- Frequency grid lines (vertical) — color: `rgba(65,255,255,255)` from Thetis
- dBm grid lines (horizontal) — adapt from Thetis `hgrid_color`
- Frequency scale bar (bottom, 20px)
- dBm scale strip (left, 36px)
- VFO frequency marker (vertical orange line for active slice)
- Filter passband (semi-transparent rectangle)
- Zero-line (red, 2px) at tuned frequency — from Thetis `grid_zero_color`

**For NereusSDR dark theme, adapt Thetis defaults:**
- Grid text: Yellow → keep (good contrast on dark bg)
- Grid lines: semi-transparent white → keep
- Background: Black → `#0f0f1a` (NereusSDR STYLEGUIDE)
- Zero line: Red → keep

---

## Step 9: Settings UI Architecture

Display settings live in **three tiers** based on access frequency:

### Tier 1: Direct manipulation (every session)
- Reference level / dynamic range → **drag dBm scale** on SpectrumWidget
- Center frequency / bandwidth → **drag/scroll** on spectrum/waterfall
- Spectrum/waterfall split → **drag divider bar**

No menu needed — built into SpectrumWidget mouse handlers.

### Tier 2: Right-click overlay menu (occasional adjustment)

**New file:** `src/gui/SpectrumOverlayMenu.h/.cpp`

A self-contained QWidget popup owned by SpectrumWidget. Shown on right-click.
Communicates via signals only — no awareness of containers or docking.
Designed so it can later be wrapped in a ContainerWidget (Phase 3F) without changes.

Contents:
- FFT Size selector (1024/2048/4096/8192/16384)
- Averaging mode + time slider
- FPS slider
- Color scheme selector
- WF color gain + black level sliders
- Auto black toggle
- WF speed slider
- Spectrum fill toggle + alpha slider
- Heat map toggle
- "Display Settings..." link → opens Setup dialog Display tab

### Tier 3: Setup dialog Display tab (set-and-forget)

**New file:** `src/gui/DisplaySettingsPage.h/.cpp` (embedded in future SetupDialog)

For now, accessible via View → Display Settings... menu item. Contains:
- Grid colors (grid, h-grid, text, zero-line, background)
- Waterfall high/low thresholds
- Waterfall low color picker
- Custom gradient editor (load/save)
- Window function selector
- Spectrum line width, data line color
- Show/hide toggles: RX filter on WF, TX filter on WF, noise floor, AGC lines
- Label alignment
- Per-receiver tabs (RX1/RX2/TX) when multi-receiver is added

### Container system note
The SpectrumOverlayMenu is NOT built on the Phase 3F container/popout architecture.
It's a simple QWidget popup — clean signals, self-contained, easy to reparent later.
The container system (Phase 3F) will use the right sidebar applet panel as its first
real use case. See `docs/MASTER-PLAN.md` Phase 3F for container architecture details.

---

## Build Order (incremental, testable at each step)

1. **FFTEngine** — create, wire to RadioModel I/Q, verify with qCDebug logging that bins are computed
2. **SpectrumWidget CPU** — QPainter spectrum trace + waterfall + grid, embed in MainWindow
3. **Build & test** — see live spectrum from ANAN-G2
4. **Waterfall color schemes** — implement all schemes, default to AetherSDR style
5. **Display settings** — expose all knobs, persist via AppSettings
6. **SpectrumOverlayMenu** — right-click popup with Tier 2 controls
7. **Shaders** — create 6 GLSL files from AetherSDR templates
8. **GPU rendering** — switch to QRhiWidget, 3 pipelines, partial texture upload
9. **Overlays** — grid, VFO marker, filter passband, freq/dBm scales
10. **Mouse interaction** — click-to-tune, drag filter, scroll zoom, drag pan, drag dBm scale
11. **DisplaySettingsPage** — Tier 3 deep settings dialog
12. **Tooltips** — adapted from Thetis setup.designer.cs

---

## Files Summary

| Action | Path |
|--------|------|
| **Create** | `src/core/FFTEngine.h` |
| **Create** | `src/core/FFTEngine.cpp` |
| **Create** | `src/gui/SpectrumWidget.h` |
| **Create** | `src/gui/SpectrumWidget.cpp` |
| **Create** | `src/gui/SpectrumOverlayMenu.h` |
| **Create** | `src/gui/SpectrumOverlayMenu.cpp` |
| **Create** | `src/gui/DisplaySettingsPage.h` |
| **Create** | `src/gui/DisplaySettingsPage.cpp` |
| **Create** | `resources/shaders/waterfall.vert` |
| **Create** | `resources/shaders/waterfall.frag` |
| **Create** | `resources/shaders/spectrum.vert` |
| **Create** | `resources/shaders/spectrum.frag` |
| **Create** | `resources/shaders/overlay.vert` |
| **Create** | `resources/shaders/overlay.frag` |
| **Modify** | `src/models/RadioModel.h` — add `rawIqData` signal |
| **Modify** | `src/models/RadioModel.cpp` — emit rawIqData in iqDataReceived lambda |
| **Modify** | `src/gui/MainWindow.h` — add FFTEngine*, SpectrumWidget* members |
| **Modify** | `src/gui/MainWindow.cpp` — create/wire FFTEngine + SpectrumWidget |
| **Modify** | `CMakeLists.txt` — new sources, qt_add_shaders() |

---

## Verification

1. **Build:** `cmake --build build -j$(sysctl -n hw.ncpu)` — no errors
2. **Launch:** App shows SpectrumWidget where the placeholder label was
3. **Connect to ANAN-G2:** spectrum trace and waterfall appear with live signals
4. **Audio still works:** I/Q→WDSP→audio path unaffected by FFT tap
5. **Waterfall scrolls:** new rows appear, ring buffer wraps correctly
6. **Grid visible:** frequency and dBm labels, grid lines
7. **Color schemes:** switch between Default/Enhanced/Spectran/etc.
8. **Settings persist:** change FFT size, restart, verify it's remembered
9. **GPU rendering (Phase B):** smooth 60fps, no tearing, Metal on macOS
