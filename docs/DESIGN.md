# Lumen Frame — Subsystem Design

> Deep design for the four core subsystems, on the bare-metal C++ / Circle architecture
> ([PRD.md](PRD.md), ADR-004..007). Circle class/API names marked **⚠️verify** should be
> confirmed against Circle's headers & `sample/` code — treat them as "the capability exists,
> confirm the exact symbol." Ordered foundation-up: Boot → Content → Render → OTA.

---

## A. Boot & image layout

### A.1 What runs at power-on
The Pi's boot chain is fixed and partly proprietary:
1. GPU ROM loads `bootcode`/`start.elf` (closed firmware) from the **boot partition**.
2. `start.elf` reads `config.txt` + `autoboot.txt`, sets up RAM/clocks, loads **`kernel8.img`**
   (our firmware) to the ARM, releases the A53 cores.
3. Our Circle code takes over at the AArch64 entry point (EL2→EL1, MMU, caches, vectors — Circle
   startup handles this).

We cannot remove the blob, but from step 3 on the machine is ours.

### A.2 SD card partition layout (A/B + data)
```
p1  bootA  (FAT32)  Pi firmware, config.txt, autoboot.txt, kernel8.img  (slot A)
p2  bootB  (FAT32)  Pi firmware, config.txt, autoboot.txt, kernel8.img  (slot B)
p3  data   (FAT32)  /photos  /thumbs  /config (wifi.conf, settings.json, index)
```
- Two full boot partitions = clean A/B: an update only ever writes the **inactive** one; the
  running slot is never mutated. `data` is shared and persistent across updates.
- **⚠️** FAT32 for `data` is simplest (FatFs), but is fragile to power-loss mid-write — see
  Content §B.5 for the write-safety strategy.

### A.3 config.txt / display pinning
```
# config.txt (both boot partitions)
arm_64bit=1
kernel=kernel8.img
disable_overscan=1
hdmi_group=2          # DMT (monitor timings)
hdmi_mode=81          # ⚠️ 81 = 1366x768@60; confirm the TV board honors it
hdmi_force_hotplug=1  # drive HDMI even if EDID read fails (junk board insurance)
# fallback if the board lies about EDID: hdmi_group=2 + a custom hdmi_cvt line
```
Display pinning is **risk C1/C3** — generic TV boards often misreport. Plan B is an explicit
`hdmi_cvt=1366 768 60` + `hdmi_group=2 hdmi_mode=87` (custom mode).

### A.4 A/B selection via `tryboot`
The Pi bootloader supports **`tryboot`** — a one-shot "boot the trial partition once" flag,
OS-agnostic (lives in firmware), which is exactly what we need.
```
# autoboot.txt (lives at the start of the card)
[all]
boot_partition=1      # normal boot → slot A
[tryboot]
boot_partition=2      # one-shot trial boot → slot B
```
- Update stages the new kernel into the inactive partition, then triggers a **one-shot tryboot**
  reboot into it.
- If the new firmware **confirms healthy**, we rewrite `autoboot.txt` to make that partition the
  permanent `[all]` target.
- If it crashes/hangs before confirming, the next (watchdog) reboot is a *normal* boot →
  automatically back to the known-good slot. **Zero-brick guarantee.**
- **⚠️ Tricky bit (spike C5):** triggering a one-shot tryboot *from bare metal*. On Linux it's
  `reboot "0 tryboot"`; bare-metal we must set the same PM reset/partition flags the firmware
  reads before resetting. Validate against rpi bootloader docs; this is the single most
  uncertain mechanism in the OTA path.

### A.5 Circle bring-up (component list)
`CMemorySystem` · `CInterruptSystem` · `CExceptionHandler` · `CTimer` · `CLogger` ·
`CBcmFrameBuffer`/`CScreenDevice` · `CEMMCDevice`+FatFs · `CScheduler`/`CTask` (cooperative
multitasking) · watchdog (**⚠️verify** exact class). Boot target: **< 10 s** power-to-photo.

---

## B. Content pipeline & storage

### B.1 Responsibilities
The single owner of "what photos exist and in what form." Import → normalize → index → serve
display-ready frames to the render engine, all within the 512 MB budget.

