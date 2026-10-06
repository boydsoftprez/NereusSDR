# Fresh Raspberry Pi or Armbian Core install

Install the headless `nereusd` Core on a fresh **64-bit Debian 13 (Trixie)**
system. Use **Raspberry Pi OS Lite (64-bit), Trixie** on a Pi, or a compatible
**Armbian Minimal/CLI Debian Trixie** image for your board. The release's
`arm64_trixie.deb` is the matching package; the Ubuntu ARM64 `.deb` is a
different build. Development used a Raspberry Pi 4 and a 2 GB Radxa Rock 5C.

These steps set up a standalone SBC with a wired connection to the radio. For an ANAN-G2's
built-in Pi, follow its Saturn-specific setup as well; the radio image has
components beyond the Core.

## 1. Prepare the card

Use [Raspberry Pi Imager](https://www.raspberrypi.com/software/) or the
[Armbian image for your board](https://www.armbian.com/download/). Choose the
64-bit Trixie image, set your own user/password and a hostname such as
`nereus-core`, and enable SSH. Connect both the Pi and the radio by Ethernet
to the same switch, or plug the radio directly into the Pi's Ethernet port
and follow [Radio plugged directly into the Pi](#radio-plugged-directly-into-the-pi)
before continuing. For the direct cable setup, also configure Wi-Fi in
Raspberry Pi Imager or use a USB Ethernet adapter so your apps can reach the
Pi and it can download the package.

**Keep the radio-to-Core connection wired.** The radio should always connect
by Ethernet, either directly to the Pi or to the same switch as the wired Pi.
A Core that reaches the radio over Wi-Fi stutters. Connect a keyboard or use:

```sh
ssh YOUR_USER@nereus-core.local
```

## 2. Check the OS and install download tools

These commands stop if the system is not ARM64 Trixie.

```sh
(
set -eu
[ "$(dpkg --print-architecture)" = arm64 ] || { echo "Use a 64-bit ARM64 image." >&2; exit 1; }
. /etc/os-release
[ "${VERSION_CODENAME:-}" = trixie ] || { echo "Use a Debian Trixie image." >&2; exit 1; }
sudo apt update
sudo apt install -y ca-certificates curl gnupg avahi-daemon nano
)
```

## 3. Download and install the Core

Use the matching Trixie package from the selected
[release's Assets](https://github.com/boydsoftprez/NereusSDR/releases).
The example below installs 2026.10.0. Copy the whole command block into the
terminal. It downloads the public signing key over HTTPS, checks its full
fingerprint, then verifies the signed checksum list and the package before
installing. Verification uses a temporary keyring.

```sh
(
set -eu
release_version=2026.10.0
core_package="nereusd_${release_version}_arm64_trixie.deb"
release_url="https://github.com/boydsoftprez/NereusSDR/releases/download/v${release_version}"
GNUPGHOME=$(mktemp -d)
export GNUPGHOME
trap 'rm -rf "$GNUPGHOME"' EXIT HUP INT TERM
curl -fL -o "$GNUPGHOME/signing-key.asc" https://nereussdr.com/nereussdr-signing-key.asc
gpg --batch --with-colons --show-keys "$GNUPGHOME/signing-key.asc" > "$GNUPGHOME/key-info"
fingerprint=$(awk -F: '$1 == "pub" { primary = 1; next } primary && $1 == "fpr" { print $10; primary = 0 }' "$GNUPGHOME/key-info")
[ "$fingerprint" = 4A95F4D22AEE9271D8A3C01B20C284473F97D2B3 ] || { echo "Signing key fingerprint mismatch." >&2; exit 1; }
gpg --batch --import "$GNUPGHOME/signing-key.asc"
curl -fLO "$release_url/$core_package"
curl -fLO "$release_url/SHA256SUMS.txt"
curl -fLO "$release_url/SHA256SUMS.txt.asc"
gpg --batch --verify SHA256SUMS.txt.asc SHA256SUMS.txt
awk -v file="$core_package" '
  $2 == file { row = $0; count++ }
  END {
    if (count != 1) { print "Expected exactly one package checksum." > "/dev/stderr"; exit 1 }
    print row
  }
' SHA256SUMS.txt > "$GNUPGHOME/package.sha256"
sha256sum --check --strict "$GNUPGHOME/package.sha256"
sudo apt install "./$core_package"
)
```

GPG may report that the key is not certified with a trusted signature. That
warning is expected in a temporary keyring; the full fingerprint check above
identifies the release key. A bad signature, fingerprint mismatch or failed
checksum stops the commands before installation.

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
Core found on your home network. Follow the pairing prompt. To display a pairing code
on the SBC:

```sh
sudo nereusd pairing show
```

The local status page is `http://nereus-core.local:47911` if you used that
hostname. It shows the pairing code while the Core is unclaimed. Core Settings
shows the connected station and its actual audio/network path.

## Radio plugged directly into the Pi

If the radio's Ethernet cable goes straight into the Pi, with no switch or
router in between, run these commands once. They tell the Pi's Ethernet
port to use a link-local address when no router answers. It still uses a
router's address if you later plug it into your LAN.

These commands are for Raspberry Pi OS Trixie with NetworkManager **1.52 or
newer**. First check the version and find the name of the Pi's Ethernet
connection:

```sh
nmcli --version
nmcli -f NAME,TYPE,DEVICE connection show
```

Choose the Ethernet connection for `eth0`, usually `Wired connection 1`.
Use its full name, inside the quotes, in place of `CONNECTION` below. If
your Ethernet device has a different name, replace `eth0` as well. Apply
the setting from a local keyboard or an SSH connection over Wi-Fi or the
USB Ethernet adapter:

```sh
sudo nmcli connection modify "CONNECTION" ipv4.method auto ipv4.link-local fallback
sudo nmcli connection up "CONNECTION"
ip -4 address show dev eth0
```

On a direct cable, the last command should show an address starting with
`169.254`. Once that address appears, the Core normally finds a radio using
automatic addressing within a few seconds. If a router is connected instead,
expect the router's address rather than `169.254`.

Your apps still need a way to reach the Pi. Its built-in Ethernet port now
belongs to the radio, so connect the Pi to your home network over Wi-Fi or
with a USB Ethernet adapter. Only the Core's traffic to your apps goes over
that connection; it is much lighter than the radio's own data. The radio's
data stays on the direct Ethernet cable.

If the Core can see another radio on your home network and no radio has
already been selected, it waits for you to choose one. Pick the cabled radio
in the app under **This Core > Change radio**, or set its MAC address on the
`radio_mac` line in `/etc/nereusd.conf` before starting the Core. A saved
choice made in the app takes precedence over `radio_mac`.

### Optional: turn off Wi-Fi power saving

If the Pi reaches your apps over Wi-Fi, turning off Wi-Fi power saving may
smooth out small hiccups. Find the Wi-Fi connection's name:

```sh
nmcli -g GENERAL.CONNECTION device show wlan0
```

Use that name in place of `WIFI_CONNECTION`. The second command reconnects
Wi-Fi briefly; if you are using SSH over Wi-Fi, reconnect afterward.

```sh
sudo nmcli connection modify "WIFI_CONNECTION" 802-11-wireless.powersave 2
sudo nmcli connection up "WIFI_CONNECTION"
```

This changes the Pi-to-app connection. It does not make Wi-Fi suitable for
the radio-to-Core connection.

### What has been tested

The direct cable setup was tested with a **Raspberry Pi 5 running Raspberry
Pi OS Trixie (64-bit), NetworkManager 1.52.1, and an ANAN-7000DLE/8000DLE using
Protocol 2**. The radio took a `169.254` address on its own, and the Core held
the link with no errors after the change.

- **Hermes Lite 2 and other radios:** direct cable operation has not been
  tested. A radio configured with a fixed (static) address will not be found
  by this link-local setup; it needs a matching network configuration.
- **Armbian:** this direct cable procedure has not been tested there. Its
  image may manage networking differently; do not assume these
  NetworkManager commands apply.
- **Older NetworkManager versions:** `fallback` requires 1.52 or newer.
  On versions without it, `ipv4.method link-local` is an alternative for a
  dedicated cable, but stops the port from using a router's address. That
  alternative has not been tested here.

For the settings behind these commands, see NetworkManager's
[IPv4 reference](https://www.networkmanager.dev/docs/api/latest/settings-ipv4.html)
and [Wi-Fi power-saving reference](https://www.networkmanager.dev/docs/api/latest/settings-802-11-wireless.html).

## Troubleshooting and existing installs

**Audio plays for a few seconds, then the radio drops, over and over on a
direct cable:** see [Radio plugged directly into the Pi](#radio-plugged-directly-into-the-pi).

For a startup problem, use `sudo systemctl status nereusd` and
`sudo journalctl -u nereusd -n 50 --no-pager`. Existing installs keep their
configuration and private state in `/var/lib/nereusd`; back that state up to
preserve the Core identity and paired devices. See the
[station package/image notes](../../packaging/station-image/README.md) for
build provenance and the separate image workflow.
