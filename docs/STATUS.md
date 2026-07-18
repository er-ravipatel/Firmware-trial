# RESUME HERE — Project Status & Handoff

_Last updated: 2026-07-18. Read this first when picking the project back up._

**What this is:** Lumen Frame — a digital photo frame built as a **genuine bare-metal firmware
OS in C++ on the Circle framework** (no Linux) for a Raspberry Pi Zero 2 W, driving an Acer
Aspire 4347 (14", 1366×768) LCD over HDMI. Broadened (InkyPi-inspired) into a modular smart
display: rotating screen plugins (photo, clock, …). Developed **emulator-first** in QEMU.

---

## ▶ Where we are (works today, verified in QEMU)
- Boots our own OS with a **visible boot sequence** → runs a modular display.
- **Plugin architecture**: PluginScheduler rotates ScreenPlugins (photo + clock).
- **Photo slideshow**: scans SD/USB for `*.jpg`, **cross-fade transitions**, **aspect-fit scaling**
  (letterbox/pillarbox), **EXIF rotation** (upright phone photos).
- **JPEG decode** on bare metal via vendored **stb_image**.
- **Storage**: SD card (EMMC+FatFs) AND **USB pendrive** (USB host + MSD). Tries **USB → SD →
  embedded** fallback. Storage is optional (boots without either).
- **Real iPhone photos** displaying (5 HEIC transcoded to JPEG).
- **21 host unit tests, 376 checks passing** (Result/EventBus/RingBuffer/SHA-256/PluginScheduler/
  ExifReader).
- All committed & pushed to GitHub (er-ravipatel/Firmware-trial).

## ⏸ PENDING DECISION (this is where we stopped)
Building **WiFi + web UI**. Key finding (see "Gotchas"): **stock QEMU cannot run Circle
networking** (usb-net RX is broken; needs a patched QEMU). WiFi can't be emulated at all
(no CYW43). Three ways forward — pick one on resume:
- **(A) Build web UI now, test logic on host** — HTTP/HTML/JSON/routing as unit-testable logic;
  live web+WiFi test on real hardware. *(Recommended — fastest real progress.)*
- **(B) Build patched QEMU** (larchcone/qemu, ~30 min) to see the web UI live in emulator over
  Ethernet. WiFi still hardware-only.
- **(C) Pivot to real hardware** for WiFi + web UI (WiFi needs the Pi anyway).

---

## 🖥 Environment (already set up on this machine)
- **WSL2 Ubuntu** is the build env (Windows has no native toolchain). Invoke via `wsl -d Ubuntu`.
- Installed in WSL: `build-essential`, `cmake`, `qemu-system-arm` (QEMU 8.2.2),
  `gcc/g++-aarch64-linux-gnu` (13.3), `dosfstools`, `mtools`, `netpbm`, `libjpeg-turbo-progs`,
  `xxd`, `imagemagick` + `libheif` + **`libde265`/`libheif-plugin-libde265`** (HEIC decode).
- **Circle** cloned at `firmware/vendor/circle` (gitignored), configured with:
  `./configure -r 3 -p aarch64-linux-gnu- --qemu -f` then `./makeall -j4`.
  Addon libs built: `addon/fatfs`, `addon/SDCard` (`make -j4` in each). `lib/usb`, `lib/input`
  built by makeall.
- **stb_image** vendored at `firmware/vendor/stb/stb_image.h` (committed).
- Display: WSLg provides an X server at `DISPLAY=:0`; QEMU **SDL** window is movable (GTK isn't).

## 🔨 Build & run cheat-sheet (run in WSL)
```bash
# Build the firmware  ->  firmware/app/kernel8.img
cd /mnt/c/Workspace/Personal/Firmware-trial/firmware/app && make          # (make clean first if headers changed)

# Host unit tests (no hardware)
cd /mnt/c/Workspace/Personal/Firmware-trial && ./tools/run_host_tests.sh

# Make an SD/USB image from a photo folder (HEIC/JPG -> resized baseline JPEGs)
bash tools/make_sd_from.sh '/mnt/c/Ravi-Doc/Apple-Iphone-Backup/202103_a' firmware/app/sd.img
bash tools/make_sd_from.sh '<folder>' firmware/app/usb.img
# Or a synthetic 3-photo test card:
bash tools/make_sd.sh firmware/app/sd.img

# LIVE window (SDL, movable). 2nd arg optional sd.img:
export DISPLAY=:0
bash tools/run_qemu.sh firmware/app/kernel8.img firmware/app/sd.img
# Boot from USB pendrive live (no SD):
qemu-system-aarch64 -M raspi3b -kernel firmware/app/kernel8.img -serial null -display sdl \
  -drive file=firmware/app/usb.img,if=none,id=stick,format=raw -device usb-storage,drive=stick

# Headless screenshot:  <kernel> <out.png> <boot_secs> [sd.img|-] [usb.img]
bash tools/qemu_capture_png.sh firmware/app/kernel8.img /tmp/shot.png 8 firmware/app/sd.img
bash tools/qemu_capture_png.sh firmware/app/kernel8.img /tmp/shot.png 8 - firmware/app/usb.img   # USB only

# Serial boot log:
cd firmware/app && timeout 7 qemu-system-aarch64 -M raspi3b -kernel kernel8.img -serial stdio \
  -display none -drive file=sd.img,if=sd,format=raw 2>&1 | head -40
```
Note: boot sequence is paced (~5s) so it's watchable — screenshot content after ~8s. Logical
clock runs slower than wall-clock in QEMU (heavy per-tick redraw), so timings drift; fine.

## 📁 Code map (firmware)
```
firmware/app/        Circle app: main.cpp, kernel.{h,cpp}, CircleCanvas.h, SdPhotoSource.h, Makefile
firmware/src/core/   Result.h, Error.h, EventBus.h            (freestanding: C headers, no std::)
firmware/src/util/   RingBuffer.h, Sha256.{h,cpp}
firmware/src/display/ ICanvas.h (draw/scale/blend/present), ScreenPlugin.h
firmware/src/app/    PluginScheduler.h (playlist w/ time windows)
firmware/src/content/ IPhotoSource.h, JpegDecoder.{h,cpp}, ExifReader.{h,cpp}, stb_image_impl.c, test_image.h
firmware/src/plugins/ PhotoFramePlugin.{h,cpp} (slideshow+fade), ClockPlugin.{h,cpp}
firmware/vendor/     circle/ (gitignored), stb/ (committed)
tests/host/          unit tests + tiny framework, CMakeLists.txt
tools/               run_qemu.sh, qemu_capture_png.sh, make_sd.sh, make_sd_from.sh, run_host_tests.sh
```

## ⚠️ Gotchas / findings (save future debugging)
- **Freestanding rule:** shared src modules must use C headers (`<stdint.h>`, not `<cstdint>`)
  and no `std::` — Circle builds with `-nostdinc++ -fno-exceptions`. Same code compiles for host
  tests (full stdlib) and firmware.
- **Storage is optional:** `m_EMMC.Initialize()` returns false with no SD card — do NOT treat as
  fatal (it aborted boot once). Init graphics before storage; storage init is non-fatal.
- **QEMU networking:** the NIC IS recognized (`ucdceth`, RNDIS), but **RX is broken in stock
  QEMU** (`dhcp: No response`, and static IP still gets no incoming TCP). Circle networking in
  QEMU needs the **patched QEMU** at https://codeberg.org/larchcone/qemu (see
  `firmware/vendor/circle/doc/qemu.txt`), built with `--enable-slirp`. WiFi (CYW43) is never
  emulated → hardware only.
- **HEIC:** bare metal can't decode it. iPhone photos are transcoded to JPEG on the host
  (make_sd_from.sh); libheif applied their rotation so they come out upright (EXIF then a no-op,
  but ExifReader is unit-tested for tagged JPEGs).
- **PowerShell↔WSL quoting:** `$(...)`, `\$`, empty `""` args, and heredocs get mangled through
  the PowerShell→wsl→bash layers. Put nontrivial shell logic in a `tools/*.sh` script and call it
  with full-path args. The capture script uses `-` as a "no SD" sentinel (empty args get dropped).
- **Cross-fade** is plain per-pixel blend (slow/steppy in QEMU); TODO: NEON-optimize for hardware.

## 🧭 Bigger roadmap (see docs/ROADMAP.md)
Done: boot, plugins, JPEG, SD, USB, slideshow+fade+EXIF. Next candidates: WiFi+web UI (pending
decision above), **real hardware bring-up** (Spikes C1/C2/C5 on the Pi + Acer panel), OTA A/B via
tryboot (spike C5 unknown), NEON blend, Ken Burns, real clock (RTC/NTP), USB auto-import.

## 📚 Doc index
PRD.md · DESIGN.md · IMPLEMENTATION.md · SCENARIOS.md · GOALS.md · ROADMAP.md · DECISIONS.md
(ADR-001..010) · ../TESTPLAN.md · ../CHANGELOG.md
