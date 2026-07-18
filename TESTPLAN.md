# Test Plan

_What we test, how, and the pass/fail bar. Rows trace to behavioral scenarios in
[docs/SCENARIOS.md](docs/SCENARIOS.md) (**S#**), design spikes (**C#**, [docs/DESIGN.md](docs/DESIGN.md)),
and success criteria in [docs/GOALS.md](docs/GOALS.md). Architecture: bare-metal C++/Circle._

## Test types
- **U — Host unit test:** runs on the dev machine (WSL2), no hardware. Pure logic: bundle
  verify, EXIF parse, index journaling, playlist/shuffle, state-machine transitions, JSON.
- **T — On-target smoke:** runs on the Pi, automated/observable (serial log + screen check).
- **H — Manual HIL** (hardware-in-the-loop): physical actions — pull power, insert USB, unplug
  HDMI, toggle WiFi — with a human/rig observing.

## How to run
- Unit:   `<host test command>`  _(TBD — set up a host build of the pure-logic modules)_
- Target: flash `kernel8.img`, watch serial `<115200>` + screen; `<smoke harness TBD>`
- HIL:    follow the manual steps per row; record pass/fail + notes.

## Priority tiers
- **P0 — Safety-critical:** power-loss, update rollback, offline. A P0 failure blocks release.
- **P1 — Core product:** boot/display, slideshow, import, WiFi, web UI.
- **P2 — Delight/optional:** transitions polish, overlays, sensors, sync.

_Status legend: ⬜ not run · 🟡 partial · ✅ pass · ❌ fail · 🚫 blocked (dep not built)_

---

## P0 — Safety-critical

| ID | Verifies | Scenario | Type | Pass criteria | Status |
|----|----------|----------|------|----------------|--------|
| P0-01 | Cold boot after outage | S6.1 | T | Power on → first photo in < ~10 s, no input | 🚫 |
| P0-02 | Power cut mid-photo-write | S6.2 | H | Cut power during import ×20 → library never corrupt; at most the in-flight file missing | 🚫 |
| P0-03 | Index journaling survives power cut | S6.2,S10.3 | U+H | Kill during index write → A/B copy or scan-rebuild yields a valid index | 🚫 |
| P0-04 | Power cut mid-update | S6.3 | H | Cut power while staging slot ×10 → boots intact running slot every time | 🚫 |
| P0-05 | No boot loop under brownout | S6.4 | H | Repeated power cycling → always reaches a good slot; `boot_attempt` forces good after 2 | 🚫 |
| P0-06 | Successful OTA end-to-end | S7.1 | T | New signed build stages→tryboot→health-good→marks good; photos+settings preserved | 🚫 |
| P0-07 | Bad-build auto-rollback | S7.2 | T+H | Deliberately-failing build → watchdog reboot → reverts to previous build automatically | 🚫 |
| P0-08 | Reject tampered/unsigned bundle | S7.3,S12 | U+T | Flipped byte / wrong key → rejected before staging; running slot untouched | 🚫 |
| P0-09 | Anti-downgrade / no update loop | S7.4 | U | Older/equal build_id refused unless force flag | ⬜ |
| P0-10 | Interrupted OTA download | S7.5 | T | Drop WiFi mid-download → partial discarded; nothing staged until complete+verified | 🚫 |
| P0-11 | Staged-slot re-validation | S6.3,S7.5 | U | Half-written slot fails length+SHA recheck → not armed for tryboot | ⬜ |
| P0-12 | Full offline operation | S4.1 | T+H | With no network: slideshow, USB import, LAN web UI, USB update, sleep/wake all work | 🚫 |
| P0-13 | USB offline update (guaranteed path) | S4.3,S7.1 | T | Signed bundle on pendrive installs with zero network | 🚫 |

## P1 — Core product

| ID | Verifies | Scenario | Type | Pass criteria | Status |
|----|----------|----------|------|----------------|--------|
| P1-01 | Circle boots + framebuffer on real panel | C1,S9.1 | T | Test image fills 1366×768 correctly on the Acer panel via the TV board | 🟡 emu-pass |
| P1-01e | Circle boots + framebuffer in QEMU | C1 | T | Sample renders to 640×480 framebuffer, captured via screendump | ✅ |
| P1-02 | HDMI timing pinned despite bad EDID | S9.1 | H | Board misreports → forced mode/cvt still drives correct resolution | 🚫 |
| P1-03 | HDMI unplug/replug recovers | S9.2 | H | Pull + reinsert cable → image returns, no reboot | 🚫 |
| P1-04 | Screen power-cycled independently | S9.3 | H | Wall-switch the display off/on → slideshow still present | 🚫 |
| P1-05 | JPEG from SD decodes + displays | C2,S2.1 | T | libjpeg-turbo decodes a known JPEG from FatFs and shows it | 🚫 |
| P1-06 | PNG/WebP decode | S3,S5 | U+T | Known PNG + WebP decode to correct pixels | ⬜ |
| P1-07 | EXIF orientation applied | S2.1 | U+T | All 8 EXIF orientations render upright | ⬜ |
| P1-08 | Fit/letterbox correct | S2.1 | T | Portrait + landscape fit within frame, aspect preserved | 🚫 |
| P1-09 | Slideshow advance + interval | S2.1 | T | Advances at configured interval; order per settings | 🚫 |
| P1-10 | Smart shuffle avoids recent repeats | S2.2 | U | No repeat within last-N; light album/date clustering | ⬜ |
| P1-11 | USB pendrive import + dedup | S5.1,S5.3 | T+H | New photos imported once; duplicates skipped; toast shows count | 🚫 |
| P1-12 | Delete photo | S5.4 | T | Removed from index+storage+thumbs; drops from rotation; no display glitch | 🚫 |
| P1-13 | Storage-full import is safe | S5.5 | H | Near-full → imports what fits, warns, library intact | 🚫 |
| P1-14 | WiFi join via wifi.conf | S1.2 | T | Valid creds → online within timeout; bad creds → stays offline, keeps showing | 🚫 |
| P1-15 | WiFi reconfigure + fallback | S1.3 | T | New creds apply; bad new creds → fall back to last-good network | 🚫 |
| P1-16 | WiFi drop → reconnect | S8.1 | H | Router reboot → keeps showing; auto-reconnects when back | 🚫 |
| P1-17 | Web UI reachable + auth | S1.3,S12.1 | T | UI loads on LAN; requires password; rate-limits bad logins | 🚫 |
| P1-18 | Web control (next/pause) | S2.3 | T | Commands reflected within a frame or two | 🚫 |
| P1-19 | No-photos screen | S10.1 | T | Empty library → friendly setup screen, not blank/error | 🚫 |
| P1-20 | Corrupt photo skipped | S10.2 | U+T | Undecodable file → skipped+logged; slideshow continues; quarantine on repeat | ⬜ |
| P1-21 | Watchdog recovers hung renderer | S2.4,S7.2 | T | Inject a render hang → watchdog reboots and recovers | 🚫 |
| P1-22 | Night sleep / morning wake | S11.1,S11.2 | T | Screen blanks/wakes at scheduled hours; Pi keeps running | 🚫 |
| P1-23 | Corrupt data partition tolerated | S10.4 | H | FS errors on data → on-screen fault, firmware still boots (embedded UI) | 🚫 |
| P1-24 | Config factory reset keeps photos | S14.1 | H | reset.txt → WiFi+password default; photos preserved | 🚫 |
| P1-25 | Re-flash preserves data partition | S14.2 | H | Re-flash boot slots → first-boot state; photos survive | 🚫 |

## P2 — Delight / optional

| ID | Verifies | Scenario | Type | Pass criteria | Status |
|----|----------|----------|------|----------------|--------|
| P2-01 | Cross-fade smoothness | C6,S2.1 | T | NEON cross-fade at ≥24 fps, no tearing (vsync flip) | 🚫 |
| P2-02 | Ken Burns performance | C6 | T | NEON bilinear pan/zoom hits ≥24 fps at 1366×768 | 🚫 |
| P2-03 | Portrait blurred-cover | S13.3 | T | Tall photos get blurred-cover fill, not black bars | 🚫 |
| P2-04 | HEIC browser transcode | S5.2 | U+H | HEIC upload → JPEG stored (Safari native + heic2any fallback); server rejects non-images | ⬜ |
| P2-05 | Clock overlay + NTP | S3.3 | T | Correct time online; hidden/flagged when unverified offline | 🚫 |
| P2-06 | Weather overlay degrades | S3.3 | T | Fetch fail → last-known persists, no error splash (needs TLS) | 🚫 |
| P2-07 | Months-uptime, no leak | S13.1 | T | Long soak → stable RSS, bounded caches, no restart needed | 🚫 |
| P2-08 | Motion wake (if PIR fitted) | S11.3 | H | Approach wakes from sleep; empty room sleeps sooner | 🚫 |
| P2-09 | WiFi photo sync | S3.2 | T | New photos on Immich/SMB pulled into rotation | 🚫 |
| P2-10 | Multi-frame naming | S8.3 | H | Two frames, distinct hostnames/ids, no collision | 🚫 |

---

## Test-first order (matches roadmap)
1. **Unit tests that need no hardware** (P0-09, P0-11, P1-06/07/10/20, P2-04): build the pure-logic
   modules host-side and test *now*, before the Pi work — de-risks logic early.
2. **Milestone 0 spikes** as target smokes: P1-01 (C1), P1-05 (C2), P0-07 (C5), P2-01/02 (C6).
3. **P0 safety suite** before any "it works" claim — power-loss + update-rollback + offline.
4. **P1 core**, then **P2 delight**.

## Known gaps ⚠️
- Host unit-test harness + on-target smoke harness commands are **TBD** (fill once the build exists).
- HIL power-cut testing needs a switchable power rig (or disciplined manual cycling) for P0-02/04/05.
- TLS-dependent rows (P2-06, secure OTA transport) blocked on the mbedTLS port.
- mDNS (P2-10) depends on confirming Circle support.
