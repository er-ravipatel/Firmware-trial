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

## ADR-001 — Base image & OTA engine: Raspberry Pi OS Lite 64-bit + RAUC

- **Date:** 2026-07-17
- **Status:** accepted
- **Context:** Need a base OS supporting the pi3d/OpenGL renderer plus robust OTA + USB
  offline update + auto-update with rollback, on a 512 MB device.
- **Decision:** Raspberry Pi OS Lite 64-bit, read-only rootfs, A/B slots + persistent data
  partition; **RAUC** as the update engine (signed bundles, atomic swap, auto-rollback).
- **Alternatives considered:**
  - *balenaOS* — great built-in OTA/fleet, but Docker + supervisor RAM overhead is too costly
    at 512 MB and adds GPU/DRM friction for pi3d.
  - *Yocto/Buildroot + RAUC* — purest appliance, smallest image, but weeks slower to first
    light and fights the pi3d Python stack. Overkill for a trial.
- **Consequences:** Full GPU/pi3d support and fast iteration; one signed bundle works for both
  OTA and USB. We own the A/B/bootcount integration on the Pi bootloader (spike R2).

## ADR-002 — Renderer: Python + pi3d

- **Date:** 2026-07-17
- **Status:** accepted (pending spike R1)
- **Context:** Need smooth Ken Burns + cross-fade transitions within 512 MB.
- **Decision:** Python + pi3d (OpenGL ES). Proven for Pi photo frames (pi3d PictureFrame).
- **Alternatives considered:** framebuffer viewers (too basic), Chromium kiosk (too heavy for
  512 MB), SDL2/ModernGL (kept as the fallback if pi3d's DRM/GBM backend misbehaves on 64-bit).
- **Consequences:** Fast iteration and rich visuals; **risk** = pi3d DRM/GBM on 64-bit Pi OS,
  de-risked by spike R1 before building around it.

## ADR-003 — Language split: Python app + Go agent

- **Date:** 2026-07-17
- **Status:** accepted
- **Context:** Want fast iteration for UI/services but a robust, small daemon for updates.
- **Decision:** Python (pi3d renderer + FastAPI services); **Go** for the `frame-agent`
  (update orchestration, health self-test, rollback, watchdog).
- **Alternatives considered:** Rust agent (excellent, but Go's cross-compile + HTTP/mDNS story
  is simpler here); all-Python (agent wants to be a small static binary).
- **Consequences:** Clear separation of concerns; two toolchains to maintain.
