# Learnings

_Append-only log of reusable lessons. When something bites you once, write it here so
it never bites you the same way twice. Promote the durable ones into
[../CLAUDE.md](../CLAUDE.md) so they apply automatically._

## Format
`YYYY-MM-DD — <short lesson>. Context: <what happened>. Rule: <what to do now>.`

## Log

### Bare-metal C/C++ (Circle, AArch64) — the ones that cost us the most

- **2026-07-18 — Thread-local storage faults on bare metal.** Context: the firmware froze at
  "decode embedded"; objdump of the faulting PC showed `mrs x0, tpidr_el0` — stb_image uses
  `__thread` locals, and on bare metal `tpidr_el0` is uninitialized, so any TLS access data-aborts.
  Rule: **compile third-party code with any thread-local / TLS feature disabled**
  (`STBI_NO_THREAD_LOCALS`); when a bare-metal hang has no obvious cause, disassemble around the
  faulting PC before theorizing. (Earlier wrong guesses — NEON, un-zeroed heap — wasted time.)

- **2026-07-18 — Circle's heap leaks blocks larger than its max bucket.** Context: OOM crash after
  ~24 s of slideshow. `CMemorySystem`/`DoFree` only returns blocks ≤512 KB (the largest bucket) to
  the free list; anything bigger is never reclaimed. Decoding megapixel JPEGs allocated multi-MB
  buffers that leaked every photo. Rule: **for large, repeated allocations on Circle, don't rely on
  malloc/free — use a fixed pre-allocated pool (bump allocator) + reused work buffers.** We gave stb
  a 96 MB pool reset per decode, and kept two fixed decoded-image buffers.

- **2026-07-18 — Per-pixel framebuffer APIs are far too slow for fullscreen.** Context: render was
  122 ms/7 fps using `C2DGraphics::DrawPixel` per pixel. Rule: **for fullscreen blits, get the raw
  back-buffer pointer (`GetBuffer()`) and write `u32` pixels directly** in a tight loop
  (`0xFF000000 | COLOR2D(r,g,b)`). Dropped to ~41 ms/21 fps. Reserve the pixel/rect/text calls for
  small UI.

- **2026-07-18 — Framebuffer color order differs QEMU vs. hardware.** Context: colors looked
  R/B-swapped. QEMU's raspi3b framebuffer is **BGR**; the real Pi framebuffer is **RGB**. If you
  swap to fix the emulator, hardware looks wrong, and vice-versa. Rule: **treat hardware as ground
  truth (RGB pass-through); expect the emulator to look colour-swapped and don't "fix" it.** Also
  build at `DEPTH=32` — 16-bit caused visible banding.

- **2026-07-18 — Circle USB hotplug needs plug-and-play enabled at construction.** Context: inserting
  a pendrive at runtime asserted `m_bPlugAndPlay`. Rule: construct `CUSBHCIDevice` with
  `bPlugAndPlay = TRUE`, then poll `UpdatePlugAndPlay()` in the main loop. Handle **both** insert
  (mount + switch source) and remove (`f_mount(0, "USB:", 0)` to release the gone volume, then fall
  back) — forgetting removal left a stale mount and odd behaviour.

- **2026-07-18 — Storage init must be non-fatal.** Context: boot aborted when no SD card was present
  (`m_EMMC.Initialize()` returns false). Rule: **initialize graphics first, treat all storage init
  as optional/non-fatal**, and always keep an embedded-asset fallback so the device does something
  useful with no SD and no USB.

- **2026-07-18 — Freestanding shared code, one source for host tests + firmware.** Context: Circle
  builds with `-nostdinc++ -fno-exceptions`. Rule: **shared modules use C headers (`<stdint.h>`,
  not `<cstdint>`) and no `std::`** — then the exact same code compiles for host unit tests (full
  stdlib) and for the bare-metal target. Big multiplier: logic is unit-tested on the host where
  iteration is instant.

- **2026-07-18 — Avoid libc hardening builtins in freestanding code.** Context: a link error for
  `__memset_chk` (from `_FORTIFY_SOURCE`) when using `memset`. Rule: in bare-metal paths, prefer an
  explicit byte-loop over `memset`/`memcpy`, or ensure the fortified builtins aren't pulled in.

### Product / rendering

