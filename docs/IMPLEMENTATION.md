# Lumen Frame — Implementation-Level Design

> The next level of detail below [DESIGN.md](DESIGN.md): concrete byte layouts, state machines,
> class maps, and API surfaces — implementation-ready. Written through **four lenses**, tagged
> where useful: 🔬 researcher (accuracy/prior-art, ⚠️ = verify), 💡 inventor (non-obvious idea),
> ⌨️ coder (concrete structure), 🛡️ firmware (failure/robustness/constraint).

**Global constraints (apply everywhere):** no C++ exceptions or RTTI (bare metal) → use a
`Result<T,E>` type; cooperative scheduling (Circle `CScheduler`/`CTask`) → every long op must
yield and no shared state without a queue; prefer static/bounded allocation; wrap every Circle
handle in RAII.

---

## 1. OTA bundle format + tryboot mechanics

### 1.1 Bundle byte layout ⌨️
Little-endian, single file `*.lfb`:
```
offset  size  field
0       4     magic            = "LFRM"
4       2     format_ver       = 1
6       2     flags            bit0=force_downgrade, bit1=has_assets
8       8     build_id         monotonic (e.g. epoch of build)
16      8     payload_len
24      32    payload_sha256   SHA-256 of payload bytes
56      8     asset_len        (0 if none)
64      ...   payload          kernel8.img [ + asset blob ]
end-64  64    signature        Ed25519 over bytes [0 .. end-64)
```
🛡️ The signature covers the **header too** (build_id, lengths, hash) — so an attacker can't
swap payloads or forge a version. The SHA-256 is a fast integrity pre-check before the (slower)
signature verify.

### 1.2 Signing (offline, `/tools/sign_bundle`) 🔬
- Ed25519 via **libsodium** (host side) / **monocypher** (device side) — same curve, interop
  verified in a unit test. Private key **never** leaves the dev machine (or a hardware token).
- Public key is compiled into the firmware as a 32-byte constant (`kUpdatePubKey`). Rotating it
  requires a firmware update signed by the *old* key → chain of trust.

### 1.3 Verify flow on device ⌨️🛡️
```
1. read 64B header → check magic, format_ver, sane payload_len (< partition size)
2. stream payload from FAT, computing SHA-256 incrementally (bounded RAM, yields periodically)
3. compare SHA-256 == header.payload_sha256           (integrity)
4. monocypher Ed25519 verify(signature, header||payload, kUpdatePubKey)   (authenticity)
5. if build_id <= current_build_id and !force → reject (anti-downgrade / anti-loop)
```
Any failure → abort **before** touching the inactive slot.

### 1.4 A/B slots & `autoboot.txt` 🔬
```
autoboot.txt (permanent state):
[all]
boot_partition=1        # current good slot (1=bootA, 2=bootB)
[tryboot]
boot_partition=2        # the slot we're trialing this reboot only
```
- `SlotWriter` writes the verified kernel to the **inactive** partition, `f_sync`s, then
  **re-verifies the written bytes** (length + SHA) — guards against a torn write.

### 1.5 The tryboot trigger — the C5 unknown 🔬🛡️
Goal: reboot **once** into the trial slot; if not confirmed, next boot reverts automatically.
- On Linux this is `reboot "0 tryboot"`. Bare-metal we must reproduce what the firmware reads.
- **Hypothesis to validate (spike C5):** the Pi bootloader encodes the target boot partition in
  **`PM_RSTS`** (bits 0,2,4,6,8,10) and treats **partition 63 (0x3F) as the "tryboot" sentinel**;
  a watchdog reset then makes the firmware honor it one-shot. So: set those RSTS bits → arm the
  PM watchdog → reset. ⚠️ **This exact encoding must be confirmed against the current rpi
  bootloader** — it's the single most uncertain mechanism in the project.
