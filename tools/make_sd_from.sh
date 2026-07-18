#!/usr/bin/env bash
# Build a FAT SD image from real photos in a source directory. HEIC/JPG are transcoded and
# resized to frame-friendly baseline JPEGs, PRESERVING EXIF orientation (so the firmware's
# EXIF rotation displays them upright). Run in WSL/Linux.
# Usage: ./tools/make_sd_from.sh <source_dir> <out.img>
set -euo pipefail

SRC="${1:?usage: make_sd_from.sh <source_dir> <out.img>}"
OUT="${2:?output .img}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

shopt -s nullglob nocaseglob
i=1
for f in "$SRC"/*.heic "$SRC"/*.jpg "$SRC"/*.jpeg; do
    out="$(printf '%s/%02d.jpg' "$TMP" "$i")"
    # Resize (only if larger), baseline (non-progressive) JPEG, keep EXIF orientation.
    if convert "$f" -resize '1920x1920>' -quality 85 -interlace none "$out" 2>/dev/null; then
        info="$(identify -format '%wx%h orient=%[orientation]' "$out" 2>/dev/null || echo '?')"
        echo "  $(basename "$f")  ->  $(basename "$out")  [$info]"
        i=$((i + 1))
    else
        echo "  $(basename "$f")  -> SKIPPED (convert failed)"
    fi
done
shopt -u nullglob nocaseglob

count=$((i - 1))
[ "$count" -gt 0 ] || { echo "No images produced from $SRC"; exit 1; }

dd if=/dev/zero of="$OUT" bs=1M count=64 status=none
mkfs.fat -F 16 -n LUMEN "$OUT" >/dev/null
MTOOLS_SKIP_CHECK=1 mcopy -i "$OUT" "$TMP"/*.jpg ::/
echo "wrote $OUT with $count photo(s) from $SRC"
