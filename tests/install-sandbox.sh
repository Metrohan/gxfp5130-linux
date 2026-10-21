#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
#
# Runs install.sh, uninstall.sh and provision-psk.sh as root against a
# throwaway filesystem, with dkms/systemctl/udevadm/gxfp_psk_tool stubbed.
# Refuses to run anywhere but a disposable container: the scripts use
# absolute paths (/usr/local/bin, /etc/udev, /var/lib/fprintd).
set -eu

if ! { [ "${GXFP_SANDBOX:-}" = 1 ] && [ -e /.dockerenv ] && [ "$(id -u)" -eq 0 ]; }; then
  echo "refusing: run as root in a throwaway container with GXFP_SANDBOX=1" >&2
  exit 2
fi

SRC=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
REPO=$T/repo BIN=$T/stubs LOG=$T/calls.log
mkdir -p "$REPO/build/userspace" "$BIN"
cp -a "$SRC/scripts" "$SRC/config" "$SRC/kernel" "$REPO/"
for t in gxfp_capture gxfp_psk_tool gxfp_recovery; do
  printf '#!/bin/sh\n' > "$REPO/build/userspace/$t"
done

# Every external command only logs itself; STUB_FAIL="<cmd> <subcmd>" makes one fail.
for c in dkms udevadm systemctl depmod make modprobe; do
  cat > "$BIN/$c" <<'STUB'
#!/bin/sh
echo "$(basename "$0") $*" >> "$LOG"
[ "${STUB_FAIL:-}" = "$(basename "$0") $1" ] && exit 1
exit 0
STUB
done
# --build-bb010002 BLOB --out-psk-raw32 PSK: writes STUB_PSK_BYTES random bytes
cat > "$BIN/gxfp_psk_tool" <<'STUB'
#!/bin/sh
echo "gxfp_psk_tool $*" >> "$LOG"
case "$1" in
  --build-bb010002) echo blob > "$2"; head -c "${STUB_PSK_BYTES:-32}" /dev/urandom > "$4" ;;
  --upload-bb010002) [ "${STUB_FAIL:-}" = upload ] && exit 1 ;;
