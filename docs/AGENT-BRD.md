# Lumen Frame — Agent-Ready BRD & Master Prompt

_Hand this whole file to a coding agent (or to a lead agent that spawns others) to build the
same product from scratch. It is a Business Requirements Document written as an executable brief:
what to build, why, the hard constraints, the decisions already made, the proven build order, the
contracts between components, and the coordination protocol for multiple agents._

---

## 0. How to use this document

**If you are a single agent:** read sections 1–6, then execute section 7 in order, one work package
at a time, following the delivery loop in section 8. Never batch unverified work.

**If you are a lead agent coordinating several agents:** read everything. Assign the work streams
in section 7 to sub-agents, publish the contracts in section 6 as frozen interfaces before anyone
codes against them, and enforce the coordination protocol in section 9. You own integration, the
task board, and the docs in section 10.

**The human owner** builds hardware, runs on-device tests you hand over as numbered test cards,
and decides product/UX forks. Everything else is yours to do without asking. Do not re-ask the
decided parameters in section 4.3.

---

## 1. Product vision (the "why")

**Lumen Frame** turns a Raspberry Pi Zero 2 W plus any HDMI screen (originally a salvaged 14"
laptop LCD on a generic HDMI driver board) into a polished digital photo frame, and later a modular
smart display with rotating screen plugins (photo, clock, weather, calendar, news).

It is **a genuine bare-metal firmware operating system in C++**, not an app on Linux. The Pi boots
the project's own `kernel8.img` directly. Authenticity of "we wrote the firmware" is a first-class
product goal, accepted knowingly as a months-long build.

**Principles, in priority order:**
1. Genuine firmware. No Linux, no distro, no shell.
2. Offline-first. Every core feature works with zero internet, zero cloud, zero accounts.
3. Fail-safe over feature-rich. It must never brick, blank, or corrupt a card.
4. It just works. Power on → photos on screen. No keyboard, no console.
5. Guard the 512 MB RAM budget.
6. Everything user-facing is configurable (white-label). No hardcoded product text.

**Success criteria (product level):**
- Powers on to a fullscreen slideshow with smooth transitions, no input needed.
- A non-technical person can join the frame from a phone and change settings in under 2 minutes.
- Photos can be added by SD card, USB pendrive, and phone (web upload / conversion).
- iPhone HEIC photos "just work" with no PC and no internet.
- Same firmware image fills any HDMI screen at its native resolution.
- Survives sudden power loss and bad updates without bricking or corruption.
- New screen plugins can be added and scheduled without a rewrite.

**Non-goals:** battery operation, touchscreen input, any hosted cloud service, on-device HEIC /
WebP / RAW decoding.

---

## 2. Target hardware and environment

| Item | Value |
|---|---|
| Compute | Raspberry Pi Zero 2 W (BCM2837, 4× Cortex-A53, 512 MB RAM). Circle `RASPPI=3`, AArch64 |
| Display | Any HDMI screen. Reference panel: Acer Aspire 4347 LCD, 1366×768, via a generic TV driver board |
| Storage | microSD (boot + FAT data). USB pendrive via micro-USB OTG (hotplug) |
| WiFi | On-board CYW43 (enumerates as chip 43430) over SDIO. Used as **SoftAP** (access point). Station mode deferred |
| Internet | Never required. Never used in the current scope |
| Dev host | Windows 11 + WSL2 Ubuntu. Cross toolchain `aarch64-linux-gnu-` or `aarch64-none-elf-`. QEMU `raspi3b` for emulation |

---

## 3. Architecture (fixed by decision, do not relitigate)

