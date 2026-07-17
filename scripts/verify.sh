#!/bin/sh
set -eu

failed=0
check() {
  if "$@"; then printf 'OK   %s\n' "$*"; else printf 'FAIL %s\n' "$*"; failed=1; fi
}

check test -e /sys/bus/acpi/devices/GXFP5130:00
check modinfo gxfp
check test -c /dev/gxfp

if test -c /dev/gxfp; then
  ls -l /dev/gxfp
fi

echo "Kernel messages:"
journalctl -k -b --no-pager -g gxfp 2>/dev/null | tail -n 50 || true
exit "$failed"
