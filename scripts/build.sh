#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
JOBS=${JOBS:-2}
set -- -DCMAKE_BUILD_TYPE=Release

# Arch Linux installs the co-installable Mbed TLS 3 compatibility package in
# versioned paths now that the default mbedtls package is version 4.
if test -f /usr/include/mbedtls3/mbedtls/gcm.h; then
  set -- "$@" -DMbedTLS_DIR=/usr/lib/mbedtls3/cmake/MbedTLS
  PKG_CONFIG_PATH=/usr/lib/mbedtls3/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}
  export PKG_CONFIG_PATH
fi

make -C "$ROOT/kernel" -j"$JOBS"
cmake -S "$ROOT/userspace" -B "$ROOT/build/userspace" "$@"
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
