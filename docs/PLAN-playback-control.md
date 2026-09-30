# Plan — Playback control via IR remote (pause · previous · next · hold)

_Companion to [ROADMAP.md](ROADMAP.md), [DECISIONS.md](DECISIONS.md) (ADR-014), and
[../TESTPLAN.md](../TESTPLAN.md) (rows PC-01..08). Raised during v0.3 beta testing (2026-09-29)._

## The problem (owner, beta feedback)

The slideshow just plays. When a photo you like comes up you cannot pause it, and when one goes by
you cannot go back. The frame has **no input at all** today: the single USB OTG port carries the
pendrive, the display board does not pass HDMI-CEC, and Circle has no Bluetooth stack.

## Decision: a 3-pin IR receiver + any NEC remote (ADR-014)

An IR receiver module (VS1838B / TSOP38238 class, 38 kHz, 3 pins: VCC, GND, OUT) on one GPIO, and
a cheap NEC-protocol remote (the 21-key "car MP3" remote sold with every Pi/Arduino kit, or any
TV remote whose codes we learn). It makes the frame feel like an appliance, works for guests with
no phone, and needs no app and no Wi-Fi join.

**Alternatives considered** — see ADR-014. Phone-as-remote over the existing SoftAP was the
cheapest (zero hardware) but forces the phone off the home network for every press; GPIO buttons
were the safest but need holes in the bezel; a USB hub + HID remote adds a hub to the USB path we
just stabilised. IR was chosen; the phone remote page may follow later as a bonus since the command
interface (below) is shared.

## Decided parameters (owner-approved 2026-09-29 — do not re-ask)
| Parameter | Value |
|---|---|
| Receiver | 38 kHz 3-pin module (VS1838B or TSOP38238), active-low output |
| GPIO | **GPIO 17** (header pin 11), internal pull-up, 3.3 V supply from pin 1; overridable `ir_gpio = N` in `lumen.conf` |
| Protocol | **NEC** (32-bit: address, ~address, command, ~command; 9 ms lead + 4.5 ms space; repeat = 9 ms + 2.25 ms) |
| Remote | Any NEC remote. Ship defaults for the common 21-key kit remote; key codes overridable in `lumen.conf` (`ir_key_next = 0x..` …) so any TV remote can be "learned" from the SD log |
| Keys | **Pause/Play** (toggle) · **Next** · **Previous** · **Hold** (keep this photo 30 min) · optional **Info** (overlay name/date 5 s) |
| Paused motion | **Freeze** Ken Burns and fade; small pause glyph bottom-right for 3 s then hidden |
| Auto-resume | Pause auto-resumes after **10 min**; Hold after **30 min** (config `pause_timeout_min`, `hold_timeout_min`) |
| Previous depth | Ring of the last **32** shown indices (survives shuffle later) |
| Repeat keys | Held key (NEC repeat frames) = one action per 250 ms for Next/Prev; ignored for Pause/Hold |

## Architecture

```
IR receiver OUT ──> GPIO17 (falling+rising edge IRQ, Circle CGPIOPin::ConnectInterrupt)
   ISR: timestamp (CTimer::GetClockTicks, 1 MHz) -> push pulse width into a lock-free ring
        (no allocation, no logging, no decoding in the ISR — bare-metal rule)
Main loop (core 0, once per frame): CIrRemote::Poll()
   drains the ring -> NEC state machine -> validated 8-bit command (+ repeat flag)
   -> maps via config to a PlaybackCommand -> m_Photo.command(cmd)
PhotoFramePlugin: command() sets a pending command; render() applies it at the next frame:
   Pause/Resume  : freeze/unfreeze timeline (offset photo_start_/fade_start_ by paused time)
   Next          : force the dwell to elapse now (preloaded next shows on the next frame)
   Previous      : pop history ring -> decode that index inline on core 1 -> hard-cut or fade
   Hold          : Pause + 30 min timeout
   Info          : overlay filename + index for 5 s
```

The decoder (`firmware/src/input/NecDecoder.{h,cpp}`) is **freestanding** (pulse widths in, commands
out; time injected) so it is host-unit-tested against recorded pulse trains. The GPIO/IRQ glue
(`firmware/app/IrRemote.h`) is the only Circle-specific part, ~60 lines.

