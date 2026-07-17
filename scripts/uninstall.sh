#!/bin/sh
set -eu

[ "$(id -u)" -eq 0 ] || { echo "Run as root (sudo $0)" >&2; exit 1; }
VERSION=0.1.0

modprobe -r gxfp 2>/dev/null || true
dkms remove -m gxfp -v "$VERSION" --all 2>/dev/null || true
rm -rf "/usr/src/gxfp-$VERSION"
rm -f /usr/local/bin/gxfp_capture /usr/local/bin/gxfp_psk_tool /usr/local/bin/gxfp_recovery
rm -f /etc/udev/rules.d/60-gxfp.rules
rm -f /etc/systemd/system/fprintd.service.d/gxfp.conf
udevadm control --reload-rules
systemctl daemon-reload
echo "GXFP transport and tools removed; PSK and fingerprints were preserved."
