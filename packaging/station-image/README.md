# Nereus station card image

The manual `Station card image` GitHub workflow builds a Raspberry Pi OS Lite
64-bit image for a Raspberry Pi 4. Its artifact contains
`nereus-station-<version>-rpi-arm64.img.xz`, the image-specific Debian trixie
arm64 `nereusd` package, and a JSON provenance manifest with source revision,
pi-gen revision, builder image digest, package and image SHA-256 hashes.

## Build contract

The workflow uses [pi-gen's arm64 branch](https://github.com/RPi-Distro/pi-gen)
at commit `74d08a337bd29da289b9aedbe5b48c79fb2e5a03` and explicitly selects
Debian trixie. It builds `nereusd` from the same checked-out source in a native
arm64 `rust:1.94.1-trixie` container, then installs that package with `apt`
inside the trixie image rootfs. The release workflow's Ubuntu 24.04 `.deb` is
not an image input. C/C++ compilation is restricted to ARMv8-A, the Pi 4
Cortex-A72 baseline. DFNR is required: the workflow builds DeepFilterNet from
source commit `d375b2d8309e0935d165700c91da9de862a99c31`, with Rust
1.94.1, cargo-c `0.10.21+cargo-0.95.0`, and `-C target-cpu=generic` for
AArch64. Its source, model, generated lock file, build flags and output hashes
are recorded in the provenance manifest. The upstream commit's Cargo.lock is
stale for Cargo 1.94.1; `DeepFilterNet-Cargo.lock` is a checked-in refresh made
with that toolchain. No DeepFilterNet prebuilt library is accepted. The package
check requires the model at the daemon's runtime search path.

The workflow needs GitHub Actions `ubuntu-24.04-arm` capacity, Docker, working
Debian and Raspberry Pi package mirrors, crates.io, `sudo`, loop devices, and enough disk
space for the Rust build, pi-gen's rootfs copies and image export (tens of
gigabytes). It is
manual only and uploads an artifact; it does not publish a release or flash a
card. Run `python3 -m pytest tests/scripts/test_station_image_stage.py -q`
for the offline stage lint before dispatching it. A successful lint does not
prove that pi-gen builds or that hardware boots.

## Flash and first boot

Verify the JSON manifest and SHA-256 before flashing with Raspberry Pi Imager
or an equivalent imaging tool. Use a fresh card and a Pi 4 on the same LAN as
the radio. Wired Ethernet avoids requiring Wi-Fi credentials in the image.
The image contains no preset SSH login, Wi-Fi password, station key, pairing
token or device list. pi-gen's cloud-init example is skipped, SSH is disabled,
and the first user has no preset password. Configure any administrative login
and Wi-Fi through a trusted first-boot setup or per-card imaging process;
never put those values in the shared image artifact.

The image stage installs `avahi-daemon`, the sample `/etc/nereusd.conf`, and
enables the existing `nereusd.service` with `DynamicUser=yes`, plus a drop-in
that adds the `dialout` group so serial accessories can be opened. A one-time
service sets the hostname `nereus-station` before the daemon starts. Avahi
publishes `nereus-station.local`; the station's own Bonjour advertisement is
handled by `nereusd`. With `radio_mac` empty, the daemon uses the sole visible
LAN radio or waits for an operator choice when multiple radios are visible.
Its identity is created under the private `nereusd` state directory when the
daemon first starts. The status page is configured at
`http://nereus-station.local:47911`; use `nereusd pairing show` on the box for
the pairing code. The code is not printed to the journal or console.

Check `systemctl status nereusd` and `journalctl -u nereusd` for service state,
but do not expect the code there. Confirm the radio, status page, Bonjour
announcement and pairing from a real Pi 4 and phone before claiming device
acceptance. A card that has been booted contains its own generated identity;
do not duplicate a booted card as a reusable image.

## Armbian Rock 5C

The Armbian script is a build input, not a prebuilt Rock image. Use an Armbian
build targeting `BOARD=rock-5c`, `RELEASE=trixie`, arm64. Place
`armbian/customize-image.sh` at `userpatches/customize-image.sh`. Copy
`common/*` and the matching Debian trixie arm64 package into
`userpatches/overlay/nereus-station/`, naming the package `nereusd.deb`.
Armbian executes the script inside the target rootfs and bind-mounts the
overlay at `/tmp/overlay`; the script rejects any other board or release.
It uses the same package dependency checks, clean sample config, first-boot
hostname service and unit enablement as the Pi stage. The Armbian build
framework's own first-boot login/network choices must be configured by its
operator. The Pi workflow's package can be used only if Armbian's actual
trixie package repositories satisfy every declared dependency; otherwise
rebuild on that exact target rootfs. Rock 5C boot remains unverified here.

Neither image writes a radio MAC or other private address. Package installation
must resolve all dependencies normally; no `dpkg --force-*` or unmanaged
shared-library copy is used. The first-boot script contains no secret material.
