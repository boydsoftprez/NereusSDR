# R3 libdatachannel v0.24.5 transport probe

Date: 2026-09-20. Scope was an isolated macOS loopback validation. No NereusSDR
product file, CMake file, hardware, STUN/TURN server, WSS signaling endpoint,
LAN path, or relay was used.

## Pinned input and dependency recovery

The supplied top-level tree was the libdatachannel v0.24.5 tree at
`443f6934d9007eb7076ab7825ba330f355fcbead`. Its supplied
`libdatachannel-tree.json` records these submodule commits, recovered from
`https://codeload.github.com` because `github.com` DNS was unavailable:

| dependency | pinned commit | downloaded archive SHA-256 |
| --- | --- | --- |
| plog | `94899e0b926ac1b0f4750bfbd495167b4a6ae9ef` | `92a08bce559b5f28aa88d3fd9071567414b9f43a83fe2a05a6dd14f1da536072` |
| usrsctp | `fec583d54493f879d2ae44a743423bf8a04371ab` | `e5c114afe73c9a0ec419fab5f5b3f63f3ce57b09b90d06ea85e63dca8aed3e7c` |
| libjuice | `3c40a3545b6b1b62c7adee7f8f2bd58aa290afd6` | `a6b1d55338ea12adc0177eaafd9521ac0101b6a8716c71024a668f4896fd6b7c` |
| nlohmann/json | `55f93686c01528224f448c19128836e7df245f72` | `67f4cdd9ca930c9c1e130af4a437c7fc98fab77a2846fc2d2a14b4943831f8ef` |
| libsrtp | `24b3bf8f19b6f5ab4cd2bcceb4f4064efca86fd5` | `063478e368d7cd13d04a908d152a46f90bfa728c4b324be6d413b0b92728207c` |

Before extracting each archive, the Python 3.9 routine enumerated every member
and rejected absolute paths, `..` path components, and unsafe symlink targets.
All five archives passed (`200`, `281`, `92`, `1271`, and `2406` members
respectively; zero unsafe entries).

## License and notice evidence

The top-level `LICENSE` is MPL-2.0. Its `README.md` explicitly says the project
has used MPL-2.0 since 0.18; the old LGPLv2.1-or-later statement does not apply
to this pinned release. `deps/libjuice/LICENSE` is also MPL-2.0 (not LGPL).
Neither pinned source tree contains an applied Exhibit B "Incompatible With
Secondary Licenses" notice in source files; the only hits are the template text
inside the MPL license itself. MPL-2.0 section 3.3 therefore supplies its normal
secondary-license route, which includes GPLv2 or later, and is source evidence
that the library can be combined into this GPLv3 project subject to MPL's
conditions. This is an engineering license audit, not legal advice.

The notice inventory from the pinned source is:

| component | license | distribution action indicated by its text |
| --- | --- | --- |
| libdatachannel | MPL-2.0 | retain MPL notice and make MPL-covered source available as required |
| libjuice | MPL-2.0 | retain MPL notice and make MPL-covered source available as required |
| plog | MIT | include copyright and permission notice |
| usrsctp | 3-clause BSD | reproduce copyright, conditions, and disclaimer in binary documentation/materials |
| libsrtp | 3-clause BSD | reproduce copyright, conditions, and disclaimer in binary documentation/materials |
| nlohmann/json | MIT | include copyright and permission notice; not built with `NO_EXAMPLES=ON` |

There are no separate `NOTICE` files in the recovered top-level/dependency
source. The configured host found Homebrew OpenSSL 3.6.3 dynamically; it is not
a pinned libdatachannel submodule or folded into `libdatachannel-static.a`, but
the selected OpenSSL license/notice must be included if a shipped application
links it.

## Static build

Executed successfully:

```sh
cmake -S /Users/j.j.boyd/.config/nereus/work/libdatachannel-0.24.5 \
  -B /Users/j.j.boyd/.config/nereus/work/r3-transport-probe/build \
  -DBUILD_SHARED_LIBS=OFF -DNO_WEBSOCKET=ON -DNO_EXAMPLES=ON -DNO_TESTS=ON \
  -DUSE_GNUTLS=OFF -DUSE_MBEDTLS=OFF
cmake --build /Users/j.j.boyd/.config/nereus/work/r3-transport-probe/build \
  --target datachannel-static -j 4
```

Result: `libdatachannel-static.a` was produced at 15 MB. CMake selected
OpenSSL 3.6.3 and built the pinned libjuice, usrsctp, plog, and libsrtp sources.
Media support was on and WebSocket support was off.

## Actual two-peer loopback probe

`transport_probe.cpp` is a standalone, local-only verification program. Each
round uses two new `PeerConnection` objects, host ICE candidates only (no ICE
servers), MTU `1000`, `forceMediaTransport=true`, and disabled auto-negotiation
so that the explicit initial offer contains both channels. It configures the spectrum
DataChannel as `unordered=true`, `maxRetransmits=0`, sends a binary payload, and
compares received bytes. Independently it negotiates an audio track with
`addOpusCodec(111)`, constructs one RTP packet (PT 111), sends through the
track, and compares the decrypted received RTP bytes. It closes both peers and
runs the same check with a new peer pair.

```text
round=1 datachannel=unreliable-binary-ok rtp-srtp-opus-track-ok closed=ok
round=2 datachannel=unreliable-binary-ok rtp-srtp-opus-track-ok closed=ok
probe=pass recreated=ok mtu=1000 ice_servers=none
```

The first version did not set `forceMediaTransport`; adding the media track
after the data-channel negotiation yielded libdatachannel's observed error
`The connection has no media transport`. The source comment at
`src/impl/peerconnection.cpp:909` identifies this case and requires
`forceMediaTransport=true`; with that explicit setting, both rounds passed.

This proves local DTLS/SCTP delivery and local SRTP-protected RTP-track
transport for a track negotiated as Opus. It does **not** prove Opus encoder or
decoder fidelity, playout/jitter-buffer behavior, WSS signaling authentication,
direct Internet traversal, relay fallback, LAN connectivity, Qt integration, or
hardware audio.

## MTU API finding

`Configuration::mtu` accepts `1000`; source rejects values below `576`.
`Track::maxMessageSize()` reports `mtu - 12 - 8 - 40`, so this configuration
reports 940 bytes for the pre-SRTP RTP packet. `Track::send()` forwards the
caller-provided bytes to `DtlsSrtpTransport::sendMedia()`, which adds the SRTP
authentication trailer, but neither path checks that a raw RTP message is at or
below `Track::maxMessageSize()`. RTP packet sizing/packetization is therefore
the caller's responsibility. The R3 sender should cap each pre-SRTP RTP packet
at `track->maxMessageSize()` (or employ a packetizer); this probe intentionally
does not implement an audio jitter buffer or packetizer.
