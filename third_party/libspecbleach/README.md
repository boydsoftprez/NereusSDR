# libspecbleach (NereusSDR third-party vendor)

This directory vendors the libspecbleach library for NereusSDR's
NR4 (SBNR) noise-reduction filter, which is ported from Thetis's
`wdsp/sbnr.c` (see `third_party/wdsp/src/sbnr.{c,h}`).

## Upstream

- **Source:** https://github.com/lucianodato/libspecbleach
- **License:** LGPL-2.1 — see `LICENSE` (copied verbatim from upstream,
  preserved byte-for-byte).
- **Pinned commit:** `d4cb7afff2c954b7938bd1e9bc77d81e788d08f0` (2026-04-23)
  — matches the revision vendored by Thetis v2.10.3.13 @ 501e3f51.

## Vendoring strategy

We use CMake `FetchContent` to pull the pinned upstream source at
configure time, rather than checking in the source tree. This keeps the
NereusSDR repo lean while preserving reproducibility: the pin is fixed
and CI caches the fetch after first run.

This mirrors the rnnoise (Task 5) approach — see `third_party/rnnoise/`
for the equivalent BSD-3-clause library used by NR3.

## LGPL-2.1 static-linking compliance

libspecbleach is licensed under LGPL-2.1. NereusSDR statically links it
as part of the wdsp_static → NereusSDR build. LGPL-2.1 Section 6 permits
static linking with a GPL application on the condition that users can
relink against a modified libspecbleach.

**Relink pathway:** modify `third_party/libspecbleach/CMakeLists.txt` to
point the `GIT_TAG` at a fork or local path, then rebuild NereusSDR.
The `specbleach_static` target links transitively into `NereusSDR` — no
prebuilt library replacement is required.

See `docs/attribution/LGPL-COMPLIANCE.md` for the full compliance notice
included with each binary release artifact.

## See also

- `docs/attribution/LIBSPECBLEACH-PROVENANCE.md` — provenance record for this vendor
- `docs/attribution/LGPL-COMPLIANCE.md` — static-link compliance notice
- `docs/attribution/THETIS-PROVENANCE.md` — NereusSDR's ports of Thetis source
- `third_party/wdsp/src/sbnr.{c,h}` — Thetis WDSP stage that calls libspecbleach
