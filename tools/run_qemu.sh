#!/usr/bin/env bash
# Open a LIVE QEMU window running kernel8.img, emulating a Raspberry Pi 3B (BCM2837 —
# same SoC family as the Zero 2 W). On Windows/WSL2 the window appears via WSLg.
#
# Usage:
#   ./tools/run_qemu.sh <kernel8.img> [sd.img] [extra qemu args...]
#
# Notes:
#   - A GTK window opens on your desktop. Close it (or Ctrl-C here) to quit.
#   - Faithful in emulation: boot, framebuffer, serial, timers, app logic, render, SD/FatFs.
#     NOT faithful: WiFi (no CYW43), tryboot/A-B, exact HDMI timing, NEON perf.
set -euo pipefail

KERNEL="${1:?usage: run_qemu.sh <kernel8.img> [sd.img] [extra qemu args]}"
shift || true

SDARGS=()
if [ "${1:-}" ] && [ -f "${1:-}" ] && [[ "${1}" == *.img ]]; then
    SDARGS=(-drive "file=$1,if=sd,format=raw")
    shift
fi

# WSLg exposes an X server at :0.
export DISPLAY="${DISPLAY:-:0}"

exec qemu-system-aarch64 \
    -M raspi3b \
    -kernel "$KERNEL" \
    -serial null \
    -display gtk \
    "${SDARGS[@]}" \
    "$@"
