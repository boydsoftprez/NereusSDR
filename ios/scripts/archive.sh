#!/bin/sh
# NereusSDR for iOS: makes the Release archive that goes to TestFlight and the App Store
# SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#
# Usage: ios/scripts/archive.sh
#
# Archives the NereusSDR scheme in its Release configuration for any iOS
# device, signed automatically with the team in ios/Config/Team.xcconfig.
# The archive carries:
#
#   * its build number, CURRENT_PROJECT_VERSION, from build-number.sh: the
#     commit count of HEAD (D132), so every archive from a newer commit
#     carries a larger number, as TestFlight asks. An archive needs one, so
#     this stops when there is no git repository to count;
#   * its version, MARKETING_VERSION, as project.yml sets it;
#   * its name, NEREUS_BUILD_TAG, from build-tag.sh, the same rule as the
#     device install and the desktop: none on a release tag, otherwise
#     <branch>@<short sha of HEAD>.
#
# An archive is built from a commit and nothing else, so its name always
# names the commit it was built from:
#
#   * it refuses a tree with uncommitted changes to tracked files, staged
#     or not (untracked files do not count, as in build-tag.sh);
#   * it refuses a name that does not end in @<short sha of HEAD>, such as
#     a NEREUS_BUILD_TAG set by hand, and a name turned off anywhere but on
#     a release tag.
#
# The archive is written to ios/.build/archive/NereusSDR <version> (<build>).xcarchive,
# with its derived data in ios/.build/archive-dd. Xcode's Organizer, or
# xcodebuild -exportArchive, uploads it. Then the archived app is checked
# for its privacy manifest, PrivacyInfo.xcprivacy, without which App Store
# Connect refuses the upload (ITMS-91053); this stops if it is missing.
#
# TestFlight archives are made from phone main only (D132). This signs with
# the team's certificate: only the controller or JJ runs it. When signing
# or provisioning fails, this stops with Xcode's own error; it never
# changes the team, the entitlements or the bundle identifiers to get past
# one.

set -u

here=$(cd "$(dirname "$0")" && pwd)
ios_dir=$(cd "$here/.." && pwd)

fail() {
    echo "$0: $*" >&2
    exit 1
}

command -v xcodebuild >/dev/null 2>&1 || fail "Xcode is not installed."

# 1. Number the archive. Without a number it would carry project.yml's
#    placeholder, which TestFlight would refuse after the first upload.
number=$(cd "$ios_dir" && "$here/build-number.sh")
[ -n "$number" ] || fail "no build number: make the archive from a git checkout (build-number.sh found no commits)."

# 2. Only a commit is archived: no uncommitted changes to tracked files.
changes=$(cd "$ios_dir" && git status --porcelain --untracked-files=no) \
    || fail "could not read the state of the working tree."
[ -z "$changes" ] || fail "the working tree has uncommitted changes to tracked files; commit them first:
$changes"
head_sha=$(cd "$ios_dir" && git rev-parse --short HEAD) || fail "could not read HEAD."

# 3. The version, as project.yml sets it.
version=$(sed -n 's/^[[:space:]]*MARKETING_VERSION:[[:space:]]*"\{0,1\}\([0-9][0-9.]*\)"\{0,1\}[[:space:]]*$/\1/p' \
    "$ios_dir/project.yml" | head -n 1)
[ -n "$version" ] || fail "could not read MARKETING_VERSION from ios/project.yml."

# 4. Name the archive now, so the name is never older than the build. The
#    name must be HEAD's: <branch>@<sha>, or none only on a release tag.
tag=$(cd "$ios_dir" && "$here/build-tag.sh")
if [ -n "$tag" ]; then
    case "$tag" in
        *"@$head_sha") ;;
        *) fail "the build name '$tag' does not name HEAD ($head_sha); unset NEREUS_BUILD_TAG." ;;
    esac
elif ! (cd "$ios_dir" && git describe --exact-match --tags HEAD >/dev/null 2>&1); then
    fail "the build name is turned off, so it does not name HEAD ($head_sha), which is not on a release tag; unset NEREUS_BUILD_TAG."
fi
if [ -n "$tag" ]; then
    echo "Archive $version ($number), build $tag"
else
    echo "Archive $version ($number), with no build name (HEAD is on a release tag, or the tag is turned off)"
fi

# 5. Regenerate the project, so the archive matches project.yml.
"$here/generate-project.sh" || fail "could not generate the Xcode project."

archive="$ios_dir/.build/archive/NereusSDR $version ($number).xcarchive"

# 6. Archive. Xcode's error, if any, is printed as it is and ends the run.
xcodebuild \
    -project "$ios_dir/NereusSDR.xcodeproj" \
    -scheme NereusSDR \
    -configuration Release \
    -jobs 2 \
    -destination generic/platform=iOS \
    -derivedDataPath "$ios_dir/.build/archive-dd" \
    -archivePath "$archive" \
    -allowProvisioningUpdates \
    NEREUS_BUILD_TAG="$tag" \
    MARKETING_VERSION="$version" \
    CURRENT_PROJECT_VERSION="$number" \
    archive || fail "the archive failed; Xcode's error is above."

# 7. The archived app must carry its privacy manifest.
app="$archive/Products/Applications/NereusSDR.app"
[ -d "$app" ] || fail "the archive finished but $app is missing."
[ -f "$app/PrivacyInfo.xcprivacy" ] \
    || fail "the archived app has no PrivacyInfo.xcprivacy; App Store Connect would refuse it."

echo "Archived NereusSDR $version ($number): $archive"
