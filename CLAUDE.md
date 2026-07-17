# Firmware-trial

Embedded firmware project. Tech stack / target hardware: **TBD — fill in once decided.**

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

## Notes for Claude

- If a command or convention above is still a placeholder, ask me before assuming one.
- Prefer showing me a plan for anything that touches boot, memory layout, or hardware config.
