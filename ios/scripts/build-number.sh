#!/bin/sh
# NereusSDR for iOS: prints the build number for the working tree it runs in
# SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#
# Usage: ios/scripts/build-number.sh
#
# The build number is the count of commits reachable from HEAD
# (`git rev-list --count HEAD`), so it rises with every commit on a branch
# and every build made from a newer commit carries a larger number, as
# TestFlight asks. It goes in as CURRENT_PROJECT_VERSION, which the app's
# and the Live Activity's Info.plist read as CFBundleVersion.
#
# Prints the number and a newline, or nothing at all when there is no git
# repository around the current directory; the caller then leaves
# CURRENT_PROJECT_VERSION alone and project.yml's value stands, as it does
# for a build from Xcode's own window.

set -u

if command -v git >/dev/null 2>&1; then
    count=$(git rev-list --count HEAD 2>/dev/null || true)
    case "$count" in
        ''|*[!0-9]*) ;;
        *) printf '%s\n' "$count" ;;
    esac
fi
exit 0
