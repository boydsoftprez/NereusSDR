# Fresh Raspberry Pi or Armbian Core install

Install the headless `nereusd` Core on a fresh **64-bit Debian 13 (Trixie)**
system. Use **Raspberry Pi OS Lite (64-bit), Trixie** on a Pi, or a compatible
**Armbian Minimal/CLI Debian Trixie** image for your board. The release's
`arm64_trixie.deb` is the matching package; the Ubuntu ARM64 `.deb` is a
different build. Development used a Raspberry Pi 4 and a 2 GB Radxa Rock 5C.

These steps set up a standalone SBC on the radio's LAN. For an ANAN-G2's
built-in Pi, follow its Saturn-specific setup as well; the radio image has
components beyond the Core.

## 1. Prepare the card

Use [Raspberry Pi Imager](https://www.raspberrypi.com/software/) or the
[Armbian image for your board](https://www.armbian.com/download/). Choose the
64-bit Trixie image, set your own user/password and a hostname such as
`nereus-core`, and enable SSH. Boot with wired Ethernet on the same LAN as
the radio. Connect a keyboard or use:

```sh
ssh YOUR_USER@nereus-core.local
```

## 2. Check the OS and install download tools

The first two commands should report `arm64` and `VERSION_CODENAME=trixie`.

```sh
dpkg --print-architecture
grep '^VERSION_CODENAME=' /etc/os-release
sudo apt update
sudo apt install -y ca-certificates curl gnupg avahi-daemon nano
```

## 3. Download and install the Core

Use the matching Trixie package from the selected
[release's Assets](https://github.com/boydsoftprez/NereusSDR/releases).
The example below targets 2026.10.0 once its artifacts are published.
Verify the signature and package checksum before installing.

```sh
(
set -eu
release_version=2026.10.0
core_package="nereusd_${release_version}_arm64_trixie.deb"
release_url="https://github.com/boydsoftprez/NereusSDR/releases/download/v${release_version}"
curl -fLO "$release_url/$core_package"
curl -fLO "$release_url/SHA256SUMS.txt"
curl -fLO "$release_url/SHA256SUMS.txt.asc"
gpg --keyserver keyserver.ubuntu.com --recv-keys 4A95F4D22AEE9271D8A3C01B20C284473F97D2B3
gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
awk -v file="$core_package" '$2 == file { print }' SHA256SUMS.txt | sha256sum --check --strict
sudo apt install "./$core_package"
)
```

`apt` installs the declared runtime dependencies. If they cannot be resolved,
check that the image is ARM64 Debian Trixie; use its normal repositories and
a matching package. Installation leaves the radio service disabled until the
next step.

## 4. Configure and start it

On this fresh installation, copy the sample and review it. With one visible
LAN radio, leave `radio_mac` empty; with several, set the intended radio's
MAC. The sample enables the Core listener, local status page and RV/relay
access. `remote_transmit` controls whether paired clients may transmit.

```sh
sudo install -m 0644 /usr/share/nereusd/nereusd.conf.sample /etc/nereusd.conf
sudo nano /etc/nereusd.conf
sudo systemctl enable --now nereusd
sudo nereusd status
```

The service now starts automatically at boot. First start generates and caches
FFTW plans; give it time to become ready.

## 5. Pair a console

Open NereusSDR on the desktop or the native iPhone/iPad app and choose the
Core found on the LAN. Follow the pairing prompt. To display a pairing code
on the SBC:

```sh
sudo nereusd pairing show
```

The local status page is `http://nereus-core.local:47911` if you used that
hostname. It shows the pairing code while the Core is unclaimed. Core Settings
shows the connected station and its actual audio/network path.

For a startup problem, use `sudo systemctl status nereusd` and
`sudo journalctl -u nereusd -n 50 --no-pager`. Existing installs keep their
configuration and private state in `/var/lib/nereusd`; back that state up to
preserve the Core identity and paired devices. See the
[station package/image notes](../../packaging/station-image/README.md) for
build provenance and the separate image workflow.
