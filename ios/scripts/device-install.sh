#!/bin/sh
# NereusSDR for iOS: builds the app for a connected iPhone, installs it and launches it
# SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#
# Usage: ios/scripts/device-install.sh [device]
#
# device is the iPhone's name or UDID, as `xcrun devicectl list devices`
# shows it. Without one, the only iPhone connected to this Mac is used.
#
# The build names itself: the tag from build-tag.sh, derived at the moment
# of the build, goes in as the NEREUS_BUILD_TAG build setting and shows at
# the foot of Setup. Its number, from build-number.sh (the commit count of
# HEAD), goes in as CURRENT_PROJECT_VERSION, so About's "build N" rises with
# every commit; without git, project.yml's number stands. Signing is automatic, with the team in
# ios/Config/Team.xcconfig. When signing or provisioning fails, this stops
# with Xcode's own error; it never changes the team, the entitlements or the
# bundle identifiers to get past one.
#
# This touches a real device. Only the controller or JJ runs it.

set -u

here=$(cd "$(dirname "$0")" && pwd)
ios_dir=$(cd "$here/.." && pwd)

bundle_id="com.boydsoftprez.NereusSDR.ios"
derived="$ios_dir/.build/device"
app="$derived/Build/Products/Debug-iphoneos/NereusSDR.app"
wanted="${1:-}"

fail() {
    echo "$0: $*" >&2
    exit 1
}

command -v xcrun >/dev/null 2>&1 || fail "Xcode's command line tools are not installed."
command -v python3 >/dev/null 2>&1 || fail "python3 is needed to read the device list."

work=$(mktemp -d "${TMPDIR:-/tmp}/nereus-device.XXXXXX") || fail "could not make a temporary directory."
trap 'rm -rf "$work"' EXIT

# 1. Find the iPhone. Simulators are listed too and are skipped; a device
#    counts as connected when it has a transport (cable or network) now.
xcrun devicectl list devices --quiet --json-output "$work/devices.json" \
    || fail "could not list the devices connected to this Mac."

found=$(python3 - "$work/devices.json" "$wanted" <<'EOF'
import json
import sys

path, wanted = sys.argv[1], sys.argv[2]
with open(path, encoding="utf-8") as f:
    devices = json.load(f).get("result", {}).get("devices", [])

connected = []
for device in devices:
    props = device.get("properties") or {}
    hardware = props.get("hardware") or device.get("hardwareProperties") or {}
    connection = props.get("connection") or device.get("connectionProperties") or {}
    state = props.get("state") or {}
    name = state.get("name") or (device.get("deviceProperties") or {}).get("name") or ""
    udid = hardware.get("udid") or ""
    if hardware.get("reality") != "physical" or hardware.get("platform") != "iOS":
        continue
    if connection.get("transportType") not in ("wired", "localNetwork"):
        continue
    if not udid:
        continue
    connected.append((name, udid, device.get("identifier") or ""))

if wanted:
    chosen = [d for d in connected
              if wanted.lower() == d[0].lower() or wanted in (d[1], d[2])]
else:
    chosen = connected

if len(chosen) == 1:
    print(chosen[0][1] + "\t" + chosen[0][0])
    sys.exit(0)
if not chosen:
    sys.exit(2)
for name, udid, _ in chosen:
    print(name + " (" + udid + ")", file=sys.stderr)
sys.exit(3)
EOF
)
status=$?
case $status in
    0) ;;
    2)
        if [ -n "$wanted" ]; then
            fail "no connected iPhone is called \"$wanted\". Connect it with a cable, unlock it and trust this Mac, then run this again."
        fi
        fail "no iPhone is connected. Connect your iPhone to this Mac with a cable, unlock it and trust this Mac, then run this again."
        ;;
    3) fail "more than one iPhone is connected (listed above). Name the one to use: $0 <name or UDID>" ;;
    *) fail "could not read the device list." ;;
esac

udid=$(printf '%s' "$found" | cut -f1)
name=$(printf '%s' "$found" | cut -f2-)
echo "Installing on $name ($udid)"

# 2. Regenerate the project, so the build matches project.yml.
"$here/generate-project.sh" || fail "could not generate the Xcode project."

# 3. Name the build now, so the name is never older than the build.
tag=$(cd "$ios_dir" && "$here/build-tag.sh")
if [ -n "$tag" ]; then
    echo "Build $tag"
else
    echo "This build carries no name (HEAD is on a release tag, or the tag is turned off)."
fi

# 3a. Number the build now too. With no number, project.yml's stands.
number=$(cd "$ios_dir" && "$here/build-number.sh")
if [ -n "$number" ]; then
    echo "Build number $number"
    set -- CURRENT_PROJECT_VERSION="$number"
else
    echo "This build keeps project.yml's build number (no git repository found)."
    set --
fi

# 4. Build for the device with automatic signing. Xcode's error, if any, is
#    printed as it is and ends the run.
xcodebuild \
    -project "$ios_dir/NereusSDR.xcodeproj" \
    -scheme NereusSDR \
    -configuration Debug \
    -destination "id=$udid" \
    -derivedDataPath "$derived" \
    -allowProvisioningUpdates \
    -allowProvisioningDeviceRegistration \
    NEREUS_BUILD_TAG="$tag" \
    "$@" \
    build || fail "the build for $name failed; Xcode's error is above."

[ -d "$app" ] || fail "the build finished but $app is missing."

# 5. Install and launch.
xcrun devicectl device install app --device "$udid" "$app" \
    || fail "could not install the app on $name; the error is above."
xcrun devicectl device process launch --device "$udid" --terminate-existing "$bundle_id" \
    || fail "the app is installed on $name but did not launch; the error is above."

echo "NereusSDR is running on $name."
