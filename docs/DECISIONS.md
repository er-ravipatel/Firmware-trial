# Decision Log (ADRs)

_Architecture Decision Records. One entry per significant choice — what we decided,
why, and what we traded away. Append-only; supersede rather than delete._

---

## ADR-000 — Template (copy this for each new decision)

- **Date:** YYYY-MM-DD
- **Status:** proposed | accepted | superseded by ADR-XXX
- **Context:** What problem/situation forced a decision?
- **Decision:** What did we choose?
- **Alternatives considered:** What else was on the table?
- **Consequences:** What does this make easier? Harder? What did we give up?

---

## ADR-001 — Base image & OTA: Pi OS Lite + RAUC — **SUPERSEDED by ADR-004**

- **Date:** 2026-07-17 · **Status:** superseded by ADR-004
- Originally chose a Linux appliance (Raspberry Pi OS Lite + RAUC). Reversed after the owner
  chose a genuine bare-metal firmware OS. Kept for history.

## ADR-002 — Renderer: Python + pi3d — **SUPERSEDED by ADR-005**

- **Date:** 2026-07-17 · **Status:** superseded by ADR-005
- pi3d/OpenGL assumes Linux + GPU stack; not applicable bare-metal. Kept for history.

## ADR-003 — Language: Python + Go — **SUPERSEDED by ADR-006**

- **Date:** 2026-07-17 · **Status:** superseded by ADR-006
- Reversed in favor of C++ on Circle. Kept for history.

---

## ADR-004 — Bare-metal firmware OS on Circle (C++), no Linux

- **Date:** 2026-07-17
- **Status:** accepted
- **Context:** Owner's primary goal shifted to building a *genuine firmware OS* ("original"),
  not an app on Linux. Must still reach the full feature set (SD/USB/WiFi/OTA).
- **Decision:** Build directly on the **Circle** C++ bare-metal framework (`RASPPI=3`, AArch64).
  No Linux; the Pi boots our `kernel8.img`.
- **Alternatives considered:**
  - *Pure Rust from scratch* — max craft, but no mature Pi USB host / WiFi stack; USB & WiFi
    would be multi-month/research efforts. Rejected: can't reach full features soon enough.
  - *Linux appliance (ADR-001)* — fastest to full features, but not a "genuine firmware OS."
  - *Circle via Rust FFI* — C++ ABI binding to Circle is brittle; rejected.
- **Consequences:** Genuine firmware; Circle provides USB, WLAN, FatFs, net, framebuffer, so
  the full frame is feasible. Costs: **months-long build**, HEIC needs boundary transcode,
  transitions are CPU/NEON, OTA is custom, SoftAP onboarding likely unavailable, dev needs WSL2.

## ADR-005 — Renderer: custom C++ software blitter (NEON)

- **Date:** 2026-07-17
- **Status:** accepted (perf pending spike C6)
- **Context:** No GPU compositor bare-metal (VideoCore is a blob). Need transitions at 1366×768.
- **Decision:** Software render engine in C++ over Circle's framebuffer; NEON-accelerated
  alpha-blend (cross-fade) and bilinear scale (Ken Burns).
- **Alternatives considered:** GPU/GLES (needs the closed VideoCore driver — infeasible bare-metal).
- **Consequences:** Full control; must hand-write/optimize blitters. Risk = hitting smooth fps,
  de-risked by spike C6.

## ADR-006 — Language: C++ (with circle-stdlib)

- **Date:** 2026-07-17
- **Status:** accepted
- **Context:** Circle is C++; owner chose C++ for authenticity.
- **Decision:** Application layer in C++ on Circle + circle-stdlib (STL, newlib, FreeType);
  port C libraries (libjpeg/libpng/libwebp, later mbedTLS) as needed.
- **Consequences:** One language, matches the framework. Manual memory discipline; library
  porting effort for codecs/TLS.

## ADR-007 — HEIC via boundary transcode (not on-device)

