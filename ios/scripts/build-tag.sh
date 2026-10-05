#!/bin/sh
# NereusSDR for iOS: prints the test-build tag for the working tree it runs in
# SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#
# Usage: ios/scripts/build-tag.sh
#
# The same rule as the desktop's cmake/NereusBuildTag.cmake, so a phone build
# and a desktop build made from one checkout carry one name:
#
#   * NEREUS_BUILD_TAG, when set and not empty, wins outright; a value of only
#     spaces or tabs turns the tag off.
#   * Otherwise nothing when HEAD sits exactly on a tag (a release build).
#   * Otherwise <branch>@<short sha>, or detached@<short sha> on a detached
#     head, with -dirty appended when a tracked file is modified. Untracked
#     files do not count.
#
# Prints the tag and a newline, or nothing at all when there is no tag. The
# tag is read from the git repository around the current directory.

set -u

tag="${NEREUS_BUILD_TAG:-}"

if [ -z "$tag" ] && command -v git >/dev/null 2>&1; then
    # Release gate: a HEAD exactly on a tag is a release build and gets no
    # name, dirty tree or not.
    if ! git describe --exact-match --tags HEAD >/dev/null 2>&1; then
        branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || true)
        sha=$(git rev-parse --short HEAD 2>/dev/null || true)

        # rev-parse --abbrev-ref prints the literal HEAD on a detached head.
        if [ "$branch" = "HEAD" ]; then
            branch="detached"
        fi

        if [ -n "$branch" ] && [ -n "$sha" ]; then
            tag="$branch@$sha"
        elif [ -n "$sha" ]; then
            tag="detached@$sha"
        fi

        if [ -n "$tag" ]; then
            changes=$(git status --porcelain --untracked-files=no 2>/dev/null || true)
            if [ -n "$changes" ]; then
                tag="$tag-dirty"
            fi
        fi
    fi
fi

# A value of only spaces and tabs means no tag.
if [ -n "$(printf '%s' "$tag" | tr -d ' \t')" ]; then
    printf '%s\n' "$tag"
fi
exit 0
