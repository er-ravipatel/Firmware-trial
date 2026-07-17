# Digital Photo Frame Firmware — Product & Engineering Spec

> Working name: **Lumen Frame** (rename freely). This is the "detailed prompt" / master
> spec for the project. It captures vision, decisions, architecture, and requirements.
> Living document — update as we learn. Assumptions and open questions are marked **⚠️**.

---

## 1. Vision & principles

A **digital photo frame appliance** built on a Raspberry Pi Zero 2 W driving a repurposed
laptop LCD via a generic HDMI TV driver board. It should feel like a polished consumer
product, not a hobby Pi:

- **It just works.** Plug in power → guided WiFi setup → photos appear.
- **Effortless content.** Add photos by SD card, USB pendrive, web upload, or WiFi sync.
- **Self-maintaining.** Auto-updates over the air, atomically, with automatic rollback.
- **Resilient.** Survives power loss and bad updates; recovers itself.
- **Beautiful.** Smooth Ken Burns pan/zoom and cross-fades, optional clock/weather overlay.

**Design principles:** offline-first · fail-safe over feature-rich · one signed artifact for
all update paths · keep the 512 MB RAM budget sacred · no cloud lock-in.

---

## 2. Target hardware & environment

| Item | Value |
|---|---|
| Compute | Raspberry Pi Zero 2 W (quad Cortex-A53 @ 1 GHz, **512 MB RAM**) |
| Display | Scrap laptop LCD + generic HDMI TV driver board, over HDMI |
| Native resolution | **⚠️ TBD — pending laptop model; detect via EDID at runtime + manual override in web UI** |
| Input media | microSD (boot + data), USB pendrive via micro-USB OTG adapter |
| Network | 2.4 GHz WiFi (onboard), BT unused for now |
| Internet available? | **Usually, not guaranteed → offline-first; opportunistic OTA/sync, USB is the guaranteed update path** |
| Sensors (optional) | PIR motion, ambient light — **⚠️ not present unless added** |
| Screen power control | Display blanking / DPMS (HDMI-CEC unlikely via generic board) |

---

## 3. Personas & key journeys

**Primary persona:** a non-technical family member who wants photos on the wall.

1. **First-boot onboarding** — Device powers on → shows a splash + on-screen instructions and
   QR code → broadcasts its own WiFi hotspot (SoftAP) → user connects with phone → captive
   portal opens → picks home WiFi + enters password → device joins network, hotspot closes,
   slideshow starts.
2. **Add photos (local)** — Insert pendrive → device detects it, copies new images to the data
   partition, shows an on-screen "imported N photos" toast → resumes slideshow.
3. **Add photos (remote)** — Open `http://lumenframe.local` on phone → drag & drop photos →
   they appear in rotation.
4. **Night mode** — At a scheduled hour the screen blanks to save the panel & power; wakes on
   schedule (or on PIR motion if fitted).
5. **Auto-update** — Overnight the frame checks for firmware, downloads a signed bundle to the
   inactive slot, reboots into it, self-tests, and either confirms or rolls back — all unattended.

---

## 4. Confirmed technical stack (see DECISIONS.md for rationale)

| Layer | Choice |
|---|---|
| Base OS | Raspberry Pi OS Lite **64-bit**, read-only rootfs |
| Partitioning | A/B rootfs slots + separate persistent **data** partition |
| Update engine | **RAUC** — signed bundles, atomic A/B, auto-rollback |
| Update orchestrator | **Go** `frame-agent` — poll/USB-ingest/health-confirm/rollback |
| Renderer | **Python + pi3d** (OpenGL ES) — Ken Burns, cross-fade, overlays |
| App services | **Python (FastAPI)** — web admin UI, content manager, onboarding |
| Onboarding | SoftAP + captive portal (hostapd + dnsmasq) + on-screen QR |
| Service mgmt | systemd units, watchdog, boot splash (plymouth or fbi) |

---

## 5. System architecture

```
frame-ui (pi3d)  ──reads──►  data partition (/data/photos, /data/thumbs)
     ▲                                 ▲
     │ IPC/state file                  │ writes
frame-services (FastAPI) ──────────────┘
     ├─ content-manager: import, EXIF, orient, thumbnail, index (SQLite)
     ├─ web-admin: manage photos, transitions, schedule, status
     ├─ onboarding: SoftAP captive portal, WiFi provisioning
     └─ scheduler: sleep/wake, playlist/album rotation
frame-agent (Go)
     ├─ update: poll endpoint / ingest USB bundle → RAUC install → reboot
     ├─ health: post-boot self-test → rauc mark good / bad (rollback)
     └─ watchdog + telemetry (local; optional remote report)
base image: bootloader(tryboot/bootcount) · RAUC · read-only rootfs · systemd
```

