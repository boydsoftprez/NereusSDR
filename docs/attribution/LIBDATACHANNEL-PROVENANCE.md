# libdatachannel Provenance - NereusSDR vendored-library inventory

This document catalogs libdatachannel as vendored into NereusSDR for iPhone
and iPad under `ios/NereusKit/Sources/CDataChannel/`. The app's media peer
(`MediaPeer`, `RtcBridge` in `NereusMedia`) uses its C API, `rtc/rtc.h`, to
answer the Core's media connection: ICE, DTLS, the SCTP display channel and
SRTP audio (design spec `docs/architecture/2026-09-23-iphone-app-design.md`
section 4.2). The desktop fetches the same release at build time through
`cmake/NereusRemoteMedia.cmake`; this file records the copy committed under
`ios/` and the shared DTLS startup patch applied to both copies.

libdatachannel is MPL-2.0, compatible with NereusSDR's GPLv3 and with the
App Store permission in `ios/LICENSE`. The local patches are recorded
below; the modified files stay published with NereusSDR's source, as
MPL-2.0 requires, and every other file is the pinned upstream archive's.

## Upstream

- **Project:** libdatachannel (Paul-Louis Ageneau and contributors)
- **Repository:** https://github.com/paullouisageneau/libdatachannel
- **Pinned release:** `v0.24.5` (the desktop's pin)
- **Archive:** https://codeload.github.com/paullouisageneau/libdatachannel/tar.gz/refs/tags/v0.24.5
- **Archive SHA-256:** `454537c3cd526bed935d847bb2dff4046f266eef84d43b2a5f2f2f293c0026f4`
- **Vendored:** 2026-09-24 (iPhone app plan, Task 10)

## License

MPL-2.0. The upstream `LICENSE` is kept unchanged at
`ios/NereusKit/Sources/CDataChannel/LICENSE`; the notice is
`packaging/third-party-licenses/libdatachannel.txt` with the full text in
`packaging/third-party-licenses/MPLv2.txt`. The app's licences screen shows
it (row in `ios/THIRD-PARTY.md`).

## Files vendored

`ios/scripts/vendor-sources.sh libdatachannel` writes the copy: it downloads
the archive, refuses it unless the SHA-256 matches, and copies these files,
keeping their upstream paths:

- `include/rtc/` (the public headers, the C API among them);
- `src/` and `src/impl/` (every `.cpp` and `.hpp`);
- `LICENSE`.

It then applies the patches in `ios/patches/libdatachannel/` (see Patches).
Left out: `deps/` (the dependencies are vendored as their own targets:
`CJuice`, `CSrtp`, `CUsrsctp`, `CPlog`), the examples, tests, pages and
build files. The script also writes `VENDORED.txt` (pin, archive, hash,
file count, patches) and `include/module.modulemap`, which exposes only
`rtc/rtc.h` to Swift and none of the C++ headers. `swift-test.sh` runs
`vendor-sources.sh libdatachannel --verify` before the tests, which fails
unless the committed copy matches the archive plus the patches byte for
byte.

## Patches

`ios/patches/libdatachannel/0001-mbedtls-fingerprint-mismatch-fails-handshake.patch`
changes `src/impl/dtlstransport.cpp` (Mbed TLS branch only; the OpenSSL and
GnuTLS code the desktop uses is untouched).

In v0.24.5, `DtlsTransport::CertificateCallback` returns `1` when the peer's
certificate does not match the fingerprint in the remote description. Mbed
TLS passes a verify callback's return back from `mbedtls_ssl_handshake`,
and `rtc::impl::mbedtls::check` treats a positive value as success, so the
handshake was logged as finished and the DTLS transport went to Connected.
A media peer still failed afterwards, because no keys were exported for
SRTP, but a data-channel-only peer had nothing to break and the check was
not what stopped the media peer.

The patch follows Mbed TLS's verify-callback convention (return 0 and set
`*flags`, or return a negative `MBEDTLS_ERR_X509_*` for a fatal error; the
transport uses `MBEDTLS_SSL_VERIFY_OPTIONAL`, under which flags alone are
ignored): on a mismatch the callback sets `MBEDTLS_X509_BADCERT_NOT_TRUSTED`
and returns `MBEDTLS_ERR_X509_CERT_VERIFY_FAILED`, which Mbed TLS reports as
a fatal error, and the handshake loop treats any positive return from
`mbedtls_ssl_handshake` as a failure. `DtlsFingerprintTests` (a
data-channel-only pair: a wrong fingerprint never opens the channel and the
log never says the handshake finished; the matching fingerprint connects)
and `MediaPeerInteropTests.aWrongFingerprintInTheOfferNeverConnects` hold
it.

