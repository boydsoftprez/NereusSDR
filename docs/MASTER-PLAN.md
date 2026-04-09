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
- I/Q data streaming confirmed: DDC0, 1444 bytes/packet, 238 samples at 48kHz
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
| Preserve full feature set of Thetis | 33 panels + 11 groups mapped to 16 widget types | Design done |
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

**All project brief objectives are covered.** No gaps identified.

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

### Phase 3B: WDSP Integration Layer
**Goal:** Process I/Q through WDSP, output demodulated audio.

Files to modify/create:
- `third_party/wdsp/` — integrate WDSP library (build from source or prebuilt)
- `src/core/WdspEngine.h/.cpp` — implement channel lifecycle, fexchange2() calls
- `src/core/RxChannel.h/.cpp` — **new** — per-receiver WDSP channel with Q_PROPERTY DSP params
- `src/core/TxChannel.h/.cpp` — **new** — TX WDSP channel
- `CMakeLists.txt` — WDSP linking, HAVE_WDSP define

Key design reference: `docs/architecture/wdsp-integration.md`

Verification: Feed I/Q from radio into WDSP, hear demodulated audio through speakers.

### Phase 3C: Audio Pipeline
**Goal:** RX audio through speakers, TX audio from microphone.

Files to modify/create:
- `src/core/AudioEngine.h/.cpp` — implement QAudioSink/Source, WDSP audio routing
- RX: I/Q → WdspEngine → decoded audio → AudioEngine → speakers
- TX: mic → AudioEngine → WdspEngine → modulated I/Q → RadioConnection → radio

Verification: Tune to a known signal on the ANAN-G2, hear audio.

### Phase 3D: GPU Spectrum & Waterfall
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

### Phase 3E: VFO & Controls
**Goal:** Tuning, mode selection, filter, AGC controls.

Files to modify/create:
- `src/gui/widgets/VfoDisplay.h/.cpp` — **new** — frequency readout, band buttons, RIT/XIT
- `src/gui/widgets/RxControls.h/.cpp` — **new** — mode, filter, AGC, AF/RF, squelch
- `src/models/SliceModel.h/.cpp` — frequency, mode, filter, AGC properties
- `src/gui/MainWindow.cpp` — VFO-to-model wiring, keyboard tuning

Key design reference: `docs/architecture/multi-panadapter.md` (slice-to-pan association)

Verification: Tune to different frequencies/modes via VFO display. Mode/filter changes reflect in WDSP demodulation.

### Phase 3F: Multi-Panadapter Layout
**Goal:** Support 1-4 panadapters in configurable layouts with proper DDC-to-ADC mapping.

Files to modify/create:
- `src/gui/PanadapterStack.h/.cpp` — **new** — QSplitter layout manager
- `src/gui/PanadapterApplet.h/.cpp` — **new** — single pan container
- `src/gui/MainWindow.cpp` — wirePanadapter(), layout menu, slice-to-pan routing
- `src/core/ReceiverManager.cpp` — port UpdateDDCs() DDC assignment strategy from Thetis console.cs:8186
- `src/core/FFTRouter.h/.cpp` — **new** — map receiver FFT output to panadapter(s)

Key design references:
- `docs/architecture/multi-panadapter.md`
- `docs/architecture/adc-ddc-panadapter-mapping.md`

Verification: 4 pans in 2x2 grid, each with independent FFT/waterfall. RX1 on DDC2, RX2 on DDC3 (2-ADC boards).

### Phase 3G: Container System & DSP Control UI
**Goal:** Unified container system with all Thetis DSP controls as widget types.

