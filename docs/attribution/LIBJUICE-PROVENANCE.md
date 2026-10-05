# libjuice Provenance - NereusSDR vendored-library inventory

This document catalogs libjuice as vendored into NereusSDR for iPhone and
iPad under `ios/NereusKit/Sources/CJuice/`. libdatachannel (`CDataChannel`)
uses it for ICE: gathering host candidates and the connectivity checks. The
desktop fetches the same revision at build time through
`cmake/NereusRemoteMedia.cmake`; this file covers the copy committed under
`ios/`.

libjuice is MPL-2.0, compatible with NereusSDR's GPLv3 and with the App
Store permission in `ios/LICENSE`. The covered files changed by the
patches below stay published with NereusSDR's source under MPL-2.0;
the other files retain the pinned upstream archive's bytes.

## Upstream

- **Project:** libjuice (Paul-Louis Ageneau and contributors)
- **Repository:** https://github.com/paullouisageneau/libjuice
- **Pinned commit:** `3c40a3545b6b1b62c7adee7f8f2bd58aa290afd6` (the desktop's pin, the revision libdatachannel v0.24.5 records)
- **Archive:** https://codeload.github.com/paullouisageneau/libjuice/tar.gz/3c40a3545b6b1b62c7adee7f8f2bd58aa290afd6
- **Archive SHA-256:** `a6b1d55338ea12adc0177eaafd9521ac0101b6a8716c71024a668f4896fd6b7c`
- **Vendored:** 2026-09-24 (iPhone app plan, Task 10)

## License

MPL-2.0. The upstream `LICENSE` is kept unchanged at
`ios/NereusKit/Sources/CJuice/LICENSE`; the notice is
`packaging/third-party-licenses/libjuice.txt` with the full text in
`packaging/third-party-licenses/MPLv2.txt`. The app's licences screen shows
it (row in `ios/THIRD-PARTY.md`).

## Files vendored

`ios/scripts/vendor-sources.sh libjuice` copies every `.c` and `.h` in
`src/` (`LIBJUICE_SOURCES` and their headers), the public header
`include/juice/juice.h`, and `LICENSE`, then applies the patches in
`ios/patches/libjuice/` (see Patches). Left out: the tests, fuzzer and
build files. The script also writes `VENDORED.txt`; `swift-test.sh` checks
the copy byte for byte against the archive plus the patches before the
tests.

## Patches

`ios/patches/libjuice/0001-give-turn-allocations-back.patch` changes
`src/agent.c` (iPhone app plan Task 28a, 2026-09-27). libjuice never gives
a TURN allocation back, so when a connection ends the relay holds it, and
a place in the per-user quota of the Core's id, for the rest of its
lifetime (`TURN_LIFETIME`, ten minutes). The link document ("Control over a
data channel") has each end give its allocations back at once with a
Refresh whose LIFETIME is 0 (RFC 8656 section 7.2). When an agent is
destroyed, before its connection is, each relay entry the relay granted
gets one such Refresh, signed with the entry's credentials and not
retransmitted. The desktop and the Core make the same change to the same
revision (`cmake/patches/libjuice-0001-give-turn-allocations-back.patch`,
applied by `cmake/NereusRemoteMedia.cmake`). The change becomes part of a covered
file, so it is under MPL-2.0 (section 3.1), not NereusSDR's GPLv3, and
`packaging/third-party-licenses/libjuice.txt` says so.
`ControlChannelInteropTests` (run by `ios/scripts/interop-test.sh`) holds
it against the TURN fake the desktop's tests use
(`tests/tools/fake_turn_server.py`): the control peer's own allocation is
given back when it closes, and a session through the relay gives back every
allocation of both ends when it ends.

`ios/patches/libjuice/0002-bounded-turn-release-lifecycle.patch` changes
`include/juice/juice.h`, `src/agent.c`, `src/agent.h` and `src/juice.c`.
It adds the nonblocking begin/status close API, keeps the original socket
and credentials for authenticated release retries within a shared
five-second deadline, and retains the agent until release and resolver
completion. Normal ICE traffic and callbacks stop during closing. The
legacy direct-destroy path remains, and the resolver barrier is compiled
only for tests.

`ios/patches/libjuice/0003-keep-poll-worker-with-retained-agents.patch`
changes `src/conn_poll.c`. A finished agent still owns its connection
registry. The worker now remains blocked on that registry's interrupt
socket until all agents are removed, allowing a new agent to wake it and
receive bookkeeping. Preparation and fatal poll errors still stop the
worker; the change adds no thread, polling loop or timeout. This is the
same patch as the Core's
`cmake/patches/libjuice-0003-keep-poll-worker-with-retained-agents.patch`
at checkpoint `b44d63876158e6d8aa982f46629cecccfeb6bab3`. It preserves the
upstream notice and records J.J. Boyd's AI-assisted modification history.
The causal poll-worker regression exercises retained finished agents,
interrupt-only waiting, normal close and last-agent destruction.

## Build wiring

`Package.swift` builds the C target `CJuice` with the defines its CMake
build sets as libdatachannel configures it: `JUICE_STATIC`, `JUICE_EXPORTS`,
`NO_SERVER` and `USE_NETTLE=0` (libjuice's own hash functions), header
search path `include/juice`, with warnings silenced and Clang modules off
on this target only.

## Updating

1. Change the pin and archive SHA-256 in `ios/scripts/vendor-sources.sh`
   (keep it equal to the desktop's pin in `cmake/NereusRemoteMedia.cmake`, which the Core's media transport builds from).
2. Run `ios/scripts/vendor-sources.sh libjuice` and paste the row it prints
   into `ios/THIRD-PARTY.md`.
3. Check whether the patches in `ios/patches/libjuice/` still apply and
   are still needed; the script fails if one does not apply.
4. Update this file, the target's settings in `Package.swift` if the
   library's own build changed them, and
   `packaging/third-party-licenses/libjuice.txt` if the licence changed.
5. Run `ios/scripts/swift-test.sh` and `ios/scripts/interop-test.sh`.

Do not edit the vendored files in place; change the patch files and run the
script again.
