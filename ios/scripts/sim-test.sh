#!/bin/sh
# NereusSDR for iOS: gets a simulator ready with the app just built, runs the chosen tests without building, and keeps the logs of a run that fails
# SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#
# Usage: ios/scripts/sim-test.sh <simulator name> <derived data folder> [xcodebuild arguments...]
#
# Run after `xcodebuild ... -derivedDataPath <folder> build-for-testing`, for
# example:
#
#   ios/scripts/sim-test.sh "iPhone 17" "$DD" -only-testing:NereusSDRTests/PanStackTests
#
# This:
#
#   1. runs ios/scripts/sim-ready.sh with the NereusSDR.app built in the
#      folder, so the new install and its first launch happen there, with
#      retries, and not first inside xcodebuild (the PR 653 re-review e, I2),
#   2. runs `xcodebuild test-without-building` on that simulator with the
#      folder, no test diagnostics, no parallel testing, -jobs
#      SIM_TEST_JOBS (default 2) and the arguments given,
#   3. when xcodebuild fails, for a test that failed or for "The test runner
#      hung before establishing connection", copies out what the next look
#      needs before Xcode prunes it: the run's result bundle, the
#      simulator's log of the last 15 minutes for testmanagerd, CoreSimulator
#      and the app (whose launch line names each process's arguments), the
#      load average and the simulators' states. They go in
#      SIM_TEST_LOGS (default <folder>/Logs/sim-test) under the time.
#
# Exits with xcodebuild's status, or sim-ready.sh's when that fails.

set -u

if [ $# -lt 2 ]; then
    echo "usage: $0 <simulator name> <derived data folder> [xcodebuild arguments...]" >&2
    exit 2
fi

name=$1
derived=$2
shift 2
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
app="$derived/Build/Products/Debug-iphonesimulator/NereusSDR.app"
jobs=${SIM_TEST_JOBS:-2}

say() {
    echo "sim-test: $*"
}

if [ ! -d "$app" ]; then
    echo "$0: no $app: build for testing with -derivedDataPath $derived first" >&2
    exit 2
fi

"$here/sim-ready.sh" "$name" "$app" || exit $?

started=$(date +%Y-%m-%dT%H%M%S)
logs=${SIM_TEST_LOGS:-$derived/Logs/sim-test}/$started
mkdir -p "$logs"
say "xcodebuild's output: $logs/xcodebuild.log"

xcodebuild -project "$root/ios/NereusSDR.xcodeproj" -scheme NereusSDR \
    -destination "platform=iOS Simulator,name=$name" -derivedDataPath "$derived" \
    -collect-test-diagnostics never -parallel-testing-enabled NO -jobs "$jobs" \
    test-without-building "$@" > "$logs/xcodebuild.log" 2>&1
status=$?
tail -n 5 "$logs/xcodebuild.log"

if [ "$status" -eq 0 ]; then
    rm -f "$logs/xcodebuild.log"
    rmdir "$logs" 2>/dev/null
    exit 0
fi

if grep -q "hung before establishing connection" "$logs/xcodebuild.log"; then
    say "the test runner hung before it connected; keeping its logs in $logs"
else
    say "xcodebuild exited $status; keeping the logs in $logs"
fi
udid=$(xcrun simctl list devices -j | python3 -c '
import json, sys
for devices in json.load(sys.stdin)["devices"].values():
    for device in devices:
        if device.get("name") == sys.argv[1] and device.get("isAvailable", True):
            print(device["udid"])
            sys.exit(0)
' "$name")
bundle=$(ls -td "$derived"/Logs/Test/*.xcresult 2>/dev/null | head -n 1)
if [ -n "$bundle" ]; then
    cp -R "$bundle" "$logs/"
fi
if [ -n "$udid" ]; then
    xcrun simctl spawn "$udid" log show --last 15m --style compact --predicate \
        'process == "testmanagerd" OR subsystem BEGINSWITH "com.apple.CoreSimulator" OR process == "NereusSDR" OR subsystem == "NereusSDR"' \
        > "$logs/simulator.log" 2>&1
fi
uptime > "$logs/load.txt" 2>&1
xcrun simctl list devices > "$logs/simulators.txt" 2>&1
exit "$status"
