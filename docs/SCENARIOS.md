# Lumen Frame — Use-Case & Behavioral Scenarios

> How the mature, deployed product **behaves** across real-world situations. Each scenario is a
> testable **Given / When / Then**, plus **↳ Handled by** (the design mechanism) so behavior maps
> to implementation ([DESIGN.md](DESIGN.md), [IMPLEMENTATION.md](IMPLEMENTATION.md)). These feed
> [../TESTPLAN.md](../TESTPLAN.md). Legend: 🛡️ fail-safe · 💡 delight touch.

---

## 0. Deployment lifecycle (mature product)

| Phase | Behavior |
|---|---|
| **Provision** (factory/maker) | Flash signed image to SD (bootA+bootB identical, empty data). Bake update public key + per-device id. |
| **First customer boot** | Splash → "NO_PHOTOS" screen with on-screen instructions + web URL/QR + how to set WiFi. |
| **Onboard** | User sets WiFi (`wifi.conf` or, once online, web UI) and adds photos. |
| **Steady state** | Runs 24/7: slideshow by day, sleeps at night, imports new photos, checks for updates opportunistically. |
| **Update** | Silent OTA within night window; auto-rollback on failure. USB update always available offline. |
| **End of life** | Old build_ids keep working forever offline; no cloud dependency means no "server shutdown" brick. |

---

## 1. Onboarding & connectivity

**S1.1 — First WiFi setup, offline start**
Given a fresh device with no `wifi.conf`, When it boots, Then it shows the NO_PHOTOS/setup screen
and runs the slideshow from any local photos; it does **not** block waiting for network.
↳ Handled by: offline-first boot; NetTask runs independently of RenderTask.

**S1.2 — Provide WiFi via file**
Given the user edits `wifi.conf` (SSID/PSK) on the SD/pendrive on a PC, When inserted and booted,
Then WLAN joins the network within the connect timeout and status shows "online."
↳ Handled by: WlanManager reads `/config/wifi.conf` on boot. 🛡️ Bad creds → stays offline, logs, keeps showing photos.

**S1.3 — Reconfigure WiFi later**
Given the device is online, When the user submits a new SSID/PSK in the web UI, Then it reconnects
to the new network; if the new creds fail, it **falls back to the previous** working network.
↳ Handled by: `POST /api/wifi`; WlanManager keeps last-good creds. 💡 no lock-out from a typo.

**S1.4 — Move the frame to a new house**
Given the old WiFi is gone, When it can't join for a grace period, Then it surfaces an on-screen
hint ("WiFi unavailable — update via web/USB") but **keeps running the slideshow offline**.
↳ Handled by: connect ret/backoff; offline is a normal state, not an error.

---

## 2. Normal steady-state operation

**S2.1 — Daily slideshow**
Given photos exist and it's daytime, When idle, Then it advances photos every `interval` with the
configured transition, applying EXIF orientation and fit/letterbox.
↳ Handled by: RenderTask SHOWING⇄TRANSITION; ContentManager pre-scaled frames.

**S2.2 — Smart ordering**
Given many photos, When advancing, Then it avoids repeating the last N and lightly clusters by
album/date so a "story" plays rather than pure chaos.
↳ 💡 Handled by: smart-shuffle playlist.

**S2.3 — User nudges it**
Given someone opens the web UI, When they tap next/prev/pause, Then the frame responds within a
frame or two without stutter.
↳ Handled by: `POST /api/control` → EventBus → render FSM (lock-free).

---

## 3. Online mode behaviors

**S3.1 — Opportunistic update check**
Given online and inside the night window, When the agent polls, Then it downloads any newer signed
bundle to the inactive slot and schedules the tryboot; the user sees nothing until it's ready.
↳ Handled by: NetUpdateSource + UpdateAgent; night-window policy.

**S3.2 — WiFi photo sync (later)**
Given a configured Immich/SMB source, When new photos appear there, Then they're pulled, run
through the pipeline, and enter rotation.
↳ Handled by: sync importer → standard content pipeline.

**S3.3 — Clock/weather overlay**
Given online + overlay enabled, When shown, Then time is correct (NTP) and weather refreshes
periodically; if the fetch fails, the last-known values persist (no error splash).
↳ Handled by: NTP sync; overlay degrades gracefully. 🛡️ (weather needs TLS port — later.)

---

## 4. Offline mode behaviors

**S4.1 — Runs fully offline**
Given no internet ever, When operating, Then slideshow, USB import, web UI (on LAN), USB update,
sleep/wake, and overlays-minus-weather all work.
↳ Handled by: offline-first; only OTA-pull and weather need internet.

**S4.2 — Clock with no NTP**
Given no internet and no RTC, When showing the clock, Then it either hides the clock or shows it
flagged as unset — it never shows a confidently-wrong time.
↳ 🛡️ Handled by: overlay suppresses unverified time. (💡 optional: add a cheap RTC module later.)