**Component responsibilities**
- **frame-ui** — the only thing that draws to the screen; owns transitions & overlays; reads
  the photo index and a small shared "control" state (pause/next/album).
- **content-manager** — the only writer of the photo index; normalizes formats, corrects EXIF
  orientation, generates thumbnails, deduplicates, prunes.
- **web-admin** — local network UI (no internet needed) for management.
- **onboarding** — runs only until WiFi is configured; then yields.
- **frame-agent** — everything update/health/rollback; compiled, small, robust.

---

## 6. Feature requirements

### MVP (v0.1 — "on the wall")
- [ ] Boots to fullscreen slideshow from photos on the SD **data** partition.
- [ ] EXIF orientation respected; fit-to-screen with letterbox; basic cross-fade.
- [ ] HDMI mode pinned to the panel's native resolution; no blanking during show.
- [ ] USB pendrive auto-detected; new images imported to data partition.
- [ ] SoftAP onboarding to join home WiFi.
- [ ] USB **offline** firmware update: drop a signed RAUC bundle on a pendrive → installs.
- [ ] Watchdog recovers a hung renderer.

### v1 (product-feel)
- [ ] Ken Burns pan/zoom + configurable transitions & timing.
- [ ] Web admin UI: view/reorder/delete photos, set interval/transition, schedule, status.
- [ ] Night sleep/wake schedule (display blank).
- [ ] OTA auto-update over WiFi with auto-rollback + stable/beta channels.
- [ ] Clock + weather overlay (optional, per-user toggle).
- [ ] Albums/playlists, shuffle, favorites.

### Later (delight / inventor layer)
- [ ] Web upload (drag-and-drop from phone).
- [ ] WiFi sync from Immich / SMB / Nextcloud / Google Photos.
- [ ] Face-aware smart cropping.
- [ ] Short video clip playback (hardware-decoded).
- [ ] PIR motion wake; ambient-light auto-brightness (needs hardware).
- [ ] Fleet dashboard for multiple frames.

---

## 7. Key subsystem designs

### 7.1 Boot, partitions & rollback
- Partitions: `boot` · `rootfs.A` · `rootfs.B` · `data` (persistent, survives updates).
- Root filesystem mounted **read-only**; writable state confined to `/data` + tmpfs overlays.
- Rollback: bootloader boots the "try" slot with a bootcount/`tryboot` guard. `frame-agent`
  runs a post-boot **health self-test** (renderer up? services up? disk ok?) and calls
  `rauc status mark-good`. If it never confirms (crash/hang), the next boot reverts to the
  known-good slot automatically.

### 7.2 Update flows (one signed bundle, three triggers)
- **Artifact:** a single **signed** RAUC bundle (`.raucb`). Same file for every path.
- **OTA pull:** `frame-agent` periodically checks an update endpoint (URL + channel). If a
  newer signed bundle exists → download to inactive slot → install → schedule reboot in the
  night window.
- **USB offline:** pendrive containing `*.raucb` → agent verifies signature → installs.
- **Auto-update policy:** configurable (auto / notify-only / manual); default auto within the
  night window; never mid-slideshow without the window unless user forces. Since internet is
  **not guaranteed**, OTA is **opportunistic** (checks whenever online); **USB offline update is
  the guaranteed delivery path** and must always work with zero network.
- **Safety:** signature required; free-space & power checks before install; atomic swap;
  rollback on failed health check.

### 7.3 Onboarding (SoftAP captive portal)
- On first boot / no known WiFi: start `hostapd` AP `LumenFrame-XXXX` + `dnsmasq` +
  captive-portal web page. On-screen: SSID, and a **QR code** that encodes the WiFi join.
- User submits home SSID/password → stored securely on `data` → AP torn down → join network →
  slideshow starts. Re-enters onboarding if WiFi is lost for a config'd grace period.

### 7.4 Renderer (pi3d)
- Fullscreen GLES; **⚠️ validate the pi3d DRM/GBM backend on 64-bit Pi OS early** (spike).
  Fallback: SDL2/ModernGL.
