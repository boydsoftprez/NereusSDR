# Mbed TLS Provenance - NereusSDR vendored-library inventory

This document catalogs Mbed TLS as vendored into NereusSDR for iPhone and
iPad under `ios/NereusKit/Sources/CMbedTLS/`. It is the app's DTLS library:
libdatachannel (`CDataChannel`) is built with `USE_MBEDTLS=1`, and libsrtp
(`CSrtp`) uses it as its crypto engine. The desktop builds the same
libdatachannel with OpenSSL instead; the app does not ship OpenSSL. The two
interoperate over DTLS 1.2 (`MediaPeerInteropTests`).

Mbed TLS is dual licensed Apache-2.0 OR GPL-2.0-or-later. The app takes it
under Apache-2.0, which is compatible with NereusSDR's GPLv3 and with the
App Store permission in `ios/LICENSE`. The files are unmodified.

## Upstream

- **Project:** Mbed TLS (The Mbed TLS Contributors, Trusted Firmware)
- **Repository:** https://github.com/Mbed-TLS/mbedtls
- **Pinned release:** `v3.6.7`, the newest 3.6 LTS release tag on 2026-09-24
- **Archive:** https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2
  (the release archive, which carries the generated sources a tag's source
  archive lacks)
- **Archive SHA-256:** `a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6`
- **Vendored:** 2026-09-24 (iPhone app plan, Task 10)

## License

Apache-2.0 (taken from the dual Apache-2.0 OR GPL-2.0-or-later offer). The
upstream `LICENSE`, which gives both texts, is kept unchanged at
`ios/NereusKit/Sources/CMbedTLS/LICENSE`; the notice is
`packaging/third-party-licenses/mbedtls.txt` with the full Apache text in
`packaging/third-party-licenses/Apache-2.0.txt`. The release archive has no
`NOTICE` file. The app's licences screen shows it (row in
`ios/THIRD-PARTY.md`).

## Files vendored

`ios/scripts/vendor-sources.sh mbedtls` copies, unchanged: every `.c` and
`.h` in `library/` (the crypto, X.509 and TLS sources its
`library/CMakeLists.txt` builds, with their private headers), the public
headers in `include/mbedtls/` and `include/psa/`, and `LICENSE`. Left out:
`3rdparty/` (Everest and p256-m, off in the default configuration), the
programs, tests, documentation, `framework/` and build files. The script
also writes `VENDORED.txt`; `swift-test.sh` checks the copy byte for byte
against the archive before the tests.

## Build wiring

`Package.swift` builds the C target `CMbedTLS`, public headers `include`,
header search path `library`, with the default configuration
(`include/mbedtls/mbedtls_config.h`) plus one define,
`MBEDTLS_SSL_DTLS_SRTP`, which libdatachannel needs to key SRTP from the
DTLS handshake. Every target that includes Mbed TLS headers (`CSrtp`,
`CDataChannel`) sets the same define, since the library's structure layouts
depend on the configuration. Warnings are silenced and Clang modules are
off on this target only.

## Updating

1. Change the pin and archive SHA-256 in `ios/scripts/vendor-sources.sh`
   (the newest Mbed TLS 3.6 LTS release; the desktop does not use Mbed TLS).
2. Run `ios/scripts/vendor-sources.sh mbedtls` and paste the row it prints
   into `ios/THIRD-PARTY.md`.
3. Update this file, the target's settings in `Package.swift` if the
   library's own build changed them, and
   `packaging/third-party-licenses/mbedtls.txt` if the licence changed.
4. Run `ios/scripts/swift-test.sh` and `ios/scripts/interop-test.sh`.

Do not edit the vendored files in place.
