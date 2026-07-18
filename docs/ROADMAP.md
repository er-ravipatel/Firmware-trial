# Roadmap

_Milestones toward [GOALS.md](GOALS.md). Full detail in [PRD.md](PRD.md)._
_Architecture: bare-metal C++ firmware on Circle (no Linux)._

## Milestone 0 — Environment & de-risk spikes (do first)
**Target outcome:** the toolchain works and the scariest bare-metal unknowns are proven.
_Emulator-first (ADR-010): prove in QEMU, then confirm on real hardware._
- [x] Set up **WSL2** + host toolchain (g++/cmake) + host unit-test harness.
- [x] Install **QEMU** (`raspi3b`) + `aarch64-linux-gnu` cross-toolchain; clone Circle.
- [x] **Spike C1 (emulator):** Circle boots in QEMU; framebuffer renders (verified via headless
      screendump → PNG). Loop proven: cross-compile → kernel8.img → QEMU raspi3b → captured image.
- [ ] **Spike C1 (hardware):** same on the Zero 2 W → Acer panel via TV board (pin 1366×768).
- [ ] **Spike C2:** read a JPEG from SD (FatFs) + ported libjpeg → display it (QEMU SD image first).
- [ ] **Spike C5 (hardware only):** A/B kernel swap via Pi `tryboot` + auto-rollback.

## Milestone 1 — MVP: genuine firmware on the wall
**Target outcome:** boots our OS to a slideshow from local photos; USB import; offline update.
- [ ] Image layout: boot + A/B kernel slots + FAT data area.
- [ ] Render engine: fullscreen slideshow, EXIF rotate, cross-fade (NEON).
- [ ] USB pendrive auto-import (Circle USB MSD) → FAT data area.
- [ ] `wifi.conf` onboarding; WLAN station joins WiFi (Spike C4 — also test SoftAP feasibility).
- [ ] USB offline signed firmware update with A/B + rollback.
- [ ] Watchdog recovers a hung render loop.

## Milestone 2 — Product feel
**Target outcome:** looks and updates like a real product.
- [ ] Ken Burns (NEON scaler) + configurable transitions (Spike C6 perf).
- [ ] Web admin UI (Circle HTTP): manage photos/settings/schedule/status.
- [ ] **HEIC upload transcode** (browser-side) + companion PC converter.
- [ ] Night sleep/wake (framebuffer blank).
- [ ] OTA auto-update over WiFi (opportunistic) + rollback + channels.
- [ ] Clock overlay (FreeType); PNG/WebP; albums/shuffle/favorites.

## Milestone 3 — Delight layer
- [ ] WiFi sync (Immich/SMB); short MJPEG clips.
- [ ] Optional PIR wake / ambient-light brightness.

## Milestone 4 — Plugins: local (modular smart display)
**Target outcome:** more than one screen, rotated on a schedule via the web UI (InkyPi-style).
- [x] ScreenPlugin architecture wired into the render engine (ICanvas/C2DGraphics + PluginScheduler),
      running in firmware in QEMU: PhotoFrame + Clock cycling on a timer.
- [ ] **Clock/date plugin** — upgrade from "time since boot" to real time (RTC/NTP).
- [ ] Web UI: enable/disable/schedule plugins (per-slot duration + time windows).

## Milestone 5 — Plugins: network (TLS unlock)
**Target outcome:** internet-driven screens.
- [ ] **Shared TLS+JSON+HttpClient layer** (mbedTLS port) — ADR-009, the one-time unlock.
- [ ] **Weather** plugin.
- [ ] **Calendar/agenda** plugin (CalDAV/Google).
- [ ] **News / stocks / fun** plugins (headlines, ticker, XKCD, word-of-the-day).

## Backlog / ideas
- SQLite port for the index; fleet update dashboard; multi-frame sync.
