# NereusSDR — Master Implementation Plan

## Context

NereusSDR is an independent cross-platform SDR client deeply informed by the workflow, feature set, and operating style of Thetis, reimagined with a new GUI, a modernized architecture, and native support for macOS, Linux, and Windows. We've completed the scaffolding (Phase 0), architectural analysis (Phase 1), and architecture design (Phase 2). Now moving to Phase 3: Implementation.

---

## Progress Summary

### Completed: Phase 0 — Scaffolding
- 69 files, 14,126 lines across the full project skeleton
- CI green (Ubuntu 24.04, Qt6, cmake + ninja)
- 7 CI workflows (build, CodeQL, AppImage, Windows, macOS, sign-release, Docker CI image)
- 16 compilable source stubs (AppSettings, RadioDiscovery, RadioConnection, WdspEngine, AudioEngine, RadioModel, SliceModel, PanadapterModel, MeterModel, TransmitModel, MainWindow)
- Full documentation (README, CLAUDE.md, CONTRIBUTING, STYLEGUIDE)

### Completed: Phase 1 — Architectural Analysis
| Doc | Lines | Key Findings |
|-----|-------|-------------|
| 1A: AetherSDR | 234 | RadioModel hub, auto-queued signals, worker threads, AppSettings XML, GPU rendering via QRhi |
| 1B: Thetis | 554 | Dual-thread DSP (RX1/RX2), pre-allocated receivers, one-way protocol, skin system (JSON+XML+PNG) |
| 1C: WDSP | 1,064 | 256 API functions, channel-based DSP, fexchange2() for I/Q, PureSignal feedback loop |

### Completed: Phase 2 — Architecture Design
| Doc | Lines | Scope |
|-----|-------|-------|
| 2A: Radio Abstraction | 1,762 | P1/P2 connections, MetisFrameParser, ReceiverManager, C&C register map, phase word math |
| 2B: Multi-Panadapter | 692 | PanadapterStack (5 layouts), PanadapterApplet, wirePanadapter(), slice-to-pan, FFTRouter |
| 2C: GPU Waterfall | 1,571 | FFTEngine (FFTW3), QRhiWidget, ring-buffer waterfall, 6 GLSL shaders, overlay system |
| 2D: Skin Compatibility | 864 | SkinParser (ZIP/XML/PNG), SkinRenderer, control mapping (70+ controls), remote servers |
| 2E: WDSP Integration | 1,968 | RxChannel/TxChannel wrappers, PureSignal, thread safety, channel lifecycle, meter/spectrum |
| 2F: ADC-DDC-Pan Mapping | 310 | Full ADC->DDC->Receiver->FFT->Pan signal chain, Thetis UpdateDDCs() analysis, per-board DDC assignment, bandwidth limits |

### Completed: Phase 3A — Radio Connection (P2 / ANAN-G2)
- P2RadioConnection faithfully ported from Thetis ChannelMaster/network.c
- Single UDP socket matching Thetis `listenSock` pattern
- P2 discovery (60-byte packet, byte[4]=0x02) on all network interfaces
- CmdGeneral/CmdRx/CmdTx/CmdHighPriority byte-for-byte from Thetis
- I/Q data streaming confirmed: DDC2, 1444 bytes/packet, 238 samples at 48kHz
- ConnectionPanel dialog with discovered radio list, connect/disconnect
- LogManager with runtime category toggles, AppSettings persistence
- SupportDialog with log viewer, category checkboxes, support bundle creation
- Status bar with live connection state indicator
- Auto-reconnect to last connected radio via saved MAC
- Key findings from pcap analysis:
  - WDT=1 (watchdog timer) required in CmdGeneral for radio to stream
  - DDC2 enable (bit 2) is the primary RX for ANAN-G2 (from Thetis UpdateDDCs)
  - Radio sends I/Q from port 1035, status from 1025, mic from 1026
  - IPv4 address must strip ::ffff: prefix for writeDatagram to work
- 12 new files, ~3500 lines of new code
- Verified with ANAN-G2 (Orion MkII, FW 27) at 192.168.109.45

### Completed: Phase 3B — WDSP Integration + Audio Pipeline
- WDSP v1.29 built as static library in third_party/wdsp/
- Cross-platform via linux_port.h/c (Windows/Linux/macOS)
- RxChannel wrapper: I/Q accumulation (238→1024), fexchange2(), NB1/NB2
- AudioEngine: QAudioSink 48kHz stereo Int16, 10ms timer drain
- Full RX pipeline verified: Radio ADC → UDP I/Q → WDSP → speakers
- FFTW wisdom generation with progress dialog, cached for subsequent launches
- Audio device selection and persistence via AppSettings

### Completed: Phase 3C — macOS Build + Crash Fix
- WDSP linux_port.h: added stdlib.h, string.h, fcntl.h, LPCRITICAL_SECTION
- ARM64 flush-to-zero guard, AVRT stubs, ResetEvent, AllocConsole/FreeConsole
- Fixed use-after-free crash: wisdom poll timer accessing deleted QThread
- Builds and runs on macOS Apple Silicon (commit bdb55e0)

### Completed: Phase 3D — GPU Spectrum & Waterfall
- FFTEngine: FFTW3 float-precision on dedicated spectrum worker thread
- SpectrumWidget: QRhiWidget GPU rendering via Metal (macOS) / Vulkan / D3D12
- 3 GPU pipelines: waterfall (ring-buffer texture), spectrum (vertex-colored), overlay (QPainter→texture)
- 6 GLSL 4.40 shaders compiled to QSB via qt_add_shaders()
- 4096-point FFT, Blackman-Harris 4-term window, 30 FPS rate limiting
- Waterfall scroll direction ported from Thetis display.cs:7719 pattern
- AetherSDR default color scheme + Thetis Enhanced/Spectran/BlackWhite schemes
- Waterfall color mapping ported from Thetis display.cs:6889 (low/high threshold)
- VFO marker (orange), filter passband overlay (cyan), cursor frequency readout
- Mouse interaction: scroll zoom, drag ref level, click-to-tune, Ctrl+scroll bandwidth
- Right-click SpectrumOverlayMenu: color scheme, gain, black level, fill, ref level, dyn range
- Display settings persistence via AppSettings (per-pan keys)
- Phase word NCO fix from pcap analysis (Hz→phase word conversion)
- Alex band filters (80/60m BPF), dither/random enabled on all ADCs
- Signal routing: RadioModel::rawIqData → FFTEngine → SpectrumWidget
- CPU fallback preserved under #ifndef NEREUS_GPU_SPECTRUM
- Verified: live spectrum + waterfall + audio on 75m LSB from ANAN-G2

### Completed: Phase 3E — VFO & Controls + Multi-Receiver Foundation
- **SliceModel enriched** — DSPMode enum (was QString), AGCMode, per-mode filter defaults from Thetis InitFilterPresets (console.cs:5180-5575), tuning step, AF/RF gain, RX/TX antenna, panId, receiverIndex
- **Floating VFO flag widget** (AetherSDR pattern) — 250px transparent panel, child of SpectrumWidget
  - Header: RX antenna (blue), TX antenna (red) with dropdown, filter width, TX badge, slice badge (A/B/C/D colors)
  - Frequency: 26px monospace "14.225.000" format, double-click to edit, mouse wheel to tune
  - Tab bar: Audio (AF gain + AGC combo), DSP (NB/NR/ANF toggles), Mode (mode combo + dynamic filter presets + RF gain), X/RIT (stub)
  - Mode-dependent positioning: USB family → flag RIGHT of marker, LSB family → LEFT
  - Per-slice color table: A=cyan #00d4ff, B=magenta, C=green, D=yellow
- **Signal wiring** — bidirectional VfoWidget ↔ SliceModel ↔ RxChannel/RadioConnection with m_updatingFromModel guards
- **Click-to-tune** wired: SpectrumWidget::frequencyClicked → SliceModel::setFrequency → RadioConnection
- **Scroll-to-tune** on spectrum: plain scroll = tune by stepHz, Ctrl+scroll = ref level, Ctrl+Shift+scroll = zoom
- **Alex filter registers** — dynamic HPF/LPF selection based on frequency (ported from Thetis console.cs:6830-7234)
  - HPF: bypass/1.5M/6.5M/9.5M/13M/20M/6M-preamp breakpoints
  - LPF: 160m/80m/60-40m/30-20m/17-15m/12-10m/6m breakpoints
  - Antenna: ANT1/ANT2/ANT3 selection via Alex register bits 24-26
  - Register encoding: Alex0 (bytes 1432-1435) + Alex1 (bytes 1428-1431) in CmdHighPriority
- **I/Q pipeline rewired through ReceiverManager** — DDC-aware routing
  - ReceiverManager maps DDC2 → receiver 0 for ANAN-G2 (from Thetis UpdateDDCs)
  - Explicit DDC mapping via setDdcMapping() (no more sequential auto-assign)
  - ADC assignment per receiver via setAdcForReceiver()
  - Signal chain: P2RadioConnection → iqDataReceived(DDC2) → ReceiverManager::feedIqData(2) → iqDataForReceiver(0) → WDSP → AudioEngine
- **Settings persistence** — VfoFrequency, VfoDspMode, VfoFilterLow/High, VfoAgcMode, VfoStepHz, VfoAfGain, VfoRfGain, VfoRxAntenna, VfoTxAntenna (coalesced 500ms saves)
- **FFTW3 float DLL fix** — added libfftw3f-3.dll + import library for Windows FFTEngine build
- **GPU overlay fix** — markOverlayDirty() on mouseMoveEvent for cursor frequency tracking
- No hardcoded frequencies, modes, or filters remain — all state flows from SliceModel
- Verified: dynamic tuning + mode switching + filter presets + audio on ANAN-G2 via Windows D3D11

### Post-3E Enhancement: CTUN Panadapter (commit 3f2283e)
- **CTUN mode** (default on): pan center and VFO are fully independent — SmartSDR/AetherSDR style
  - Click/scroll/passband-drag tunes VFO within fixed pan; DDC frequency locked at pan center
  - Waterfall drag pans the view and retunes DDC NCO
  - WDSP shift offsets audio demodulation when VFO ≠ DDC center
  - Off-screen VFO indicator with double-click to recenter
  - Band jumps auto-recenter; CTUN toggle in right-click overlay menu
- **FFT fixes**: full N-bin output (was only positive half), FFT-shift + mirror for correct sideband orientation
- **Alex BPF board** fully enabled (byte 59 in CmdGeneral had missing enable flag)
- **VfoWidget improvements**: AetherSDR-style floating control buttons (close/lock/record/play), green toggle filter preset buttons with exclusive selection
- Files modified: `FFTEngine.cpp`, `P2RadioConnection.cpp`, `ReceiverManager.h/.cpp`, `RxChannel.h/.cpp`, `MainWindow.cpp`, `SpectrumWidget.h/.cpp`, `SpectrumOverlayMenu.h/.cpp`, `VfoWidget.h/.cpp`

### Post-3E Enhancement: CTUN Zoom — Bin Subsetting (commit a4568c2)
- **Zoom via bin subsetting** — `visibleBinRange()` maps display window (m_centerHz ± m_bandwidthHz/2) to FFT bin indices using m_ddcCenterHz + m_sampleRateHz
- **GPU + CPU + waterfall** all render only the visible bin subset, stretched to full display width
- **Hybrid zoom** — smooth bin subsetting during freq scale bar drag, FFT replan on mouse release for sharp resolution
- **Waterfall preserves** existing rows across zoom changes (new rows at current scale)
- **DDC center tracking** — MainWindow wires m_ddcCenterHz on init, band jumps, and CTUN pan drags
- **Zoom direction** — drag right to zoom in, left to zoom out; Ctrl+scroll also zooms
- Design: `docs/architecture/ctun-zoom-design.md`, Plan: `docs/architecture/ctun-zoom-plan.md`
- Files modified: `SpectrumWidget.h/.cpp`, `MainWindow.cpp`

### Completed: Phase 3-UI — Full UI Skeleton

**Applets (12 total, 150+ control widgets):**
- RxApplet — mode, AGC, AF/RF gain, filter presets; Tier 1 wired to SliceModel
- TxApplet — mic gain, drive, tune power, compression, DEX
- PhoneCwApplet — voice/CW macro keys, VOX, CW speed/weight
- EqApplet — 10-band RX/TX graphic equalizer
- FmApplet — FM deviation, sub-tone, repeater offset
- DigitalApplet — digital mode settings (baud, shift, encoding)
- PureSignalApplet — PureSignal calibrate, feedback level indicator
- DiversityApplet — diversity RX phase/gain controls
- CwxApplet — CW message memory keyer
- DvkApplet — digital voice keyer playback controls
- CatApplet — CAT/rigctld port configuration
- TunerApplet — antenna tuner control (tune, bypass, memory)

**Spectrum Overlay Panel:**
- `SpectrumOverlayPanel` — 10-button overlay panel on SpectrumWidget with 5 flyout sub-panels (display, filter, noise, spots, tools); auto-close on outside click

**Menu Bar:** 9 menus (File, Radio, View, DSP, Band, Mode, Containers, Tools, Help), ~60 items wired to MainWindow

**Status Bar:** Double-height (46px) with UTC clock, radio info, TX/RX indicators, signal level

**Setup Dialog:** `SetupDialog` — 47 pages across 10 categories with real controls

**Applet Panel:** `AppletPanelWidget` — fixed S-Meter header + scrollable applet body for Container #0

**Infrastructure:**
- `StyleConstants.h` — shared color palette, fonts, widget style constants
- `HGauge` — horizontal bar gauge widget
- `ComboStyle` — styled combo box shared across applets

**Container / Persistence fixes:**
- Content, pin-on-top state, and floating position all persist across restarts
- Container minimum width 260px enforced in all dock modes

**S-Meter enhancements:**
- Dynamic resize with aspect-ratio scaling and full-width arc
- SMeterWidget ported directly from AetherSDR for pixel-identical fidelity

**RxApplet Tier 1 wired:** mode, AGC, AF gain, and filter presets fully wired to SliceModel

### Up Next (parallel pair after v0.2.3)
- **Phase 3Q — Connection Workflow Refactor** (in progress on `feature/phase3q-connection-workflow-refactor`; chrome layer landing first). Triggered by user report of WireGuard-tunneled HL2 not connecting via manual entry. Spec: [docs/architecture/2026-04-26-connection-workflow-refactor-design.md](architecture/2026-04-26-connection-workflow-refactor-design.md). The shell-chrome / status-bar / dashboard / SVG-icon work + ship-default calibration (commit c656fad + follow-ups) is the first PR; the unicast-probe + ConnectionPanel + AddRadioDialog rebuild + spectrum disconnect overlay land in subsequent PRs on the same branch.
- **Phase 3M-1 — Basic SSB TX** (next major epic). TxChannel WDSP wrapper, MOX state machine, TX I/Q output. No file overlap with 3Q.