Upstream: as of 2026-09-24, no release after v0.24.5 exists, and both
`master` and the `v0.24` branch still return 1 from this callback. Drop the
patch once a release fixes it and the desktop's pin moves to that release.

`ios/patches/libdatachannel/0002-c-api-deferred-gathering-and-remote-fingerprint.patch`
changes `include/rtc/rtc.h` and `src/capi.cpp` (iPhone app plan Task 28a,
2026-09-27). The app drives the library through its C API, which in v0.24.5
lacks two things the C++ API has and the control connection through the
remote access service needs (link document, "Control over a data channel"):

- gathering held until the relay is known: `rtcConfiguration` gains
  `disableAutoGathering` (the last field, so a zeroed configuration keeps
  today's behaviour; the C++ API's `Configuration::disableAutoGathering`),
  and `rtcGatherLocalCandidates(pc, turnServers, count)` wraps
  `PeerConnection::gatherLocalCandidates(additionalIceServers)` with the
  relay servers as `turn:` URLs. The device's offer goes out in `introduce`
  before the Core's answer brings the relay credentials;
- the certificate the Core presented in DTLS:
  `rtcGetRemoteFingerprint(pc, buffer, size)` wraps
  `PeerConnection::remoteFingerprint()`, which the DTLS verifier records
  from the certificate the handshake carried, as `"<algorithm> <value>"`.
  The device checks the Core's identity against that, never the SDP's
  fingerprint, which arrives through the service.

