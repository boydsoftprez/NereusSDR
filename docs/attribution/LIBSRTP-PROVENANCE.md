# libsrtp Provenance - NereusSDR vendored-library inventory

This document catalogs libsrtp as vendored into NereusSDR for iPhone and
iPad under `ios/NereusKit/Sources/CSrtp/`. libdatachannel (`CDataChannel`)
uses it to protect the audio line (SRTP, keyed from the DTLS handshake). The
desktop fetches the same revision at build time through
`cmake/NereusRemoteMedia.cmake`, with OpenSSL as its crypto engine; the app
builds it with Mbed TLS. This file covers the copy committed under `ios/`.

libsrtp is BSD-3-Clause, compatible with NereusSDR's GPLv3 and with the App
Store permission in `ios/LICENSE`. The files are unmodified.

## Upstream

- **Project:** libsrtp (Cisco Systems, Inc.)
- **Repository:** https://github.com/cisco/libsrtp
- **Pinned commit:** `24b3bf8f19b6f5ab4cd2bcceb4f4064efca86fd5` (the desktop's pin, libsrtp 2.8.0)
- **Archive:** https://codeload.github.com/cisco/libsrtp/tar.gz/24b3bf8f19b6f5ab4cd2bcceb4f4064efca86fd5
- **Archive SHA-256:** `063478e368d7cd13d04a908d152a46f90bfa728c4b324be6d413b0b92728207c`
- **Vendored:** 2026-09-24 (iPhone app plan, Task 10)

## License

BSD-3-Clause. The upstream `LICENSE` is kept unchanged at
`ios/NereusKit/Sources/CSrtp/LICENSE` and reproduced in
`packaging/third-party-licenses/libsrtp.txt`. The app's licences screen
shows it (row in `ios/THIRD-PARTY.md`).

## Files vendored

`ios/scripts/vendor-sources.sh libsrtp` copies, unchanged, the source lists
its `CMakeLists.txt` builds with the Mbed TLS crypto engine
(`ENABLE_MBEDTLS`): `srtp/srtp.c`; `crypto/cipher/` `cipher.c`,
`cipher_test_cases.c` and `.h`, `null_cipher.c`, `aes_icm_mbedtls.c`,
`aes_gcm_mbedtls.c`; `crypto/hash/` `auth.c`, `auth_test_cases.c` and `.h`,
`null_auth.c`, `hmac_mbedtls.c`; `crypto/kernel/` `alloc.c`,
`crypto_kernel.c`, `err.c`, `key.c`; `crypto/math/datatypes.c`;
`crypto/replay/` `rdb.c`, `rdbx.c`; the headers in `crypto/include/` and
`include/`; and `LICENSE`. Left out: the OpenSSL, NSS and built-in AES and
SHA-1 files, the tests, fuzzer and build files.

The script also writes `VENDORED.txt` and `config/config.h`, the values
libsrtp's `config_in_cmake.h` takes for this build (what its CMake
`configure_file` would write): package version 2.8.0, `MBEDTLS` and `GCM`
in place of the desktop's `OPENSSL`, `CPU_CISC`, and the headers and
functions every Apple platform has. `swift-test.sh` checks the copy byte for
byte against the archive before the tests.

## Build wiring

`Package.swift` builds the C target `CSrtp` with `HAVE_CONFIG_H`, header
search paths `config` and `crypto/include`, and `MBEDTLS_SSL_DTLS_SRTP` to
match `CMbedTLS`, with warnings silenced and Clang modules off on this
target only.

## Updating

1. Change the pin and archive SHA-256 in `ios/scripts/vendor-sources.sh`
   (keep it equal to the desktop's pin in `cmake/NereusRemoteMedia.cmake`, which the Core's media transport builds from).
2. Run `ios/scripts/vendor-sources.sh libsrtp` and paste the row it prints
   into `ios/THIRD-PARTY.md`.
3. Update this file, the target's settings in `Package.swift` if the
   library's own build changed them, and
   `packaging/third-party-licenses/libsrtp.txt` if the licence changed.
4. Run `ios/scripts/swift-test.sh` and `ios/scripts/interop-test.sh`.

Do not edit the vendored files in place.
