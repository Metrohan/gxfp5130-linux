#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
JOBS=${JOBS:-2}

make -C "$ROOT/kernel" -j"$JOBS"
cmake -S "$ROOT/userspace" -B "$ROOT/build/userspace" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build/userspace" -j"$JOBS"

meson setup "$ROOT/build/libfprint" "$ROOT/libfprint" \
  --wipe --prefix=/usr \
  -Ddrivers=gxfp -Ddoc=false -Dintrospection=false -Dgtk-examples=false \
  -Dudev_rules=disabled -Dudev_hwdb=disabled
meson compile -C "$ROOT/build/libfprint" -j "$JOBS"

printf '%s\n' "Build complete:"
printf '  %s\n' "$ROOT/kernel/gxfp.ko"
printf '  %s\n' "$ROOT/build/userspace/gxfp_capture"
printf '  %s\n' "$ROOT/build/libfprint/libfprint/libfprint-2.so.2"
