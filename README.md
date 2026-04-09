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

**Phase 3D — GPU spectrum + waterfall rendering.** NereusSDR connects to an ANAN-G2 (Orion MkII) via Protocol 2, receives raw I/Q data, demodulates audio through WDSP, and renders a live GPU-accelerated spectrum trace + waterfall via Metal (macOS), Vulkan (Linux), or D3D12 (Windows).

## Key Features

**Working now:**
- OpenHPSDR Protocol 2 radio discovery and connection
- Raw I/Q reception from ANAN-G2 (DDC2, 48kHz, 238 samples/packet)
- WDSP v1.29 DSP engine — USB demodulation, AGC, bandpass filtering
- Real-time audio output via QAudioSink (48kHz stereo Int16)
- FFTW wisdom caching with first-run progress dialog
- Audio device selection and persistence
- GPU-accelerated spectrum + waterfall (QRhi — Metal, Vulkan, D3D12, OpenGL fallback)
- FFTW3 client-side FFT (4096-point, Blackman-Harris window, 30 FPS)
- VFO marker, filter passband overlay, cursor frequency readout
- Right-click display settings (color scheme, gain, black level, ref level)
- Mouse interaction (scroll zoom, drag ref level, click-to-tune)
- Phase word NCO tuning with Alex band filters (80m BPF verified)
- Display settings persistence via AppSettings
- Cross-platform build (Windows, Linux, macOS)
- Up to 4 independent panadapters in configurable layouts
- Full WDSP DSP features (NR/NR2, NB/NB2, ANF, EQ, compression, PureSignal)
- VFO tuning, mode selection, filter controls
- Configurable floating/dockable containers — put any control anywhere, across multiple monitors
- Thetis-inspired skin system with 4-pan support
- OpenHPSDR Protocol 1 (Hermes Lite 2, older ANAN radios)
- TCI protocol server, CAT control (rigctld), DAX virtual audio
- TX support (mic → WDSP → radio)

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
| **3E: VFO & Controls** | Tuning, mode selection, filter, AGC controls | Next up |
| **3F: Multi-Panadapter** | 1-4 pans in configurable layouts | Planned |
| **3G: Container System** | Unified float/dock containers with 16 widget types | Planned |
| **3H: Skin System** | Thetis-inspired skins with 4-pan support | Planned |
| **3I: TX Pipeline** | Mic → WDSP → radio, TX audio + silence frames | Planned |
| **3J: TCI Server** | TCI v2.0 WebSocket for external apps | Planned |
| **3K: Protocol 1** | P1 support for Hermes Lite 2 / older ANAN | Planned |
| **3L: Packaging** | AppImage, Windows installer, macOS DMG | Planned |

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
