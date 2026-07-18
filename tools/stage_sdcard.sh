#!/usr/bin/env bash
# Assemble a ready-to-copy SD-card folder for the Raspberry Pi Zero 2 W (bare-metal Circle).
# Copy everything in firmware/sdcard/ onto a FAT32-formatted microSD, then boot the Pi.
# Usage: ./tools/stage_sdcard.sh [photo_source_dir]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BOOT="$ROOT/firmware/vendor/circle/boot"
OUT="$ROOT/firmware/sdcard"
PHOTOS="${1:-}"

if [ ! -f "$BOOT/bootcode.bin" ]; then
    echo "Boot firmware missing. Run: (cd $BOOT && make firmware)"; exit 1
fi
if [ ! -f "$ROOT/firmware/app/kernel8.img" ]; then
    echo "kernel8.img missing. Build first: (cd firmware/app && make)"; exit 1
fi

rm -rf "$OUT"; mkdir -p "$OUT"

# 1. Raspberry Pi firmware boot chain (GPU first-stage -> loads our kernel).
cp "$BOOT/bootcode.bin" "$BOOT/start.elf" "$BOOT/fixup.dat" "$OUT/"

# 2. config.txt — 64-bit, our kernel, HDMI pinned to the Acer panel.
cat > "$OUT/config.txt" <<'EOF'
# Lumen Frame - Raspberry Pi Zero 2 W (bare-metal Circle)
arm_64bit=1
kernel=kernel8.img
kernel_address=0x80000
initial_turbo=0

# Pin HDMI to the Acer 4347 panel's native 1366x768@60 (generic TV board may misreport EDID).
# If the screen stays blank, try: comment these out to auto-detect, or use hdmi_mode=87 + hdmi_cvt.
hdmi_force_hotplug=1
hdmi_group=2
hdmi_mode=81
disable_overscan=1

# Serial console on GPIO14 (pin 8) / GPIO15 (pin 10) @115200 for debugging (optional, harmless).
enable_uart=1
EOF

# 3. Our firmware.
cp "$ROOT/firmware/app/kernel8.img" "$OUT/"

# 4. Photos: transcode HEIC/JPG -> resized baseline JPEG into a photos/ subfolder
#    (the firmware scans photos/ then images/ then the root).
if [ -n "$PHOTOS" ] && [ -d "$PHOTOS" ]; then
    mkdir -p "$OUT/photos"
    shopt -s nullglob nocaseglob
    i=1
    for f in "$PHOTOS"/*.heic "$PHOTOS"/*.jpg "$PHOTOS"/*.jpeg; do
        out="$(printf '%s/photos/%02d.jpg' "$OUT" "$i")"
        if convert "$f" -resize '1920x1920>' -quality 85 -interlace none "$out" 2>/dev/null; then
            i=$((i + 1))
        fi
    done
    shopt -u nullglob nocaseglob
    echo "staged $((i - 1)) photo(s) into photos/ from $PHOTOS"
fi

echo "== SD staging ready: $OUT =="
ls -la "$OUT"
