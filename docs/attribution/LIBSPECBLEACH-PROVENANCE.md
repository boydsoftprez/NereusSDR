# libspecbleach Provenance & License (NereusSDR)

Used by NR4 (SBNR) noise reduction via `third_party/wdsp/src/sbnr.{c,h}`.

## Upstream

- **Repo:** https://github.com/lucianodato/libspecbleach
- **Pinned commit:** `d4cb7afff2c954b7938bd1e9bc77d81e788d08f0` (2026-04-23)
- **License:** LGPL-2.1 (see `third_party/libspecbleach/LICENSE`)
- **Primary author:** Luciano Dato

## Vendoring strategy

FetchContent-at-configure — source pulled from upstream at the pinned
commit; only the build scaffolding (`CMakeLists.txt`, `LICENSE`,
`README.md`) lives in the NereusSDR tree. Upstream commit chosen to
match the revision bundled in Thetis v2.10.3.13 @ 501e3f51.

## LGPL-2.1 compliance

libspecbleach is licensed under LGPL-2.1. NereusSDR statically links it.
LGPL-2.1 Section 6 requires offering users the ability to relink against
a modified version. The relink pathway is documented in:

- `docs/attribution/LGPL-COMPLIANCE.md` — full compliance notice
- `third_party/libspecbleach/README.md` — rebuild instructions

## GPL compatibility

LGPL-2.1 is compatible with GPLv2-or-later combined work when the
relinking obligation described above is satisfied. See
`docs/attribution/LGPL-COMPLIANCE.md`.

## NereusSDR in-tree files

| File | Description |
|------|-------------|
| `third_party/libspecbleach/CMakeLists.txt` | FetchContent build target (`specbleach_static`) |
| `third_party/libspecbleach/LICENSE` | LGPL-2.1 license (verbatim from upstream) |
| `third_party/libspecbleach/README.md` | Vendoring rationale and LGPL compliance notes |

## Change log

| Date | Author | Note |
|------|--------|------|
| 2026-04-23 | JJ Boyd (KG4VCF) / Claude Sonnet 4.6 | Initial vendor (Task 6, Phase 3G-RX Epic C-1) |
