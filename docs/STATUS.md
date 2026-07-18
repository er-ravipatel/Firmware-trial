# RESUME HERE — Project Status & Handoff

_Last updated: 2026-07-19. Read this first when picking the project back up._

**What this is:** Lumen Frame — a digital photo frame built as a **genuine bare-metal firmware
OS in C++ on the Circle framework** (no Linux) for a Raspberry Pi Zero 2 W, driving an Acer
Aspire 4347 (14", 1366×768) LCD over HDMI. Broadened (InkyPi-inspired) into a modular smart
display: rotating screen plugins (photo, clock, …). Developed **emulator-first** in QEMU.

---

## ▶▶ RESUME HERE (next session) — v0.3 "Universal", in progress

**Currently building:** v0.3 (offline "any screen / any image"). Full plan: [PLAN-v0.3.md](PLAN-v0.3.md).
The whole **networking + onboarding half is DONE and hardware-proven** (SoftAP + DHCP + DNS + HTTP +
captive portal + on-device QR + a boot settings window). **Pick up at ONE of these two:**

1. **Settings web page content** — the web server currently serves a placeholder "connected" page
   (`firmware/src/net/webserver.cpp`). Build the real settings page + apply-on-restart, and route
   **all on-screen/web text through config** (the [[configurable-branding]] requirement: name,
   wordmark, tagline, credits, AP SSID — defaults in code → `lumen.conf` → web-editable).
2. **The conversion flow (core v0.3)** — PLAN phases 2b/2c/3: **HEIC detection** on a pendrive
   (classify displayable vs needs-conversion) → **Conversion-mode** trigger (replace nothing; it's a
   new state) → the **libheif-WASM conversion page** (phone converts + resizes) → **FAT write-back**
   to the pendrive → reboot-to-resume.

**Build/deploy (hardware = SDHOST; see BUILD NOTE below):**
`wsl bash -lc "cd /mnt/c/.../firmware/app && make -j4"` → `Copy-Item ...\app\kernel8.img D:\`.
**QEMU note:** the current build is **SDHOST** so QEMU can't read the SD image (no config/photos);
QEMU still renders (embedded image) — a `qrtest = on` config path and forcing `m_bNetUp=TRUE` were
used to screenshot QR screens. **Card state:** `D:\lumen.conf` = `logging = on` (wifi defaults on →
boot QR window). CYW43 firmware blobs live at `D:\firmware\` (needed for WiFi). Restore-to-normal
isn't needed anymore — the boot-QR settings window *is* the normal behavior now.

**Reusable spike/net code:** `firmware/src/net/` (dhcpd, dnsd, webserver) + `firmware/vendor/qrcodegen/`.
Debug hooks: `portal = on` (full-screen portal via `RunPortalMode`), `qrtest = on` (QR render, QEMU).

---

## 🏷 MILESTONE: v0.2.0-beta "smooth & polished" (2026-07-18)
Second milestone, still fully **offline**, on real hardware. The per-photo freeze is **gone** (decode
moved to CPU core 1), the boot is a **modern gradient splash** that dissolves into the slideshow, and
the intermittent USB-removal reboot is **fixed**. See CHANGELOG `[0.2.0-beta]`. Clean return point.
(Previous: v0.1.0-beta "offline" — first hardware bring-up; see CHANGELOG `[0.1.0-beta]`.)

## ▶ Where we are (works today, verified on HARDWARE)
- Boots our own OS → **gradient hero splash** (wordmark + credits, 10 s) → dissolves into the slideshow.
- **Photo slideshow** is the sole screen (clock plugin removed): scans SD/USB for `*.jpg`, **Fit +
  blurred background** (whole photo visible, no crop), **gentle Ken Burns**, cross-fade, **EXIF rotation**.
- **Multicore**: CPU **core 1 decodes the next photo in the background** while core 0 renders at
  ~40 ms/frame — no more per-photo freeze. Lock-free handshake via GCC atomic builtins; inline fallback.
- **JPEG decode** on bare metal via vendored **stb_image** (pool allocator; `STBI_NO_THREAD_LOCALS`
  is the critical bare-metal fix — see Gotchas).
- **Storage**: SD card (EMMC+FatFs) AND **USB pendrive** (USB host + MSD, hotplug in/out — removal is
  now crash-free, presence via device-name-service, no I/O on a vanished device). USB → SD → embedded.
- **SD file logging** (`SD:/lumenlog.txt`): appends across boots with ~1 MB rollover; gated by
  `logging=on` in `SD:/lumen.conf` (OFF by default = production), force-on for beta/dev builds.
- **Build identity** in `version.h` (`LUMEN_VERSION`/`LUMEN_MODE`/`LUMEN_BUILD`) shown on the splash.
- **21 host unit tests, 376 checks passing**.

## ⚠️ BUILD NOTE (multicore): `vendor/circle` is gitignored
The core-1 decode needs `-DARM_ALLOW_MULTI_CORE` in **`vendor/circle/Config.mk`** (alongside
`-DNO_SDHOST -DNO_SCREEN_DMA_BURST_LENGTH -DDEPTH=32`), then a **clean rebuild** of Circle:
`cd vendor/circle && ./makeall clean && ./makeall -j4`, plus clean+rebuild `addon/fatfs` and
`addon/SDCard`. Because Circle is gitignored, this flag is **not** in the repo — re-apply it after a
fresh Circle clone or the app won't link (`CMultiCoreSupport` is `#ifdef`-guarded).

## ▶ IN PROGRESS: v0.3 "Universal" — see [PLAN-v0.3.md](PLAN-v0.3.md)
**Spikes W1 + W2 + captive portal PASSED on hardware (2026-07-18):** bare-metal **SoftAP + DHCP +
DNS + HTTP**, and the setup page **auto-opens on both Android and iOS** — fully offline (TESTPLAN
V3-01/02, V3-08 partial). The gating risk is retired. Key fixes: **drop `-DNO_SDHOST`** (SD card on
SDHOST so EMMC is free for WiFi), a **hand-written DHCP server** and a **DNS responder** (Circle has
neither) — all validated code in `firmware/src/net/` (dhcpd, dnsd, webserver) + docs/LEARNINGS.md.
**Boot settings window DONE (2026-07-19):** every boot brings up the SoftAP + web server and shows
the splash with a **Wi-Fi-join QR in the bottom-left + a 10 s countdown** ("Slideshow starts in N").
If a phone joins (DHCP ACK sets `g_dhcpClientConnected`) → **settings mode** (serve the web page,
slideshow paused, restart to apply); otherwise the slideshow starts and the **AP stays responsive**
(cheap `m_CoopSched.Yield()` per frame — net+render+multicore coexistence verified on hardware). On-
device **QR encoder** = vendored Nayuki qrcodegen (MIT). Gated by `wifi` in lumen.conf (default on);
fails gracefully with no CYW43 (QEMU). This is the entry point for the settings web page.

**Step 1c DONE (2026-07-18):** the whole net stack (SoftAP + DHCP + DNS + HTTP + captive portal) is
now **integrated into the real kernel** (`RunPortalMode()`), gated by `portal = on` in `lumen.conf`.
Verified on hardware: normal slideshow mode unchanged (net members inert), and portal mode brings up
the AP + serves the setup page from our kernel. Needs SDHOST build (WiFi libs: `libwlan/libnet/
libsched`; net modules in `firmware/src/net/`). **Next:** (2) on-device **QR encoder** + **HEIC-
detection trigger** (replace the `portal` flag) + **Conversion-mode** state machine; (3) **libheif-
WASM conversion page + FAT write-back** to the pendrive, then reboot-to-resume. Full plan below.


Two pillars, **fully offline** (no internet): **(A) display-agnostic** — EDID auto-detect + safe
fallback so it fills any HDMI screen (ADR-011); **(B) image-agnostic** — a pendrive with HEIC pops
an on-screen **QR** → phone joins the Pi's **SoftAP** → converts in the **phone browser**
(libheif-WASM) → JPEGs written **back to the pendrive** (ADR-012); plus PNG/GIF/BMP on-device
(ADR-013). **Start with Spike W1 (CYW43 SoftAP bring-up)** — it's the gate and is hardware-only
(unemulatable); Circle `addon/wlan/sample/hello_ap` shows it's possible. Full phased plan,
risks, and pass/fail in PLAN-v0.3.md.

_Deferred past v0.3: station-mode WiFi / home-LAN web admin, TLS (mbedTLS), OTA, network plugins._

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
