#!/bin/sh
# NereusSDR for iOS: compile and run the isolated macOS cleanup-lifetime probe
# SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#
# Usage: ios/scripts/retirement-probe.sh
# Requires Xcode's macOS toolchain. Builds the pinned CDataChannel target and
# dependencies first. The probe runs as its own process, so rtc::Cleanup()
# cannot stop another test suite's global thread pool.

set -eu
here=$(cd "$(dirname "$0")" && pwd)
ios_dir=$(cd "$here/.." && pwd)
package_dir="$ios_dir/NereusKit"

(cd "$package_dir" && swift build --configuration debug --target CDataChannel)

products="$package_dir/.build/out/Products/Debug"
output="$ios_dir/.build/retirement-probe"
mkdir -p "$(dirname "$output")"

clang++ -std=c++17 -pthread \
    -DRTC_STATIC -DRTC_ENABLE_MEDIA=1 -DRTC_ENABLE_WEBSOCKET=0 \
    -DRTC_SYSTEM_JUICE=0 -DRTC_SYSTEM_SRTP=0 \
    -DUSE_MBEDTLS=1 -DUSE_GNUTLS=0 -DUSE_NICE=0 -DJUICE_STATIC \
    -I"$package_dir/Sources/CDataChannel/include" \
    -I"$package_dir/Sources/CDataChannel/include/rtc" \
    -I"$package_dir/Sources/CDataChannel/src" \
    -I"$package_dir/Sources/CPlog/include" \
    -I"$package_dir/Sources/CJuice/include" \
    -I"$package_dir/Sources/CSrtp/include" \
    -I"$package_dir/Sources/CMbedTLS/include" \
    -I"$package_dir/Sources/CUsrsctp/usrsctplib" \
    "$package_dir/Tests/RetirementProbe/main.cpp" \
    "$products/CDataChannel.o" "$products/CJuice.o" \
    "$products/CMbedTLS.o" "$products/CSrtp.o" "$products/CUsrsctp.o" \
    -framework Security -framework CoreFoundation -framework Network \
    -o "$output"

"$output"
