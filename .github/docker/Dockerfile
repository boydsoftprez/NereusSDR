# CI image for NereusSDR's Linux build and test jobs on the self-hosted
# runners (label nereus-linux). CI speed round, 2026-10-16.
#
# The runners share their machine with other work, so nothing is installed
# on it: each Linux job runs its configure, build and tests in this image
# (scripts/ci/box-run.sh). The job builds the image on the runner from this
# file when no image with this file's hash is there yet, so a change here
# takes effect in the pull request that makes it.
# .github/workflows/docker-ci-image.yml also publishes it to
# ghcr.io/boydsoftprez/nereussdr-ci on pushes to main.
#
# The contents match what the GitHub-hosted Linux jobs in
# .github/workflows/ci.yml install, so the suite runs against the same
# libraries either way:
#   - the packages of "Install system dependencies (Linux)";
#   - the packages jurplel/install-qt-action adds on Linux;
#   - the build tools the ubuntu-24.04 runner image has and the vendored
#     Opus build needs (autoconf, automake, libtool, wget, git, make);
#   - fonts, so text renders as on the hosted runner;
#   - Qt 6.8.3 from aqtinstall 3.3, the version and tool install-qt-action
#     resolves '6.8.*' to on the hosted jobs, with the same modules.

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl wget git make file unzip xz-utils patch \
        cmake ninja-build pkg-config g++ mold ccache \
        autoconf automake libtool \
        libgl1-mesa-dev libfftw3-dev \
        libasound2-dev libudev-dev libjack-jackd2-dev \
        libpipewire-0.3-dev libpulse-dev \
        libssl-dev openssl \
        python3 python3-venv python3-websockets python3-cryptography \
        gdb \
        libhamlib-utils \
        iputils-ping \
        xvfb xauth \
        libxkbcommon-dev libxkbcommon-x11-0 libxcb-cursor0 libxcb-xkb-dev \
        libxcb1 libxcb-glx0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
        libxcb-randr0 libxcb-render0 libxcb-render-util0 libxcb-shape0 \
        libxcb-shm0 libxcb-sync1 libxcb-util1 libxcb-xfixes0 \
        libxcb-xinerama0 libxcb-xkb1 \
        libgstreamer-gl1.0-0 libegl1 libfontconfig1 libdbus-1-3 \
        fonts-dejavu-core fonts-dejavu-extra fonts-liberation fonts-liberation2 \
    && rm -rf /var/lib/apt/lists/*

# Up to three tries: a Qt download mirror can time out (it did on the
# first local build of this image, 2026-10-16).
ARG QT_VERSION=6.8.3
RUN python3 -m venv /tmp/aqt \
    && /tmp/aqt/bin/pip install --no-cache-dir 'aqtinstall==3.3.*' \
    && for try in 1 2 3; do \
           /tmp/aqt/bin/aqt install-qt linux desktop "${QT_VERSION}" linux_gcc_64 \
               -m qtmultimedia qtshadertools qtwebsockets qtserialport \
               -O /opt/Qt \
           && break; \
           if [ "$try" = 3 ]; then exit 1; fi; \
           echo "Qt download failed (try $try of 3); trying again in 30 s."; \
           rm -rf /opt/Qt; sleep 30; \
       done \
    && test -f "/opt/Qt/${QT_VERSION}/gcc_64/lib/cmake/Qt6/Qt6Config.cmake" \
    && rm -rf /tmp/aqt

# The variables install-qt-action sets for the hosted jobs.
ENV QT_ROOT_DIR=/opt/Qt/${QT_VERSION}/gcc_64 \
    Qt6_DIR=/opt/Qt/${QT_VERSION}/gcc_64/lib/cmake/Qt6 \
    CMAKE_PREFIX_PATH=/opt/Qt/${QT_VERSION}/gcc_64 \
    QT_PLUGIN_PATH=/opt/Qt/${QT_VERSION}/gcc_64/plugins \
    QML2_IMPORT_PATH=/opt/Qt/${QT_VERSION}/gcc_64/qml \
    PKG_CONFIG_PATH=/opt/Qt/${QT_VERSION}/gcc_64/lib/pkgconfig \
    LD_LIBRARY_PATH=/opt/Qt/${QT_VERSION}/gcc_64/lib \
    PATH=/opt/Qt/${QT_VERSION}/gcc_64/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
