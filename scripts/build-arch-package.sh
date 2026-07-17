#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
command -v makepkg >/dev/null 2>&1 || {
  echo "makepkg is required (install Arch base-devel)." >&2
  exit 1
}

cd "$ROOT/config/arch"
makepkg --cleanbuild --force
echo "Package built in $ROOT/config/arch"
echo "Install with: sudo pacman -U $ROOT/config/arch/libfprint-gxfp-*.pkg.tar.zst"