```
kernel8.img  (single bare-metal image, C++ on the Circle framework)
├─ boot/init (Circle): EL2→EL1, MMU, caches, interrupts, HDMI framebuffer, multicore
├─ drivers (Circle): SD via SDHOST + FatFs · USB host + MSD · CYW43 WLAN + Circle TCP/IP (libnet)
├─ app layer (ours, C++):
│   ├─ display/   ICanvas (framebuffer-backed drawing surface), ScreenPlugin interface
│   ├─ plugins/   PhotoFramePlugin (slideshow, fit+blur, Ken Burns, cross-fade, convert slide)
│   ├─ content/   IPhotoSource, JpegDecoder (vendored stb_image), ExifReader
│   ├─ app/       PluginScheduler (playlist with time windows)
│   ├─ core/      Result/Error, EventBus       (freestanding, host-testable)
│   ├─ net/       dhcpd, dnsd, webserver        (SoftAP services, captive portal, conversion API)
│   └─ app/       kernel (mode state machine), Config (lumen.conf), FileLogDevice (SD log), version
└─ FAT data on the SD card: lumen.conf · photos/ · firmware/ (CYW43 blobs) · lumenlog.txt
```

**Concurrency model:** core 0 runs the render loop and yields a time-slice per frame to Circle's
cooperative scheduler (network tasks: DHCP, DNS, HTTP). Core 1 is a dedicated image decoder with a
lock-free acquire/release handshake. Cores 2–3 idle.

**Rendering:** pure software. Fullscreen work writes `u32` pixels straight into the back buffer.
No GPU, no GLES (VideoCore is a closed blob, unusable bare-metal).

**Image formats:** JPEG (and cheaply PNG/GIF/BMP) decode on-device via stb_image. HEIC/WebP/RAW are
never decoded on the Pi. They are converted at the boundary, on the user's phone, over the frame's
own WiFi hotspot (see 5.3).

### 3.1 Architecture Decision Records to honour (summary)
| ADR | Decision |
|---|---|
| 004 | Bare-metal C++ on Circle, no Linux. Rejected: Linux appliance, pure Rust, Rust FFI to Circle |
| 005 | Custom software blitter (NEON where worthwhile). No GPU |
| 006 | C++ throughout, circle-stdlib available, vendor C libraries as needed |
| 007 / 012 | HEIC via boundary transcode. v0.3 refinement: phone-side conversion over on-device SoftAP + QR, zero internet |
| 008 | Modular `ScreenPlugin` + `PluginScheduler`. Photo frame first, plugin-ready from day one |
| 009 | One shared TLS + JSON + HttpClient layer before any network plugin (deferred milestone) |
| 010 | Emulator-first (QEMU raspi3b) for display/app/content; hardware for WiFi, timing, perf, OTA |
| 011 | Adaptive HDMI resolution: EDID auto-detect plus a safe pinned fallback and a config override |
| 013 | Enable PNG/GIF/BMP in stb. Heavy formats stay boundary-only |

Any new significant choice gets a new ADR appended to `docs/DECISIONS.md` (never edit or delete
old ones; supersede).

---

## 4. Requirements

### 4.1 Functional requirements by milestone (the proven order)

**M1 — v0.1 "offline" (boot on the wall)**
- FR-1.1 Boots the custom kernel on the Pi and fills the HDMI framebuffer (32-bit depth).
- FR-1.2 Scans `photos/` → `images/` → root on USB, then SD, for supported images. Falls back to an
  embedded image when no storage is present. Storage init is never fatal.
- FR-1.3 Decodes JPEG with vendored stb_image using a fixed pre-allocated pool.
- FR-1.4 Applies EXIF orientation (all 8 cases).
- FR-1.5 Displays each photo **Fit** (never cropped) over a darkened, heavily blurred copy of itself
  that fills the screen. Gentle Ken Burns (zoom ≤ 1.06), cross-fade between photos, real-clock-driven
  animation (not tick-driven).
- FR-1.6 USB pendrive hotplug in and out at runtime without crashing. Priority USB → SD → embedded.
- FR-1.7 Persistent diagnostics log on SD (`f_write` + `f_sync` per line).

**M2 — v0.2 "smooth & polished"**
- FR-2.1 Decode of the next photo runs on CPU core 1 while core 0 renders at ~40 ms/frame. No
  per-photo freeze. Inline fallback if multicore is unavailable.
- FR-2.2 Boot splash: dithered diagonal gradient, wordmark, tagline, credits, version label, ~10 s,
  dissolving into the first photo.
