#!/bin/bash
set -euo pipefail
: "${1:?package path required}"
: "${2:?expected version required}"
deb=$1
version=$2
test -f "$deb"
test "$(dpkg-deb --field "$deb" Package)" = nereusd
test "$(dpkg-deb --field "$deb" Architecture)" = arm64
test "$(dpkg-deb --field "$deb" Version)" = "$version"
deps=$(dpkg-deb --field "$deb" Depends)
test -n "$deps"
case "$deps" in
    *libc6*libqt6*|*libqt6*libc6*) ;;
    *) echo 'Package lacks expected libc6/Qt6 dependencies' >&2; exit 1 ;;
esac
contents=$(dpkg-deb --contents "$deb")
printf '%s\n' "$contents" | grep -q 'usr/bin/nereusd$'
printf '%s\n' "$contents" | grep -q 'systemd/system/nereusd.service$'
printf '%s\n' "$contents" | grep -q 'usr/share/nereusd/nereusd.conf.sample$'
printf '%s\n' "$contents" | grep -q 'usr/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz$'
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
dpkg-deb --extract "$deb" "$tmp"
test "$(readelf -h "$tmp/usr/bin/nereusd" | sed -n 's/^[[:space:]]*Machine:[[:space:]]*//p')" = AArch64
grep -q '^DynamicUser=yes$' "$tmp/usr/lib/systemd/system/nereusd.service" 2>/dev/null || \
    grep -q '^DynamicUser=yes$' "$tmp/lib/systemd/system/nereusd.service"
test -s "$tmp/usr/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz"
