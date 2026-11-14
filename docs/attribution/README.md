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
- [`UPSTREAM-SYNC-PROTOCOL.md`](UPSTREAM-SYNC-PROTOCOL.md) — When and how
  to pull from Thetis / mi0bot / AetherSDR / WDSP upstreams, refresh
  inline-cite version stamps, and log the sync in `REMEDIATION-LOG.md`.
- [`ASSETS.md`](ASSETS.md) — Inventory of every graphical/binary asset
  under `resources/` and `docs/images/` with origin, author, and license.

Per-file license headers live in the source files themselves; this
directory is the index.

NereusSDR as a whole is licensed under GPLv3 (see root `LICENSE`), which
is compatible with Thetis's GPLv2-or-later terms.

## Per-component provenance

- [WDSP-PROVENANCE.md](WDSP-PROVENANCE.md) — WDSP v1.29 vendored at `third_party/wdsp/`
- [SGP4-PROVENANCE.md](SGP4-PROVENANCE.md) — Vallado's SGP4/SDP4 reference propagator (MIT via python-sgp4) vendored at `third_party/sgp4/`
- [FFTW3-PROVENANCE.md](FFTW3-PROVENANCE.md) — FFTW3 3.3.5 binaries vendored at `third_party/fftw3/` (Windows only)
- [THETIS-PROVENANCE.md](THETIS-PROVENANCE.md) — NereusSDR files derived from Thetis
- [GGMORSE-PROVENANCE.md](GGMORSE-PROVENANCE.md) — ggmorse (MIT, Georgi Gerganov) vendored byte-identical at `third_party/ggmorse/`, the CW decoder behind `src/core/CwDecoder`
- [MBEDTLS-PROVENANCE.md](MBEDTLS-PROVENANCE.md) — Mbed TLS 3.6.7 (Apache-2.0), nur ECDSA P-256 + SHA-256, byte-identical at `third_party/mbedtls/`, the Roundtable sign-in behind `src/core/roundtable/RoundtableKey`
- [NEREUS-ROUNDTABLE-PROVENANCE.md](NEREUS-ROUNDTABLE-PROVENANCE.md) — Roundtable aus dem Upstream NereusSDR: `roundtable/` (Server, Tests, Konformitaetssuite) unveraendert, `RoundtableWire` uebernommen