**S4.3 — Update while offline**
Given no internet, When the user wants to update, Then they drop a signed bundle on a pendrive and
it installs — the **guaranteed** path.
↳ Handled by: UsbUpdateSource (identical bundle to OTA).

---

## 5. Content ingestion

**S5.1 — USB pendrive import**
Given a pendrive with photos, When inserted, Then new/changed images are imported (dedup by hash),
an "imported N" toast shows, and rotation includes them; already-known photos are skipped.
↳ Handled by: UsbImporter → pipeline → Deduper; IMPORT_TOAST overlay.

**S5.2 — HEIC from iPhone via web**
Given an iPhone user uploads HEIC in the web UI, When submitted, Then the **browser** transcodes to
JPEG first (Safari native, or heic2any WASM fallback) and the frame stores JPEG only.
↳ 💡 Handled by: client-side transcode; server validates magic bytes. 🛡️ non-image rejected.

**S5.3 — Duplicate photo across sources**
Given the same image arrives via SD and USB, When imported, Then it appears once.
↳ Handled by: content-hash dedup.

**S5.4 — Delete a photo**
Given a photo in rotation, When deleted in the web UI, Then it's removed from index + storage +
thumbnails and drops out of rotation without disturbing the current display.
↳ Handled by: `DELETE /api/photos/{id}`; render reads the updated playlist next advance.

**S5.5 — Storage full on import**
Given the data partition is near full, When importing, Then it imports what fits, warns "storage
full" in the UI/toast, and **never corrupts** the existing library.
↳ 🛡️ Handled by: pre-check free space; atomic writes; partial import is safe.

---

## 6. Power & boot

**S6.1 — Cold boot after outage**
Given power returns after an outage, When it boots, Then it resumes the slideshow from local photos
in < ~10 s with no user action.
↳ Handled by: fast bare-metal boot; state on FAT.

**S6.2 — Power cut mid-write (photo/index)**
Given power drops while writing a photo or the index, When it reboots, Then the library is intact
(the in-progress item may be missing, never a corrupted catalog).
↳ 🛡️ Handled by: temp-file + atomic rename; journaled index (A/B copies + validity marker).

**S6.3 — Power cut mid-update**
Given power drops while staging a new kernel to the inactive slot, When it reboots, Then it boots
the **still-intact running slot**; the half-written slot is re-validated (length+sig) before any
future tryboot and simply re-staged.
↳ 🛡️ Handled by: never write the running slot; staged-slot validation before arming tryboot.

**S6.4 — Repeated brownouts**
Given flaky power cycling it repeatedly, When booting, Then it always reaches a known-good slot and
never enters a boot loop.
↳ 🛡️ Handled by: `boot_attempt` counter forces the good slot if a trial boot never confirms.

---

## 7. Firmware update scenarios

**S7.1 — Successful OTA**
Given a newer signed bundle online, When applied overnight, Then it stages → tryboots → self-tests
healthy → marks good; morning shows the new build, photos and settings preserved.
↳ Handled by: UpdateAgent full flow; data partition untouched by updates.

**S7.2 — Bad update auto-rollback**
Given a new build that fails its health self-test (or hangs), When trialed, Then the watchdog
reboots and it **reverts to the previous build automatically**; user never sees a dead frame.
↳ 🛡️ Handled by: one-shot tryboot + watchdog + boot_attempt fallback.

**S7.3 — Tampered / unsigned bundle**
Given a corrupted or wrong-key bundle (USB or net), When discovered, Then it's rejected before
staging; the running firmware is untouched; the attempt is logged.
↳ 🛡️ Handled by: SHA-256 + Ed25519 verify before SlotWriter.

**S7.4 — Downgrade / update loop attempt**
Given an older or same build_id, When offered, Then it's refused (unless an explicit force flag),
preventing version thrash.
↳ Handled by: monotonic build_id anti-downgrade check.

**S7.5 — Interrupted download (WiFi drops mid-OTA)**
Given the connection drops during download, When it resumes, Then the partial is discarded and
re-fetched; nothing is staged until a **complete, verified** bundle exists.
↳ 🛡️ Handled by: verify-then-stage ordering; no partial arming.

---

## 8. Network scenarios

**S8.1 — WiFi lost then restored**
Given WiFi drops (router reboot), When it's gone, Then the frame keeps showing photos and web UI is
simply unreachable; When WiFi returns, Then it reconnects automatically and resumes online tasks.
↳ Handled by: WlanManager reconnect/backoff; offline is non-fatal.

**S8.2 — Weak signal**
Given a marginal link, When operating, Then updates/sync retry with backoff and never stall the
render loop.
↳ 🛡️ Handled by: NetTask isolation from RenderTask.

**S8.3 — Two frames on one network**
Given multiple frames, When each boots, Then each is reachable by its own hostname/id without
collision.
↳ Handled by: per-device id / hostname. (⚠️ mDNS support to confirm.)