The desktop and the Core use the C++ API and need neither. The C API's
other calls are unchanged. `ControlPeerTests` (no candidate before
`rtcGatherLocalCandidates`, host candidates after it),
`RendezvousDialerTests` (the fingerprint read after the channel opens is
the answering peer's certificate) and `ControlChannelInteropTests` (a relay
allocation gathered with the servers given, and a whole session against the
Core's own code) hold it; drop the patch once a release carries these
calls.

### ICE candidate resource lifetime

`ios/patches/libdatachannel/0004-ice-agent-lifetime-completion.patch`
(2026-09-27, NereusSDR-original, MPL-2.0) adds an optional creation-time
C lifetime callback. The configuration retains a shared token before any
configuration parsing can fail. Every libjuice ICE transport copies the
token before creating its agent, and transfers it to patch 0003's retirement
owner. That owner releases it only after `juice_destroy` returns. Retaining
the configuration also covers concurrent or late transport creation; this
uses no weak-pointer expiration or peer-handle deletion as a send barrier.

`rtcCreatePeerConnectionWithLifetime` consumes its callback pointer on both
success and failure and calls back exactly once when all native owners are
gone. The callback can occur synchronously on creation failure and must not
block or reenter RTC. Swift passes a separately retained `RtcPeerLifetime`
completion box, without a bridge or peer reference, and resumes waiters
outside its lock. Ordinary peers use the unchanged creation API. A control
peer deletes its handles off callback threads, then awaits the native
lifetime before releasing its persistent relay claim. This preserves the
independent media context and covers teardown delayed on the native queue.

`CPeerLifetimeTestSupport` is a tests-only target that holds the real
`TearDownProcessor` queue. `ControlPeerTests` admits the claimed candidate,
observes the gathered agent's STUN checks, holds teardown past C handle
deletion, and verifies that the claim is still unavailable. After native
destruction, the same relay socket accepts and replies to a replacement UDP
sender. Other cases cover no ICE, malformed configuration, exactly-once
failure callback, repeated close and cancellation of a close waiter.

## Desktop/Core and iOS DTLS startup patch

`cmake/patches/libdatachannel-0005-defer-dtls-startup.patch` changes the
pinned v0.24.5 `src/impl/peerconnection.cpp` in the build-owned desktop/Core
copy (2026-10-05, J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex).
The byte-identical `ios/patches/libdatachannel/0005-defer-dtls-startup.patch`
is applied by `ios/scripts/vendor-sources.sh` to the committed iOS copy.
Both are MPL-2.0 and carry that exact file's two copyright lines and MPL
notice byte for byte; `CDataChannel/VENDORED.txt` records the iOS patch.

The libjuice Connected callback holds its registry lock. Inline DTLS start
can wait for the SSL mutex while a receive worker holds that mutex and
sends a relayed handshake record through the registry. Required Linux CI
run 37287215172, job 111688854690, captured this lock inversion during
`tst_path_racer`'s initial handshake. The patch keeps ICE state changes in
place and queues DTLS initialization on the existing peer processor with
owning peer/ICE captures. It rejects closing, failed, disconnected or stale
work and accepts an ICE transport already Completed. Existing admitted
startup/close overlap retains the post-start stop/member-clear behavior;
this patch does not promise zero transient startup after a concurrent close.
Fingerprint checks, MTU-before-incoming, dependency pins, deadlines and
TURN retirement/lifetime policy are unchanged.

`tst_dtls_startup` compiles the actual patched OpenSSL vendor code in an
isolated test library. Test-only generated observers and friendship hold
the real SSL mutex and verify the Connected callback can return, then
cover obsolete queued work, completion, close around publication, retained
owners and exception/processor cleanup. Observers are absent from shipping
RTC. Actual OpenSSL causal red and corrected focused green were observed;
Mbed TLS has the same source-level SSL/relay-registry lock ordering, but
its causal runtime regression has not been captured. Existing iOS DTLS
fingerprint and owner-lifetime tests remain required validation.
The unchanged path-racer case remains integration coverage for the
original relay handshake. Drop or revisit this patch when the upstream pin
changes or upstream fixes this synchronous startup lock boundary.

## Build wiring

`ios/NereusKit/Package.swift` builds the copy as the C++17 target
`CDataChannel` with the defines the library's own CMake build sets for this
configuration: `RTC_STATIC`, `RTC_EXPORTS`, `RTC_ENABLE_MEDIA=1`,
`RTC_ENABLE_WEBSOCKET=0`, `RTC_SYSTEM_JUICE=0`, `RTC_SYSTEM_SRTP=0`,
`USE_MBEDTLS=1` (Mbed TLS for DTLS, where the desktop uses OpenSSL),
`USE_GNUTLS=0`, `USE_NICE=0` and `JUICE_STATIC`, plus
`MBEDTLS_SSL_DTLS_SRTP` to match `CMbedTLS`. Header search paths are
`include/rtc`, `src` and `../CPlog/include`. Compiler warnings are
silenced (`-w`) and Clang modules are off (`-fno-modules`) on this target
only, since the files stay unchanged.

## Updating

1. Change the pin and archive SHA-256 in `ios/scripts/vendor-sources.sh`
   (keep it equal to the desktop's pin in `cmake/NereusRemoteMedia.cmake`, which the Core's media transport builds from).
2. Run `ios/scripts/vendor-sources.sh libdatachannel` and paste the row it prints
   into `ios/THIRD-PARTY.md`.
3. Check whether the patches in `ios/patches/libdatachannel/` still apply
   and are still needed; the script fails if one does not apply.
4. Update this file, the target's settings in `Package.swift` if the
   library's own build changed them, and
   `packaging/third-party-licenses/libdatachannel.txt` if the licence changed.
5. Run `ios/scripts/swift-test.sh` and `ios/scripts/interop-test.sh`.

Do not edit the vendored files in place; change the patch files and run the
script again.
