#!/bin/bash
set -euo pipefail

# Run only inside the target arm64 Debian trixie rootfs. The .deb is built in
# the same distro, from the same source revision, before image construction.
assets=${NEREUS_STAGING_DIR:-/tmp/nereus-install}
deb="$assets/nereusd.deb"
test -f "$deb"
# shellcheck source=/dev/null
. /etc/os-release
test "${ID:-}" = debian && test "${VERSION_CODENAME:-}" = trixie || {
    echo 'Station image requires a Debian trixie rootfs' >&2
    exit 1
}
test "$(dpkg --print-architecture)" = arm64
test "$(dpkg-deb --field "$deb" Package)" = nereusd
test "$(dpkg-deb --field "$deb" Architecture)" = arm64

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends "$deb" avahi-daemon
test -x /usr/bin/nereusd
test -f /usr/lib/systemd/system/nereusd.service -o -f /lib/systemd/system/nereusd.service
grep -q '^DynamicUser=yes$' /usr/lib/systemd/system/nereusd.service 2>/dev/null || \
    grep -q '^DynamicUser=yes$' /lib/systemd/system/nereusd.service

# Only non-sensitive defaults are shipped. The daemon creates its identity in
# its private StateDirectory on the target at runtime.
install -m 0644 /usr/share/nereusd/nereusd.conf.sample /etc/nereusd.conf
install -Dm 0755 "$assets/nereus-firstboot.sh" /usr/local/libexec/nereus-firstboot
install -Dm 0644 "$assets/nereus-firstboot.service" /etc/systemd/system/nereus-firstboot.service
install -Dm 0644 "$assets/nereusd-firstboot.conf" \
    /etc/systemd/system/nereusd.service.d/firstboot.conf
install -Dm 0644 "$assets/nereusd-serial.conf" \
    /etc/systemd/system/nereusd.service.d/serial.conf
install -d -m 0755 /var/lib/nereus-station
touch /var/lib/nereus-station/first-boot-pending
systemctl enable nereus-firstboot.service nereusd.service avahi-daemon.service

# The package cache and staging files must not become part of the image.
apt-get clean
rm -f /var/cache/apt/archives/nereusd_*.deb
