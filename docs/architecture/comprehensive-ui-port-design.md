# NereusSDR — Comprehensive UI Port Design Spec

## Overview

This document specifies the comprehensive port of Thetis Setup dialog features
and AetherSDR overlay/applet concepts into NereusSDR. The design creates a
three-layer UI architecture that brings the full breadth of both applications
into NereusSDR while maintaining a clean, modern UX.

**Design principles:**
- AetherSDR patterns for **structure** (overlay, applets, containers)
- Thetis features for **completeness** (every setup item accounted for)
- Full UI skeleton built upfront with NYI indicators for unwired controls
- Controls wired progressively across existing phase milestones

---

## 1. Three-Layer UI Architecture

NereusSDR's feature surface is organized into three interaction layers,
each targeting a different usage frequency:

| Layer | Location | Purpose | Interaction frequency |
|-------|----------|---------|----------------------|
| **Left Overlay** | Top-left of SpectrumWidget | Quick-access operating controls | Every QSO |
| **Right Containers** | Right panel (ContainerManager) | Applet widgets for monitoring + control | Frequently during operation |
| **Setup Dialog** | Modal dialog (File → Settings) | Deep configuration, hardware, calibration | Occasionally / once |

All three layers share `RadioModel` as the single source of truth. Controls
that appear in multiple places (e.g., AGC mode in VFO tab AND RxApplet)
stay in sync via RadioModel/SliceModel signals.

### Data Flow

```
Left Overlay    ─┐
Right Containers ─┤─→ signals ─→ RadioModel/SliceModel ─→ RadioConnection ─→ Radio
Setup Dialog    ─┘                    ↑                         │
                                      └── status updates ──────┘
                                      │
                              AppSettings (persistence)
```

---

## 2. Layer 1 — Left Spectrum Overlay (SpectrumOverlayPanel)

Port of AetherSDR's `SpectrumOverlayMenu`. A vertical strip of toggle buttons
anchored to the top-left of each SpectrumWidget. Each button opens a flyout
sub-panel over the spectrum.

### New class: `SpectrumOverlayPanel`

**File:** `src/gui/SpectrumOverlayPanel.h/.cpp`

**Parent pattern:** AetherSDR `SpectrumOverlayMenu` (214-line header, 800+ line impl)

### Buttons (top to bottom)

| Button | Flyout | Controls | Tier |
|--------|--------|----------|------|
| **+RX** | None (direct action) | Adds receive slice on current panadapter | Tier 2 (Phase 3F) |
| **+TNF** | None (direct action) | Adds tracking notch filter at cursor freq | Tier 2 |
| **BAND** | Band grid panel | 6x3 HF bands + WWV/GEN/LF/XVTR. Ported from AetherSDR layout. | Tier 1 |
| **ANT** | Antenna panel | RX antenna dropdown, TX antenna dropdown, RF gain slider, WNB toggle + level | Tier 1 (partial — antenna dropdowns exist in VfoWidget) |
| **DSP** | DSP panel | NB, NR, ANF, SNB toggle buttons + level sliders. Maps to WDSP via RxChannel. | Tier 1 |
| **Display** | Display panel | **Per-slice tabbed** — see below | Tier 1 |
| **DAX** | DAX panel | DAX Audio channel selector (Off, 1–4), DAX IQ channel selector (Off, 1–4) | Tier 2 (Phase 3-DAX) |

### Display Flyout — Expanded Design

The Display flyout is significantly expanded vs AetherSDR. It uses **per-slice
tabs** (RX1 / RX2 / TX) and exposes the most-used Thetis Display settings:

**Tab: RX1 (and RX2 when enabled)**
- FFT Average (slider, 0–10)
- FFT FPS (slider, 15–60)
- Fill Alpha (slider, 0–100%)
- Fill Color (color picker button)
- Waterfall Gain (slider, 0–100)
- Waterfall Black Level (slider, 0–125)
- Auto Black toggle
- Color Scheme dropdown (Default, Grayscale, BlueGreen, Fire, Plasma)
- Show Grid toggle
- Cursor Frequency toggle
- Heat Map toggle
- Noise Floor Enable + Position slider
- Weighted Average toggle

**Tab: TX** (same controls scoped to TX display when transmitting)