- FR-2.3 Build identity in `version.h` (`LUMEN_VERSION`, `LUMEN_MODE`, `LUMEN_BUILD`). Any version
  string containing `beta` or `dev` forces logging on.
- FR-2.4 Log is config-gated (`logging = on` in `lumen.conf`, default off), appends across boots,
  rolls over at ~1 MB to one `.old` generation.
- FR-2.5 USB removal never triggers device I/O (presence via device-name lookup only).

**M3 — v0.3 "Universal" (any screen, any image, fully offline) — current**
- FR-3.1 **SoftAP** on every boot (config `wifi`, default on): open network, SSID from config.
  Own DHCP server (one lease per client), own DNS responder (answers every A query with the AP IP),
  own single-threaded HTTP/1.0 server using one reused buffer.
- FR-3.2 **Captive portal:** answer OS connectivity probes (Android, iOS, Windows) with the settings
  page so the phone stays on the AP and the page auto-opens.
- FR-3.3 **Boot settings window:** splash shows a Wi-Fi-join QR (`WIFI:S:<ssid>;T:nopass;;`) with a
  10 s countdown. If a client takes a DHCP lease → **settings mode** (slideshow paused, page served).
  Otherwise the slideshow starts and the AP stays responsive during the show.
- FR-3.4 **Config system:** `lumen.conf` key = value file on SD. Defaults in code, overridden by
  file, editable at runtime, saved back. All user-facing text is config-driven: `name`, `tagline`,
  `credits`, `mode`, `ssid`, plus `wifi`, `logging`, and debug flags `portal`, `qrtest`.
- FR-3.5 **Settings web page:** edit the fields above, Save → `lumen.conf`, Restart button reboots.
- FR-3.6 **Needs-convert classification:** the photo source flags files the Pi cannot decode
  (HEIC/WebP/RAW) via `needs_convert(index)`, `name(index)`, `path(index)`.
- FR-3.7 **Inline convert slide:** when the rotation reaches a needs-convert file, render a QR slide in
  that slot for its normal dwell instead of stopping. Not on the AP yet → Wi-Fi-join QR. On the AP →
  QR with the URL of the conversion page. Then move on.
- FR-3.8 **Conversion page** (`GET /photos`): lists all pending files, on-screen one first. Per file
  the browser fetches raw bytes (`GET /heic?i=N`), decodes it natively (Safari/iOS decode HEIC;
  a libheif-WASM path for other browsers is planned, not built), resizes to ~1920 px long edge,
  re-encodes baseline JPEG q≈85, and POSTs it back (`POST /jpg?i=N`).
- FR-3.9 **Write-back:** the Pi writes the JPEG next to the source (keep the original), `f_sync`,
  then re-scans so the slot shows the photo next time. No reboot.
- FR-3.10 (not started) **Adaptive resolution:** EDID auto-detect plus safe pinned fallback plus
  `resolution = auto | WxH` override. Render engine is already resolution-independent.
- FR-3.11 (not started) PNG/GIF/BMP decode on-device. stb is still built `STBI_ONLY_JPEG`.

**Later milestones (out of current scope, keep the architecture ready):**
station-mode WiFi + LAN web admin · mbedTLS + JSON + HttpClient (ADR-009) · clock/weather/calendar/news
plugins · sleep/wake schedule · signed A/B OTA via the Pi bootloader `tryboot` with health-confirm
and rollback · hardware watchdog · USB auto-import with dedupe.

