# Changelog

## [Unreleased]

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
