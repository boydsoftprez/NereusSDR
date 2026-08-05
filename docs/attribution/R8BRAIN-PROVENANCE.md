# r8brain-free-src Provenance - NereusSDR vendored-library inventory

This document catalogs the vendored r8brain-free-src polyphase resampler
library shipped under `third_party/r8brain/`. The library is consumed
exclusively through the C++ wrapper at `src/core/Resampler.{h,cpp}` (which
itself is a port of AetherSDR's `Resampler.{h,cpp}`; see
`docs/attribution/aethersdr-reconciliation.md` Phase 3R Task I2a).

NereusSDR is distributed under GPLv3 (root `LICENSE`). r8brain-free-src is
MIT, fully GPL-compatible.

## Upstream

- **Project:** r8brain-free-src
- **Repository:** https://github.com/avaneev/r8brain-free-src
- **Primary author:** Aleksey Vaneev (`avaneev`)
- **Pinned SHA:** `5c44bebe9c477d47b1dc7037fcaae2794ff2b4e1`
- **Pinned date:** `2026-03-12`
- **Vendored:** `2026-05-11` (Phase 3R Task I2a)
- **Languages:** C++ header-only (one anchor `r8bbase.cpp` added by
  NereusSDR; see "NereusSDR-added compilation anchor" below)

## License

r8brain-free-src is distributed under the **MIT License**. The verbatim
upstream `LICENSE` is preserved at `third_party/r8brain/LICENSE.txt`. MIT
is GPL-compatible by its own terms; no special obligations apply when
NereusSDR statically links r8brain into the consumer object library.

## Files vendored from r8brain-free-src

All files are copied verbatim from upstream at the pinned SHA. They live
under `third_party/r8brain/`:

| Upstream file | NereusSDR path |
|---|---|
| `LICENSE` | `third_party/r8brain/LICENSE.txt` |
| `CDSPBlockConvolver.h` | `third_party/r8brain/CDSPBlockConvolver.h` |
| `CDSPFIRFilter.h` | `third_party/r8brain/CDSPFIRFilter.h` |
| `CDSPFracInterpolator.h` | `third_party/r8brain/CDSPFracInterpolator.h` |
| `CDSPHBDownsampler.h` | `third_party/r8brain/CDSPHBDownsampler.h` |
| `CDSPHBDownsampler.inc` | `third_party/r8brain/CDSPHBDownsampler.inc` |
| `CDSPHBUpsampler.h` | `third_party/r8brain/CDSPHBUpsampler.h` |
| `CDSPHBUpsampler.inc` | `third_party/r8brain/CDSPHBUpsampler.inc` |
| `CDSPProcessor.h` | `third_party/r8brain/CDSPProcessor.h` |
| `CDSPRealFFT.h` | `third_party/r8brain/CDSPRealFFT.h` |
| `CDSPResampler.h` | `third_party/r8brain/CDSPResampler.h` |
| `CDSPSincFilterGen.h` | `third_party/r8brain/CDSPSincFilterGen.h` |
| `r8bbase.h` | `third_party/r8brain/r8bbase.h` |
| `r8bconf.h` | `third_party/r8brain/r8bconf.h` |
| `r8butil.h` | `third_party/r8brain/r8butil.h` |
| `fft/fft4g.h` | `third_party/r8brain/fft/fft4g.h` |

Files NOT copied (upstream has them; NereusSDR does not consume them):

- `example.cpp` (upstream example, not part of the API surface)
- `DLL/*` (DLL-export wrappers, not needed for a vendored object library)
- `bench/*` (benchmarking tools)
- `other/*` (filter-table generation helpers)
- `fft/pffft*` (alternative FFT backend; we use the bundled `fft4g.h`)
- `README.md`

## NereusSDR-added compilation anchor

r8brain-free-src is header-only since the FFT engine was inlined into
`CDSPRealFFT.h`. The function-local `static CSyncObject` singletons
inside the headers must instantiate, so NereusSDR adds one
translation-unit anchor:

- `third_party/r8brain/r8bbase.cpp` - a single `#include "CDSPResampler.h"`
  so the headers compile into one object file and the function-local
  statics anchor there. Carries an `SPDX-License-Identifier: MIT` tag and
  a one-paragraph comment explaining why it exists.

This file is **not** copied from upstream; it is a NereusSDR-native build
helper. Future r8brain syncs leave it untouched.

## Build wiring

`third_party/r8brain/CMakeLists.txt` exposes the library as an OBJECT
library named `r8brain` with `POSITION_INDEPENDENT_CODE ON` and the
vendored directory on its `PUBLIC` include path. The top-level
`CMakeLists.txt` adds the subdirectory near the existing
`add_subdirectory(third_party/rade)` line and links r8brain into
`NereusSDRObjs` with `target_link_libraries(NereusSDRObjs PUBLIC r8brain)`.

Compiler-warning suppression on the r8brain target only:
`-Wno-unused-parameter`, `-Wno-unused-variable`, `-Wno-sign-compare`,
`-Wno-shadow` (Clang/GCC). NereusSDR sources are not affected.

## Updating

To resync r8brain to a newer upstream SHA:

1. Clone upstream at the new SHA into a temp directory.
2. Overwrite the files listed in the table above verbatim with the
   upstream versions.
3. Update the `Pinned SHA` and `Pinned date` lines in this file and in
   `third_party/r8brain/VERSION.txt` and in
   `third_party/r8brain/CMakeLists.txt`'s first comment.
4. Leave `third_party/r8brain/r8bbase.cpp` alone (NereusSDR-added).
5. Rebuild and re-run `tst_resampler` to confirm no regression in the
   round-trip / latency contracts.

Do not edit the vendored headers in place. If a NereusSDR-specific
patch is required, raise it upstream first or wrap it in
`src/core/Resampler.{h,cpp}` instead.
