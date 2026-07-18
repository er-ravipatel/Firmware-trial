#!/usr/bin/env bash
# Build a FAT SD-card image containing /photo.jpg, for QEMU.
# Uses a DISTINCT pattern (argyle) from the embedded fallback (madras), so a successful
# SD load is visually obvious. Run in WSL/Linux.
# Usage: ./tools/make_sd.sh <out.img>
set -euo pipefail

OUT="${1:?usage: make_sd.sh <out.img>}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

ppmpat -argyle -width 480 -height 288 > "$TMP/photo.ppm" 2>/dev/null \
    || ppmpat -argyle 480 288 > "$TMP/photo.ppm"
cjpeg -quality 85 -baseline "$TMP/photo.ppm" > "$TMP/photo.jpg"

# 64 MiB (power-of-two, required by QEMU raspi SD) FAT16 superfloppy.
dd if=/dev/zero of="$OUT" bs=1M count=64 status=none
mkfs.fat -F 16 -n LUMEN "$OUT" >/dev/null
MTOOLS_SKIP_CHECK=1 mcopy -i "$OUT" "$TMP/photo.jpg" ::/photo.jpg

echo "wrote $OUT with /photo.jpg ($(stat -c%s "$TMP/photo.jpg") bytes, argyle pattern)"
