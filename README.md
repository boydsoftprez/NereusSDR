# NereusSDR

**A cross-platform SDR console for OpenHPSDR radios**

> [!IMPORTANT]
> **Release candidate: 2026.10.0, the first calendar-versioned release.**
> This brings together the work since 0.5.2: independent receivers, remote
> Core operation, WDSP 2.10 with NNR and PureSignal 3, 3D display history,
> TX EQ/CFC graph editors, and movable applets with configurable meters.
>
> **Alpha testers, start here:**
> [2026.10.0 tester guide](docs/debugging/v2026.10.0-alpha-tester-smoketest.md).
> Read its upgrade notes before connecting an existing station. Core and
> desktop should be updated together. Settings migrate automatically, but
> watchdog and TCI interval defaults change once, and PS3 requires compatible
> version-2 correction files.
>
> The latest published release remains **v0.5.2** until the new release
> artifacts are available. Hardware and on-air acceptance retain their
> recorded status in the feature verification documents.
>
> J.J. Boyd ~ KG4VCF

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

Pre-built binaries for Linux (AppImage, x86_64 + aarch64), macOS (DMG +
PKG, Apple Silicon + Intel), and Windows (NSIS installer + portable ZIP,
x64) are published as GitHub Releases:

**<https://github.com/boydsoftprez/NereusSDR/releases>**

All artifacts are GPG-signed (`KG4VCF`) via `SHA256SUMS.txt.asc`. To verify:

```bash
gpg --keyserver keyserver.ubuntu.com --recv-keys KG4VCF
gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
sha256sum -c SHA256SUMS.txt
```

> **Platform signing:** macOS release packaging includes Apple Developer ID
> signing and notarization. Windows installers currently have no Authenticode
> signature and may prompt through SmartScreen. Check the selected release's
> artifact list and signing results; a source or CI build does not establish
> that a release package was signed successfully.

---

## Current Status

**Preparing 2026.10.0.** The latest published release is v0.5.2 (2026-05-24).
This is a substantial alpha release spanning the station Core, receiver/DSP
architecture and operator console. See [CHANGELOG.md](CHANGELOG.md) for
release history and the [tester guide](docs/debugging/v2026.10.0-alpha-tester-smoketest.md)
for upgrade checks and current limits.

## Key Features

- **Independent receivers and displays.** Per-receiver tuning, mode, DSP and
  audio; radio-capacity-aware allocation; saved multi-pan layouts, floating
  pans and coloured receiver markers. The transmit receiver owns its TX
  spectrum/waterfall. Native 3D stacked spectrum/waterfall adds display history.
- **A shared station Core.** Run with the desktop or as headless `nereusd`
  beside the radio. Authenticated remote desktop sessions share receiver
  control, displays and audio. Pairing, saved/manual station targets, device
  authority, transfer confirmations, link recovery and explicit direct/relayed
  connection status are part of the station system. Core Settings presents
  connection choices and current Core/audio status.
- **WDSP 2.10.** AGC, noise filters, squelch and advanced DSP controls;
  NNR Standard/Premium station-owned model assets; PureSignal 3 correction
  assets, status and AmpView. Capability and authority determine which actions
  a local or remote device can perform. See the
  [operator notes](docs/architecture/wdsp210-operator-notes.md) and
  [verification status](docs/architecture/wdsp210-verification/README.md).
- **Receiver tools.** CTUN, tunable notch filters, calibrated signal readings,
  per-band DSP/display persistence and Core-owned Diversity on eligible receivers.
- **TX processing and graph editors.** SSB, AM/SAM/DSB and RADE transmit;
  microphone profiles, independent Graphic/Parametric EQ, 5/10/18-band
  Parametric/CFC curves, exact entry, width/Q editing and undo/redo. Leveler,
  ALC, CFC, CPDR, CESSB, phase rotator, DEXP/VOX and anti-VOX remain integrated.
  See the [EQ/CFC guide](docs/guides/tx-eq-cfc.md).
- **Containers and complete meter objects.** Move, reorder and float applets
  and meters, return them to remembered homes, preview draft settings and
  exchange portable layouts. Composite faces retain calibrated source bindings;
  missing readings are shown as unavailable. External MMIO data bindings and
  recovery data survive migration.
- **Station audio and connections.** Receive mixes, radio speaker/headphone
  output, capability-gated hardware microphone controls, VAX audio buses,
  remote media recovery, discovery and manual/unicast connection targets.
