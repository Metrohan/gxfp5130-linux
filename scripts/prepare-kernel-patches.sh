#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
#
# prepare-kernel-patches.sh — stage GXFP5130 driver into a kernel tree and
# generate a ready-to-send patch series.
#
# Usage:
#   ./scripts/prepare-kernel-patches.sh /path/to/linux-kernel [branch-name]
#
# Prerequisites:
#   - A clean Linux kernel git tree (git clone torvalds/linux or stable)
#   - git send-email configured for your mailer
#   - perl (for checkpatch.pl)
#
# What it does:
#   1. Creates a new branch in the kernel tree
#   2. Stages four commits (UAPI header, driver, docs, MAINTAINERS)
#   3. Runs checkpatch.pl on every patch
#   4. Writes patches to scripts/patches/ (ready for git send-email)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DRIVER_SRC="$REPO_ROOT/kernel"

# ── argument handling ───────────────────────────────────────────────────────
if [ -z "${1:-}" ]; then
    echo "Usage: $0 /path/to/linux-kernel [branch-name]" >&2
    exit 1
fi

KDIR="$(realpath "$1")"
BRANCH="${2:-gxfp5130-driver}"
PATCH_OUT="$REPO_ROOT/scripts/patches"

# ── sanity checks ───────────────────────────────────────────────────────────
if [ ! -f "$KDIR/Makefile" ] || ! grep -q "LINUX_VERSION_CODE\|KERNELVERSION" "$KDIR/Makefile" 2>/dev/null; then
    echo "Error: '$KDIR' does not look like a Linux kernel source tree." >&2
    exit 1
fi

if ! git -C "$KDIR" diff --quiet HEAD 2>/dev/null; then
    echo "Error: kernel tree has uncommitted changes. Please stash or commit first." >&2
    exit 1
fi

if git -C "$KDIR" show-ref --verify --quiet "refs/heads/$BRANCH"; then
    echo "Error: branch '$BRANCH' already exists in kernel tree." >&2
    echo "Delete it first:  git -C '$KDIR' branch -D '$BRANCH'" >&2
    exit 1
fi

echo "==> Kernel tree : $KDIR"
echo "==> Branch      : $BRANCH"
echo "==> Patch output: $PATCH_OUT"
echo ""

# ── create branch ──────────────────────────────────────────────────────────
git -C "$KDIR" checkout -b "$BRANCH"
echo "--> Created branch $BRANCH"

# ────────────────────────────────────────────────────────────────────────────
# COMMIT 1: UAPI header
# ────────────────────────────────────────────────────────────────────────────
echo ""
echo "==> [1/4] include/uapi/linux/gxfp_ioctl.h"

install -Dm644 \
    "$DRIVER_SRC/include/uapi/linux/gxfp_ioctl.h" \
    "$KDIR/include/uapi/linux/gxfp_ioctl.h"

git -C "$KDIR" add include/uapi/linux/gxfp_ioctl.h
git -C "$KDIR" commit -s -m "$(cat <<'EOF'
include/uapi/linux: add gxfp_ioctl.h for GXFP5130 fingerprint sensor

Define the userspace ABI for the Goodix GXFP5130 eSPI fingerprint sensor
driver:

 - struct gxfp_tap_hdr: header prepended to every read(2) record, carries
   payload length, MP protocol type and a ktime_get_ns() timestamp.
 - struct gxfp_tx_pkt_hdr: header that userspace prepends to write(2)
   payloads so the driver can wrap them in the eSPI frame format.
 - GXFP_IOCTL_FLUSH_RXQ: flush the kernel-side RX record queue.

Signed-off-by: Metehan Günen <metehangnen@gmail.com>
EOF
)"

# ────────────────────────────────────────────────────────────────────────────
# COMMIT 2: driver source tree
# ────────────────────────────────────────────────────────────────────────────
echo ""
echo "==> [2/4] drivers/misc/gxfp5130/"

DEST="$KDIR/drivers/misc/gxfp5130"
mkdir -p "$DEST"

