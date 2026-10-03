#!/bin/bash
set -euo pipefail
# Run in a native arm64 Debian trixie container from the workflow checkout.
cd /workspace
# shellcheck source=/dev/null
. /etc/os-release
test "${ID:-}" = debian && test "${VERSION_CODENAME:-}" = trixie
test "$(dpkg --print-architecture)" = arm64
test "$(uname -m)" = aarch64
: "${NEREUS_VERSION:?NEREUS_VERSION required}"

export DEBIAN_FRONTEND=noninteractive
export CFLAGS='-march=armv8-a' CXXFLAGS='-march=armv8-a'
apt-get update
apt-get install -y --no-install-recommends \
    ca-certificates cmake ninja-build pkg-config build-essential \
    qt6-base-dev qt6-multimedia-dev qt6-base-private-dev \
    qt6-shadertools-dev qt6-svg-dev qt6-websockets-dev \
    libfftw3-dev libssl-dev libasound2-dev libjack-jackd2-dev \
    libpipewire-0.3-dev libgl1-mesa-dev libxkbcommon-dev \
    autoconf automake libtool wget git python3

bash packaging/station-image/build-dfnr-source.sh
test -s third_party/deepfilter/lib/linux-aarch64/libdeepfilter.a
test -s third_party/deepfilter/models/DeepFilterNet3_onnx.tar.gz
test -s station-output/dfnr-provenance.json

# Pi 4 has a Cortex-A72 (ARMv8-A) CPU. Never inherit the runner's CPU level.
# DFNR's locked source build used generic AArch64; require CMake to use it.
cmake -S . -B /tmp/nereus-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr -DNEREUS_GPU_SPECTRUM=OFF \
    -DENABLE_DFNR=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_C_FLAGS='-march=armv8-a' \
    -DCMAKE_CXX_FLAGS='-march=armv8-a'
python3 - <<'PY'
import json
from pathlib import Path
commands = json.loads(Path('/tmp/nereus-build/compile_commands.json').read_text())
assert any('DeepFilterFilter.cpp' in item['file'] and '-DHAVE_DFNR' in item['command']
           for item in commands), 'DFNR silently disabled in CMake'
PY
cmake --build /tmp/nereus-build --target nereusd -j2
test -x /tmp/nereus-build/nereusd
DESTDIR=/tmp/nereus-debroot cmake --install /tmp/nereus-build --component nereusd
test -x /tmp/nereus-debroot/usr/bin/nereusd
test -s /tmp/nereus-debroot/usr/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz

core_lib=$(find /tmp/nereus-debroot/usr/lib -name libNereusCore.so -type f -print -quit)
rade_lib=$(find /tmp/nereus-debroot/usr/lib -name librade.so.0.1 -type f -print -quit)
test -n "$core_lib" && test -n "$rade_lib"
libdir=$(dirname "$core_lib")
mkdir -p /tmp/nereus-package-work/debian /tmp/nereus-debroot/DEBIAN /workspace/station-output
cp packaging/deb/source-control /tmp/nereus-package-work/debian/control
cd /tmp/nereus-package-work
deps=$(dpkg-shlibdeps -O --ignore-missing-info \
    -l"$libdir" -e/tmp/nereus-debroot/usr/bin/nereusd \
    -e"$core_lib" -e"$rade_lib" | sed -n 's/^shlibs:Depends=//p')
test -n "$deps"
cd /workspace
# No LD_LIBRARY_PATH: the staged binaries must find their own libraries
# through the RUNPATH they ship ($ORIGIN-relative), exactly as they will
# under systemd on the installed system.
: > /tmp/nereus-ldd.txt
for elf in /tmp/nereus-debroot/usr/bin/nereusd "$core_lib" "$rade_lib"; do
    env -u LD_LIBRARY_PATH ldd "$elf" >> /tmp/nereus-ldd.txt
done
if grep -q 'not found' /tmp/nereus-ldd.txt; then
    cat /tmp/nereus-ldd.txt >&2
    exit 1
fi
sed -e "s/@VERSION@/$NEREUS_VERSION/g" -e 's/@ARCH@/arm64/g' \
    -e "s/@DEPENDS@/$deps/g" packaging/deb/control.in \
    > /tmp/nereus-debroot/DEBIAN/control
install -m 0755 packaging/deb/postinst /tmp/nereus-debroot/DEBIAN/postinst
install -m 0755 packaging/deb/postrm /tmp/nereus-debroot/DEBIAN/postrm
dpkg-deb --build --root-owner-group /tmp/nereus-debroot \
    "/workspace/station-output/nereusd_${NEREUS_VERSION}_arm64_trixie.deb"
dpkg-deb --info "/workspace/station-output/nereusd_${NEREUS_VERSION}_arm64_trixie.deb"
