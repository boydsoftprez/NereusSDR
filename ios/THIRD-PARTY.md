# Third-party code in NereusSDR for iOS

Every library vendored into the app has one row here. The provenance check
(`scripts/verify-ios-provenance.py`) reads this table: a directory listed in
the Path column is exempt from the app's header rule, and its row must carry
the archive SHA-256 and one of the licences the App Store permission allows
(BSD-2-Clause, BSD-3-Clause, MIT, ISC, Apache-2.0, MPL-2.0). Paths are
relative to `ios/`. Each library's notice also appears on the app's licences
screen.

| Name | Version or commit | Archive URL | Archive SHA-256 | Licence | Path |
| --- | --- | --- | --- | --- | --- |
| Opus | 940d4e5af64351ca8ba8390df3f555484c567fbb | https://github.com/xiph/opus/archive/940d4e5af64351ca8ba8390df3f555484c567fbb.zip | 20e37f9079ac2b80e3235cd8ce2547829e147bf44a2f5dd28a888fa3e9c24341 | BSD-3-Clause | NereusKit/Sources/COpus |
| libdatachannel | v0.24.5, patched (ios/patches/libdatachannel) | https://codeload.github.com/paullouisageneau/libdatachannel/tar.gz/refs/tags/v0.24.5 | 454537c3cd526bed935d847bb2dff4046f266eef84d43b2a5f2f2f293c0026f4 | MPL-2.0 | NereusKit/Sources/CDataChannel |
| libjuice | 3c40a3545b6b1b62c7adee7f8f2bd58aa290afd6, patched (ios/patches/libjuice) | https://codeload.github.com/paullouisageneau/libjuice/tar.gz/3c40a3545b6b1b62c7adee7f8f2bd58aa290afd6 | a6b1d55338ea12adc0177eaafd9521ac0101b6a8716c71024a668f4896fd6b7c | MPL-2.0 | NereusKit/Sources/CJuice |
| libsrtp | 24b3bf8f19b6f5ab4cd2bcceb4f4064efca86fd5 | https://codeload.github.com/cisco/libsrtp/tar.gz/24b3bf8f19b6f5ab4cd2bcceb4f4064efca86fd5 | 063478e368d7cd13d04a908d152a46f90bfa728c4b324be6d413b0b92728207c | BSD-3-Clause | NereusKit/Sources/CSrtp |
| usrsctp | fec583d54493f879d2ae44a743423bf8a04371ab | https://codeload.github.com/paullouisageneau/usrsctp/tar.gz/fec583d54493f879d2ae44a743423bf8a04371ab | e5c114afe73c9a0ec419fab5f5b3f63f3ce57b09b90d06ea85e63dca8aed3e7c | BSD-3-Clause | NereusKit/Sources/CUsrsctp |
| plog | 94899e0b926ac1b0f4750bfbd495167b4a6ae9ef | https://codeload.github.com/SergiusTheBest/plog/tar.gz/94899e0b926ac1b0f4750bfbd495167b4a6ae9ef | 92a08bce559b5f28aa88d3fd9071567414b9f43a83fe2a05a6dd14f1da536072 | MIT | NereusKit/Sources/CPlog |
| Mbed TLS | v3.6.7 | https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2 | a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6 | Apache-2.0 | NereusKit/Sources/CMbedTLS |
| libsodium | 1.0.22-RELEASE | https://github.com/jedisct1/libsodium/releases/download/1.0.22-RELEASE/libsodium-1.0.22.tar.gz | adbdd8f16149e81ac6078a03aca6fc03b592b89ef7b5ed83841c086191be3349 | ISC | NereusKit/Sources/CSodium |
| spake2-ee | fd3ea61f27a75ff63b0f192c9e619b5a494d048e | https://codeload.github.com/jedisct1/spake2-ee/tar.gz/fd3ea61f27a75ff63b0f192c9e619b5a494d048e | 20d63587c1191b952e98b9a4d8bd557c8a6c4f6bfbac0b9fb77b28854c244bb6 | BSD-2-Clause | NereusKit/Sources/CSpake2EE |

The libdatachannel patch set includes `0004-ice-agent-lifetime-completion.patch`,
which retains external ICE candidate resources through native agent destruction.
Its modified files remain MPL-2.0; the pinned archive and licence notice are
unchanged. See `docs/attribution/LIBDATACHANNEL-PROVENANCE.md` for ownership
and verification details.
