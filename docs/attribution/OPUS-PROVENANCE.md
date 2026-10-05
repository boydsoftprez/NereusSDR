# Opus Provenance - NereusSDR vendored-library inventory

This document catalogs the Opus audio codec as vendored into NereusSDR for
iPhone and iPad under `ios/NereusKit/Sources/COpus/`. The app uses it to
decode the Core's receive audio and to encode the microphone for transmit
(design spec `docs/architecture/2026-09-23-iphone-app-design.md` section
4.1). The desktop build fetches the same commit at build time through
`third_party/rade/cmake/BuildOpus.cmake`; this file covers the copy
committed under `ios/`.

Opus is BSD-3-Clause, compatible with NereusSDR's GPLv3 and with the App
Store permission in `ios/LICENSE`.

## Upstream

- **Project:** Opus (Xiph.Org Foundation)
- **Repository:** https://github.com/xiph/opus
- **Pinned commit:** `940d4e5af64351ca8ba8390df3f555484c567fbb` (the
  desktop's pin in `third_party/rade/cmake/BuildOpus.cmake`)
- **Archive:** https://github.com/xiph/opus/archive/940d4e5af64351ca8ba8390df3f555484c567fbb.zip
- **Archive SHA-256:** `20e37f9079ac2b80e3235cd8ce2547829e147bf44a2f5dd28a888fa3e9c24341`
- **Vendored:** 2026-09-24 (iPhone app plan, Task 6)

## License

BSD-3-Clause. The upstream `COPYING` is kept unchanged at
`ios/NereusKit/Sources/COpus/COPYING` and copied to
`packaging/third-party-licenses/opus.txt`. The app's licences screen shows
it (row in `ios/THIRD-PARTY.md`).

## Files vendored

`ios/scripts/vendor-sources.sh opus` writes the copy: it downloads the
archive, refuses it unless the SHA-256 matches, and copies these files
unchanged, keeping their upstream paths:

- the make lists `CELT_SOURCES` (`celt_sources.mk`), `SILK_SOURCES` and
  `SILK_SOURCES_FLOAT` (`silk_sources.mk`), `OPUS_SOURCES` and
  `OPUS_SOURCES_FLOAT` (`opus_sources.mk`);
- the header lists `CELT_HEAD`, `SILK_HEAD` and `OPUS_HEAD`;
- every header in `include/`;
- `COPYING`.

Left out: `dnn/` (deep PLC, DRED and OSCE, none of which the app turns on),
every processor-specific file (`x86/`, `arm/`, `mips/`), the fixed-point
SILK files (`fixed/`), and the demos, tests and build files. The script
also writes `VENDORED.txt` (pin, archive, hash, file count). `swift-test.sh`
runs `vendor-sources.sh opus --verify` before the tests, which extracts the
archive into a temporary directory and fails unless the committed copy
matches it byte for byte.

## Build wiring

`ios/NereusKit/Package.swift` builds the copy as the C target `COpus`:
the float build, with `OPUS_BUILD`, `USE_ALLOCA`, `HAVE_LRINTF`,
`HAVE_LRINT` and `PACKAGE_VERSION="940d4e5a"`, header search paths
`celt`, `silk` and `silk/float`, and compiler warnings silenced (`-w`) on
this target only, since the files stay unchanged. `COpusShim` adds
non-variadic wrappers over `opus_encoder_ctl` for Swift, and `NereusMedia`
holds `OpusDecoder` and `OpusEncoder`.

## Updating

1. Change the commit and archive SHA-256 in `ios/scripts/vendor-sources.sh`.
2. Run `ios/scripts/vendor-sources.sh opus` and paste the row it prints
   into `ios/THIRD-PARTY.md`.
3. Update this file, `PACKAGE_VERSION` in `Package.swift` and
   `packaging/third-party-licenses/opus.txt` if `COPYING` changed.
4. Run `ios/scripts/swift-test.sh` (the Opus conformance vectors included).

Do not edit the vendored files in place.