### CI Status: GREEN
- Build passes on Ubuntu 24.04 with Qt6, cmake, ninja, fftw3
- Windows local build passes with Qt 6.11.0 / MinGW 13.1
- macOS Apple Silicon build passes (local, commit 39e35a6)

---

## Objective Cross-Check (Project Brief vs Plan)

| Project Brief Objective | Plan Coverage | Status |
|---|---|---|
| Port Thetis from C# to Qt6/C++20 | Full architecture designed | Phase 1+2 done |
| Cross-platform, GPU-accelerated SDR console | QRhi GPU rendering (2C) | **3D complete** |
| Preserve full feature set of Thetis | 35 panels + 11 groups mapped to container item types | Design done |
| Multi-panadapter (up to 4) | PanadapterStack with 5 layouts (2B), ADC-DDC-Pan chain (2F) | Design done |
| Waterfall fluidity | Client-side FFT + ring-buffer GPU waterfall (2C) | **3D complete** |
| Protocol 1 and Protocol 2 support | P2 first (ANAN-G2), P1 later (2A) | **P2 working** |
| WDSP integration (100% feature parity) | 256 API functions mapped, RxChannel/TxChannel designed (2E) | **3B complete** |
| Legacy skin compatibility | Extended skin format + Thetis import (2D) | Design done |
| Configurable containers (Thetis multi-meter) | Unified container system (float/dock/axis-lock) | Design done |
| PureSignal PA linearization | Feedback RX channel + pscc() loop designed (2E), DDC sync (2F) | Design done |
| TCI protocol | Planned as Phase 3J | Not started (integration points reserved in 3O) |
| Cross-platform packaging | CI workflows in place (AppImage, Windows, macOS) | **3N complete** — `release.yml` + `/release` skill; v0.2.3 shipped GPG-signed across all three platforms |
| AetherSDR architecture patterns | RadioModel hub, signal/slot, worker threads, AppSettings | Adopted |
| Radio-authoritative state | Designed per AetherSDR pattern | Adopted |
| Multi-receiver ADC/DDC mapping | Full signal chain analyzed (2F), UpdateDDCs() porting needed | Design done |
| **VAX audio routing + Thetis VAC/cmASIO parity** | **Phase 3O spec at `docs/architecture/2026-04-19-vax-design.md`** | **Designed 2026-04-19, impl planned** |

**All project brief objectives are covered.** Feature gap analysis completed 2026-04-10
(see `docs/architecture/reviews/2026-04-10-plan-review.md` for full audit). Phase 3O
VAX design added 2026-04-19.

---

## Phase 3 — Implementation (Named Phases)

### Phase 3A: Radio Connection (P2 — ANAN-G2) ✅ COMPLETE
**Goal:** Connect to an ANAN-G2 (Protocol 2) radio, receive raw I/Q data.

**Hardware:** ANAN-G2 (Orion MkII board, FW 27) at 192.168.109.45 via ZeroTier VPN.

**Files created/modified:**
- `src/core/LogCategories.h/.cpp` — **new** — LogManager with runtime category toggles
- `src/core/SupportBundle.h/.cpp` — **new** — diagnostic archive (logs, system/radio info)
- `src/core/RadioDiscovery.h/.cpp` — BoardType/ProtocolVersion enums, P2 discovery, multi-interface broadcast
- `src/core/RadioConnection.h/.cpp` — abstract base with factory, worker thread pattern
- `src/core/P2RadioConnection.h/.cpp` — **new** — faithfully ported from Thetis ChannelMaster/network.c
- `src/core/ReceiverManager.h/.cpp` — **new** — logical-to-hardware receiver mapping
- `src/models/RadioModel.h/.cpp` — worker thread (moveToThread+init), signal wiring, teardown
- `src/gui/ConnectionPanel.h/.cpp` — **new** — discovered radio list, connect/disconnect UI
- `src/gui/SupportDialog.h/.cpp` — **new** — log viewer, category toggles, bundle creation
- `src/gui/MainWindow.h/.cpp` — menus, status bar, auto-reconnect

**Protocol corrections (vs original architecture doc):**
- P2 is **UDP-only** on multiple ports, not TCP+UDP as originally assumed
- Single socket for all communication (matching Thetis `listenSock`)
- P2 discovery uses 60-byte packet with byte[4]=0x02, NOT the P1 0xEF 0xFE format
- ANAN-G2 uses DDC2 (bit 2) as primary receiver, not DDC0 (from Thetis UpdateDDCs)
- Watchdog timer (WDT=1) MUST be enabled in CmdGeneral or radio won't stream
- CmdGeneral byte 37 = 0x08 (phase word flag) per Thetis

**Known issues for future work:**
- Sequence errors over ZeroTier VPN (packets arrive slightly out of order)
- Not sending TX I/Q (port 1029) or audio (port 1028) silence frames
- DDC mapping hardcoded — should port full UpdateDDCs() from Thetis console.cs:8186

Key design reference: `docs/architecture/radio-abstraction.md`

Verification: Discover the ANAN-G2 on the local network, connect via P2, receive I/Q data stream.

### Phase 3B: WDSP Integration Layer ✅ COMPLETE
**Goal:** Process I/Q through WDSP, output demodulated audio.

Files to modify/create:
- `third_party/wdsp/` — integrate WDSP library (build from source or prebuilt)
- `src/core/WdspEngine.h/.cpp` — implement channel lifecycle, fexchange2() calls
- `src/core/RxChannel.h/.cpp` — **new** — per-receiver WDSP channel with Q_PROPERTY DSP params
- `src/core/TxChannel.h/.cpp` — **new** — TX WDSP channel
- `CMakeLists.txt` — WDSP linking, HAVE_WDSP define

Key design reference: `docs/architecture/wdsp-integration.md`

Verification: Feed I/Q from radio into WDSP, hear demodulated audio through speakers.

### Phase 3C: macOS Build + Crash Fix ✅ COMPLETE
**Goal:** Cross-platform WDSP build, macOS Apple Silicon support.

Files to modify/create:
- `src/core/AudioEngine.h/.cpp` — implement QAudioSink/Source, WDSP audio routing
- RX: I/Q → WdspEngine → decoded audio → AudioEngine → speakers
- TX: mic → AudioEngine → WdspEngine → modulated I/Q → RadioConnection → radio

Verification: Tune to a known signal on the ANAN-G2, hear audio.

### Phase 3D: GPU Spectrum & Waterfall ✅ COMPLETE
**Goal:** Display live FFT spectrum and waterfall from I/Q data.

Files to modify/create:
- `src/core/FFTEngine.h/.cpp` — **new** — FFTW3-based FFT, windowing, averaging
- `src/gui/SpectrumWidget.h/.cpp` — **new** — QRhiWidget GPU rendering
- `resources/shaders/waterfall.vert/.frag` — ring-buffer waterfall
- `resources/shaders/spectrum.vert/.frag` — FFT trace with fill
- `resources/shaders/overlay.vert/.frag` — grid, markers, spots
- `CMakeLists.txt` — qt_add_shaders(), link Qt6::GuiPrivate

Key design reference: `docs/architecture/gpu-waterfall.md`

Verification: See live spectrum + waterfall from the ANAN-G2 while receiving.

### Phase 3E: VFO & Controls + Multi-Receiver Foundation ✅ COMPLETE
**Goal:** Add VFO tuning, mode selection, filter, AGC controls. Simultaneously rewire
the I/Q pipeline to route through ReceiverManager with per-receiver WDSP channels and
FFTEngines. Only one receiver is active, but the plumbing supports N.

**I/Q Pipeline Rewire** (critical prerequisite for Phase 3F multi-panadapter):
The current RadioModel.cpp:207-241 hardwires a single-receiver pipeline — ddcIndex is
ignored, ReceiverManager is bypassed, and rxChannel(0) is hardcoded. This phase routes
I/Q through ReceiverManager with per-receiver WDSP channels and FFTEngines.

Files to modify/create:
- `src/models/RadioModel.cpp` — route iqDataReceived through ReceiverManager instead of direct processing
- `src/core/ReceiverManager.h/.cpp` — add adcIndex to ReceiverConfig; board-type-aware DDC mapping (DDC2=RX1 on 2-ADC boards)
- `src/core/WdspEngine.h/.cpp` — support multiple RxChannel instances keyed by receiver index
- `src/core/FFTEngine.h/.cpp` — support instantiation per receiver
- `src/models/SliceModel.h/.cpp` — frequency, mode, filter, AGC properties; slice→receiver mapping; N-slice capable
- `src/gui/widgets/VfoDisplay.h/.cpp` — **new** — frequency readout with click-to-tune digit editing
- `src/gui/widgets/RxControls.h/.cpp` — **new** — mode, filter, AGC, AF/RF controls
- `src/gui/MainWindow.cpp` — VFO-to-model wiring, per-receiver FFTEngine wiring

Key design references:
- `docs/architecture/multi-panadapter.md` (slice-to-pan association)
- `docs/architecture/adc-ddc-panadapter-mapping.md` (DDC assignment per board type)

Verification: Tune to different frequencies/modes via VFO display. Mode/filter changes
reflect in WDSP demodulation. I/Q flows through ReceiverManager (no regression).

### Phase 3G-1: Container Infrastructure ✅ COMPLETE
**Goal:** Dock/float/resize/persist container shells — no rendering yet.

Scope:
- `ContainerWidget` — dock/float/resize/axis-lock (8 positions), title bar, settings gear
  - Properties: border, background color, title/notes, RX source, show on RX/TX,
    auto-height, locked, hidden-by-macro, container-minimises, container-hides-when-rx-not-used
