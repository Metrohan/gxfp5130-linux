#!/bin/sh
set -eu

[ "$(id -u)" -eq 0 ] || { echo "Run as root (sudo $0)" >&2; exit 1; }
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=0.1.0
DKMS_SRC=/usr/src/gxfp-$VERSION

[ -e /sys/bus/acpi/devices/GXFP5130:00 ] || {
  echo "Warning: ACPI device GXFP5130:00 was not detected." >&2
}

rm -rf "$DKMS_SRC"
install -d "$DKMS_SRC"
cp -a "$ROOT/kernel/." "$DKMS_SRC/"
make -C "$DKMS_SRC" clean >/dev/null 2>&1 || true

dkms remove -m gxfp -v "$VERSION" --all >/dev/null 2>&1 || true
dkms add -m gxfp -v "$VERSION"
dkms build -m gxfp -v "$VERSION"
dkms install -m gxfp -v "$VERSION"

install -o root -g root -Dm0755 "$ROOT/build/userspace/gxfp_capture" /usr/local/bin/gxfp_capture
install -o root -g root -Dm0755 "$ROOT/build/userspace/gxfp_psk_tool" /usr/local/bin/gxfp_psk_tool
install -o root -g root -Dm0755 "$ROOT/build/userspace/gxfp_recovery" /usr/local/bin/gxfp_recovery
install -o root -g root -Dm0644 "$ROOT/config/60-gxfp.rules" /etc/udev/rules.d/60-gxfp.rules
install -o root -g root -Dm0644 "$ROOT/config/fprintd-gxfp.conf" \
  /etc/systemd/system/fprintd.service.d/gxfp.conf

udevadm control --reload-rules
systemctl daemon-reload
depmod -a
echo "Installed. Run: modprobe gxfp && $ROOT/scripts/verify.sh"
