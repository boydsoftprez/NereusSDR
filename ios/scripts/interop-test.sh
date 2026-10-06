#!/bin/sh
# NereusSDR for iOS: runs the app's media peer and its pairing against the station's own code
# SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#
# Usage: ios/scripts/interop-test.sh [swift test arguments]
#
# Builds the station helpers nereus_media_offerer and nereus_pairing_peer
# (tests/tools) and the Core itself, nereusd, in the station build
# directory, then runs, through swift-test.sh:
#
#   MediaPeerInteropTests   with NEREUS_MEDIA_OFFERER: the helper runs the
#       Core's LibDataChannelMediaTransport as the offering peer; the tests
#       connect the app's MediaPeer to it over 127.0.0.1.
#   PairingInteropTests     with NEREUS_PAIRING_PEER: the helper runs the
#       Core's pairing (StationServer) over standard input and output; the
#       app's PairingClient pairs with it by code and by one tap.
#   CorePairingSignInTests  with NEREUS_NEREUSD: a scratch nereusd, kept
#       off every network but loopback; the app pairs with it by code over
#       TLS, then signs in with its device key, once with the Core on
#       127.0.0.1 and once with it on ::1, reached as [::1].
#   ControlChannelInteropTests  with NEREUS_RENDEZVOUS_PEER: the rendezvous
#       service (rendezvous/server, python3 with websockets and
#       cryptography), the STUN and TURN fake (tests/tools/
#       fake_turn_server.py) and the helper's "core" mode, a Core as nereusd
#       runs it, all on 127.0.0.1; the app reaches the Core through the
#       service and runs its session over the control data channel, direct
#       and through the relay, and gives its relay allocation back.
#
# The station build directory is $NEREUS_STATION_BUILD, or build/ at the
# repository root. It must already be configured with
# -DNEREUS_BUILD_TESTS=ON; this script only builds these targets in it.

set -u

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
build_dir=${NEREUS_STATION_BUILD:-"$repo/build"}

if [ ! -f "$build_dir/CMakeCache.txt" ]; then
    echo "$0: $build_dir is not a configured station build" >&2
    echo "configure it with: cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON" >&2
    exit 1
fi

cmake --build "$build_dir" --target nereus_media_offerer || exit 1
cmake --build "$build_dir" --target nereus_pairing_peer nereusd nereus_rendezvous_peer || exit 1

offerer="$build_dir/tests/nereus_media_offerer"
pairing_peer="$build_dir/tests/nereus_pairing_peer"
nereusd="$build_dir/nereusd"
rendezvous_peer="$build_dir/tests/nereus_rendezvous_peer"
for built in "$offerer" "$pairing_peer" "$nereusd" "$rendezvous_peer"; do
    if [ ! -x "$built" ]; then
        echo "$0: the build did not produce $built" >&2
        exit 1
    fi
done

NEREUS_MEDIA_OFFERER=$offerer
NEREUS_PAIRING_PEER=$pairing_peer
NEREUS_NEREUSD=$nereusd
NEREUS_RENDEZVOUS_PEER=$rendezvous_peer
export NEREUS_MEDIA_OFFERER NEREUS_PAIRING_PEER NEREUS_NEREUSD NEREUS_RENDEZVOUS_PEER
exec "$here/swift-test.sh" --filter 'MediaPeerInteropTests|PairingInteropTests|CorePairingSignInTests|ControlChannelInteropTests' "$@"
