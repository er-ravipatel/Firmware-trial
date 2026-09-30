//
// IrRemote.h — IR remote input for Lumen Frame: a 38 kHz receiver (TSOP38238 / VS1838B) on a
// GPIO, decoded as NEC into PlaybackCommands. See docs/PLAN-playback-control.md (ADR-014).
//
//   receiver OUT ──> GPIO (default 17, `ir_gpio` in lumen.conf), internal pull-up, active-low
//   ISR (both edges): timestamp only -> pulse (width µs, mark?) into a lock-free ring
//   Poll() (main loop, once per frame): drain ring -> lf::NecDecoder -> key -> config key map
//
// The ISR does no allocation, no logging and no decoding — bare-metal rule. All the work is in
// Poll() on core 0. Decoded keys are logged, unmapped ones as "(unknown key)" so any NEC remote
// can be mapped in lumen.conf without a rebuild:
//   ir_key_pause = 0x45   ir_key_next = 0x43   ir_key_prev = 0x44   ir_key_hold = 0x09
//   ir_key_info = 0x0D    ir_addr = any|0xNN   ir_debug = on   (logs raw pulse widths — spike IR-1)
//
#ifndef _irremote_h
#define _irremote_h

#include <circle/gpiopin.h>
#include <circle/gpiomanager.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/types.h>
#include "Config.h"
#include "input/NecDecoder.h"             // via EXTRAINCLUDE=-I../src
#include "plugins/PhotoFramePlugin.h"

class CIrRemote
{
public:
	typedef lf::PhotoFramePlugin::PlaybackCommand Command;

	CIrRemote (CInterruptSystem *pInterrupt, CConfig *pConfig)
	:	m_GPIOManager (pInterrupt), m_pPin (0), m_pConfig (pConfig),
		m_nHead (0), m_nTail (0), m_bOverflow (FALSE),
		m_nLastEdgeUs (0), m_nLastActionMs (0), m_nBurstPulses (0),
		m_bEnabled (FALSE), m_bDebug (FALSE), m_nAddrFilter (-1)
	{
		for (unsigned i = 0; i < kActions; i++) m_KeyMap[i] = -1;
	}

	// Set up the pin + IRQ. Returns FALSE (and stays inert) if `ir` is off in the config.
	boolean Initialize (void)
	{
		if (!m_pConfig->GetBool ("ir", TRUE)) return FALSE;
		unsigned nPin = (unsigned) m_pConfig->GetInt ("ir_gpio", 17);
		m_bDebug = m_pConfig->GetBool ("ir_debug", FALSE);
		m_nAddrFilter = ParseCode (m_pConfig->GetStr ("ir_addr", "any"));
		static const char *s_Keys[kActions] = {"ir_key_pause", "ir_key_next", "ir_key_prev",
						       "ir_key_hold", "ir_key_info"};
		for (unsigned i = 0; i < kActions; i++)
			m_KeyMap[i] = ParseCode (m_pConfig->GetStr (s_Keys[i], ""));

		if (!m_GPIOManager.Initialize ()) return FALSE;
		// The manager must be given at construction (ConnectInterrupt asserts on it), so the
		// pin is created here, once, after the config told us which GPIO to use.
		m_pPin = new CGPIOPin (nPin, GPIOModeInputPullUp, &m_GPIOManager);
		if (m_pPin == 0) return FALSE;
		m_pPin->ConnectInterrupt (IsrStub, this);           // auto-ack
		m_pPin->EnableInterrupt  (GPIOInterruptOnFallingEdge);
		m_pPin->EnableInterrupt2 (GPIOInterruptOnRisingEdge);
		m_nLastEdgeUs = CTimer::GetClockTicks ();
		m_bEnabled = TRUE;

		CLogger::Get ()->Write ("ir", LogNotice,
			"IR receiver on GPIO%u (edge IRQ), addr=%s, keys pause=%d next=%d prev=%d hold=%d info=%d%s",
			nPin, m_nAddrFilter < 0 ? "any" : "filtered",
			m_KeyMap[0], m_KeyMap[1], m_KeyMap[2], m_KeyMap[3], m_KeyMap[4],
			m_bDebug ? "  [raw pulse debug ON]" : "");
		return TRUE;
	}