### B.2 Components (C++)
- `ImportSource` (interface) → `SdImporter`, `UsbImporter`, `UploadImporter` (from web UI).
- `ImageDecoder` — wraps ported **libjpeg-turbo** (NEON SIMD), **libpng**(+zlib), **libwebp**.
- `ExifReader` — small custom parser; only needs the Orientation tag (1..8).
- `Prescaler` — decode → rotate per EXIF → scale-to-fit 1366×768 → cache as display-ready.
- `ThumbGen` — small (e.g. 320×180) JPEG thumbnails for the web UI.
- `PhotoIndex` — the catalog (see B.4).
- `Deduper` — content hash (e.g. xxHash/BLAKE2s) to skip duplicates.

### B.3 Import → display data flow
```
new file detected (SD/USB/upload)
   → validate + decode (JPEG/PNG/WebP)         [libjpeg-turbo / libpng / libwebp]
   → EXIF orient                               [ExifReader]
   → pre-scale to ≤1366×768                     [Prescaler — protects RAM]
   → write display-ready JPEG to /photos        [FatFs]
   → generate thumbnail to /thumbs
   → hash + add record to index                 [PhotoIndex, Deduper]
```
Storing a **pre-scaled** copy means the render loop never decodes a 12 MP original — it decodes
a ~1 MP display-ready image. Big RAM + latency win.

### B.4 The index
- MVP: a single **`index.json`** (or line-delimited records) in `/config`: `{id, file, w, h,
  orientation, hash, added, album, favorite}`.
- Later: port **SQLite** (single-file DB, works with a FatFs I/O shim) if we outgrow flat files.
- Loaded into an in-RAM vector at boot; the render engine reads a shuffled/ordered playlist view.

### B.5 Power-loss safety (the FAT hazard)
FAT + sudden power cut can corrupt. Mitigations:
- **Atomic writes:** write to `foo.tmp` then `f_rename` (rename is the closest FAT gets to atomic).
- **Index journaling:** write `index.json.new`, fsync/`f_sync`, then rename over `index.json`.
- Never write the boot partitions during normal operation (read-only after flashing).
- **⚠️** Consider a small double-buffered index (A/B copies + a validity marker) so a torn write
  never loses the whole catalog.

### B.6 RAM budget (512 MB)
- Framebuffers: 1366×768×4 ×2 (double buffer) ≈ 8.4 MB.
- Current + next decoded frame (RGB): ≈ 2×3.1 MB ≈ 6.2 MB.
- Index + thumbnail cache: cap explicitly (e.g. LRU, ≤ 64 MB).
- Decoder scratch: transient. → Comfortable headroom; the rule is **never hold an un-scaled
  original longer than the decode call**.

---

## C. Render engine & transitions

### C.1 The loop
Cooperative `CTask` running the slideshow; separate tasks for net/update so rendering never blocks.
```
loop:
  next = playlist.advance()
  decode(next) → frameB (display-ready JPEG → RGB)
  transition(frameA → frameB)      # cross-fade or Ken Burns, per settings
  frameA = frameB
  wait(dwell_time, honoring pause/next commands)
```

### C.2 Framebuffer & double buffering
- `CBcmFrameBuffer` allocated with **virtual height = 2× physical** (1366×1536); draw to the
  offscreen half, then **`SetVirtualOffset(0, 768)`** to page-flip on vsync. (**⚠️verify** exact
  Circle call for pan/vsync.) Eliminates tearing.

### C.3 Cross-fade (cheap)
`out = A·(1−t) + B·t` per pixel, t stepped over ~0.5–1 s.
- NEON: process 8–16 px/iteration (`vmull`/`vshrn` on 8-bit lanes).
- Cost: ~3.1 MB read×2 + write per frame; at 30 fps ≈ ~280 MB/s memory traffic — **trivial for
  A53+NEON**. Cross-fade is not a perf concern.

### C.4 Ken Burns (the perf risk — spike C6)
Slow pan/zoom: sample a source region (slightly larger than screen) with **bilinear
interpolation**, animating the source rect over several seconds.
- Per output pixel = 4 source taps + blend → random-ish memory access → heavier than cross-fade.
- NEON bilinear at 1366×768 @ 24–30 fps is **achievable but must be measured** (C6). Fallbacks:
  drop to 24 fps, integer-step pans, or pre-render the pan as a short sequence.

