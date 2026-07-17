#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PARENT=$(dirname -- "$ROOT")
NAME=$(basename -- "$ROOT")
ARCHIVE="$PARENT/${NAME}.tar.gz"

tar -C "$PARENT" -czf "$ARCHIVE" \
  --exclude="$NAME/build" \
  --exclude="$NAME/.codegraph" \
  --exclude="$NAME/.cursor" \
  --exclude='*.ko' \
  --exclude='*.o' \
  --exclude='*.mod' \
  --exclude='*.mod.c' \
  --exclude='Module.symvers' \
  --exclude='modules.order' \
  --exclude='.*.cmd' \
  "$NAME"

sha256sum "$ARCHIVE" > "$ARCHIVE.sha256"
printf 'Created %s\n' "$ARCHIVE"
printf 'Checksum: '
cat "$ARCHIVE.sha256"
