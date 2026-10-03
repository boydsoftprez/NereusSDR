# CI Docker image for NereusSDR
# Build: docker build -t ghcr.io/boydsoftprez/nereussdr-ci:latest .github/docker/
# This will be populated when build times warrant a custom container.

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    cmake ninja-build pkg-config g++ \
    qt6-base-dev qt6-base-private-dev qt6-multimedia-dev \
    qt6-shadertools-dev \
    libgl1-mesa-dev libfftw3-dev \
    libssl-dev \
    git \
    && rm -rf /var/lib/apt/lists/*