**Footer:** "More Display Options →" button — opens Setup Dialog pre-navigated
to Setup → Display → [current slice's tab].

### Styling

- Button size: 68px × 22px (AetherSDR constants `BTN_W`, `BTN_H`)
- Normal: `rgba(20, 30, 45, 240)` background, `#304050` border
- Hover/Active: `rgba(0, 112, 192, 180)` (blue highlight)
- Sub-panel background: `rgba(15, 15, 26, 220)`, border `#304050`
- Event filter blocks mouse/wheel from falling through to spectrum

### Signals (emitted to MainWindow)

```cpp
// Direct actions
void addRxClicked(const QString& panId);
void addTnfClicked();

// Band
void bandSelected(const QString& bandName, double freqHz, const QString& mode);

// Antenna
void rxAntennaChanged(int index);
void txAntennaChanged(int index);
void rfGainChanged(int gain);
void wnbToggled(bool enabled);
void wnbLevelChanged(int level);

// DSP
void nbToggled(bool enabled);
void nrToggled(bool enabled);
void anfToggled(bool enabled);
void snbToggled(bool enabled);

// Display (per-slice via panId)
void fftAverageChanged(int frames);
void wfColorGainChanged(int gain);
void wfBlackLevelChanged(int level);
void colorSchemeChanged(int index);
void fftFpsChanged(int fps);
void fillAlphaChanged(int percent);
void fillColorChanged(const QColor& color);
void autoBlackToggled(bool enabled);
void showGridToggled(bool enabled);
void cursorFreqToggled(bool enabled);
void heatMapToggled(bool enabled);
void noiseFloorToggled(bool enabled);
void noiseFloorPositionChanged(int position);
void weightedAverageToggled(bool enabled);

// DAX
void daxAudioChannelChanged(int channel);
void daxIqChannelChanged(int channel);

// Shortcut to Setup
void openDisplaySettings(const QString& sliceName);
```

---

## 3. Layer 2 — Right Container Applets

NereusSDR's `ContainerManager` is extended to host **applet widgets** in
addition to the existing `MeterWidget` content. Each applet is a QWidget
subclass that can be installed into any container.

### Architecture

```
ContainerWidget (existing)
  └── content area currently holds MeterWidget
  └── EXTENDED to hold any QWidget:
      ├── MeterWidget (existing — S-meter, Power/SWR, ALC)
      ├── RxApplet (new)
      ├── TxApplet (new)
      ├── EqApplet (new)
      ├── PhoneCwApplet (new)
      ├── PureSignalApplet (new)
      ├── DiversityApplet (new)
      ├── CwxApplet (new)
      ├── DvkApplet (new)
      ├── FmApplet (new)
      ├── DigitalApplet (new)
      └── CatApplet (new)
```

### Applet Base Class

**File:** `src/gui/applets/AppletWidget.h`

```cpp
class AppletWidget : public QWidget {
    Q_OBJECT
public:
    explicit AppletWidget(RadioModel* model, QWidget* parent = nullptr);
    virtual QString appletId() const = 0;     // e.g., "RX", "TX", "EQ"
    virtual QString appletTitle() const = 0;   // e.g., "RX Controls"
    virtual QIcon appletIcon() const;
    virtual void syncFromModel() = 0;          // refresh all controls from RadioModel
protected:
    RadioModel* m_model = nullptr;
    bool m_updatingFromModel = false;          // feedback loop guard
};
```

### Applet Inventory

#### RxApplet
**Source:** AetherSDR `RxApplet` + Thetis front-panel RX controls
- AF Gain slider (0–100)
- RF Gain slider (range depends on radio)
- AGC mode combo (Off, Long, Slow, Med, Fast, Custom)
- AGC Threshold slider
- Squelch toggle + level slider
- NB toggle + level
- NR toggle + level
- ANF toggle
- SNB toggle
- BIN (binaural) toggle
- Mute toggle
- **Tier 1** — most controls map to existing SliceModel/RxChannel

#### TxApplet
**Source:** AetherSDR `TxApplet` + Thetis front-panel TX controls
- Drive / Power slider (0–100W)
- Mic Gain slider
- MOX button (toggle transmit)
- TUN button (tune carrier)
- PS button (PureSignal toggle)
- VOX toggle + VOX gain + VOX delay
- COMP (compressor) toggle + level
- CESSB toggle
- TX meter select dropdown (Power, SWR, ALC, Mic, EQ, Leveler, CESSB, CFC)
- **Tier 2** — requires Phase 3I (TX DSP)

#### EqApplet
**Source:** AetherSDR `EqApplet` + Thetis EQ
- RX / TX tab toggle
- 10-band graphic EQ (sliders: 32Hz, 63Hz, 125Hz, 250Hz, 500Hz, 1kHz, 2kHz, 4kHz, 8kHz, 16kHz)
- Preset dropdown (Flat, Bass Boost, Treble Boost, Custom, etc.)
- Enable toggle (RX EQ / TX EQ independent)
- **Tier 2** — requires WDSP EQ channel setup

#### PhoneCwApplet
**Source:** AetherSDR `PhoneCwApplet` + Thetis front-panel mode controls
- Mode-dependent stacked widget:
  - **Phone mode:** Mic source combo, TX EQ toggle, COMP toggle, DX toggle, Leveler toggle
  - **CW mode:** CW Speed (WPM slider), Sidetone freq slider, Sidetone level, QSK toggle, Break-in delay, Iambic A/B toggle, CW keyer mode
  - **Digital mode:** VAC stereo/mono, sample rate combo, TX ID toggle
- **Tier 2** — CW requires Phase 3I-2, Phone requires Phase 3I-1

#### PureSignalApplet
**Source:** Thetis PSForm + AetherSDR (no equivalent)
- PS Enable toggle
- Calibration status indicator (LED-style: red/yellow/green)
- Feedback level bar
- Correction magnitude bar
- Auto-calibrate button
- Info readout (iterations, feedback dB, correction dB)
- Distortion metric display
- **Tier 2** — requires Phase 3I-4 (PureSignal)

#### DiversityApplet
**Source:** Thetis Diversity controls
- Diversity enable toggle
- Source selector (RX1, RX2)
- Gain slider (sub-RX relative gain)
- Phase slider (0–360°)
- ESC (Enhanced Signal Combining) mode selector
- R2 gain + phase for ADC2 alignment
- **Tier 3** — requires multi-receiver + diversity DDC routing

#### CwxApplet
**Source:** Thetis CWX panel + AetherSDR CwxPanel
- Text input field (CW message buffer)
- Send button
- Speed override slider (WPM)
- Memories (6–12 programmable message buttons: CQ, RST, 73, etc.)
- Repeat toggle + interval
- Keyboard-to-CW mode toggle
- **Tier 2** — requires Phase 3I-2 (CW TX)

#### DvkApplet
**Source:** Thetis DVK + AetherSDR DvkPanel
- 4–8 voice keyer slots (record / play / stop per slot)
- Record button + level meter
- Repeat toggle + interval
- Semi break-in (auto TX/RX switching)
- WAV file import button
- **Tier 2** — requires Phase 3I-1 (TX audio)

#### FmApplet
**Source:** Thetis FM controls
- CTCSS tone combo (Off, 67.0Hz, 71.9Hz, ... 254.1Hz — full CTCSS table)
- CTCSS enable toggle
- Deviation selector (5kHz / 2.5kHz)
- TX offset combo (+600kHz, -600kHz, Simplex, Custom)
- FM squelch level
- **Tier 2** — requires FM mode support in WDSP

#### DigitalApplet
**Source:** Thetis Digital mode controls + AetherSDR CatApplet
- VAC 1 enable + device combo
- VAC 2 enable + device combo
- VAC sample rate combo
- VAC stereo/mono toggle
- VAC buffer size
- VAC TX gain / RX gain
- rigctld channel assignment
- **Tier 2** — requires VAC/DAX audio system

#### CatApplet
**Source:** AetherSDR `CatApplet` + Thetis CAT
- rigctld enable toggle + port number
- TCP CAT enable toggle + port number
- TCI server enable toggle + port number
- Active connections count display
- Serial port combo (for serial CAT)
- **Tier 3** — requires Phase 3K (CAT/rigctld)

### Container Defaults

On first run with no saved layout, create these default containers:

| Container | Content | Dock Mode | Position |
|-----------|---------|-----------|----------|
| #0 | MeterWidget (S-Meter + Power/SWR + ALC) | Panel-docked | Right, top |
| #1 | RxApplet | Panel-docked | Right, below #0 |
| #2 | TxApplet | Panel-docked | Right, below #1 |

Additional containers (EQ, Phone/CW, etc.) available via Containers menu but
not shown by default — user adds as needed.

---

## 4. Layer 3 — Setup Dialog

### Class: `SetupDialog`

**File:** `src/gui/SetupDialog.h/.cpp`

**Pattern:** QDialog with QSplitter — tree navigation left pane (QTreeWidget),
content area right pane (QStackedWidget). Matches AetherSDR RadioSetupDialog
structure but with Thetis breadth.

**Access:** File → Settings... (or keyboard shortcut)

### NYI Control Treatment

Controls not yet wired to backend:
- `setEnabled(false)` — greyed out, not interactive
- Small "NYI" QLabel badge positioned top-right of control or group
- Phase label on NYI badge tooltip: "Available in Phase 3I"
- Section headers show progress fraction: "Hardware (3/28 wired)"

### Tree Navigation Structure

10 top-level categories, ~45 sub-pages. Every Thetis Setup control gets a
placeholder. Controls are organized by functional area, not by Thetis tab
structure (some reorganization for clarity).

```
General
├── Startup & Preferences
│   ├── Start minimized                          [Tier 3]
│   ├── Auto-connect last radio                  [Tier 1] ✓
│   ├── Confirm on quit                          [Tier 3]
│   ├── Check for updates                        [Tier 3]
│   └── Language / locale                        [Tier 3]
├── UI Scale & Theme
│   ├── UI scale % (75–200)                      [Tier 2]
│   ├── Dark / Light theme toggle                [Tier 2]
│   ├── Font size override                       [Tier 3]
│   └── Custom accent color                      [Tier 3]
└── Navigation
    ├── Mouse wheel action combo (Tune/Zoom/Ref) [Tier 2]
    ├── Click-to-tune behavior                   [Tier 1] ✓
    ├── Tuning step sizes per mode               [Tier 1] ✓
    └── Scroll direction invert                  [Tier 3]

Hardware
├── Radio Selection
│   ├── Radio model display (read-only)          [Tier 1] ✓
│   ├── Board serial number                      [Tier 1] ✓
│   ├── Firmware version                         [Tier 1] ✓
│   ├── Protocol version (P1/P2)                 [Tier 1] ✓
│   ├── Nickname / callsign field                [Tier 2]
│   └── Hardware options checkboxes              [Tier 2]
├── ADC / DDC Configuration
│   ├── ADC count display                        [Tier 1] ✓
│   ├── DDC enable matrix (DDC0–6 per ADC)       [Tier 2]
│   ├── DDC sample rate per DDC                  [Tier 2]
│   ├── Dither enable per ADC                    [Tier 1] ✓
│   ├── Random enable per ADC                    [Tier 1] ✓
│   ├── ADC attenuation per ADC (0–31 dB)        [Tier 1]
│   └── Preamp enable per ADC                    [Tier 1]
├── Calibration
│   ├── RX1 frequency calibration offset         [Tier 3]
│   ├── RX2 frequency calibration offset         [Tier 3]
│   ├── TX frequency calibration offset          [Tier 3]
│   ├── RX level calibration (dBm ref)           [Tier 3]
│   └── PA gain calibration per band             [Tier 3]
├── Penny / Hermes OC (Open Collector)
│   ├── OC output enable per band (7 bits)       [Tier 3]
│   ├── OC output on TX per band                 [Tier 3]
│   └── OC timing / sequencing                   [Tier 3]
├── Alex Filters (HPF / LPF)
│   ├── Alex HPF auto/manual per band            [Tier 1] ✓
│   ├── Alex LPF auto/manual per band            [Tier 1] ✓
│   ├── Alex bypass options                      [Tier 1]
│   ├── Alex antenna relay matrix (RX/TX/XVTR)   [Tier 2]
│   └── Alex 2 (second Alex board) config        [Tier 3]
├── Firmware
│   ├── Current firmware version display         [Tier 1] ✓
│   ├── Firmware update button + progress        [Tier 3]
│   └── FPGA configuration display               [Tier 3]
├── Andromeda
│   ├── Encoder assignment matrix                [Tier 3]
│   ├── Button assignment matrix                 [Tier 3]
│   ├── LED brightness                           [Tier 3]
│   └── VFO encoder sensitivity                  [Tier 3]
└── Other H/W
    ├── External PA relay enable                 [Tier 3]
    ├── Amp protect (SWR trip level)             [Tier 3]
    └── External tuner interface                 [Tier 3]

Audio
├── Device Selection
│   ├── Output device combo                      [Tier 1] ✓
│   ├── Input device combo (mic)                 [Tier 2]
│   ├── Sample rate combo (44.1/48/96 kHz)       [Tier 1]
│   ├── Buffer size combo                        [Tier 2]
│   ├── Audio backend selector (WASAPI/CoreAudio/PulseAudio/ASIO) [Tier 2]
│   └── Latency display (read-only)              [Tier 2]
├── ASIO Configuration
│   ├── ASIO device combo                        [Tier 2]
│   ├── ASIO buffer size                         [Tier 2]
│   ├── ASIO sample rate                         [Tier 2]
│   ├── ASIO channel mapping (L/R)               [Tier 2]
│   ├── ASIO latency display                     [Tier 2]
│   └── ASIO control panel button                [Tier 2]
├── VAC 1
│   ├── VAC 1 enable toggle                      [Tier 2]
│   ├── VAC 1 device combo (input + output)      [Tier 2]
│   ├── VAC 1 sample rate combo                  [Tier 2]
│   ├── VAC 1 buffer size                        [Tier 2]
│   ├── VAC 1 stereo/mono toggle                 [Tier 2]
│   ├── VAC 1 TX gain slider                     [Tier 2]
│   ├── VAC 1 RX gain slider                     [Tier 2]
│   ├── VAC 1 latency display                    [Tier 2]
│   └── VAC 1 auto-enable on digital mode        [Tier 2]
├── VAC 2
│   ├── (same controls as VAC 1)                 [Tier 2]
│   └── VAC 2 purpose label (secondary route)    [Tier 2]
├── NereusDAX
│   ├── DAX enable toggle                        [Tier 2]
│   ├── DAX channel count (1–4)                  [Tier 2]
│   ├── DAX IQ channel count (1–4)               [Tier 2]
│   ├── Per-channel device mapping               [Tier 2]
│   ├── Per-channel sample rate                  [Tier 2]
│   ├── Per-channel buffer size                  [Tier 2]
│   ├── Auto-detect virtual devices button       [Tier 2]
│   ├── Create virtual devices button (platform) [Tier 3]
│   └── DAX status indicator per channel         [Tier 2]
└── Recording
    ├── Recording format combo (WAV, FLAC)       [Tier 3]
    ├── Recording sample rate                    [Tier 3]
    ├── Recording path selector                  [Tier 3]
    ├── Record pre-DSP / post-DSP toggle         [Tier 3]
    └── I/Q recording enable                     [Tier 3]

DSP
├── AGC / ALC
│   ├── RX1 AGC mode combo                       [Tier 1] ✓
│   ├── RX1 AGC threshold slider                 [Tier 1] ✓
│   ├── RX1 AGC slope (dB)                       [Tier 2]
│   ├── RX1 AGC decay (ms)                       [Tier 2]
│   ├── RX1 AGC hang threshold                   [Tier 2]
│   ├── RX1 AGC hang time (ms)                   [Tier 2]
│   ├── RX1 AGC fixed gain (dB)                  [Tier 2]
│   ├── RX2 AGC (same set)                       [Tier 2]
│   ├── TX ALC threshold                         [Tier 2]
│   ├── TX ALC decay                             [Tier 2]
│   └── Leveler enable + attack/decay/max        [Tier 2]
├── Noise Reduction (NR / ANF)
│   ├── NR mode selector (NR / NR2)             [Tier 1]
│   ├── NR taps                                  [Tier 2]
│   ├── NR delay                                 [Tier 2]
│   ├── NR gain                                  [Tier 2]
│   ├── NR leak                                  [Tier 2]
│   ├── NR2 (spectral) gain                      [Tier 2]
│   ├── NR2 NPE method                           [Tier 2]
│   ├── NR2 position (pre/post AGC)              [Tier 2]
│   ├── ANF taps                                 [Tier 2]
│   ├── ANF delay                                [Tier 2]
│   ├── ANF gain                                 [Tier 2]
│   └── ANF leak                                 [Tier 2]
├── Noise Blanker (NB / SNB)
│   ├── NB mode (NB1 / NB2)                     [Tier 1]
│   ├── NB threshold                             [Tier 1]
│   ├── NB2 mode (Zero/Sample-Hold)              [Tier 2]
│   ├── SNB enable                               [Tier 2]
│   └── SNB parameters                           [Tier 2]
├── CW
│   ├── CW pitch (Hz)                            [Tier 2]
│   ├── CW keyer speed (WPM)                     [Tier 2]
│   ├── CW keyer mode (Iambic A/B, Bug, Straight)[Tier 2]
│   ├── CW keyer weight                          [Tier 2]
│   ├── CW sidetone enable + volume              [Tier 2]
│   ├── CW QSK delay (ms)                        [Tier 2]
│   ├── CW break-in enable                       [Tier 2]
│   ├── CW auto-character spacing                [Tier 2]
│   ├── CW hang time                             [Tier 2]
│   └── CW RX audio delay                        [Tier 2]
├── AM / SAM
│   ├── AM TX carrier level                      [Tier 2]
│   ├── SAM sideband lock (LSB/USB/DSB)          [Tier 2]
│   ├── SAM PLL bandwidth                        [Tier 2]
│   ├── SAM fade leveling                        [Tier 2]
│   └── SAM PLL lock indicator enable            [Tier 2]
├── FM
│   ├── FM deviation (5kHz / 2.5kHz)             [Tier 2]
│   ├── FM CTCSS tone combo                      [Tier 2]
│   ├── FM CTCSS enable                          [Tier 2]
│   ├── FM TX offset                             [Tier 2]
│   ├── FM pre-emphasis enable                   [Tier 2]
│   └── FM de-emphasis enable                    [Tier 2]
├── VOX / Digi VOX
│   ├── VOX enable                               [Tier 2]
│   ├── VOX threshold                            [Tier 2]
│   ├── VOX hang time (ms)                       [Tier 2]
│   ├── VOX anti-trip gain                       [Tier 2]
│   ├── Digi VOX enable                          [Tier 2]
│   └── Digi VOX threshold                      [Tier 2]
├── CFC (Continuous Frequency Compressor)
│   ├── CFC enable                               [Tier 3]
│   ├── CFC pre-compression gain                 [Tier 3]
│   ├── CFC post-compression gain                [Tier 3]
│   ├── CFC 10-band threshold/ratio matrix       [Tier 3]
│   └── CFC EQ curve display                     [Tier 3]
├── EER (Envelope Elimination & Restoration)
│   ├── EER enable                               [Tier 3]
│   ├── EER AM/delay/phase adjustments           [Tier 3]
│   └── EER waveform display                     [Tier 3]
└── MNF (Manual Notch Filter)
    ├── MNF enable                               [Tier 2]
    ├── MNF frequency (Hz offset)                [Tier 2]
    └── MNF bandwidth                            [Tier 2]

Display
├── Spectrum Defaults
│   ├── Default FFT size combo (1024–16384)      [Tier 1]
│   ├── Default window function combo            [Tier 2]
│   ├── Default FFT average                      [Tier 1] ✓
│   ├── Default FPS cap                          [Tier 1] ✓
│   ├── Default fill alpha                       [Tier 1] ✓
│   ├── Spectrum line width                      [Tier 2]
│   └── Peak hold enable + decay                 [Tier 2]
├── Waterfall Defaults
│   ├── Default color scheme                     [Tier 1] ✓
│   ├── Default gain                             [Tier 1] ✓
│   ├── Default black level                      [Tier 1] ✓
│   ├── Waterfall rate (ms per line)             [Tier 2]
│   ├── Custom color scheme editor               [Tier 3]
│   └── Waterfall interpolation mode             [Tier 3]
├── Grid & Scales
│   ├── Grid line color                          [Tier 2]
│   ├── Grid line alpha                          [Tier 2]
│   ├── Frequency scale font size                [Tier 2]
│   ├── dBm scale font size                      [Tier 2]
│   ├── Band plan overlay enable                 [Tier 2]
│   └── Band plan color scheme                   [Tier 3]
├── RX2 Display
│   ├── RX2 spectrum enable                      [Tier 2]
│   ├── RX2 waterfall enable                     [Tier 2]
│   └── RX2 display settings (mirror of RX1)     [Tier 2]
└── TX Display
    ├── TX spectrum enable                       [Tier 2]
    ├── TX waterfall enable                      [Tier 2]
    ├── TX spectrum color                        [Tier 2]
    └── TX passband fill color                   [Tier 2]

Transmit
├── Power & PA
│   ├── Max power per band (W)                   [Tier 2]
│   ├── PA enable toggle                         [Tier 2]
│   ├── Tune power level (W)                     [Tier 2]
│   ├── Tune duration (s)                        [Tier 2]
│   ├── SWR protection threshold                 [Tier 2]
│   ├── SWR protection action (reduce/inhibit)   [Tier 2]
│   └── TX delay (ms)                            [Tier 2]
├── TX Profiles
│   ├── Profile name selector combo              [Tier 2]
│   ├── Save / Load / Delete profile buttons     [Tier 2]
│   ├── Profile includes: EQ, COMP, CESSB, Leveler, CFC settings [Tier 2]
│   └── Import / Export buttons                  [Tier 3]
├── Speech Processor
│   ├── Compressor enable + threshold + ratio    [Tier 2]
│   ├── CESSB enable + overshoot control         [Tier 2]
│   ├── Leveler enable + attack / decay / max    [Tier 2]
│   └── Processor order display                  [Tier 2]
└── PureSignal
    ├── PS enable toggle                         [Tier 2]
    ├── PS auto-attenuate                        [Tier 2]
    ├── PS feedback ADC selector                 [Tier 2]
    ├── PS calibration interval                  [Tier 2]
    ├── PS info display (Pk, FB, Corr)           [Tier 2]
    └── PS advanced parameters (attack, decay)   [Tier 3]

Appearance
├── Colors & Theme
│   ├── Theme selector (Dark / Light / Custom)   [Tier 2]
│   ├── Accent color                             [Tier 3]
│   ├── VFO color per slice (A/B/C/D)            [Tier 2]
│   ├── Background color                         [Tier 3]
│   ├── Text color                               [Tier 3]
│   └── Button style combo                       [Tier 3]
├── Meter Styles
│   ├── S-meter style combo (Analog/Bar/Both)    [Tier 2]
│   ├── Meter edge color                         [Tier 2]
│   ├── Meter background image selector          [Tier 3]
│   └── Meter font size                          [Tier 2]
├── Gradients
│   ├── S-meter gradient editor (6 color stops)  [Tier 3]
│   ├── Power meter gradient editor              [Tier 3]
│   └── SWR meter gradient editor                [Tier 3]
├── Skins
│   ├── Skin selector combo                      [Tier 3]
│   ├── Import skin button (ZIP)                 [Tier 3]
│   ├── Skin preview pane                        [Tier 3]
│   └── Thetis legacy skin import                [Tier 3]
└── Collapsible Display
    ├── Collapsed meter list config              [Tier 3]
    ├── Collapsed panel auto-hide                [Tier 3]
    └── Gadget enable/disable matrix             [Tier 3]

CAT & Network
├── Serial Ports (1–4)
│   ├── Port 1 enable + COM port + baud + params [Tier 3]
│   ├── Port 2 enable + COM port + baud + params [Tier 3]
│   ├── Port 3 enable + COM port + baud + params [Tier 3]
│   └── Port 4 enable + COM port + baud + params [Tier 3]
├── TCI Server
│   ├── TCI enable toggle                        [Tier 3]
│   ├── TCI port number                          [Tier 3]
│   ├── TCI active connections display           [Tier 3]
│   └── TCI protocol version                     [Tier 3]
├── TCP/IP CAT
│   ├── TCP CAT enable toggle                    [Tier 3]
│   ├── TCP CAT port number                      [Tier 3]
│   └── TCP CAT active connections display       [Tier 3]
└── MIDI Control
    ├── MIDI enable toggle                       [Tier 3]
    ├── MIDI device combo                        [Tier 3]
    ├── MIDI control mapping table               [Tier 3]
    └── MIDI learn mode button                   [Tier 3]

Keyboard
└── Shortcuts
    ├── Shortcut list (action + key binding)     [Tier 2]
    ├── Edit binding button                      [Tier 2]
    ├── Reset to defaults button                 [Tier 2]
    └── Import / Export shortcuts                [Tier 3]

Integrations
└── Discord Presence
    ├── Discord presence enable                  [Tier 3]
    ├── Show frequency toggle                    [Tier 3]
    ├── Show mode toggle                         [Tier 3]
    └── Show radio model toggle                  [Tier 3]

Diagnostics
├── Signal Generator
│   ├── Tone generator enable                    [Tier 3]
│   ├── Tone frequency (Hz)                      [Tier 3]
│   ├── Tone level (dBm)                         [Tier 3]
│   ├── Noise generator enable                   [Tier 3]
│   └── Two-tone test mode                       [Tier 3]
├── Hardware Tests
│   ├── ADC overflow counter                     [Tier 2]
│   ├── Sequence error counter                   [Tier 1] ✓
│   ├── Network packet loss display              [Tier 1]
│   └── I/Q balance check                        [Tier 3]
└── Logging
    ├── Log category enable matrix               [Tier 1] ✓
    ├── Log level combo (Debug/Info/Warning)      [Tier 1] ✓
    ├── Log file path                            [Tier 1] ✓
    ├── Create support bundle button             [Tier 1] ✓
    └── View log button                          [Tier 1] ✓
```

### Tier Summary

| Tier | Description | Phase(s) | Approx. controls |
|------|-------------|----------|------------------|
| **Tier 1** | Existing backend — wire now | Current | ~45 |
| **Tier 2** | Wire with upcoming phases | 3I, 3F, 3G-4..6 | ~130 |
| **Tier 3** | Wire later | 3J, 3K, 3L, 3H, 3N | ~80 |
| **Total** | | | **~255** |

---

## 5. NereusDAX — Cross-Platform Virtual Audio

### Architecture

NereusDAX is a platform abstraction layer that creates virtual audio devices
for routing processed audio to/from external applications (WSJT-X, fldigi,
CW Skimmer, etc.).

**File:** `src/audio/NereusDAX.h/.cpp` + platform backends

### Interface

```cpp
class NereusDAX {
    Q_OBJECT
public:
    struct Channel {
        int id;                 // 1–4
        enum Type { Audio, IQ };
        Type type;
        int sampleRate;         // 48000 for audio, 192000 for IQ
        int channels;           // 2 (stereo)
        int bufferFrames;       // configurable
        QString deviceName;     // "NereusDAX Audio 1"
        bool active;
    };

    bool createChannel(int id, Channel::Type type, int sampleRate);
    void destroyChannel(int id);
    QVector<Channel> listChannels() const;

    // Audio I/O — called from audio thread
    void writeAudio(int channelId, const float* samples, int frameCount);
    int readAudio(int channelId, float* buffer, int maxFrames);

signals:
    void channelCreated(int id);
    void channelDestroyed(int id);
    void channelError(int id, const QString& error);
};
```

### Platform Backends

| Platform | Backend class | Mechanism | Bundled? |
|----------|--------------|-----------|----------|
| macOS | `CoreAudioDAX` | AudioServerPlugIn HAL bundle installed to `/Library/Audio/Plug-Ins/HAL/` | Yes — ship NereusDAX.driver |
| Linux | `PipeWireDAX` | `pw-loopback` / `module-null-sink` created programmatically | Yes — uses system PipeWire/PulseAudio |
| Windows | `WindowsDAX` | Phase A: auto-detect VB-Cable; Phase B: virtual ASIO; Phase C: WDM miniport (MSVAD-based, attestation-signed) | Phase A: no; Phase B–C: yes |

### DAX Audio Tap Points

```
Radio → DDC → [DAX IQ tap] → WDSP → [DAX Audio tap] → Speaker
                  ↑                        ↑
            Pre-DSP I/Q              Post-DSP audio
            (192kHz stereo)          (48kHz stereo)
```

- DAX IQ channels tap raw I/Q **before** WDSP processing
- DAX Audio channels tap processed audio **after** WDSP processing
- Both are configurable per-channel in Setup → Audio → NereusDAX

---

## 6. ASIO Audio Backend

### Integration

ASIO is supported as an alternative audio backend for the primary audio I/O
(speaker output, microphone input). It is separate from NereusDAX — ASIO
handles the real hardware interface, DAX handles virtual routing.

**File:** `src/audio/AsioBackend.h/.cpp`

### Configuration (Setup → Audio → ASIO Configuration)

- ASIO device selector combo
- Buffer size selector (power-of-2: 64, 128, 256, 512, 1024, 2048)
- Sample rate selector (44100, 48000, 96000, 192000)
- Channel mapping — assign L/R to specific ASIO output channels
- Latency readout (calculated from buffer size + sample rate)
- "Open ASIO Control Panel" button (calls `ASIOControlPanel()`)

### Implementation Notes

- Use ASIO SDK (Steinberg, free for non-commercial — check license for
  NereusSDR's open-source status)
- Alternatively, PortAudio with ASIO host API (LGPL, well-tested)
- ASIO callbacks run on a dedicated high-priority thread — same atomic
  parameter pattern as current AudioEngine
- When ASIO is selected, QAudioSink is not used — ASIO replaces it entirely

---

## 7. Menu Bar Expansion

The menu bar is expanded to provide access to all three layers. New menus
shown in bold:

| Menu | Items |
|------|-------|
| File | Settings..., **Profiles →**, Quit |
| Radio | Discover..., Connect, Disconnect, Radio Info, **Radio Setup... →** |
| **View** | Pan Layout →, Add/Remove Pan, Band Plan →, Display Mode →, UI Scale →, Theme →, Minimal Mode, **Keyboard Shortcuts...** |
| **DSP** | NR ☑, NB ☑, ANF ☑, SNB ☑, BIN ☑, AGC → (Off/Long/Slow/Med/Fast), **EQ ☑**, **TNF ☑**, **PureSignal ☑**, **Diversity ☑** |
| **Band** | 160m..6m, WWV, GEN, XVTR → |
| **Mode** | LSB, USB, DSB, CWL, CWU, AM, SAM, FM, DIGL, DIGU, DRM, SPEC |
| **Containers** | New Container..., Container Settings..., Reset Default Layout, ──, [dynamic container list with show/hide checkboxes] |
| **Tools** | CWX Panel, DVK Panel, Memory Manager, **CAT Control...**, **TCI Server...**, **DAX Audio...**, **MIDI Mapping...**, **Macro Buttons...**, Network Diagnostics, Support Bundle |
| Help | Getting Started, NereusSDR Help, What's New, ──, About NereusSDR |

---

## 8. Wiring Tier Definitions

### Tier 1 — Wire Now (existing backend)

Controls that map to existing RadioModel, SliceModel, RxChannel, or
AppSettings functionality:

- All existing VfoWidget controls (freq, mode, filter, AGC mode/thresh, AF/RF gain, antenna, NB/NR/ANF)
- Spectrum/waterfall display settings (already in SpectrumOverlayMenu)
- Band selection (frequency + mode change via SliceModel)
- Audio output device (AudioEngine)
- Diagnostics/logging (SupportDialog controls relocated)
- Basic radio info display (from RadioConnection status)
- Alex HPF/LPF auto mode (already in RadioConnection)
- DDC dither/random (already sent in command packets)

### Tier 2 — Wire with Upcoming Phases

| Phase | Controls enabled |
|-------|-----------------|
| 3G-4..6 | Advanced meter items, container settings, interactive meters |
| 3I-1 | TX applet (MOX, TUN, drive, mic gain), speech processor, TX profiles |
| 3I-2 | CW applet (speed, sidetone, QSK), CWX applet |
| 3I-3 | Full TX DSP chain, EQ applet, VOX, compressor, CESSB, leveler |
| 3I-4 | PureSignal applet, PS setup controls |
| 3F | Multi-pan layout, DDC assignment, RX2, +RX button, display RX2 tab |
| 3-DAX | NereusDAX, DAX overlay button, VAC 1/2, digital applet, ASIO backend |

### Tier 3 — Wire Later

CAT/TCI/MIDI (3K), Protocol 1 (3L), Recording (3M), Skins (3H),
Andromeda, Discord, calibration, signal generator, firmware update,
profiles, OC outputs, custom color editors.

---

## 9. File Structure

New files created by this design:

```
src/gui/
├── SpectrumOverlayPanel.h/.cpp          ← Layer 1: left overlay
├── SetupDialog.h/.cpp                   ← Layer 3: setup dialog
├── SetupPage.h/.cpp                     ← Base class for setup pages
├── setup/                               ← Setup sub-pages
│   ├── GeneralPage.h/.cpp
│   ├── HardwarePage.h/.cpp
│   ├── AudioPage.h/.cpp
│   ├── DspPage.h/.cpp
│   ├── DisplayPage.h/.cpp
│   ├── TransmitPage.h/.cpp
│   ├── AppearancePage.h/.cpp
│   ├── CatNetworkPage.h/.cpp
│   ├── KeyboardPage.h/.cpp
│   ├── IntegrationsPage.h/.cpp
│   └── DiagnosticsPage.h/.cpp
├── applets/                             ← Layer 2: container applets
│   ├── AppletWidget.h/.cpp              ← Base class
│   ├── RxApplet.h/.cpp
│   ├── TxApplet.h/.cpp
│   ├── EqApplet.h/.cpp
│   ├── PhoneCwApplet.h/.cpp
│   ├── PureSignalApplet.h/.cpp
│   ├── DiversityApplet.h/.cpp
│   ├── CwxApplet.h/.cpp
│   ├── DvkApplet.h/.cpp
│   ├── FmApplet.h/.cpp
│   ├── DigitalApplet.h/.cpp
│   └── CatApplet.h/.cpp
src/audio/
├── NereusDAX.h/.cpp                     ← DAX abstraction
├── CoreAudioDAX.h/.cpp                  ← macOS backend
├── PipeWireDAX.h/.cpp                   ← Linux backend
├── WindowsDAX.h/.cpp                    ← Windows backend
└── AsioBackend.h/.cpp                   ← ASIO audio backend
```

---

## 10. Implementation Priority

This design is implemented as a **new phase** that can run in parallel with
Phase 3G-3 (current meter work) since it's primarily UI skeleton:

1. **SpectrumOverlayPanel** — left overlay buttons + flyout panels (Tier 1 controls wired)
2. **SetupDialog + tree navigation** — all 10 categories, ~45 pages as NYI skeletons
3. **AppletWidget base + RxApplet** — first container applet (Tier 1 controls wired)
4. **TxApplet + PhoneCwApplet + EqApplet** — skeleton applets (NYI until Phase 3I)
5. **Remaining applets** — PureSignal, Diversity, CWX, DVK, FM, Digital, CAT
6. **Menu bar expansion** — View, DSP, Band, Mode, Containers, Tools menus
7. **NereusDAX + ASIO** — audio routing (parallel with Phase 3-DAX)
8. **Wire Tier 1 controls** — connect all controls that have existing backend support
9. **Progressive Tier 2/3 wiring** — as each phase completes, light up corresponding controls
