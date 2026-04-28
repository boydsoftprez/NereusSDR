# NereusSDR

**A cross-platform SDR console for OpenHPSDR radios**

> 📖 **Alpha testers — start here:** [docs/debugging/v0.2.3-alpha-tester-smoketest.md](docs/debugging/v0.2.3-alpha-tester-smoketest.md)
>
> Full v0.2.3 walkthrough of what to try, what "success" looks like on
> your OpenHPSDR radio, and — just as important — which UI controls are
> intentionally stubbed so you don't file bugs against them. Includes
> 19 numbered steps from launch → live SSB QSO → quit, plus
> v0.2.3-specific tests for the dBm scale strip (step 12), the DSP grid
> + 7-filter NR family (step 13), Alex antenna routing per-band
> (step 14), VAX audio routing (step 15), and the new Linux PipeWire
> audio bridge (step 16).
> Earlier-release walkthroughs remain at
> [docs/debugging/alpha-tester-hl2-smoke-test.md](docs/debugging/alpha-tester-hl2-smoke-test.md)
> for historical reference.
>
> — J.J. Boyd ~ KG4VCF

[![CI](https://github.com/boydsoftprez/NereusSDR/actions/workflows/ci.yml/badge.svg)](https://github.com/boydsoftprez/NereusSDR/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Qt6](https://img.shields.io/badge/Qt-6-green.svg)](https://www.qt.io/)

NereusSDR is a C++20/Qt6 port of [Thetis](https://github.com/ramdor/Thetis) — the canonical OpenHPSDR / Apache Labs SDR console, itself descended from FlexRadio PowerSDR — carrying its radio logic, DSP integration, and feature set forward to a native cross-platform codebase (macOS, Linux, Windows) with a Qt-based GUI. The Thetis contributor lineage (FlexRadio Systems, Doug Wigley W5WC, Richard Samphire MW0LGE, and the wider OpenHPSDR community) is preserved per-file in source headers and summarized in [docs/attribution/THETIS-PROVENANCE.md](docs/attribution/THETIS-PROVENANCE.md). Distributed under GPLv3 (root [LICENSE](LICENSE)), elected under the "or later" grant in upstream Thetis source-file headers (Thetis is GPLv2-or-later). A verbatim copy of GPLv2 ships at [docs/attribution/LICENSE-GPLv2](docs/attribution/LICENSE-GPLv2) for reference, since several WDSP and ChannelMaster source files explicitly reference v2.

![NereusSDR v0.1.6 — ANAN-G2 on 40m LSB](docs/images/nereussdr-v016-screenshot.jpg)

---

## Supported Radios

Works with any radio implementing OpenHPSDR Protocol 1 or Protocol 2:

- **Apache Labs ANAN line** — ANAN-G2 (Saturn), ANAN-7000DLE, ANAN-8000DLE, ANAN-200D, ANAN-100D, ANAN-100, ANAN-10E
- **Hermes Lite 2**
- **All OpenHPSDR Protocol 1 radios** — Metis, Hermes, Angelia, Orion, Orion MkII
- **All OpenHPSDR Protocol 2 radios**

---

## Releases & Installation

Pre-built binaries for Linux (AppImage, x86_64 + aarch64), macOS (DMG, Apple
Silicon + Intel), and Windows (NSIS installer + portable ZIP, x64) are
published as GitHub Releases:

**<https://github.com/boydsoftprez/NereusSDR/releases>**

All artifacts are GPG-signed (`KG4VCF`) via `SHA256SUMS.txt.asc`. To verify:

```bash
gpg --keyserver keyserver.ubuntu.com --recv-keys KG4VCF
gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
sha256sum -c SHA256SUMS.txt
```

> **Alpha builds:** until Apple Developer ID and Authenticode certificates
> are obtained, macOS users will need to right-click → Open the DMG on first
> launch, and Windows users will see a SmartScreen warning to click through.
> Linux is unaffected. See the per-release notes for details.

---

## Current Status

**Current release: v0.2.3** (2026-04-24). Builds on v0.2.2 with the **Phase 3G RX experience epic** (Sub-epic A — AetherSDR-style dBm strip with hover/wheel-zoom and arrow clicks; Sub-epic B — full Thetis NB family port with NbFamily wrapper, cycling NB button, per-slice-per-band persistence; Sub-epic C-1 — 7-filter NR stack: NR1/2/3/4 plus DFNR DeepFilterNet3 neural NR and macOS-native MNR Apple Accelerate MMSE-Wiener), the **Phase 3P-I-a/b Alex antenna integration** (`AlexController` pump driving full `Alex.cs:310-413` composition, RX-only antennas with SKU-driven labels, Alex-2 Filters sub-tab gating, RX-Bypass 3rd VFO flag button, P1 bank0 C3 bits 5-7 + P2 Alex0 bits 8-11 wire-locked), and a **Linux PipeWire-native audio bridge** (`PipeWireBus`, `PipeWireStream`, `PipeWireThreadLoop`, lock-free SPSC ring, full backend detection / auto-open dialog, AudioOutputPage with per-slice routing, Help → Diagnose audio backend). Earlier-shipped 3O VAX, 3P-A…H, and the v0.2.2 maintenance fixes still apply. Next implementation phase is **3M-1: Basic SSB TX**.

### What's working end-to-end today

- **Radio connection — every OpenHPSDR P1 and P2 board.** ANAN-G2 (Saturn / Protocol 2), ANAN-10/10E/100/100B/100D/200D, Hermes, Hermes Lite 2, Angelia, Orion, Metis, Red Pitaya — all discover, connect, stream I/Q, demodulate through WDSP, and persist per-radio settings keyed by MAC. `BoardCapabilities` registry drives per-board DDC count, ADC count, BPF, Alex filter, and sample-rate support; `HardwareProfile` engine overlays Thetis `clsHardwareSpecific.cs` behaviour for ambiguous discovery bytes (user-selectable radio-model override in ConnectionPanel).
- **Sample-rate wiring** — P1 48 / 96 / 192 kHz (+ 384 on Red Pitaya); P2 48 / 96 / 192 / 384 / 768 / 1536 kHz. Per-MAC persistence under `hardware/<mac>/radioInfo/`. Inline reconnect banner on RadioInfoTab when the selected rate differs from the active wire rate.
- **Full Display parity** — `Setup → Display` (Spectrum / Waterfall / Grid & Scales) is wired end-to-end to the renderer on both the QPainter fallback and the QRhi/Metal/Vulkan/D3D12 GPU path. 47-control verification matrix at `docs/architecture/phase3g8-verification/README.md`. Per-band grid state persists across all 14 bands (160m–6m + GEN + WWV + XVTR) via the first-class `Band` enum on `PanadapterModel`.
- **Clarity adaptive display** — Clarity Blue waterfall palette, `ClarityController` with cadence / EWMA / deadband, `NoiseFloorEstimator` percentile, Reset-to-Smooth-Defaults button, Re-tune button + Clarity status badge in the spectrum overlay panel, per-band Clarity memory. Zoom persistence across restarts.
- **Full RX DSP parity** — 10 WDSP feature slices wired end-to-end through SliceModel → RadioModel → RxChannel → WDSP: AGC advanced (threshold / hang / slope / attack / decay), EMNR (NR2), SNB, APF (SPCW), 3-variant squelch (SSB / AM / FM), mute / audio pan / binaural, NB2 advanced, RIT / XIT client offset, frequency lock, mode containers (FM OPT / DIG / RTTY). Per-slice-per-band persistence under `Slice<N>/Band<key>/*`.
- **VfoWidget rewrite** — 4-tab layout (Audio / DSP / Mode / X-RIT), 4×2 DSP toggle grid, AGC 5-button row, S-meter level bar with dBm readout and cyan→green gradient, mode containers, tooltip coverage test. AGC-T ↔ RF Gain bidirectional sync prevents audio-breaking gain runaway.
- **Auto AGC-T** — `NoiseFloorTracker` feeds the Auto-threshold timer with MOX guard and `agcCalOffset`; AUTO button toggles auto-mode; right-click the AGC-T slider opens Setup directly on the AGC/ALC page.
- **Step attenuator + ADC overload** — `StepAttenuatorController` with Classic + Adaptive auto-attenuation modes, hysteresis, per-MAC persistence. P1/P2 `adcOverflow` signal from frame parsers, OVL status badge in RxApplet, per-model preamp items from Thetis `SetComboPreampForHPSDR`.
- **Container / meter system** — GPU-rendered meter engine (QRhi 3-pipeline), 31 `MeterItem` types, 38+ ItemGroup presets (S-Meter, Power/SWR, ALC, ANANMM 7-needle, CrossNeedle, Magic Eye, History, SignalText, TX bar meters), full Thetis-parity Container Settings Dialog (3-column layout, per-item property editors), MMIO external-data subsystem (UDP / TCP-listen / TCP-client / Serial transports; JSON / XML / RAW formats).
- **VAX audio routing** — NereusSDR-native multi-channel audio bus. `IAudioBus` abstraction with 5 platform backends (CoreAudio HAL plugin on macOS, PulseAudio pipes / pactl on Linux, PortAudio on Windows). First-run VAX dialog auto-detects Windows virtual-cable families (VB-Audio / VAC / Voicemeeter / Dante / FlexRadio DAX); `MasterOutputWidget` in the menu bar; Setup → Audio sub-tabs (Devices / VAX / TCI / Advanced); per-slice VAX channel assignment on the VFO Flag, persisted under `Slice<N>/`.
- **App polish** — Help → About NereusSDR (version / Qt / WDSP / GPG fingerprint / heritage credits), 💡 AI-assisted issue reporter in the menu bar corner (structured prompts, submits to the `bug_report.yml` / `feature_request.yml` GitHub templates), radio-model override persistence, P1 full 17-bank C&C round-robin.
- **Packaging** — `release.yml` prepare → build×3 → sign-and-publish pipeline. GPG-signed alpha artifacts: Linux AppImage (x86_64 + aarch64), macOS Apple Silicon DMG, Windows portable ZIP + NSIS installer.

### Deferred / not yet implemented

- **TX pipeline** (Phase 3M-1 through 3M-4) — TxChannel, mic input, MOX state machine, 18-stage TXA chain, PureSignal feedback DDC.
- **Multi-panadapter** (Phase 3F) — DDC assignment, FFTRouter, PanadapterStack, RX2 enable.
- ~~**HL2 `IoBoardHl2`** (Phase 3L)~~ — completed via Phase 3P-E: I2C TLV queue + 12-step state machine + bandwidth-monitor two-pointer byte-rate compute + NereusSDR throttle-detection layer; `P1CodecHl2` now intercepts C&C frames to inject I2C TLV payloads.
- **Skin system** (Phase 3H), **TCI + Spots** (Phase 3J), **CAT/rigctld** (Phase 3K), **WAV/IQ recording** (Phase 3M).

---

## Key Features

**Working now:**
- OpenHPSDR Protocol 1 and Protocol 2 radio discovery, connection, and per-MAC persistence across the full ANAN / Hermes / Metis / Red Pitaya family
- Per-MAC hardware sample-rate selection (P1 up to 192 / 384 kHz; P2 up to 1536 kHz)
- WDSP v1.29 DSP engine — USB/LSB/AM/CW/DIGI/FM demodulation with full RX DSP parity (AGC advanced, EMNR, SNB, APF, 3-variant squelch, NB1/NB2 advanced, RIT/XIT, mute/pan/binaural, frequency lock, mode containers)
- Per-slice-per-band persistence of DSP state (`Slice<N>/Band<key>/*`)
- Auto AGC-T with noise-floor tracker + MOX guard
- Step attenuator (Classic + Adaptive auto-attenuation) and ADC-overload OVL badge
- Real-time audio output via QAudioSink (48kHz stereo Int16)
- FFTW wisdom caching with first-run progress dialog; audio device selection and persistence
- GPU-accelerated spectrum + waterfall (QRhi — Metal, Vulkan, D3D12, OpenGL fallback); 4096-point FFTW3 FFT, Blackman-Harris window, 30 FPS, FFT-shift + mirror
- Full `Setup → Display` wiring — 47 Spectrum / Waterfall / Grid controls live on both render paths; 7 colour schemes; per-band grid state across all 14 bands (160m–6m + GEN + WWV + XVTR)
- Clarity Blue waterfall palette + Clarity adaptive auto-tune, Re-tune button, per-band Clarity memory, zoom persistence
- VFO flag widget — 4-tab layout (Audio / DSP / Mode / X-RIT), 4×2 DSP grid, AGC 5-button row, integrated S-meter level bar with dBm readout
- CTUN panadapter — independent pan center and VFO, WDSP shift offsets, bin-subset zoom with hybrid FFT replan, off-screen VFO indicator with double-click recenter
- Filter passband overlay, cursor frequency readout, click-to-tune, scroll-to-tune, filter drag, waterfall pan
- Phase word NCO tuning with Alex HPF/LPF/BPF filters, P1 full 17-bank C&C round-robin
- Dockable / floatable containers with axis-lock, hover-reveal title bar, XML serialization
- GPU-rendered meter engine (QRhi 3-pipeline), 31 `MeterItem` types, 38+ ItemGroup presets, ANANMM 7-needle with exact Thetis calibration, CrossNeedle dual fwd/rev, Magic Eye, History, Edge mode
- Full Thetis-parity Container Settings Dialog — 3-column layout, per-item property editors (~155 fields), snapshot+revert, container-level Lock/Notes/Highlight/Minimises/Auto-height, Duplicate, Copy/Paste item settings
- MMIO (Multi-Meter I/O) external-data subsystem — UDP / TCP-listen / TCP-client / Serial transports, JSON / XML / RAW formats, endpoint manager, variable picker, 10 fps polled bindings
- VAX multi-channel audio bus — 5 platform backends, Windows virtual-cable auto-detect (VB-Audio / VAC / Voicemeeter / Dante / DAX), MasterOutputWidget, per-slice VAX channel routing
- Interactive button grids — band (14), mode, filter, antenna, tuning step, macro — with hover/click feedback
- Full UI skeleton — 12 applets, 9-menu bar, 47-page SetupDialog, SpectrumOverlayPanel with 5 flyout sub-panels, status bar
- Help → About dialog + 💡 AI-assisted issue reporter wired to the GitHub issue tracker
- GPG-signed cross-platform alpha builds (Linux AppImage ×2 archs, macOS DMG, Windows portable ZIP + NSIS installer)

**Planned (see Roadmap):**
- **Phase 3M-1 Basic SSB TX** — TxChannel, mic input, MOX state machine, I/Q output (next up)
- **Phase 3M-2 CW TX** — sidetone, firmware keyer, QSK/break-in
- **Phase 3M-3 TX Processing** — 18-stage TXA chain + TX-side RX DSP additions
- **Phase 3M-4 PureSignal** — feedback DDC, calcc/IQC engine, PA linearization
- **Phase 3F Multi-Panadapter** — DDC assignment (including PS states), FFTRouter, PanadapterStack, RX2 enable
- **Phase 3H Skin System** — Thetis-inspired skin format with 4-pan support and legacy-skin import
- **Phase 3J TCI + Spots** — TCI v2.0 WebSocket server, DX Cluster / RBN clients, spot overlay
- **Phase 3K CAT / rigctld** — 4-channel rigctld, TCP CAT server
- ~~**Phase 3L HL2 `IoBoardHl2`**~~ — **delivered via Phase 3P-E**: I2C-over-ep2 TLV queue + 12-step UpdateIOBoard state machine + full bandwidth-monitor port with throttle detection.
- **Phase 3M Recording** — WAV record/playback, I/Q record, scheduled

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
| **3G-1: Container Infrastructure** | Dock/float/resize/persist container shells | **Complete** |
| **3G-2: MeterWidget GPU Renderer** | QRhi-based meter rendering engine | **Complete** |
| **3G-3: Core Meter Groups** | S-Meter, Power/SWR, ALC presets | **Complete** |
| **3-UI: Full UI Skeleton** | 12 applets, 9-menu bar, SetupDialog, SpectrumOverlayPanel | **Complete** |
| **3G-4: Advanced Meter Items** | 12 item types + ANANMM/CrossNeedle presets + Edge mode | **Complete** |
| **3G-5: Interactive Meter Items** | 14 interactive items + mouse forwarding + ButtonBoxItem base | **Complete** |
| **3G-6: Container Settings Dialog** | 3-column Thetis layout + per-item editors + in-place editing + MMIO external-data subsystem + container-level parity | **Complete** |
| **3G-7: Polish** | MMIO clone-path fix + 5 subclass accessor gap fills + NeedleItemEditor QGroupBox grouping | **Complete** |
| **3G-8: RX1 Display Parity** | 47 Spectrum/Waterfall/Grid controls wired, `Band` enum + per-band grid, `BandButtonItem` 12→14, GPU path polish | **Complete** |
| **3G-9: Display Refactor** | 3G-9a audit + Thetis-first tooltips + slider/spinbox refactor; 3G-9b smooth defaults + Clarity Blue palette; 3G-9c Clarity adaptive auto-tune | **Complete** |
| **3G-10: RX DSP Parity + AetherSDR Flag Port** | AetherSDR VfoWidget visual port + 10 WDSP feature slices wired through WDSP with per-slice-per-band persistence | **Complete** |
| **3G-11: P1 Field Fixes** | P1 VFO frequency encoding (raw Hz, not NCO phase word); Red Pitaya / Hermes family C&C bank fixes | **Complete** |
| **3I: Radio Connector & Radio-Model Port** | Full P1 family (Atlas/Hermes/HermesII/Angelia/Orion/HL2), `BoardCapabilities` registry, ConnectionPanel, HardwarePage 9-tab capability-gated, per-MAC persistence, `RadioConnectionError` taxonomy | **Complete** |
| **3G-13: Step Attenuator & ADC Overload** | `StepAttenuatorController` (Classic + Adaptive), P1/P2 `adcOverflow` emission, OVL status badge, Setup→General→Options page, RxApplet ATT/S-ATT row, per-model preamp items | **Complete** |
| **3G-14: About + AI Issue Reporter** | Help → About dialog, 💡 menu-bar issue reporter with structured prompts submitting to `bug_report.yml` / `feature_request.yml` | **Complete** |
| **3N: Packaging** | Consolidated `release.yml`, `/release` skill, GPG-signed alpha builds: Linux AppImage ×2 archs, macOS Apple Silicon DMG, Windows portable ZIP + NSIS installer | **Complete** |
| **3O: VAX Audio Routing** | NereusSDR-native multi-channel audio bus — 5 platform backends + first-run virtual-cable auto-detect (VB-Audio / VAC / Voicemeeter / Dante / DAX) + `MasterOutputWidget` + Setup → Audio sub-tabs + per-slice VAX channel persistence | **Complete** |
| **3P: All-Board Radio-Control Parity** | 8 stacked sub-phases (A-H) delivering: HL2 BPF + S-ATT bug fixes, per-board P1/P2 codec subclasses, Alex-1/2 Filters live-LED sub-sub-tabs, OC Outputs matrix page, Calibration page (incl. freq-correction factor), Antenna Control per-band grid, HL2 I/O (closes Phase 3L), Accessories (Alex/Apollo/Penny), Diagnostics → Radio Status dashboard + 4 sibling sub-tabs, attribution enforcement pipeline. After merge: NereusSDR's **hardware / radio-plumbing / status-readout surfaces are userland-complete vs Thetis** — DSP-parameter / Transmit / CAT / Appearance / Keyboard Setup pages are still page shells with disabled controls pending later phases (see the [alpha-tester guide](docs/debugging/alpha-tester-hl2-smoke-test.md) for the honest wired-vs-stub breakdown). | **Complete** |
| 3M-1: Basic SSB TX | TxChannel, mic input, MOX state machine, I/Q output | **Next** |
| 3M-2: CW TX | Sidetone, firmware keyer, QSK/break-in | Planned |
| 3M-3: TX Processing | 18-stage TXA chain + TX-side RX DSP additions | Planned |
| 3M-4: PureSignal | Feedback DDC, calcc/IQC engine, PA linearization | Planned |
| 3F: Multi-Panadapter | DDC assignment (incl. PS states), FFTRouter, PanadapterStack, enable RX2 | Planned |
| 3H: Skin System | Thetis-inspired skins with 4-pan support + legacy import | Planned |
| 3J: TCI + Spots | TCI v2.0 WebSocket, DX Cluster/RBN clients, spot overlay | Planned |
| 3K: CAT/rigctld | 4-channel rigctld, TCP CAT server | Planned |
| ~~3L: HL2 ChannelMaster.dll port~~ | **Delivered via Phase 3P-E** | ~~Planned~~ **Complete** |
| 3M: Recording | WAV record/playback, I/Q record, scheduled | Planned |

See [docs/MASTER-PLAN.md](docs/MASTER-PLAN.md) for the full implementation plan.

---

## Building from Source

### Dependencies

```bash
# Ubuntu 24.04+ / Debian
sudo apt install qt6-base-dev qt6-base-private-dev \
  qt6-multimedia-dev qt6-shadertools-dev qt6-svg-dev \
  cmake ninja-build pkg-config \
  libfftw3-dev libgl1-mesa-dev \
  libasound2-dev libjack-jackd2-dev \
  libpipewire-0.3-dev

# Arch / CachyOS / Manjaro
sudo pacman -S qt6-base qt6-multimedia qt6-svg \
  cmake ninja pkgconf fftw \
  alsa-lib jack2 pipewire

# macOS (Homebrew)
brew install qt@6 ninja cmake pkgconf fftw
```

The bundled PortAudio is built with `PA_USE_ALSA=ON` and `PA_USE_JACK=ON` on Linux,
so the ALSA and JACK development headers are required at compile time even if you
don't use those audio backends at runtime. `libpipewire-0.3-dev` (≥ 0.3.50) is
strongly recommended on PipeWire-default distributions (Ubuntu 24.04+, Fedora 39+,
Arch) — without it the Linux audio path falls back from the native libpipewire-0.3
bridge to the older pactl route.

### Windows (FFTW3 Setup)

No manual setup required. CMake auto-downloads [`fftw-3.3.5-dll64.zip`](https://fftw.org/pub/fftw/fftw-3.3.5-dll64.zip) on first configure and drops `fftw3.h` / `libfftw3-3.dll` / `libfftw3-3.def` into `third_party/fftw3/`. Requires network access on the first `cmake -B build` run; offline builds need to pre-populate those three files by hand.

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

*NereusSDR is a derivative work of Thetis licensed under the GNU General Public License. It is not affiliated with or endorsed by Apache Labs, FlexRadio Systems, ramdor/Thetis, or the OpenHPSDR project.*
