#!/usr/bin/env bash
# Build a FAT SD-card image with several photos of DIFFERENT sizes/patterns, for QEMU —
# proves multi-photo slideshow + aspect-preserving scaling. Run in WSL/Linux.
# Usage: ./tools/make_sd.sh <out.img>
set -euo pipefail

OUT="${1:?usage: make_sd.sh <out.img>}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# gen <pattern> <w> <h> <out.jpg>  (falls back to -madras if a pattern is unavailable)
gen() {
    ppmpat "-$1" -width "$2" -height "$3" > "$TMP/p.ppm" 2>/dev/null \
        || ppmpat "-$1" "$2" "$3" > "$TMP/p.ppm" 2>/dev/null \
        || ppmpat -madras -width "$2" -height "$3" > "$TMP/p.ppm"
    cjpeg -quality 85 -baseline "$TMP/p.ppm" > "$4"
}

gen argyle 640 480 "$TMP/01.jpg"   # large landscape (tests downscale)
gen madras 300 400 "$TMP/02.jpg"   # portrait (tests aspect / letterbox)
gen tartan 480 288 "$TMP/03.jpg"   # exact frame ratio

# 64 MiB (power-of-two) FAT16 superfloppy.
dd if=/dev/zero of="$OUT" bs=1M count=64 status=none
mkfs.fat -F 16 -n LUMEN "$OUT" >/dev/null
MTOOLS_SKIP_CHECK=1 mcopy -i "$OUT" "$TMP/01.jpg" "$TMP/02.jpg" "$TMP/03.jpg" ::/

echo "wrote $OUT with 3 photos (01 640x480 argyle, 02 300x400 madras, 03 480x288 tartan)"
