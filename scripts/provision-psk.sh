#!/bin/sh
set -eu

[ "$(id -u)" -eq 0 ] || { echo "Run as root (sudo $0)" >&2; exit 1; }
case "$#:${1:-}" in
  0:|1:--replace) ;;
  *) echo "Usage: $0 [--replace]" >&2; exit 2 ;;
esac

PSK_DIR=/var/lib/fprint/gxfp
PSK="$PSK_DIR/psk_raw32.bin"
LEGACY_PSK=/var/lib/fprintd/gxfp/psk_raw32.bin

# Preserve existing installations; changing directories is not key rotation.
if test -e "$LEGACY_PSK" || test -L "$LEGACY_PSK"; then
  if test -e "$PSK" || test -L "$PSK"; then
    echo "Both new and legacy PSK paths exist; resolve them before provisioning." >&2
    exit 2
  fi
  PSK="$LEGACY_PSK"
  PSK_DIR=${PSK%/*}
  echo "Using legacy PSK path: $PSK (see README for migration)." >&2
fi

if { test -e "$PSK" || test -L "$PSK"; } && test "${1:-}" != "--replace"; then
  echo "Refusing to replace existing PSK: $PSK" >&2
  echo "Back it up and explicitly run: sudo $0 --replace" >&2
  exit 2
fi
# Never follow a key symlink, even during explicitly requested replacement.
if test -L "$PSK"; then
  echo "Refusing to replace a symlink: $PSK" >&2
  exit 2
fi

WORK=$(mktemp -d /tmp/gxfp-provision.XXXXXX)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM

test -c /dev/gxfp || { echo "/dev/gxfp is missing; load the gxfp module first." >&2; exit 1; }
command -v gxfp_psk_tool >/dev/null 2>&1 || { echo "gxfp_psk_tool is not installed." >&2; exit 1; }

gxfp_psk_tool --build-bb010002 "$WORK/blob.bin" \
  --out-psk-raw32 "$WORK/psk_raw32.bin"
test "$(wc -c < "$WORK/psk_raw32.bin")" -eq 32 || { echo "Generated PSK has invalid size." >&2; exit 1; }

gxfp_psk_tool --upload-bb010002 "$WORK/blob.bin"
install -d -m 0700 -o root -g root "$PSK_DIR"
install -m 0600 -o root -g root "$WORK/psk_raw32.bin" "$PSK"
if command -v restorecon >/dev/null 2>&1; then
  restorecon -R "$PSK_DIR"
fi

echo "PSK provisioned and installed at $PSK"
echo "Next: sudo gxfp_capture --psk-raw32 $PSK"