- `FloatingContainer` — `QWidget` with `Qt::Window | Qt::Tool`, pin-on-top, per-monitor DPI, geometry persist
- `ContainerManager` — singleton, create/destroy, float/dock transitions, axis-lock reposition, serialize to AppSettings, macro visibility
- Default layout: single right-side container (Container #0) with placeholder content

Thetis source: `ucMeter.cs`, `frmMeterDisplay.cs`, `MeterManager.cs`

Verification: Create containers, dock/float/resize, persist across restart, axis-lock holds on resize.

### Phase 3G-2: MeterWidget GPU Renderer ✅ COMPLETE
**Goal:** QRhi-based meter rendering engine following SpectrumWidget's 3-pipeline pattern.

Scope:
- `MeterWidget : public QRhiWidget` — one per container, renders all items in one draw pass
- Pipeline 1 (textured quad): cached QPainter textures for backgrounds and images
- Pipeline 2 (vertex-colored geometry, Triangles topology): animated bar fills with attack/decay smoothing
- Pipeline 3 (QPainter → texture overlay, split static/dynamic): tick marks, text readouts, scale labels
- `MeterItem` base class — position (normalized 0-1), data source binding, z-order, serialization
- Concrete item types: BarItem (H/V), TextItem, ScaleItem (H/V), SolidColourItem, ImageItem
- `ItemGroup` — composites N items into named presets with factory methods
- `MeterPoller` — QTimer polling RxChannel::getMeter() at 100ms, pushes to bound MeterWidgets
- Shaders: `meter_textured.vert/.frag` (Pipelines 1 & 3), `meter_geometry.vert/.frag` (Pipeline 2)
- Pipe-delimited item serialization compatible with ContainerWidget persistence
- Container #0 pre-populated with live horizontal signal strength bar

Verification: Container #0 displays live H_BAR bound to WDSP SignalPeak, updating at 10fps via GPU. ✅

### Phase 3G-3: Core Meter Groups ✅ COMPLETE
**Goal:** Ship the meters operators expect on day one.

Scope:
- SMeterWidget: dedicated QWidget port of AetherSDR's SMeterWidget for pixel-identical S-meter
- NeedleItem (MeterItem system): composable arc needle for custom meter configurations
- Default presets: Power/SWR, ALC bar, Mic/Comp bars (via MeterItem system)
- Default Container #0 pre-loaded with: SMeterWidget (top), Power/SWR + ALC (bottom)
- TX MeterBinding constants 100-105 (stubs until TxChannel in Phase 3M-1)
- Data binding: SIGNAL_STRENGTH, AVG_SIGNAL_STRENGTH, ADC, AGC_GAIN, PWR, REVERSE_PWR, SWR, MIC, COMP, ALC

Delivered:
- Multi-layer MeterItem infrastructure (participatesIn + paintForLayer)
- NeedleItem with arc rendering, smoothing, peak hold, S-unit scaling
- TX MeterBinding constants (100-105)
- Preset factories (S-Meter, Power/SWR, ALC, Mic, Comp)
- Default Container #0 layout (S-Meter 55% + Power/SWR 30% + ALC 15%)
- SMeterWidget: direct AetherSDR port for pixel-identical fidelity; dynamic aspect-ratio resize, full-width arc

Verification: Live S-meter needle, Power/SWR during TX, correct readings vs Thetis on same signal.

### Phase 3-UI: Full UI Skeleton ✅ COMPLETE
**Goal:** Build the complete application UI frame — all applets, menus, setup dialog, and spectrum controls — so every feature has a home before deep wiring begins.

**Files created:**
- `src/gui/applets/RxApplet.h/.cpp` — RX controls; Tier 1 (mode, AGC, AF gain, filter presets) wired to SliceModel
- `src/gui/applets/TxApplet.h/.cpp` — TX controls (mic gain, drive, tune power, compression, DEX)
- `src/gui/applets/PhoneCwApplet.h/.cpp` — voice/CW macro keys, VOX, CW speed/weight
- `src/gui/applets/EqApplet.h/.cpp` — 10-band RX/TX graphic equalizer
- `src/gui/applets/FmApplet.h/.cpp` — FM deviation, sub-tone, repeater offset
- `src/gui/applets/DigitalApplet.h/.cpp` — digital mode settings
- `src/gui/applets/PureSignalApplet.h/.cpp` — PureSignal calibrate + feedback level
- `src/gui/applets/DiversityApplet.h/.cpp` — diversity RX phase/gain controls
- `src/gui/applets/CwxApplet.h/.cpp` — CW message memory keyer
- `src/gui/applets/DvkApplet.h/.cpp` — digital voice keyer playback controls
- `src/gui/applets/CatApplet.h/.cpp` — CAT/rigctld port configuration
- `src/gui/applets/TunerApplet.h/.cpp` — antenna tuner control
- `src/gui/SpectrumOverlayPanel.h/.cpp` — 10-button overlay panel, 5 flyout sub-panels, auto-close
- `src/gui/SetupDialog.h/.cpp` — 47 pages across 10 categories with real controls
- `src/gui/AppletPanelWidget.h/.cpp` — fixed S-Meter header + scrollable applet body
- `src/gui/StyleConstants.h` — shared color palette, fonts, widget style constants
- `src/gui/widgets/HGauge.h/.cpp` — horizontal bar gauge widget
- `src/gui/widgets/ComboStyle.h/.cpp` — styled combo box shared across applets

**Implementation plan:** `docs/architecture/phase3-ui-skeleton-plan-v2.md`

Verification: All menus, applets, and dialogs present and navigable; RxApplet Tier 1 controls update SliceModel live.

### Phase 3G-4: Advanced Meter Items ✅ COMPLETE
**Goal:** Visual flair — items that make it look like a real radio console.

Delivered:
- 12 new passive MeterItem types: SpacerItem, FadeCoverItem, LEDItem, HistoryGraphItem, MagicEyeItem, NeedleScalePwrItem, SignalTextItem, DialItem, TextOverlayItem, WebImageItem, FilterDisplayItem, RotatorItem
- ANANMM 7-needle composite preset with exact Thetis calibration points
- CrossNeedle dual fwd/rev power meter with mirrored geometry
- Edge meter display mode (BarItem Filled/Edge style)
- 15+ new bar preset factories, MeterBinding extensions (7-8, 106-112, 200-202, 300-301)

**Implementation plan:** `docs/architecture/phase3g4-advanced-meters-plan.md`

Verification: All items render correctly, ANANMM/CrossNeedle calibration matches Thetis. ✅

### Phase 3G-5: Interactive Meter Items ✅ COMPLETE
**Goal:** Button grids and frequency displays inside containers.

Delivered:
- MeterItem mouse event virtuals (hitTest, handleMousePress/Release/Move, handleWheel)
- MeterWidget reverse z-order mouse forwarding
- ButtonBoxItem shared base class (grid layout, 13 indicator types, 3-state colors, 100ms click highlight)
- 8 button grid items: BandButtonItem (12 bands), ModeButtonItem (10 modes), FilterButtonItem (12 filters), AntennaButtonItem (10 color-coded), TuneStepButtonItem (7 steps), OtherButtonItem (34 core + 31 macros), VoiceRecordPlayItem (5 DVK), DiscordButtonItem (12 status)
- VfoDisplayItem (XX.XXX.XXX format, per-digit wheel tuning, mode/filter/band labels)
- ClockItem (dual Local + UTC, 1s timer, 24h/12h)
- ClickBoxItem (invisible hit region), DataOutItem (MMIO bridge)
- ContainerWidget signal forwarding via wireInteractiveItem()
- ItemGroup deserialize registry (13 new tags) + 3 presets (VFO Display, Clock, Contest)

**Implementation plan:** `docs/architecture/phase3g5-interactive-meters-plan.md`

Verification: Build clean, all items compile, signal chain wired through ContainerWidget. ✅

### Phase 3G-6: Container Settings Dialog
**Goal:** Full user customization — the composability UI.

Scope:
- Container settings dialog: item palette, current item list (drag reorder), per-item property panel, live preview
- Preset templates for common configurations
- Import/export (Base64-encoded container strings)
- Duplicate, recover off-screen, macro visibility hooks

Verification: Create container from scratch, add items, configure data sources, export/import Base64.

### Phase 3G-8: RX1 Display Parity ✅ COMPLETE
**Goal:** Bring `Setup → Display` to feature parity with Thetis for RX1 only — the 47-control wire-up.

**Shipped 2026-04-12** on `feature/phase3g8-rx1-display-parity` as PR #8 (base `main`). 10 GPG-signed code commits + 3 doc-amend prep commits.

Scope delivered:
- 3 Setup pages fully wired: Spectrum Defaults (17), Waterfall Defaults (17), Grid & Scales (13)
- New `ColorSwatchButton` reusable QColorDialog-backed widget (`src/gui/ColorSwatchButton.h/.cpp`) used by 9 call sites
- New `Band` enum (`src/models/Band.h`): 14 bands (160m–6m + GEN + WWV + XVTR), IARU Region 2 frequency lookup, UI-index mapping
- Per-band display grid storage on `PanadapterModel` (28 per-band Max/Min keys + 1 global Step)
- `BandButtonItem` expanded 12 → 14 buttons
- `SpectrumWidget` renderer additions: averaging modes (None/Weighted/Log/TimeWindow), peak hold + decay, trace fill/alpha/line-width/gradient, cal offset, waterfall AGC/reverse/opacity/overlays/timestamp, 3 new colour schemes (LinLog/LinRad/Custom — total 7), configurable grid colours, 5-mode frequency label alignment, FPS overlay
- `FFTEngine` already had FFT size and window switching; wired through the Spectrum Defaults page
- `RadioModel::spectrumWidget()` / `fftEngine()` non-owning view hooks so setup pages reach the renderer without depending on `MainWindow`
- GPU path polish: overlay texture cache invalidation (11 controls), waterfall chrome factored into `drawWaterfallChrome()` and drawn into the GPU overlay texture (W6 opacity, W8/W9 timestamp, W11/W13 filter/zero-line overlays), new `m_fftPeakVbo` for GPU peak hold, vertex-gen changes so cal offset / gradient toggle / fill toggle / fill colour are live in the GPU render path

Plan §13 open questions resolved:
1. Cal Offset (S8) — real Thetis field at `display.cs:1372` (`Display.RX1DisplayCalOffset`), not an extension
2. FPS overlay (G8) — `QPainter` text in `paintEvent` + GPU overlay texture path
3. Display Thread Priority (S17) — 1:1 map Thetis ThreadPriority → QThread::Priority, default HighPriority
4. Per-band grid initial values — Thetis uniform -40 / -140 for all 14 slots (authorised one-off §10 divergence)

Authorised Thetis divergence (plan §10, one-off): new per-band grid slots initialise to Thetis uniform -40 / -140 rather than NereusSDR's existing -20 / -160. Source-first protocol stays as written — this phase is an exception, not a precedent.

Known deferrals (tracked in PR description):
- S7 Line Width on GPU (QRhi lacks portable setLineWidth; needs triangle strip rendering — deferred)
- S16 FFT Decimation (UI scaffolded, no FFTEngine setter)
- W12/W14 TX filter/zero-line overlays (gated on TX state model — post-3M-1)
- Data Line / Data Fill Color splitting (shares `m_fillColor` until UX feedback justifies splitting)
- W10 Waterfall Low Color runtime effect (persisted; waits for Custom-scheme `AppSettings` parser)

Verification: 47-control matrix at `docs/architecture/phase3g8-verification/README.md`. Screenshot capture deferred; manual smoke-test workflow driven by the user.

**Implementation plan:** `docs/architecture/phase3g8-rx1-display-parity-plan.md` (plan §13 resolutions in commit `0308b1b`, plan §5.3 correction in `b8045cc`).

### Phase 3G-9: Display Refactor (audit · smooth defaults · Clarity)
**Goal:** Take the 3G-8 wiring from "every control works" to "every control is sourced, well-labeled, well-presented, and the default display looks great without the user touching anything." Close with an adaptive auto-tune feature called **Clarity** that keeps the waterfall dialed in as band conditions and tuning change.

**Motivation:** Side-by-side comparison between NereusSDR v0.1.4 and AetherSDR v0.8.12 on 80m (2026-04-14 reference screenshots) shows a significant readability gap — AetherSDR's narrow-band monochrome palette renders noise as uniform dark navy and signals as bright cyan/white, while NereusSDR's "Enhanced" rainbow spends color contrast on noise. Decomposes into seven specific knob choices, not one mystery setting.

**Design spec:** `docs/architecture/2026-04-15-display-refactor-design.md` (committed `4b0a027`, 2026-04-15).

Shipped as three sequential PRs off `main`. Strict dependency: PR1 helpers feed PR2, PR2 static defaults feed PR3 as the Clarity-off fallback.

#### Phase 3G-9a: Source-First Audit + Tooltips + Slider Readouts (PR1) ✅ COMPLETE
**Shipped 2026-04-15** as PR #25 (merged). 11 GPG-signed commits.

- Per-control Thetis source citation comments on all 47 controls
- Tooltip port — verbatim where Thetis is substantive, rewritten where weak (annotated with original)
- `SliderRow` / `SliderRowD` helpers in new `src/gui/setup/SetupHelpers.{h,cpp}`: every numeric slider gets a bidirectional spinbox companion with unit suffix (matches Thetis `udDisplay*` NumericUpDown pattern)
- 47/47 controls with tooltips (from 10/47), 8 sliders converted (4 in Spectrum + 4 in Waterfall; GridScales has no sliders)
- `setToolTip` count in `DisplaySetupPages.cpp`: 10 → 62
- New `tst_setup_helpers` 6-slot QtTest smoke locking bidirectional sync + no-feedback-loop
- Implementation plan: `docs/architecture/2026-04-15-phase3g9a-display-audit-plan.md`

#### Phase 3G-9b: Smooth Defaults + Clarity Blue Palette (PR2) ✅ COMPLETE (v0.1.5)
**Opened 2026-04-15** as PR #26 (awaiting merge). 11 GPG-signed commits.

The PR2 design underwent significant iteration during live-radio tuning. What was originally specced as a narrow-band navy-only palette turned out to be wrong — the reference AetherSDR look is actually a **full-spectrum rainbow** (black → blue → cyan → green → yellow → red → magenta) where the "blue look" comes from AGC + tight thresholds keeping normal signals in the cool region. The final ClarityBlue palette is a 10-stop rainbow with a deep-black floor.

- New `WfColorScheme::ClarityBlue` — 10-stop full-spectrum rainbow, selectable from Waterfall Defaults (8th palette option)
- `RadioModel::applyClaritySmoothDefaults()` sets the seven recipe values (ClarityBlue palette, log-recursive averaging, `0.05f` alpha, pure-white `#ffffff` α230 thin trace, pan-fill off, waterfall AGC on, 30 ms update period)
- "Reset to Smooth Defaults" button on `Setup → Display → Spectrum Defaults` — the **only** entry point. **Decision 2026-04-15:** no first-launch auto-apply. Out-of-box default stays `WfColorScheme::Default`.
- **AGC margin widened 3 dB → 12 dB** in `SpectrumWidget::pushWaterfallRow` — gives palette breathing room for FFT skirt falloff across signal amplitude. Affects all palettes when AGC is on; pure quality improvement.
- New rationale doc `docs/architecture/waterfall-tuning.md` with before/after/reference screenshots (iter0 → iter6 live-tuning progression) and per-recipe source attribution (Thetis-native vs NereusSDR-native)
- Recipe #5 (explicit thresholds) dropped during implementation: AGC overwrites Low/High every frame so setting them in the profile was redundant
- New `tst_clarity_defaults` 5-slot QtTest locking palette invariants (enum ordinal, deep-black floor, monotonic stops, rainbow progression with warm upper-half stop, vivid peak)
- Implementation plan: `docs/architecture/2026-04-15-phase3g9b-smooth-defaults-plan.md`

#### Phase 3G-9c: Clarity Adaptive Display Tuning (PR3) ✅ COMPLETE (v0.1.5)
- **Research-doc gated** — no code until `docs/architecture/clarity-design.md` is written and signed off
- Flavor C: continuous adaptive thresholds + one-shot "Re-tune now" button
- New `NoiseFloorEstimator` (percentile-based, TDD unit tests on synthetic FFT bins)
- New `ClarityController` (2 Hz polling, EWMA τ ≈ 3s smoothing, ±2 dB deadband, TX-pause, manual-override detection)
- Per-band Clarity memory in `BandGridSettings` — band-switch snaps to last-known good
- Clarity status badge ("C") on Spectrum Overlay panel — green active / amber paused
- W3 "Waterfall AGC" checkbox deprecated in favor of Clarity with migration label
- Prior-art survey in the research doc: Thetis `WaterfallAGC`, WDSP `nob.c`, GQRX / SDR++ auto-range implementations
- ~250 LOC algorithm + ~400 LOC tests + ~300 LOC UI/wiring + research doc; Risk: high — algorithmic, mitigated by doc-first gate, TDD, clean off switch, PR2 fallback always present

**Non-goals:** RX2/TX display surface, Spectrum Overlay flyout refactors, skin system, Thetis default-value adoption beyond the seven PR2 recipes. Source-first protocol per CLAUDE.md governs everything else; 3G-8's §10 divergence exception is **not** extended.

### Phase 3G-12: P1 Field-Report Fixes
**Goal:** Close out the bugs surfaced by alpha testers running Protocol 1 hardware after 3I shipped. Grouped under one phase so each fix doesn't have to claim its own number. Each bullet below lands from its own session / branch.

- **P1 RX/TX VFO frequency encoding** — `P1RadioConnection::composeCcBankRxFreq` / `composeCcBankTxFreq` were pre-converting Hz to an NCO phase word (`freqHz * 2^32 / 122.88e6`). P1 firmware expects raw Hz; Thetis `NetworkIO.cs:215-223` only calls `Freq2PhaseWord` on the P2 (`ETH`) branch, and the native `networkproto1.c:476-494` splats `prn->{tx,rx}[0].frequency` directly into C1..C4 with no conversion. Symptom reproduced in alpha tester pcap4 (2026-04-15, ANAN-10E): 319 consecutive `C0=0x04` frames pinned at phase word `0x080D5555`, aliased waterfall, VFO did not track dial. Fix aligns with pre-existing `tst_p1_wire_format` assertions that had silently drifted unvalidated. Branch `fix/p1-freq-encoding`, confirmed working against live ANAN-10E 2026-04-15.

### Phase 3G-12: Display Persistence Polish
**Goal:** Close persistence gaps in the Display surface that surfaced during live tuning of Phase 3G-9b. Bugs where user state isn't preserved across app restarts despite being "display preferences" — these slip through because they're not part of any setup dialog page, they're live-mutated runtime state.

- **Zoom level persistence** — `SpectrumWidget::m_bandwidthHz` (the visible spectrum span) was reset to the full DDC sample rate on every launch. Iterative visual tuning against the AetherSDR reference during 3G-9b was blocked by this — every rebuild/relaunch cycle lost the zoom level. Fix: new `DisplayBandwidth` AppSettings key in `loadSettings()`/`saveSettings()` + `scheduleSettingsSave()` hook in `setFrequencyRange()` when bandwidth changes + MainWindow overrides removed (the initial connect path and `wireSampleRateChanged` handler were both unconditionally calling `setFrequencyRange(freq, 768000.0)` / `setFrequencyRange(freq, rateHz)`, wiping the loaded value). Branch `feature/phase3g12-zoom-persistence`, verified end-to-end 2026-04-15 on live Saturn 20m. **Known limitation:** this is a global zoom, not per-band — switching band stack slots currently loses the zoom. Per-band zoom (slot in `PanadapterModel::BandGridSettings`) is a follow-up.

### Phase 3I: Radio Connector & Radio-Model Port ✅ COMPLETE
**Goal:** Full Protocol 1 support across every ANAN/Hermes-family board (Hermes Lite 2, ANAN-10/10E/100/100B/100D/200D, Metis) with feature parity at the wire-format and Hardware-setup-UI level for all supported radios. A P1 radio should behave identically to how ANAN-G2 on P2 behaves today.

**Shipped 2026-04-13** on `feature/phase3i-radio-connector-port` as PR #12 (base `main`). 25 GPG-signed commits across 6 logical sections.

Scope delivered:
- **Enums (Task 1):** `HPSDRModel` and `HPSDRHW` ported 1:1 from mi0bot/Thetis@Hermes-Lite `enums.cs:109` / `:388`. Integer values preserved exactly including the 7..9 reserved gap for wire-format compatibility.
- **Capability registry (Task 2):** `BoardCapabilities` `constexpr` table — 10 entries (9 boards + `Unknown` fallback), pure data, 20+ invariant tests. Drives both protocol connections and the Hardware setup UI.
- **Enum migration (Task 3):** legacy `BoardType` → `HPSDRHW` across the tree. One docs-era bug fix: `BoardType::Griffin=2` was a mistake; mi0bot `enums.cs:392` clarifies slot 2 is `HermesII` (ANAN-10E / 100B).
- **Discovery (Task 4):** rewrote `RadioDiscovery` following mi0bot `clsRadioDiscovery.cs` — 6 tunable timing profiles (UltraFast → VeryTolerant), dual P1+P2 NIC walk with MAC-based de-dup.
- **Discovery parsers (Task 5):** `parseP1Reply` / `parseP2Reply` as public statics with hex-fixture unit tests (`tst_radio_discovery_parse`).
- **`P1RadioConnection` (Tasks 6-12):** skeleton + factory hookup (Task 6), ep2 frame/C&C bank compose (Task 7), ep6 frame parse + 24-bit sample scaler (Task 8), loopback integration test via `P1FakeRadio` (Task 9), 2 s watchdog + bounded reconnection state machine (Task 10), per-board quirks (atten clamp, firmware gate) + `RadioConnectionError` enum with 9 structured codes replacing string-only `errorOccurred` (Task 11), HL2 `IoBoardHl2` init + bandwidth-monitor sequence-gap heuristic (Task 12).
- **P2 audit (Task 13):** `P2RadioConnection` reads `BoardCapabilities` instead of hard-coded Saturn assumptions; recognises `SaturnMKII`, `ANAN_G2_1K`, `AnvelinaPro3` via existing P2 wire path with zero wire-format rewrite.
- **ConnectionPanel (Tasks 14-17):** expanded from 315-LOC skeleton into a full Thetis `ucRadioList.cs` port — 8 sortable columns, color-coded state, right-click context menu, saved-radio persistence keyed by MAC (`AppSettings::saveRadio/forgetRadio/savedRadios`), manual-add dialog ported from `frmAddCustomRadio.cs`, auto-reconnect on launch.
- **HardwarePage (Tasks 18-21):** new top-level SetupDialog entry with 9 capability-gated nested tabs mirroring Thetis Setup.cs Hardware Config (Radio Info · Antenna/ALEX · OC Outputs · XVTR · PureSignal · Diversity · PA Calibration · HL2 I/O Board · Bandwidth Monitor). Per-MAC settings persistence under `hardware/<MAC>/*` in `AppSettings`, tested with radio-swap isolation.
- **Docs + verification (Tasks 22-23):** CHANGELOG entry, `docs/architecture/radio-abstraction.md` drift fixes, per-board hardware smoke checklist at `docs/architecture/phase3i-verification.md`, smoke-test walkthrough at `docs/debugging/phase3i-smoke-test.md`.

**Source-first wins during implementation** (subagents caught by reading Thetis directly):
1. P2 boards go up to 1536 kHz, not 384 kHz (`Setup.cs:854`)
2. Hermes/HermesII cap at 192 kHz (`Setup.cs:850-853`) — 384k is HL2-only among single-ADC P1
3. P1 wire bytes ≠ `HPSDRHW` enum values (e.g., Angelia is wire byte 4, enum value 3) — handled via `mapP1DeviceType` in the parser
4. `HermesII` has PureSignal (`console.cs:30276`)
5. `IoBoardHl2.cs` is not portable byte data — it wraps closed `ChannelMaster.dll` I2C-over-ep2 framing, correctly stubbed pending Phase 3L

**Test coverage:** 9 automated test executables, 9/9 pass in ~51 s (~49 s of that is the real-time bounded-retry contract test `tst_reconnect_on_silence`). Full suite:
1. `tst_hpsdr_enums`
2. `tst_board_capabilities`
3. `tst_radio_discovery_parse` (hex fixtures under `tests/fixtures/discovery/`)
4. `tst_p1_wire_format` (24 slots)
5. `tst_p1_loopback_connection` (P1FakeRadio end-to-end)
6. `tst_reconnect_on_silence`
7. `tst_connection_panel_saved_radios`
8. `tst_hardware_page_capability_gating`
9. `tst_hardware_page_persistence`

**Post-merge hotfixes** (applied during smoke testing before PR #12 was marked ready):
- Removed the 5 s continuous NIC-walk timer from `RadioDiscovery` — `scanAllNics()` uses blocking `QUdpSocket::waitForReadyRead` on the main thread; on a laptop with 8-10 NICs the freeze was 15-20 s every 5 s. Discovery is now user-triggered only; full async rewrite is a follow-up.
- Replaced `std::exit(0)` in `MainWindow::closeEvent` with `QCoreApplication::quit()` — the exit path was running C++ static destructors before Qt's thread-local cleanup, causing `QThreadStoragePrivate::finish` to fire a `qWarning` against destructed `QRegularExpression` objects in the PII-redaction message handler. ~100 crash reports in one afternoon of testing before the fix.
- Added an `m_active` guard to `RxChannel::getMeter()` — `MeterPoller` was polling `GetRXAMeter` on a WDSP channel that hadn't been `SetChannelState`-activated yet, segfaulting on connect. The race was latent on Saturn too but only reliably exposed by the P1 connect ordering.
- Wired `RadioModel::currentRadioChanged(RadioInfo)` signal and connected it in `HardwarePage` constructor so sub-tabs actually populate when a radio connects. Task 18 had left the wiring as a manual call; Task 21 added the slot but forgot to connect it at runtime.

Deferred (see design §9 + verification doc): TX IQ producer (Phase 3M), PureSignal feedback DSP (Phase 3M), HL2 I2C-over-ep2 wire encoding (Phase 3L), full bandwidth-monitor port (Phase 3L), TCI protocol (Phase 3J), RedPitaya board, sidetone generator, firmware flasher, multi-radio simultaneous connection.

**Design doc:** `docs/architecture/phase3i-radio-connector-port-design.md` (865 lines)
**Plan doc:** `docs/architecture/phase3i-radio-connector-port-plan.md` (23 tasks, 2575 lines)
**Smoke test:** `docs/debugging/phase3i-smoke-test.md`

### Phase 3G-14: 💡 AI-Assisted Issue Reporter
**Goal:** Port AetherSDR's light-bulb issue reporter — a 💡 button in the menu bar corner widget that guides users through filing feature requests or bug reports with AI assistance.

Ported from AetherSDR `TitleBar::showFeatureRequestDialog()` / `showFeatureRequestDialogImpl()`.

Scope:
- **💡 corner widget** — amber-styled QPushButton via `QMenuBar::setCornerWidget()`, always visible
- **Version check gate** — hits `api.github.com/repos/boydsoftprez/NereusSDR/releases/latest`, warns if outdated before filing
- **AI-assisted issue dialog** — structured prompt copied to clipboard (adapted for NereusSDR: OpenHPSDR radios, Thetis reference, `boydsoftprez/NereusSDR` URLs, NereusSDR label set), provider buttons (Claude, ChatGPT, Gemini, Grok, Perplexity), "Submit Your Idea" → `feature_request.yml`, "Report a Bug" → `bug_report.yml`
- Existing SupportDialog "File an Issue" flow unchanged

Files touched: `MainWindow.h`, `MainWindow.cpp`. No new source files. Requires Qt6::Network (already linked).

Independent of all other phases — no file overlap with 3G-9, 3G-10, 3G-13, or 3M-*.

### Phase 3Q: Connection Workflow Refactor
**Goal:** Make the connect / discover / disconnect flow coherent so a user on a Layer-3 VPN (WireGuard, ZeroTier, etc.) can reach a remote radio reliably, and so the disconnected state actually announces itself instead of just freezing the spectrum. Triggered by an April 2026 user report (HL2 across a WireGuard tunnel couldn't be reached via the existing manual entry path).

Design spec: [docs/architecture/2026-04-26-connection-workflow-refactor-design.md](architecture/2026-04-26-connection-workflow-refactor-design.md)

Scope:

- **Single state machine** — `Disconnected → Probing → Connecting → Connected → (Disconnected | LinkLost)` with broadcast scan and unicast probe as different *triggers* into the same path. Replaces today's broadcast-only-then-blind-connect flow.
- **Unicast probe** in `RadioDiscovery::probeAddress(addr, port, timeout)` — 1.5 s timeout, parallel P1 + P2, parses replies via the existing `parseP1Reply()` / `parseP2Reply()` helpers. New code path (today is broadcast-only).
- **TitleBar connection segment** — state dot · radio name · IP · ▲▼ Mbps · activity LED that pulses on each ep6/DDC frame · click opens panel · right-click for Reconnect/Disconnect/Manage Radios. Drops into the existing 32 px `TitleBar` widget where the file header already noted the connection-state UI was deferred.
- **Status bar verbose strip** — sample rate · firmware · MAC · packets/s · drops · "● live" when connected; red dot + "No radio connected" + "last connected ANAN-G2 · 14:23 today" breadcrumb when disconnected.
- **ConnectionPanel polish** — modal kept (today's behavior); status strip up top with inline Disconnect; state-pill column (🟢 Online <60 s · 🟡 Stale 60 s–5 min · 🔴 Offline) replaces the bare `●`; Last Seen column replaces MAC; single ↻ Scan in the table header replaces Start/Stop Discovery; Disconnect moves out of the bottom strip into the status strip; Auto-connect-on-launch checkbox added to the detail panel; auto-opens on launch + on disconnect; auto-closes 1 s after Connected.
- **Add Radio dialog rebuild** — replaces the 9-board picker at `AddCustomRadioDialog.cpp:294-302` with a 16-SKU model dropdown organized by silicon family in `<optgroup>`s ("Auto-detect" first, then Atlas / Hermes (3) / Hermes II (2) / Angelia / Orion / Orion MkII (5) / Hermes Lite 2 / Saturn (2)). Two action buttons: `Probe and connect now` and `Save offline`. Failure path keeps the dialog open with form preserved + red error band; success path auto-closes and lands the row in the table tagged "(probe)".
- **Radio menu rework** — `Connect (⌘K) · Disconnect (⌘⇧K) · Discover Now · Manage Radios… · Antenna Setup… (NYI) · Transverters… (NYI) · Protocol Info` with state-aware enablement (Connect/Disconnect mutually exclusive; Protocol Info follows connection). Replaces today's four-item-three-aliases set.
- **Spectrum disconnect overlay** — 800 ms fade to ~40 % opacity + DISCONNECTED label + click-anywhere-to-open-panel. Multi-cue feedback closes the loop on the "spectrum just freezes" complaint.
- **Stale policy change** — saved radios *never* age out (today they get dropped at 15 s); discovered-only radios age out at 60 s (raised from 15 s, long enough not to flap); connected MAC stays exempt.
- **Auto-connect-on-launch** — uses the existing per-radio `AppSettings::autoConnect` flag; on failure the panel auto-opens with the target highlighted offline + a status-bar diagnostic. Multi-flag case picks most-recent-connected MAC + one-time setup-bar warning.
- **macKey migration** — offline entries saved with the synthetic `manual-<IP>-<port>` key get migrated under the real MAC on first probe success, preserving Name / Model / Auto-connect / Pin-to-MAC.

Files touched: `src/gui/{TitleBar,MainWindow,ConnectionPanel,AddCustomRadioDialog,SpectrumWidget}.{h,cpp}`, `src/core/{RadioDiscovery,RadioConnection,AppSettings}.{h,cpp}`, `src/models/RadioModel.{h,cpp}`. No new files except possibly a small `ConnectionState` enum header.

Open decisions (deferred per design §10): HL2 variant SKUs (HL Plus, RX-only, etc. not yet in `HPSDRModel`), async discovery rewrite (synchronous scan stays for now), background unicast pings of saved radios (not in this phase), link-lost auto-retry (none — explicit user click), auto-close-on-connect timing (1 s), multi-auto-connect-radio behavior.

No file-level overlap with 3M-1 — runs in parallel with the TX epic. Expected to land before 3M-1 completes (it's the smaller piece and Miguel's pain blocks more users than no-TX-yet does).

### Phase 3M-1: Basic SSB TX  **[Complete — 2026-04-29]**
**Status:** Shipped via three sub-phases — 3M-1a TUNE-only first RF (PR #144),
3M-1b SSB voice + mic-jack family (PR #149), 3M-1c polish + persistence +
Thetis-faithful semaphore-wake TX pump v3 + HL2 P1 setTxDrive triage + Codex
P1/P2 fixes (PR #152).

**HL2 hardware bench (rows 58-60 in `phase3m-0-verification/README.md`)** ran
during v0.3.1 polish and the ATT/filter safety audit closed — the
attenuator and filter-bank wiring code paths that JJ originally flagged were
verified end-to-end on HL2 hardware.  HL2 SSB TX is now bench-cleared.

**Goal:** Get RF out the door — prove the TX I/Q output path works.

Scope:
- `TxChannel` WDSP wrapper — create TX channel, `fexchange2()` TX path
- Mic input via QAudioSource (48kHz, 16-bit)
- WDSP internal rate: 192kHz for P2, 48kHz for P1 (resampling handled by WDSP)
- MOX state machine — RX→TX→RX transition ported from Thetis `console.cs:29311`
  - 6 configurable delays: `rf_delay`, `mox_delay`, `ptt_out_delay`, `key_up_delay`, etc.
  - RX channel muting, DDC reconfiguration, T/R relay switching
  - Ordered WDSP channel enable/disable with flush
- TX I/Q output to radio — port 1029, 240 samples/packet, 24-bit big-endian
- `sendCmdTx()` on P2RadioConnection — port 1026 for mic/CW data
- TUNE function (reduced power carrier)

Thetis source: `console.cs:29311-29650`, `cmaster.cs:491-540`, `network.c:1250-1273`

Verification: Key MOX, see RF output on ANAN-G2, hear SSB on another receiver.

### Phase 3M-2: CW TX  **[Next major TX epic — after 3M-3a-iii ships]**
**Status:** Originally scheduled before 3M-3.  Schedule swapped 2026-04-29
because (a) 3M-3 didn't need the HL2 hardware bench (DSP stages are
introspectable on ANAN-G2), letting the HL2 ATT/filter safety audit run in
parallel without blocking forward TX progress; (b) 3M-3 makes the voice TX
that 3M-1 just shipped genuinely good (broadcast-grade preprocessing) before
adding a new state-machine layer.  The HL2 ATT/filter audit closed in v0.3.1,
so HL2 CW bench-verification is no longer gated.

**Absorbs:** the HL2 CWX bit-3 follow-up (`networkproto1.c:1247-1252
[@c26a8a4]` — desk-review B3, "HL2 firmware uses bit 3 of I-low byte for
CWX PTT, non-HL2 boards don't").

**BandPlanGuard mode-gate** — until 3M-2 ships, CWL/CWU continue to be
rejected with the verbatim toast "CW TX coming in Phase 3M-2"
(`tst_band_plan_guard_mode_allow_list`).

**Goal:** Full CW transmit with keyer and sidetone.

Scope:
- Sidetone generator — port from Thetis `sidetone.c`
  - Raised-cosine edge shaping, NOT through TXA WDSP channel
  - Dot/dash timing: `dot_time = 1.2 / wpm`
- Firmware keyer support — dot/dash/PTT via P2 port 1025
- QSK / semi break-in — CWHangTime, key_up_delay
- CW MOX special case — TX WDSP channel NOT enabled in CW mode
- APF (Audio Peak Filter) — narrowband CW filter via WDSP

Thetis source: `sidetone.c`, `cwkeyer.cs`, `console.cs:29590`

Verification: Send CW via paddle/keyboard, hear sidetone, clean CW on air.

### Phase 3M-3: TX Processing Chain + RX DSP Additions  **[Next — pulled forward from after-3M-2]**
**Status:** Pulled forward 2026-04-29 from its original "after 3M-2 CW TX"
slot.  Rationale in §3M-2 above.  Branched at `feature/phase3m-3-tx-processing`
worktree on 2026-04-29 from `origin/main` `1d40670` (post-PR-#152 merge).

**Goal:** Full TX audio processing feature parity with Thetis TXA chain (18 stages).

TX DSP (all need WDSP call wiring + UI controls):
- Phase Rotator (`SetTXAosctrlRun`)
- 10-band TX EQ (`SetTXAEQRun`, `SetTXAGrphEQ10`)
- Leveler (`SetTXALevelerSt`)
- CFC — Continuous Frequency Compressor (`SetTXACFCRun` + dedicated config dialog)
- CPDR — Compressor (`SetTXACompressorRun`, `SetTXACompressorGain`)
- CESSB — Controlled-Envelope SSB (`SetTXAosctrlRun` with CESSB params)
- ALC (`SetTXAALCMaxGain`, `SetTXAALCDecay`)
- DEXP — Downward Expander / VOX (`SetDEXPRunVox`, full 9+ parameter set)

RX DSP additions (slot into existing widgets):
- SNB (Stochastic Noise Blanker) — RxControls toggle
- Spectral Peak Hold — SpectrumWidget overlay pipeline
- Histogram display mode — SpectrumWidget alternate render path
- RTTY mark/shift parameters — SliceModel properties

Thetis source: `TXA.c:557-591`, `dsp.cs`, `radio.cs`, `setup.cs`

Verification: Clean SSB with TX EQ + compression + ALC. Proper FM deviation. CESSB measurably improves average power.

### Phase 3M-4: PureSignal PA Linearization
**Goal:** Client-side PA predistortion — full feature parity with Thetis PSForm.

PureSignal is entirely client-side for OpenHPSDR radios (no AetherSDR reference — pure Thetis port).

Scope:
- **Feedback RX channel** — dedicated WDSP channel wired to DDC0 (synced with DDC1 at 192kHz)
  - ADC cntrl1 override: `(rx_adc_ctrl1 & 0xf3) | 0x08`
  - `SetPSRxIdx(0, 0)` / `SetPSTxIdx(0, 1)` — stream routing
- **calcc engine** — port `calcc.c` 10-state machine (LRESET→LWAIT→LMOXDELAY→LSETUP→LCOLLECT→MOXCHECK→LCALC→LDELAY→LSTAYON→LTURNON)
  - Amplitude-binned sample collection, piecewise cubic Hermite spline correction
  - Stabilize/pin/map modes, polynomial tolerance validation
  - 4-second collection watchdog, IQC dog count watchdog
- **IQC real-time correction** — port `iqc.c` (runs on every TX sample)
  - `PRE_I = ym * (I*yc - Q*ys)`, `PRE_Q = ym * (I*ys + Q*yc)`
  - Double-buffered coefficients with cosine crossfade
- **TX/RX delay alignment** — fractional delay lines, 20ns step resolution
- **Auto-attenuation** — monitors feedback level (target 128-181 of 256), adjusts TX attenuation
- **PSForm UI** — accessible from menu bar (not buried in sub-dialog)
  - Calibrate (single-shot + auto-cal), feedback level indicator (color-coded)
  - Advanced: mox delay, loop delay, TX delay, stabilize/map/pin, tolerance
  - Save/restore coefficients, two-tone test generator
  - Info bar integration for status bar feedback display
- **AmpView** — correction curve visualization, accessible from menu bar (DSP or Tools)

Thetis source: `PSForm.cs` (1164 lines), `calcc.c`, `iqc.c`, `TXA.c:557-591`

Verification: Enable PS on ANAN-G2, feedback level green, measurable IMD improvement.

### Phase 3F: Multi-Panadapter Layout
**Goal:** Support 1-4 panadapters with proper DDC-to-ADC mapping and multiple active receivers.
Multi-receiver plumbing from Phase 3E is a prerequisite.

**Critical note:** `UpdateDDCs()` port must include ALL state machine cases from Thetis
`console.cs:8186-8538`, including PureSignal DDC states (DDC0+DDC1 sync at 192kHz, ADC
cntrl1 override `(rx_adc_ctrl1 & 0xf3) | 0x08`). PureSignal (Phase 3M-4) is complete
by this point, so these states should be fully wired — not just stubbed.

Files to modify/create:
- `src/core/ReceiverManager.cpp` — add updateDdcAssignment() ported from Thetis UpdateDDCs (console.cs:8186-8538)
- `src/core/FFTRouter.h/.cpp` — **new** — map receiver FFT output to 1+ panadapter(s)
- `src/gui/PanadapterStack.h/.cpp` — **new** — QSplitter layout manager (5 layouts)
- `src/gui/PanadapterApplet.h/.cpp` — **new** — single pan container with independent display state
- `src/gui/MainWindow.cpp` — wirePanadapter(), layout menu, multi-pan FFT routing, enable RX2

Key design references:
- `docs/architecture/multi-panadapter.md`
- `docs/architecture/adc-ddc-panadapter-mapping.md`
- Implementation plan: `docs/architecture/phase3f-multi-panadapter-plan.md`

Verification: 2 pans stacked — RX1 on 20m, RX2 on 40m with independent spectrums.
4 pans in 2x2 grid — 2 pans share RX1 at different zoom, 2 pans on RX2.
RX1 on DDC2 (ADC0), RX2 on DDC3 (ADC1) for 2-ADC boards.

### Phase 3H: Skin System
**Goal:** Thetis-inspired skin format with 4-pan support + legacy skin import.

Updates from 2026-04-10 review:
- Always allow 4 pans (removed 2-pan legacy cap)
- `TransparencyKey` dropped (no Qt6 equivalent — known limitation)
- Font fallback: "Microsoft Sans Serif" → system sans-serif on macOS/Linux
- Graceful degradation for non-standard community skin ZIPs

Files to create:
- `src/gui/SkinParser.h/.cpp` — **new** — parse skin ZIP (JSON + XML + PNG)
- `src/gui/SkinRenderer.h/.cpp` — **new** — apply theme/images to widgets
- `src/gui/SkinManager.h/.cpp` — **new** — skin server integration, cache

Key design reference: `docs/architecture/skin-compatibility.md`

Verification: Load a Thetis skin, see colors/images applied, 4 pans still work.

### Phase 3J: TCI Protocol + Spot System
**Goal:** TCI v2.0 WebSocket server + DX spot overlay system.

Scope:
- TCI WebSocket server (~50 commands, scope v2.0 command set)
- TCI spot ingestion path — external programs push spots via TCI
- Built-in DX Cluster telnet client (AetherSDR-style `DxClusterClient`)
- Built-in RBN client (AetherSDR-style `RbnClient`)
- `SpotModel` (AetherSDR pattern, batched at 1Hz)
- Spectrum overlay rendering for spots (callsign + frequency on panadapter)
- Country/prefix database for callsign → country lookup
- Verify against Thetis `SpotManager2.cs` for completeness

### Phase 3K: CAT/rigctld + TCP Server
**Goal:** External radio control for logging and contest software.

Scope:
- 4-channel slice-bound rigctld server (AetherSDR `RigctlServer` + `RigctlPty` pattern)
- TCP/IP CAT server (AetherSDR pattern, verified against Thetis `TCPIPcatServer.cs`)
- Verify CAT command set against Thetis `SIOListenerII` for completeness
- CatApplet UI for configuration

### Phase 3L: Protocol 1 Support
**Goal:** Add P1 support for Hermes Lite 2 and older ANAN radios.

Scope:
- `P1RadioConnection` — UDP-only Metis framing (1032-byte frames)
- `MetisFrameParser` — C&C register rotation, EP6 I/Q format
- P1 discovery (0xEF 0xFE format)
- Phase word encoding: `freq * 2^32 / 122880000`
- P1-specific DDC assignment (`rx_adc_ctrl_P1` encoding)
- TX at 48kHz (not 192kHz) — no CFIR needed

### Phase 3M: Recording/Playback
**Goal:** Full audio and I/Q recording system.

Scope:
- WAV audio recording — tap demodulated audio after WDSP
- WAV playback through WDSP — route WAV as live I/Q (dev/demo without hardware)
- Quick record/playback — one-button 30-second scratch pad
- Scheduled recording — timer-based auto-start/stop
- I/Q recording — raw I/Q samples for offline analysis
- Recording controls wired to VfoWidget (existing button stubs)

### Phase 3P: All-Board Radio-Control Parity ✅ COMPLETE (all sub-phases A–I-b shipped in v0.1.7 / v0.2.x — Apr 2026)
**Goal:** Take every Setup page a Thetis user reaches for and give it a NereusSDR equivalent; take every radio-state readout a Thetis user expects and expose it in the UI. After Phase 3P merges, NereusSDR's hardware / radio-control surface is **userland-complete** vs Thetis.

**Sub-phase stack (each a PR, stacked bottom-up):**

| Sub | Branch | PR | Status |
|---|---|---|---|
| **A** | `phase3p-a-p1-wire-parity` | #85 | Merged — P1 wire-bytes parity + **HL2 BPF + S-ATT bug fixes**. Per-board codec subclasses (`P1CodecStandard` for Hermes/Orion; `P1CodecHl2` for HL2 with mi0bot 6-bit mask + 0x40 enable + MOX TX/RX branch). Regression-freeze gate against pre-refactor JSON baseline proves 288 tuples byte-identical. |
| **B** | `phase3p-b-p2-wire-parity` | #91 | Merged — P2 wire-bytes parity with `P2CodecOrionMkII` / `P2CodecSaturn` subclasses (G8NJJ BPF1 override). `AlexFilterMap` shared with P1. New Hardware → Antenna/ALEX → Alex-1 / Alex-2 Filters sub-sub-tabs. ADC OVL per-ADC split in RxApplet. RX1 preamp toggle for OrionMKII family. Regression-freeze 36 P2 tuples byte-identical. |
| **C** | `phase3p-c-preamp-combo` | #92 | Merged — `RxApplet` preamp combo populates from `BoardCapabilities::preampItemsForBoard()` at construction + repopulates on connect. HL2 preamp corrected from 1-item to 4-item (anan100d set). `tst_preamp_combo` 17-assertion lock. |
| **D** | `phase3p-d-oc-matrix` | #94 | Merged — `OcMatrix` model (per-band × per-pin × per-mode bit storage + TX pin actions) + Hardware → OC Outputs Setup page (master toggles, per-band RX/TX matrices, TX pin action grid, USB BCD, external PA control, live pin-state LED stubs). Codec ocByte routing wired. Per-MAC persistence. |
| **E** | `phase3p-e-hl2-ioboard` | #95 | Merged — Closes long-deferred Phase 3I-T12 work. `IoBoardHl2` (33-register enum, I2C TLV queue, 12-step state machine) + `HermesLiteBandwidthMonitor` (mi0bot two-pointer byte-rate compute + NereusSDR throttle detection layer) + Hardware → HL2 I/O Setup page. `P1CodecHl2` I2C intercept on C&C compose. This supersedes the old Phase 3L placeholder. |
| **F** | `phase3p-f-accessories` | #96 | Merged — `AlexController` / `ApolloController` / `PennyLaneController` accessory models. Hardware → Antenna/ALEX → Antenna Control sub-sub-tab (14×3 per-band antenna grid + Block-TX safety). `RxApplet` antenna buttons auto-populate per band. `BoardCapabilities` `hasApollo` / `hasAlex` / `hasPennyLane` per-board gates (source-first corrected — only HPSDR-kit enables Apollo, not all ANAN family). |
| **G** | `phase3p-g-calibration` | #97 | Merged — Hardware → Calibration page (renamed from PA Calibration). 5 Thetis-1:1 group boxes (Freq Cal, Level Cal, HPSDR Freq Cal Diagnostic with 9-decimal correction factor + 10 MHz ref, TX Display Cal, existing PA Current). `CalibrationController` with `effectiveFreqCorrectionFactor()` wired into `P2RadioConnection` Hz→phase-word conversion. |
| **H** | `phase3p-h-status-hygiene` | #107 | Merged — Diagnostics → Radio Status dashboard (PA Status, Forward/Reflected/SWR, PTT Source, Connection Quality, Settings Hygiene) + 4 sibling sub-tabs (Connection Quality / Settings Validation / Export / Import / Logs). `RadioStatus` + `SettingsHygiene` + `PttSource` models owned by `RadioModel`. PA telemetry parsed from both P1 (parseEp6Frame C0 cases 0x08/0x10/0x18) and P2 (processHighPriorityStatus), scaled via Thetis per-board formulas. Live LED wire-up across Alex-1 / Alex-2 Filters (SliceModel-driven), OC Outputs pin-state, HL2 I/O register-table 40 ms poll. ADC Overload status-bar label left of STATION (Thetis ucInfoBar parity, 2 s auto-hide, yellow/red by hysteresis level). Dark-theme checkbox + radio-button fix in `SetupPage`. |
| **I-a** | `phase3p-i-antenna-routing` | #116 | **Merged** — 3P-I-a: Core per-band routing + UI gating (#98 fix). `AlexController` → `RadioConnection::setAntennaRouting` pump wired with 3 triggers (antennaChanged, bandChanged, onConnectionStateChanged). All 5 writeable antenna surfaces (VFO Flag, RxApplet, Setup-grid, SpectrumOverlayPanel combos, AntennaButtonItem) funnel through `AlexController` as the single source of truth; SliceModel caches via `refreshAntennasFromAlex`. Universal `kPopupMenu` dark-palette stylesheet fixes Ubuntu 25.10 GNOME dark-on-dark menus. HL2/Atlas hide all antenna UI (`!caps.hasAlex \|\| antennaInputCount < 3`). Byte-for-byte P1/P2 wire-lock tests + 10 new test cases + manual verification matrix. |
| **I-b** | `claude/unruffled-mcclintock-df7476` | #117 | **Merged (v0.2.3)** — 3P-I-b: RX-only antennas + SKU labels + Ext/Bypass flags + XVTR. Wires `AntennaRouting.rxOnlyAnt` + `rxOut` through P1 bank0 C3 bits 5-7 and P2 Alex0 bits **8-11** (plan/design-doc had bit 27 — `_TR_Relay` — caught + corrected during T5; Thetis `network.h:263-307` is authoritative). `SkuUiProfile` 14-SKU overlay drives per-product labels (RX1/RX2/XVTR vs EXT2/EXT1/XVTR vs BYPS/EXT1/XVTR) across Setup Antenna Control, VFO Flag BYPS 3rd button, AntennaButtonItem meter. `AlexController` gains 6 flags (Ext1/Ext2/RxOutOnTx mutual-exclusion trio + rxOutOverride + useTxAntForRx + xvtrActive), 5 persisted per-MAC. `RadioModel::applyAlexAntennaForBand` now full `Alex.cs:310-413` port (minus MOX/Aries → 3M-1): isTx branch, Ext-on-TX mapping, xvtrActive-from-band derivation, rx_out_override clamp. Setup → Antenna → Alex-2 Filters sub-tab gates on `caps.hasAlex2`. 32 new test cases + verification matrix §7-§8. Bench QA on ANAN-G2 / 100D / 8000DLE / Hermes / HL2 bare pending. |

**Phase 3P attribution infrastructure (added mid-flight after a `//DH1KLM` drop was caught during Phase H review):**
- `scripts/discover-thetis-author-tags.py` + `docs/attribution/thetis-author-tags.json` — discovery-driven contributor corpus, 19 authors, drift-detected in CI.
- `scripts/verify-inline-tag-preservation.py` — every cite must carry verbatim source tags; pre-commit + CI gated.
- `scripts/generate-contributor-indexes.py` — regenerates `thetis-contributor-index.md` + `thetis-inline-mods-index.md` mechanically (151 files / 2947 markers indexed).
- Attribution sweep closed 74 historical tag drops (`//DH1KLM`, `//MW0LGE`, `//G8NJJ`, `//W2PA`) across 22 files.

**User-facing outcome:** a Thetis user switching to NereusSDR sees every Setup page they expect (including Calibration, OC Outputs, Antenna Control, Alex-1/2 Filters, HL2 I/O, Hardware Config all 9 tabs), every status readout they're used to (PA temp/current/SWR, PTT source, connection quality, ADC overload warning), and every persistence boundary scoped per-MAC. HL2 bandpass filter switching and step attenuator both now work correctly per mi0bot's wire encoding.

### Phase 3N: Cross-Platform Packaging ✅ COMPLETE
**Goal:** Release builds for Linux, Windows, macOS.

Shipped: consolidated `release.yml` (prepare → build×3 → sign-and-publish), `/release` skill, GPG-signed alpha builds across Linux AppImage ×2 archs, macOS Apple Silicon DMG, Windows portable ZIP + NSIS installer. v0.1.2 → v0.1.4 → v0.1.7 → v0.2.0 → v0.2.1 → v0.2.2 → v0.2.3 all shipped via this pipeline.

### Phase 3O: VAX — Audio Routing & Cross-Platform Audio Engine
**Goal:** Ship NereusSDR's complete RX audio routing story — per-receiver VAX assignment, Thetis-grade Setup → Audio power-user surface, native VAX drivers on macOS/Linux, auto-detect for user-installed Windows virtual cables, optional Direct ASIO (cmASIO parity) engine.

**Design spec:** `docs/architecture/2026-04-19-vax-design.md` (brainstormed 2026-04-19, ready for implementation plan)

Scope:
- **`IAudioBus` abstraction** with five platform backends: `CoreAudioHalBus` (macOS HAL plugin, port of AetherSDR `VirtualAudioBridge`), `LinuxPipeBus` (PulseAudio module-pipe, port of AetherSDR `PipeWireAudioBridge`), `PortAudioBus` (Windows default + Mac/Linux fallback), `DirectAsioBus` (Windows opt-in, Thetis `cmasio.c` port with Thetis attribution), `CoreAudioBus` (Mac fallback).
- **macOS HAL plugin** at `hal-plugin/NereusSDRVAX.cpp` — 4 VAX outputs + 1 TX input as native CoreAudio devices. libASPL-based, POSIX shm IPC. Dev-ID-signed + notarized `.pkg` installer with macOS 14.4+ `killall coreaudiod` fallback.
- **Linux bridge** — `pactl`-loaded `module-pipe-source` × 4 + `module-pipe-sink` × 1, rebranded to `nereussdr-vax-*`. Works on both Pulse and PipeWire-via-pipewire-pulse. Stale-module cleanup on startup.
- **Windows BYO path** — `VirtualCableDetector` regex-matches VB-Audio family, VAC, Voicemeeter, Dante, FlexRadio DAX. First-run dialog pre-fills bindings; links to vendor install pages when none detected.
- ~~Optional Direct ASIO engine (Windows) — cmASIO parity~~ **Deferred** per 2026-04-19 GPL-3 compliance review (Steinberg ASIO SDK not GPL-compatible). PortAudio's built-in ASIO host API remains as the ASIO path. See plan's GPL Compliance Review section and spec §8.5 compliance note.
- **Routing model** — SmartSDR-style: `SliceModel.vaxChannel` (0..4) set via new `VaxChannelSelector` row on `VfoWidget`; speakers always-on unless muted; one VAX owns TX at a time (`TransmitModel.txOwnerSlot`).
- **`VaxApplet` (docked, ported from AetherSDR `DaxApplet`, renamed DAX→VAX)** — 4 channel strips with meter + gain + mute + device picker + assigned-slice tags; TX row with gain + meter.
- **Menu-bar `MasterOutputWidget`** — global volume + mute + right-click device picker; scroll-wheel fine-tune.
- **Setup → Audio page** — sub-tabs Devices / VAX / TCI (disabled placeholder) / Advanced. Per-device Driver API dropdown exposes MME / DirectSound / WDM-KS / WASAPI-shared / WASAPI-exclusive / ASIO on Windows; CoreAudio on Mac; ALSA / Pulse / PipeWire / JACK on Linux. Buffer size with derived ms; manual latency override; exclusive mode + event-driven + bypass-mixer options per WASAPI; full cmASIO control surface behind Engine radio.
- **First-run auto-detect dialog** — 5 scenarios per-platform (Windows cables-found / Windows none-found / Mac native / Linux native / rescan-on-new-cable).
- **AppSettings schema additions** — per-device driver API + format + buffer; per-VAX enabled + device + gain + mute; per-slice VAX channel; TX owner slot; master volume/mute/device; IVAC feedback parity controls (gain/slew/ring min-max/alpha); global toggles for IQ-to-VAX (reserved), mute-VAX-during-TX-on-other-slice, first-run-complete.
- **Settings migration** — single legacy `audio/OutputDevice` key migrates to `audio/Speakers/DeviceName` with sensible defaults; first-run dialog triggers.
- **Audio engine refactor** — replaces `QAudioSink`-only output with the `IAudioBus` model routing per-slice through `MasterMixer` + per-VAX taps.
- **Full end-to-end wiring** — every widget listed in spec §6 has its round-trip wiring (widget → model → action → persistence → feedback guard) specified before implementation. Zero unwired controls; this is a spec-enforced bar per the design doc.

Attribution (per `docs/attribution/HOW-TO-PORT.md`):
- AetherSDR-ported files (`VirtualAudioBridge`, `PipeWireAudioBridge`, `hal-plugin/AetherSDRDAX.cpp`, `DaxApplet`, `MeterSlider`) get NereusSDR port-citation + modification-history headers per rule 6 (AetherSDR has no per-file GPL headers; cite at project URL + primary author level).
- ~~Thetis-ported files (`DirectAsioBus`, `AsioSdkHost` from `ChannelMaster/cmasio.{h,c}` + `Console/clsCMASIOConfig.cs`) get byte-for-byte Thetis headers~~ — deferred with Direct ASIO.
- PROVENANCE rows added in the same commits that introduce the ports.
- `COMPLIANCE-INVENTORY.md` updated at end of phase documenting macOS HAL plugin's separate-binary / mere-aggregation status and the 2026-04-19 GPL review outcome.

Dependencies:
- Phase 3B (AudioEngine skeleton) — ✓
- Phase 3G-10 Stage 2 (`VfoWidget` tab structure and per-slice persistence) — ✓
- Phase 3G-8 (`SetupPage` base + STYLEGUIDE.md) — ✓

Reserves integration points for:
- **Phase 3J (TCI):** `IAudioBus` tap contract lets TCI subscribe to the same RX taps with no refactor.
- **Future:** NereusSDR-owned Windows signed driver can replace BYO cables transparently via `VirtualCableDetector`'s reserved "NereusSDR VAX N" pattern.

Explicitly out of scope (in this phase):
- TCI server and TCI audio streams (Phase 3J).
- TX mic DSP chain — compressor, TX EQ, leveler (Phase 3M-3).
- IQ-to-VAX activation (setting persisted but logs-and-ignores until future phase).
- NereusSDR-owned Windows virtual audio driver.

Success criteria (subset, see spec §14 for full list):
- All controls round-trip wired; zero unwired buttons.
- Thetis migrator can configure equivalent VAC1/VAC2 in Setup → Audio in under 5 minutes on Windows.
- Mac/Linux users see "NereusSDR VAX 1–4" + "NereusSDR TX" in WSJT-X audio picker with zero extra install.
- Windows user with VB-CABLE installed sees auto-detect suggestions on first launch.
- `scripts/verify-thetis-headers.py` + `scripts/check-new-ports.py` pass on all ported files.

---

## Recommended Next Steps: Phase 3Q + Phase 3M-1 (parallel pair after v0.2.3)

v0.2.3 shipped 2026-04-24 with the RX epic, NB family, 7-filter NR stack, PipeWire-native Linux audio, and Alex antenna integration all complete. Two phases come next, runnable in parallel:

- **Phase 3Q — Connection Workflow Refactor.** Triggered by an April 2026 user report: an HL2 across a WireGuard tunnel couldn't be reached through the existing manual-entry path, and the disconnect state gave no feedback beyond a frozen spectrum. Single state machine, new unicast-probe code path, modal ConnectionPanel polish (state pills, Last Seen column, ↻ Scan, inline Disconnect), rebuilt Add Radio dialog with model-aware SKU picker (16 SKUs by silicon family replacing the 9-board picker), Radio menu cleanup (no more four-item-three-aliases), spectrum disconnect overlay. Design spec at [docs/architecture/2026-04-26-connection-workflow-refactor-design.md](architecture/2026-04-26-connection-workflow-refactor-design.md). Touches `src/gui/{TitleBar, MainWindow, ConnectionPanel, AddCustomRadioDialog, SpectrumWidget}` + `src/core/{RadioDiscovery, RadioConnection, AppSettings}` + `src/models/RadioModel`.
- **Phase 3M-1 — Basic SSB TX.** TxChannel WDSP wrapper, mic input via QAudioSource, MOX state machine ported from `console.cs:29311-29650`, TX I/Q output to port 1029. Proves the TX path end-to-end and unblocks 3M-2..4, 3F, 3H. Touches `src/core/TxChannel` (new), the AudioEngine input path, RadioConnection TX paths, and the MOX state in RadioModel.

3Q and 3M-1 have no file-level overlap and ship in parallel. 3Q is the smaller piece and is expected to land first — it directly unblocks remote-operating users on Layer-3 VPNs and addresses the most-visible UX complaint, while 3M-1 is a multi-week effort that brings a whole new subsystem online.

Phases 3A–3E, 3G-1 through 3G-8, and 3-UI are all complete. The radio connects,
demodulates audio, renders live GPU spectrum + waterfall, supports full VFO tuning with
CTUN panadapter mode, has a complete meter system with 31 item types (18 passive + 13
interactive) including button grids, VFO display, and clock, has a full Thetis-parity
Container Settings Dialog with MMIO external-data subsystem, and — as of 3G-8 — a fully
wired Display setup category where every Spectrum Defaults / Waterfall Defaults / Grid
& Scales control routes through to the renderer live on both the QPainter fallback and
the QRhi/Metal GPU path.

The next meaningful steps:

- **3G-9 (Display Refactor)** — ✅ **All three sub-phases shipped in v0.1.5** (3G-9a audit + tooltips + slider readouts in PR #25; 3G-9b smooth defaults + Clarity Blue palette; 3G-9c ClarityController adaptive tuning + NoiseFloorEstimator + Re-tune button + per-band Clarity memory).
- **3G-10 (RX DSP Parity + AetherSDR Flag Port)** — ✅ **Complete.** Stage 1 (PRs #28 + #30): VfoWidget visual shell with 4×2 DSP grid, mode containers, tooltip coverage test. Stage 2: 10 WDSP feature slices wired (AGC-adv, EMNR, SNB, APF, squelch, mute/pan/binaural, NB2 polish, RIT/XIT, frequency lock, mode containers), per-slice-per-band persistence, Thetis-first tooltips. CW autotune deferred (no WDSP API).
- **3G-13 (Step Attenuator & ADC Overload)** — ✅ **Merged in v0.1.5 (PR #34).** StepAttenuatorController with Classic (Thetis 1:1) and Adaptive (NereusSDR attack/hold/decay with per-band memory) auto-att modes. P1/P2 adcOverflow emission, ADC OVL status badge (yellow/red, per-ADC), Setup→General→Options page, RxApplet ATT/S-ATT row with per-model preamp items from Thetis SetComboPreampForHPSDR (console.cs:40755), stepAttMaxDb 31/61 from setup.cs:15765, per-MAC persistence, 9 unit tests. Smoke-tested on ANAN-G2. **Note:** HL2 ATT logic may need cross-checking against mi0bot/Thetis-HL2 fork before HL2 field testing.
- **3G-14 (💡 AI-Assisted Issue Reporter)** — ✅ **Merged in v0.1.6 (PR #36).** Lightbulb menu-bar corner widget + version-check gate + AI-assisted issue dialog ported from AetherSDR TitleBar. Provider buttons, structured prompt, `feature_request.yml` / `bug_report.yml` integration.
- **3G-RX-Epic v0.2.3 (dBm strip + NB family + 7-filter NR + PipeWire)** — ✅ Sub-A AetherSDR-style dBm scale strip with wheel-zoom range and hover crosshair; Sub-B full Thetis NB/NB2/SNB family port via `NbFamily` wrapper; Sub-C-1 7-filter NR stack on VFO flag DSP grid (NR1/NR2/NR3 RNNR rnnoise / NR4 SBNR libspecbleach / DFNR DeepFilterNet3 / MNR Apple Accelerate MMSE-Wiener / ANF) with `DspParamPopup` right-click quick controls; Phase 3O Linux PipeWire-native audio bridge supersedes 0.2.2 pactl fallback.
- **3M-1 (Basic SSB TX)** (formerly 3I-1; renumbered after Phase 3I became the radio connector port) — **NEXT.** TxChannel WDSP wrapper, mic input, MOX state machine, TX I/Q output. Proves the TX path end-to-end and unblocks 3M-2..4, 3F, 3H.

Execution order: **3M-1..4 → 3F → 3H → 3J+** (all 3G-* RX prep and 3P all-board parity have shipped; TX epic is the next major milestone)

### Phase Dependencies

```
3G-4 → 3G-5 → 3G-6    (meter system, sequential)

3G-8 → 3G-9a → 3G-9b → 3G-9c    (display surface polish, sequential; 3G-9c gated on research doc)
3G-8 → 3G-10 Stage1 ✓ → 3G-10 Stage2    (RX DSP + AetherSDR flag port; parallel with 3G-9)

3M-1 → 3M-2 → 3M-3 → 3M-4 → 3F → 3H    (TX then multi-RX)
                               ↑
                       PureSignal must complete before 3F because
                       UpdateDDCs() state machine includes PS DDC
                       states (DDC0+DDC1 sync at 192kHz)
```

3G-9, 3G-10, 3G-13, and 3G-14 touch disjoint subsystems from each other and from 3M-* — they can all run in parallel if desired. 3G-9 owns the Display setup surface; 3G-10 owns the VFO flag and RX DSP wiring; 3G-13 owns the step attenuator + ADC overload protection (protocol layer + Setup Options + RxApplet ATT row + status bar); 3G-14 owns the 💡 issue reporter (menu bar corner widget + dialog).

Independent phases (can start anytime): **3Q (Connection Workflow Refactor — design complete, ready to plan; runs in parallel with 3M-1)**, 3G-14 (issue reporter — complete), 3J (TCI — depends on 3O IAudioBus contract), 3K (CAT), 3L (P1), 3M (Recording), **3O (VAX audio routing — depends on 3B, 3G-8, 3G-10 Stage 2; all complete)**.

---

## Documented / Deferred Features

Features recognized but not on the active roadmap. Revisit based on user demand or when
prerequisite infrastructure exists.

| Feature | Why Deferred | Revisit When |
|---|---|---|
| N1MM+ UDP integration | Not needed to ship | After 3K (CAT framework) |
| DVK (Digital Voice Keyer) | Contest feature, not core | After 3M (recording framework) |
| Andromeda front panel | Niche hardware (G8NJJ serial) | User demand |
| RA (signal level recorder) | Low priority; HistoryItem covers similar ground | After 3G-4 |
| Quick Recall pad | Nice-to-have UX | Post-ship |
| Finder (settings search) | Nice-to-have UX | Post-ship |
| Wideband display | Research done (`wideband-adc-brainstorm.md`) | After 3L (P1) |
| Discord integration | Thetis-specific, niche | Unlikely |
| Phase/Phase2 display | I/Q Lissajous, niche diagnostic | User demand |
| Panascope/Spectrascope | Combined display modes | User demand |
| External amp monitoring | PGXL/TGXL are FlexRadio-specific | Post-ship, design for OpenHPSDR amps |
| DRM/SPEC modes | Listed but no implementation detail | When digital mode support designed |

---

## Plan Review History

| Date | Scope | Document |
|---|---|---|
| 2026-04-10 | Full plan review: Thetis/AetherSDR deep-dive, feature gap analysis, phase restructuring, container/PureSignal/TX architecture | [2026-04-10-plan-review.md](architecture/reviews/2026-04-10-plan-review.md) |
| 2026-04-19 | Added Phase 3O (VAX — Audio Routing & Cross-Platform Audio Engine). Design brainstormed against Thetis VAC/cmASIO, AetherSDR DaxApplet/VirtualAudioBridge/PipeWireAudioBridge, and reference UIs (Rogue Amoeba Loopback, Dante Controller, RME TotalMix, qpwgraph). Chose AetherSDR-style routing + VFO-flag VAX selector + docked VaxApplet over patchbay/matrix alternatives. Windows BYO-with-auto-detect chosen over signed-kernel-driver for v1; TCI scoped out to Phase 3J with integration points reserved. Also audited status of 3N (marked complete) and added VAX row to Objective Cross-Check. | [2026-04-19-vax-design.md](architecture/2026-04-19-vax-design.md) |
| 2026-04-19 | Implementation plan authored for Phase 3O with pre-execution GPL-3 compliance review. **Dropped Direct ASIO engine** from the phase: Steinberg ASIO SDK is not GPL-3 compatible; linking it into distributed NereusSDR binaries would violate both the ASIO SDK terms and GPL-3. PortAudio's built-in ASIO host API remains as the ASIO path. All other components (AetherSDR GPL-3 ports, libASPL MIT, PortAudio MIT, Qt6/FFTW3/WDSP) verified compatible. Compliance inventory update folded into final verification task. | [2026-04-19-phase3o-vax-plan.md](architecture/2026-04-19-phase3o-vax-plan.md) |

---

## Menu Bar Layout

Combines AetherSDR's organized hierarchy with Thetis's quick-access approach:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ File │ Radio │ View │ DSP │ Band │ Mode │ Containers │ Tools │ Help         │
└──────────────────────────────────────────────────────────────────────────────┘
```

### File
- Settings... (opens Setup dialog)
- Profiles ▸ (Profile Manager, Import/Export, [user profiles])
- Quit (Ctrl+Q)

### Radio
- Discover... (find radios on network)
- Connect (connect to selected/last radio)
- Disconnect
- ─────
- Radio Setup... (hardware config, network, ADC cal)
- Antenna Setup... (Alex relay config, antenna ports)
- Transverters... (XVTR offset config)
- ─────
- Protocol Info (show connected radio model, firmware, protocol version)

### View
- Pan Layout ▸ (1-up, 2v, 2h, 2x2, 12h)
- Add Panadapter / Remove Panadapter
- ─────
- Band Plan ▸ (Off, Small, Medium, Large + ARRL/IARU region select)
- Display Mode ▸ (Panadapter, Waterfall, Pan+WF, Scope)
- ─────
- UI Scale ▸ (100%, 125%, 150%, 175%, 200%)
- Dark Theme / Light Theme
- Minimal Mode (hide spectrum, controls only)
- ─────
- Keyboard Shortcuts...
- Configure Shortcuts...

### DSP (quick toggles — checkboxes in menu for fast access)
- ☐ NR (Noise Reduction)
- ☐ NR2 (Spectral NR)
- ☐ NB (Noise Blanker)
- ☐ NB2
- ☐ ANF (Auto Notch Filter)
- ☐ TNF (Tracking Notch Filter)
- ☐ BIN (Binaural)
- ─────
- AGC ▸ (Off, Slow, Medium, Fast, Custom)
- ─────
- Equalizer... (opens EQ dialog)
- PureSignal... (opens PureSignal controls)
- Diversity... (opens diversity/ESC controls)

### Band (quick band switching)
- HF ▸ (160m, 80m, 60m, 40m, 30m, 20m, 17m, 15m, 12m, 10m, 6m)
- VHF ▸ (2m, 70cm, VHF0-13)
- GEN ▸ (General coverage bands)
- WWV
- ─────
- Band Stacking... (manage per-band frequency/mode/filter memory)

### Mode (quick mode switching)
- LSB / USB / DSB
- CWL / CWU
- AM / SAM
- FM
- DIGL / DIGU
- DRM / SPEC

### Containers
- New Container... (create floating or docked container)
- Container Settings... (opens container config dialog)
- ─────
- Reset to Default Layout
- ─────
- [List of all containers with show/hide checkboxes]

### Tools
- CWX... (CW macros and keyer)
- Memory Manager...
- CAT Control... (rigctld config)
- TCI Server... (WebSocket TCI config)
- DAX Audio... (virtual audio channels)
- ─────
- MIDI Mapping...
- Macro Buttons...
- ─────
- Network Diagnostics...
- Support Bundle...

### Help
- Getting Started...
- NereusSDR Help...
- Understanding Data Modes...
- ─────
- What's New...
- About NereusSDR

### 💡 Issue Reporter (menu bar corner widget)
- Always-visible amber 💡 button in menu bar corner (right side)
- Version check gate → AI-assisted issue dialog
- Provider buttons: Claude, ChatGPT, Gemini, Grok, Perplexity
- "Submit Your Idea" → `feature_request.yml` template
- "Report a Bug" → `bug_report.yml` template
- Ported from AetherSDR `TitleBar::showFeatureRequestDialog()`

---

## GUI Design: Container Mapping (Thetis → NereusSDR)

### Layout Architecture

NereusSDR follows AetherSDR's 3-zone layout with applet panel:

```
┌─────────────────────────────────────────────────────────┐
│ MenuBar                                                  │
├────────┬──────────────────────────────────┬──────────────┤
│ Conn   │  PanadapterStack (center)       │ AppletPanel  │
│ Panel  │  Up to 4 pans (5 layout modes)  │ (right, scrollable)
│ (popup)│  Each pan: SpectrumWidget +      │ Drag-reorder │
│        │  VfoWidget overlay + SMeter      │ Float/dock   │
├────────┴──────────────────────────────────┴──────────────┤
│ StatusBar                                                │
└─────────────────────────────────────────────────────────┘
```

### Complete Thetis Container → NereusSDR Applet Mapping

Thetis has 35 panels (31 PanelTS + 4 plain Panel), 11 group boxes, and ~16 spawned forms.
NereusSDR maps these into container item types + dialogs (see Phase 3G):

#### Always-Available Applets (in AppletPanel)

| NereusSDR Applet | Thetis Containers Absorbed | Controls |
|---|---|---|
| **RxApplet** | panelDSP, panelFilter, panelSoundControls, panelVFOLabels, panelVFOALabels | Mode, filter presets, AGC combo+slider, AF/RF gain, squelch, NB/NB2/NR/ANF toggles, preamp, step attenuator |
| **TxApplet** | panelOptions (MOX/TUN), panelPower, grpMultimeterMenus | Fwd power gauge, SWR gauge, RF power slider, tune power slider, TX profile, TUNE/MOX/ATU buttons |
| **MeterApplet** | grpMultimeter, grpRX2Meter, panelMeterLabels | S-meter (RX), power/SWR/ALC (TX), selectable meter modes |

#### Mode-Dependent Applets (auto-show/hide on mode change)

| NereusSDR Applet | Thetis Containers | Visibility |
|---|---|---|
| **PhoneCwApplet** (QStackedWidget) | panelModeSpecificPhone + panelModeSpecificCW | Phone page: VOX, noise gate, mic, CPDR, TX EQ, TX filter. CW page: speed, pitch, sidetone, iambic, breakin/QSK, APF (grpCWAPF, grpSemiBreakIn) |
| **FmApplet** | panelModeSpecificFM | CTCSS freq/enable, deviation (2k/5k), offset, simplex, FM memory, FM TX profile |
| **DigitalApplet** | panelModeSpecificDigital, grpVACStereo, grpDIGSampleRate | VAC stereo, sample rate, RX/TX VAC gain, digital TX profile |

#### On-Demand Applets (toggle via button row)

| NereusSDR Applet | Thetis Containers | Purpose |
|---|---|---|
| **EqApplet** | EQForm (spawned form) | 10-band graphic EQ for RX and TX, presets |
| **TunerApplet** | ATU controls from panelOptions | ATU status, SWR capture, tune timeout |
| **CatApplet** | CAT settings | rigctld channels, virtual serial ports |
| **PureSignalApplet** | PSForm (spawned form) | Feedback RX status, calibration, correction enable |
| **DiversityApplet** | DiversityForm, panelMultiRX | Sub-RX gain/pan, diversity combine, ESC beamforming |
| **AmpApplet** | External PA controls | Amp status, band relay control |

#### Per-Pan Components (inside each PanadapterApplet)

| Component | Thetis Source | Notes |
|---|---|---|
| **SpectrumWidget** | pnlDisplay (rendering surface) | GPU FFT + waterfall |
| **VfoWidget** (overlay) | grpVFOA/grpVFOB, panelVFO | Frequency display, tabbed sub-menus (Audio/DSP/Mode/RIT-XIT/DAX) |
| **SMeterWidget** | S-meter portion of grpMultimeter | Per-pan signal level |
| **CwxPanel** (dockable below spectrum) | CWX form | CW message macros, decode text |

#### Dialogs (modal/modeless, not applets)

| NereusSDR Dialog | Thetis Source | Purpose |
|---|---|---|
| **SetupDialog** | Setup form (huge) | Hardware config, network, ADC cal, relay control, Alex antenna |
| **MemoryDialog** | MemoryForm, frmBandStack2 | Channel memory, band stacking registers |
| **FilterDialog** | FilterForm (×2) | Custom filter definition per mode |
| **XvtrDialog** | XVTRForm | Transverter frequency offset config |
| **ProfileDialog** | TX profile management | Save/load TX processing profiles |

#### VfoWidget Sub-Menus (tabbed inside floating overlay)

| Tab | Thetis Containers | Controls |
|---|---|---|
| **Audio** | panelSoundControls (AF/pan/mute) | AF gain slider, pan slider, mute, squelch on/off + level, AGC mode + threshold |
| **DSP** | panelDSP | NB, NB2, NR, NR2, ANF, BIN toggles. APF slider (CW). RTTY mark/shift (RTTY). DIG offset (DIG) |
| **Mode** | panelMode, panelFilter | Mode combo, 3 quick-mode buttons, filter preset grid (mode-dependent) |
| **RIT/XIT** | panelVFO (RIT/XIT section) | RIT on/off + offset, XIT on/off + offset, zero beat |
| **DAX** | VAC controls | DAX channel select, IQ streaming enable |

#### Band Selection (in VfoWidget or separate BandApplet)

| Component | Thetis Source | Controls |
|---|---|---|
| **HF bands** | panelBandHF | 160m, 80m, 60m, 40m, 30m, 20m, 17m, 15m, 12m, 10m, 6m, WWV, GEN |
| **VHF bands** | panelBandVHF | VHF0-VHF13 |
| **GEN bands** | panelBandGEN | GEN0-GEN13 |
| **Band stacking** | grpVFOBetween | Quick save/restore per band, tune step, VFO A↔B copy/swap |

#### RX2 Strategy

When RX2 is enabled, its 10+ panels (panelRX2Mode, panelRX2Filter, panelRX2DSP, panelRX2Power, panelRX2RF, panelRX2Mixer, panelRX2Display, grpRX2Meter) are NOT separate applets. Instead:

- RX2 gets its own PanadapterApplet in the PanadapterStack (with its own SpectrumWidget + VfoWidget)
- Clicking RX2's pan makes it the active pan
- AppletPanel's RxApplet automatically rewires to the RX2 SliceModel (same applet class, different data)
- All DSP controls in RxApplet reflect RX2's independent state
- This is exactly how AetherSDR handles multi-slice — no code duplication

---

## Skin System: 4-Pan Support

### Design Change from Phase 2D

The original Phase 2D design constrained legacy skins to 2 panadapters.
**Updated approach:** NereusSDR skins always support up to 4 pans.

### Thetis-Inspired Skin Format (Extended)

Instead of strict Thetis skin compatibility (which assumes 2-pan WinForms layout), NereusSDR uses a **Thetis-inspired** skin format that:

1. **Keeps the JSON + XML + PNG structure** from Thetis skins
2. **Extends XML to support 4 pan regions** with configurable layout
3. **Maps control names** from Thetis skins to NereusSDR applet widgets
4. **Adds pan layout definition** to the skin XML

### Extended Skin XML Schema

```xml
<NereusSDRSkin>
  <Meta>
    <Name>Dark Operator</Name>
    <Version>1.0</Version>
    <Author>KG4VCF</Author>
    <BasedOn>Thetis Dark Theme</BasedOn>
  </Meta>

  <Layout>
    <!-- Skin can define preferred pan layout -->
    <PanLayout>2v</PanLayout>  <!-- or 1, 2h, 2x2, 12h -->
    <MaxPans>4</MaxPans>        <!-- user can always add more up to 4 -->
    <AppletPanelWidth>320</AppletPanelWidth>
  </Layout>

  <Theme>
    <!-- Colors map to STYLEGUIDE.md roles -->
    <Background>#0f0f1a</Background>
    <TextPrimary>#c8d8e8</TextPrimary>
    <Accent>#00b4d8</Accent>
    <ButtonBase>#1a2a3a</ButtonBase>
    <Border>#205070</Border>
    <!-- ... full color palette -->
  </Theme>

  <Controls>
    <!-- Button state images (same 8-state as Thetis) -->
    <Control name="btnMOX" type="button">
      <NormalUp>Controls/btnMOX/NormalUp.png</NormalUp>
      <NormalDown>Controls/btnMOX/NormalDown.png</NormalDown>
      <MouseOverUp>Controls/btnMOX/MouseOverUp.png</MouseOverUp>
      <!-- ... -->
    </Control>

    <!-- Thetis control names are mapped to NereusSDR widgets -->
    <Control name="chkNR" type="toggle" mapTo="RxApplet.nrButton"/>
    <Control name="comboAGC" type="combo" mapTo="RxApplet.agcCombo"/>
  </Controls>

  <ConsoleBackground>Console/Console.png</ConsoleBackground>
</NereusSDRSkin>
```

### Skin Loading Flow

1. Parse skin ZIP → extract XML + PNGs
2. Apply `<Theme>` colors to global stylesheet (overrides STYLEGUIDE defaults)
3. Apply `<Layout>` preferred pan layout (user can override)
4. Apply `<Controls>` button images via `border-image` stylesheet
5. Apply `<ConsoleBackground>` to MainWindow
6. 4 pans always supported — skin just sets the **default** layout

### Legacy Thetis Skin Import

For actual Thetis skin ZIPs:
1. Parse Thetis XML format (different schema)
2. Map Thetis control names → NereusSDR control names via lookup table
3. Apply what we can (colors, button images)
4. Ignore layout constraints (always allow 4 pans)
5. Log unmapped controls for debugging

---

## Container System (Critical Feature)

Thetis has a sophisticated configurable container/docking system (under Settings → Appearance → Multi-Meters). NereusSDR must replicate this.

### Thetis Container Architecture

- **ucMeter** — User control representing a container (title bar + content area + resize handle)
- **frmMeterDisplay** — Floating Form window for popped-out containers
- **MeterManager** — Singleton managing all containers (create, destroy, float, dock, persist)

**Key capabilities:**
- Containers can be **docked** (inside main window) or **floating** (separate window, any screen)
- **Drag** to reposition (docked: within console bounds; floating: anywhere including other monitors)
- **Resize** via corner grab handle
- **Pin-on-top** to keep floating containers above all windows
- **Axis lock** (8 positions: TL, T, TR, R, BR, B, BL, L) — anchors docked containers to window edges so they maintain relative position on resize
- **Per-container settings:** border, background color, title bar visibility, RX1/RX2 data source, show on RX/TX, auto-height, lock against changes
- **Macro integration** — macro buttons can show/hide specific containers
- **Import/export** — containers as self-contained Base64-encoded strings
- **Multi-monitor DPI** handling for floating windows
- **Touch support** for tablet drag/resize
- **Full persistence** — all state serialized to database, restored on startup

### NereusSDR Container System Design

**Approach:** Extend AetherSDR's existing applet float/dock system with Thetis's container configurability.

AetherSDR already has:
- AppletPanel with drag-reorderable applets
- FloatingAppletWindow for popped-out applets
- Applet show/hide toggle buttons

We need to add:
1. **ContainerWidget** (equivalent to ucMeter) — base container with:
   - Title bar (drag handle, float/dock button, pin-on-top, axis lock selector, settings gear)
   - Content area (holds one or more meter items / applet content)
   - Resize handle (bottom-right corner)
   - Configurable: border, background color, title visibility

2. **FloatingContainer** (equivalent to frmMeterDisplay) — QWidget with:
   - Qt::Window | Qt::Tool flags for separate window
   - TopMost via Qt::WindowStaysOnTopHint (pin-on-top)
   - Per-monitor DPI via QScreen
   - Save/restore position and size per container ID

3. **ContainerManager** — owns all containers:
   - Create/destroy containers
   - Float/dock transitions (reparent ContainerWidget between MainWindow and FloatingContainer)
   - Axis-lock positioning on main window resize
   - Serialization: save all container state to AppSettings
   - Restore: rebuild containers from saved state on startup
   - Macro visibility control

4. **Container Settings Dialog** — in Setup:
   - Container selector dropdown
   - Per-container: border, background, title, RX source, show on RX/TX, auto-height, lock
   - Meter/content items: available list, in-use list, add/remove/reorder
   - Copy/paste settings between containers
   - Add RX1/RX2 container, duplicate, delete, recover (off-screen rescue)
   - Import/export

5. **Container Content Types** — what can go inside a container:
   - S-meter (various modes: signal, ADC, AGC gain)
   - Power/SWR/ALC gauges
   - Compression meter
   - Clock/UTC display
   - VFO frequency display
   - Band scope (mini waterfall)
   - Custom meter items

### Key Class Interfaces

```cpp
namespace NereusSDR {

enum class AxisLock { Left, TopLeft, Top, TopRight, Right,
                      BottomRight, Bottom, BottomLeft };

class ContainerWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool floating READ isFloating NOTIFY floatingChanged)
    Q_PROPERTY(bool pinOnTop READ isPinOnTop WRITE setPinOnTop)
    Q_PROPERTY(AxisLock axisLock READ axisLock WRITE setAxisLock)
    Q_PROPERTY(bool locked READ isLocked WRITE setLocked)
public:
    // Float/dock
    bool isFloating() const;
    void setFloating(bool floating);  // reparent to FloatingContainer or MainWindow
    
    // Axis lock (docked position anchoring)
    AxisLock axisLock() const;
    void setAxisLock(AxisLock lock);
    void updateDockedPosition(const QSize& consoleDelta);  // called on main window resize
    
    // Content
    void addMeterItem(MeterItem* item);
    void removeMeterItem(const QString& itemId);
    void reorderMeterItems(const QStringList& itemIds);
    
    // Serialization
    QString serialize() const;         // → pipe-delimited string
    static ContainerWidget* deserialize(const QString& data, QWidget* parent);

signals:
    void floatingChanged(bool floating);
    void floatRequested();
    void dockRequested();
    void settingsRequested();
};

class ContainerManager : public QObject {
    Q_OBJECT
public:
    ContainerWidget* createContainer(int rxSource);  // 1=RX1, 2=RX2
    void destroyContainer(const QString& id);
    void floatContainer(const QString& id);
    void dockContainer(const QString& id);
    void recoverContainer(const QString& id);  // off-screen rescue
    
    void saveState();    // persist all containers to AppSettings
    void restoreState(); // rebuild from AppSettings on startup
    
    void setContainerVisible(const QString& id, bool visible);  // macro control
    
    QList<ContainerWidget*> allContainers() const;
    ContainerWidget* container(const QString& id) const;
};

} // namespace NereusSDR
```

### Unified Container Architecture

**There is NO separate "AppletPanel" vs "ContainerManager."** Everything is a container.

The right sidebar in AetherSDR's layout is just one container that happens to be docked on the right side by default. It is not special — users can:

- **Add/remove any widget** from any container (including the default right panel)
- **Create new containers** freely (docked anywhere or floating on any monitor)
- **Move items between containers** (drag an applet from the right panel into a floating container)
- **Remove items from the right panel** entirely (it can be empty or hidden)
- **Put any widget type in any container**: meters, DSP controls, VFO display, EQ, CW controls, etc.

### Widget Types (Content Items)

These are the building blocks that can be placed in any container:

| Widget Type | Description | Slice-bound? |
|---|---|---|
| **RxControls** | Mode, filter, AGC, AF/RF, squelch, NB/NR/ANF | Yes (follows active slice) |
| **TxControls** | Power, tune, MOX, ATU, TX profile | No (global TX) |
| **SMeter** | Signal meter (multiple modes: S-units, dBm, ADC) | Yes |
| **PowerMeter** | Forward power + SWR + ALC gauges | No (global TX) |
| **EqControls** | 10-band RX/TX graphic EQ | Per-slice or global |
| **CwControls** | Speed, pitch, breakin, QSK, APF, sidetone | Global CW |
| **FmControls** | CTCSS, deviation, offset, simplex | Per-slice |
| **PhoneControls** | VOX, noise gate, mic gain, CPDR, TX EQ/filter | Global TX |
| **DigitalControls** | VAC stereo, sample rate, VAC gain | Per-slice |
| **VfoDisplay** | Frequency readout + band buttons + RIT/XIT | Yes |
| **BandButtons** | HF/VHF/GEN band selection grid | Yes |
| **PureSignalStatus** | Calibration, feedback, correction status | Global |
| **DiversityControls** | Sub-RX gain, ESC beamforming | Per-diversity pair |
| **CatStatus** | rigctld channels, virtual serial ports | Global |
| **ClockDisplay** | UTC/local time | Global |
| **CustomMeter** | User-configurable meter (any WDSP meter type) | Per-slice or global |

### Default Layout (Out of Box)

On first launch, NereusSDR creates a **default right-side container** pre-loaded with:
1. RxControls
2. TxControls
3. PowerMeter
4. SMeter

This looks like AetherSDR's applet panel. But the user can immediately:
- Drag RxControls out into a floating container on monitor 2
- Add an EqControls widget to the right panel
- Create a new floating container with just SMeter + PowerMeter
- Remove everything from the right panel and hide it entirely
- Dock a CwControls container to the bottom of the main window

### Container Behavior

Every container supports:
- **Dock** inside the main window (any edge, any position, axis-locked on resize)
- **Float** as an independent window (any monitor, pin-on-top optional)
- **Add/remove** widget items (via settings gear or drag)
- **Reorder** items within the container (drag up/down)
- **Resize** (corner grab handle)
- **Lock** (prevent accidental changes)
- **Per-container settings**: border, background color, title, RX source (which slice), show on RX/TX
- **Macro control**: programmable buttons can show/hide any container

### The "Right Panel" Is Just Default Container #0

```
Default layout on first run:

┌─────────────────────────────────────────────────────────┐
│ MenuBar                                                  │
├────────┬──────────────────────────────────┬──────────────┤
│        │  PanadapterStack (center)       │ Container #0 │
│        │  ┌────────────────────────┐     │ (docked right)│
│        │  │ Pan A: Spectrum+WF     │     │ ┌──────────┐ │
│        │  │ + VfoWidget overlay    │     │ │RxControls│ │
│        │  ├────────────────────────┤     │ │TxControls│ │
│        │  │ Pan B: Spectrum+WF     │     │ │PowerMeter│ │
│        │  └────────────────────────┘     │ │SMeter    │ │
│        │                                 │ └──────────┘ │
├────────┴──────────────────────────────────┴──────────────┤
│ StatusBar                                                │
└─────────────────────────────────────────────────────────┘

User customized layout:

┌─────────────────────────────────────────────────────────┐
│ MenuBar                                                  │
├──────────────────────────────────────────┬───────────────┤
│  PanadapterStack (center, full width)   │ Container #0  │
│  ┌──────────┬──────────┐               │ (docked right, │
│  │ Pan A    │ Pan B    │               │  narrow)       │
│  ├──────────┼──────────┤               │ ┌───────────┐  │
│  │ Pan C    │ Pan D    │               │ │VfoDisplay │  │
│  └──────────┴──────────┘               │ │BandButtons│  │
│                                         │ └───────────┘  │
├──────────────────────────────────────────┴───────────────┤
│  Container #1 (docked bottom, axis: BOTTOMLEFT)          │
│  ┌──────────┬──────────┬──────────┐                      │
│  │ SMeter   │PowerMeter│CwControls│                      │
│  └──────────┴──────────┴──────────┘                      │
├──────────────────────────────────────────────────────────┤
│ StatusBar                                                │
└─────────────────────────────────────────────────────────┘

  + Floating Container #2 (on monitor 2):
    ┌────────────────────┐
    │ RxControls (RX1)   │
    │ TxControls         │
    │ EqControls         │
    └────────────────────┘

  + Floating Container #3 (pinned on top):
    ┌──────────────┐
    │ PureSignal   │
    └──────────────┘
```

### Why This Matters

This unified approach means:
- **No artificial separation** between "applets" and "containers"
- Users with simple needs get a sensible default (right panel with basics)
- Power users can build completely custom layouts across multiple monitors
- Skin system only needs to define container layouts + content assignments
- Same persistence model for everything (ContainerManager saves all)

---

## Build Verification

After each priority:
- [ ] CI passes (cmake configure + build)
- [ ] No new compiler warnings with -Wall -Wextra -Wpedantic
- [ ] App launches and doesn't crash
