#!/bin/sh
# NereusSDR for iOS: boots a simulator, waits until it has really finished booting, then warm-launches the app
# SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#
# Usage: ios/scripts/sim-ready.sh <simulator name> [path to NereusSDR.app]
#
# Run before xcodebuild's tests on a simulator. On a busy Mac a cold simulator
# is not ready when it first says so: `simctl bootstatus -b` has returned
# early (exit status -1 while "Waiting on Data Migration"), and xcodebuild's
# first launch of the app then failed ("failed to launch", "No such
# process", "The test runner hung before establishing connection"). So this:
#
#   1. boots the simulator named <simulator name> (one the name matches
#      exactly; it must already exist),
#   2. loops until simctl lists it as Booted and `simctl bootstatus -b`
#      exits 0, checking both every round,
#   3. installs the app when a path is given, then launches it and
#      terminates it again, retrying until one launch succeeds.
#
# Without an app path step 3 launches the app already installed there.
# Gives up with exit 1 after SIM_READY_TIMEOUT seconds (default 900).

set -u

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "usage: $0 <simulator name> [path to NereusSDR.app]" >&2
    exit 2
fi

name=$1
app=${2:-}
bundle=com.boydsoftprez.NereusSDR.ios
timeout=${SIM_READY_TIMEOUT:-900}
started=$(date +%s)

say() {
    echo "sim-ready: $*"
}

out_of_time() {
    [ $(( $(date +%s) - started )) -ge "$timeout" ]
}

# The simulator's UDID and state, from simctl's JSON: "<udid> <state>".
device_line() {
    xcrun simctl list devices -j | python3 -c '
import json, sys
name = sys.argv[1]
for devices in json.load(sys.stdin)["devices"].values():
    for device in devices:
        if device.get("name") == name and device.get("isAvailable", True):
            print(device["udid"], device["state"])
            sys.exit(0)
sys.exit(1)
' "$name"
}

line=$(device_line) || {
    echo "$0: no available simulator is named \"$name\"" >&2
    exit 1
}
udid=${line%% *}
say "$name is $udid"

round=0
while :; do
    round=$((round + 1))
    state=$(device_line | awk '{print $2}')
    if [ "$state" != "Booted" ]; then
        say "round $round: state $state, booting"
        xcrun simctl boot "$udid" 2>/dev/null
    fi
    xcrun simctl bootstatus "$udid" -b >/dev/null 2>&1
    status=$?
    state=$(device_line | awk '{print $2}')
    if [ "$status" -eq 0 ] && [ "$state" = "Booted" ]; then
        say "round $round: booted, bootstatus exit 0"
        break
    fi
    say "round $round: bootstatus exit $status, state $state; waiting"
    if out_of_time; then
        echo "$0: $name did not finish booting within ${timeout}s" >&2
        exit 1
    fi
    sleep 5
done

if [ -n "$app" ]; then
    attempt=0
    until xcrun simctl install "$udid" "$app"; do
        attempt=$((attempt + 1))
        say "install attempt $attempt failed; retrying"
        if out_of_time; then
            echo "$0: could not install $app within ${timeout}s" >&2
            exit 1
        fi
        sleep 5
    done
    say "installed $app"
fi

attempt=0
while :; do
    attempt=$((attempt + 1))
    if xcrun simctl launch "$udid" "$bundle" >/dev/null 2>&1; then
        say "launch attempt $attempt succeeded"
        xcrun simctl terminate "$udid" "$bundle" >/dev/null 2>&1
        break
    fi
    say "launch attempt $attempt failed; retrying"
    if out_of_time; then
        echo "$0: the app did not launch on $name within ${timeout}s" >&2
        exit 1
    fi
    sleep 5
done
say "$name is ready"
