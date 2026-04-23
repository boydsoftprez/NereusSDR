# rnnoise Provenance & License (NereusSDR)

Used by NR3 (RNNR) noise reduction via `third_party/wdsp/src/rnnr.{c,h}`.

## Upstream

- **Repo:** https://gitlab.xiph.org/xiph/rnnoise
- **Pinned commit:** `70f1d256acd4b34a572f999a05c87bf00b67730d` (2026-04-23)
- **License:** BSD 3-clause (see `third_party/rnnoise/LICENSE`)
- **Primary author:** Jean-Marc Valin (jmvalin@jmvalin.ca)
- **Additional copyright holders:**
  - Copyright (c) 2007-2017, 2024 Jean-Marc Valin
  - Copyright (c) 2023 Amazon
  - Copyright (c) 2017, Mozilla
  - Copyright (c) 2005-2017, Xiph.Org Foundation
  - Copyright (c) 2003-2004, Mark Borgerding

## Vendoring strategy

FetchContent-at-configure — source pulled from upstream at the pinned
commit; only the build scaffolding (`CMakeLists.txt`, `LICENSE`,
`README.md`, `models/Default_large.bin`) lives in the NereusSDR tree.
Upstream commit chosen to match Thetis v2.10.3.13's rnnoise revision.

The library is built with `-DUSE_WEIGHTS_FILE` to exclude the 58MB
baked-in model-weight tables (`rnnoise_data.c` / `rnnoise_data_little.c`).
The `models/Default_large.bin` file (3.5MB) is loaded at runtime via
`RNNRloadModel()` on radio connect, which mirrors Thetis's behavior of
loading the "Default (large)" model. See `third_party/rnnoise/README.md`
for full technical rationale.

## GPL compatibility

BSD 3-clause is compatible with GPLv2-or-later combined work. No
copyleft obligations apply; the in-tree `LICENSE` file satisfies all
attribution requirements for the BSD 3-clause terms.

## NereusSDR in-tree files

| File | Description |
|------|-------------|
| `third_party/rnnoise/CMakeLists.txt` | FetchContent build target (`rnnoise_static`) |
| `third_party/rnnoise/LICENSE` | BSD 3-clause license (verbatim from upstream COPYING) |
| `third_party/rnnoise/README.md` | Vendoring rationale and model loading notes |
| `third_party/rnnoise/models/Default_large.bin` | 3.5MB runtime model (from Thetis v2.10.3.13) |

## Change log

| Date | Author | Note |
|------|--------|------|
| 2026-04-23 | JJ Boyd (KG4VCF) / Claude Sonnet 4.6 | Initial vendor (Task 5, Phase 3G-RX Epic C-1) |