### C.5 Overlays
Clock/date/album name composited **last** onto the final frame.
- Text via **FreeType** (in circle-stdlib): rasterize glyphs → alpha-blend onto framebuffer.
- Keep a glyph cache; redraw only when the string changes (clock = once/min).

### C.6 Fit / letterbox
Scale-to-fit within 1366×768 preserving aspect; fill margins with black (or a blurred-cover
option later). EXIF orientation already applied at import, so the render path assumes upright.

---

## D. OTA / A/B update & rollback

### D.1 Design goals
No update can brick the device; **one signed artifact** for USB (guaranteed) and WiFi
(opportunistic); fully automatic rollback.

### D.2 Bundle format
```
[ magic | format-ver | build-id | payload-len | payload (kernel8.img [+assets]) | signature ]
```
- **Signature:** Ed25519 over the payload, verified with a **public key baked into the running
  firmware**. Use **monocypher** (single-file, no_std-friendly, no mbedTLS needed just for
  signing) — clean fit for bare metal. Private signing key stays offline on the dev machine.
- Versioning: monotonic `build-id`; refuse downgrades unless a "force" flag is set.

### D.3 Components (C++)
- `UpdateAgent` (task): orchestrates the whole flow.
- `UsbUpdateSource`: scans a pendrive for `*.bundle`.
- `NetUpdateSource`: when online, HTTP GET `{updateURL}/{channel}/latest`, compares build-id.
  (**⚠️** HTTPS needs an mbedTLS port; MVP may allow signed-bundle-over-HTTP since the *bundle*
  is signed regardless of transport.)
- `BundleVerifier`: signature + version checks (monocypher).
- `SlotWriter`: writes the payload to the **inactive** boot partition via FatFs.
- `BootControl`: reads/writes `autoboot.txt`, triggers one-shot tryboot, marks good/reverts.
- `HealthCheck`: post-boot self-test gating confirmation.

### D.4 Update flow
```
1. discover bundle (USB scan OR net poll when online)
2. verify signature + version                      [BundleVerifier]  ── fail → abort, log
3. pre-checks: free space on inactive slot, (power ok)
4. write payload → inactive boot partition          [SlotWriter]
5. f_sync; set one-shot TRYBOOT into inactive slot  [BootControl]    (⚠️ spike C5)
6. reboot
--- new firmware boots in the trial slot ---
7. HealthCheck: framebuffer up? SD mounts? services start? render heartbeat?
8a. healthy → BootControl.markGood(): rewrite autoboot.txt [all]→new slot. Done.
8b. unhealthy / crash / hang → watchdog reboots → normal boot → OLD slot (auto-rollback)
```

### D.5 Why it can't brick
- The **running** slot is never overwritten (we only write the inactive one).
- tryboot is **one-shot**: a trial boot that isn't confirmed reverts by default.
- A hang before confirmation is caught by the **hardware watchdog** → reboot → known-good slot.
- A corrupt/mis-signed bundle is rejected at step 2, before anything is staged.

### D.6 Auto-update policy
- Configurable: `auto` / `notify` / `manual`; default `auto` within a night window.
- WiFi OTA is **opportunistic** (only when online); **USB works with zero network** and is the
  guaranteed path. Both consume the identical signed bundle.
- Channels: `stable` / `beta` selectable in the web UI.

### D.7 Open validation items ⚠️
- **C5:** bare-metal one-shot tryboot trigger (the load-bearing unknown).
- mbedTLS port timing for HTTPS OTA (or ship signed-over-HTTP first).
- Power-loss during `SlotWriter` — inactive slot may be half-written; guard by validating the
  staged kernel (length + signature re-check) before arming tryboot.

---

## Cross-cutting: the spikes this design depends on
| Spike | Proves | Blocks |
|---|---|---|
| C1 | Circle boots + HDMI framebuffer on the real panel | everything |
| C2 | JPEG from SD (FatFs + libjpeg-turbo) | Content, Render |
| C3 | USB pendrive enumerates + reads | Content import, USB OTA |
| C4 | WLAN station join (+ SoftAP feasibility) | WiFi OTA, web UI, sync |
| C5 | tryboot A/B swap + auto-rollback | **all of OTA** |
| C6 | NEON cross-fade & Ken Burns hit target fps | Render transitions |
