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

Full spec: [docs/PRD.md](docs/PRD.md).

> This file is the project's "master prompt." Claude Code loads it automatically at the
> start of every session, so anything written here does not need to be repeated in chat.
> Edit it freely as the project takes shape.

## Project overview

_Describe what the firmware does, the target MCU/board, and the toolchain here._

## Commands

_Fill these in once the build system exists. Examples:_

- Build:  `<build command>`   <!-- e.g. make, west build, cargo build -->
- Flash:  `<flash command>`   <!-- e.g. make flash, west flash, cargo flash -->
- Test:   `<test command>`
- Clean:  `<clean command>`

## Conventions

- Prefer clear, small functions; keep them focused.
- Match the style of surrounding code (naming, indentation, comment density).
- No dynamic memory allocation inside interrupt service routines (ISRs).
- Keep hardware register access behind a HAL/abstraction layer where practical.
- Document any non-obvious timing, register, or hardware assumptions with a comment.

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
