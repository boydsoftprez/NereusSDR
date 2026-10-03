# libsodium Provenance - NereusSDR fetched-library record

libsodium is the cryptography library under the pairing code's key
exchange (iPhone app plan Task 14, spec D37): Argon2id password hashing,
the edwards25519 arithmetic SPAKE2+EE runs on, BLAKE2b, the key
derivation and the XChaCha20-Poly1305 confirmation boxes. It is fetched
at build time, not vendored, and compiled into NereusCore as a static
library. The iPhone and iPad app (plan Task 15) vendors the same archive.

NereusSDR is distributed under GPLv3 (root `LICENSE`). libsodium is ISC,
a permissive licence compatible with GPLv3 and with the App Store build
of the iPhone and iPad app.

## Upstream

- **Project:** libsodium
- **Repository:** https://github.com/jedisct1/libsodium
- **Author:** Frank Denis
- **Pinned release:** 1.0.22, git tag `1.0.22-RELEASE`
- **Archive:** `libsodium-1.0.22.tar.gz`, the fixed release archive attached
  to the GitHub release of that tag:
  `https://github.com/jedisct1/libsodium/releases/download/1.0.22-RELEASE/libsodium-1.0.22.tar.gz`
- **Archive SHA-256:** `adbdd8f16149e81ac6078a03aca6fc03b592b89ef7b5ed83841c086191be3349`
  (2008529 bytes), pinned by `URL_HASH SHA256` in `cmake/NereusPairing.cmake`
- **Signature:** the release carries `libsodium-1.0.22.tar.gz.minisig`
  (trusted comment `timestamp:1775774745 file:libsodium-1.0.22.tar.gz hashed`).
  It was **not** checked when this pin was taken (2026-09-24): minisign
  was not installed on the machine that took it. Check it with minisign
  and libsodium's published public key before the pin is next changed.
- **Never a `-stable` tarball.** Those are rolling snapshots of the stable
  branch, re-cut under the same name, so a pinned hash would break.
- **Pinned:** 2026-09-24 (iPhone app plan Task 14)

## License

ISC. The archive's `LICENSE` is copied byte for byte to
`packaging/third-party-licenses/libsodium.txt`
(SHA-256 `508a76d186356c0dd807a670ef510964f8724557024796a2c426c6c0e19ab683`).
Some compiled source files carry notices of their own (the scrypt code's
BSD notices by Colin Percival and Alexander Peslyak, public-domain and CC0
dedications). They are collected, byte for byte, in
`packaging/third-party-licenses/libsodium-notices.txt`, written by
`python3 scripts/collect-source-notices.py libsodium --build-dir <a built
tree> --output packaging/third-party-licenses/libsodium-notices.txt`;
rerun it after the pin changes.

## How it is built

libsodium has no CMake build. `cmake/NereusPairing.cmake` compiles every
`src/libsodium/**/*.c` of the pinned archive into the static library
`nereus_sodium`, with the portable configuration:

- no assembly (`HAVE_AMD64_ASM`, `HAVE_AVX_ASM` unset); with GCC and
  Clang no SIMD variants either (no `HAVE_*INTRIN_H`, no `HAVE_ARMCRYPTO`),
  so those files compile to nothing and the reference C code runs; with
  MSVC, libsodium's own `private/common.h` turns its intrinsics on, as its
  Visual Studio projects build it;
- the platform macros of the archive's own `build.zig` for macOS, Linux
  and Windows, minus its assembly and intrinsics entries;
- `HAVE_TI_MODE` with GCC and Clang on 64-bit targets (MSVC has no
  `__int128`);
- `version.h` from the archive's `builds/msvc/version.h`, as `build.zig`
  does, in a copy of the include tree under the build directory, so the
  fetched sources are used exactly as the archive has them;
- `SODIUM_STATIC` for every consumer; upstream warnings silenced on this
  target only.

Checked on the machine that took the pin (macOS arm64, Apple Clang): all
101 of the archive's own `test/default` programs pass against this build.
The macOS x86_64, Linux x86_64 and arm64, and Windows MSVC builds are
proven by CI and the Rock and Pi Docker builder.

## Updating

1. Download the new release's fixed archive and its `.minisig`; check the
   signature with minisign and libsodium's public key.
2. Update the URL and `URL_HASH` in `cmake/NereusPairing.cmake` and this
   file; compare the new `LICENSE` with `libsodium.txt` and copy it if it
   changed.
3. Rebuild and run `tst_spake_exchange` and `tst_station_pairing`.
4. Tell the iPhone session, whose Task 15 vendors the same archive.
