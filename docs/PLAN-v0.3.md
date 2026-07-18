# Plan — v0.3 "Universal" (any HDMI screen · any image · fully offline)

_Concrete work plan for the v0.3 milestone. Companion to [ROADMAP.md](ROADMAP.md),
[DECISIONS.md](DECISIONS.md) (ADR-011/012/013), and [../TESTPLAN.md](../TESTPLAN.md)._
_Prev milestone: v0.2.0-beta "smooth & polished" (see [../CHANGELOG.md](../CHANGELOG.md))._

## Theme

Make the frame **universal and forgiving**, with **no internet required, ever**:

- **Pillar A — Display-agnostic:** plug it into *any* HDMI screen (laptop panel, monitor, TV) and
  it fills the screen at that screen's resolution. No per-device config editing.
- **Pillar B — Image-agnostic:** drop *any* image (incl. iPhone **HEIC**) and it displays — the
  heavy conversion is done **on the user's phone**, over the Pi's own WiFi **hotspot**, triggered
  by a **QR code on the screen**. The Pi never touches the internet and never decodes HEIC itself.

**Hard constraint:** everything works with zero internet, zero cloud, zero accounts. The only
network is a local, on-demand SoftAP between the frame and the user's phone.

## Decided parameters (owner-approved 2026-07-18 — do not re-ask)
| Parameter | Value |
|---|---|
| SoftAP SSID / password | `LumenFrame` / `lumen1234` (WPA2); shown on-screen + in the QR; overridable in `lumen.conf` |
| Source file after convert | **Keep** the original (write `.jpg` alongside; non-destructive) |
| Conversion trigger | On-demand when un-displayable files are found on **any** inserted drive (pendrive or SD) |
| Oversized JPEGs | **Not** normalized in v0.3 (they display; multicore hides the cost). Convert only HEIC/WebP/RAW |
| Output encoding | Baseline JPEG, ~1920px long edge, quality ≈85 |
| Resolution | `auto` (EDID) + safe fallback; `lumen.conf resolution=auto` default |
| AP idle timeout | Drop AP + resume slideshow after 5 min with no client |

