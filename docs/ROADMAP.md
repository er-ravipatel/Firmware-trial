# Roadmap

_Milestones toward [GOALS.md](GOALS.md). Full detail in [PRD.md](PRD.md)._
_Architecture: bare-metal C++ firmware on Circle (no Linux)._

## Milestone 0 — Environment & de-risk spikes (do first)
**Target outcome:** the toolchain works and the scariest bare-metal unknowns are proven.
- [ ] Set up **WSL2** + `aarch64-none-elf` toolchain; build a stock Circle sample to SD.
- [ ] **Spike C1:** Circle boots on the Zero 2 W; HDMI framebuffer shows a test image on the
      Acer panel through the TV board (pin 1366×768 timing).
- [ ] **Spike C2:** read a JPEG from SD (FatFs) + ported libjpeg → display it.
- [ ] **Spike C5:** A/B kernel swap via Pi `tryboot` + auto-rollback on a scratch SD.

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
- [ ] ScreenPlugin architecture wired into the render engine (interface + PluginScheduler done).
- [ ] **Clock/date plugin** (local, FreeType) — the first non-photo plugin.
- [ ] Web UI: enable/disable/schedule plugins (per-slot duration + time windows).

## Milestone 5 — Plugins: network (TLS unlock)
**Target outcome:** internet-driven screens.
- [ ] **Shared TLS+JSON+HttpClient layer** (mbedTLS port) — ADR-009, the one-time unlock.
- [ ] **Weather** plugin.
- [ ] **Calendar/agenda** plugin (CalDAV/Google).
- [ ] **News / stocks / fun** plugins (headlines, ticker, XKCD, word-of-the-day).

## Backlog / ideas
- SQLite port for the index; fleet update dashboard; multi-frame sync.
