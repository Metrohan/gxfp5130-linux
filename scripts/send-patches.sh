#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
#
# send-patches.sh — send the GXFP5130 patch series via git send-email.
#
# Run prepare-kernel-patches.sh first to generate patches/ contents.
# Pass --dry-run as first argument to preview without actually sending.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PATCH_DIR="$SCRIPT_DIR/patches"
DRY_RUN="${1:-}"

if [ ! -f "$PATCH_DIR/0001-"*.patch 2>/dev/null ]; then
    echo "Error: no patches found in $PATCH_DIR/" >&2
    echo "Run ./scripts/prepare-kernel-patches.sh first." >&2
    exit 1
fi

FLAGS=()
[ "$DRY_RUN" = "--dry-run" ] && FLAGS+=(--dry-run)

git send-email \
    "${FLAGS[@]}" \
    --to="linux-kernel@vger.kernel.org" \
    --cc="gregkh@linuxfoundation.org" \
    --cc="linux-acpi@vger.kernel.org" \
    --cc="linux-gpio@vger.kernel.org" \
    --annotate \
    "$PATCH_DIR"/00*.patch
