#!/usr/bin/env bash
# Run a bare-metal kernel8.img in QEMU, emulating a Raspberry Pi 3B (BCM2837 —
# the same SoC family as the Zero 2 W, which is why Circle uses RASPPI=3 for both).
#
# Usage:
#   ./tools/run_qemu.sh <path-to-kernel8.img> [extra qemu args...]
#
# Notes:
#   - Display opens via WSLg (GTK window). For headless, append: -display none
#   - Serial console is routed to this terminal (stdio). Ctrl-A X to quit QEMU.
#   - Faithful in emulation: boot, framebuffer, serial, timers, app logic, render.
#     NOT faithful: WiFi (no CYW43), tryboot/A-B, exact HDMI timing, NEON perf.
set -euo pipefail

KERNEL="${1:?usage: run_qemu.sh <kernel8.img> [extra qemu args]}"
shift || true

exec qemu-system-aarch64 \
    -M raspi3b \
    -kernel "$KERNEL" \
    -serial stdio \
    -display gtk \
    "$@"