### NEC decoding rules (for the implementer)
- Frame: 9.0 ms mark, 4.5 ms space, then 32 bits; bit = 562 µs mark + 562 µs space (0) or
  1687 µs space (1); trailing 562 µs mark. Tolerance ±25 %.
- Repeat: 9.0 ms mark, 2.25 ms space, 562 µs mark → "same command again" (emit repeat, not a
  fresh press).
- Validate `command == ~command_inv`; log `ir: addr=0x.. cmd=0x.. (unknown key)` for any code not
  mapped, so a new remote's codes can be read straight off the SD log and put in `lumen.conf`.
- Gap > 100 ms resets the state machine. Ring overflow drops the frame (never blocks the ISR).

### Interaction with existing features
- Paused/held state is **cleared** by a USB insert/remove, a rescan after conversion, or a restart.
- A convert (QR) slide can be paused too (useful: gives the phone time to scan).
- The settings web page later gets the same Pause/Next/Prev buttons for free (shared command API).

## Phased plan (risk-first)

### Phase 0 — Spike IR-1: receive raw pulses on hardware 🔴 (gates everything)
Wire the receiver, register edge IRQs on GPIO17, log the first 40 pulse widths of one key press to
SD. **Pass:** the log shows a 9000/4500 µs lead followed by 562-ish marks. Verifies the receiver
polarity, the pin, and that the IRQ fires under the render + net load. **Owner runs; test card
PC-01.**

### Phase 1 — Freestanding NEC decoder (host-tested) — ✅ DONE 2026-09-30
`firmware/src/input/NecDecoder.{h,cpp}` + `tests/host/test_nec_decoder.cpp` (12 tests, incl. repeat
frames, ±20 % jitter, +40 % rejected, corrupted complement, extended address, truncated-then-good,
2000 random noise pulses). 419 host checks, 0 failures. Linked into `kernel8.img` (not yet called).
`NecDecoder` + unit tests fed with synthetic and recorded pulse trains: a clean frame, a repeat,
a ±20 % jittered frame, a corrupted frame (must not emit), a truncated frame followed by a good one.
**Done when:** tests pass and the decoder emits `(address, command, repeat)`.

### Phase 2 — Playback command API in the plugin — ✅ DONE 2026-09-30
`PhotoFramePlugin::command(PlaybackCommand)` (PauseToggle / Next / Previous / Hold / Info).
Pause freezes a **virtual clock** (`vtime()` = real − paused time) so Ken Burns and fades stop dead
and resume with no jump; Next forces the advance (finishes a fade instantly); Previous pops a
32-entry history ring and retargets the core-1 decode (deferred if the worker is busy); Hold =
pause with the 30 min deadline; Info = filename + (i/n) overlay 5 s; pause glyph 3 s. Hard-cut
instead of fade while paused. `set_source()`/rescan clears pause + history. Config:
`pause_timeout_min` (10), `hold_timeout_min` (30). Debug: `ir_sim = next,pause,prev,...` fires one
command every 4 s from 20 s after boot (lumen.conf is also read from `USB:/` when there is no SD,
so this runs in QEMU). Verified in QEMU: log shows each cmd → show, screenshots show the glyph,
the info strip, and a resumed cross-fade.
`PhotoFramePlugin::command(PlaybackCommand)`; pause/resume with timeline offset; next; previous with
a 32-entry history ring; hold; auto-resume timeouts; pause glyph + info overlay via `ICanvas`.
**Done when:** a debug config `ir_sim = next,pause,prev` (fed by the kernel on a timer) drives the
slideshow correctly in QEMU screenshots.

### Phase 3 — Integration on hardware
Kernel: `CIrRemote` (GPIO + ring + Poll), key map from `lumen.conf`, log every decoded key.
**Done when:** the kit remote pauses/steps the real slideshow (test card PC-03..06), unknown keys
are logged with their codes, and fps is unchanged while keys are held.

### Phase 4 — Polish, docs, close
Config docs, learnings, CHANGELOG, TESTPLAN ticks, `version.h` bump. Optional: the same commands on
the settings web page.

