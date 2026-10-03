#!/bin/bash -e
set -o pipefail
: "${NEREUS_STATION_SOURCE_DIR:?Set source directory to packaging/station-image}"
: "${NEREUS_DEB:?Set path to Debian trixie arm64 nereusd package}"
test -f "$NEREUS_DEB"
test -d "$NEREUS_STATION_SOURCE_DIR/common"
install -d -m 0755 "${ROOTFS_DIR}/tmp/nereus-install"
install -m 0644 "$NEREUS_DEB" "${ROOTFS_DIR}/tmp/nereus-install/nereusd.deb"
install -m 0755 "$NEREUS_STATION_SOURCE_DIR/common/install-station.sh" \
    "${ROOTFS_DIR}/tmp/nereus-install/install-station.sh"
install -m 0755 "$NEREUS_STATION_SOURCE_DIR/common/nereus-firstboot.sh" \
    "${ROOTFS_DIR}/tmp/nereus-install/nereus-firstboot.sh"
install -m 0644 "$NEREUS_STATION_SOURCE_DIR/common/nereus-firstboot.service" \
    "${ROOTFS_DIR}/tmp/nereus-install/nereus-firstboot.service"
install -m 0644 "$NEREUS_STATION_SOURCE_DIR/common/nereusd-firstboot.conf" \
    "${ROOTFS_DIR}/tmp/nereus-install/nereusd-firstboot.conf"
install -m 0644 "$NEREUS_STATION_SOURCE_DIR/common/nereusd-serial.conf" \
    "${ROOTFS_DIR}/tmp/nereus-install/nereusd-serial.conf"
on_chroot <<'EOF'
/bin/bash /tmp/nereus-install/install-station.sh
EOF
rm -rf "${ROOTFS_DIR}/tmp/nereus-install"
