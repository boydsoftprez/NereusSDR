#!/usr/bin/env bash
# The steps of a Linux CI job on the self-hosted runners, run inside the CI
# image by scripts/ci/box-run.sh (the checkout is /src, the runner's own
# folder /box). CI speed round, 2026-10-16; moved into this script,
# 2026-10-17.
#
#   scripts/ci/box-linux.sh configure   fetch the dependencies and configure
#   scripts/ci/box-linux.sh build       build the app and $TARGET
#   scripts/ci/box-linux.sh test        run this job's tests (not the
#                                       realtime tests: linux-realtime in
#                                       ci.yml runs those on an idle runner)
#   scripts/ci/box-linux.sh times       write this job's test times to
#                                       box-test-times-$SHARD.txt
#   scripts/ci/box-linux.sh stats       this job's compiler cache hits
#
# From the environment: SHARDS and SHARD (the test shards; SHARDS=0 for
# none) and TARGET (the test target to build, all_tests). The tests go to
# shards by name hash, as on the hosted legs, unless the checkout holds a
# .box-test-shards.txt from the workflow (scripts/ci/test-split.py).
set -euo pipefail

: "${SHARDS:?}" "${SHARD:?}" "${TARGET:?}"
# ctest's measured test times for this job's tests, kept between runs.
cost_data="/box/CTestCostData-$SHARDS-$SHARD-$TARGET.txt"

case "${1:-}" in
    configure)
        # The dependency downloads stay in /box/deps between runs, so only
        # a changed pin downloads anything.
        mirrors="$HOME/nereus-git-mirrors.gitconfig"
        cmake -DNEREUS_DEPENDENCY_ARCHIVE_DIR="$NEREUS_DEPENDENCY_ARCHIVE_DIR" \
              -DNEREUS_GIT_MIRROR_CONFIG="$mirrors" \
              -P cmake/FetchDependencyArchives.cmake
        git config --global --add include.path "$mirrors"
        # The hosted Linux legs' configure (Configure (Linux) in ci.yml),
        # with this job's shards: by measured time when the changes job
        # split them (the workflow writes its split to this file), by name
        # hash otherwise.
        shard_file=()
        if [ -s .box-test-shards.txt ]; then
            shard_file=(-DNEREUS_TEST_SHARD_FILE=/src/.box-test-shards.txt)
        fi
        cmake -B build -G Ninja \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            "-DCMAKE_C_FLAGS_RELWITHDEBINFO=-O2 -g1 -DNDEBUG" \
            "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O2 -g1 -DNDEBUG" \
            -DCMAKE_BUILD_RPATH_USE_ORIGIN=ON \
            -DNEREUS_TEST_PYTHON=/usr/bin/python3 \
            -DCMAKE_C_COMPILER_LAUNCHER=ccache \
            -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
            -DCMAKE_EXE_LINKER_FLAGS_INIT=-fuse-ld=mold \
            -DCMAKE_SHARED_LINKER_FLAGS_INIT=-fuse-ld=mold \
            -DCMAKE_MODULE_LINKER_FLAGS_INIT=-fuse-ld=mold \
            -DNEREUS_BUILD_TESTS=ON \
            -DNEREUS_TEST_SHARDS="$SHARDS" \
            -DNEREUS_TEST_SHARD="$SHARD" \
            "${shard_file[@]}"
        # ctest starts the longest tests first from these times.
        mkdir -p build/Testing/Temporary
        cp "$cost_data" build/Testing/Temporary/CTestCostData.txt 2>/dev/null || true
        ;;

    build)
        # `all` as well: the tests run nereusd, nereus_pairing_peer and
        # nereus_rendezvous_peer (see Build in ci.yml's build job).
        cmake --build build -j10 --target all "$TARGET"
        ;;

    test)
        # The hosted legs' Test step, first pass: the tests not labeled
        # realtime, 8 at a time.
        export QT_QPA_PLATFORM=xcb
        export QTEST_FUNCTION_TIMEOUT=240000
        export NEREUS_HAMLIB_RIGCTL=/usr/bin/rigctl
        # Each test's home (settings, stores) and QTemporaryDir scratch go in
        # /tmp, which is memory here (box-run.sh). A test saves settings with
        # a disk sync: tst_cat_coverage about 2 per row, tst_confirm_step 11
        # per function. Run 41764912150 ran 6 to 15 times slower on those
        # tests while tests that do not save kept their usual time.
        if [ ! -L build/tests/test-home ]; then
            cp -a build/tests/test-home /tmp/nereus-test-home
            rm -rf build/tests/test-home
            ln -s /tmp/nereus-test-home build/tests/test-home
        fi
        # Xvfb puts its socket here, and the memory /tmp hides anything the
        # image left there. The job runs as the runner's user, so Xvfb warns
        # that it is not owned by root, which does it no harm.
        mkdir -p -m 1777 /tmp/.X11-unix
        # The most /tmp held, sampled every 5 s, to check against its 6 GB cap.
        ( while sleep 5; do df -k --output=used /tmp | tail -1; done ) > /tmp/nereus-tmp-use.txt &
        sampler=$!
        warn_over_limit="$PWD/scripts/ci/warn-handover-over-limit.sh"
        cd build
        status=0
        xvfb-run -a ctest -LE realtime -j8 --output-on-failure --no-tests=error || status=1
        "$warn_over_limit" || true
        cp Testing/Temporary/CTestCostData.txt "$cost_data" 2>/dev/null || true
        kill "$sampler" 2>/dev/null || true
        peak_kb="$(sort -n /tmp/nereus-tmp-use.txt | tail -1)"
        echo "Peak use of /tmp during the tests: $(( ${peak_kb:-0} / 1024 )) MB"
        df -h /tmp | tail -1
        exit "$status"
        ;;

    times)
        # This job's measured time per test executable, for the next run's
        # split (scripts/ci/test-split.py).
        python3 scripts/ci/test-split.py times build > "box-test-times-$SHARD.txt"
        echo "Times for $(wc -l < "box-test-times-$SHARD.txt") test executables."
        ;;

    stats)
        # This job's hits and misses (the cache is shared, so its own
        # counters cover every job on the machine).
        ccache --show-log-stats -v
        echo
        ccache --show-stats
        ;;

    *)
        echo "usage: $0 configure | build | test | times | stats" >&2
        exit 2
        ;;
esac
