#!/bin/sh
# NereusSDR for iOS: prints the build number for the working tree it runs in
# SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#
# Usage: ios/scripts/build-number.sh
#
# The build number is the count of commits reachable from HEAD
# (`git rev-list --count HEAD`) plus kBuildNumberOffset, so it rises with
# each commit on the same history. Archive from merged main: squash merges
# can remove branch ancestors and lower the count despite newer source.
#
# The offset exists because TestFlight builds up to 2026.9.0 (5813) came
# from the old phone trunk, claude/iphone-app, whose 983 commits reached
# main squashed. Main counted 5128 on 2026-10-09, so its builds sorted
# below the older ones in TestFlight. 1000 puts main's builds above 5813
# and keeps them rising with every commit. It goes in as CURRENT_PROJECT_VERSION, which the app's
# and the Live Activity's Info.plist read as CFBundleVersion.
#
# 2026-10-31: raise the offset from 1000 to 1003 after squash merges
# left main at 8638 reachable commits, below diagnostic branch build 9640.
# The next merged-main commit therefore produces 9642, above Apple's
# uploaded build. J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#
# 2026-11-26: raise the offset from 1003 to 1100. Branch builds up to
# 10299 (2026.11.1) were uploaded from JJ's Mac while main counted 9215
# commits, so main's next build (10218) would have been refused. Main now
# numbers about 10315 and rises from there; the TestFlight workflow checks
# App Store Connect before archiving. J.J. Boyd (KG4VCF), AI-assisted via
# Claude Code.
#
# Prints the number and a newline, or nothing at all when there is no git
# repository around the current directory; the caller then leaves
# CURRENT_PROJECT_VERSION alone and project.yml's value stands, as it does
# for a build from Xcode's own window.

set -u

kBuildNumberOffset=1100

if command -v git >/dev/null 2>&1; then
    count=$(git rev-list --count HEAD 2>/dev/null || true)
    case "$count" in
        ''|*[!0-9]*) ;;
        *) printf '%s\n' "$((count + kBuildNumberOffset))" ;;
    esac
fi
exit 0
