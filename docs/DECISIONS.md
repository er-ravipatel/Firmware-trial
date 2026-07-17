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
