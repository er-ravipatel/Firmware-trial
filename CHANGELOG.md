# Changelog

_All notable changes to this project. Newest first._
_Format loosely follows [Keep a Changelog](https://keepachangelog.com/)._

## [0.2.0-beta] — 2026-07-18 — "smooth & polished"
Second milestone (still fully **offline**): the slideshow is now buttery — the per-photo freeze is
gone — and the product feels finished on boot. Focus was smoothness, a premium boot experience, and
robustness of the storage hotplug paths.

### Added
- **Multicore background decode (the jerk fix):** enabled `ARM_ALLOW_MULTI_CORE`; **CPU core 1 is now
  a dedicated JPEG decoder**. The render loop (core 0) never decodes inline — it posts a job and keeps
  rendering at ~40 ms/frame while core 1 decodes+scales+blurs the next photo. The 440–840 ms freeze
  that hit once per photo is eliminated. Lock-free handshake via GCC atomic builtins (acquire/release)
  keeps the plugin freestanding while emitting correct ARM memory barriers; graceful inline fallback if
  multicore is ever unavailable.
- **Modern gradient boot splash:** a rich, opaque diagonal 3-stop gradient (deep indigo → wine-magenta
  → plum, dithered so it's band-free) as a web-hero-style background, with the wordmark, tagline
  **"Memory Lane Walkthrough"**, and credit line **"Thought by Vikash & Guided by Ravi & Written by
  Claude"** on top. Fades in, holds 10 s (readable), then dissolves straight into the first photo.
- **Build/mode identity:** `version.h` now carries `LUMEN_MODE` ("Offline-only") and `LUMEN_BUILD`
  (compiler build date); a small bottom-right label shows `v0.2.0-beta | Offline-only | build <date>`.
  Beta/dev version strings force diagnostic logging on.
- **Config-gated SD logging:** logging is OFF by default (production), enabled via `logging = on` in
  `SD:/lumen.conf`, and force-on for any beta/dev build. Log now **appends across boots** (each run
  separated by the boot banner) with a ~1 MB rollover cap so it can never fill the card.

### Changed
- **Removed the clock plugin** — the photo slideshow is the sole screen; plugin rotation is skipped
  when there's only one screen (avoids a needless dwell restart).
- Boot is silent on screen now (the `[ok]` steps go only to the SD log); the splash covers boot.

### Fixed
- **Intermittent USB-removal reboot (~1 in 7):** on a plug-and-play event the old handler forced a
  FatFs mount (`f_mount "USB:",1`) that read sector 0 — racing Circle's device teardown on removal and
  data-aborting when it lost the race. Now presence is decided by a **name-service lookup (no device
  I/O)**; mount/scan runs only on a confirmed *insert*, and removal touches nothing on the vanished
  device. Diagnosed from the field log (7 removals, 6 clean, 1 crash, no panic dump).

## [0.1.0-beta] — 2026-07-18 — "offline"
First milestone running on **real hardware**: our own bare-metal OS boots a Raspberry Pi Zero 2 W
directly (`kernel8.img`, no Linux) and drives the Acer Aspire 4347 LCD over HDMI as a polished,
fully **offline** photo frame. Reads photos from SD card and/or USB pendrive, decodes JPEG on bare
metal, and displays a fullscreen Ken Burns slideshow with cross-fade at ~21 fps.

### Added
- **Fit + blurred-background mode:** the whole photo is always visible (no cover-crop); empty space
  is filled with a soft, darkened blurred copy of the same image (48×27 thumbnail → bilinear
  upscale, one-time per photo). Gentle Ken Burns (zoom 1.00→1.06) on the sharp foreground.
- **SD-card file logging:** CFileLogDevice retargets the Circle logger to `SD:/lumenlog.txt`
  (f_write + f_sync), so on-hardware boot/exception/perf output survives a power cycle and can be
  read back on a PC — the primary hardware-debugging channel.
- **Per-photo + per-second perf instrumentation** in the render loop (decode/scale ms, fps,
  render/present avg+max, fade frames) written to the SD log.
- **USB hotplug (insert *and* remove):** switches to a pendrive when inserted mid-run and falls
  back to the SD card when it's pulled (releases the gone volume, re-scans SD).

### Fixed
- **THE decode hang on hardware:** stb_image's `__thread` locals emitted `mrs x0, tpidr_el0`, which
  faults on bare metal (uninitialized TLS register) → froze at "decode embedded". Fixed with
  `STBI_NO_THREAD_LOCALS` (also `STBI_NO_SIMD`). Found via objdump of the faulting PC.
- **Out-of-memory after ~24s:** Circle's heap `DoFree` only reclaims blocks ≤512 KB, so large
  decode buffers leaked. Fixed with a fixed 96 MB bump-allocator pool for stb + reused work buffers
  (no per-photo malloc).
- **Colors / banding:** rebuilt Circle at `DEPTH=32` (16-bit caused banding); confirmed the real Pi
  framebuffer is RGB (pass-through) while QEMU's is BGR — so emulator-only swap, hardware correct.
- **USB hotplug crash** (`assertion m_bPlugAndPlay`): construct `CUSBHCIDevice` with
  `bPlugAndPlay = TRUE`.
- **Slow decode** (~3.8 s on 29 MP photos): stage photos resized to 1920 px (`tools/_resize_dir.sh`)
  → ~290 ms.
- **Slow render** (122 ms / 7 fps): replaced per-pixel `DrawPixel` with direct 32-bit back-buffer
  writes via `GetBuffer()` → ~41 ms / 21 fps.

### Known limitations (offline beta)
- Occasional motion hitch (~300–450 ms) every ~10 s while the next photo is decoded on the same
  core. Planned fix: background decode on core 1. No cropping/quality regression — just a stutter.
- Offline only: **WiFi + web UI not yet built** (that's the next milestone).

## [Unreleased]
### Added
- Host foundation code (C++17, WSL2-built): Result/Error, EventBus, RingBuffer, SHA-256 (NIST
  vectors), and the plugin-ready ScreenPlugin interface + PluginScheduler playlist — 17 unit
  tests, 371 checks, all passing.
- Modular smart-display reframe (InkyPi-inspired): ADR-008 (ScreenPlugin architecture),
  ADR-009 (shared TLS+JSON layer); roadmap M4 (local plugins) + M5 (network plugins). Photo-first.
- Test plan (TESTPLAN.md): 48 traceable test rows (P0 safety / P1 core / P2 delight) mapped from
  scenarios and spikes, with pass criteria, types (unit/target/HIL), and a test-first order.
- Behavioral scenario catalog (docs/SCENARIOS.md): 50+ Given/When/Then scenarios for the deployed
  product (online/offline, power loss, updates, network, faults, security, recovery) → feeds tests.
- Deep subsystem design (docs/DESIGN.md): boot/image layout, content pipeline, render engine,
  and OTA/A-B update — with Circle APIs, ported libs, and the dependent spikes.
- Project scaffolding: CLAUDE.md, guardrails, goals/roadmap/decisions/outcomes docs,
  test plan, retrospective & learnings logs.
- Product & engineering spec (docs/PRD.md) for the Lumen Frame digital photo frame.
- Filled in Goals, Roadmap, and Decisions (ADR-001..003) from the planning session.

### Added (build)
- Emulator bring-up: QEMU (raspi3b) + aarch64 toolchain + Circle configured/built. **Spike C1
  (emulator) PASSED** — Circle boots in QEMU and renders to the framebuffer (headless screendump
  → PNG). Helpers: tools/run_qemu.sh, tools/qemu_capture_png.sh.
- **Our first firmware app** (firmware/app/): a Circle CKernel that boots and draws the Lumen
  Frame boot splash (border, accent bar, dynamic resolution text, color swatches). Verified
  rendering in QEMU — *our* code booting, not Circle's sample.
- **Render loop + plugin architecture running in firmware:** ICanvas (device-neutral surface)
  backed by C2DGraphics (double-buffered, pixel-accurate text); PhotoFrame + Clock ScreenPlugins
  rotated by the (host-unit-tested) PluginScheduler on a timer. Verified in QEMU: photo screen
  and live clock cycling every 2s. The tested host logic now drives real rendered frames.
- Made the shared logic modules truly freestanding (C headers, no std::) so the same code
  compiles for host unit tests and the bare-metal (-nostdinc++, -fno-exceptions) firmware.
- **Real JPEG decode on bare metal (Spike C2 decode):** vendored stb_image, building under
  Circle's flags; JpegDecoder wrapper; ICanvas.blit_rgb. PhotoFrame plugin now decodes and
  displays an embedded baseline JPEG in QEMU. tools/gen_test_asset.sh generates the test image.
- **JPEG from SD card (Spike C2 storage):** wired Circle EMMC + FatFs; kernel mounts the SD and
  loads /photo.jpg, passed to the PhotoFrame plugin (embedded image as fallback). Verified in
  QEMU with a FAT image (tools/make_sd.sh). Lights up the SD/FatFs path USB import & OTA reuse.
- **Multi-photo slideshow with scaling:** IPhotoSource abstraction + CSdPhotoSource (scans SD
  for *.jpg); PhotoFrame cycles all photos, decoding + aspect-fit scaling (ICanvas.blit_rgb_scaled)
  with letterbox/pillarbox. Verified in QEMU across 3 photos of different sizes/orientations.
- **Visible boot sequence** on the framebuffer (title + paced [ok] init steps: framebuffer, SD
  mount, photo scan, plugins) before the slideshow — watchable in a live QEMU window (WSLg).
  tools/run_qemu.sh opens an SDL window with the SD card attached.
- **docs/STATUS.md** — a "resume here" handoff doc: current state, environment, build/run
  cheat-sheet, code map, gotchas, and the pending WiFi/web-UI decision. Finding: stock QEMU can't
  run Circle networking (usb-net RX broken → needs patched QEMU); WiFi is hardware-only (no CYW43).
- **USB pendrive support:** Circle USB host + mass-storage stack integrated; kernel tries a USB
  drive first (USB:/), falls back to SD (SD:/), then the embedded image. Storage is now optional
  (boot no longer aborts without an SD card). Verified in QEMU: detects the pendrive, mounts FAT,
  runs the slideshow from it. Same CSdPhotoSource scans either drive.
- **Cross-fade transitions + EXIF rotation:** PhotoFrame is now a self-advancing slideshow with
  dwell + dissolve (pre-rendered frame buffers, ICanvas.blit_rgb_blend). ExifReader (unit-tested,
  LE/BE) + rotation applied at decode so phone photos display upright. Verified in QEMU with real
  iPhone photos (HEIC transcoded via tools/make_sd_from.sh). 21 host tests, 376 checks passing.

### Changed
- **Major pivot:** re-architected from a Linux appliance to a **genuine bare-metal firmware OS
  in C++ on Circle** (no Linux). Rewrote PRD, roadmap, and goals; superseded ADR-001..003 with
  ADR-004..007 (Circle bare-metal, C++ software renderer, HEIC boundary transcode).

### Fixed
- _..._
