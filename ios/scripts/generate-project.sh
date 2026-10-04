#!/bin/sh
# NereusSDR for iOS: writes the Xcode project from ios/project.yml with XcodeGen
# SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#
# Usage: ios/scripts/generate-project.sh
#
# Writes ios/NereusSDR.xcodeproj, which is generated and never committed.
# Needs XcodeGen (brew install xcodegen). Then, for example:
#
#   xcodebuild -project ios/NereusSDR.xcodeproj -scheme NereusSDR \
#       -destination 'platform=iOS Simulator,name=iPhone 17' test

set -u

here=$(cd "$(dirname "$0")" && pwd)
ios_dir=$(cd "$here/.." && pwd)

if ! command -v xcodegen >/dev/null 2>&1; then
    echo "$0: XcodeGen is not installed; install it with: brew install xcodegen" >&2
    exit 1
fi

exec xcodegen generate --spec "$ios_dir/project.yml" --project "$ios_dir"
