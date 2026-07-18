//
// dhcpd.cpp — minimal DHCP server (see dhcpd.h).
//
#include "dhcpd.h"
#include <circle/net/in.h>
#include <circle/net/ipaddress.h>
#include <circle/logger.h>
#include <circle/util.h>
#include <assert.h>

static const char From[] = "dhcpd";

// BOOTP/DHCP fixed-header field offsets.
#define OP        0     // 1=BOOTREQUEST, 2=BOOTREPLY
#define HTYPE     1
#define HLEN      2
#define HOPS      3
#define XID       4     // 4 bytes
#define SECS      8
#define FLAGS     10    // 2 bytes (0x8000 = broadcast)
#define CIADDR    12    // 4
#define YIADDR    16    // 4 ("your" IP the server assigns)
#define SIADDR    20    // 4 (server IP)
#define GIADDR    24    // 4
#define CHADDR    28    // 16 (client hardware address)
#define MAGIC     236   // 4 (0x63 0x82 0x53 0x63)
#define OPTIONS   240

#define DHCP_DISCOVER  1
#define DHCP_OFFER     2
#define DHCP_REQUEST   3
#define DHCP_ACK       5

volatile bool g_dhcpClientConnected = false;   // set once a phone completes a lease (see dhcpd.h)

CDHCPD::CDHCPD (CNetSubSystem *pNet, const u8 *pServerIP)
:	m_pNet (pNet)
{
	memcpy (m_ServerIP, pServerIP, 4);
}

CDHCPD::~CDHCPD (void)
{
	m_pNet = 0;
}

// Return the value pointer + length of a DHCP option, or 0 if absent.
static const u8 *FindOption (const u8 *pMsg, unsigned nLen, u8 nOption, u8 *pValLen)
{
	unsigned i = OPTIONS;
	while (i + 1 < nLen)
	{
		u8 code = pMsg[i];
		if (code == 255) break;       // end
		if (code == 0) { i++; continue; }  // pad
		u8 len = pMsg[i + 1];
		if (i + 2 + len > nLen) break;
		if (code == nOption) { if (pValLen) *pValLen = len; return &pMsg[i + 2]; }
		i += 2 + len;
	}
	return 0;
}

void CDHCPD::Run (void)
{
	assert (m_pNet != 0);
	CSocket Socket (m_pNet, IPPROTO_UDP);
	if (Socket.Bind (67) < 0)                       // DHCP server port
	{
		CLogger::Get ()->Write (From, LogError, "Cannot bind port 67");
		return;
	}
	Socket.SetOptionBroadcast (TRUE);

	CLogger::Get ()->Write (From, LogNotice, "DHCP server up (offering %u.%u.%u.100+)",
				m_ServerIP[0], m_ServerIP[1], m_ServerIP[2]);

	u8 Buffer[1024];
	for (;;)
	{
		CIPAddress ClientIP;
		u16 nClientPort;
		int n = Socket.ReceiveFrom (Buffer, sizeof Buffer, 0, &ClientIP, &nClientPort);
		if (n >= (int) OPTIONS + 1)
		{
			HandlePacket (&Socket, Buffer, (unsigned) n);
		}
	}
}

void CDHCPD::HandlePacket (CSocket *pSocket, u8 *pMsg, unsigned nLen)
{
	// Validate: a BOOTREQUEST with the DHCP magic cookie.
	if (pMsg[OP] != 1) return;
	if (!(pMsg[MAGIC] == 0x63 && pMsg[MAGIC+1] == 0x82
	   && pMsg[MAGIC+2] == 0x53 && pMsg[MAGIC+3] == 0x63)) return;

	u8 nValLen = 0;
	const u8 *pType = FindOption (pMsg, nLen, 53, &nValLen);
	if (pType == 0 || nValLen < 1) return;

	// Diagnostic: prove packets are actually reaching the server (tests broadcast delivery).
	CLogger::Get ()->Write (From, LogNotice, "rx type=%u from %02x:%02x:%02x:%02x:%02x:%02x (%u bytes)",
				pType[0], pMsg[CHADDR], pMsg[CHADDR+1], pMsg[CHADDR+2],
				pMsg[CHADDR+3], pMsg[CHADDR+4], pMsg[CHADDR+5], nLen);

	u8 nReplyType;
	if (pType[0] == DHCP_DISCOVER)     nReplyType = DHCP_OFFER;
	else if (pType[0] == DHCP_REQUEST) { nReplyType = DHCP_ACK; g_dhcpClientConnected = true; }
	else return;                        // ignore RELEASE/DECLINE/INFORM for this minimal server

	// Assign a stable-per-device address: 192.168.x.(100 + (MAC_last6bits)).
	u8 YourIP[4] = { m_ServerIP[0], m_ServerIP[1], m_ServerIP[2],
			 (u8) (100 + (pMsg[CHADDR+5] & 0x3F)) };

	// --- Build the reply ---
	u8 R[300];
	memset (R, 0, sizeof R);
	R[OP]    = 2;                        // BOOTREPLY
	R[HTYPE] = 1;                        // Ethernet
	R[HLEN]  = 6;
	memcpy (&R[XID], &pMsg[XID], 4);     // echo transaction id
	memcpy (&R[FLAGS], &pMsg[FLAGS], 2); // echo broadcast flag
	memcpy (&R[YIADDR], YourIP, 4);      // offered address
	memcpy (&R[SIADDR], m_ServerIP, 4);
	memcpy (&R[CHADDR], &pMsg[CHADDR], 16);
	R[MAGIC] = 0x63; R[MAGIC+1] = 0x82; R[MAGIC+2] = 0x53; R[MAGIC+3] = 0x63;

	unsigned o = OPTIONS;
	R[o++] = 53; R[o++] = 1; R[o++] = nReplyType;                 // message type
	R[o++] = 54; R[o++] = 4; memcpy (&R[o], m_ServerIP, 4); o += 4;  // server id
	R[o++] = 51; R[o++] = 4;                                       // lease time = 86400s
	R[o++] = 0x00; R[o++] = 0x01; R[o++] = 0x51; R[o++] = 0x80;
	R[o++] = 1;  R[o++] = 4; R[o++] = 255; R[o++] = 255; R[o++] = 255; R[o++] = 0;  // subnet
	R[o++] = 3;  R[o++] = 4; memcpy (&R[o], m_ServerIP, 4); o += 4;  // router
	R[o++] = 6;  R[o++] = 4; memcpy (&R[o], m_ServerIP, 4); o += 4;  // DNS
	R[o++] = 255;                                                 // end

	// Client has no IP yet → broadcast the reply to 255.255.255.255:68.
	// Pad to the 300-byte BOOTP minimum (bytes after the 255 'end' option are zero pad).
	unsigned nSend = o < 300 ? 300 : o;
	CIPAddress Broadcast;
	Broadcast.SetBroadcast ();
	pSocket->SendTo (R, nSend, 0, Broadcast, 68);

	CLogger::Get ()->Write (From, LogNotice, "%s -> %u.%u.%u.%u",
				nReplyType == DHCP_OFFER ? "OFFER" : "ACK",
				YourIP[0], YourIP[1], YourIP[2], YourIP[3]);
}
