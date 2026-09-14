#!/bin/sh
set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
errors=0
warnings=0

ok() { printf 'OK    %s\n' "$1"; }
warn() { printf 'WARN  %s\n' "$1"; warnings=$((warnings + 1)); }
fail() { printf 'FAIL  %s\n' "$1"; errors=$((errors + 1)); }

if test -e /sys/bus/acpi/devices/GXFP5130:00; then
  ok "ACPI device GXFP5130:00 detected"
else
  fail "ACPI device GXFP5130:00 not detected"
fi

if test -d "/lib/modules/$(uname -r)/build"; then
  ok "headers match running kernel $(uname -r)"
else
  fail "missing headers for running kernel $(uname -r)"
fi

for command in make cmake meson ninja dkms modinfo; do
  if command -v "$command" >/dev/null 2>&1; then
    ok "command available: $command"
  else
    fail "missing command: $command"
  fi
done

mbedtls_version=$(pkg-config --modversion mbedtls 2>/dev/null) || mbedtls_version=
case "$mbedtls_version" in
  2.*|3.*) supported_mbedtls=1 ;;
  *) supported_mbedtls=0 ;;
esac
if test "$supported_mbedtls" -eq 1 && test -f /usr/include/mbedtls/mbedtls/gcm.h; then
  ok "Mbed TLS $mbedtls_version development package detected"
elif test -f /usr/include/mbedtls3/mbedtls/gcm.h && \
     PKG_CONFIG_PATH=/usr/lib/mbedtls3/pkgconfig pkg-config --exists mbedtls 2>/dev/null; then
  ok "Mbed TLS 3 compatibility development package detected"
else
  fail "Mbed TLS development package not detected"
fi

for pc_module in gusb pixman-1 nss gudev-1.0 cairo; do
  if pkg-config --exists "$pc_module" 2>/dev/null; then
    ok "pkg-config module available: $pc_module"
  else
    fail "missing pkg-config module: $pc_module"
  fi
done

if pkg-config --exists opencv5 2>/dev/null || pkg-config --exists opencv4 2>/dev/null; then
  ok "pkg-config module available: opencv"
else
  fail "missing pkg-config module: opencv (opencv5 or opencv4)"
fi

glib_mkenums=$(pkg-config --variable=glib_mkenums glib-2.0 2>/dev/null) || glib_mkenums=
if test -n "$glib_mkenums" && test -x "$glib_mkenums"; then
  ok "GLib codegen tools available ($glib_mkenums)"
else
  fail "glib-2.0 codegen tools missing (install glib2-devel on Arch)"
fi

if test -f "$ROOT/kernel/gxfp.ko"; then
  built_kernel=$(modinfo -F vermagic "$ROOT/kernel/gxfp.ko" 2>/dev/null | awk '{print $1}')
  if test "$built_kernel" = "$(uname -r)"; then
    ok "local gxfp.ko matches running kernel"
  else
    warn "local gxfp.ko is absent or built for another kernel"
  fi
else
  warn "local gxfp.ko not built; run ./scripts/build.sh"
fi

if test -c /dev/gxfp; then
  ok "/dev/gxfp exists"
else
  warn "/dev/gxfp absent; install and load the module"
fi

psk=/var/lib/fprintd/gxfp/psk_raw32.bin
if test -f "$psk"; then
  size=$(wc -c < "$psk")
  mode=$(stat -c %a "$psk" 2>/dev/null || stat -f %Lp "$psk")
  if test "$size" -eq 32; then ok "PSK is exactly 32 bytes"; else fail "PSK must be 32 bytes (found $size)"; fi
  case "$mode" in 600|400) ok "PSK permissions are restricted ($mode)" ;; *) warn "PSK permissions should be 0600 or 0400 (found $mode)" ;; esac
else
  warn "PSK not installed at $psk"
fi

printf '\nSummary: %d error(s), %d warning(s)\n' "$errors" "$warnings"
test "$errors" -eq 0
