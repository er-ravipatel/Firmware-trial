# Firmware-trial — Lumen Frame

**Modular smart display** (InkyPi-inspired) firmware for a **Raspberry Pi Zero 2 W** (512 MB RAM)
driving a repurposed laptop LCD over HDMI (via a generic TV driver board). Rotating screen
**plugins** — photo frame first, then clock/weather/calendar/news — managed via a web UI.
Photo-frame-first, plugin-ready (see ADR-008/009).

**Architecture: a genuine bare-metal firmware OS in C++ on the [Circle](https://github.com/rsta2/circle)
framework — no Linux.** The Pi boots our `kernel8.img` directly.

Stack: Circle (`RASPPI=3`, AArch64) + circle-stdlib · **C++** app layer (render engine, content
pipeline, web-admin, update agent) · Circle drivers (framebuffer, SD/FatFs, USB-MSD, WLAN+lwIP) ·
custom A/B OTA via Pi `tryboot` · HEIC via boundary transcode (browser/PC). Build via **WSL2** +
`aarch64-none-elf`. Software (NEON) rendering — no GPU/GLES bare-metal.

Full spec: [docs/PRD.md](docs/PRD.md). **Resuming work? Read [docs/STATUS.md](docs/STATUS.md)
first** — current state, build/run commands, findings, and the pending next-step decision.

> This file is the project's "master prompt." Claude Code loads it automatically at the
> start of every session, so anything written here does not need to be repeated in chat.
> Edit it freely as the project takes shape.

## Project overview

Lumen Frame is a digital photo frame implemented as a **bare-metal firmware OS in C++ on Circle**
(no Linux) for a **Raspberry Pi Zero 2 W** (BCM2837, `RASPPI=3`, AArch64), driving an Acer Aspire
4347 LCD (1366×768) over HDMI. Photos load from SD/USB, decode via vendored stb_image, and display
as a Fit + blurred-background Ken Burns slideshow with cross-fade. Current release: **v0.1.0-beta
"offline"** (runs on real hardware; WiFi/web-UI is the next milestone). See [docs/STATUS.md](docs/STATUS.md).

## Commands

Firmware builds in **WSL2 Ubuntu** — invoke via `wsl bash -lc "..."` (the agent's Bash tool is Git
Bash and cannot see `/mnt/c`). Repo path in WSL: `/mnt/c/Workspace/Personal/Firmware-trial`.

- Build:  `wsl bash -lc "cd /mnt/c/Workspace/Personal/Firmware-trial/firmware/app && make -j4"`  → `kernel8.img`
- Deploy: `Copy-Item C:\Workspace\Personal\Firmware-trial\firmware\app\kernel8.img D:\ -Force` then re-seat the SD card in the Pi (photos live on the same card under `photos/`)
- Test:   `wsl bash -lc "cd /mnt/c/Workspace/Personal/Firmware-trial && ./tools/run_host_tests.sh"`  (host unit tests, no hardware)
- Run (emulator, live SDL window): `wsl bash -lc "export DISPLAY=:0 && bash tools/run_qemu.sh firmware/app/kernel8.img firmware/app/sd.img"`
- Clean:  `wsl bash -lc "cd .../firmware/app && make clean"`  (do this when headers change)

## Conventions

- Prefer clear, small functions; keep them focused.
- Match the style of surrounding code (naming, indentation, comment density).
- No dynamic memory allocation inside interrupt service routines (ISRs).
- Keep hardware register access behind a HAL/abstraction layer where practical.
- Document any non-obvious timing, register, or hardware assumptions with a comment.

### Bare-metal rules learned the hard way (full detail in [docs/LEARNINGS.md](docs/LEARNINGS.md))

- **No thread-local storage.** `__thread` / TLS faults on bare metal (`tpidr_el0` is uninitialized).
  Compile third-party code with TLS disabled (e.g. `STBI_NO_THREAD_LOCALS`).
- **Don't malloc/free large buffers repeatedly.** Circle's heap leaks blocks >512 KB. Use a fixed
  pre-allocated pool + reused work buffers for anything big and per-frame/per-photo.
- **Fullscreen = write the back buffer directly** (`GetBuffer()`, `u32` per pixel). Never `DrawPixel`
  in a hot loop.
- **Hardware framebuffer is RGB; QEMU's is BGR.** Hardware is ground truth — expect the emulator to
  look colour-swapped and don't "fix" it. Build `DEPTH=32`.
- **Storage is optional / non-fatal.** Init graphics first; always keep an embedded fallback so the
  frame works with no SD and no USB.
- **Shared logic is freestanding** (C headers, no `std::`) so it compiles for both host tests and
  the `-nostdinc++ -fno-exceptions` firmware. Prefer byte-loops over `memset`/`memcpy` (avoids
  `_FORTIFY_SOURCE` `__*_chk` link errors).
- **On-hardware debugging = log to SD** (`f_write` + `f_sync` per line) and read the card on a PC.
  Instrument timings (decode/scale ms, fps) before optimizing.

## Guardrails (things to always/never do)

- **Never** edit third-party/vendor code (e.g. `vendor/`, SDK folders) without being asked.
- **Always ask** before changing linker scripts, memory maps, or startup/boot code.
- **Always ask** before force-pushing or rewriting git history.
- Don't commit build artifacts or secrets — keep them out via `.gitignore`.
- When unsure about hardware behavior, ask rather than guessing.

## Project docs

These files carry the project's intent and history — read the relevant one before
working in that area, and keep them updated as part of the work:

- [docs/PRD.md](docs/PRD.md) — full product & engineering spec (the master "detailed prompt").
- [docs/DESIGN.md](docs/DESIGN.md) — deep subsystem design (boot/layout, content, render, OTA).
- [docs/IMPLEMENTATION.md](docs/IMPLEMENTATION.md) — implementation-level detail: bundle byte
  layout + tryboot, render state machine, C++ class map, web API + HEIC transcode.
- [docs/SCENARIOS.md](docs/SCENARIOS.md) — behavioral spec: how the deployed product behaves
  (online/offline, power loss, updates, faults, recovery) as testable Given/When/Then.
- [docs/GOALS.md](docs/GOALS.md) — mission, success criteria, non-goals, constraints.
- [docs/ROADMAP.md](docs/ROADMAP.md) — milestones and their target outcomes.
- [docs/DECISIONS.md](docs/DECISIONS.md) — architecture decisions (ADRs) and rationale.
- [docs/OUTCOMES.md](docs/OUTCOMES.md) — expected vs. actual results per milestone.
- [TESTPLAN.md](TESTPLAN.md) — what to test, how, and pass/fail criteria.
- [docs/RETROSPECTIVE.md](docs/RETROSPECTIVE.md) — reflections after each milestone.
- [docs/LEARNINGS.md](docs/LEARNINGS.md) — reusable lessons; promote durable ones into this file.
- [CHANGELOG.md](CHANGELOG.md) — notable changes over time.

**The refinement loop:** goals → build → test → outcomes → retrospective → learnings →
promote durable lessons back into this file (CLAUDE.md) and GOALS.md.

## Notes for Claude

- If a command or convention above is still a placeholder, ask me before assuming one.
- Prefer showing me a plan for anything that touches boot, memory layout, or hardware config.
