---

## Installation

### Linux desktop

Download `NereusSDR-@VERSION@-x86_64.AppImage` or
`NereusSDR-@VERSION@-aarch64.AppImage` for your architecture:

```bash
chmod +x NereusSDR-@VERSION@-*.AppImage
./NereusSDR-@VERSION@-*.AppImage
```

The ARM AppImage uses the CPU spectrum fallback. The desktop packages include
the headless Core; the dedicated packages below install it as a service.

### Raspberry Pi OS Lite / Armbian Core

For a fresh ARM64 Debian 13/Trixie system, use
`nereusd_@VERSION@_arm64_trixie.deb`. Follow the
[short SBC install guide](https://github.com/boydsoftprez/NereusSDR/blob/v@VERSION@/docs/guides/install-core-sbc.md)
to prepare the image, verify the package, configure the Core, start its service
and pair a desktop or native iPhone/iPad console. The matching
`nereusd_@VERSION@_arm64_trixie.provenance.json` records source and build inputs.
The Ubuntu `.deb` packages target Ubuntu 24.04 and are separate builds.

### macOS

- **Apple Silicon, macOS 14 or later:**
  `NereusSDR-@VERSION@-macOS-apple-silicon.dmg`
- **Intel, macOS 12 or later:**
  `NereusSDR-@VERSION@-macOS-intel.dmg`

Open the matching DMG and drag NereusSDR to Applications. Matching `.pkg`
installers are also attached. Developer ID signing and notarization follow the
release workflow's configured Apple credentials; the completed run records
the result. For an ad-hoc build, use right-click > Open on first launch.

### Windows

- **Installer:** `NereusSDR-@VERSION@-Windows-x64-setup.exe` installs to
  Program Files and adds a Start Menu shortcut and uninstaller.
- **Portable:** `NereusSDR-@VERSION@-Windows-x64-portable.zip`; extract it and
  run `NereusSDR.exe`.

Windows binaries do not have an Authenticode signature. SmartScreen may warn
on first launch; use More info > Run anyway after verifying the download.

### Native iPhone / iPad app

The native mobile console has its own version counter and delivery process.
Desktop/Core release assets do not install the iOS app; its testing and
TestFlight/App Store delivery status are tracked separately.

## Verification

Artifacts and `SHA256SUMS.txt` have detached GPG signatures. Import the full
KG4VCF signing-key fingerprint and verify the checksum list, then check the
file you downloaded. The following example verifies the Trixie Core package;
replace `core_package` with another asset filename as needed.

```bash
(
set -eu
core_package=nereusd_@VERSION@_arm64_trixie.deb
gpg --keyserver keyserver.ubuntu.com --recv-keys 4A95F4D22AEE9271D8A3C01B20C284473F97D2B3
gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
awk -v file="$core_package" '$2 == file { print }' SHA256SUMS.txt | sha256sum --check --strict
)
```

On macOS, use `shasum -a 256 -c` in place of `sha256sum --check --strict`.

## Source & Licence (GPLv3 §6 / GPLv2 §3 corresponding source)

NereusSDR is distributed under **GPLv3** (see `LICENSE` inside any
release artifact, or <https://github.com/boydsoftprez/NereusSDR/blob/v@VERSION@/LICENSE>).
The combined work links/bundles:

- **Thetis**-derived C++ ports (GPL-2.0-or-later; combined under GPLv3
  via §5(b)) — see `licenses/thetis.txt`
- **mi0bot/Thetis-HL2**-derived HL2 bits (GPL-2.0-or-later) — see
  `licenses/mi0bot-thetis.txt`
- **AetherSDR**-derived Qt6 scaffolding (GPL-3.0) — see
  `licenses/aethersdr.txt`
- **Qt 6** (LGPL-3.0, dynamic linking) — see `licenses/qt6.txt`
- **FFTW3** (GPL-2.0-or-later, static on Windows / system on Linux+macOS)
  — see `licenses/fftw3.txt`
- **WDSP** (GPL-2.0-or-later, static) — see `licenses/wdsp.txt`

Every release artifact ships a `licenses/` directory containing the
three full FSF licence texts (`GPLv2.txt`, `GPLv3.txt`, `LGPLv3.txt`),
all per-dependency notices listed above, and a written source offer at
`licenses/SOURCE-OFFER.txt` covering GPLv3 §6(a) (primary: this release
page) and §6(b) (fallback: 3-year offer via email).

Corresponding source for every binary on this release page is provided
on the same medium:

- **NereusSDR** itself — `NereusSDR-@VERSION@-source.tar.gz` (this release).
  Equivalent to a `git archive` of the tag commit. This archive also
  contains the vendored WDSP sources (`third_party/wdsp/`) and the
  FFTW3 Windows binary-plus-header tree (`third_party/fftw3/`).
- **FFTW3** (GPLv2-or-later) — `fftw-3.3.5.tar.gz` (this release).
  Exact upstream archive used to build the bundled `libfftw3f-3.dll`
  shipped in the Windows installer and portable ZIP. Upstream:
  <https://fftw.org/pub/fftw/fftw-3.3.5.tar.gz>. Provenance:
  `docs/attribution/FFTW3-PROVENANCE.md` in the source archive.
- **Qt 6** (LGPLv3) — dynamically linked on all platforms; replace the
  bundled Qt 6 libraries with your own modified build per
  `licenses/qt6.txt`. Linux: `libQt6*.so.6` in the AppImage's
  `usr/lib/`; macOS: `Qt*.framework` in `NereusSDR.app/Contents/Frameworks/`;
  Windows: `Qt6*.dll` in the install directory. Upstream source:
  <https://download.qt.io/archive/qt/>.
- **WDSP** (GPLv2-or-later) — statically aggregated; corresponding source
  is in NereusSDR's `third_party/wdsp/` (included in
  `NereusSDR-@VERSION@-source.tar.gz`). Provenance:
  `docs/attribution/WDSP-PROVENANCE.md` in the source archive.

A written 3-year source offer applies independently per
`licenses/SOURCE-OFFER.txt`; contact <kg4vcf@gmail.com> to invoke it.

## Reporting Issues

This is an alpha build for debuggers/testers. Please report issues at
<https://github.com/boydsoftprez/NereusSDR/issues> with: OS, radio model,
protocol version, and log file (`~/.config/NereusSDR/nereussdr.log`).
