# Goals

_The "why." See [PRD.md](PRD.md) for the full spec._

## Mission
Build a polished **digital photo frame appliance** on a Raspberry Pi Zero 2 W driving a
repurposed laptop LCD (via a generic HDMI TV board) — that feels like a consumer product:
effortless setup, effortless content, self-maintaining via OTA.

## Success criteria
- [ ] Powers on to a fullscreen slideshow with smooth transitions, no keyboard/console needed.
- [ ] A non-technical user can join WiFi via phone (SoftAP onboarding) in under 2 minutes.
- [ ] Photos can be added by SD, USB pendrive, web upload, and WiFi sync.
- [ ] Firmware updates apply over the air atomically and roll back automatically on failure.
- [ ] Survives sudden power loss and bad updates without bricking or corruption.
- [ ] Runs comfortably within the 512 MB RAM budget.

## Non-goals (for now)
- Battery/portable operation (mains-powered wall frame).
- Touchscreen input (HDMI display is output-only).
- A hosted cloud service we operate (self-host / offline-first instead).

## Constraints
- **Bare-metal firmware OS** in C++ on the Circle framework — **no Linux**. The Pi boots our
  `kernel8.img` directly. Authenticity ("a genuine firmware OS") is a first-class goal.
- Target hardware: Raspberry Pi Zero 2 W (BCM2837, quad A53, **512 MB RAM**), HDMI, 2.4 GHz WiFi.
- Display: Acer Aspire 4347 (14") LCD + generic HDMI TV board; **1366×768** (confirm).
- Offline-first: core features must work with no internet; **USB is the guaranteed update path**.
- HEIC handled by **boundary transcode** (browser/PC/network), not on-device.
- No cloud lock-in; one signed artifact for all update paths.
- Accepted trade-off: a **months-long build** in exchange for genuine bare-metal firmware.
