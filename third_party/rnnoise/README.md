# rnnoise (NereusSDR third-party vendor)

This directory vendors the Xiph.Org rnnoise library for NereusSDR's
NR3 (RNNR) noise-reduction filter, which is ported from Thetis's
`wdsp/rnnr.c` (see `third_party/wdsp/src/rnnr.{c,h}`).

## Upstream

- **Source:** https://gitlab.xiph.org/xiph/rnnoise
- **License:** BSD 3-clause — see `LICENSE` (copied verbatim from upstream
  `COPYING`, preserved byte-for-byte).
- **Pinned commit:** `70f1d256acd4b34a572f999a05c87bf00b67730d` (2026-04-23)
  — matches the revision vendored by Thetis v2.10.3.13 @ 501e3f51.

## Vendoring strategy

We use CMake `FetchContent` to pull the pinned upstream source at
configure time, rather than checking in the ~108MB in-tree tree (mostly
pre-generated model-weight .c files). This keeps the NereusSDR repo
lean while preserving reproducibility: the pin is fixed and CI caches
the fetch after first run.

## Model files

`models/Default_large.bin` is the 3.5MB user-loadable model file
copied from Thetis v2.10.3.13 (`Project Files/lib/NR_Algorithms_x64/
rnnoise_weights_large.bin`). NereusSDR loads it at runtime via
`RNNRloadModel(<path>)` on radio connect — mirrors Thetis's
"Default (large)" UI item.

We do NOT build rnnoise with its baked-in model weights
(`rnnoise_data.c` / `rnnoise_data_little.c`), which would require
fetching a separate 58MB model tarball at configure time. Instead,
we compile with `-DUSE_WEIGHTS_FILE` which suppresses the baked-weight
fallback code path in `denoise.c` at the preprocessor level. The
`rnnoise_create(NULL)` path is never taken — every NereusSDR launch
calls `RNNRloadModel(<path-to-Default_large.bin>)` before NR3 can run.

Technical note: `denoise.c:285-308` contains:
```c
if (model != NULL) { /* use runtime model */ }
#ifndef USE_WEIGHTS_FILE
else { init_rnnoise(&st->model, rnnoise_arrays); }  // <- suppressed
#endif
```
`rnnoise_arrays` is defined only in `rnnoise_data_little.c` / `rnnoise_data.c`.
Defining `USE_WEIGHTS_FILE` at compile time makes that else branch dead
and allows excluding those files from the build without linker errors.

If you want an alternate model, set `Nr3ModelPath` in
`~/.config/NereusSDR/NereusSDR.settings` or use Setup -> DSP -> NR/ANF ->
NR3 sub-tab -> "Use Model..." button.

## BSD / GPL compliance

BSD 3-clause has no copyleft obligation; the in-tree `LICENSE` file
satisfies attribution requirements. No relinking offer required.
BSD 3-clause is compatible with GPLv2-or-later (NereusSDR's license).

## See also

- `docs/attribution/RNNOISE-PROVENANCE.md` — provenance record for this vendor
- `docs/attribution/THETIS-PROVENANCE.md` — NereusSDR's ports of Thetis source
- `third_party/wdsp/src/rnnr.{c,h}` — Thetis WDSP stage that calls rnnoise
