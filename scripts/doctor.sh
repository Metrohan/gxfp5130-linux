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

if pkg-config --exists mbedtls 2>/dev/null; then
  ok "Mbed TLS development package detected"
else
  fail "Mbed TLS development package not detected"
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
