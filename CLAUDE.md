# NereusSDR — Project Context for Claude

## Project Goal

Port **Thetis** (the OpenHPSDR / Apache Labs SDR console, written in C#) to a
**cross-platform C++20 application** using Qt6. The architectural template is
**AetherSDR** (a FlexRadio SmartSDR client). Target radios: all OpenHPSDR
Protocol 1 and Protocol 2 devices, including the Apache Labs ANAN line and
Hermes Lite 2.

**Critical implication:** The client does ALL signal processing (DSP, FFT,
demodulation). The radio is essentially an ADC/DAC with network transport.

---

## ⚠️ SOURCE-FIRST PORTING PROTOCOL (Read This Before Every Task)

NereusSDR is a **port**, not a reimagination. The Thetis codebase is the
authoritative source for all radio logic, DSP behavior, protocol handling,
constants, state machines, and feature behavior. **Do not guess. Do not
infer. Do not improvise.** Read the source, then translate it.

### The Rule: READ → SHOW → TRANSLATE

For every piece of logic you write that has a Thetis equivalent:

1. **READ** the relevant Thetis source file(s). Use `find`, `grep`, or `rg`
   to locate the C# code. The Thetis repo should be cloned at
   `../Thetis/` (relative to the NereusSDR root).
2. **SHOW** the original code before writing anything. State:
   `"Porting from [file]:[function/line range] — original C# logic:"` and
   quote or summarize the relevant section.
3. **TRANSLATE** the C# to C++20/Qt6 faithfully. Use AetherSDR patterns for
   the Qt6 structure (signals/slots, class layout, threading), but the
   **behavior and logic** must come from Thetis.

### What Counts As "Guessing" (NEVER Do These)

- Writing a function body without first reading the Thetis equivalent
- Assuming what WDSP function signatures, parameters, or return types look like
- Inventing enum values, constants, magic numbers, thresholds, or defaults
- Paraphrasing what a Thetis feature "probably does" based on its name
- Writing placeholder/stub logic with TODOs for things that exist in Thetis
- Assuming protocol message formats or byte layouts without reading the code
- "Improving" or "simplifying" Thetis logic without being asked to
- Using general DSP knowledge instead of the actual WDSP API calls Thetis makes

### Constants and Magic Numbers

Preserve ALL constants, thresholds, scaling factors, and magic numbers exactly
as they appear in Thetis. If Thetis uses `0.98f`, NereusSDR uses `0.98f`. If
Thetis uses `2048` as a buffer size, document where it came from and keep it.
Give constants a `constexpr` name but note the Thetis origin in a comment:

```cpp
// From Thetis console.cs:4821 — original value 0.98f
static constexpr float kAgcDecayFactor = 0.98f;
```

### WDSP Calls — Extra Caution

- Every WDSP function call must match the exact name, parameter order, and
  types from `Project Files/Source/wdsp/` in the Thetis repo
- Cross-reference against `Project Files/Source/Console/dsp.cs` (the C#
  P/Invoke declarations) for the managed-side signatures
- DSP parameter ranges, defaults, and scaling come from Thetis code, not
  from general knowledge or WDSP documentation
- When in doubt, read both the WDSP C source AND the Thetis C# callsite

### If You Can't Find the Source

**STOP AND ASK.** Say: "I cannot locate the Thetis source for [X]. Which
file or class should I look in?" Do NOT fabricate an implementation. It is
always better to ask than to guess wrong.

### The Two-Source Rule

| Question | Source |
| --- | --- |
| **What** does the code do? | Thetis (C# source) |
| **How** do we structure it in Qt6? | AetherSDR (C++20/Qt6 patterns) |

AetherSDR provides the **skeleton** (class structure, signals/slots, threading,
state management patterns). Thetis provides the **organs** (logic, algorithms,
constants, protocol handling, DSP flow, feature behavior).

### Thetis Source Layout Quick Reference

```
../Thetis/
├── Project Files/
│   └── Source/
│       ├── Console/          ← Main UI, radio logic, state management
│       │   ├── console.cs    ← Monster file: VFO, band, mode, DSP, display
│       │   ├── setup.cs      ← Setup dialog (hardware config, DSP params)
│       │   ├── display.cs    ← Spectrum/waterfall rendering
│       │   ├── audio.cs      ← Audio engine, VAC, portaudio
│       │   ├── cmaster.cs    ← Channel master (WDSP channel management)
│       │   ├── dsp.cs        ← WDSP P/Invoke declarations
│       │   ├── NetworkIO.cs  ← Protocol 1/2 network I/O
│       │   ├── protocol2.cs  ← Protocol 2 specific handling
│       │   └── ...
│       └── wdsp/             ← WDSP C source (DSP engine)
│           ├── channel.c     ← Channel create/destroy/exchange
│           ├── RXA.c         ← RX channel pipeline
│           ├── TXA.c         ← TX channel pipeline
│           └── ...
```

---

## AI Agent Guidelines

When helping with NereusSDR:

* Prefer C++20 / Qt6 idioms (std::ranges, concepts if clean, Qt signals/slots)
* Keep classes small and single-responsibility
* Use RAII everywhere (no naked new/delete)
* Comment non-obvious protocol decisions with protocol version (P1 vs P2)
* Never suggest Wine/Crossover workarounds — goal is native cross-platform
* Flag any proposal that would break the core RX path (I/Q → WDSP → audio)
* If unsure about protocol behavior → ask for pcap captures first
* **Use `AppSettings`, never `QSettings`** — see "Settings Persistence" below
* **Read `CONTRIBUTING.md`** for full contributor guidelines and coding conventions
* Reference OpenHPSDR protocol specs, not SmartSDR protocol

### Autonomous Agent Boundaries

AI agents may autonomously fix:

* **Bugs with clear root cause** — persistence missing, guard missing, crash fix
* **Protocol compliance** — matching OpenHPSDR protocol spec behavior
* **Build/CI fixes** — missing dependencies, platform compatibility

AI agents must **NOT** autonomously change:

* **Visual design** — colors, fonts, layout, theme
* **UX behavior** — how controls work, what clicks do, keyboard shortcuts
* **Architecture** — adding new threads, changing signal routing, new dependencies
* **Feature scope** — adding features beyond what the issue describes
* **Default values** — changing defaults that affect all users
* **DSP parameters or constants** — unless directly porting from Thetis source

When in doubt, implement the fix and note in the PR that design decisions need
maintainer review.

---

## C++ Style Guide

* **No `goto`** — use early returns, break, or restructure the logic
* **No raw `new`/`delete`** — use `std::unique_ptr`, `std::make_unique`, or Qt parent ownership
* **No `#define` macros for constants** — use `constexpr` or `static constexpr`
* **Braces on all control flow** — even single-line `if`/`else`/`for`/`while`
* **`auto` sparingly** — use explicit types unless the type is obvious from context
* **Naming**: classes `PascalCase`, methods/variables `camelCase`, constants `kPascalCase`, member variables `m_camelCase`
* **Platform guards**: use `#ifdef Q_OS_WIN` / `Q_OS_MAC` / `Q_OS_LINUX`, not `_WIN32` or `__APPLE__`
* **Don't remove code you didn't add** — review the diff before submitting
* **Atomic parameters for cross-thread DSP** — main thread writes via `std::atomic`, audio thread reads. Never hold a mutex in the audio callback.
* **Error handling**: log with `qCWarning(lcCategory)`, don't throw exceptions
* **Thetis origin comments**: when porting logic, add `// From Thetis [file]:[line or function]` comments

---

## Build

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
./build/NereusSDR
```

Dependencies: `qt6-base qt6-multimedia cmake ninja pkgconf fftw`

WDSP source is in `third_party/wdsp/` (TAPR v1.29 + linux_port.h for cross-platform).
FFTW3: system package on Linux/macOS, pre-built DLL on Windows (`third_party/fftw3/`).
First run generates FFTW wisdom (~15 min). Cached in `~/.config/NereusSDR/` for subsequent launches.

Current version: **0.1.4** (set in `CMakeLists.txt`).

---

## Architecture Quick Reference

Key source directories: `src/core/` (protocol, audio, DSP), `src/models/`
(RadioModel, SliceModel, etc.), `src/gui/` (MainWindow, SpectrumWidget, applets).

**Key classes:**

* `RadioModel` — central state, owns connection + all sub-models + WdspEngine
* `SliceModel` — per-receiver VFO state (freq, mode, filter, AGC, gains, antenna). Single source of truth.
* `PanadapterModel` — per-panadapter display state (center freq, bandwidth, dBm range). As of 3G-8, also owns the per-band grid storage (`BandGridSettings {dbMax, dbMin}` × 14 bands, global `gridStep`) and the current `band()` derived from `setCenterFrequency()` via `Band::bandFromFrequency()`. Emits `bandChanged(Band)` on boundary crossings and pushes the stored slot into `dBmFloor`/`dBmCeiling` automatically. `RadioModel::spectrumWidget()` / `fftEngine()` non-owning view hooks are set here by `MainWindow` so setup pages reach the renderer.
* `Band` (`src/models/Band.h`) — first-class 14-band enum (160m–6m + GEN + WWV + XVTR) with `bandLabel()`, `bandKeyName()` (AppSettings key suffix), `bandFromFrequency()` (IARU Region 2 lookup with WWV discrete centers), `bandFromUiIndex()` / `uiIndexFromBand()`. Added in 3G-8.
* `ReceiverManager` — DDC-aware receiver lifecycle, maps logical receivers to hardware DDCs; exposes DDC center frequency for CTUN pan positioning
* `RadioDiscovery` — OpenHPSDR radio discovery on UDP port 1024
* `RadioConnection` — Protocol 1 (UDP) and Protocol 2 (UDP multi-port) connections
* `WdspEngine` — WDSP lifecycle manager (wisdom, channels, impulse cache)
* `RxChannel` — per-receiver WDSP channel wrapper (fexchange2, NB, mode/filter/AGC, shift offset for CTUN demodulation)
* `AudioEngine` — QAudioSink output (Int16 stereo, timer-based drain)
* `FFTEngine` — FFTW3 spectrum computation (worker thread, I/Q → dBm bins)
* `SpectrumWidget` — GPU spectrum trace + waterfall display (QRhiWidget — Metal/Vulkan/D3D12); zoom via visibleBinRange() bin subsetting with m_ddcCenterHz/m_sampleRateHz
* `VfoWidget` — floating VFO flag (AetherSDR pattern): freq display, mode/filter/AGC tabs, antenna buttons
* `ContainerWidget` — dock/float/resize/axis-lock container shell (Thetis ucMeter equivalent)
* `FloatingContainer` — top-level window wrapper for floating containers (Thetis frmMeterDisplay equivalent)
* `ContainerManager` — singleton container lifecycle: 3 dock modes (panel/overlay/floating), axis-lock reposition, QSplitter, persistence
* `MeterWidget` — GPU meter renderer (QRhiWidget — 3 pipelines: background texture, vertex geometry, QPainter overlay); one per container, renders all MeterItems in single draw pass
* `MeterItem` — base class for composable meter elements (normalized 0-1 positioning, data binding, z-order); concrete types: BarItem (+ Edge mode), TextItem, ScaleItem, SolidColourItem, ImageItem, NeedleItem (+ ANANMM/CrossNeedle calibration extensions), SpacerItem, FadeCoverItem, LEDItem, HistoryGraphItem, MagicEyeItem, NeedleScalePwrItem, SignalTextItem, DialItem, TextOverlayItem, WebImageItem, FilterDisplayItem, RotatorItem, ButtonBoxItem (shared grid base), BandButtonItem, ModeButtonItem, FilterButtonItem, AntennaButtonItem, TuneStepButtonItem, OtherButtonItem, VoiceRecordPlayItem, DiscordButtonItem, VfoDisplayItem, ClockItem, ClickBoxItem, DataOutItem
* `ItemGroup` — composites N MeterItems into named presets; 35+ factory methods including S-Meter, Power/SWR, ANANMM (7-needle), CrossNeedle (dual fwd/rev), MagicEye, History, SignalText, and all TX bar meters
* `MeterPoller` — QTimer-based WDSP meter polling (100ms/10fps); calls RxChannel::getMeter(), pushes to bound MeterWidgets
* `AppSettings` — custom XML settings persistence (NOT QSettings)
* `MainWindow` — wires everything together, signal routing hub; uses QSplitter for spectrum + container panel
* `SpectrumOverlayPanel` — 10-button overlay panel on SpectrumWidget with 5 flyout sub-panels (display/filter/noise/spots/tools), auto-close
* `SetupDialog` — 47-page setup dialog across 10 categories with real controls
* `AppletPanelWidget` — fixed S-Meter header + scrollable applet body for Container #0
* `applets/` — 12 applets: RxApplet, TxApplet, PhoneCwApplet, EqApplet, FmApplet, DigitalApplet, PureSignalApplet, DiversityApplet, CwxApplet, DvkApplet, CatApplet, TunerApplet
* `StyleConstants.h` — shared color palette, fonts, widget style constants
* `HGauge` — horizontal bar gauge widget
* `ComboStyle` — styled combo box shared across applets
* `ColorSwatchButton` (`src/gui/ColorSwatchButton.h`) — reusable color picker button: QPushButton subclass, QColorDialog with alpha, `colorChanged(QColor)` signal, static `colorToHex` / `colorFromHex` helpers for AppSettings `"#RRGGBBAA"` round-trip. Added in 3G-8; used by 9 call sites across the Display setup pages (S11/S13 trace colours, W10 waterfall low colour, G6 band edge, G9–G13 grid/text/zero-line colours).

**Thread Architecture:**

| Thread | Components |
| --- | --- |
| **Main** | GUI rendering, RadioModel, all sub-models, user input |
| **Connection** | RadioConnection (UDP I/O, protocol framing) |
| **Audio** | AudioEngine + WdspEngine (I/Q processing, DSP, audio output) |
| **Spectrum** | FFT computation, waterfall data generation |

Cross-thread communication uses auto-queued signals exclusively.
RadioModel owns all sub-models on the main thread. Never hold a mutex in the
audio callback.

### Data Flow (Phase 3E + CTUN + Zoom — VERIFIED WORKING)

```
Radio (ADC) → UDP port 1037 (DDC2) → P2RadioConnection
    ↓ iqDataReceived(ddcIndex=2, interleaved float I/Q)
ReceiverManager::feedIqData(2) → maps DDC2 → receiver 0
    ↓ iqDataForReceiver(0, samples)
RadioModel lambda:
    ├── emit rawIqData(samples) → FFTEngine → SpectrumWidget
    ├── Deinterleave I/Q, accumulate 238 → 1024 samples
    └── RxChannel::processIq() → fexchange2() → decoded audio
        ↓
    AudioEngine::feedAudio() → float→int16 → m_rxBuffer
        ↓ 10ms timer drain
    QAudioSink (48kHz stereo Int16) → Speakers

FFT → Display (with zoom):
    FFTEngine emits N bins (full DDC bandwidth)
    → SpectrumWidget::updateSpectrum() stores in m_smoothed
    → visibleBinRange(N) maps m_centerHz ± m_bandwidthHz/2 to bin indices
      using m_ddcCenterHz + m_sampleRateHz for bin-to-frequency mapping
    → GPU/CPU renderer iterates only [firstBin..lastBin], stretched to full display
    → pushWaterfallRow() writes only visible bin subset to waterfall texture

User zooms (freq scale drag or Ctrl+scroll):
    m_bandwidthHz changes → visibleBinRange() narrows → immediate visual zoom
    On mouse release → bandwidthChangeRequested → MainWindow replans FFT size
    → FFTEngine delivers more bins → sharper resolution at new zoom level

User tunes VFO:
    VfoWidget (wheel/click/edit) → emit frequencyChanged(hz)
    → SliceModel::setFrequency(hz)
    → ReceiverManager::setReceiverFrequency(0, hz)
      → hardwareFrequencyChanged(DDC2, hz)
      → P2RadioConnection::setReceiverFrequency(2, hz) + Alex HPF/LPF update
      → sendCmdHighPriority() → radio retunes DDC NCO
```

---

## Key Implementation Patterns

### Settings Persistence (AppSettings — NOT QSettings)

**IMPORTANT:** Do NOT use `QSettings` anywhere in NereusSDR. All client-side
settings are stored via `AppSettings` (`src/core/AppSettings.h`), which writes
an XML file at `~/.config/NereusSDR/NereusSDR.settings`. Key names use
PascalCase (e.g. `LastConnectedRadioMac`, `DisplayFftAverage`). Boolean
values are stored as `"True"` / `"False"` strings.

```
auto& s = AppSettings::instance();
s.setValue("MyFeatureEnabled", "True");
bool on = s.value("MyFeatureEnabled", "False").toString() == "True";
```

### Radio-Authoritative Settings Policy

**Radio-authoritative (do NOT persist):** ADC attenuation, preamp, TX power,
antenna selection, hardware sample rate.

**Client-authoritative (persist in AppSettings):** VFO frequency, mode, filter,
DSP settings (AGC, NR, NB, ANF), layout arrangement, UI preferences, display
preferences. OpenHPSDR radios don't store per-slice state.

### GUI↔Model Sync (No Feedback Loops)

* Model setters emit signals → RadioConnection sends protocol commands
* Protocol responses update models via `applyStatus()` or equivalent
* Use `m_updatingFromModel` guard or `QSignalBlocker` to prevent echo loops
* Follow AetherSDR's proven pattern exactly

---

## Documentation Index

### Master Plan & Progress

| Document | Description |
| --- | --- |
| [docs/MASTER-PLAN.md](docs/MASTER-PLAN.md) | Full phased roadmap, menu bar layout, GUI container mapping (Thetis → NereusSDR), skin system design, progress tracking |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contributor guidelines, coding conventions, PR process |
| [STYLEGUIDE.md](STYLEGUIDE.md) | Applet color palette, button states, gauge zones, slider/combo styling |
| [CHANGELOG.md](CHANGELOG.md) | Version history and per-phase feature additions |

### Architecture Design Docs (`docs/architecture/`)

| Document | Scope |
| --- | --- |
| [overview.md](docs/architecture/overview.md) | Layer diagram, thread architecture, RX/TX data flow overview |
| [radio-abstraction.md](docs/architecture/radio-abstraction.md) | P1/P2 connections, MetisFrameParser, ReceiverManager, C&C register map, protocol details |
| [multi-panadapter.md](docs/architecture/multi-panadapter.md) | PanadapterStack (5 layouts), PanadapterApplet, wirePanadapter(), FFTRouter |
| [gpu-waterfall.md](docs/architecture/gpu-waterfall.md) | FFTEngine, SpectrumWidget, QRhi shaders, overlay system, color schemes |
| [wdsp-integration.md](docs/architecture/wdsp-integration.md) | RxChannel/TxChannel wrappers, PureSignal, thread safety, WDSP channel lifecycle |
| [skin-compatibility.md](docs/architecture/skin-compatibility.md) | SkinParser, extended skin format, Thetis import, 4-pan support |
| [adc-ddc-panadapter-mapping.md](docs/architecture/adc-ddc-panadapter-mapping.md) | ADC->DDC->Receiver->FFT->Pan signal chain, Thetis UpdateDDCs() analysis, per-board DDC assignment, bandwidth limits |
| [ctun-zoom-design.md](docs/architecture/ctun-zoom-design.md) | CTUN zoom bin subsetting: visibleBinRange(), hybrid FFT replan, DDC center tracking |

### Implementation Plans (`docs/architecture/phase*-plan.md`)

| Plan | Phase | Status |
| --- | --- | --- |
| [phase3d-spectrum-waterfall-plan.md](docs/architecture/phase3d-spectrum-waterfall-plan.md) | 3D: GPU Spectrum & Waterfall | **Complete** |
| [ctun-zoom-plan.md](docs/architecture/ctun-zoom-plan.md) | 3E: CTUN Zoom Bin Subsetting | **Complete** |
| [phase3g1-container-infrastructure-plan.md](docs/architecture/phase3g1-container-infrastructure-plan.md) | 3G-1: Container Infrastructure | **Complete** |
| [phase3g2-meter-widget.md](docs/superpowers/plans/2026-04-10-phase3g2-meter-widget.md) | 3G-2: MeterWidget GPU Renderer | **Complete** |
| [phase3g3-core-meter-groups.md](docs/superpowers/plans/2026-04-10-phase3g3-core-meter-groups.md) | 3G-3: Core Meter Groups | **Complete** |
| [phase3-ui-skeleton-plan-v2.md](docs/architecture/phase3-ui-skeleton-plan-v2.md) | 3-UI: Full UI Skeleton | **Complete** |
| [phase3g4-g6-advanced-meters-design.md](docs/architecture/phase3g4-g6-advanced-meters-design.md) | 3G-4/5/6: Advanced Meters Design Spec | **Approved** |
| [phase3g4-advanced-meters-plan.md](docs/architecture/phase3g4-advanced-meters-plan.md) | 3G-4: Advanced Meter Items | **Complete** |
| [phase3g5-interactive-meters-plan.md](docs/architecture/phase3g5-interactive-meters-plan.md) | 3G-5: Interactive Meter Items | **Complete** |
| [phase3g6a-plan.md](docs/architecture/phase3g6a-plan.md) | 3G-6 (one-shot): Full Thetis Parity + MMIO | **Complete** |
| [phase3g8-rx1-display-parity-plan.md](docs/architecture/phase3g8-rx1-display-parity-plan.md) | 3G-8: RX1 Display Parity (47 Spectrum/Waterfall/Grid controls, Band enum, per-band grid, GPU polish) | **Complete** |
| [phase3g8-verification/README.md](docs/architecture/phase3g8-verification/README.md) | 3G-8: 47-control manual verification matrix | Matrix drafted |
| [phase3f-multi-panadapter-plan.md](docs/architecture/phase3f-multi-panadapter-plan.md) | 3F: Multi-Panadapter + DDC Assignment | Planning (after 3I-4) |

### Protocol Reference (`docs/protocols/`)

| Document | Scope |
| --- | --- |
| [openhpsdr-protocol1.md](docs/protocols/openhpsdr-protocol1.md) | P1 summary + pointer to capture reference; Thetis P1 source map |
| [openhpsdr-protocol1-capture-reference.md](docs/protocols/openhpsdr-protocol1-capture-reference.md) | Annotated HL2↔Thetis capture: discovery, start/stop, EP6/EP2 frames, C0 maps, cadence, band/TX traces, HL2 quirks, Phase 3L checklist |
| [openhpsdr-protocol2.md](docs/protocols/openhpsdr-protocol2.md) | P2 UDP multi-port, command packets, per-DDC I/Q streams |

### Phase 1 Analysis Docs (`docs/phase1/`)

| Document | Key Findings |
| --- | --- |
| 1A: AetherSDR Analysis | RadioModel hub, auto-queued signals, worker threads, AppSettings XML, GPU rendering via QRhi |
| 1B: Thetis Analysis | Dual-thread DSP (RX1/RX2), pre-allocated receivers, one-way protocol, skin system |
| 1C: WDSP Analysis | 256 API functions, channel-based DSP, fexchange2() for I/Q, PureSignal feedback loop |

### Current Phase: 3I complete — Radio Connector & Radio-Model Port

| Phase | Goal | Status |
| --- | --- | --- |
| 3A: Radio Connection | Connect to ANAN-G2 via P2, receive I/Q | **Complete** |
| 3B: WDSP Integration | Process I/Q through WDSP, audio output | **Complete** |
| 3C: macOS Build | Cross-platform WDSP build + wisdom crash fix | **Complete** |
| 3D: Spectrum Display | GPU spectrum + waterfall (QRhi Metal/Vulkan/D3D12) | **Complete** |
| 3E: VFO + Multi-RX Foundation | VFO controls, CTUN panadapter, rewired I/Q pipeline | **Complete** |
| **3G-1: Container Infrastructure** | **Dock/float/resize/persist container shells** | **Complete** |
| **3G-2: MeterWidget GPU Renderer** | **QRhi-based meter rendering engine** | **Complete** |
| **3G-3: Core Meter Groups** | **S-Meter, Power/SWR, ALC presets** | **Complete** |
| **3-UI: Full UI Skeleton** | **12 applets, 9-menu bar, SetupDialog (47pp), SpectrumOverlayPanel** | **Complete** |
| **3G-4: Advanced Meter Items** | **12 item types + ANANMM/CrossNeedle presets + Edge mode** | **Complete** |
| **3G-5: Interactive Meter Items** | **14 interactive items + mouse forwarding + ButtonBoxItem base** | **Complete** |
| **3G-6: Container Settings Dialog (one-shot)** | **3-column dialog, 31 per-item editors, MMIO subsystem (4 transports + JSON/XML/RAW), Edit Container submenu** | **Complete** |
| **3G-7: Polish** | **MMIO clone-path bug fix + 5 subclass accessor gap fills + NeedleItemEditor QGroupBox grouping** | **Complete** |
| **3G-8: RX1 Display Parity** | **47 Spectrum/Waterfall/Grid controls wired (Setup → Display), `Band` enum + per-band grid on PanadapterModel, `BandButtonItem` 12→14, GPU polish: overlay cache invalidation, waterfall chrome in overlay texture, peak hold VBO, fill/gradient/cal-offset in vertex gen** | **Complete (PR #8)** |
| **3I: Radio Connector & Radio-Model Port** | **P1 full family (Atlas/Hermes/HermesII/Angelia/Orion/HL2), BoardCapabilities registry, ConnectionPanel, HardwarePage 9-tab capability-gated, per-MAC persistence, mi0bot RadioDiscovery port, RadioConnectionError taxonomy** | **Complete** |
| 3G-9: Display Refactor | Source-first audit + Thetis-first tooltip port + slider/spinbox refactor (3G-9a merged as PR #25); 3G-9b smooth defaults + Clarity palette and 3G-9c adaptive auto-tune planned | **3G-9a Complete** |
| **3G-10: RX DSP Parity + AetherSDR Flag Port** | **Stage 1 complete: widget library + SliceModel stubs + VfoWidget S-meter (PR #28), tab rewrite + 4×2 DSP grid + mode containers + AudioTab AGC 5-button row + tooltip coverage test (PR #30). Stage 2 (WDSP wiring + per-slice-per-band persistence) not yet started.** | **Stage 1 Complete** |
| **3G-12: Step Attenuator & ADC Overload** | **StepAttenuatorController (Classic + Adaptive auto-att), P1/P2 adcOverflow emission, ADC OVL status badge, Setup→General→Options page, RxApplet ATT/S-ATT row, per-model preamp items from Thetis SetComboPreampForHPSDR, stepAttMaxDb (31/61), per-MAC persistence, 9 tests. PR #34.** | **In flight** |
| 3M-1: Basic SSB TX (was 3I-TX) | TxChannel, mic input, MOX state machine, I/Q output | Next |
| 3M-2: CW TX (was 3I-CW) | Sidetone, firmware keyer, QSK/break-in | Planned |
| 3M-3: TX Processing | 18-stage TXA chain + TX-side RX DSP additions | Planned |
| 3M-4: PureSignal (was 3I-PS) | Feedback DDC, calcc/IQC engine, PSForm, AmpView | Planned |
| 3F: Multi-Panadapter | DDC assignment (incl. PS states), FFTRouter, PanadapterStack, enable RX2 | Planned |
| 3H: Skins | Thetis-inspired skin format, 4-pan, legacy import | Planned |
| 3J: TCI + Spots | TCI server, DX Cluster/RBN clients, spot overlay | Planned |
| 3K: CAT/rigctld | 4-channel rigctld, TCP CAT server | Planned |
| 3L: HL2 ChannelMaster.dll port | HL2 IoBoardHl2 I2C-over-ep2 wire encoding, bandwidth monitor full port | Planned |
| 3M: Recording | WAV record/playback, I/Q record, scheduled | Planned |
| **3N: Packaging** | **Consolidated `release.yml` (prepare → build×3 → sign-and-publish), `/release` skill, GPG-signed alpha builds: Linux AppImage ×2 archs, macOS Apple Silicon DMG, Windows portable ZIP + NSIS installer** | **Complete** |

---

## Reference Repositories

1. **AetherSDR** — `https://github.com/ten9876/AetherSDR`
   * Architectural template: radio abstraction, state management, signal/slot patterns, GPU rendering, multi-pan layout
2. **Thetis** — `https://github.com/ramdor/Thetis`
   * Feature source: every Thetis capability must be accounted for and ported
   * **Clone to `../Thetis/` relative to NereusSDR root**
3. **WDSP** — `https://github.com/TAPR/OpenHPSDR-wdsp`
   * DSP engine: all signal processing functions
