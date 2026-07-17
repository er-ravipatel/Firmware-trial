# Roadmap

_Milestones toward [GOALS.md](GOALS.md). Full detail in [PRD.md](PRD.md)._

## Milestone 0 — De-risk spikes (do first)
**Target outcome:** the two scariest unknowns are proven on real hardware.
- [ ] **Spike R1:** pi3d fullscreen image on the actual screen (64-bit Pi OS DRM/GBM).
- [ ] **Spike R2:** RAUC A/B rollback on a scratch SD (bootcount reverts a bad slot).
- [ ] Measure & pin the panel's native HDMI resolution/timing.

## Milestone 1 — MVP: on the wall
**Target outcome:** boots to a slideshow from local photos; basic import & offline update.
- [ ] Base image: read-only rootfs, A/B + data partition, systemd, splash.
- [ ] frame-ui: fullscreen slideshow, EXIF rotate, cross-fade, HDMI pinned, no blanking.
- [ ] USB pendrive auto-import to data partition.
- [ ] SoftAP onboarding to join home WiFi.
- [ ] USB offline firmware update (signed RAUC bundle).
- [ ] Watchdog recovers a hung renderer.

## Milestone 2 — Product feel
**Target outcome:** looks and updates like a real product.
- [ ] Ken Burns pan/zoom + configurable transitions.
- [ ] Web admin UI (manage photos, transitions, schedule, status).
- [ ] Night sleep/wake schedule (display blank).
- [ ] OTA auto-update over WiFi with rollback + stable/beta channels.
- [ ] Clock/weather overlay; albums/playlists/shuffle/favorites.

## Milestone 3 — Delight layer
**Target outcome:** the features that make people say "wow."
- [ ] Web upload (drag-and-drop from phone).
- [ ] WiFi sync (Immich / SMB / Nextcloud).
- [ ] Face-aware smart crop; short video clips.
- [ ] Optional PIR wake / ambient-light brightness.

## Backlog / ideas
- Fleet dashboard for multiple frames.
- Email-to-frame; multi-frame sync.
