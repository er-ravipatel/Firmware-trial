# firmware/src/net — offline networking (v0.3 Pillar B)

Validated spike components for the offline **QR → SoftAP → phone-convert → write-back** flow.
Proven end-to-end on a real Pi Zero 2 W (Spikes W1 + W2, 2026-07-18): SoftAP → DHCP → HTTP page
served to a phone, all with no internet.

- **dhcpd.{h,cpp}** — minimal DHCP *server* (Circle only ships a client). Hands a phone an IP when it
  joins the AP. RFC-2131-checked; broadcast reply; MAC-stable lease; diagnostic logging.
- **webserver.{h,cpp}** — `CHTTPDaemon` subclass serving a self-contained branded page on any path
  (so captive-portal probes reach it). Grows into the photo-conversion page.
- **dnsd.{h,cpp}** — minimal DNS responder: answers every A query with the AP IP → the phone's
  connectivity check resolves to us and the page **auto-opens** (confirmed on Android + iOS).

**Build note:** WiFi requires the SDHOST build (SD card on SDHOST, EMMC free for WiFi SDIO) — i.e.
**drop `-DNO_SDHOST`** in `vendor/circle/Config.mk` (see docs/LEARNINGS.md). CYW43 firmware blobs go
on the SD at `SD:/firmware/`.

**Next:** DNS responder (captive-portal auto-open), then integrate AP+DHCP+HTTP+DNS into the Lumen
Frame kernel with the QR trigger + Conversion-mode state machine, then the libheif-WASM conversion
page + FAT write-back. See [docs/PLAN-v0.3.md](../../../docs/PLAN-v0.3.md).