Files to create:
- `src/gui/ContainerWidget.h/.cpp` — **new** — base container (dock/float/axis-lock/resize)
- `src/gui/FloatingContainer.h/.cpp` — **new** — separate window for floating containers
- `src/gui/ContainerManager.h/.cpp` — **new** — owns all containers, persistence, macro control
- Widget types (all **new**):
  - `src/gui/widgets/RxControls.h/.cpp` — mode, filter, AGC, AF/RF, squelch, NB/NR/ANF
  - `src/gui/widgets/TxControls.h/.cpp` — power, tune, MOX, ATU, TX profile
  - `src/gui/widgets/PowerMeter.h/.cpp` — fwd power, SWR, ALC gauges
  - `src/gui/widgets/SMeterWidget.h/.cpp` — signal meter (multiple modes)
  - `src/gui/widgets/EqControls.h/.cpp` — 10-band RX/TX graphic EQ
  - `src/gui/widgets/CwControls.h/.cpp` — speed, pitch, breakin, QSK, APF
  - `src/gui/widgets/FmControls.h/.cpp` — CTCSS, deviation, offset
  - `src/gui/widgets/PhoneControls.h/.cpp` — VOX, noise gate, mic, CPDR
  - `src/gui/widgets/DigitalControls.h/.cpp` — VAC, sample rate
  - `src/gui/widgets/VfoDisplay.h/.cpp` — frequency readout, band buttons, RIT/XIT
  - `src/gui/widgets/BandButtons.h/.cpp` — HF/VHF/GEN band grid
  - `src/gui/widgets/PureSignalStatus.h/.cpp` — calibration, feedback status
  - `src/gui/widgets/DiversityControls.h/.cpp` — sub-RX, ESC beamforming
  - `src/gui/widgets/ClockDisplay.h/.cpp` — UTC/local time
  - `src/gui/widgets/CustomMeter.h/.cpp` — configurable WDSP meter
- `src/gui/ContainerSettingsDialog.h/.cpp` — **new** — per-container config

Key design reference: Container System section in this plan

Verification: Create containers, add/remove widgets, float/dock, persist across restart.

### Phase 3H: Skin System
**Goal:** Thetis-inspired skin format with 4-pan support + legacy skin import.

Files to create:
- `src/gui/SkinParser.h/.cpp` — **new** — parse skin ZIP (JSON + XML + PNG)
- `src/gui/SkinRenderer.h/.cpp` — **new** — apply theme/images to widgets
- `src/gui/SkinManager.h/.cpp` — **new** — skin server integration, cache

Key design reference: `docs/architecture/skin-compatibility.md`

Verification: Load a Thetis skin, see colors/images applied, 4 pans still work.

### Phase 3I: TX Pipeline
**Goal:** Transmit audio from microphone through WDSP to radio.

Files to create:
- `src/core/TxChannel.h/.cpp` — **new** — TX WDSP channel wrapper
- `src/core/AudioEngine.h/.cpp` — add mic input via QAudioSource
- `src/core/P2RadioConnection.cpp` — send TX I/Q (port 1029) and audio (port 1028)

Key design reference: `docs/architecture/wdsp-integration.md`

Verification: Key radio into TX, transmit SSB/CW/AM signal.

### Phase 3J: TCI Protocol Server
**Goal:** TCI v2.0 WebSocket server for external app integration.

Files to create:
- `src/core/TciServer.h/.cpp` — **new** — WebSocket server
- `src/core/TciProtocol.h/.cpp` — **new** — TCI command parsing

### Phase 3K: Protocol 1 Support
**Goal:** Add P1 support for Hermes Lite 2 and older ANAN radios.

Files to create:
- `src/core/P1RadioConnection.h/.cpp` — **new** — UDP-only Metis framing
- `src/core/MetisFrameParser.h/.cpp` — **new** — 1032-byte frame parsing

### Phase 3L: Cross-Platform Packaging
**Goal:** Release builds for Linux, Windows, macOS.

CI workflows already in place. Finalize:
- AppImage (Linux x86_64 + ARM)
- Windows NSIS installer + portable ZIP
- macOS DMG (Apple Silicon)

---

## Recommended Next Step: Phase 3E — VFO & Controls

Phases 3A-3D are complete — the radio connects, demodulates audio, and renders
live GPU spectrum + waterfall. Next: add VFO tuning, mode selection, filter,
and AGC controls so the user can actually operate the radio.

### Key References
- Thetis: `console.cs` (VFO, band, mode, filter logic)
- Design docs: `docs/architecture/multi-panadapter.md` (slice-to-pan association),
  `docs/architecture/adc-ddc-panadapter-mapping.md` (DDC assignment for multi-RX)
- AetherSDR: SliceModel pattern for frequency/mode/filter state

### Implementation Steps
1. Implement SliceModel frequency/mode/filter/AGC properties
2. Create VfoDisplay widget with click-to-tune digit editing
3. Wire frequency changes through RadioConnection to hardware NCO
4. Add mode/filter/AGC controls with WDSP parameter routing
5. Implement band stacking registers for per-band memory

### Verification
- Tune to different frequencies by clicking VFO digits
- Change mode (USB/LSB/CW/AM) and hear correct demodulation
- Adjust filter bandwidth, see passband overlay update on spectrum

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

Thetis has 33 panels, 11 group boxes, and 9 spawned forms.
NereusSDR maps these into ~12 applets + dialogs:

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
