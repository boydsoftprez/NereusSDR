#!/bin/bash
set -euo pipefail
# Armbian executes this in the target rootfs chroot. Prepare
# userpatches/overlay/nereus-station with common/* and nereusd.deb.
: "${1:?Armbian RELEASE required}"
: "${3:?Armbian BOARD required}"
test "$1" = trixie && test "$3" = rock-5c || {
    echo 'This customization supports only Debian trixie on Rock 5C' >&2
    exit 1
}
assets=/tmp/overlay/nereus-station
test -f "$assets/nereusd.deb"
NEREUS_STAGING_DIR="$assets" /bin/bash "$assets/install-station.sh"