- **Date:** 2026-07-17
- **Status:** accepted
- **Context:** Bare metal can't decode HEIC (no HEVC decoder). Owner still wants iPhone photos.
- **Decision:** Transcode HEIC→JPEG **before** it reaches the frame: primarily **client-side in
  the phone browser** on upload; also a companion PC converter; network sources serve JPEG.
  The frame stores/displays JPEG/PNG/WebP only.
- **Alternatives considered:** On-device decode (infeasible); drop HEIC (rejected — loses iPhone).
- **Consequences:** iPhone photos supported without a bare-metal HEVC decoder; adds a small
  browser transcode + companion tool to build.

## ADR-008 — Modular ScreenPlugin architecture (InkyPi-inspired)

- **Date:** 2026-07-18
- **Status:** accepted
- **Context:** Product vision broadened (ref: InkyPi) from "photo frame" to a **modular smart
  display**: multiple screens (photo, clock, weather, calendar, news) rotated on a schedule and
  managed via the web UI. On a full-color HDMI LCD, not e-ink.
- **Decision:** A `ScreenPlugin` interface (`id/on_activate/update/render/has_content`) + a
  `PluginScheduler` playlist (per-slot duration + optional time window, midnight-wrap aware).
  PhotoFrame is plugin #1; the render engine composites whatever the active plugin draws;
  the EventBus/web UI enable & schedule plugins. **Photo-frame-first**, plugin-ready from day one.
- **Alternatives considered:** hard-coded photo-only app (rejected — reframe would be a rewrite);
  copy InkyPi on Linux/Python (rejected — contradicts the bare-metal-firmware goal, ADR-004).
- **Consequences:** No rewrite — the existing architecture already fits. Local plugins
  (photo, clock) are cheap; network plugins depend on ADR-009. Slight added structure now.

## ADR-009 — Shared TLS + JSON layer for network plugins

- **Date:** 2026-07-18
- **Status:** accepted (scheduled after MVP)
- **Context:** Weather, Calendar, News/stocks each need HTTPS + JSON + a specific API. Building
  that per-plugin is wasteful and risky.
- **Decision:** Build **one** shared layer — **mbedTLS** (HTTPS client) + a small **JSON parser**
  + an `HttpClient` — as a single unlock, *then* add the network plugins on top. Local plugins
  (Clock) ship first and need none of it.
- **Alternatives considered:** per-plugin ad-hoc networking (rejected — duplication); plain HTTP
  (rejected — most modern APIs require TLS).
- **Consequences:** Moves the mbedTLS port from "later" to a scheduled core milestone (M-plugins).
  Once done, all network plugins become cheap. Adds a meaningful chunk of bare-metal porting work.

## ADR-010 — Emulator-first development with QEMU

- **Date:** 2026-07-18
- **Status:** accepted
- **Context:** Hardware is on hand, but flashing an SD card per iteration is slow. We want a fast
  edit-build-run loop for the display + app + content layers.
- **Decision:** Develop and test in **QEMU** (`-M raspi3b`, emulating BCM2837 = same SoC family as
  the Zero 2 W) first; move to real hardware for what QEMU can't model. Circle supports QEMU.
- **Faithful in emulation:** boot, framebuffer/display, serial, timers, app logic, render engine,
  content pipeline (SD image), basic net/web UI. **Not faithful:** WiFi (no CYW43 emulation),
  tryboot/A-B rollback, exact HDMI timing through the TV board, NEON performance.
- **Consequences:** Fast iteration for most of the firmware without reflashing. Hardware-only
  items (WiFi, OTA rollback, HDMI timing, perf) are validated on the real Pi in a later pass.
  Toolchain: `aarch64-linux-gnu-` (distro) for bare metal; fall back to ARM `aarch64-none-elf` if
  Circle needs it. Run via `tools/run_qemu.sh`.

## ADR-011 — Adaptive HDMI resolution with EDID auto-detect + safe fallback

