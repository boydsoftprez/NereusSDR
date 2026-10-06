#!/bin/sh
# NereusSDR for iOS: runs the NereusKit package tests with swift test
# SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#
# Usage: ios/scripts/swift-test.sh [swift test arguments]
#
# First it checks that every vendored library under NereusKit/Sources is
# exactly what vendor-sources.sh extracts from its pinned archive (the
# archive is downloaded once, then cached under ios/.build/vendor-cache).
#
# With only the command line tools selected (no Xcode), swift-testing's
# framework and its interop library live outside the default search paths,
# so this adds them to the compile and to the test bundle's rpath.

set -u

here=$(cd "$(dirname "$0")" && pwd)
package_dir="$here/../NereusKit"

CLT=/Library/Developer/CommandLineTools
FW="$CLT/Library/Developer/Frameworks"
LIB="$CLT/Library/Developer/usr/lib"

developer_dir=$(xcode-select -p 2>/dev/null || true)

for library in opus libdatachannel libjuice libsrtp usrsctp plog mbedtls libsodium spake2ee; do
    "$here/vendor-sources.sh" "$library" --verify || exit 1
done

cd "$package_dir" || exit 1

if [ "$developer_dir" = "$CLT" ]; then
    swift test \
        -Xswiftc -F -Xswiftc "$FW" \
        -Xlinker -rpath -Xlinker "$FW" \
        -Xlinker -rpath -Xlinker "$LIB" \
        "$@"
else
    swift test "$@"
fi
exit $?
