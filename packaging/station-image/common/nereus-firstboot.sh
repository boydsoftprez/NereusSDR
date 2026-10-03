#!/bin/sh
set -eu
marker=/var/lib/nereus-station/first-boot-pending
[ -e "$marker" ] || exit 0
hostnamectl set-hostname nereus-station
if grep -q '^127\.0\.1\.1[[:space:]]' /etc/hosts; then
    sed -i 's/^127\.0\.1\.1[[:space:]].*/127.0.1.1 nereus-station/' /etc/hosts
else
    printf '127.0.1.1 nereus-station\n' >> /etc/hosts
fi
rm -f "$marker"
