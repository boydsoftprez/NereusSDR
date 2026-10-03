# SPAKE2+EE (spake2-ee) Provenance - NereusSDR fetched-library record

spake2-ee is the password-authenticated key exchange behind the pairing
code (iPhone app plan Task 14, spec D37; pairing design section 4.3): a
device and the Core that share the short code agree keys, and a
rendezvous that carries their messages learns nothing it could use to
guess the code offline. It is fetched at build time, not vendored, and
compiled into NereusCore as a static library over libsodium
(`LIBSODIUM-PROVENANCE.md`). NereusSDR calls it only through
`src/core/security/SpakeExchange.{h,cpp}`. The iPhone and iPad app (plan
Task 15) vendors the same commit.

NereusSDR is distributed under GPLv3 (root `LICENSE`). spake2-ee is
BSD-2-Clause, compatible with GPLv3 and with the App Store build of the
iPhone and iPad app.

## Upstream

- **Project:** SPAKE2+EE (SPAKE2+ Elligator Edition) for libsodium
- **Repository:** https://github.com/jedisct1/spake2-ee
- **Author:** Frank Denis
- **Pinned commit:** `fd3ea61f27a75ff63b0f192c9e619b5a494d048e` (the
  repository has no tags)
- **Archive:** the commit's codeload archive,
  `https://codeload.github.com/jedisct1/spake2-ee/tar.gz/fd3ea61f27a75ff63b0f192c9e619b5a494d048e`
- **Archive SHA-256:** `20d63587c1191b952e98b9a4d8bd557c8a6c4f6bfbac0b9fb77b28854c244bb6`
  (5772 bytes), pinned by `URL_HASH SHA256` in `cmake/NereusPairing.cmake`
- **Pinned:** 2026-09-24 (iPhone app plan Task 14)

## License

BSD-2-Clause, "Copyright (c) 2017-2026, Frank Denis". The archive's
`LICENSE` is copied byte for byte to
`packaging/third-party-licenses/spake2-ee.txt`
(SHA-256 `5538ee99f815dcf87fe40e1b9181fb1f928a2ce6613aeb3b03b8992cf87aa190`).
The compiled files (`src/crypto_spake.c`, `src/crypto_spake.h`,
`src/pushpop.h`) carry no notice of their own
(`scripts/collect-source-notices.py spake2-ee --build-dir <a built tree>`
finds none), so there is no `spake2-ee-notices.txt`.

## How it is built

`cmake/NereusPairing.cmake` compiles `src/crypto_spake.c` of the pinned
archive, unchanged, into the static library `nereus_spake2ee`, linked to
`nereus_sodium`. Upstream warnings are silenced on this target only.

## How NereusSDR uses it

The fixed values both ends use (the iPhone app plan's Part C wire values):
client identity `"nereussdr-device-v1"`, server identity
`"nereussdr-station-v1"`, password hashing `crypto_pwhash_OPSLIMIT_INTERACTIVE`
and `crypto_pwhash_MEMLIMIT_INTERACTIVE` with libsodium's default
algorithm (Argon2id), which a device checks in the Core's first message
(`crypto_spake_validate_public_data`) before it hashes the code. The
wire is the link document's Pairing section
(`docs/architecture/2026-09-23-station-link-v1.md`).

## Updating

1. Take the new commit's codeload archive, hash it, and read the diff of
   `src/` against this commit.
2. Update the URL and `URL_HASH` in `cmake/NereusPairing.cmake` and this
   file; compare `LICENSE` with `spake2-ee.txt`.
3. Rebuild and run `tst_spake_exchange` and `tst_station_pairing`, and the
   iPhone app's pairing interop test against `nereus_pairing_peer`.
4. Tell the iPhone session, whose Task 15 vendors the same commit.