## Risk register
| Risk | Sev | Mitigation |
|---|---|---|
| IRQ latency/jitter under render + net load corrupts timing | 🟡 | Timestamp in the ISR itself (not in Poll); ±25 % tolerance; spike IR-1 measures it first |
| Receiver polarity/pin wrong | 🟢 | Spike logs raw edges; config-overridable GPIO |
| Ambient IR (sunlight, LED lamps) triggers noise | 🟢 | NEC validation (inverse-byte check) rejects noise; 100 ms gap reset |
| "Previous" after shuffle/rescan points at a different file | 🟡 | History stores indices *and* names; on mismatch after rescan, clear history |
| Forgotten pause leaves one photo for days | 🟢 | Auto-resume timeouts |

## The display board and its remote (identified 2026-09-29)
The panel is driven by a **VS.T56U11.2** universal LCD board (MStar **TSUMV56RUU** scaler; HDMI/VGA/
AV/USB, own IR receiver + NEC remote). **No HDMI-CEC**: the generic TSUMV56 firmware does not
implement it and the CEC pin is not routed — confirmed by the absence of any CEC/HDMI-Control item
in its OSD. CEC is therefore off the table for this build (ADR-014).

**Remote choice (owner, 2026-09-29, final):** use the **board's own remote** for everything. Its
Power key already drives the display natively, and the keys the board ignores on HDMI become the
frame's controls. Suggested map (confirm codes from the SD log on first press):
| Frame action | Board-remote key | Why it is free |
|---|---|---|
| Pause / Play | ⏯ in the transport row | board only uses it for its own USB media |
| Previous / Next | ⏮ / ⏭ (fallback CH− / CH+) | same |
| Hold | red coloured key | teletext keys, unused |
| Info | green coloured key | same |
Avoid Power, Source, Menu, arrows/OK, Vol±, Mute, Exit — the board acts on those.

**Lower priority (deferred):** the small 21-key kit remote. Fixed-code, cannot be programmed to
the board's Power code, so using it needs the Pi to relay Power via an IR emitter (below). Kept as
an optional Phase 5; not on the critical path.

### Deferred — Phase 5: kit remote + IR emitter (display power relay, sleep schedule)
The TV board only obeys its *own* remote's codes, so the kit remote's Power key cannot switch the
display directly. Solution: the Pi **re-transmits the board's power code** through a 940 nm **IR
LED** on a second GPIO (**GPIO 18**, 38 kHz carrier from a timer, NEC encode = mirror of the
decoder), aimed at the board's receiver. Kit-remote Power → Pi → board sees "its" Power key.
- Learn the board remote's power code once from the SD log (same S15.5 path); store as
  `ir_tx_power = <addr>,<cmd>` in `lumen.conf`.
- The Pi keeps running while the display is off; Power again turns it back on, slideshow resumes.
- **Bonus:** with display on/off under firmware control, the long-planned **night sleep schedule**
  (`sleep = 23:00`, `wake = 07:00`) is a config feature, and the Pi can switch the display on at boot.
- Plan B if line-of-sight inside the bezel is awkward: drive the POWER line of the T56 board's
  front-panel key header through an optocoupler (needs the header pinout checked first).

## Hardware shopping list
- **Now:** 1× TSOP38238 IR receiver (the Amazon kit at ~₹249 is fine; its remote is a spare).
  Wiring: OUT→pin 11 (GPIO17), GND→pin 6, VS→pin 1 (3.3 V). Kit wires are male-male; use
  female-female for the Pi header.
- **Remote:** the VS.T56U11.2 board's own remote. Nothing to buy.
- **Deferred (Phase 5 only):** 1× 940 nm IR LED, 1× 100 Ω, 1× NPN (2N2222) — ~₹20.
  GPIO18 (pin 12) → 1 kΩ → base; LED + 100 Ω from 5 V (pin 2) to collector; emitter to GND.

## Definition of done
1. The kit remote pauses, resumes, steps forward/back, and holds the real slideshow.
2. An unmapped remote's codes appear in the SD log and can be mapped in `lumen.conf` without a rebuild.
3. Frame rate and the AP are unaffected while keys are pressed or held.
4. Pause/Hold auto-resume; no state leaks across USB insert/remove or rescan.
5. Decoder host tests pass; PC-01..08 ticked; docs updated; version bumped.