---

## 9. Display / hardware

**S9.1 — TV board misreports resolution**
Given the generic HDMI board advertises a wrong/absent EDID, When booting, Then the pinned HDMI
timing (config.txt / custom cvt) still drives 1366×768 correctly.
↳ 🛡️ Handled by: `hdmi_force_hotplug` + explicit mode/cvt; validated in spike C1/C3.

**S9.2 — HDMI unplug/replug**
Given the cable is pulled and reinserted, When reconnected, Then the image returns without a reboot.
↳ Handled by: forced hotplug; framebuffer persists.

**S9.3 — Panel/board power-cycled independently**
Given the screen is switched off/on at the wall while the Pi runs, When the screen returns, Then
the slideshow is still there.
↳ Handled by: Pi is source-of-truth; no dependence on display power state.

---

## 10. Degraded & error states

**S10.1 — No photos**
Given an empty library, When running, Then it shows a friendly "add photos" screen with the web
URL/QR — not a blank screen or error.
↳ 💡🛡️ Handled by: NO_PHOTOS state.

**S10.2 — One corrupt photo**
Given a file that fails to decode, When reached in rotation, Then it's skipped and logged; the
slideshow continues; repeated failures quarantine the file.
↳ 🛡️ Handled by: decode returns Result; skip-and-log, never crash.

**S10.3 — Corrupt index**
Given the photo index is unreadable, When booting, Then it falls back to the A/B index copy, or
rebuilds the index by scanning `/photos`.
↳ 🛡️ Handled by: journaled index + rebuild-on-scan recovery.

**S10.4 — Corrupt/failing SD data area**
Given filesystem errors on `data`, When detected, Then it reports the fault on-screen and in the
UI, and still boots (firmware lives on the intact boot slots).
↳ 🛡️ Handled by: firmware independent of data partition; UI embedded in image.

---

## 11. Schedule, sleep & power-saving

**S11.1 — Night sleep**
Given a configured sleep window, When the hour arrives, Then the screen blanks (panel off/DPMS) to
save the panel and power; the Pi keeps running (imports/updates continue).
↳ Handled by: Scheduler + PowerManager.

**S11.2 — Morning wake**
Given the wake hour, When it arrives, Then the slideshow reappears smoothly.
↳ Handled by: SCHED_WAKE event.

**S11.3 — Motion wake (optional)**
Given a PIR is fitted and it's asleep, When someone approaches, Then it wakes early; when the room's
empty, it sleeps sooner.
↳ 💡 Handled by: MOTION event + motion-aware dwell (hardware-dependent).

---

## 12. Security & privacy

**S12.1 — Web UI auth**
Given the admin password is set, When someone on the LAN opens the UI, Then they must authenticate
before managing photos/settings.
↳ 🛡️ Handled by: HTTP Basic + rate-limit. (⚠️ no TLS until mbedTLS — LAN-trust documented.)

**S12.2 — No remote attack surface offline**
Given no internet, When deployed, Then there is no shell/SSH/cloud callback — the attack surface is
just the LAN web UI.
↳ 🛡️ Handled by: bare metal has no OS shell by construction.

**S12.3 — Photos stay local**
Given photos are added, When operating, Then nothing leaves the device except opt-in sync/update
traffic to configured endpoints.
↳ Handled by: no telemetry by default; explicit opt-in for any upload.

---

## 13. Long-run reliability

**S13.1 — Months of uptime**
Given continuous operation, When running for months, Then no memory growth degrades it (bounded
caches, no leaks) and it doesn't need manual restarts.
↳ 🛡️ Handled by: bounded allocation; LRU caches; watchdog backstop.

**S13.2 — Clock drift (no RTC)**
Given no internet for a long time, When showing time, Then it avoids displaying a wrong time (see
S4.2) rather than drifting visibly.
↳ 🛡️ Handled by: suppress unverified time.

**S13.3 — Panel longevity**
Given a static-ish image risk, When idle, Then transitions + night sleep + (optional) slight pan
reduce burn-in risk.
↳ 💡 Handled by: never truly static; sleep window.

---

## 14. Recovery / factory reset

**S14.1 — Forgotten WiFi/password**
Given the user is locked out of settings, When they place a `reset.txt` (or hold-config) on the SD,
Then network + admin password reset to defaults **without** deleting photos.
↳ 🛡️ Handled by: config reset separate from photo data.

**S14.2 — Full re-flash**
Given a wedged device, When re-flashed with the signed image, Then it returns to first-boot state;
photos survive if only the boot slots are re-written (data preserved).
↳ Handled by: partition separation (boot vs data).

---

## Coverage map → tests
Every **S#** here becomes a row in [../TESTPLAN.md](../TESTPLAN.md) (unit, on-target smoke, or
manual HIL). Power-loss (S6), update-safety (S7), and offline (S4) scenarios are the
highest-priority to exercise, since they're where "product" reliability is won or lost.
