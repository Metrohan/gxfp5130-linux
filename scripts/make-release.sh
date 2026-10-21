#!/bin/sh
# Source tarball of a git ref (default HEAD): tracked files only, so untracked
# or ignored local files (recordings, finger.pgm, .claude/) can never leak in.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PARENT=$(dirname -- "$ROOT")
NAME=$(basename -- "$ROOT")
REF=${1:-HEAD}
ARCHIVE="$PARENT/${NAME}.tar.gz"

git -C "$ROOT" archive --format=tar.gz --prefix="$NAME/" -o "$ARCHIVE" "$REF"

( cd "$PARENT" && sha256sum "${NAME}.tar.gz" > "${NAME}.tar.gz.sha256" )
printf 'Created %s from %s\n' "$ARCHIVE" "$REF"
printf 'Checksum: '
cat "$ARCHIVE.sha256"