- **Date:** 2026-07-18
- **Status:** accepted (planned for v0.3, see [PLAN-v0.3.md](PLAN-v0.3.md))
- **Context:** The frame currently pins 1366×768 in `config.txt` for the Acer panel because the
  generic TV board may misreport EDID. Goal: be **display-agnostic** — work on any HDMI screen.
- **Decision:** Let the GPU auto-detect the native mode from **EDID**, but keep a **safe pinned
  fallback** so a board that lies about (or omits) EDID can never blank the screen. Read + log EDID
  from our kernel via the Circle mailbox; expose a `lumen.conf` override (`resolution = auto |
  WxH`). The render engine is **already resolution-independent** (draws from `canvas.width/height`),
  so no render rewrite — just verify odd/non-16:9 aspect ratios.
- **Alternatives considered:** *Keep hardcoding per panel* (rejected — not a product). *Pure
  EDID auto with no fallback* (rejected — that's exactly what blanked the screen before).
- **Consequences:** One image works on any screen; a misbehaving screen degrades to the fallback
  or a manual pin instead of a black display. HDMI mode is still ultimately GPU/config.txt-set
  (bare metal can't re-mode after boot), so "auto" is a boot-config strategy + our EDID logging.

## ADR-012 — Offline image normalization via phone-side transcode (QR + SoftAP, no internet)

- **Date:** 2026-07-18
- **Status:** accepted (planned for v0.3) — refines ADR-007
- **Context:** Bare metal can't decode HEIC/WebP/RAW (ADR-007), yet users drop iPhone HEIC on a
  pendrive. We want it to "just work" **with no internet** and no companion PC step.
- **Decision:** When a pendrive contains un-displayable files, the frame brings up its own **WiFi
  SoftAP on demand** and shows a **full-screen QR** (WiFi-join payload) + instructions. The phone
  joins the hotspot; a captive portal opens a page the Pi serves; the page converts the images
  **in the phone's browser** (libheif-WASM: decode + resize to ~1920px + re-encode JPEG) and uploads
  the JPEGs; the Pi **writes them back to the same pendrive**. The Pi shuttles bytes and never
  decodes an image. Everything is local — **zero internet**.
- **Alternatives considered:** *Companion PC converter* (kept as the fallback if SoftAP bring-up
  fails — still offline, but less seamless). *On-device HEIC decode* (infeasible — ADR-007).
  *Cloud transcode* (rejected — violates the no-internet constraint). *Station-mode WiFi + LAN
  upload* (deferred — needs home WiFi; SoftAP is zero-config).
- **Consequences:** iPhone photos work fully offline with only the phone the user already has, using
  the phone's compute. Costs: **CYW43 SoftAP bare-metal bring-up is the gating risk** (unemulatable;
  Circle `hello_ap` shows it's possible), plus an HTTP/DNS/captive-portal server, an on-device QR
  encoder, and FAT **write** to USB (new path). Converted JPEGs persist on the pendrive → fast reuse.

## ADR-013 — Expand on-device decode to JPEG/PNG/GIF/BMP (stb); heavy formats at the boundary only

- **Date:** 2026-07-18
- **Status:** accepted (planned for v0.3)
- **Context:** stb is currently built `STBI_ONLY_JPEG`. PNG/GIF/BMP are cheap to enable and make
  "drop anything common on the card" work without the conversion flow.
- **Decision:** Enable **PNG, GIF, BMP** in stb alongside JPEG (keep the fixed pool allocator +
  `STBI_NO_THREAD_LOCALS` + `STBI_NO_SIMD`). **HEIC/WebP/RAW stay boundary-only** (ADR-012) — never
  decoded on-device. A scanner classifies files into *displayable* vs *needs-conversion*.
- **Alternatives considered:** *Add libwebp* (deferred — real porting effort/RAM; WebP is rarer than
  HEIC). *Keep JPEG-only* (rejected — PNG/GIF/BMP are nearly free).
- **Consequences:** More formats "just work" directly; small code-size increase. The image-agnostic
  promise is: common raster formats on-device, HEIC/WebP/RAW via the phone-side boundary.