	boolean IsEnabled (void) const { return m_bEnabled; }

	// Drain the pulse ring, decode, map. Call once per frame from the render loop (core 0).
	// Returns the command to apply (Command::None if nothing happened this frame).
	Command Poll (void)
	{
		if (!m_bEnabled) return Command::None;
		Command result = Command::None;

		if (m_bOverflow) { m_bOverflow = FALSE; m_Decoder.reset (); }

		unsigned nTail = m_nTail;
		unsigned nHead = __atomic_load_n (&m_nHead, __ATOMIC_ACQUIRE);
		while (nTail != nHead)
		{
			const TPulse &p = m_Ring[nTail % kRing];
			nTail++;

			if (m_bDebug) DebugPulse (p);

			lf::NecKey key;
			if (m_Decoder.feed (p.nWidthUs, p.bMark != 0, key))
			{
				Command c = MapKey (key);
				if (c != Command::None) result = c;   // last one wins within a frame
			}
		}
		m_nTail = nTail;
		return result;
	}

private:
	static const unsigned kRing = 512;         // pulses; one NEC frame is 67, plenty of slack
	static const unsigned kActions = 5;        // pause, next, prev, hold, info
	static const unsigned kRepeatMs = 250;     // held Next/Prev: one step per this interval

	struct TPulse { u32 nWidthUs; u8 bMark; };

	static void IsrStub (void *pParam) { static_cast<CIrRemote *> (pParam)->Isr (); }

	// Runs on every edge with IRQs masked. Timestamp, classify the pulse that just ENDED, push.
	void Isr (void)
	{
		unsigned nNow = CTimer::GetClockTicks ();
		unsigned nWidth = nNow - m_nLastEdgeUs;
		m_nLastEdgeUs = nNow;
		// The line just toggled. If it is HIGH now (rising edge), the pulse that ended was LOW,
		// i.e. carrier present = a MARK (receiver output is active-low).
		u8 bMark = (m_pPin->Read () == HIGH) ? 1 : 0;

		unsigned nHead = m_nHead;
		if (nHead - m_nTail >= kRing) { m_bOverflow = TRUE; return; }   // drop, never block
		m_Ring[nHead % kRing].nWidthUs = nWidth;
		m_Ring[nHead % kRing].bMark = bMark;
		__atomic_store_n (&m_nHead, nHead + 1, __ATOMIC_RELEASE);
	}

	Command MapKey (const lf::NecKey &key)
	{
		static const Command s_Actions[kActions] = {
			Command::PauseToggle, Command::Next, Command::Previous, Command::Hold, Command::Info};
		static const char *s_Names[kActions] = {"pause", "next", "prev", "hold", "info"};

		int nAction = -1;
		if (m_nAddrFilter < 0 || m_nAddrFilter == key.address)
			for (unsigned i = 0; i < kActions; i++)
				if (m_KeyMap[i] == key.command) { nAction = (int) i; break; }

		unsigned nNowMs = CTimer::GetClockTicks () / 1000;
		if (nAction < 0)
		{
			if (!key.repeat)
				CLogger::Get ()->Write ("ir", LogNotice, "ir: addr=0x%02X cmd=0x%02X (unknown key)%s",
							key.address, key.command, key.extended ? " ext" : "");
			return Command::None;
		}
		if (key.repeat)
		{
			// Held key: Next/Prev auto-step every kRepeatMs; Pause/Hold/Info ignore repeats.
			if (nAction != 1 && nAction != 2) return Command::None;
			if (nNowMs - m_nLastActionMs < kRepeatMs) return Command::None;
		}
		m_nLastActionMs = nNowMs;
		CLogger::Get ()->Write ("ir", LogNotice, "ir: addr=0x%02X cmd=0x%02X -> %s%s",
					key.address, key.command, s_Names[nAction], key.repeat ? " (held)" : "");
		return s_Actions[nAction];
	}

