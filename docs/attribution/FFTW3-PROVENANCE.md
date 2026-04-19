# FFTW3 Provenance & License

FFTW3 (Matteo Frigo & Steven G. Johnson's FFT library) is vendored as
pre-built binaries in `third_party/fftw3/` for Windows targets only.
Linux and macOS builds link against the system FFTW3 package provided
by the distribution / Homebrew.

## Upstream

- **Authors:** Matteo Frigo, Steven G. Johnson (Massachusetts Institute of Technology)
- **Canonical repository:** https://github.com/FFTW/fftw3
- **Upstream homepage:** https://fftw.org/
- **Version in NereusSDR:** 3.3.5 (determined from DLL string `fftw-3.3.5 fftwf_wisdom` embedded in `libfftw3f-3.dll`)
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

`libfftw3f-3.dll` ships inside:

- The NereusSDR Windows NSIS installer (Phase 3N `release.yml` artifact)
- The NereusSDR Windows portable ZIP (Phase 3N `release.yml` artifact)

GPLv2 §3 corresponding-source availability is satisfied by the public
upstream download page at https://fftw.org/install/windows.html and by
the upstream git repository at https://github.com/FFTW/fftw3. NereusSDR
release notes point recipients to these locations; the
`THIRD-PARTY-LICENSES.txt` bundled with each release artifact (Phase 4,
Task 12 of the 2026-04-18 audit plan) carries a verbatim copy of this
provenance notice.

## Attribution Chain

1. **FFTW3 original** ← Matteo Frigo and Steven G. Johnson, MIT, GPL-2.0-or-later / MIT commercial dual licence
2. **NereusSDR distribution** ← JJ Boyd KG4VCF and contributors, GPLv3 (with FFTW3 binaries accompanied by their upstream licence via `third_party/fftw3/COPYING` and this provenance doc)
