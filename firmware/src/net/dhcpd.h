//
// dhcpd.h — minimal DHCP server for the Lumen Frame SoftAP (Circle has only a DHCP client).
// Hands out one lease per client so a phone joining the AP gets an IP automatically.
//
#ifndef _dhcpd_h
#define _dhcpd_h

#include <circle/sched/task.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include <circle/types.h>

// Set TRUE once a client has taken a lease (completed a DHCP ACK) — i.e. a phone actually joined
// the AP. The kernel polls this to switch from the splash/slideshow into settings mode.
extern volatile bool g_dhcpClientConnected;

class CDHCPD : public CTask
{
public:
	// pServerIP: the AP's own IP (also used as gateway + DNS), e.g. {192,168,1,1}.
	CDHCPD (CNetSubSystem *pNet, const u8 *pServerIP);
	~CDHCPD (void);

	void Run (void) override;

private:
	void HandlePacket (CSocket *pSocket, u8 *pMsg, unsigned nLen);

	CNetSubSystem *m_pNet;
	u8 m_ServerIP[4];
};

#endif
