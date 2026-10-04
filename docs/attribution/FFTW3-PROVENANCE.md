# FFTW3 Provenance & License

FFTW3 (Matteo Frigo & Steven G. Johnson's FFT library) is vendored as
pre-built binaries in `third_party/fftw3/` for Windows targets only.
Desktop AppImages bundle distribution-provided FFTW shared libraries.
macOS release apps bundle dylibs: Intel builds FFTW from upstream source,
while Apple Silicon uses Homebrew. Daemon `.deb` packages use installed
system FFTW dependencies.

## Upstream

- **Authors:** Matteo Frigo, Steven G. Johnson (Massachusetts Institute of Technology)
- **Canonical repository:** https://github.com/FFTW/fftw3
- **Upstream homepage:** https://fftw.org/
- **Windows vendored version:** 3.3.5 (determined from DLL string `fftw-3.3.5 fftwf_wisdom` embedded in `libfftw3f-3.dll`)
- **Binary source:** https://fftw.org/install/windows.html (pre-built 64-bit DLLs)
- **Vendored file tree:**
  - `third_party/fftw3/include/fftw3.h` — public C header
  - `third_party/fftw3/lib/libfftw3f-3.a` — Windows static import library
  - `third_party/fftw3/lib/libfftw3f-3.def` — Windows DEF file
  - `third_party/fftw3/bin/libfftw3f-3.dll` — Windows runtime DLL
  - `third_party/fftw3/COPYING` — verbatim FFTW3 GPLv2 text (sha256: 231f7edcc7352d7734a96eef0b8030f77982678c516876fcb81e25b32d68564c)

## License Analysis

FFTW3 is **dual-licensed**:

1. **GPL-2.0-or-later** — the default free-software grant. NereusSDR uses this route.
2. **MIT commercial licence** — available for purchase from MIT Technology Licensing Office.
   NereusSDR does **not** hold an MIT commercial licence.

Upstream reference: https://www.fftw.org/faq/section1.html#isfftwfree

The `COPYING` file at the root of the FFTW3 repository (and now at
`third_party/fftw3/COPYING` in NereusSDR) carries the GNU General Public
License version 2, with FFTW3's source files referring to "version 2
or any later version" — i.e. GPL-2.0-or-later.

## Compatibility with NereusSDR

NereusSDR is distributed under GPLv3 (`/LICENSE`). GPLv3 §5(b) permits
combining GPLv3 code with code under "version 2 or any later version of
the GNU General Public License". FFTW3's "or later" language satisfies
this clause unambiguously — identical argument to the WDSP compatibility
analysis in `WDSP-PROVENANCE.md`.

**Result: FFTW3 (GPL-2.0-or-later) is compatible with NereusSDR's GPLv3
combined-work distribution.**

## NereusSDR modifications

**None.** The vendored binaries are used verbatim from the upstream
Windows distribution. `fftw3.h` is unmodified. No NereusSDR code
statically patches FFTW3 symbols.

## Binary distribution

Windows installer and portable ZIP packages ship both `libfftw3-3.dll`
(double precision, fetched from the upstream 3.3.5 binary ZIP during build)
and `libfftw3f-3.dll` (single precision, vendored in the source tree).
Windows import libraries link to those runtime DLLs.

Release source mapping:

| Package | FFTW input | Accompanying source |
| --- | --- | --- |
| Windows installer / ZIP | Upstream DLLs 3.3.5 | `fftw-3.3.5.tar.gz` |
| Intel macOS app | Upstream source build 3.3.10, shared double/float with threads | `fftw-3.3.10.tar.gz`; configure steps in `.github/workflows/release.yml` |
| Apple Silicon macOS app | Homebrew FFTW 3.3.11 | `fftw-3.3.11.tar.gz` upstream base; existing written source offer |
| x86_64 desktop AppImage | Ubuntu `3.3.8-2ubuntu8` | `fftw-3.3.8.tar.gz` upstream base plus `fftw-ubuntu-source.tar.gz` |
| ARM desktop AppImage | Ubuntu `3.3.10-1ubuntu3` | `fftw-3.3.10.tar.gz` upstream base plus `fftw-ubuntu-source.tar.gz` |
| Daemon `.deb` | Distribution-installed runtime dependency | Distribution package source; no FFTW library bundled in the `.deb` |

The Ubuntu bundle retains each package's signed `.dsc`, its original source
archive, and `.debian.tar.xz` containing patches and build recipes. Downloads
are verified against the SHA256 values in the `.dsc`. Ubuntu's 3.3.10 orig
archive differs from the fftw.org archive, so both are retained verbatim.
Source: <https://archive.ubuntu.com/ubuntu/pool/main/f/fftw3/>.

The Apple Silicon packaging log records the 3.3.11 Homebrew bottle version,
but does not establish an immutable formula revision for that bottle.
The upstream archive is its source base, not a claim that the exact Homebrew
bottle recipe has been captured. Source requests remain covered by the
existing `licenses/SOURCE-OFFER.txt` offer.

All four upstream archives and the Ubuntu source bundle accompany the
release binaries and pass through the existing SHA256 checksum and detached
signature pipeline. Upstream archives: <https://fftw.org/pub/fftw/>.
Bundled notices are `licenses/fftw3.txt`, `licenses/fftw3-notices.txt`,
`licenses/GPLv2.txt`, and `licenses/SOURCE-OFFER.txt` (inside each platform's
license directory).

## Attribution Chain

1. **FFTW3 original** ← Matteo Frigo and Steven G. Johnson, MIT, GPL-2.0-or-later / MIT commercial dual licence
2. **NereusSDR distribution** ← JJ Boyd KG4VCF and contributors, GPLv3 (with FFTW3 binaries accompanied by their upstream licence via `third_party/fftw3/COPYING` and this provenance doc)