- **2026-07-18 — "Fill the screen" ≠ crop the subject.** Context: cover-scaling filled the screen but
  cut 30 %+ off portraits ("main content gets out of screen"). Rule: **fit the whole photo (never
  crop), and fill the empty space with a darkened, heavily-blurred copy of the same image** (tiny
  downscale → bilinear upscale, computed once per photo). Looks premium and loses nothing. Keep any
  Ken Burns zoom gentle (≤1.06) so it doesn't re-introduce edge cropping.

- **2026-07-18 — Decode cost scales with source megapixels — resize at ingest, not runtime.**
  Context: 29 MP card photos took ~3.8 s to decode and stuttered. Rule: **resize photos to ~1920 px
  at staging time** (`tools/_resize_dir.sh`) → ~290 ms decode. Do expensive image prep on the PC,
  not on the Pi.

- **2026-07-18 — Real-time-driven animation, not tick-driven.** Context: fixed-tick animation is
  frame-rate dependent and looks jerky when frame time varies. Rule: **drive zoom/pan/fade from a
  real millisecond clock** (`CTimer::GetClockTicks()`), so motion pace is constant regardless of fps.
  Remaining hitches then come from work blocking the loop (e.g. decoding the next photo on the same
  core) — the fix is to move that work off the render core, not to change the timing model.

### Debugging on real hardware (no debugger)

- **2026-07-18 — Persist a log to the SD card; it's your only window into the Pi.** Context: on
  hardware there's no serial console handy and the screen only shows the final state. Rule: **retarget
  the logger to a file on SD (`f_write` + `f_sync` every line) so boot, exceptions, and perf stats
  survive a power-cut**; pull the card, read it on the PC, diagnose. `f_sync` matters — without it a
  crash loses the buffered tail that names the culprit.

- **2026-07-18 — Instrument timings before optimizing.** Context: "some images are jerky" was vague.
  Rule: **log per-photo (decode ms, scale ms, dimensions) and per-second (fps, render/present
  avg+max, fade frames) stats.** The numbers pinpointed the hitch as a load stall (decode+scale
  blocking the loop), not a rendering or fade problem — so we fixed the right thing.

### Toolchain / environment (Windows + WSL)

