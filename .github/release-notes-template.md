---

## Install NereusSDR

Choose the package for your computer from this release's **Assets**. Start
with the [user guide](https://nereussdr.com/manual/)
for desktop and mobile operation, or the [upgrade checklist](https://nereussdr.com/guides/upgrading-to-2026.10.0.html)
when updating an existing station. Update the Core and desktop together.

### Linux desktop

Download `NereusSDR-@VERSION@-x86_64.AppImage` or
`NereusSDR-@VERSION@-aarch64.AppImage` for your architecture:

```sh
chmod +x NereusSDR-@VERSION@-*.AppImage
./NereusSDR-@VERSION@-*.AppImage
```

The ARM AppImage uses the CPU spectrum renderer. Desktop packages include
the headless Core; the dedicated Core packages below install it as a service.

### Raspberry Pi OS Lite / Armbian Core

For a fresh **64-bit Debian 13/Trixie** board, download
`nereusd_@VERSION@_arm64_trixie.deb`. The
[SBC install guide](https://nereussdr.com/guides/install-core-sbc.html)
walks through preparing Raspberry Pi OS Lite or compatible Armbian,
verifying the download, installing with `apt`, starting the Core at boot
and pairing your desktop or native iPhone/iPad console.

The Ubuntu `.deb` packages target Ubuntu 24.04. Use the package matching
your operating system. The accompanying Trixie `.provenance.json` records
its source and package identity.

### macOS

- **Apple Silicon, macOS 14 or later:** `NereusSDR-@VERSION@-macOS-apple-silicon.dmg`.
- **Intel, macOS 12 or later:** `NereusSDR-@VERSION@-macOS-intel.dmg`.

Open the matching DMG and drag NereusSDR to Applications, or use the matching
`.pkg` installer. Release packaging uses Apple Developer ID signing and
notarization; the completed packaging run records the result.

### Windows

- **Installer:** `NereusSDR-@VERSION@-Windows-x64-setup.exe` installs the console
  with Start Menu and uninstall entries.
- **Portable:** extract `NereusSDR-@VERSION@-Windows-x64-portable.zip` and run
  `NereusSDR.exe`.

Windows binaries do not have an Authenticode signature. SmartScreen may prompt
on first launch; verify the download before choosing More info > Run anyway.

### Native iPhone and iPad app

The mobile console has its own TestFlight/App Store delivery process.
Desktop/Core assets do not install it. Follow
[mobile setup](https://nereussdr.com/manual/06-iphone-connect.html)
for connecting the app to a running Core; mobile availability is announced separately.

## Verify your download

Each asset and `SHA256SUMS.txt` has a detached GPG signature. Import the full
KG4VCF signing-key fingerprint, verify the checksum list, and check your file.
This example verifies the Trixie Core package; replace `core_package` with
the filename you downloaded when checking another asset.

```sh
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

NereusSDR is distributed under **GPLv3** (see `LICENSE` in the NereusSDR
packages and source archive, or <https://github.com/boydsoftprez/NereusSDR/blob/v@VERSION@/LICENSE>).
The combined work links/bundles:

- **Thetis**-derived C++ ports (GPL-2.0-or-later; combined under GPLv3
  via §5(b)) — see `licenses/thetis.txt`
- **mi0bot/Thetis-HL2**-derived HL2 bits (GPL-2.0-or-later) — see
  `licenses/mi0bot-thetis.txt`
- **AetherSDR**-derived Qt6 scaffolding (GPL-3.0) — see
  `licenses/aethersdr.txt`
- **Qt 6** (LGPL-3.0, dynamic linking) — see `licenses/qt6.txt`
- **FFTW3** (GPL-2.0-or-later, dynamic; bundled DLLs on Windows, dylibs on macOS,
  shared libraries in desktop AppImages; system dependency for daemon `.deb` packages)
  — see `licenses/fftw3.txt`
- **WDSP** (GPL-2.0-or-later, static) — see `licenses/wdsp.txt`

NereusSDR desktop and Core packages include a `licenses/` directory containing the
three full FSF licence texts (`GPLv2.txt`, `GPLv3.txt`, `LGPLv3.txt`),
all per-dependency notices listed above, and a written source offer at
`licenses/SOURCE-OFFER.txt`. Source downloads are provided on this release
page, with a three-year email fallback described in the offer.

The following source archives accompany the binaries on this release page;
the written source offer below applies independently:

- **NereusSDR** itself — `NereusSDR-@VERSION@-source.tar.gz` (this release).
  Equivalent to a `git archive` of the tag commit. This archive also
  contains the vendored WDSP sources (`third_party/wdsp/`) and the
  FFTW3 Windows binary-plus-header tree (`third_party/fftw3/`).
- **FFTW3** (GPLv2-or-later) — versioned upstream archives on this release:
  `fftw-3.3.5.tar.gz` (Windows DLLs), `fftw-3.3.8.tar.gz` (x86_64 AppImage
  upstream base), `fftw-3.3.10.tar.gz` (Intel macOS build and ARM AppImage
  upstream base), and `fftw-3.3.11.tar.gz` (Apple Silicon Homebrew upstream
  base). Ubuntu package sources, including their patches and build recipes,
  are in `fftw-ubuntu-source.tar.gz`: the `.dsc`, `.orig.tar.gz`, and
  `.debian.tar.xz` files for `3.3.8-2ubuntu8` and `3.3.10-1ubuntu3`.
  Upstream: <https://fftw.org/pub/fftw/>; Ubuntu:
  <https://archive.ubuntu.com/ubuntu/pool/main/f/fftw3/>.
  Homebrew's exact bottle recipe revision was not captured by the packaging
  logs; its upstream base is supplied here and the existing source offer
  applies. Provenance: `docs/attribution/FFTW3-PROVENANCE.md`.
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
`licenses/SOURCE-OFFER.txt`; contact <jj@skyrunner.net> to invoke it.

## Help and community

Explore the project at [nereussdr.com](https://nereussdr.com/), read the
[user guide](https://nereussdr.com/manual/),
or join [NereusSDR on Discord](https://discord.gg/m35ERjwRe) for setup questions,
operating discussion and feedback.

For a problem you want tracked, open a
[GitHub issue](https://github.com/boydsoftprez/NereusSDR/issues). Include the
NereusSDR version, OS, radio model and firmware, protocol, whether the Core is
local or remote, and the relevant application or Core log. The
[troubleshooting guide](https://nereussdr.com/manual/11-troubleshooting.html)
has the first checks to try.
