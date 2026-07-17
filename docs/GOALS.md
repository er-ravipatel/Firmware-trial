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
- Target hardware: Raspberry Pi Zero 2 W (quad A53, **512 MB RAM**), HDMI out, 2.4 GHz WiFi.
- Display: scrap laptop LCD + generic HDMI TV driver board; native resolution **TBD**.
- Offline-first: core features must work with no internet.
- No cloud lock-in; one signed artifact for all update paths.