- **2026-07-18 — Two different "bash" environments on this machine.** Context: the agent's Bash tool
  is **Git Bash** (`/mnt/c` doesn't exist there); the firmware build lives in **WSL2 Ubuntu**
  (`/mnt/c` is the Windows drive). Rule: **run firmware builds via `wsl bash -lc "..."`, not the
  Bash tool.** Build: `cd /mnt/c/.../firmware/app && make`.

- **2026-07-18 — PowerShell → wsl → bash mangles quoting.** Context: `$(...)`, `\$`, empty `""`
  args, `<`/`>`, and heredocs get eaten crossing the shell layers. Rule: **put any nontrivial shell
  logic in a `tools/*.sh` script and call it with full-path args**; use a sentinel (we used `-`) for
  "no value" positional args since empty strings get dropped.

- **2026-07-18 — Deploy = copy `kernel8.img` to the SD card root.** Context: no flashing tool needed
  once the boot partition is set up. Rule: rebuild → `Copy-Item kernel8.img D:\` → re-seat the card
  in the Pi. Keep photos on the same card under `photos/` (scanner tries `photos/` → `images/` → root).

- **2026-07-18 — Stock QEMU can't exercise Circle networking or WiFi.** Context: NIC enumerates but
  RX is broken (`dhcp: No response`); CYW43 WiFi is never emulated. Rule: **plan WiFi/web-UI work as
  host-unit-tested logic + on-hardware live test**, or build the patched QEMU (larchcone) for
  Ethernet only. Don't burn time trying to make WiFi work in the emulator.

### v0.2 — multicore, storage-hotplug robustness, splash (2026-07-18)

- **2026-07-18 — Move CPU-bound work off the render core; don't optimize the timing model.** Context:
  a ~440–840 ms freeze hit once per photo. It was the next photo's decode+scale running *inline* on
  the render core (`render_max = decode + scale + ~40 ms`, matched every `load:` line in the log).
  Rule: **for periodic CPU-bound work that stalls the UI, run it on another core** — here core 1 as a
  dedicated JPEG decoder while core 0 renders at ~40 ms/frame. Interleaving/incremental decode on one
  core doesn't help (stb can't yield). See the multicore rules below.

- **2026-07-18 — Enabling Circle multicore is a global rebuild, not a link-time add.** Context:
  `CMultiCoreSupport` and *all* secondary-core startup are `#ifdef ARM_ALLOW_MULTI_CORE`; without it,
  cores 1–3 are parked at boot. Rule: **add `-DARM_ALLOW_MULTI_CORE` in `vendor/circle/Config.mk`,
  then CLEAN-rebuild libcircle + addons** (`./makeall clean && ./makeall`, and clean+rebuild
  `addon/fatfs` + `addon/SDCard`) — Rules.mk doesn't track Config.mk flag changes, so an incremental
  build silently mixes ABIs. Subclass `CMultiCoreSupport`, construct with `CMemorySystem::Get()`,
  call `Initialize()` LAST; each secondary core runs `Run(nCore)` — an infinite loop for a worker,
  or return to halt that core. The app picks up the define too (its Makefile includes Config.mk).

- **2026-07-18 — Cross-core handshake from freestanding code = GCC atomic builtins.** Context: the
  photo plugin is freestanding (no Circle headers) but the core-0/core-1 handshake needs memory
  barriers. Rule: use `__atomic_load_n/__atomic_store_n` with `__ATOMIC_ACQUIRE`/`__ATOMIC_RELEASE`
  (and `__ATOMIC_RELAXED` for owned flags) — compiler intrinsics, no headers, correct `DMB` on ARM.
  A53 caches are hardware-coherent (inner-shareable once multicore is on), so acquire/release on the
  done-flag safely publishes the worker's buffer writes. Keep the buffers disjoint per job (core 0
  reads `cur_`, worker writes `next_`) and swap pointers only on core 0 when the worker is idle.
  Give the worker an idle nap (`SimpleMsDelay(2)`) so it isn't a 100 % busy-spin between jobs, and a
  core-0 inline fallback so a multicore failure degrades to the old behaviour instead of hanging.

- **2026-07-18 — Never do device I/O on a plug-and-play *event*; check presence via the name
  service.** Context: an intermittent (~1-in-7) reboot on USB removal. The hotplug handler called
  `f_mount("USB:",1)` on every PnP event; the `1` forces FatFs to read sector 0, which **races
  Circle's device teardown on removal** — `disk_read` is only null-safe *after* the removed-callback
  runs, so losing the race read freed memory → data abort → reset (no panic dump). Rule: **decide USB
  presence with `CDeviceNameService::GetDevice("umsd1", TRUE)` (pure lookup, no I/O)**; mount+scan only
  on a confirmed *insert*; on *removal* touch nothing on the device (just `f_mount(0,…)` to release
  the volume) and fall back. Worst case is being one loop late — never a crash.

- **2026-07-18 — Reading a crash out of an append log: time-reset + missing handler line + no panic.**
  Context: distinguishing real crashes from manual power-cycles across 6 appended boots. Rule: a
  **crash** = the timestamp jumps back to ~0 (reboot) mid-operation AND the expected follow-up log
  line never printed AND there's no exception dump (a hard fault resets before the handler can flush).
  A **manual power-cycle** = a clean stop at an arbitrary point with the prior run intact. Counting
  events (7 removals, 6 “back to SD”, 1 crash) pinpoints an intermittent bug precisely.

- **2026-07-18 — `FA_CREATE_ALWAYS` truncates every boot; append + cap for durable logs.** Context:
  each boot wiped the SD log, so a crash-then-reboot lost its own evidence. Rule: open the log with
  `FA_WRITE | FA_OPEN_APPEND`, separate runs by the boot banner (no RTC → timestamps reset to 0 each
  boot), and roll over to one `.old` generation past a size cap so it can't fill the card. `f_rename`
  wants the new name WITHOUT a drive prefix (drive comes from the old name).

- **2026-07-18 — Ship one image; gate diagnostics by config + build channel.** Context: production
  frames shouldn't write the card every log line, but dev builds must never be silent. Rule: read a
  tiny `SD:/lumen.conf` (`key = value`) at boot for a `logging` flag (default OFF = production), and
  **force it ON when the version string contains `beta`/`dev`** (case-insensitive substring on
  `LUMEN_VERSION`). One `kernel8.img` serves both; a bare semver ships quiet.