esac
exit 0
STUB
chmod +x "$BIN"/*
export PATH="$BIN:$PATH" LOG

n=0 bad=0
t() { # t "description" command...
  desc=$1; shift; n=$((n + 1))
  if "$@" >/dev/null 2>&1; then echo "ok   $n $desc"; else echo "FAIL $n $desc"; bad=$((bad + 1)); fi
}
perm() { [ "$(stat -c '%a %U:%G' "$1")" = "$2" ]; }
absent() { [ ! -e "$1" ]; }
sum() { sha256sum "$1" | cut -d' ' -f1; }
reset() {
  rm -rf /usr/src/gxfp-0.1.0 /usr/local/bin/gxfp_* /etc/udev/rules.d/60-gxfp.rules \
    /etc/systemd/system/fprintd.service.d /var/lib/fprintd
  : > "$LOG"
}
run() { rc=0; env "$@" >/dev/null 2>&1 || rc=$?; }  # sets rc

mkdir -p /etc/pam.d /var/lib/fprint
printf 'auth sufficient pam_unix.so\n' > /etc/pam.d/system-auth
pam_sum=$(find /etc/pam.d -type f -exec sha256sum {} + | sort | sha256sum)
pam_same() { [ "$(find /etc/pam.d -type f -exec sha256sum {} + | sort | sha256sum)" = "$pam_sum" ]; }

echo "# install"
reset
run sh "$REPO/scripts/install.sh"
t "install exits 0" [ "$rc" -eq 0 ]
t "tools installed 0755 root:root" perm /usr/local/bin/gxfp_capture "755 root:root"
t "udev rule installed 0644" perm /etc/udev/rules.d/60-gxfp.rules "644 root:root"
t "fprintd drop-in installed 0644" perm /etc/systemd/system/fprintd.service.d/gxfp.conf "644 root:root"
t "DKMS source copied" [ -f /usr/src/gxfp-0.1.0/dkms.conf ]
t "dkms add, build, install in that order" \
  [ "$(grep -E '^dkms (add|build|install) ' "$LOG" | cut -d' ' -f2 | tr '\n' ' ')" = "add build install " ]
run sh "$REPO/scripts/install.sh"
t "install is idempotent (second run exits 0)" [ "$rc" -eq 0 ]
t "install leaves /etc/pam.d untouched" pam_same

echo "# uninstall"
mkdir -p /var/lib/fprintd/gxfp
head -c 32 /dev/urandom > /var/lib/fprintd/gxfp/psk_raw32.bin
before=$(sum /var/lib/fprintd/gxfp/psk_raw32.bin)
run sh "$REPO/scripts/uninstall.sh"
t "uninstall exits 0" [ "$rc" -eq 0 ]
t "tools removed" absent /usr/local/bin/gxfp_capture
t "udev rule removed" absent /etc/udev/rules.d/60-gxfp.rules
t "fprintd drop-in removed" absent /etc/systemd/system/fprintd.service.d/gxfp.conf
t "DKMS source removed" absent /usr/src/gxfp-0.1.0
t "dkms remove called" grep -q '^dkms remove ' "$LOG"
t "PSK preserved byte for byte" [ "$(sum /var/lib/fprintd/gxfp/psk_raw32.bin)" = "$before" ]
t "uninstall leaves /etc/pam.d untouched" pam_same

echo "# failed install does not leave half a userspace behind"
reset
run STUB_FAIL="dkms build" sh "$REPO/scripts/install.sh"
t "install exits non-zero when dkms build fails" [ "$rc" -ne 0 ]
t "no tools installed" absent /usr/local/bin/gxfp_capture
t "no udev rule installed" absent /etc/udev/rules.d/60-gxfp.rules
t "failed install leaves /etc/pam.d untouched" pam_same

echo "# provision-psk"
PSK=/var/lib/fprintd/gxfp/psk_raw32.bin
reset
rm -f /dev/gxfp
run sh "$REPO/scripts/provision-psk.sh"
t "refuses without /dev/gxfp (exit 1)" [ "$rc" -eq 1 ]
mknod /dev/gxfp c 1 3
run sh "$REPO/scripts/provision-psk.sh"
t "fresh provision exits 0" [ "$rc" -eq 0 ]
t "PSK is 32 bytes" [ "$(wc -c < "$PSK")" -eq 32 ]
t "PSK file is 0600 root:root" perm "$PSK" "600 root:root"
t "PSK dir is 0700 root:root" perm /var/lib/fprintd/gxfp "700 root:root"
before=$(sum "$PSK")
run sh "$REPO/scripts/provision-psk.sh"
t "refuses to replace existing PSK (exit 2)" [ "$rc" -eq 2 ]
t "existing PSK unchanged after refusal" [ "$(sum "$PSK")" = "$before" ]
run STUB_FAIL=upload sh "$REPO/scripts/provision-psk.sh" --replace
t "failed upload exits non-zero" [ "$rc" -ne 0 ]
t "existing PSK unchanged after failed upload" [ "$(sum "$PSK")" = "$before" ]
run STUB_PSK_BYTES=31 sh "$REPO/scripts/provision-psk.sh" --replace
t "wrong-size PSK exits 1" [ "$rc" -eq 1 ]
t "existing PSK unchanged after wrong-size PSK" [ "$(sum "$PSK")" = "$before" ]
run sh "$REPO/scripts/provision-psk.sh" --replace
t "--replace exits 0" [ "$rc" -eq 0 ]
t "--replace changes the PSK" [ "$(sum "$PSK")" != "$before" ]
rm -f /dev/gxfp

echo "$((n - bad))/$n passed"
[ "$bad" -eq 0 ]
