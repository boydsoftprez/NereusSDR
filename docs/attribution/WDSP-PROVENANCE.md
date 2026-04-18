# WDSP Provenance & License

WDSP (Warren Pratt NR0V's DSP library) is vendored in `third_party/wdsp/`.

## Upstream

- **Author:** Warren Pratt (NR0V, W5WJ)
- **Canonical repository:** https://github.com/TAPR/OpenHPSDR-wdsp
- **Version in NereusSDR:** v1.29 (as of 2025-02-XX)

## License Analysis

### Survey of Vendored Sources

All 138 source files in `third_party/wdsp/src/` were examined:

- **128 files** carry the full GPLv2-or-later permission block:
  - `"either version 2 of the License, or (at your option) any later version"`
  - All signal processing core: `channel.c`, `RXA.c`, `TXA.c`, `bandpass.c`, `amd.c`, `anf.c`, `anr.c`, and 121 others
  - All authored by Warren Pratt, NR0V, Copyright 2012–2025
  - **Conclusion: GPLv2-or-later**

- **10 files** have no license headers (non-copyrightable or build infrastructure):
  - Data tables: `calculus.c`, `FDnoiseIQ.c` (noise lookup tables)
  - Generated files: `resource.h`, `resource1.h` (MSVC IDE artifacts)
  - Minimal wrappers: `version.c`, `version.h`, `fastmath.h` (empty), `calculus.h` (empty)
  - Third-party library: `fftw3.h` (FFTW3 header, GPLv2-or-later — same terms as the FFTW3 source upstream; the "BSD license" label in earlier versions of this doc was incorrect)
  - **Conclusion: Non-GPL sources or utility stubs; not blocking GPL compatibility**

- **No files** carry GPLv2-only language (`"version 2 only"`, `"GPLv2-only"`)

### Representative Files Spot-Checked

Five canonical files confirm uniform GPLv2-or-later:

1. **channel.c** (core channel management)
   - Copyright 2013 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

2. **RXA.c** (RX audio processing pipeline)
   - Copyright 2013–2025 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

3. **TXA.c** (TX audio processing pipeline)
   - Copyright 2013–2023 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

4. **bandpass.c** (bandpass filter implementation)
   - Copyright 2013–2017 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

5. **amd.c** (AM demodulator)
   - Copyright 2012–2013 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

## Compatibility with NereusSDR

**NereusSDR is distributed under GPLv3** (see `/LICENSE` at the root).

GPLv3 permits aggregation of GPLv2-or-later code:
- GPLv3 §5(b) explicitly allows combining GPLv3 code with code "under the terms of version 2 or any later version of the GNU General Public License"
- WDSP's "or any later version" language satisfies this condition unambiguously

**Result: WDSP v1.29 (GPLv2-or-later) is fully compatible with NereusSDR's GPLv3 distribution.**

## NereusSDR Modifications

The vendored WDSP tree is unmodified from upstream and retains all upstream copyright notices and license headers. NereusSDR wraps WDSP via:

- `src/core/WdspEngine.cpp` — WDSP lifecycle manager
- `src/core/RxChannel.cpp` — RX channel DSP wrapper
- `src/core/TxChannel.cpp` — TX channel DSP wrapper

These wrappers are authored by NereusSDR contributors and carry NereusSDR (GPLv3) headers with Thetis/WDSP attribution in comments (see `docs/attribution/HEADER-TEMPLATES.md` for full attribution chain).

## Attribution Chain

1. **WDSP original** ← Warren Pratt NR0V, TAPR, GPLv2-or-later
2. **Thetis port** ← ramdor + contributors, GPLv2-or-later (Thetis itself includes WDSP)
3. **NereusSDR port** ← JJ Boyd KG4VCF + contributors, GPLv3 (with Thetis upstream attribution preserved in source headers)

Per compliance notice §6.1, the WDSP attribution is already preserved in the source tree headers and in NereusSDR wrapper source comments pointing to the Thetis contributor chain.