- **Digital-app integration and spots.** TCI v2.0 WebSocket control/audio;
  DX Cluster, RBN, WSJT-X, DXLab, POTA, FreeDV Reporter and PSK Reporter sources;
  spots with click-to-tune and a live FreeDV station view. RADE supports
  end-of-over callsigns when FreeDV Reporter is enabled.
- **Accessories and hardware controls.** PGXL, TGXL and RF-Kit RF2K-S;
  Core-authoritative accessory operation, tuner sequencing, step attenuator,
  preamp/Level Cal corrections, ADC overload indication and per-radio controls.
- **Cross-platform distribution.** Linux AppImage, macOS DMG/PKG and Windows
  installer/portable ZIP, with GPG-signed checksums. Headless Core install is
  separate from the console package; see the build/install instructions below.

## Roadmap and acceptance

CW transmit/keyer/QSK, FM pre-emphasis, CAT/rigctld, legacy skin import and
WAV/IQ recording remain future work. Disabled actions retain an explanation;
being represented by an applet or Setup page does not establish implementation.
The iPhone/iPad app has an independent release process and calendar counter.

Cross-radio TNF listening, NNR quality, PureSignal RF improvement, RADE on-air
interoperability, sustained Pi/Radxa operation and accessory bench matrices
retain their documented pending checks. Software tests do not close these
hardware matrices. The reconnect fixes in this release also need the original
reporters' retests for issues #235, #299 and #300.

[docs/MASTER-PLAN.md](docs/MASTER-PLAN.md) preserves the original phase plan and
release history. Its current release summary takes precedence over older
phase scheduling notes. Feature designs and verification documents are under
[docs/architecture/](docs/architecture/).

---

## Building from Source

### Dependencies

```bash
# Ubuntu 24.04+ / Debian
sudo apt install qt6-base-dev qt6-base-private-dev \
  qt6-multimedia-dev qt6-shadertools-dev qt6-svg-dev qt6-websockets-dev \
  cmake ninja-build pkg-config \
  libfftw3-dev libgl1-mesa-dev \
  libasound2-dev libjack-jackd2-dev \
  libpipewire-0.3-dev libssl-dev

# Arch / CachyOS / Manjaro
sudo pacman -S qt6-base qt6-multimedia qt6-svg qt6-websockets \
  cmake ninja pkgconf fftw \
  alsa-lib jack2 pipewire openssl

# macOS (Homebrew)
brew install qt@6 ninja cmake pkgconf fftw
```

The bundled PortAudio is built with `PA_USE_ALSA=ON` and `PA_USE_JACK=ON` on Linux,
so the ALSA and JACK development headers are required at compile time even if you
don't use those audio backends at runtime. `libpipewire-0.3-dev` (≥ 0.3.50) is
strongly recommended on PipeWire-default distributions (Ubuntu 24.04+, Fedora 39+,
Arch) — without it the Linux audio path falls back from the native libpipewire-0.3
bridge to the older pactl route.

`openssl` / `libssl-dev` (≥ 3.0) is hard-required (`find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)`) since Remote Daemon R2 Task 17: `src/core/security/CertificateStore` links libcrypto directly to mint nereusd's self-signed TLS certificate, because Qt6 has no certificate-*generation* API. macOS resolves this through Homebrew's `openssl@3` formula with no extra hints; it is not keg-only (verified via `brew info --json=v2 openssl@3` fix round 3), its files are ordinary live symlinks into `${HOMEBREW_PREFIX}/lib` and `.../include`, the same path already searched for every other Homebrew library. The macOS Intel release row (no arm64 keg to link against) and both Windows rows source OpenSSL differently; see the `find_package(OpenSSL)` block in `CMakeLists.txt` and `vcpkg.json` for the full per-platform acquisition story.

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

The build produces **two** binaries. `NereusSDR` is the GUI. `nereusd` is the
headless daemon added in remote-daemon R1: it links `NereusCore` only, no GUI
object code, and is guarded by `tst_core_has_no_gui_includes`. It is installed
separately so the GUI's release artifacts are unaffected:

```
cmake --install build --component nereusd
```

A plain `cmake --install` deliberately installs neither `nereusd` nor its
systemd unit. Note that `release.yml` is the only workflow that runs
`cmake --install` and it triggers only on `v*` tags, so **the daemon's install
path has no PR-CI coverage**. Always pass `--profile <name>` when running
`nereusd` by hand, or it writes the same settings the GUI reads.

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