	// Spike IR-1 support: log raw pulse widths, 8 per line, first 72 pulses of each burst.
	void DebugPulse (const TPulse &p)
	{
		if (p.nWidthUs >= lf::NecDecoder::kGapReset) { FlushDebug (); m_nBurstPulses = 0; return; }
		if (m_nBurstPulses >= 72) return;
		unsigned k = m_nBurstPulses % 8;
		m_DebugLine[k] = p.nWidthUs | (p.bMark ? 0x80000000u : 0);
		m_nBurstPulses++;
		if (k == 7) FlushDebug ();
	}
	void FlushDebug (void)
	{
		unsigned n = m_nBurstPulses % 8; if (n == 0 && m_nBurstPulses) n = 8;
		if (n == 0) return;
		char line[160]; unsigned o = 0;
		for (unsigned i = 0; i < n && o + 14 < sizeof line; i++)
		{
			unsigned w = m_DebugLine[i] & 0x7FFFFFFFu; boolean m = (m_DebugLine[i] >> 31) != 0;
			o += FmtNum (line + o, w); line[o++] = m ? 'M' : 'S'; line[o++] = ' ';
		}
		line[o] = '\0';
		CLogger::Get ()->Write ("ir", LogNotice, "irraw: %s", line);
	}
	static unsigned FmtNum (char *d, unsigned v)
	{
		char t[12]; unsigned n = 0;
		do { t[n++] = (char) ('0' + v % 10); v /= 10; } while (v && n < sizeof t);
		unsigned o = 0; while (n) d[o++] = t[--n];
		return o;
	}

	// "0x45" / "45h" / "69" / "" -> code; "any"/"" -> -1.
	static int ParseCode (const char *s)
	{
		if (s == 0 || s[0] == '\0') return -1;
		if ((s[0] == 'a' || s[0] == 'A') && (s[1] == 'n' || s[1] == 'N')) return -1;
		unsigned base = 10; const char *p = s;
		if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
		unsigned v = 0; boolean any = FALSE;
		for (; *p; ++p)
		{
			unsigned d;
			if (*p >= '0' && *p <= '9') d = (unsigned) (*p - '0');
			else if (*p >= 'a' && *p <= 'f') d = (unsigned) (*p - 'a' + 10);
			else if (*p >= 'A' && *p <= 'F') d = (unsigned) (*p - 'A' + 10);
			else if (*p == 'h' || *p == 'H') { base = 16; continue; }
			else break;
			if (d >= base && base == 10) { base = 16; }   // "45" with a hex digit later -> hex
			v = v * base + d; any = TRUE;
		}
		return any ? (int) (v & 0xFF) : -1;
	}

	CGPIOManager   m_GPIOManager;
	CGPIOPin      *m_pPin;               // created in Initialize() (needs the manager + config pin)
	CConfig       *m_pConfig;

	// ISR -> Poll ring. Head written by the ISR, tail by Poll; indices free-run (mod kRing).
	TPulse            m_Ring[kRing];
	volatile unsigned m_nHead;
	volatile unsigned m_nTail;
	volatile boolean  m_bOverflow;
	unsigned          m_nLastEdgeUs;

	lf::NecDecoder m_Decoder;
	int            m_KeyMap[kActions];   // NEC command byte per action, -1 = unmapped
	unsigned       m_nLastActionMs;
	unsigned       m_DebugLine[8];
	unsigned       m_nBurstPulses;
	boolean        m_bEnabled;
	boolean        m_bDebug;
	int            m_nAddrFilter;        // -1 = accept any address
};

#endif
