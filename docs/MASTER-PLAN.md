# NereusSDR — Master Implementation Plan

## Context

NereusSDR is a ground-up port of Thetis (OpenHPSDR SDR console) from C# to Qt6/C++20, using AetherSDR as the architectural template. We've completed the scaffolding (Phase 0), architectural analysis (Phase 1), and architecture design (Phase 2). Now moving to Phase 3: Implementation.

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
| TCI protocol | Planned as Phase 3J | Not started |
| Cross-platform packaging | CI workflows in place (AppImage, Windows, macOS) | CI done |
| AetherSDR architecture patterns | RadioModel hub, signal/slot, worker threads, AppSettings | Adopted |
| Radio-authoritative state | Designed per AetherSDR pattern | Adopted |
| Multi-receiver ADC/DDC mapping | Full signal chain analyzed (2F), UpdateDDCs() porting needed | Design done |

**All project brief objectives are covered.** Feature gap analysis completed 2026-04-10
(see `docs/architecture/reviews/2026-04-10-plan-review.md` for full audit).

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

### Phase 3G-3: Core Meter Groups
**Goal:** Ship the meters operators expect on day one.

Scope:
- SMeterWidget: dedicated QWidget port of AetherSDR's SMeterWidget for pixel-identical S-meter
- NeedleItem (MeterItem system): composable arc needle for custom meter configurations
- Default presets: Power/SWR, ALC bar, Mic/Comp bars (via MeterItem system)
- Default Container #0 pre-loaded with: SMeterWidget (top), Power/SWR + ALC (bottom)
- TX MeterBinding constants 100-105 (stubs until TxChannel in Phase 3I-1)
- Data binding: SIGNAL_STRENGTH, AVG_SIGNAL_STRENGTH, ADC, AGC_GAIN, PWR, REVERSE_PWR, SWR, MIC, COMP, ALC

Status:
- ✅ Multi-layer MeterItem infrastructure (participatesIn + paintForLayer)
- ✅ NeedleItem with arc rendering, smoothing, peak hold, S-unit scaling
- ✅ TX MeterBinding constants (100-105)
- ✅ Preset factories (S-Meter, Power/SWR, ALC, Mic, Comp)
- ✅ Default Container #0 layout (S-Meter 55% + Power/SWR 30% + ALC 15%)
- 🔄 SMeterWidget: direct AetherSDR port for pixel-identical fidelity (next)

Verification: Live S-meter needle, Power/SWR during TX, correct readings vs Thetis on same signal.

### Phase 3G-4: Advanced Meter Items
**Goal:** Visual flair — items that make it look like a real radio console.

Scope:
- HistoryItem (HISTORY) — scrolling signal graph, ring buffer texture, `meter_history.vert/.frag`
- MagicEyeItem (MAGIC_EYE) — animated vacuum tube iris, `meter_eye.vert/.frag`
- DialItem (DIAL_DISPLAY) — analog dial with rotating pointer
- LedItem (LED) — on/off/blink, FadeCoverItem, WebImageItem, CustomMeterBarItem

Verification: History graph scrolling, magic eye responding to signal strength.

### Phase 3G-5: Interactive Meter Items
**Goal:** Button grids and frequency displays inside containers.

Scope:
- ButtonItem (GPU face + QWidget overlay): Band, Mode, Filter, Antenna, TuneStep, VoiceRecordPlay, Other
- VfoDisplayItem, ClockItem, SpacerItem, ClickBoxItem

Verification: Band buttons switch bands, mode buttons switch modes, all route through SliceModel.

### Phase 3G-6: Container Settings Dialog
**Goal:** Full user customization — the composability UI.

Scope:
- Container settings dialog: item palette, current item list (drag reorder), per-item property panel, live preview
- Preset templates for common configurations
- Import/export (Base64-encoded container strings)
- Duplicate, recover off-screen, macro visibility hooks

Verification: Create container from scratch, add items, configure data sources, export/import Base64.

### Phase 3I-1: Basic SSB TX
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

### Phase 3I-2: CW TX
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

### Phase 3I-3: TX Processing Chain + RX DSP Additions
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

### Phase 3I-4: PureSignal PA Linearization
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
cntrl1 override `(rx_adc_ctrl1 & 0xf3) | 0x08`). PureSignal (Phase 3I-4) is complete
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

### Phase 3N: Cross-Platform Packaging
**Goal:** Release builds for Linux, Windows, macOS.

CI workflows already in place. Finalize:
- AppImage (Linux x86_64 + ARM)
- Windows NSIS installer + portable ZIP
- macOS DMG (Apple Silicon)

---

## Recommended Next Step: Phase 3G-2 — MeterWidget GPU Renderer

Phases 3A–3E and 3G-1 are complete — the radio connects, demodulates audio, renders
live GPU spectrum + waterfall, supports full VFO tuning with CTUN panadapter mode,
and the container infrastructure (dock/float/resize/persist) is in place with a
QSplitter-based right panel (Container #0).

Next: build the MeterWidget QRhi renderer (3G-2) so containers can display live GPU-
rendered meter items. Then core meter groups (3G-3), interactive items (3G-5), and the
AetherSDR-style AppletPanel (3G-AP) for the default right-panel experience.

Execution order: **3G-2..6 → 3I-1..4 → 3F → 3H → 3J+**

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
