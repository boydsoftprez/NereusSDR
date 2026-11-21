#!/bin/bash
set -euo pipefail
: "${1:?package path required}"
: "${2:?expected public release version required}"
deb=$1
script_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
debian_version=$(python3 "$script_root/scripts/release-version.py" debian-version "$2")
test -f "$deb"
test "$(dpkg-deb --field "$deb" Package)" = nereusd
test "$(dpkg-deb --field "$deb" Architecture)" = arm64
test "$(dpkg-deb --field "$deb" Version)" = "$debian_version"
deps=$(dpkg-deb --field "$deb" Depends)
test -n "$deps"
case "$deps" in
    *libc6*libqt6*|*libqt6*libc6*) ;;
    *) echo 'Package lacks expected libc6/Qt6 dependencies' >&2; exit 1 ;;
esac
contents=$(dpkg-deb --contents "$deb")
printf '%s\n' "$contents" | grep -q 'usr/bin/nereusd$'
printf '%s\n' "$contents" | grep -q 'systemd/system/nereusd.service$'
# The Core opens a sound card only with the audio group (DynamicUser has none).
printf '%s\n' "$contents" | grep -q 'systemd/system/nereusd.service.d/audio.conf$'
printf '%s\n' "$contents" | grep -q 'usr/share/nereusd/nereusd.conf.sample$'
# The network receive buffer limit the radio socket needs, and the helper the
# postinst applies it with (packaging/nereusd-receive-buffer.conf).
printf '%s\n' "$contents" | grep -q 'usr/lib/sysctl.d/10-nereusd.conf$'
printf '%s\n' "$contents" | grep -q 'usr/libexec/nereusd/raise-receive-buffer-limit$'
printf '%s\n' "$contents" | grep -q 'usr/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz$'
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
dpkg-deb --extract "$deb" "$tmp"
test "$(readelf -h "$tmp/usr/bin/nereusd" | sed -n 's/^[[:space:]]*Machine:[[:space:]]*//p')" = AArch64
grep -q '^DynamicUser=yes$' "$tmp/usr/lib/systemd/system/nereusd.service" 2>/dev/null || \
    grep -q '^DynamicUser=yes$' "$tmp/lib/systemd/system/nereusd.service"
grep -q '^SupplementaryGroups=audio$' "$tmp/usr/lib/systemd/system/nereusd.service.d/audio.conf" 2>/dev/null || \
    grep -q '^SupplementaryGroups=audio$' "$tmp/lib/systemd/system/nereusd.service.d/audio.conf"
test -s "$tmp/usr/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz"
grep -q '^net\.core\.rmem_max = [0-9][0-9]*$' "$tmp/usr/lib/sysctl.d/10-nereusd.conf"
test -x "$tmp/usr/libexec/nereusd/raise-receive-buffer-limit"
dpkg-deb --control "$deb" "$tmp/DEBIAN"
grep -q 'raise-receive-buffer-limit /usr/lib/sysctl.d/10-nereusd.conf' "$tmp/DEBIAN/postinst"