- **2026-07-18 — Software gradients: dither to kill banding; reuse existing buffers.** Context: a
  modern web-hero splash on a software renderer. Rule: build a diagonal multi-stop gradient once into
  a buffer (interpolate along `(tx+ty)/2`), add a cheap per-pixel dither (`±((x*13+y*7)&7)-4`) so
  24-bit gradients don't band, then `blit_rgb` it; cross-fade to the photo with `blit_rgb_blend`.
  Reuse a spare fullscreen buffer (the plugin's incoming-frame buffer) instead of allocating another
  3 MB. `C2DGraphics::DrawText` supports `AlignCenter` + larger fonts (`Font12x22`) for a title card;
  colours scale by an alpha for fade in/out. (Remember: hardware fb is RGB, QEMU is BGR.)

### v0.3 — WiFi/SoftAP bring-up (2026-07-18)

- **2026-07-18 — `NO_SDHOST` is a QEMU-only setting; on real hardware it breaks WiFi.** Context: the
  SoftAP spike failed with `wlan: can't find brcmfmac43430-sdio.bin` + `emmc: EnsureDataMode() error
  sending CMD13`, even though the blob was on the card and the WiFi chip enumerated
  (`ether4330: chip 43430`). Root cause: on Pi 3 / Zero 2 W the **WiFi SDIO uses the EMMC (Arasan)
  controller**; our `-DNO_SDHOST` (added so the SD card works in QEMU, which has no SDHOST) forced the
  **SD card onto EMMC too** → SD and WiFi collide → WLAN can't read its firmware off the SD. Rule:
  **for any WiFi build on real hardware, use SDHOST for the SD card (drop `NO_SDHOST`)** so the EMMC
  controller is free for the WiFi SDIO. `CEMMCDevice` is a *unified* SD driver — with `USE_SDHOST`
  defined it drives the SDHOST controller (logs `sdhost: emmc1: sdhost-bcm2835 loaded`), still
  registering the FatFs volume as `emmc1`. Consequence: **QEMU can no longer read an SD image** (no
  SDHOST) → keep `NO_SDHOST` behind a QEMU-only build switch; use the embedded image for QEMU.

- **2026-07-18 — CYW43 firmware loads from `SD:/firmware/` at runtime, not embedded.** Context:
  Circle's `CWLAN(FIRMWARE_PATH)` reads the brcmfmac blobs from the card. Rule: copy the whole
  `addon/wlan/firmware/*.{bin,txt,clm_blob}` set to `SD:/firmware/`; the driver auto-selects by
  detected chip. The **Pi Zero 2 W enumerates as chip 43430** (not 43436s as the model suggests), so
  `brcmfmac43430-sdio.bin` is the one it actually loads. Fetch the blobs once via the addon Makefile
  (needs internet); build `libwlan.a` (`make` in `addon/wlan`). SoftAP: `m_WLAN.Initialize()` →
  `CreateOpenNet(ssid, channel, hidden)` → `CNetSubSystem(...NetDeviceTypeWLAN)`. **Unemulatable —
  hardware-only.** Spike W1 PASSED on the real Zero 2 W (firmware ready, MAC up, AP visible).

### v0.3 — large photo libraries (2026-09-04)

- **2026-09-04 — `CKernel` lives on the 128 KB kernel stack; big tables must be heap-allocated once.**
  Context: a 10k-photo pendrive showed only the first 64 files because `CSdPhotoSource` kept a fixed
  `m_Paths[64][64]` inline, and it could not simply be enlarged — `CKernel Kernel;` is a local in
  `main()` on Circle's `KERNEL_STACK_SIZE` (0x20000) stack, so a 1.5 MB member array would overflow
  it silently. Rule: **any table beyond a few KB inside the kernel object is a one-time `malloc` on
  first use, never freed** (freeing > 512 KB leaks anyway). Same for the per-photo file buffer:
  a fixed 32 MB buffer allocated once replaced `malloc(size)`/`free` per photo, which leaked every
  full-size (> 512 KB) camera JPEG until the frame ran out of RAM and rebooted — which *also* looks
  like "only a few photos in a loop". Scan recurses sub-folders (depth 4) and logs only the first 12
  names plus a count, so a 10k drive cannot flood the SD log. Verified in QEMU with a 300-file USB image.

- **2026-09-05 — Size the decode pool for the biggest camera on the card, and reject the rest from
  the header.** Context: the first 10 h field log on an 8,222-photo drive showed 80 `DECODE FAILED`
  lines — all 6000×4000 (24 MP) from one DSLR; the drive also holds 26 files of 32–64 MP (phone
  "high-res" shots) not yet reached. stb is a bump-pool consumer here: ~1.5 B/px raw planes + 3 B/px
  RGB out + 3 B/px more when EXIF-rotated → a 24 MP portrait needs ~180 MB, so the 96 MB pool was
  always going to fail it, after ~2.2 s of wasted core-1 time. Rule: **pool = 192 MB** (covers 24 MP
  incl. rotation; heap still ~170 MB free), and **pre-check the header with `stbi_info_from_memory`**
  so anything the pool can't hold is rejected in microseconds with a logged reason (`too large for
  decode pool`, `bad header`). The slideshow **skips** an undecodable file immediately instead of
  showing a dark slide for a dwell (bounded to 64 consecutive skips). Verified on the host against
  the real DSC_0483.JPG (24 MP, now OK) and 20220419_225232.jpg (64 MP, rejected), then in QEMU.
  The 64 MP class still needs downsizing off-device (`tools/_resize_dir.sh`) or the phone converter.

- **2026-09-05 — A boot-time log cap is not a cap.** Context: `lumenlog.txt` reached 6.3 MB against a
  1 MB cap because the rollover check ran only in `Open()`, and the per-second `perf:` line wrote
  36k lines in 10 h. Rule: check `f_size` every N writes and rotate mid-run; log aggregates every
  10 s, not every second.

### IR remote / GPIO (2026-09-30)

- **2026-09-30 — `CGPIOPin` needs its `CGPIOManager` at construction for interrupts.** Context:
  `ConnectInterrupt()` asserted `m_pManager != 0` and halted the boot; the default constructor +
  `AssignPin()` never sets the manager. Rule: when the pin number comes from config, create the pin
  with `new CGPIOPin(pin, mode, &manager)` in `Initialize()` (one-time allocation) rather than as a
  default-constructed member. Enable both edges with `EnableInterrupt` + `EnableInterrupt2`.

- **2026-09-30 — A dynamically-initialised function-local `static` does not link bare-metal.**
  Context: `static const char *s = m_Config.GetStr(...)` inside `Run()` produced
  `undefined reference to __getauxval` from libgcc's `lse-init.o`. The C++ guard-variable runtime
  (`__cxa_guard_acquire`) uses outline atomics, which pull in an LSE-detection routine that needs
  libc. Rule: no function-local statics with runtime initialisers in firmware; use members or
  locals declared before the loop. (`constexpr`/constant-initialised statics are fine.)

- **2026-09-30 — Keep the ISR to a timestamp and a ring push.** Context: NEC pulses are ~562 µs;
  the render loop runs at ~40 ms per frame, so polling would miss everything, but decoding in the
  ISR would violate the no-alloc/no-log rule. Rule: ISR = `GetClockTicks()` delta + level read +
  push into a fixed ring with an atomic-release head; the main loop drains and decodes once per
  frame. Drop on overflow, never block. A freestanding decoder (pulse widths in) is host-testable
  with synthetic trains — jitter, noise, truncation — before any hardware exists.

- **2026-09-30 — Freeze the slideshow with a virtual clock, not by touching the state machine.**
  Context: Pause had to stop Ken Burns, fades and dwell together. Rule: derive every timing from
  `vtime() = now − paused_time`; pausing freezes the returned value, resuming adds the paused span
  to the offset. Nothing else changes; hard-cut instead of fade while paused so a dissolve can't
  freeze half-blended.

- **2026-09-30 — Read `lumen.conf` from the pendrive when there is no SD.** Context: the SDHOST
  build cannot read an SD image in QEMU, which blocked config-driven debug paths (`ir_sim`). Rule:
  fall back to `USB:/lumen.conf` at boot when the SD is absent — QEMU with a USB image then
  exercises any config flag.
