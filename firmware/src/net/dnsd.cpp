//
// dnsd.cpp — see dnsd.h.
//
#include "dnsd.h"
#include <circle/net/in.h>
#include <circle/net/socket.h>
#include <circle/net/ipaddress.h>
#include <circle/logger.h>
#include <circle/util.h>
#include <assert.h>

static const char From[] = "dnsd";

CDNSD::CDNSD (CNetSubSystem *pNet, const u8 *pServerIP)
:	m_pNet (pNet)
{
	memcpy (m_ServerIP, pServerIP, 4);
}

CDNSD::~CDNSD (void)
{
	m_pNet = 0;
}

void CDNSD::Run (void)
{
	assert (m_pNet != 0);
	CSocket Socket (m_pNet, IPPROTO_UDP);
	if (Socket.Bind (53) < 0)
	{
		CLogger::Get ()->Write (From, LogError, "Cannot bind port 53");
		return;
	}
	CLogger::Get ()->Write (From, LogNotice, "DNS responder up (all A -> %u.%u.%u.%u)",
				m_ServerIP[0], m_ServerIP[1], m_ServerIP[2], m_ServerIP[3]);

	u8 Q[512];
	for (;;)
	{
		CIPAddress ClientIP;
		u16 nClientPort;
		int nr = Socket.ReceiveFrom (Q, sizeof Q, 0, &ClientIP, &nClientPort);
		if (nr < 12) continue;                       // smaller than a DNS header
		unsigned n = (unsigned) nr;

		if (Q[2] & 0x80) continue;                   // already a response, ignore
		unsigned nQD = (Q[4] << 8) | Q[5];
		if (nQD < 1) continue;

		// Walk the first question's QNAME (labels terminated by a 0 byte).
		unsigned p = 12;
		while (p < n && Q[p] != 0)
		{
			if (Q[p] & 0xC0) { p = n; break; }   // compression pointer in a question: bail
			p += Q[p] + 1;
		}
		if (p >= n) continue;
		unsigned nQTypePos = p + 1;                  // just past the terminating 0
		if (nQTypePos + 4 > n) continue;
		unsigned nQType = (Q[nQTypePos] << 8) | Q[nQTypePos + 1];
		unsigned nQEnd = nQTypePos + 4;              // end of the question section

		// Turn the query into a response in place.
		Q[2] = 0x81;                                 // QR=1, Opcode=0, RD
		Q[3] = 0x80;                                 // RA=1, RCODE=0
		Q[8] = Q[9] = Q[10] = Q[11] = 0;             // NSCOUNT=ARCOUNT=0

		unsigned nOut = nQEnd;
		if (nQType == 1)                             // A record -> answer with our IP
		{
			Q[6] = 0; Q[7] = 1;                  // ANCOUNT=1
			u8 *a = &Q[nQEnd];
			a[0] = 0xC0; a[1] = 0x0C;            // name = pointer to offset 12 (the question)
			a[2] = 0x00; a[3] = 0x01;            // TYPE  A
			a[4] = 0x00; a[5] = 0x01;            // CLASS IN
			a[6] = 0; a[7] = 0; a[8] = 0; a[9] = 0x3C;   // TTL 60s
			a[10] = 0x00; a[11] = 0x04;          // RDLENGTH 4
			memcpy (&a[12], m_ServerIP, 4);      // RDATA = AP IP
			nOut = nQEnd + 16;
		}
		else
		{
			Q[6] = 0; Q[7] = 0;                  // no answer for non-A (e.g. AAAA)
		}

		Socket.SendTo (Q, nOut, 0, ClientIP, nClientPort);
	}
}
