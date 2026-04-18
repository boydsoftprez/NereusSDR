# NereusSDR Attribution

This directory holds the public-facing attribution record for NereusSDR:

- [`HOW-TO-PORT.md`](HOW-TO-PORT.md) — Authoritative porting protocol.
  Every file ported from Thetis, mi0bot/Thetis-HL2, or any other upstream
  GPL source must carry that source's verbatim top-of-file header plus a
  NereusSDR port-citation and Modification-History block. Verbatim
  preservation supersedes the earlier template model.
- [`THETIS-PROVENANCE.md`](THETIS-PROVENANCE.md) — File-by-file inventory
  mapping each NereusSDR derived source file to its Thetis upstream source,
  line ranges, derivation type, and contributor set.
- [`ASSETS.md`](ASSETS.md) — Inventory of every graphical/binary asset
  under `resources/` and `docs/images/` with origin, author, and license.

Per-file license headers live in the source files themselves; this
directory is the index.

NereusSDR as a whole is licensed under GPLv3 (see root `LICENSE`), which
is compatible with Thetis's GPLv2-or-later terms.