- 💡 **Belt-and-suspenders (don't trust one mechanism):** also keep a `boot_attempt` counter file
  on the **data** partition. New firmware, on first boot, increments it; on health-confirm it
  deletes it and calls `markGood`. If a boot ever starts with `boot_attempt >= 2`, we *know* the
  last update failed and can force-select the good slot in software — a fallback that works even
  if tryboot semantics differ across bootloader versions.

### 1.6 Confirm / rollback ⌨️
```
markGood():  write autoboot.tmp with [all] boot_partition=<new>; f_sync; f_rename → autoboot.txt
             delete /data/boot_attempt
rollback:    (implicit) unconfirmed tryboot + watchdog reboot → normal boot → old slot
```

### 1.7 Delta updates (later) 💡
WiFi bundles can be large. A `bsdiff`-style delta against the *installed* build (referenced by
build_id) shrinks a typical update from MBs to KBs. USB always carries a full bundle (no
assumption about the installed base). Signed the same way.

---

## 2. Render task + state machine

### 2.1 States ⌨️
```
BOOT_SPLASH → NO_PHOTOS ⇄ SHOWING ⇄ TRANSITION
                         SHOWING → PAUSED → SHOWING
                         SHOWING → SLEEPING → SHOWING
                         any → IMPORT_TOAST (overlay, returns to prior)
```
| State | Draws | Leaves on |
|---|---|---|
| BOOT_SPLASH | logo | init done |
| NO_PHOTOS | "add photos" + web URL/QR | PHOTOS_ADDED |
| SHOWING | current frame (+overlays) | DWELL_EXPIRE / CMD_* / SCHED_SLEEP |
| TRANSITION | blended A→B, per-frame | transition complete → SHOWING |
| PAUSED | frozen frame + subtle indicator | CMD_RESUME |
| SLEEPING | framebuffer blank / HDMI off | SCHED_WAKE / MOTION |
| IMPORT_TOAST | prior frame + "imported N" | timeout → prior state |

### 2.2 Events (from a bounded command queue) ⌨️🛡️
`DWELL_EXPIRE, CMD_NEXT, CMD_PREV, CMD_PAUSE, CMD_RESUME, SCHED_SLEEP, SCHED_WAKE, IMPORT_DONE,
PHOTOS_ADDED, PHOTOS_EMPTY, MOTION(later)`. Web/USB/scheduler tasks **post** events; the render
task is the **only** consumer → no locks on render state. Queue is fixed-size; overflow drops
oldest non-critical event and logs (🛡️ never blocks a producer).

### 2.3 Frame-by-frame transition driver ⌨️
A `Compositor` owns the framebuffer; the state machine tells it *what*, the Compositor decides
*how per tick*:
```
tick(now_ms):
  t = clamp((now_ms - t_start) / duration_ms, 0..1)
  ease = smoothstep(t)                      # 💡 eased, not linear → looks pro
  if mode==CROSSFADE: neon_blend(bufOut, A, B, ease)
  if mode==KENBURNS:  neon_sample(bufOut, B, rect_lerp(r0,r1,ease))
  flip_on_vsync()                           # SetVirtualOffset, ⚠️verify
  if t>=1: post(TRANSITION_COMPLETE)
```
🛡️ Decode of B happens **before** the transition starts (in SHOWING), so a slow decode never
stutters the animation. If decode fails → skip to next photo, log, never crash.

### 2.4 Timing & the loop 🛡️
- Monotonic ms from `CTimer`. The render task loops: process one event → advance compositor →
  **yield** (`CScheduler::Yield`) → **pet the watchdog**. The watchdog pet lives *inside* the
  render loop, so a hung renderer is exactly what triggers recovery.

### 2.5 Inventor layer 💡
- **Smart shuffle:** weighted random that avoids the last N shown and lightly clusters by
  album/date so a "story" plays, not chaos.
- **Motion-aware dwell:** PIR (if fitted) extends dwell / wakes from SLEEPING when someone's near;
  saves the panel when the room's empty.
- **Portrait cover:** for tall photos, fill the letterbox with a blurred, dimmed zoom of the same
  image instead of black bars — looks far more premium.

---

## 3. Firmware module / class map

### 3.1 Folder structure ⌨️
```
/firmware
  /vendor        circle/, circle-stdlib/, libjpeg-turbo/, libpng/, libwebp/, monocypher/  (submodules)
  /src
    /core        App, Config, Result, Logger, EventBus
    /platform    circle bring-up glue, RAII wrappers, Watchdog, Rng
    /display     FrameBuffer, Compositor, Transitions(neon), TextOverlay(freetype), Splash
    /content     ImportSource, SdImporter, UsbImporter, UploadImporter,
                 ImageDecoder, ExifReader, Prescaler, ThumbGen, PhotoIndex, Deduper
    /net         WlanManager, HttpServer, WebAdmin(routes), NetUpdateSource
    /update      UpdateAgent, BundleVerifier, SlotWriter, BootControl, HealthCheck
    /sched       Scheduler(sleep/wake), PowerManager(display blank)
    /util        Sha256, Ed25519(monocypher), Json, RingBuffer, FixedVector
  /web           SPA assets (embedded into firmware, see §4.6)
  main.cpp       CKernel-style entry: bring-up → App::Run()
```

### 3.2 Ownership tree 🛡️
`App` constructs and owns everything (single-owner, no globals):
```
App
├─ Platform (framebuffer, sd, usb, wlan, timer, watchdog, rng)
├─ ContentManager (decoders, index, importers)   ── owns the catalog
├─ RenderTask     (Compositor, state machine)     ── sole render-state owner
├─ NetTask        (WlanManager, HttpServer→WebAdmin, NetUpdateSource)
├─ UpdateTask     (UpdateAgent → Verifier/SlotWriter/BootControl/HealthCheck)
└─ SchedTask      (Scheduler, PowerManager)
```

### 3.3 Concurrency model ⌨️🔬
- Cooperative `CTask`s: `RenderTask`, `NetTask`, `UpdateTask`, `SchedTask`. No preemption →
  fewer races, but **every task must yield** and never busy-wait.
- Cross-task communication = **EventBus** (bounded MPSC queues), not shared mutable state.
  🛡️ This is what lets the render loop stay lock-free and deterministic.

### 3.4 Error handling ⌨️
```cpp
template<class T> using Result = expected<T, Error>;   // no exceptions
// every fallible op returns Result; App logs + degrades (skip photo, retry import,
// abort update) — a subsystem error must never take down the render loop.
```

### 3.5 Inventor layer 💡
A 12-line `EventBus` + a `Result` type are the two abstractions that keep a bare-metal C++
codebase from rotting into spaghetti. Worth building first, before features.

---

## 4. Web admin UI + HEIC transcode

### 4.1 HTTP API surface 🔬⌨️
Served by `HttpServer` (Circle `CHTTPDaemon`, ⚠️verify multipart support — we likely hand-roll
multipart parsing) on the LAN, hostname via mDNS (⚠️ confirm Circle mDNS; else IP only):
```
GET  /                      SPA (embedded)
GET  /api/status            device, build_id, free space, wifi, current photo
GET  /api/photos            paged list (id, thumb url, meta)
GET  /api/photos/{id}/thumb JPEG thumbnail
POST /api/upload            multipart images  → UploadImporter  (JPEG in, HEIC pre-converted)
DELETE /api/photos/{id}     remove + reindex
GET  /api/settings          interval, transition, schedule, overlays
PUT  /api/settings          update settings
POST /api/wifi              {ssid, psk} → WlanManager reconfigure
POST /api/control           {next|prev|pause|resume}  → EventBus
GET  /api/update/check      poll NetUpdateSource
POST /api/update/apply      arm an available update
```

### 4.2 Auth 🛡️
LAN-only but **not** default-open: a password set at first config; HTTP Basic over the local
network for MVP (⚠️ no TLS until mbedTLS port — acceptable on a trusted home LAN, documented as a
limitation). Rate-limit auth attempts.

### 4.3 HEIC transcode — the key move 💡🔬
The frame **never** decodes HEIC. The browser does, before upload:
```
on file picked:
  if type is HEIC/HEIF:
    try:  bitmap = await createImageBitmap(file)     # Safari/iOS decodes natively
    catch: bitmap = await heic2any(file)             # heic2any = libheif compiled to WASM,
                                                      # fallback for Chrome/Firefox/desktop
    canvas.drawImage(bitmap); blob = canvas.toBlob('image/jpeg', 0.9)
    upload(blob)                                      # frame receives JPEG only
  else: upload(file)
```
🔬 Bundling the **heic2any/libheif-WASM** fallback means *any* browser can transcode, not just
Safari — so a desktop Chrome user isn't stuck. 🛡️ The server still validates that every uploaded
byte stream is a real JPEG/PNG/WebP (magic check) and rejects anything else — defense in depth,
never trust the client.

### 4.4 Upload flow ⌨️🛡️
`POST /api/upload` → stream multipart parts to a temp file on `data` (bounded max size, e.g.
≤ 25 MB/part) → `UploadImporter` runs the standard pipeline (decode→EXIF→prescale→thumb→index)
→ post `IMPORT_DONE`. Runs on `NetTask`, never blocks `RenderTask`.

### 4.5 The SPA 💡
Single small HTML/JS page (no framework, no CDN — bare metal serves it): photo grid with
drag-drop upload, live status, settings form, WiFi form, update button. Kept < ~100 KB.

### 4.6 Where the UI lives 🛡️
Embed the SPA assets **in the firmware image** (a generated `web_assets.cpp` byte array), not on
FAT — so the management UI works even if the data partition is empty/corrupt, and can't be
tampered with independently of a signed update.

---

## How the four topics interlock
- The **EventBus** (§3.3) is what lets Web (§4), Scheduler, and USB post `next`/`import`/`sleep`
  into the **render state machine** (§2) without locks.
- The **web UI** (§4) arms updates that the **OTA agent** (§1) executes; both share the signed
  bundle and the `boot_attempt` safety counter.
- The **HEIC boundary transcode** (§4.3) is what keeps the **content pipeline** (DESIGN §B) pure
  JPEG/PNG/WebP, which is what makes bare-metal decode tractable at all.
- Everything pets one **watchdog** from inside the **render loop** (§2.4) — the health signal the
  **OTA rollback** (§1.6) depends on.
