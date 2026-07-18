#!/usr/bin/env bash
# Boot kernel8.img in QEMU headless, capture the framebuffer, and write a PNG — all in
# one process so no temp state is lost between steps.
# Usage: ./tools/qemu_capture_png.sh <kernel8.img> <out.png> [boot_secs]
set -euo pipefail

KERNEL="${1:?usage: qemu_capture_png.sh <kernel8.img> <out.png> [boot_secs] [sd.img]}"
OUTPNG="${2:?output png path}"
WAIT="${3:-3}"
SDIMG="${4:-}"

SDARGS=()
if [ -n "$SDIMG" ]; then
    SDARGS=(-drive "file=$SDIMG,if=sd,format=raw")
fi

TMPPPM="$(mktemp --suffix=.ppm)"
trap 'rm -f "$TMPPPM"' EXIT

{ sleep "$WAIT"; echo "screendump $TMPPPM"; sleep 1; echo "quit"; } | \
    qemu-system-aarch64 \
        -M raspi3b -kernel "$KERNEL" \
        -display none -monitor stdio -serial null -no-reboot \
        "${SDARGS[@]}" \
        >/dev/null 2>&1 || true

if [ ! -s "$TMPPPM" ]; then
    echo "FAIL: QEMU produced no framebuffer dump"
    exit 1
fi

pnmtopng "$TMPPPM" > "$OUTPNG"
echo "OK wrote $OUTPNG ($(stat -c%s "$OUTPNG") bytes)"