**Autonomy (owner-approved):** routine ops — build (WSL `make`), QEMU screenshot tests, deploy
(`kernel8.img` → `D:\`), vendor/Circle rebuilds when a feature needs one (one-line heads-up), and
**commit+push at each completed spike/phase** — are done without asking. Stop only for: a **hardware
test** (owner must run), a genuine **product/UX fork**, or anything **destructive/irreversible**.
Per-phase hardware checks are handed over as a short numbered test card + everything self-logs to SD.

---

## Architecture at a glance

### The crux of Pillar B: the Pi shuttles bytes, the phone does the work
The un-displayable files (HEIC/WebP/RAW) sit on the pendrive in the Pi, but the conversion runs in
the phone's browser. The data round-trips through the Pi without the Pi ever decoding an image:

```
USB pendrive → Pi reads raw .heic → serves bytes to phone over the hotspot
   → phone browser (libheif-WASM) decodes + resizes to ~1920px + re-encodes → JPEG
   → phone POSTs the .jpg back → Pi writes it to the SAME pendrive (f_sync)
```

The Pi's whole job is: **SoftAP + tiny HTTP/DNS server + FAT read/write + QR on screen.** No image
codec beyond what stb already gives us. See [ADR-012](DECISIONS.md#adr-012).

### The user experience (Pillar B) — REVISED & STREAMLINED (2026-07-19, owner)
The original "separate Conversion mode + its own QR" is **superseded**. Since the SoftAP + web page +
config are already live *during the slideshow*, conversion is **inline and non-disruptive**:

1. Slideshow runs normally. The photo source **classifies** each file: displayable (JPEG/PNG/…) vs
   **needs-convert** (HEIC/WebP/RAW).
2. When the rotation reaches a needs-convert file it renders a **QR slide** in that slot ("Scan to
   convert this photo") **instead of stopping** — just a placeholder for its normal dwell, then the
   slideshow moves on. The AP is already up, so the QR is scannable mid-show.
3. QR = the same Wi-Fi-join code → phone joins → captive portal opens the web page.
4. The page shows **all pending unsupported images** (option **B**), the on-screen one first. Each is
   a card: the browser fetches the raw HEIC from the Pi, **decodes it (libheif-WASM) to show a
   thumbnail**, with a **Convert** button centered on the image.
5. Tap Convert → the button is replaced **in place** by a **semi-transparent progress bar + Stop**;
   the browser resizes + re-encodes → uploads the JPEG → Pi **writes it back** to the pendrive.
6. **No reboot:** the frame **re-scans** the drive → the file is now a displayable JPEG → next time
   its slot comes up it shows the **photo instead of the QR**. The image "resolves" into the show.

**Deleted vs the old plan:** the separate Conversion-mode state, the second/dedicated QR, the
"drop the AP / reboot to resume" steps. Everything rides the existing AP + web + config.

### Components (new in v0.3)
| Component | Where | Notes |
|---|---|---|
| CYW43 **SoftAP** bring-up | Pi (Circle `addon/wlan`) | Based on `sample/hello_ap`. The gating risk. **Prereq:** build `libwlan.a` + fetch the CYW43 firmware blobs (`addon/wlan/firmware` Makefile) — not present yet. |
| **HTTP server on Circle net** | Pi | Circle's own TCP/IP stack (`CNetSubSystem`/`CSocket`, `libnet.a` — **not lwIP**); serve page, accept multipart upload, stream files. |
| **DNS responder** (captive portal) | Pi | Intercept the OS connectivity-check → auto-open page. |
| **QR encoder** | Pi | Vendored single-file lib (e.g. Nayuki qrcodegen); render to bitmap. |
| **FAT write** to USB | Pi | FatFs `f_write`+`f_sync`; new `.jpg` next to source. |
| **Conversion web page** | served from SD | Self-contained: **libheif-WASM** (`heic2any`) + canvas resize + `toBlob('image/jpeg')`. |
| **stb formats** JPEG→+PNG/GIF/BMP | Pi | Flag change; makes those "just work" dropped on the card. |
| **EDID read + adaptive res** | Pi + config.txt | Pillar A. Auto-detect with safe fallback. |

---

## Phased plan (risk-first)

### Phase 0 — De-risk spikes (do FIRST; mostly hardware-only)
The point is to prove the scary unknowns before building UX on top of them.

- **Setup (once, before W1):** build `addon/wlan` → `libwlan.a`, and fetch the **CYW43 firmware
  blobs** (`addon/wlan/firmware` Makefile — one-time internet download); rebuild is
  multicore-flag-aware (Config.mk already has `-DARM_ALLOW_MULTI_CORE`). Also link `libnet.a`.
- **Spike W1 — SoftAP** 🔴 _the gate._ Bring up CYW43 AP mode bare-metal (port `addon/wlan/
  sample/hello_ap`). **Pass:** a phone sees "LumenFrame", joins with the passphrase, and gets a
  DHCP lease from the Pi. **If this fails, v0.3's Pillar B is blocked** — re-scope to a companion
  PC converter (ADR-012 fallback).
- **Spike W2 — HTTP over AP.** Minimal lwIP HTTP server serving one static page. **Pass:** phone
  browser loads a page served from the Pi over the hotspot.
- **Spike U1 — FAT write to USB.** `f_write` a file to the pendrive + `f_sync`; re-read on a PC.
  **Pass:** the file is intact and appears on the pendrive on another machine.
- **Spike D1 — EDID/adaptive res (Pillar A).** Read EDID via the Circle mailbox; boot with
  auto-detect + fallback. **Pass:** connect a *different-resolution* screen → frame fills it
  correctly; EDID + chosen mode logged.

### Phase 1 — Pillar A: adaptive resolution (parallel, low risk)
- config.txt strategy: EDID auto-detect **with a safe pinned fallback** (a lying TV board must
  never blank the screen).
- Read + log EDID (mailbox); surface detected resolution on the status/splash.
- Verify the (already resolution-independent) render at ≥2 resolutions incl. 1080p and a portrait
  mode; harden fit+blur/gradient/splash for odd aspect ratios.
- `lumen.conf` override: `resolution = auto | 1366x768` to force-pin a misbehaving screen.
- **Done when:** the same `kernel8.img` fills a different HDMI screen with no code/config edit.

### Phase 2 — On-device formats + "needs conversion" classifier
- Enable **PNG/GIF/BMP** in stb (in addition to JPEG). These now display dropped straight on the card.
- Scanner classifies each file: **displayable** (JPEG/PNG/GIF/BMP within size) vs **needs
  conversion** (HEIC/WebP/RAW, or oversized beyond a threshold). The needs-conversion list is what
  drives the QR flow.
- **Done when:** the scan produces an accurate "N files need converting" set (host-unit-tested on
  the classifier logic).

### Phase 3 — Pillar B core: SoftAP + HTTP/DNS + QR onboarding
- Bring the SoftAP up **on demand** (only when the scan finds needs-conversion files), not always-on.
- On-screen **QR (WiFi join)** + instructions (why + what to do), rendered full-screen.
- **Captive portal:** DNS responder + intercept the connectivity-probe URL → auto-open the page;
  fallback prints `http://lumen.local` (or the AP IP) on screen.
- Serve the self-contained conversion page.
- **Done when:** plug a HEIC pendrive → QR appears → phone joins → page auto-opens.

### Phase 4 — Pillar B: convert + write-back
- Page lists the needs-conversion files (from the Pi's scan) and a **Convert** button.
- Per file: browser `fetch`es raw bytes from the Pi → **libheif-WASM** decode → canvas resize
  (~1920px) → `toBlob('image/jpeg')` → `POST` back.
- Pi writes the JPEG to the pendrive (new `.jpg`; leave or delete the source — configurable),
  `f_sync` each; multipart upload streamed to avoid RAM spikes.
- Progress on the phone **and** a "Converting X/N" state on the frame.
- **Done when:** a real iPhone HEIC pendrive is converted end-to-end and the JPEGs display.

### Phase 5 — Integration, resilience, docs
- State machine: Slideshow ↔ Conversion mode ↔ Done → resume (incl. the new files).
- Failure handling: phone disconnects mid-convert; pendrive pulled mid-write (warn "don't remove");
  partial batch (resume the remaining); AP timeout if nobody connects.
- Update `docs/*`, `CHANGELOG`, `TESTPLAN`; cut **v0.3.0-beta**.

---

## Risk register
| Risk | Sev | Mitigation |
|---|---|---|
| **SoftAP bare-metal bring-up (CYW43)** | 🔴 High | Spike W1 first; Circle `hello_ap` proves it's possible. Fallback: companion PC converter (still offline, no QR). |
| Captive-portal auto-open across phones | 🟡 Med | Always show the URL on screen as fallback; auto-open is a bonus. |
| FAT write corruption if pendrive pulled mid-write | 🟡 Med | `f_sync` per file; on-screen "don't remove until done"; write to temp then rename. |
| Serving HTTP without stalling the slideshow | 🟡 Med | During conversion the screen shows QR/status (not slideshow), so no contention; run net work on a spare core if needed. |
| libheif-WASM size / phone RAM on huge HEIC | 🟢 Low | Resize during decode; process one file at a time; bundle is ~1–2 MB served locally. |
| EDID lies from the generic TV board (Pillar A) | 🟡 Med | Safe pinned fallback + `lumen.conf` override. |

## Tech stack additions
- **Circle `addon/wlan`** (build `libwlan.a`; fetch CYW43 firmware blobs — needs internet once) +
  **Circle net stack** `libnet.a` / `CNetSubSystem` (**not lwIP** — Circle has its own TCP/IP).
- A **minimal HTTP server** on `CSocket` (or Circle's `CHTTPDaemon`) + a **DNS responder** for the
  captive portal.
- **QR encoder**: vendor a single-file lib (Nayuki `qrcodegen`, public domain) → render bitmap.
- **Conversion page**: `libheif` compiled to **WASM** (`heic2any`/`libheif-js`), inlined
  self-contained; canvas `drawImage` + `toBlob('image/jpeg', q)`.
- **stb**: drop `STBI_ONLY_JPEG` → allow PNG/GIF/BMP (keep the pool allocator + no-TLS flags).

## Explicitly deferred (NOT in v0.3)
- Internet / WiFi **station** mode, `wifi.conf` join to home WiFi, `lumen.local` over LAN.
- **TLS/HTTPS** and the mbedTLS port (that's the network-plugins milestone, ADR-009).
- General **web admin** (settings/photo management/scheduling beyond conversion).
- **OTA** A/B updates (tryboot), watchdog.
- **On-device** HEIC/WebP/RAW decode (physically impractical — ADR-007/012).
- Network plugins (weather/calendar/news), real-time clock/NTP.

## Definition of done (v0.3.0-beta)
1. The same `kernel8.img` fills **any HDMI screen** it's plugged into (auto-detect + fallback).
2. **JPEG/PNG/GIF/BMP** dropped on the card/pendrive display directly.
3. A pendrive of **HEIC** triggers the on-screen **QR → phone hotspot → convert → write-back**
   flow, fully **offline**, and the resulting JPEGs display.
4. No internet used at any point. No regression to the v0.2 slideshow/splash/multicore/USB paths.
5. Docs + CHANGELOG + TESTPLAN updated; learnings captured.
