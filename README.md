# NereusSDR

**Your station. Wherever you operate.**

NereusSDR is a free, open-source console for OpenHPSDR radios on macOS,
Windows and Linux, with a native iPhone and iPad app. Keep the radio and its
signal processing at your station, and operate from the console that suits
you: a local desktop, another computer, or your phone or tablet.

[Website](https://nereussdr.com/) · [Downloads](https://github.com/boydsoftprez/NereusSDR/releases) · [User guide](https://nereussdr.com/manual/) · [Discord](https://discord.gg/m35ERjwRe) · [Release notes](CHANGELOG.md)

[![CI](https://github.com/boydsoftprez/NereusSDR/actions/workflows/ci.yml/badge.svg)](https://github.com/boydsoftprez/NereusSDR/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Qt6](https://img.shields.io/badge/Qt-6-green.svg)](https://www.qt.io/)

## Start here

| What you want to do | Where to go |
| --- | --- |
| Install the desktop console and connect your radio | [Downloads](https://github.com/boydsoftprez/NereusSDR/releases) and [desktop setup](https://nereussdr.com/manual/01-desktop-connect.html) |
| Upgrade from 2026.10.0 | [2026.10.1 upgrade note](https://nereussdr.com/guides/upgrading-to-2026.10.1.html) and [release notes](CHANGELOG.md) |
| Upgrade from 0.5.2 | [2026.10.0 upgrade checklist](https://nereussdr.com/guides/upgrading-to-2026.10.0.html) and [release notes](CHANGELOG.md) |
| Run the Core on a Raspberry Pi or another SBC | [Fresh Raspberry Pi OS Lite / Armbian install](https://nereussdr.com/guides/install-core-sbc.html) |
| Connect an iPhone or iPad | [Mobile setup](https://nereussdr.com/manual/06-iphone-connect.html) and [mobile operation](https://nereussdr.com/manual/07-iphone-operate.html) |
| Arrange receivers, displays, applets and meters | [Receivers and panadapters](https://nereussdr.com/manual/04-slices.html) and [workspace and meters](https://nereussdr.com/manual/10-customize.html) |
| Set up transmit audio | [Transmit setup](https://nereussdr.com/manual/05-transmit.html) and [EQ/CFC editing](https://nereussdr.com/guides/tx-eq-cfc.html) |
| Get help or share your station | [Discord](https://discord.gg/m35ERjwRe), [troubleshooting](https://nereussdr.com/manual/11-troubleshooting.html) and [GitHub Issues](https://github.com/boydsoftprez/NereusSDR/issues) |

The [user guide](https://nereussdr.com/manual/) brings the operating instructions
together, from the first connection to shared stations, digital modes and
accessories. Join Discord for questions and discussion; use GitHub Issues
for bug reports and feature requests that need tracking.

## What's new in 2026.10.1

- **CAT and Hamlib rigctld.** Logging, contest and digital-mode programs
  control NereusSDR over four slice-bound CAT channels, on TCP, serial or a
  virtual serial port, with a separate rigctld port for Hamlib programs. A
  remote window can run its own rigctld, so WSJT-X on that computer tunes and
  keys the window, with its audio from NereusSDR TX.
- **Antenna rotor control.** Turn a Yaesu GS-232A/B or Hamlib `rotctld` rotor
  through the Core, from a Rotor applet, the iPhone or iPad app, or a spot.
- **Native audio engines.** Each system uses its own audio system, device lists
  update by themselves, and a Core speaker can be chosen from any window. The
  header has separate PC and RADIO volumes.
- **The workspace look is the default.** Show or hide container headers,
  arrange and dock on all four edges, pop containers out, and switch between
  workspace profiles.
- **Receivers come back as you left them.** After a relaunch every receiver
  returns with its own pan, squelch, APF, FM and AGC-T settings, and receivers
  B, C and D run on the values they show.
- **A Display pill on every pan.** Each panadapter has its own Display pill with
  Spectrum, Levels, Waterfall, View and FFT tabs, and Setup's Display pages set
  the defaults every pan starts from. A pan you adjust keeps its own value until
  you reset it. Waterfall levels are one choice: Clarity, Auto, Floor, Manual or
  Scale.
- **PureSignal Bench and two-tone measurements.** The PureSignal applet and
  Bench read the same correction and feedback instruments, AmpView has a
  cursor readout, and a two-tone test measures IMD3 and IMD5 on the display.
- **RADE waveform meters.** Add a microphone waveform and a decoded-speech
  waveform to any container to watch RADE audio, on the desktop or a remote
  desktop. Experimental RADE2-U and RADE2-L modes stay off until you turn them
  on.
- **Fixes.** Display and waterfall, two-tone and PureSignal, remote audio, and
  Core installs. Protocol 1 radios get their transmit packets from a thread of
  their own, aimed at Hermes Lite 2 PTT dropping through a Pi Core; that is
  not yet confirmed on a keyed radio. See the [release notes](CHANGELOG.md).

**Current version: 2026.10.1.** Packages are listed on the
[releases page](https://github.com/boydsoftprez/NereusSDR/releases). Read the
[2026.10.1 upgrade note](https://nereussdr.com/guides/upgrading-to-2026.10.1.html)
before updating a station, and update the Core and desktop together. Rotor
control, CAT Control on the phone and the Core speaker need a 2026.10.1 Core;
older Cores and apps still connect.

## What's new in 2026.10.0

This release brings together the work since 0.5.2: independent receivers,
a shared station Core, remote desktop and native mobile operation,
IPv6-aware connections, WDSP 2.10 with NNR and PureSignal 3, 3D display
history, TX EQ/CFC graph editors, and a workspace of movable applets and
configurable meters.

It is our first **calendar-versioned release**. `2026.10.0` is the first
release in October 2026; another release that month becomes `2026.10.1`.
The first release in a new month starts at `.0`. Releases ship when ready.

Coming from 0.5.2, read the
[2026.10.0 upgrade checklist](https://nereussdr.com/guides/upgrading-to-2026.10.0.html)
first.

## One station, several ways to operate

The **Core** connects to your radio and does the signal processing. It runs
receive and transmit DSP, noise reduction and PureSignal, prepares spectra
and meters, manages station audio and accessories, and coordinates receiver
control and transmit access.

The **console** is what you operate. It displays receivers, waterfalls,
meters and editors, plays received audio, and sends your tuning changes,
microphone audio and transmit requests to the Core.

On a local desktop, the Core and console run together. For a remote station,
the headless **`nereusd`** Core runs beside the radio while you use a Mac,
Windows or Linux console, or the native iPhone/iPad app elsewhere. Several
paired devices can share a station within its capacity. The Core keeps
receiver ownership and a single transmit holder clear across those devices.

### Keep the Core beside the radio

A suitable Linux single-board computer can run the Core without a monitor
or a local GUI. Use a Raspberry Pi inside an **ANAN-G2**, or a separate SBC
on the radio's network. Development testing included a **Raspberry Pi 4**
and a **Radxa Rock 5C with 2 GB RAM**. Receiver count, DSP choices and display
load determine how much a particular board can sustain.

For a fresh **64-bit Raspberry Pi OS Lite or Armbian Debian 13/Trixie**
installation:

1. Flash the image, set your login and hostname, enable SSH, and connect
   wired Ethernet to the radio's LAN.
2. Download the matching `nereusd_<version>_arm64_trixie.deb` and verify
   its signed checksum.
3. Install it with `sudo apt install ./nereusd_<version>_arm64_trixie.deb`.
   Copy `/usr/share/nereusd/nereusd.conf.sample` to `/etc/nereusd.conf`
   and review the configuration.
4. Run `sudo systemctl enable --now nereusd`, then `sudo nereusd status`.
5. Select the Core in your desktop or mobile console and pair with it.
   `sudo nereusd pairing show` displays a pairing code on the SBC.

The [SBC install guide](https://nereussdr.com/guides/install-core-sbc.html) has the complete
commands, signature checks and startup troubleshooting. An ANAN-G2's built-in
Pi also needs its Saturn-specific setup. The Ubuntu Core packages and the
Trixie package are separate builds; choose the one matching your OS.

#### Sound from the Core's own sound card

Three things found on the maintainer's Rock 5C bench Core on 2026-10-10:

- **Access comes from the `audio` group.** The service runs as an account
  with no groups of its own, and Linux sound cards belong to the `audio`
  group, so the Core can open one only when a small systemd drop-in,
  `nereusd.service.d/audio.conf`, adds that group. The `.deb` packages ship
  it, and the station images and the Pi 4 and Rock install scripts write it
  as well. If an older install lacks it, create
  `/etc/systemd/system/nereusd.service.d/audio.conf` containing the two lines
  `[Service]` and `SupplementaryGroups=audio`, then run
  `sudo systemctl daemon-reload` and `sudo systemctl restart nereusd`.
- **A card can be open and playing while its own mixer has the output
  switched off.** On the Rock 5C's on-board ES8316 codec (ALSA card
  `rockchip-es8316`) the `Left DAC Switch` and `Right DAC Switch` controls
  were off, so the headphone jack stayed silent. Turn them on with
  `amixer -c <card> cset name='Left DAC Switch' on` and the same for
  `Right DAC Switch`, then keep the setting with `alsactl store <card>`.
  NereusSDR does not change a card's mixer.
- **Real-time priority can be refused by the kernel.** The service is allowed
  to ask for it (`LimitRTPRIO=99`), but a kernel built with
  `CONFIG_RT_GROUP_SCHED=y` on cgroup v2 refuses real-time scheduling
  (`SCHED_FIFO`) to every service while the cgroup CPU controller is on. The
  Radxa vendor kernel `6.1.115-vendor-rk35xx` is one, and on Debian any login
  switches the controller on, because each login manager (`user@.service`)
  asks for it. The Core then logs "Raised thread priority was refused", and
  each audio thread refused when it started runs at normal priority. Signal
  processing never asks for real-time priority; it runs at nice -10 whether
  or not the controller is on. The Rock install script writes a drop-in,
  `/etc/systemd/system/user@.service.d/nereusd-realtime-priority.conf`, so
  login managers no longer ask for the controller; see
  `scripts/radxa/README.md`. To check a kernel:
  `zcat /proc/config.gz | grep RT_GROUP_SCHED`. To see whether the controller
  is on, look for `cpu` in `cat /sys/fs/cgroup/cgroup.subtree_control`.

A rotor or serial CAT on the Core also needs the `dialout` group, through a
`serial.conf` drop-in that the `.deb` packages do not ship. The
[SBC install guide](https://nereussdr.com/guides/install-core-sbc.html) has it.

#### The system setting the Core's installer changes

The `nereusd` packages and the Pi 4 scripts change one system-wide Linux
setting and no other: `net.core.rmem_max`, the largest receive buffer a
program may ask for on a network socket. The Rock script changes that setting
too, and also writes the login-manager drop-in described above.

- **What it is for.** The Core asks for a 4 MB receive buffer (4194304
  bytes) on the socket its radio sends to, so the system holds the radio's
  packets while the Core is briefly held up. Linux grants at most
  `net.core.rmem_max`. On the maintainer's Rock 5C that limit was 212992
  bytes as found, a twentieth of what the Core asks for: 6 to 9 ms of
  packets with three or four wide receivers open. A longer hold-up lost
  packets, and the receivers lost samples (measured 2026-10-10).
- **What is installed.** The `nereusd` `.deb` packages ship
  `/usr/lib/sysctl.d/10-nereusd.conf`, which sets the limit to 4194304 at
  every system start, and they apply it when the package is installed. The
  station images get it from the package, and the Pi 4 and Rock install
  scripts install and apply the same file. The installer prints what it
  changed. The setting is a ceiling, not a size any socket is given: a
  program that does not ask keeps the system's default size.
- **A limit is never lowered.** The install step leaves a limit already at
  or above 4194304 as it is. An install, reinstall or upgrade also leaves
  the limit as it is, whatever its value, when the Core's file is masked or
  replaced in `/etc/sysctl.d` or `/run/sysctl.d`, or when a file read after
  it sets this limit. At system start, a sysctl file whose name sorts after
  the Core's and sets this limit wins over it, whether its value is higher
  or lower; an administrator's own files (numbered 60 to 90 by systemd's
  convention) do. `/etc/sysctl.conf` is read at system start only where a
  file in `/etc/sysctl.d` links to it, as Debian's `99-sysctl.conf` did up
  to bookworm.
- **To undo it.** Remove the `nereusd` package, or delete the file; the
  system then starts with its own limit again. To keep the system's own
  limit with the package installed, link `/etc/sysctl.d/10-nereusd.conf` to
  `/dev/null`. To put the Rock's old limit back at once:
  `sudo sysctl -w net.core.rmem_max=212992`.
- **To check it.** When the Core connects to a radio its log says
  `radio socket receive buffer 4194304 bytes, all that was asked for`, or
  how many bytes it was given of the 4194304 asked for.

The desktop packages do not change this setting. A Core installed from
source with `cmake --install build --component nereusd` gets the file too.
It takes effect at the next system start, or at once with
`sudo /usr/local/libexec/nereusd/raise-receive-buffer-limit /usr/lib/sysctl.d/10-nereusd.conf`
(under the install prefix; `/usr/local` unless you chose another).

### Take the console with you

The native **iPhone and iPad app**, in the desktop's workspace look,
provides live spectrum and waterfall, touch tuning, receiver controls,
receive audio, microphone uplink and PTT, transmit readings, station Setup,
spots and accessory pages. Native EQ/CFC
controls, Filter Presets and named TX profiles bring voice shaping and everyday
receiver setup to the phone. It follows the same pairing, receiver ownership
and transmit-holder rules as the desktop.
The Core continues processing the radio while you operate from the phone.

The mobile app is delivered separately through its own TestFlight/App Store
process. Desktop/Core packages do not install it. See [mobile setup](https://nereussdr.com/manual/06-iphone-connect.html)
and [mobile operation](https://nereussdr.com/manual/07-iphone-operate.html) for the console workflow.

### Reach your station from another network

The **rendezvous (RV) service** introduces your console to your Core. The Core
registers its station identity; the console asks to connect to that station.
The service exchanges connection offers and network candidates, and provides
a pairing mailbox when the devices are on different networks. Your Core
still authenticates each device and decides what it may control.

Once connected, the session can travel directly between console and Core,
through a **TURN relay**, or through a **WebSocket relay** on networks that
only pass web traffic. The RV signalling service handles introductions;
relay services carry session traffic when needed. A reachable Core on your
LAN or VPN can also be selected directly by address.

**IPv4 and IPv6** are supported for Core/client discovery and connections.
IPv6 can provide a direct route when both ends and their firewalls allow it.
When a direct route is unavailable, the outbound relay paths provide alternatives.
This is useful on CGNAT and mobile broadband networks, including services such
as [T-Mobile Home Internet](https://www.t-mobile.com/support/home-internet/connect)
whose gateways do not offer configurable port forwarding. Core Settings shows
the station, selected connection path and audio status.

The Core uses your radio's existing OpenHPSDR connection on every route.
High-rate radio I/Q stays at the station.

```mermaid
flowchart LR
    Radio["OpenHPSDR radio"] <-->|"Radio I/Q and control"| Core["Station Core"]
    Core <-->|"Controls, audio, displays and meters"| Desktop["Desktop console"]
    Core <-->|"Controls, audio, displays and meters"| Mobile["Native iPhone/iPad"]
    Core <-->|"Registration and introduction"| RV["RV service"]
    Desktop <-->|"Introduction and pairing"| RV
    Mobile <-->|"Introduction and pairing"| RV
    Core <-->|"Optional session path"| Relay["TURN or WebSocket relay"]
    Relay <-->|"Optional session path"| Desktop
    Relay <-->|"Optional session path"| Mobile
```

See [shared Core operation](https://nereussdr.com/manual/08-shared-core.html) for operating a
station from several devices, and [RV installation](https://nereussdr.com/guides/remote-access.html)
if you want to host the remote-access service yourself.

## Make the console your own

- **Independent receivers and displays.** Tune, filter and listen to each
  receiver independently. Arrange several pans, float them across displays,
  and save your layouts. Coloured receiver markers, tunable notch filters,
  CTUN and per-band settings keep each receiver easy to follow. The native
  3D spectrum/waterfall adds depth and history; the transmit receiver has
  its own spectrum and waterfall settings.
- **Receive DSP.** WDSP 2.10 brings the processing engine forward. NNR offers
  Standard and Premium station-owned models, with per-radio and per-receiver
  tuning and a Models manager. AGC, noise reduction, squelch and other DSP
  controls remain available alongside Core-owned Diversity on eligible receivers.
- **Transmit audio.** Microphone profiles carry Graphic and Parametric EQ,
  5/10/18-band curves, CFC and the rest of the transmit processing chain.
  Native graph editors provide exact entry, frequency/width handles, shared
  CFC frequency selection and undo/redo. PureSignal 3 adds correction assets,
  status and AmpView. AM/SAM/DSB transmit and the AM modulation monitor add
  carrier, envelope and positive/negative modulation readings.
- **Workspace look.** Headers show or hide with the eye button, an Arrange
  mode lets you move containers, containers dock on all four edges, pop-out
  windows have a pin, and workspace profiles save whole layouts.
- **Applets, Canvas and meters.** Stack or float containers, or arrange
  applets, meters and fifteen individual controls on an editable Canvas.
  The TX applet's rows (monitor, power, filter, chain, VOX, processing and
  keying) and the RX applet's rows (antennas, mode, step, filter, pan,
  squelch, attenuator, AGC, RIT and XIT) can each go in a container on their
  own.
  Move, resize, layer and lock objects, preview changes, and Apply or Cancel.
  Responsive meter text, complete meter faces and the Nereus ANAN artwork
  bring readings together. Supported objects can follow the selected receiver
  or stay with a fixed receiver. Existing layouts and external MMIO bindings
  survive migration and portable layout exchange.
- **Native audio and CAT.** Each system plays and records through its own
  audio system (Core Audio, Windows audio and ASIO, PipeWire or PulseAudio,
  and ALSA direct on the headless Core) with device lists that update live.
  The header has separate PC and RADIO volumes. CAT runs on four slice-bound
  channels, with Hamlib rigctld for loggers.
- **Audio and digital applications.** Choose receiver mixes, station and
  radio speaker/headphone output, microphone sources and VAX audio buses.
  TCI provides WebSocket control/audio for external applications. Spots from
  DX Cluster, RBN, WSJT-X, DXLab, POTA, FreeDV Reporter and PSK Reporter can
  be filtered and selected to tune. RADE supports end-of-over callsigns when
  FreeDV Reporter is enabled.
- **Station accessories.** Operate PGXL, TGXL and RF-Kit RF2K-S through the
  Core, with device authority and tuner sequencing. Hardware controls follow
  the connected radio's capabilities, including attenuation, preamp,
  microphone inputs and ADC overload indication. Turn a GS-232A/B or Hamlib
  `rotctld` rotor through the Core, from the Rotor applet, the iPhone or a spot.

## Radios and downloads

NereusSDR supports radios using **OpenHPSDR Protocol 1 or Protocol 2**:

- Apache Labs ANAN-G2/Saturn, ANAN-7000DLE, ANAN-8000DLE, ANAN-200D,
  ANAN-100D, ANAN-100, ANAN-10E and other compatible ANAN models.
- Anvelina Pro 3.
- Hermes Lite 2.
- OpenHPSDR Metis, Hermes, Angelia, Orion and Orion MkII hardware.

Choose your package from [GitHub Releases](https://github.com/boydsoftprez/NereusSDR/releases):

| System | Package |
| --- | --- |
| macOS Apple Silicon, macOS 14 or later | DMG or PKG |
| macOS Intel, macOS 12 or later | DMG or PKG |
| Windows x64 | Installer or portable ZIP |
| Linux x86_64 or ARM64 | AppImage |
| Headless Linux Core | Ubuntu `.deb`, or Debian 13/Trixie ARM64 `.deb` for compatible SBCs |

Release downloads include detached GPG signatures and a signed checksum list.
Verify the list and the file you downloaded before installing:

```sh
(
set -eu
asset="YOUR_DOWNLOADED_FILENAME"
GNUPGHOME=$(mktemp -d)
export GNUPGHOME
trap 'rm -rf "$GNUPGHOME"' EXIT HUP INT TERM
curl -fL -o "$GNUPGHOME/signing-key.asc" https://nereussdr.com/nereussdr-signing-key.asc
gpg --batch --with-colons --show-keys "$GNUPGHOME/signing-key.asc" > "$GNUPGHOME/key-info"
fingerprint=$(awk -F: '$1 == "pub" { primary = 1; next } primary && $1 == "fpr" { print $10; primary = 0 }' "$GNUPGHOME/key-info")
[ "$fingerprint" = 4A95F4D22AEE9271D8A3C01B20C284473F97D2B3 ] || { echo "Signing key fingerprint mismatch." >&2; exit 1; }
gpg --batch --import "$GNUPGHOME/signing-key.asc"
gpg --batch --verify SHA256SUMS.txt.asc SHA256SUMS.txt
awk -v file="$asset" '
  $2 == file { row = $0; count++ }
  END {
    if (count != 1) { print "Expected exactly one asset checksum." > "/dev/stderr"; exit 1 }
    print row
  }
' SHA256SUMS.txt > "$GNUPGHOME/asset.sha256"
sha256sum --check --strict "$GNUPGHOME/asset.sha256"
)
```

Download `SHA256SUMS.txt` and `SHA256SUMS.txt.asc` from the same release as
your asset, and replace `YOUR_DOWNLOADED_FILENAME` with its exact filename.
Verification uses a temporary keyring. GPG's warning that the key is not
certified with a trusted signature is expected here; the full fingerprint
check identifies the release key. On macOS, use `shasum -a 256 -c` in place of
`sha256sum --check --strict`. For Raspberry Pi OS Lite or Armbian, follow the
[Core install guide](docs/guides/install-core-sbc.md) for the complete download
and installation commands.

macOS release packaging uses Developer ID signing and notarization. Windows
installers do not have an Authenticode signature and may prompt through SmartScreen.

## Known issues and what's next

PureSignal two-tone runs have an open transmit-stream continuity issue.
Receive restoration after unkeying still needs physical-radio validation.
These items remain under investigation.
The original reconnect reports in [#235](https://github.com/boydsoftprez/NereusSDR/issues/235),
[#299](https://github.com/boydsoftprez/NereusSDR/issues/299) and
[#300](https://github.com/boydsoftprez/NereusSDR/issues/300) still need reporter retests.

CAT/rigctld and rotor control arrived in 2026.10.1. CW transmit/keyer/QSK, FM pre-emphasis,
legacy skin import and WAV/IQ recording remain future work. Radio-specific and
on-air checks are recorded in the [feature verification documents](docs/architecture/).

## Building from Source

### Dependencies

```bash
# Ubuntu 24.04+ / Debian
sudo apt install qt6-base-dev qt6-base-private-dev \
  qt6-multimedia-dev qt6-shadertools-dev qt6-svg-dev qt6-websockets-dev \
  qt6-serialport-dev \
  cmake ninja-build pkg-config \
  libfftw3-dev libgl1-mesa-dev \
  libasound2-dev libjack-jackd2-dev \
  libpipewire-0.3-dev libpulse-dev libssl-dev

# Arch / CachyOS / Manjaro
sudo pacman -S qt6-base qt6-multimedia qt6-svg qt6-websockets qt6-serialport \
  cmake ninja pkgconf fftw \
  alsa-lib jack2 pipewire libpulse openssl

# macOS (Homebrew)
brew install qt@6 ninja cmake pkgconf fftw openssl@3
```

Each system plays and records through its own sound system: Core Audio on
macOS; Windows audio (shared or exclusive) and ASIO on Windows; PipeWire or
PulseAudio on a Linux desktop, and ALSA direct on the headless Linux Core.
PortAudio stays for the older drivers (MME, DirectSound, WDM-KS, JACK and ALSA).
On Linux the build needs `libpipewire-0.3-dev` (0.3.50 or later) for the
PipeWire engine, `libpulse-dev` for the PulseAudio engine and
`libasound2-dev` for the ALSA engine, and the bundled PortAudio is built with
`PA_USE_ALSA=ON` and `PA_USE_JACK=ON`, so the ALSA and JACK development
headers are required at compile time even if you don't use those older
drivers. Without `libpipewire-0.3-dev` the Linux VAX path falls back from the
native libpipewire-0.3 bridge to the older pactl route. On Windows, ASIO
comes from the ASIO SDK vendored in `third_party/asiosdk/`, so nothing extra
is installed to build it.

OpenSSL 3.0 or later is required for Core certificates. Install `libssl-dev`
on Debian/Ubuntu or `openssl@3` through Homebrew on macOS. Windows dependency
setup is described in [CONTRIBUTING.md](CONTRIBUTING.md).

### Windows (FFTW3 Setup)

No manual setup required. CMake auto-downloads [`fftw-3.3.5-dll64.zip`](https://fftw.org/pub/fftw/fftw-3.3.5-dll64.zip) on first configure and drops `fftw3.h` / `libfftw3-3.dll` / `libfftw3-3.def` into `third_party/fftw3/`. Requires network access on the first `cmake -B build` run; offline builds need to pre-populate those three files by hand.

### Build & Run

```bash
git clone https://github.com/boydsoftprez/NereusSDR.git
cd NereusSDR
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

Launch `./build/NereusSDR` on Linux, open `build/NereusSDR.app` on macOS,
or run `build/NereusSDR.exe` on Windows with this Ninja build.

The build produces **two** binaries. `NereusSDR` is the GUI. `nereusd` is the
headless Core. To install the Core and its service separately:

```
cmake --install build --component nereusd
```

A plain `cmake --install` deliberately installs neither `nereusd` nor its
systemd unit. Release packaging installs the daemon component separately;
the Trixie job also checks installation and CLI startup in a fresh runtime
container. Package verification can run from a branch with the release
workflow's `verify_only` dispatch. Always pass `--profile <name>` when running
`nereusd` by hand, or it writes the same settings the GUI reads.
For a board installation, use the [short SBC guide](https://nereussdr.com/guides/install-core-sbc.html).

On first run, NereusSDR generates FFTW wisdom (optimized FFT plans). This takes ~15 minutes and shows a progress dialog. The wisdom file is cached for subsequent launches.

See [docs/MASTER-PLAN.md](docs/MASTER-PLAN.md) for the full implementation plan and [docs/project-brief.md](docs/project-brief.md) for the project brief.

---

## Contributing

Contributions, bug reports and feature requests are welcome. Follow
[CONTRIBUTING.md](CONTRIBUTING.md) for build instructions, code conventions,
attribution and review requirements. Pull requests must pass CI and include
GPG-signed commits.

---

## Heritage

NereusSDR builds on these projects, with original station Core, remote-console
and native mobile work alongside its upstream foundations. Contributor notices
are preserved in source files and the [provenance record](docs/attribution/THETIS-PROVENANCE.md).

- **[Thetis](https://github.com/ramdor/Thetis)** - The canonical Apache Labs / OpenHPSDR SDR console (C# / WinForms). NereusSDR's feature source.
- **[AetherSDR](https://github.com/ten9876/AetherSDR)** - Native FlexRadio client (C++20 / Qt6). NereusSDR's architectural template.
- **[WDSP](https://github.com/TAPR/OpenHPSDR-wdsp)** - Warren Pratt NR0V's DSP library. The signal processing engine.
- **[OpenHPSDR](https://openhpsdr.org/)** - The open-source high-performance SDR project and protocol specifications.

---

## License

NereusSDR is free and open-source software licensed under the [GNU General Public License v3](LICENSE).

*NereusSDR is a derivative work of Thetis licensed under the GNU General Public License. It is not affiliated with or endorsed by Apache Labs, FlexRadio Systems, ramdor/Thetis, or the OpenHPSDR project.*