# Top-level files
install -Dm644 "$DRIVER_SRC/Kconfig"    "$DEST/Kconfig"
install -Dm644 "$DRIVER_SRC/Makefile"   "$DEST/Makefile"
install -Dm644 "$DRIVER_SRC/gxfp_main.c" "$DEST/gxfp_main.c"

# Internal headers (private, not exported to uapi)
install -Dm644 "$DRIVER_SRC/include/gxfp_priv.h"      "$DEST/include/gxfp_priv.h"
install -Dm644 "$DRIVER_SRC/include/gxfp_constants.h" "$DEST/include/gxfp_constants.h"

# Subdirectory sources
for subdir in transport hw proto cmd driver; do
    mkdir -p "$DEST/$subdir"
    for f in "$DRIVER_SRC/$subdir"/*.c "$DRIVER_SRC/$subdir"/*.h; do
        [ -f "$f" ] || continue
        install -Dm644 "$f" "$DEST/$subdir/$(basename "$f")"
    done
done

# Wire into drivers/misc/Kconfig — insert after the last 'source' line
# starting with "drivers/misc/" that alphabetically precedes GXFP5130.
# We insert before the closing comment block / endif.  A safe anchor is the
# line containing "GOOGLE_FIRMWARE" or, failing that, just before endif.
MISC_KCONFIG="$KDIR/drivers/misc/Kconfig"
if grep -q 'gxfp5130' "$MISC_KCONFIG" 2>/dev/null; then
    echo "    (drivers/misc/Kconfig already contains gxfp5130 entry, skipping)"
elif grep -q 'GOOGLE_FIRMWARE\|google-firmware' "$MISC_KCONFIG" 2>/dev/null; then
    # Insert right after the google-firmware source line
    sed -i '/GOOGLE_FIRMWARE\|google-firmware/a source "drivers/misc/gxfp5130/Kconfig"' "$MISC_KCONFIG"
else
    # Fallback: insert before the final 'endmenu'
    sed -i '/^endmenu/i source "drivers/misc/gxfp5130/Kconfig"\n' "$MISC_KCONFIG"
fi

# Wire into drivers/misc/Makefile
MISC_MAKEFILE="$KDIR/drivers/misc/Makefile"
if grep -q 'gxfp5130' "$MISC_MAKEFILE" 2>/dev/null; then
    echo "    (drivers/misc/Makefile already contains gxfp5130 entry, skipping)"
else
    printf '\nobj-$(CONFIG_GXFP5130)\t\t+= gxfp5130/\n' >> "$MISC_MAKEFILE"
fi

git -C "$KDIR" add \
    drivers/misc/gxfp5130/ \
    drivers/misc/Kconfig \
    drivers/misc/Makefile

git -C "$KDIR" commit -s -m "$(cat <<'EOF'
drivers/misc: add Goodix GXFP5130 eSPI fingerprint sensor driver

The GXFP5130 is a press-type fingerprint sensor by Goodix Technology.
It is found in Huawei MateBook laptops (D16 2024, X Pro 2024, 14 2024)
and is enumerated through ACPI with HID GXFP5130.

Hardware interface
------------------
The sensor does not sit on SPI or I²C. Instead the ACPI firmware maps a
memory window (eSPI flash-channel mailbox) and three GPIOs:

  write-done  host pulses high after writing a command frame
  read-done   host pulses high after reading each response chunk
  irq         sensor asserts to signal that a response is ready

All frames use a 4-byte eSPI wrapper header followed by a Goodix MP
protocol frame, which in turn carries the command/response body.

Driver structure
----------------
  transport/   eSPI framing layer (TX, synchronous RX, IRQ-driven RX)
  hw/          ACPI resource parsing, MMIO helpers, GPIO helpers, delay
  proto/       MP and Goodix command protocol encode/decode
  cmd/         individual command helpers (version, MCU state, reset …)
  driver/      platform probe/remove, IRQ thread, UAPI misc device,
               debugfs trace ring buffer
  include/     private driver headers (gxfp_priv.h, gxfp_constants.h)

Userspace interface
-------------------
A misc character device /dev/gxfp is created. Only one reader at a time
is allowed (CAP_SYS_ADMIN required). write(2) sends command frames;
read(2)/poll(2) return response records prefixed with gxfp_tap_hdr.
GXFP_IOCTL_FLUSH_RXQ discards the kernel-side response queue.

The libfprint library communicates with the sensor through this device.

Startup
-------
On probe the driver executes a firmware-version handshake (up to 3
retries) and falls back to a session-recovery sequence if the sensor
does not acknowledge the first attempt. The IRQ line is kept masked
until the handshake succeeds.

Signed-off-by: Metehan Günen <metehangnen@gmail.com>
EOF
)"

# ────────────────────────────────────────────────────────────────────────────
# COMMIT 3: documentation
# ────────────────────────────────────────────────────────────────────────────
echo ""
echo "==> [3/4] Documentation/misc-devices/gxfp5130.rst"

install -Dm644 \
    "$DRIVER_SRC/docs/gxfp5130.rst" \
    "$KDIR/Documentation/misc-devices/gxfp5130.rst"

# Add to Documentation/misc-devices/index.rst if it exists
INDEX_RST="$KDIR/Documentation/misc-devices/index.rst"
if [ -f "$INDEX_RST" ] && ! grep -q 'gxfp5130' "$INDEX_RST"; then
    # Append inside the toctree block, before the closing blank line / next heading
    sed -i '/^\.\. toctree::/,/^$/{/^$/i\   gxfp5130
}' "$INDEX_RST" 2>/dev/null || true
fi

git -C "$KDIR" add Documentation/misc-devices/gxfp5130.rst
[ -f "$INDEX_RST" ] && git -C "$KDIR" add Documentation/misc-devices/index.rst 2>/dev/null || true

git -C "$KDIR" commit -s -m "$(cat <<'EOF'
Documentation/misc-devices: add gxfp5130.rst

Document the Goodix GXFP5130 eSPI fingerprint sensor driver:
hardware interface (MMIO mailbox + 3 GPIOs), userspace ABI
(open/write/read/poll/ioctl), sysfs attribute, debugfs trace interface,
and the table of supported laptop models.

Signed-off-by: Metehan Günen <metehangnen@gmail.com>
EOF
)"

# ────────────────────────────────────────────────────────────────────────────
# COMMIT 4: MAINTAINERS
# ────────────────────────────────────────────────────────────────────────────
echo ""
echo "==> [4/4] MAINTAINERS"

MAINTAINERS="$KDIR/MAINTAINERS"
ENTRY="$(cat <<'ENTRY'

GOODIX GXFP5130 FINGERPRINT SENSOR DRIVER
M:	Metehan Günen <metehangnen@gmail.com>
L:	linux-kernel@vger.kernel.org
S:	Maintained
F:	Documentation/misc-devices/gxfp5130.rst
F:	drivers/misc/gxfp5130/
F:	include/uapi/linux/gxfp_ioctl.h
ENTRY
)"

if grep -q 'GXFP5130' "$MAINTAINERS"; then
    echo "    (MAINTAINERS already contains GXFP5130 entry, skipping)"
else
    # Insert after the last GOODIX entry, or before GOOGLE if no GOODIX yet
    if grep -q '^GOODIX' "$MAINTAINERS"; then
        # Find last GOODIX block and insert after its last F: line
        python3 - "$MAINTAINERS" "$ENTRY" <<'PY'
import sys, re
path, entry = sys.argv[1], sys.argv[2]
text = open(path).read()
# Find end of last GOODIX block
blocks = list(re.finditer(r'^GOODIX.*?(?=\n[A-Z\(]|\Z)', text, re.M | re.S))
if blocks:
    pos = blocks[-1].end()
    text = text[:pos] + '\n' + entry.lstrip('\n') + text[pos:]
else:
    text = text + entry
open(path, 'w').write(text)
PY
    else
        # Insert before GOOGLE block
        python3 - "$MAINTAINERS" "$ENTRY" <<'PY'
import sys, re
path, entry = sys.argv[1], sys.argv[2]
text = open(path).read()
m = re.search(r'^GOOGLE', text, re.M)
if m:
    text = text[:m.start()] + entry.lstrip('\n') + '\n\n' + text[m.start():]
else:
    text = text + entry
open(path, 'w').write(text)
PY
    fi
fi

git -C "$KDIR" add MAINTAINERS
git -C "$KDIR" commit -s -m "$(cat <<'EOF'
MAINTAINERS: add entry for GXFP5130 fingerprint sensor driver

Add maintainer entry for the new Goodix GXFP5130 driver covering
the driver source tree, UAPI header, and documentation.

Signed-off-by: Metehan Günen <metehangnen@gmail.com>
EOF
)"

# ────────────────────────────────────────────────────────────────────────────
# Run checkpatch on the four commits
# ────────────────────────────────────────────────────────────────────────────
echo ""
echo "==> Running checkpatch.pl ..."
CHECKPATCH="$KDIR/scripts/checkpatch.pl"
if [ -x "$CHECKPATCH" ]; then
    git -C "$KDIR" format-patch -4 --stdout | \
        perl "$CHECKPATCH" --no-tree - || true
else
    echo "    checkpatch.pl not found at $CHECKPATCH — skipping"
fi

# ────────────────────────────────────────────────────────────────────────────
# Generate the patch series
# ────────────────────────────────────────────────────────────────────────────
echo ""
echo "==> Generating patch series in $PATCH_OUT/ ..."
mkdir -p "$PATCH_OUT"
rm -f "$PATCH_OUT"/00*.patch

git -C "$KDIR" format-patch \
    --cover-letter \
    --subject-prefix="PATCH" \
    --output-directory="$PATCH_OUT" \
    -4

# Fill in cover letter placeholders
COVER="$PATCH_OUT/0000-cover-letter.patch"
sed -i \
    -e 's/\*\*\* SUBJECT HERE \*\*\*/drivers\/misc: add Goodix GXFP5130 eSPI fingerprint sensor driver/' \
    -e 's/\*\*\* BLURB HERE \*\*\*/'"$(cat <<'BLURB'