### 4.2 Non-functional requirements
| ID | Requirement |
|---|---|
| NFR-1 | Resident RAM < ~350 MB. Big buffers are pre-allocated once and reused. Never malloc/free > 512 KB repeatedly (Circle's heap leaks them) |
| NFR-2 | Power-on to first photo < ~10 s (excluding the deliberate 10 s splash/settings window) |
| NFR-3 | Render loop ≥ ~20 fps during transitions at 1366×768 with the AP up |
| NFR-4 | No crash on: no SD, no USB, USB pulled mid-show, phone disconnect mid-convert, empty photo folder |
| NFR-5 | Zero network dependency. Zero cloud. Zero accounts |
| NFR-6 | One image serves production and dev. Diagnostics gated by config plus build channel |
| NFR-7 | Shared logic is freestanding (C headers, no `std::`, no exceptions) so it compiles for both host unit tests and the `-nostdinc++ -fno-exceptions` firmware |
| NFR-8 | Security: no shell exists. Web settings are LAN/AP-local. Future bundles must be signed |

### 4.3 Decided parameters (owner-approved, do not re-ask)
| Parameter | Value |
|---|---|
| AP SSID | Config `ssid`, default `LumenFrame`. Open network in the current build (WPA2 `lumen1234` was the original decision; open was chosen for QR-join simplicity). Shown on-screen and inside the QR |
| Source file after convert | Keep the original. Write `.jpg` alongside (non-destructive) |
| Conversion trigger | On-demand whenever un-displayable files are found on any inserted drive |
| Oversized JPEGs | Not normalized on-device. Convert only HEIC/WebP/RAW |
| Output encoding | Baseline JPEG, ~1920 px long edge, quality ≈ 85 |
| Resolution | `auto` (EDID) with safe fallback; `lumen.conf resolution=auto` default |
| AP idle policy | AP stays up during the slideshow (it is cheap); settings mode only while a client holds a lease |
| Photo scan order | `photos/` → `images/` → root. USB before SD before embedded |
| Splash | Gradient hero, 10 s, dissolves into the first photo |

---

## 5. Behavioural specification (Given / When / Then)

**5.1 Boot**
- Given power is applied with a valid SD, when the kernel starts, then graphics initialise first,
  storage second (non-fatal), the splash appears with the QR and countdown, and the SD log records
  the boot banner.
- Given no SD and no USB, then the embedded image is shown and the frame does not halt.

**5.2 Slideshow**
- Given N displayable photos, then each shows for its dwell with Fit + blurred background, Ken Burns,
  and a cross-fade to the next, at a steady frame time, while the next photo decodes on core 1.
- Given a USB pendrive is inserted mid-show, then the source switches to USB after a scan. Given it
  is removed, then the source falls back to SD or embedded with no I/O on the vanished device.

**5.3 Offline HEIC conversion (the flagship flow)**
- Given a pendrive with HEIC files, when the rotation reaches one, then the slot shows a QR slide
  ("Scan to join the frame's Wi-Fi", or if already joined "Scan to convert this photo").
- When the phone joins the AP, then it gets a lease from the Pi's DHCP, its captive-portal probe is
  answered by the Pi's DNS + HTTP, and the settings page opens automatically.
- When the user opens the conversion page, then every pending file is listed with a thumbnail decoded
  in the browser and a Convert button. Tapping Convert shows an in-place progress bar, uploads the
  JPEG, and the Pi writes it next to the source and re-scans.
- Then the next time that slot comes up it shows the photo, not the QR. The Pi never decoded a HEIC.
- Given the phone disconnects mid-upload, then the partial file is discarded and the source is intact.

**5.4 Settings**
- Given a phone on the AP during the boot window, then the slideshow pauses in settings mode. Saving
  writes `lumen.conf`. Restart reboots. Every visible string reflects the config.

**5.5 Faults**
- Given a card with zero images, then the embedded image is shown with no crash.
- Given a lying or absent EDID, then the pinned fallback mode drives the screen; the screen is never
  blank.

---

## 6. Contracts (freeze these before parallel work starts)

**6.1 `lf::ScreenPlugin`** — `id()`, `on_activate()`, `on_deactivate()`, `update()` (non-blocking
background refresh), `render(ICanvas&)`, `has_content()`, `wants_continuous_redraw()`.

**6.2 `lf::ICanvas`** — `width()`, `height()`, `clear`, `set_pixel`, `fill_rect`, `text`,
`blit_rgb`, `blit_rgb_scaled`, `blit_rgb_blend(a, b, alpha 0..256)`, `present()`. Colours are plain
RGB888. Backends override the blits with direct back-buffer writes.

**6.3 `lf::IPhotoSource`** — `count()`, `jpeg(index, len&)` (buffer owned by source, valid until
next call), `needs_convert(index)`, `name(index)`, `path(index)` (full drive path like
`USB:/photos/x.heic`).

**6.4 `lf::PluginScheduler<MaxSlots>`** — slots `{id, enabled, duration_s, window_start_min,
window_end_min}`; `next(now_min)` round-robins enabled + in-window slots; midnight-wrap aware; time is
injected so it is fully host-testable.

**6.5 `CConfig`** — `Load(path)`, `GetStr/GetInt/GetBool(key, default)`, `Set`, `Save(path)`.
File format: `key = value`, one per line, `#` comments. Max 24 keys, 24-char keys, 80-char values.

**6.6 `lumen.conf` schema**
```
name     = LUMEN FRAME            # wordmark on splash and web page
tagline  = Memory Lane Walkthrough
credits  = ...
mode     = Offline-only           # label on the splash
ssid     = LumenFrame             # SoftAP name (also the QR payload)
wifi     = on                     # bring up the AP at boot
logging  = off                    # SD log (forced on for beta/dev builds)
resolution = auto                 # auto | 1366x768 (planned)
portal   = off                    # debug: full-screen portal mode
qrtest   = off                    # debug: render QR in QEMU
```

**6.7 HTTP routes (server on port 80, HTTP/1.0, one connection at a time)**
| Route | Purpose |
|---|---|
| `GET /` and any captive probe path | Settings page (captive-portal landing) |
| `POST /` | Save settings → confirm + Restart button |
| `POST /restart` | Sets `g_restartRequested`; kernel reboots |
| `GET /photos` | Conversion page listing all needs-convert files |
| `GET /heic?i=N` | Stream raw bytes of file N |
| `POST /jpg?i=N` | Write the uploaded JPEG next to file N, `f_sync`, set `g_rescanRequested` |

**6.8 Kernel ↔ services signals (volatile globals, set by tasks, polled by the render loop)**
`g_dhcpClientConnected` (a phone took a lease), `g_restartRequested`, `g_rescanRequested`.

**6.9 Kernel modes**
`Splash(QR + countdown)` → `Settings` (client present) | `Slideshow` (AP still up) ; `Slideshow` renders
convert slides inline; `Rescan` on write-back; `Restart` on request. Debug: `Portal`, `QrTest`.

**6.10 SD card layout**
`kernel8.img`, `config.txt` (+ Pi boot files), `lumen.conf`, `photos/`, `firmware/` (CYW43
`brcmfmac43430-sdio.{bin,txt,clm_blob}`), `lumenlog.txt` (+ `.old`).

---

## 7. Work breakdown and work streams

Order is risk-first. Each work package (WP) is independently buildable and verifiable. Mark the
verification type: **U** host unit test, **E** QEMU screenshot, **H** hardware (owner runs a test card).

**WS-0 Platform & build** (one agent; everyone depends on it)
- WP-0.1 WSL toolchain + Circle clone/configure (`-r 3 -p aarch64-linux-gnu- --qemu -f`), `Config.mk`
  flags `-DARM_ALLOW_MULTI_CORE -DNO_SCREEN_DMA_BURST_LENGTH -DDEPTH=32`, build libcircle + addons
  (fatfs, SDCard, wlan, net, sched). Clean rebuild after any flag change. (Verify: sample boots in QEMU. E)
- WP-0.2 App Makefile, `version.h`, embedded fallback image, `tools/run_qemu.sh`,
  `tools/qemu_capture_png.sh`, `tools/make_sd*.sh`, `tools/run_host_tests.sh`. (E, U)
- WP-0.3 Host unit-test framework + CMake for freestanding modules. (U)
- WP-0.4 SD logging device (append, `f_sync`, rollover, config + channel gating). (H)
- WP-0.5 SD (SDHOST) + USB MSD hotplug with safe presence detection. (H)

**WS-1 Render & content** (one or two agents)
- WP-1.1 `ICanvas` + `CircleCanvas` with direct back-buffer blits. (E)
- WP-1.2 stb_image vendoring with `STBI_NO_THREAD_LOCALS`, `STBI_NO_SIMD`, pool allocator. (U, E)
- WP-1.3 `ExifReader` (all 8 orientations) + `JpegDecoder`. (U)
- WP-1.4 `PhotoFramePlugin`: fit + blur background, Ken Burns, cross-fade, clock-driven. (E)
- WP-1.5 Core-1 decoder with acquire/release handshake + inline fallback. (H, timing logged)
- WP-1.6 Gradient splash with config-driven text and version label. (E)
- WP-1.7 `PluginScheduler` + `ScreenPlugin` + `EventBus`. (U)
- WP-1.8 PNG/GIF/BMP enablement + adaptive resolution (EDID + fallback + override). (E, H)

**WS-2 Networking & web** (one agent; gated by the SoftAP spike)
- WP-2.1 **Spike W1** SoftAP bring-up from Circle `addon/wlan/sample/hello_ap`. Pass: phone sees
  SSID and associates. This spike gates the whole stream; if it fails, fall back to a companion PC
  converter and re-scope. (H)
- WP-2.2 DHCP server (one lease, gateway + DNS = AP IP). (H)
- WP-2.3 DNS responder (all A queries → AP IP). (H)
- WP-2.4 HTTP/1.0 server, single reused buffer, receive/send timeouts. (H)
- WP-2.5 Captive-portal probe handling for iOS, Android, Windows. (H)
- WP-2.6 `CConfig` + settings page + Save + Restart. (U for parser, H for page)
- WP-2.7 Net + render + multicore coexistence: yield per frame; verify fps with the AP up. (H)

**WS-3 Offline conversion flow** (one agent; depends on WS-1 and WS-2 contracts)
- WP-3.1 Needs-convert classifier in the photo source (extension based; host-tested). (U)
- WP-3.2 On-device QR encoder (vendor Nayuki qrcodegen) + QR renderer on `ICanvas`. (E via `qrtest`)
- WP-3.3 Inline convert slide (two states: join Wi-Fi / open converter). (E, H)
- WP-3.4 Conversion page: list, browser decode + thumbnail, in-place progress, upload. (H)
- WP-3.5 `GET /heic` streaming + `POST /jpg` write-back with `f_sync` + re-scan. (H)
- WP-3.6 Resilience: disconnect mid-upload, pendrive pulled mid-write, partial batch. (H)

**WS-4 Quality, docs, integration** (the lead, or a dedicated agent)
- WP-4.1 Keep `docs/STATUS.md` as the resume point; `CHANGELOG.md`; `TESTPLAN.md` rows ticked.
- WP-4.2 Write hardware test cards for the owner (numbered steps, expected log lines, pass/fail).
- WP-4.3 Milestone close: bump `version.h`, retrospective, promote learnings to `CLAUDE.md`, commit.

**Dependency graph:** WS-0 → WS-1 and WS-2 in parallel → WS-3 → WS-4 closes each milestone. Within
WS-2, WP-2.1 must pass before any other WS-2 package starts.

---

## 8. Delivery loop (mandatory for every WP)

1. Restate the WP goal and its "done" criterion in one line.
2. Decompose into steps that each build and verify on their own. Track them on the task board.
3. De-risk first: prove the scariest unknown with a throwaway spike before building on it.
4. Implement → build → verify (host test / QEMU screenshot / hardware log) → only then continue.
5. Keep it bootable at every step. New paths degrade with a fallback instead of bricking.
6. Report against the checklist. Surface blockers and decisions early.
7. On milestone close: CHANGELOG, STATUS, LEARNINGS, TESTPLAN, version bump, commit.

**Verification bar:** a WP is not done until its named verification ran and the evidence (test
output, screenshot path, or SD log excerpt) is recorded in the task board or STATUS.

---

## 9. Multi-agent coordination protocol

**9.1 Roles**
- **Lead / integrator:** owns the task board, freezes contracts (section 6), assigns WPs, reviews
  and merges, keeps STATUS.md current, writes hardware test cards for the owner.
- **Stream agents:** one per work stream above. Own their directories. Never edit another stream's
  files without a message to its owner and the lead.
- **Owner (human):** hardware tests, product/UX forks, anything destructive or irreversible.

**9.2 Single source of truth**
- Requirements: this file. Decisions: `docs/DECISIONS.md` (append-only ADRs). Current state and
  resume point: `docs/STATUS.md`. Lessons: `docs/LEARNINGS.md`. Tests: `TESTPLAN.md`.
- A stream agent that discovers a fact worth keeping writes it to LEARNINGS the same session.

**9.3 Interfaces first**
- No stream codes against another stream's internals. Only the contracts in section 6.
- Changing a contract needs an ADR-style note in the task board, a message to every dependent
  stream, and a bump of all call sites in the same change. Prefer adding a defaulted virtual over
  changing a signature.

**9.4 Branching and integration**
- One branch per WP. Small diffs. Build passes and host tests pass before requesting merge.
- The lead merges to `main` after a build in WSL and a QEMU screenshot for any render-affecting change.
- Commit at every completed spike or WP. Never rewrite history. Never commit `vendor/circle`,
  `kernel8.img`, `.img` cards, or secrets.

**9.5 Communication**
- Every message between agents names the WP id, the state (blocked / needs-review / done), the
  verification evidence, and any contract impact.
- Blockers go to the lead immediately, with the smallest reproducible case and the SD log excerpt.
- Hardware test requests to the owner are numbered test cards: steps, what to observe on screen,
  what log lines prove it, pass/fail. Everything self-logs to SD so the card comes back with evidence.

**9.6 Autonomy boundaries**
- Do without asking: build, QEMU tests, host tests, deploy `kernel8.img` to the card path, Circle
  rebuilds when a feature needs one (one-line heads-up), commit at WP completion.
- Ask first: linker scripts, memory map, startup/boot code, `config.txt` timing changes, editing
  anything under `vendor/`, force-push, deleting user data, a product/UX fork.
- Never guess hardware behaviour. Log it, hand a test card to the owner, and wait.

---

## 10. Guardrails learned the hard way (treat as rules)

- **No thread-local storage.** `__thread` faults on bare metal (`tpidr_el0` uninitialised). Compile
  third-party code with TLS off (`STBI_NO_THREAD_LOCALS`).
- **Circle's heap leaks blocks > 512 KB.** Use fixed pools and reused buffers for anything large or
  per-photo. Keep web buffers ≤ 512 KB or allocate once and never free.
- **Fullscreen = direct back-buffer writes.** Per-pixel API calls are 3× too slow.
- **Hardware framebuffer is RGB, QEMU's is BGR.** Hardware is truth. Do not "fix" the emulator.
- **Storage is optional and non-fatal.** Graphics first, then storage, always an embedded fallback.
- **Freestanding shared code.** C headers, no `std::`, byte-loops instead of `memset`/`memcpy`.
- **Multicore is a global rebuild.** `-DARM_ALLOW_MULTI_CORE` in `Config.mk`, then clean-rebuild
  libcircle and every addon. Incremental builds silently mix ABIs.
- **Cross-core handshake** = GCC `__atomic_*` builtins with acquire/release. Disjoint buffers per
  job. Swap pointers only on core 0 when the worker is idle.
- **USB removal: never do I/O on a plug-and-play event.** Presence via `CDeviceNameService`; mount
  only on confirmed insert; on removal just `f_mount(0, ...)`.
- **WiFi needs the EMMC controller.** SD must use SDHOST (drop `NO_SDHOST`) on real hardware.
  Consequence: QEMU cannot read the SD image in that build; use the embedded image there.
- **CYW43 firmware loads from `SD:/firmware/` at runtime.** Zero 2 W enumerates as chip 43430.
- **Give the net stack a time-slice per frame** or DHCP and the page starve behind the render loop.
- **Log to SD with `f_sync` per line.** It is the only window into the Pi. Instrument timings
  (decode ms, scale ms, fps) before optimising.
- **Resize photos at ingest, not runtime.** A 29 MP JPEG costs ~3.8 s to decode; ~1920 px costs ~290 ms.
- **Fit, never crop.** Fill the letterbox with a darkened blurred copy.
- **Real-time-driven animation.** Drive zoom/pan/fade from a millisecond clock, not frame ticks.
- **Stock QEMU cannot do Circle networking or WiFi.** Plan net work as host-tested logic plus
  on-hardware live tests.
- **Windows + WSL quoting.** Put non-trivial shell logic in `tools/*.sh` and call it with full paths.
- **Ship one image.** Diagnostics gated by config plus build channel (beta/dev forces on).
- **Every user-facing string comes from config.** Name, tagline, credits, mode, SSID.

---

## 11. Acceptance test plan (minimum rows)

| ID | Verifies | Type | Pass criteria |
|---|---|---|---|
| A-01 | Boot to splash and slideshow | H | First photo within ~10 s after the splash; boot banner in SD log |
| A-02 | No SD / no USB | H | Embedded image shown; no halt |
| A-03 | EXIF orientations | U | All 8 render upright |
| A-04 | Fit + blur, no crop | E | Portrait and landscape fully visible with blurred fill |
| A-05 | Smoothness | H | Log shows render avg ≤ ~45 ms, no per-photo stall; decode on core 1 |
| A-06 | USB hotplug ×10 | H | Switch to USB on insert, fall back on removal, zero resets |
| A-07 | SoftAP visible + lease | H | Phone joins the SSID and receives an IP from the Pi |
| A-08 | Captive auto-open | H | Settings page opens automatically on iOS and Android |
| A-09 | Settings round-trip | H | Edit name, Save, Restart → splash shows the new name |
| A-10 | HEIC end-to-end | H | Pendrive with HEIC → QR slide → convert on phone → JPEG written → photo shows next cycle |
| A-11 | Disconnect mid-upload | H | Source intact, no partial JPEG picked up by the scan |
| A-12 | AP up during slideshow | H | fps unchanged with a client browsing the page |
| A-13 | Scheduler logic | U | Windows incl. midnight wrap; disabled slots skipped |
| A-14 | Config parser | U | Defaults, override, Set + Save round-trip, malformed lines ignored |
| A-15 | Different HDMI screen | H | Same image fills a 1080p monitor; fallback on bad EDID (not started) |

---

## 12. Environment and commands (reference implementation)

```bash
# Build (WSL2 Ubuntu)                        -> firmware/app/kernel8.img
cd /mnt/c/<repo>/firmware/app && make -j4    # make clean first when headers change

# Host unit tests (no hardware)
cd /mnt/c/<repo> && ./tools/run_host_tests.sh

# QEMU live window / headless screenshot
export DISPLAY=:0 && bash tools/run_qemu.sh firmware/app/kernel8.img firmware/app/sd.img
bash tools/qemu_capture_png.sh firmware/app/kernel8.img /tmp/shot.png 8 firmware/app/sd.img

# Deploy: copy kernel8.img to the SD card root, re-seat the card in the Pi.
# Card contents: kernel8.img, config.txt, lumen.conf, photos/, firmware/ (CYW43 blobs)
```

Repo layout: `firmware/app` (Circle app), `firmware/src/{core,util,display,app,content,plugins,net}`
(freestanding where possible), `firmware/vendor/{circle (gitignored), stb, qrcodegen}`,
`tests/host`, `tools/`, `docs/`.

---

## 13. Definition of done for the current milestone (v0.3.0-beta)

1. The same `kernel8.img` fills any HDMI screen it is plugged into (auto-detect + fallback).
2. JPEG/PNG/GIF/BMP dropped on the card or pendrive display directly.
3. A pendrive of HEIC produces the QR → hotspot → convert → write-back flow, fully offline, and the
   resulting JPEGs display without a reboot.
4. No internet used at any point. No regression to the slideshow, splash, multicore, or USB paths.
5. Docs, CHANGELOG, TESTPLAN updated; learnings captured; version bumped; committed.
