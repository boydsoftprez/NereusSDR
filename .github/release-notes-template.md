---

## Installation

### Linux

**x86_64:** download `NereusSDR-vX.Y.Z-x86_64.AppImage`
**aarch64:** download `NereusSDR-vX.Y.Z-aarch64.AppImage`

```bash
chmod +x NereusSDR-vX.Y.Z-*.AppImage
./NereusSDR-vX.Y.Z-*.AppImage
```

### macOS

**Apple Silicon (M1/M2/M3/M4):** `NereusSDR-vX.Y.Z-macOS-apple-silicon.dmg`

Intel Macs are not currently built on CI (GitHub Actions has retired the
`macos-13` runner label). Intel Mac users should build from source for now.

This build is **ad-hoc signed** (no Apple Developer ID yet). On first launch:
1. Open the DMG, drag NereusSDR to Applications.
2. **Right-click** NereusSDR.app → **Open** → **Open** in the dialog.
3. After the first launch, double-clicking works normally.

### Windows

Two options — pick one:

- **Installer (recommended):** `NereusSDR-vX.Y.Z-Windows-x64-setup.exe` — installs
  to `Program Files`, adds Start Menu shortcut, registers an uninstaller.
- **Portable:** `NereusSDR-vX.Y.Z-Windows-x64-portable.zip` — extract and run
  `NereusSDR.exe`. No install footprint.

Both are unsigned at the moment. SmartScreen may flag the binary on first run
(no Authenticode signature yet); click **More info → Run anyway**.

## Verification

All artifacts are GPG-signed by `KG4VCF`. To verify:

```bash
gpg --keyserver keyserver.ubuntu.com --recv-keys KG4VCF
gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
sha256sum -c SHA256SUMS.txt
```

## Source & Licence (GPLv3 §6 / GPLv2 §3 corresponding source)

NereusSDR is distributed under **GPLv3** (see `LICENSE` inside any
release artifact, or <https://github.com/boydsoftprez/NereusSDR/blob/main/LICENSE>).
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

- **NereusSDR** itself — `NereusSDR-vX.Y.Z-source.tar.gz` (this release).
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
  `NereusSDR-vX.Y.Z-source.tar.gz`). Provenance:
  `docs/attribution/WDSP-PROVENANCE.md` in the source archive.

A written 3-year source offer applies independently per
`licenses/SOURCE-OFFER.txt`; contact <kg4vcf@gmail.com> to invoke it.

## Reporting Issues

This is an alpha build for debuggers/testers. Please report issues at
<https://github.com/boydsoftprez/NereusSDR/issues> with: OS, radio model,
protocol version, and log file (`~/.config/NereusSDR/nereussdr.log`).
