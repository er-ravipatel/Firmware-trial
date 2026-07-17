# Digital Photo Frame — Bare-Metal Firmware Spec

> Working name: **Lumen Frame**. This is the master spec / "detailed prompt."
> **Architecture: a genuine bare-metal firmware OS** written in **C++** on the
> [Circle](https://github.com/rsta2/circle) framework — **no Linux**. The Pi boots straight
> into our `kernel8.img`. Living document; assumptions & open items marked **⚠️**.

---

## 1. Vision & principles

A **from-scratch firmware OS** that turns a Raspberry Pi Zero 2 W + a repurposed Acer Aspire
4347 (14", 1366×768) LCD (via a generic HDMI TV board) into a polished digital photo frame.

- **It is the firmware.** No distro, no Linux, no systemd — the Pi boots our C++ OS directly.
- **It just works.** Power on → photos on screen. Configuration is simple and file/web based.
- **Effortless content.** SD card, USB pendrive, web upload, and WiFi sync.
- **Self-maintaining.** Custom A/B OTA with rollback; USB is the guaranteed update path.
- **Resilient.** Survives power loss and bad updates without bricking.

**Principles:** authenticity (real firmware) · offline-first · fail-safe over feature-rich ·
guard the 512 MB RAM budget · one signed artifact for all update paths · no cloud lock-in.

---

## 2. Target hardware & environment

| Item | Value |
|---|---|
| Compute | Raspberry Pi Zero 2 W (BCM2837, quad Cortex-A53, **512 MB RAM**); Circle `RASPPI=3`, AArch64 |
| Display | Acer Aspire 4347 (14") LCD + generic HDMI TV driver board, over HDMI |
| Native resolution | **1366×768 (WXGA HD, 16:9)** — confirm via panel sticker/EDID; pin HDMI timing to match |
| Input media | microSD (boot + FAT data), USB pendrive via micro-USB OTG adapter |
| Network | 2.4 GHz WiFi (CYW43-class, SDIO) — **station mode** via Circle WLAN |
| Internet | Usually, not guaranteed → offline-first; opportunistic OTA/sync; **USB is guaranteed** |
| Sensors (optional) | PIR motion, ambient light — **⚠️ not present unless added** |
| Screen power | Framebuffer blank / HDMI power off (no HDMI-CEC via generic board) |

---

## 3. Foundation: Circle bare-metal (what we get vs. what we build)

**Circle provides (the mountains, already climbed, in C++):**
- HDMI **framebuffer** (mailbox), **timers**, **interrupts**, screen/log.
- **EMMC/SD** + **FatFs** filesystem.
- **USB host stack** incl. **mass storage** (pendrive) and HID.
- **WLAN station** (CYW43 firmware) + **lwIP TCP/IP** + **wpa_supplicant** (WPA2).
- Sample **HTTP server** (basis for the web admin UI).
- `circle-stdlib`: C/C++ std library (newlib + STL), **FreeType** (text), and the ability to
  link ported C libraries (libjpeg/libpng/libwebp, mbedTLS).

**We build (the application layer, all C++):**
- Slideshow/render engine (software blit + transitions).
- Image decode pipeline (ported libjpeg/libpng/libwebp) + EXIF orientation + pre-scale.
- Content index (flat/JSON on FAT, or ported SQLite), import logic, dedupe, thumbnails.
- Web admin UI (served over Circle's HTTP) + HEIC client-side transcode page.
- Config/onboarding (`wifi.conf` + web reconfigure).
- Custom A/B OTA agent (in-firmware) using the Pi bootloader's `tryboot`.
- Scheduler (sleep/wake), watchdog integration, health/rollback confirm.

**⚠️ To validate / port (not guaranteed):** SoftAP mode (probably unsupported → config-file
onboarding); HTTPS/TLS via mbedTLS (for weather + secure OTA); SQLite port (else flat index).

---

## 4. HEIC / iPhone photos — boundary transcode

On-device bare-metal cannot decode HEIC (needs an HEVC decoder). We keep iPhone photos by
transcoding **before they reach the frame**, where a decoder already exists:
1. **Phone-browser transcode (primary):** the web-upload page converts HEIC→JPEG in the
   browser (Safari decodes HEIC; canvas→JPEG) before upload. Frame receives JPEG only.
2. **Companion PC converter:** drag-and-drop tool to prep SD/pendrive.
3. **Network source:** WiFi sync source serves JPEG.

The frame stores/displays **JPEG, PNG, WebP** only.

---

## 5. System architecture (single C++ firmware image)

```
kernel8.img  (our Circle-based firmware OS)
├─ boot/init: EL2→EL1, MMU, caches, interrupts, HDMI framebuffer, watchdog
├─ drivers (Circle): SD/FatFs · USB(MSD) · WLAN+lwIP · timers
├─ render engine: slideshow loop, software blit, cross-fade, Ken Burns (NEON), overlays
├─ content pipeline: decode(JPEG/PNG/WebP) · EXIF · pre-scale · index · thumbnails · dedupe
├─ services: HTTP web-admin · WiFi provisioning · scheduler(sleep/wake)
├─ update agent: poll(WiFi)/ingest(USB) → verify signature → stage slot → tryboot → confirm/rollback
└─ config/state on FAT data area: wifi.conf, settings, photo index, /photos, /thumbs
```

Single-image firmware: all of the above compiles into one `kernel8.img`. No processes/OS —
Circle's task/scheduler primitives coordinate concurrency (render loop, net, update checks).

---

## 6. Feature requirements

### MVP (v0.1 — "genuine firmware, on the wall")
- [ ] Circle boots on Zero 2 W; HDMI framebuffer at 1366×768 on the Acer panel.
- [ ] Read JPEGs from SD (FatFs); fullscreen slideshow with EXIF rotate + basic cross-fade.
- [ ] USB pendrive auto-detected; new JPEG/PNG imported to the FAT data area.
- [ ] `wifi.conf` onboarding; WLAN station joins home WiFi.
- [ ] USB **offline** firmware update (signed bundle) with A/B + rollback.
- [ ] Watchdog recovers a hung render loop.

### v1 (product-feel)
- [ ] Ken Burns pan/zoom (NEON software scaler) + configurable transitions/timing.
- [ ] Web admin UI: manage photos, transitions, schedule, status; **HEIC upload transcode**.
- [ ] Night sleep/wake schedule (framebuffer blank).
- [ ] OTA auto-update over WiFi (opportunistic) with rollback + stable/beta channels.
- [ ] Clock overlay (FreeType); PNG/WebP support; albums/shuffle/favorites.

### Later (delight)
- [ ] Weather overlay (needs TLS port); WiFi sync (Immich/SMB); short MJPEG clips.
- [ ] PIR wake / ambient-light brightness (needs hardware).

---

## 7. Key subsystem designs

### 7.1 Boot, image layout & A/B
- Boot partition (FAT): Pi firmware (`start.elf` etc.), `config.txt`, `autoboot.txt`,
  `tryboot.txt`, `kernel8.img` (slot A) + `kernel8-b.img` (slot B); FAT **data** area for
  photos/config/index.
- A/B selection via the **Pi bootloader `tryboot`** mechanism (firmware-level, OS-agnostic):
  update writes the new kernel to the inactive slot and requests a one-shot `tryboot`; on
  successful health self-test the firmware makes it permanent, else the next boot reverts.

### 7.2 Update flow (one signed bundle, two triggers)
- **Artifact:** a single **signed** bundle (kernel + version + manifest). Same file for USB & OTA.
- **USB (guaranteed):** pendrive with `*.bundle` → verify signature → stage inactive slot → tryboot.
- **OTA (opportunistic):** when online, agent checks an update URL/channel → download → stage → tryboot.
- **Safety:** signature required; free-space/power checks; atomic slot swap; health-confirm or rollback.

### 7.3 Onboarding & WiFi
- Primary: edit `wifi.conf` (SSID/PSK) on the SD/pendrive from a PC → WLAN station joins.
- After first join: reconfigure WiFi from the web UI. **⚠️ SoftAP captive portal = validate;
  likely unsupported on Circle WLAN.**

### 7.4 Render engine
- Double-buffered framebuffer; fit-to-screen with letterbox.
- Cross-fade via per-pixel alpha blend (NEON). Ken Burns via NEON bilinear scale/pan.
- Overlays (clock/date) composited last via FreeType glyph rasterization.
- Target smooth ~24–30 fps for transitions at 1366×768 (optimize with NEON; validate).

### 7.5 Content pipeline
- Import: detect new on SD/USB/upload → decode (JPEG/PNG/WebP) → EXIF orient → **pre-scale to
  1366×768** (protects RAM) → thumbnail → hash dedupe → add to index. Prune on delete.
- Index: flat/JSON on FAT for MVP; consider ported SQLite later.

### 7.6 Web admin UI
- Served over Circle HTTP on the LAN. Manage photos/settings/schedule; show status; trigger
  update check; **HEIC→JPEG client-side transcode** on upload. Auth via a password in config.

### 7.7 Scheduler, power, health
- Cron-like sleep/wake → blank framebuffer / HDMI power off.
- BCM hardware watchdog via Circle; render-loop heartbeat; update health-confirm gates rollback.

---

## 8. Non-functional requirements
- **RAM:** target < ~350 MB resident; pre-scale images; cap thumbnail cache; stream large files.
- **Boot time:** bare metal is fast — target < ~10 s power-to-first-photo.
- **Reliability:** survive power loss anytime (FAT data, careful writes); no index corruption.
- **Update safety:** no update can brick (A/B + tryboot rollback).
- **Offline:** all core features work with zero network.

---

## 9. Security model
- OTA/USB bundles **cryptographically signed**; unsigned/invalid rejected before staging.
- Web admin behind a password; WiFi creds stored with care on FAT (encryption ⚠️ limited bare-metal).
- OTA transport over HTTPS (needs mbedTLS port) with cert verification.
- No shell/SSH exists (bare metal) — smaller attack surface by construction.

---

## 10. Toolchain & repo structure
- **Build env:** `aarch64-none-elf` GCC cross-toolchain; on this Windows box use **WSL2**.
- **Deps:** Circle + circle-stdlib as submodules; ported libjpeg/libpng/libwebp/FreeType (+mbedTLS).
```
/firmware        # Circle-based C++ OS: boot, drivers glue, render, services, update agent
/firmware/vendor # circle, circle-stdlib, ported libs (submodules)
/web             # web-admin UI static assets + HEIC transcode page
/tools           # bundle signing, HEIC companion converter, WSL build/flash scripts
/docs            # this spec + goals/roadmap/decisions/etc.
/tests           # host-side unit tests + on-target smoke checklist
```

---

## 11. Open questions / assumptions ⚠️
1. **Confirm panel is 1366×768** via sticker/EDID (working assumption).
2. **Does the TV board advertise the true mode over HDMI?** (junk boards lie) → pin timing, test.
3. **SoftAP support on Circle WLAN?** If none, config-file onboarding stands.
4. **TLS (mbedTLS) port needed** for HTTPS OTA + weather — schedule it.
5. **SQLite port vs flat index** — start flat, revisit.
6. **Where are OTA bundles hosted?** (self-hosted static HTTPS.)

---

## 12. Risks & first spikes (bare-metal)
- **C1 (high): Circle boots + HDMI framebuffer on the real Acer panel via the TV board.** Prove first.
- **C2 (high): JPEG from SD** — FatFs read + ported libjpeg decode + display.
- **C3 (med): USB pendrive** — Circle USB MSD enumerates + reads files.
- **C4 (med): WLAN station join** + confirm whether SoftAP is possible.
- **C5 (med): A/B via tryboot** — swap kernel slot + auto-rollback on failed health check.
- **C6 (med): transition performance** — NEON cross-fade/Ken Burns hits target fps at 1366×768.
