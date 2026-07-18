# Changelog

_All notable changes to this project. Newest first._
_Format loosely follows [Keep a Changelog](https://keepachangelog.com/)._

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
