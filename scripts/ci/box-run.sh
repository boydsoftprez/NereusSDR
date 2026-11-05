#!/usr/bin/env bash
# Run one step of a Linux CI job on a self-hosted runner, inside the CI image.
#
# The self-hosted runners (label nereus-linux) share their machine with other
# work, so nothing is installed on it and no build runs on it directly: each
# step runs here in a throwaway container of .github/docker/Dockerfile's
# image. CI speed round, 2026-10-16.
#
#   scripts/ci/box-run.sh image          build the image if it is missing;
#                                        writes NEREUS_BOX_IMAGE to $GITHUB_ENV
#   scripts/ci/box-run.sh run '<script>' run a bash script in the container
#   scripts/ci/box-run.sh cleanup        remove this job's containers
#
# What the container sees:
#   /src     the job's checkout ($GITHUB_WORKSPACE). A fixed path, so the
#            compiler cache matches across the runners, whose work folders
#            differ.
#   /ccache  the runners' shared compiler cache ($NEREUS_BOX_CCACHE).
#   /box     this runner's own folder for the dependency downloads and
#            ctest's measured test times, kept between runs
#            ($RUNNER_WORKSPACE/.nereus-box).
# It runs as the runner's user, so every file it writes belongs to the
# runner and the next checkout can clean it. HOME is /src/.box-home, inside
# the checkout. Limits keep a job to its share of the machine: 10 processors
# and 16 GB of memory. With NEREUS_BOX_TMP_IN_MEMORY=1, which ci.yml sets on
# the Test step only, /tmp is memory (--tmpfs): the test suite's scratch
# settings and stores are saved with a disk sync, and a busy disk on this
# shared machine made those saves slow enough to time out 49 tests (run
# 41764912150; scripts/ci/box-linux.sh, test step). A sync on tmpfs costs
# nothing. It counts against the 16 GB only for what is written.
set -euo pipefail

: "${GITHUB_WORKSPACE:?}" "${RUNNER_WORKSPACE:?}" "${GITHUB_RUN_ID:?}" "${RUNNER_NAME:?}"

# Labels this job's containers so cleanup finds them after a cancel.
owner="${GITHUB_RUN_ID}-${GITHUB_RUN_ATTEMPT:-1}-${RUNNER_NAME}"
box_dir="${RUNNER_WORKSPACE}/.nereus-box"

case "${1:-}" in
    image)
        # Named by the Dockerfile's hash: a pull request that changes it
        # gets its own image, and main's stays in place for other jobs.
        hash="$(sha256sum .github/docker/Dockerfile | cut -c1-16)"
        image="nereus-ci:${hash}"
        if docker image inspect "$image" >/dev/null 2>&1; then
            echo "Using $image, already on this machine."
        else
            echo "Building $image from .github/docker/Dockerfile."
            # Jobs that start together on a new Dockerfile all build it,
            # and every one but the first to finish fails with "image ...
            # already exists" (run 39027844137). An image that exists by
            # then is the same build, so that job uses it.
            if ! docker build --pull --label org.nereussdr.ci=1 -t "$image" .github/docker; then
                if docker image inspect "$image" >/dev/null 2>&1; then
                    echo "Another job built $image meanwhile; using it."
                else
                    exit 1
                fi
            fi
        fi
        echo "NEREUS_BOX_IMAGE=$image" >> "$GITHUB_ENV"
        # Drop images of older Dockerfiles that no container uses; one in
        # use by another job stays (docker refuses, and that is fine).
        docker image ls nereus-ci --format '{{.Repository}}:{{.Tag}}' \
            | { grep -v -x -F "$image" || true; } \
            | while read -r old; do
                  docker image rm "$old" >/dev/null 2>&1 && echo "Removed $old." || true
              done
        ;;

    run)
        : "${NEREUS_BOX_IMAGE:?run the image step first}" "${NEREUS_BOX_CCACHE:?}"
        if [ ! -d "$NEREUS_BOX_CCACHE" ] || [ ! -w "$NEREUS_BOX_CCACHE" ]; then
            echo "::error::The shared compiler cache folder $NEREUS_BOX_CCACHE is missing or not writable on $RUNNER_NAME."
            exit 1
        fi
        mkdir -p "$box_dir" "$GITHUB_WORKSPACE/.box-home"
        tmp_in_memory=()
        if [ "${NEREUS_BOX_TMP_IN_MEMORY:-}" = "1" ]; then
            tmp_in_memory=(--tmpfs /tmp:rw,exec,size=6g,mode=1777)
        fi
        # IPv6 is on for the container's loopback: tests listen on ::1.
        exec docker run --rm --init \
            --label "org.nereussdr.ci.owner=$owner" \
            --user "$(id -u):$(id -g)" \
            --cpus 10 --memory 16g --shm-size 2g \
            "${tmp_in_memory[@]}" \
            --sysctl net.ipv6.conf.all.disable_ipv6=0 \
            --sysctl net.ipv6.conf.lo.disable_ipv6=0 \
            -v "$GITHUB_WORKSPACE:/src" \
            -v "$NEREUS_BOX_CCACHE:/ccache" \
            -v "$box_dir:/box" \
            -w /src \
            -e HOME=/src/.box-home \
            -e CCACHE_DIR=/ccache \
            -e CCACHE_MAXSIZE=20G \
            -e CCACHE_SLOPPINESS=pch_defines,time_macros,include_file_mtime,include_file_ctime \
            -e CCACHE_NOHASHDIR=1 \
            -e CCACHE_COMPILERCHECK=content \
            -e CCACHE_STATSLOG=/src/.box-home/ccache-stats.log \
            -e NEREUS_DEPENDENCY_ARCHIVE_DIR=/box/deps \
            -e GITHUB_ACTIONS \
            -e CI \
            "$NEREUS_BOX_IMAGE" \
            bash -euo pipefail -c "${2:?no script given}"
        ;;

    cleanup)
        ids="$(docker ps -aq --filter "label=org.nereussdr.ci.owner=$owner")"
        if [ -n "$ids" ]; then
            echo "Removing containers left by a cancelled step: $ids"
            # shellcheck disable=SC2086
            docker rm -f $ids >/dev/null
        fi
        ;;

    *)
        echo "usage: $0 image | run '<script>' | cleanup" >&2
        exit 2
        ;;
esac
