# usrsctp Provenance - NereusSDR vendored-library inventory

This document catalogs usrsctp as vendored into NereusSDR for iPhone and
iPad under `ios/NereusKit/Sources/CUsrsctp/`. libdatachannel
(`CDataChannel`) uses it for SCTP over DTLS, which carries the display data
channel. The desktop fetches the same revision at build time through
`cmake/NereusRemoteMedia.cmake`; this file covers the copy committed under
`ios/`.

usrsctp is BSD-3-Clause, compatible with NereusSDR's GPLv3 and with the App
Store permission in `ios/LICENSE`. The files are unmodified.

## Upstream

- **Project:** usrsctp (Randall Stewart, Michael Tuexen and contributors; libdatachannel's fork)
- **Repository:** https://github.com/paullouisageneau/usrsctp
- **Pinned commit:** `fec583d54493f879d2ae44a743423bf8a04371ab` (the desktop's pin, the revision libdatachannel v0.24.5 records)
- **Archive:** https://codeload.github.com/paullouisageneau/usrsctp/tar.gz/fec583d54493f879d2ae44a743423bf8a04371ab
- **Archive SHA-256:** `e5c114afe73c9a0ec419fab5f5b3f63f3ce57b09b90d06ea85e63dca8aed3e7c`
- **Vendored:** 2026-09-24 (iPhone app plan, Task 10)

## License

BSD-3-Clause. The upstream `LICENSE.md` is kept unchanged at
`ios/NereusKit/Sources/CUsrsctp/LICENSE.md` and reproduced in
`packaging/third-party-licenses/usrsctp.txt`. The app's licences screen
shows it (row in `ios/THIRD-PARTY.md`).

## Files vendored

`ios/scripts/vendor-sources.sh usrsctp` copies, unchanged, every `.c` and
`.h` under `usrsctplib/` (the sources `usrsctplib/CMakeLists.txt` builds and
their headers) and `LICENSE.md`. Left out: the programs, fuzzer and build
files. The script also writes `VENDORED.txt`; `swift-test.sh` checks the
copy byte for byte against the archive before the tests.

## Build wiring

`Package.swift` builds the C target `CUsrsctp`, public headers
`usrsctplib`, with the defines the desktop's CMake build of this revision
sets on Apple platforms: `__Userspace__`, `SCTP_SIMPLE_ALLOCATOR`,
`SCTP_PROCESS_LEVEL_LOCKS`, `SCTP_DEBUG`, `__APPLE_USE_RFC_2292`,
`HAVE_SYS_QUEUE_H`, `HAVE_NETINET_IP_ICMP_H`, `HAVE_NET_ROUTE_H`,
`HAVE_STDATOMIC_H`, `HAVE_SA_LEN`, `HAVE_SIN_LEN`, `HAVE_SIN6_LEN` and
`HAVE_SCONN_LEN`; without `INET` and `INET6`, as libdatachannel builds it
(SCTP runs over DTLS only). Warnings are silenced and Clang modules are
off on this target only: usrsctp's own `netinet` headers collide with the
SDK's modules otherwise.

## Updating

1. Change the pin and archive SHA-256 in `ios/scripts/vendor-sources.sh`
   (keep it equal to the desktop's pin in `cmake/NereusRemoteMedia.cmake`, which the Core's media transport builds from).
2. Run `ios/scripts/vendor-sources.sh usrsctp` and paste the row it prints
   into `ios/THIRD-PARTY.md`.
3. Update this file, the target's settings in `Package.swift` if the
   library's own build changed them, and
   `packaging/third-party-licenses/usrsctp.txt` if the licence changed.
4. Run `ios/scripts/swift-test.sh` and `ios/scripts/interop-test.sh`.

Do not edit the vendored files in place.
