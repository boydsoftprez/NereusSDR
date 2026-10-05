# plog Provenance - NereusSDR vendored-library inventory

This document catalogs plog as vendored into NereusSDR for iPhone and iPad
under `ios/NereusKit/Sources/CPlog/`. libdatachannel (`CDataChannel`) logs
through it; the app never turns its logging on. The desktop fetches the same
revision at build time through `cmake/NereusRemoteMedia.cmake`; this file
covers the copy committed under `ios/`.

plog is MIT, compatible with NereusSDR's GPLv3 and with the App Store
permission in `ios/LICENSE`. The files are unmodified.

## Upstream

- **Project:** plog (Sergey Podobry and contributors)
- **Repository:** https://github.com/SergiusTheBest/plog
- **Pinned commit:** `94899e0b926ac1b0f4750bfbd495167b4a6ae9ef` (the desktop's pin, the revision libdatachannel v0.24.5 records)
- **Archive:** https://codeload.github.com/SergiusTheBest/plog/tar.gz/94899e0b926ac1b0f4750bfbd495167b4a6ae9ef
- **Archive SHA-256:** `92a08bce559b5f28aa88d3fd9071567414b9f43a83fe2a05a6dd14f1da536072`
- **Vendored:** 2026-09-24 (iPhone app plan, Task 10)

## License

MIT. The upstream `LICENSE` is kept unchanged at
`ios/NereusKit/Sources/CPlog/LICENSE` and reproduced in
`packaging/third-party-licenses/plog.txt`. The app's licences screen shows
it (row in `ios/THIRD-PARTY.md`).

## Files vendored

plog is header-only. `ios/scripts/vendor-sources.sh plog` copies, unchanged,
every header under `include/plog/` and `LICENSE`. Left out: the samples,
tests, documentation and build files. The script also writes
`VENDORED.txt`; `swift-test.sh` checks the copy byte for byte against the
archive before the tests.

## Build wiring

`CPlog` is a directory, not a target: `CDataChannel` adds
`../CPlog/include` to its header search paths.

## Updating

1. Change the pin and archive SHA-256 in `ios/scripts/vendor-sources.sh`
   (keep it equal to the desktop's pin in `cmake/NereusRemoteMedia.cmake`, which the Core's media transport builds from).
2. Run `ios/scripts/vendor-sources.sh plog` and paste the row it prints
   into `ios/THIRD-PARTY.md`.
3. Update this file, the target's settings in `Package.swift` if the
   library's own build changed them, and
   `packaging/third-party-licenses/plog.txt` if the licence changed.
4. Run `ios/scripts/swift-test.sh` and `ios/scripts/interop-test.sh`.

Do not edit the vendored files in place.