- Transitions: cross-fade (MVP), Ken Burns pan/zoom (v1). Per-image duration; smart fit.
- Overlays: optional clock/weather/date drawn as a top layer.
- Reads photo index + a lightweight control channel (file/socket) for pause/next/album.

### 7.5 Content pipeline
- Import sources normalized into `/data/photos`; index in **SQLite**.
- Steps: detect new → validate/decode (**JPEG, PNG, HEIC, WebP** — HEIC via a libheif/pillow-heif
  dependency for iPhone photos) → fix EXIF orientation → generate thumbnail → dedupe (hash) →
  add to index. Prune on delete. Large images pre-scaled to panel resolution on import to save RAM.
- Memory-aware: never hold more than N decoded frames; pre-scale large images to panel size.

### 7.6 Web admin UI
- FastAPI + a small static frontend; served on LAN; mDNS `lumenframe.local`.
- Manage photos, transitions, interval, schedule; show device/update status; trigger
  update check; (later) upload.
- Auth: simple local password set during onboarding.

### 7.7 Scheduler & power
- Cron-like schedule for sleep/wake → display blank via DPMS/`vcgencmd display_power`.
- Optional PIR wake. Reduces panel burn-in risk & power.

### 7.8 Health, watchdog, telemetry
- systemd watchdog + hardware watchdog; restart crashed units.
- Local health page; optional anonymized remote status for fleet (opt-in).

---

## 8. Non-functional requirements
- **RAM budget:** renderer + services + cache must leave headroom on 512 MB; target < ~350 MB
  resident under load. Pre-scale images; cap thumbnail cache.
- **Boot time:** target under ~25 s from power to first photo (stretch: < 15 s).
- **Reliability:** survive sudden power loss at any time (read-only rootfs; journaled data FS);
  no corruption of the photo index (SQLite WAL).
- **Update safety:** no update can brick the device (atomic A/B + rollback).
- **Offline:** all core features (local photos, slideshow, USB import/update) work with no
  internet.

---

## 9. Security model
- OTA/USB bundles **must be cryptographically signed**; unsigned/invalid rejected.
- WiFi credentials & web password stored with restricted permissions on `data` (encrypt ⚠️ TBD).
- Web admin behind a password; no default open SSH; SSH keys only if enabled.
- Read-only rootfs limits tampering & corruption.
- Update endpoint over HTTPS with pinned/verified cert.

---

## 10. Proposed repo structure
```
/firmware        # image build: partitioning, RAUC config, systemd units, config.txt
/frame-ui        # Python + pi3d renderer
/frame-services  # Python FastAPI: content-manager, web-admin, onboarding, scheduler
/frame-agent     # Go: update + health + watchdog
/tools           # bundle signing, mender/rauc helpers, dev flashing scripts
/docs            # this spec + goals/roadmap/decisions/etc.
/tests           # unit + on-target smoke tests
```

---

## 11. Open questions / assumptions ⚠️
1. **Panel native resolution & HDMI timing** — ⚠️ still open, pending laptop model. Renderer
   detects the mode at runtime with a manual override, so build is not blocked.
2. ~~Is internet available?~~ **Resolved:** usually-but-not-guaranteed → offline-first;
   opportunistic OTA/sync; USB is the guaranteed update path.
3. ~~Image formats?~~ **Resolved:** JPEG, PNG, HEIC, WebP (HEIC via libheif/pillow-heif).
4. **Where will OTA bundles be hosted?** (self-hosted static HTTPS vs a service.)
5. **Any sensors** (PIR/light) to design for, or software-only v1?
6. **Single frame or a fleet** eventually? (affects telemetry/dashboard investment.)
7. **Encryption at rest** for credentials — required or password-permissions enough?

---

## 12. Risks & first spikes
- **R1 (high): pi3d on 64-bit Pi OS DRM/GBM.** → Spike a fullscreen image test on real hardware
  *before* building the renderer. Fallback: SDL2/ModernGL.
- **R2 (med): RAUC A/B on the Pi bootloader.** → Spike a bootcount rollback on a scratch SD.
- **R3 (med): junk TV board EDID/timing.** → Pin `hdmi_group/hdmi_mode` (or `cmdline` video=)
  explicitly; test hotplug behavior.
- **R4 (low): 512 MB RAM under large images.** → Enforce pre-scaling + cache caps from day one.