This series adds a kernel driver for the Goodix GXFP5130 fingerprint
sensor found in Huawei MateBook laptops (D16 2024, X Pro 2024, 14 2024).

The sensor is enumerated through ACPI (HID: GXFP5130) and communicates
with the host via an eSPI-based memory-mapped mailbox window plus three
GPIOs.  It does not use SPI or I²C bus drivers.

A misc character device \/dev\/gxfp is exposed to userspace.  The
libfprint library (with a corresponding GXFP5130 plugin) uses this
device to perform biometric enrollment and verification via fprintd.

The four patches are:
  [1\/4] UAPI header (include\/uapi\/linux\/gxfp_ioctl.h)
  [2\/4] Driver source tree (drivers\/misc\/gxfp5130\/)
  [3\/4] Documentation (Documentation\/misc-devices\/gxfp5130.rst)
  [4\/4] MAINTAINERS entry
BLURB
)/" \
    "$COVER" 2>/dev/null || true

echo ""
echo "==> Done. Patches written to $PATCH_OUT/"
echo ""
echo "    To review:   cat $PATCH_OUT/0000-cover-letter.patch"
echo ""
echo "    To send (dry run first):"
echo "      git send-email --dry-run \\"
echo "        --to='linux-kernel@vger.kernel.org' \\"
echo "        --cc='gregkh@linuxfoundation.org' \\"
echo "        --cc='linux-acpi@vger.kernel.org' \\"
echo "        --cc='linux-gpio@vger.kernel.org' \\"
echo "        $PATCH_OUT/*.patch"
echo ""
echo "    Then remove --dry-run to actually send."
