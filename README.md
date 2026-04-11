# NereusSDR

**A cross-platform SDR console for OpenHPSDR radios**

[![CI](https://github.com/boydsoftprez/NereusSDR/actions/workflows/ci.yml/badge.svg)](https://github.com/boydsoftprez/NereusSDR/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Qt6](https://img.shields.io/badge/Qt-6-green.svg)](https://www.qt.io/)

NereusSDR is a ground-up port of [Thetis](https://github.com/ramdor/Thetis) (the Apache Labs / OpenHPSDR SDR console) from C# to C++20 and Qt6. It uses [AetherSDR](https://github.com/ten9876/AetherSDR) as the architectural template. The goal is a modern, cross-platform, GPU-accelerated SDR console that preserves the full feature set of Thetis while dramatically improving the UI, multi-panadapter experience, and waterfall fluidity.

---

## Supported Radios

Works with any radio implementing OpenHPSDR Protocol 1 or Protocol 2:

- **Apache Labs ANAN line** — ANAN-G2 (Saturn), ANAN-7000DLE, ANAN-8000DLE, ANAN-200D, ANAN-100D, ANAN-100, ANAN-10E
- **Hermes Lite 2**
- **All OpenHPSDR Protocol 1 radios** — Metis, Hermes, Angelia, Orion, Orion MkII
- **All OpenHPSDR Protocol 2 radios**

---

## Current Status

**Phase 3G-3 in progress — core meter groups.** NereusSDR connects to an ANAN-G2 (Orion MkII) via Protocol 2, receives raw I/Q data, demodulates audio through WDSP, renders a live GPU-accelerated spectrum + waterfall with VFO tuning (CTUN mode), and displays live GPU-rendered signal meters in dockable/floatable containers. Container #0 shows an arc-style S-meter (NeedleItem), Power/SWR bars, and ALC bar. A dedicated SMeterWidget (direct AetherSDR port) is next for pixel-identical S-meter rendering.

## Key Features

**Working now:**
- OpenHPSDR Protocol 2 radio discovery and connection
- Raw I/Q reception from ANAN-G2 (DDC2, 48kHz, 238 samples/packet)
- WDSP v1.29 DSP engine — USB/LSB/AM/CW demodulation, AGC, NB1/NB2, bandpass filtering
- Real-time audio output via QAudioSink (48kHz stereo Int16)
- FFTW wisdom caching with first-run progress dialog
- Audio device selection and persistence
- GPU-accelerated spectrum + waterfall (QRhi — Metal, Vulkan, D3D12, OpenGL fallback)
- Full-spectrum FFTW3 FFT (4096-point, Blackman-Harris window, 30 FPS, FFT-shift + mirror)
- VFO tuning, mode selection, filter controls (floating VFO flag widget)
- CTUN panadapter mode — independent pan center and VFO, WDSP shift offsets
- CTUN zoom — frequency scale bar drag or Ctrl+scroll zooms into FFT bin subsets with hybrid FFT replan
- Off-screen VFO indicator with double-click to recenter
- VFO marker, filter passband overlay, cursor frequency readout, filter drag
- Right-click display settings (color scheme, gain, black level, ref level, CTUN toggle)
- Mouse interaction (scroll-to-tune, drag ref level, click-to-tune, waterfall pan)
- Phase word NCO tuning with Alex HPF/LPF/BPF filters (fully enabled)
- Display settings persistence via AppSettings
- Dockable/floatable containers with axis-lock, hover-reveal title bar, serialization
- GPU-rendered meter engine (QRhi 3-pipeline: background texture, vertex geometry, QPainter overlay)
- Live signal strength bar meter in Container #0 (WDSP polling at 10 FPS)
- Composable MeterItems: BarItem, TextItem, ScaleItem, SolidColourItem, ImageItem
- Cross-platform build (Windows, Linux, macOS)

**Planned (see Roadmap):**
- Core meter presets: S-Meter needle, Power/SWR, ALC (Phase 3G-3)
- TX pipeline — SSB, CW, full processing chain, PureSignal (Phase 3I)
- Up to 4 independent panadapters in configurable layouts (Phase 3F)
- Thetis-inspired skin system (Phase 3H)
- TCI protocol server, DX Cluster/RBN spots (Phase 3J)
- CAT/rigctld for logging and contest software (Phase 3K)
- OpenHPSDR Protocol 1 — Hermes Lite 2, older ANAN radios (Phase 3L)

---

## Roadmap

### Phase 1 — Architectural Analysis ✅

| Deliverable | Status |
|---|---|
| 1A: AetherSDR architecture deep dive | Complete |
| 1B: Thetis architecture deep dive | Complete |
| 1C: WDSP API investigation (256 functions mapped) | Complete |

### Phase 2 — Architecture Design ✅

| Deliverable | Status |
|---|---|
| 2A: Radio abstraction (P1/P2, MetisFrameParser, ReceiverManager) | Complete |
| 2B: Multi-panadapter layout engine (5 layout modes) | Complete |
| 2C: GPU waterfall rendering (FFTW3, QRhi, shaders) | Complete |
| 2D: Skin compatibility (Thetis skin import + extended format) | Complete |
| 2E: WDSP integration (RxChannel/TxChannel, PureSignal, thread safety) | Complete |
| 2F: ADC-DDC-Panadapter mapping (signal chain, DDC assignment, bandwidth) | Complete |

### Phase 3 — Implementation

| Phase | Goal | Status |
|---|---|---|
| **3A: Radio Connection** | Connect to ANAN-G2 via Protocol 2, receive I/Q | **Complete** |
| **3B: WDSP Integration** | Process I/Q through WDSP, demodulate audio | **Complete** |
| **3C: macOS Build** | Cross-platform WDSP build + wisdom crash fix | **Complete** |
| **3D: Spectrum Display** | GPU spectrum + waterfall (QRhi Metal/Vulkan/D3D12) | **Complete** |
| **3E: VFO + Multi-RX Foundation** | VFO controls + rewire I/Q pipeline for N receivers + CTUN panadapter | **Complete** |
| **3G-1: Container Infrastructure** | **Dock/float/resize/persist container shells** | **Complete** |
| **3G-2: MeterWidget GPU Renderer** | **QRhi-based meter rendering engine** | **Complete** |
| **3G-3: Core Meter Groups** | **S-Meter, Power/SWR, ALC presets** | **In Progress** |
| 3G-4: Advanced Meter Items | History graph, magic eye, dial, LED | Planned |
| 3G-5: Interactive Meter Items | Band/mode/filter buttons, VFO display, clock | Planned |
| 3G-6: Container Settings Dialog | Full composability UI, import/export | Planned |
| 3I-1: Basic SSB TX | TxChannel, MOX state machine, RF output | Planned |
| 3I-2: CW TX | Sidetone, firmware keyer, QSK/break-in | Planned |
| 3I-3: TX Processing | 18-stage TXA chain + RX DSP additions | Planned |
| 3I-4: PureSignal | Feedback DDC, calcc/IQC engine, PA linearization | Planned |
| 3F: Multi-Panadapter | DDC assignment, FFTRouter, PanadapterStack, enable RX2 | Planned |
| 3H: Skin System | Thetis-inspired skins with 4-pan support | Planned |
| 3J: TCI + Spots | TCI v2.0 WebSocket, DX Cluster/RBN clients, spot overlay | Planned |
| 3K: CAT/rigctld | 4-channel rigctld, TCP CAT server | Planned |
| 3L: Protocol 1 | P1 support for Hermes Lite 2 / older ANAN | Planned |
| 3M: Recording | WAV record/playback, I/Q record, scheduled | Planned |
| 3N: Packaging | AppImage, Windows installer, macOS DMG | Planned |

See [docs/MASTER-PLAN.md](docs/MASTER-PLAN.md) for the full implementation plan.

---

## Building from Source

### Dependencies

```bash
# Ubuntu 24.04+ / Debian
sudo apt install qt6-base-dev qt6-multimedia-dev \
  cmake ninja-build pkg-config \
  libfftw3-dev libgl1-mesa-dev

# Arch / CachyOS / Manjaro
sudo pacman -S qt6-base qt6-multimedia cmake ninja pkgconf fftw

# macOS (Homebrew)
brew install qt@6 ninja cmake pkgconf fftw
```

### Windows (FFTW3 Setup)

Download the [FFTW3 64-bit DLLs](https://fftw.org/install/windows.html) and place `fftw3.h` in `third_party/fftw3/include/` and `libfftw3-3.dll` in `third_party/fftw3/bin/`.

### Build & Run

```bash
git clone https://github.com/boydsoftprez/NereusSDR.git
cd NereusSDR
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
./build/NereusSDR
```

On first run, NereusSDR generates FFTW wisdom (optimized FFT plans). This takes ~15 minutes and shows a progress dialog. The wisdom file is cached for subsequent launches.

See [docs/MASTER-PLAN.md](docs/MASTER-PLAN.md) for the full implementation plan and [docs/project-brief.md](docs/project-brief.md) for the project brief.

---

## Contributing

PRs, bug reports, and feature requests welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**Development environment:** NereusSDR is developed using [Claude Code](https://claude.com/claude-code) as the primary development tool. We encourage contributors to use Claude Code for consistency. PRs must follow project conventions, pass CI, and include GPG-signed commits.

---

## Heritage

NereusSDR stands on the shoulders of these projects:

- **[Thetis](https://github.com/ramdor/Thetis)** — The canonical Apache Labs / OpenHPSDR SDR console (C# / WinForms). NereusSDR's feature source.
- **[AetherSDR](https://github.com/ten9876/AetherSDR)** — Native FlexRadio client (C++20 / Qt6). NereusSDR's architectural template.
- **[WDSP](https://github.com/TAPR/OpenHPSDR-wdsp)** — Warren Pratt NR0V's DSP library. The signal processing engine.
- **[OpenHPSDR](https://openhpsdr.org/)** — The open-source high-performance SDR project and protocol specifications.

---

## License

NereusSDR is free and open-source software licensed under the [GNU General Public License v3](LICENSE).

*NereusSDR is an independent project and is not affiliated with or endorsed by Apache Labs or the OpenHPSDR project.*
