#!/bin/bash
set -euo pipefail

# Build NereusSDR macOS installer (.pkg)
# Usage: ./packaging/macos/hal-installer.sh [build-dir]
#
# Bundles NereusSDR.app + the NereusSDRVAX HAL plugin into a single
# productbuild .pkg. The HAL component's postinstall script restarts
# coreaudiod with a macOS 14.4+ killall fallback (see hal-postinstall.sh).
# Ported from AetherSDR's packaging/macos/build-installer.sh.

BUILD_DIR="${1:-build}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PKG_DIR="${BUILD_DIR}/pkg-staging"
VERSION=$(grep 'project(NereusSDR' CMakeLists.txt | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || echo "0.0.0")

echo "=== Building NereusSDR macOS installer v${VERSION} ==="

# 1. Build app — skip if already built (preserves pre-existing signatures from CI).
if [ ! -d "${BUILD_DIR}/NereusSDR.app" ]; then
    cmake -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build "${BUILD_DIR}" -j$(sysctl -n hw.ncpu)
else
    echo "--- Reusing existing ${BUILD_DIR}/NereusSDR.app ---"
fi

# 1b. Build HAL plugin — skip if already built (preserves pre-existing signatures from CI).
HAL_BUILD="${BUILD_DIR}-hal"
if [ -f "hal-plugin/CMakeLists.txt" ]; then
    if [ ! -d "${HAL_BUILD}/NereusSDRVAX.driver" ]; then
        echo "--- Building HAL plugin ---"
        cmake -B "${HAL_BUILD}" -S hal-plugin -DCMAKE_BUILD_TYPE=RelWithDebInfo
        cmake --build "${HAL_BUILD}" -j$(sysctl -n hw.ncpu)
    else
        echo "--- Reusing existing ${HAL_BUILD}/NereusSDRVAX.driver ---"
    fi
fi

# 2. Prepare staging
rm -rf "${PKG_DIR}"
mkdir -p "${PKG_DIR}/app" "${PKG_DIR}/hal" "${PKG_DIR}/scripts"

# Copy app bundle
cp -R "${BUILD_DIR}/NereusSDR.app" "${PKG_DIR}/app/"

# Copy HAL plugin (check both possible build locations)
if [ -d "${HAL_BUILD}/NereusSDRVAX.driver" ]; then
    cp -R "${HAL_BUILD}/NereusSDRVAX.driver" "${PKG_DIR}/hal/"
elif [ -d "${BUILD_DIR}/NereusSDRVAX.driver" ]; then
    cp -R "${BUILD_DIR}/NereusSDRVAX.driver" "${PKG_DIR}/hal/"
else
    echo "ERROR: HAL plugin not found at ${HAL_BUILD}/NereusSDRVAX.driver" >&2
    echo "The HAL plugin must be built before packaging. Check that hal-plugin/ exists and builds cleanly." >&2
    exit 1
fi

# 3. Install postinstall script (standalone file; see hal-postinstall.sh)
cp "${SCRIPT_DIR}/hal-postinstall.sh" "${PKG_DIR}/scripts/postinstall"
chmod +x "${PKG_DIR}/scripts/postinstall"

# 4. Build component packages
pkgbuild \
    --root "${PKG_DIR}/app" \
    --install-location /Applications \
    --identifier com.nereussdr.app \
    --version "${VERSION}" \
    "${PKG_DIR}/NereusSDR-app.pkg"

if [ -d "${PKG_DIR}/hal/NereusSDRVAX.driver" ]; then
    pkgbuild \
        --root "${PKG_DIR}/hal" \
        --install-location "/Library/Audio/Plug-Ins/HAL" \
        --identifier com.nereussdr.vax \
        --version "${VERSION}" \
        --scripts "${PKG_DIR}/scripts" \
        "${PKG_DIR}/NereusSDR-vax.pkg"
fi

# 5. Create distribution XML
cat > "${PKG_DIR}/Distribution.xml" << DISTXML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>NereusSDR ${VERSION}</title>
    <welcome file="welcome.html"/>
    <options customize="allow" require-scripts="false"/>

    <script>
    function vaxDriverInstalled() {
        return system.files.fileExistsAtPath('/Library/Audio/Plug-Ins/HAL/NereusSDRVAX.driver');
    }
    function vaxDriverNeedsUpdate() {
        // Always offer update if installed version differs
        var plist = system.files.plistAtPath('/Library/Audio/Plug-Ins/HAL/NereusSDRVAX.driver/Contents/Info.plist');
        if (plist) {
            var installed = plist['CFBundleShortVersionString'] || '0';
            return (installed !== '${VERSION}');
        }
        return true;
    }
    </script>

    <choices-outline>
        <line choice="app"/>
        <line choice="vax"/>
    </choices-outline>

    <choice id="app" title="NereusSDR Application"
            description="OpenHPSDR / Apache Labs SDR client (Thetis port).">
        <pkg-ref id="com.nereussdr.app"/>
    </choice>

    <choice id="vax" title="VAX Virtual Audio Driver"
            description="Virtual audio devices for digital mode apps (WSJT-X, fldigi, etc.). Uncheck if already installed."
            selected="!vaxDriverInstalled() || vaxDriverNeedsUpdate()"
            tooltip="Currently installed: will skip unless a newer version is available">
        <pkg-ref id="com.nereussdr.vax"/>
    </choice>

    <pkg-ref id="com.nereussdr.app" version="${VERSION}" onConclusion="none">NereusSDR-app.pkg</pkg-ref>
    <pkg-ref id="com.nereussdr.vax" version="${VERSION}" onConclusion="none">NereusSDR-vax.pkg</pkg-ref>
</installer-gui-script>
DISTXML

# 6. Create welcome HTML
mkdir -p "${PKG_DIR}/resources"
cat > "${PKG_DIR}/resources/welcome.html" << 'WELCOME'
<html><body>
<h1>NereusSDR</h1>
<p>A native OpenHPSDR / Apache Labs SDR client for macOS.</p>
<p>This installer includes:</p>
<ul>
<li><b>NereusSDR.app</b> &mdash; the main application</li>
<li><b>VAX Virtual Audio Driver</b> &mdash; creates virtual audio devices for digital mode apps (WSJT-X, fldigi, VARA, etc.)</li>
</ul>
<p>After installation, the CoreAudio daemon will restart automatically to register the new audio devices.</p>
</body></html>
WELCOME

# 7. Build product archive
productbuild \
    --distribution "${PKG_DIR}/Distribution.xml" \
    --resources "${PKG_DIR}/resources" \
    --package-path "${PKG_DIR}" \
    "${BUILD_DIR}/NereusSDR-${VERSION}-macOS.pkg"

echo ""
echo "=== Installer created: ${BUILD_DIR}/NereusSDR-${VERSION}-macOS.pkg ==="
