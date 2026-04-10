# Changelog

## [Unreleased]

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
