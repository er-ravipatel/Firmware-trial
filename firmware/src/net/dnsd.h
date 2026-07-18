//
// dnsd.h — minimal DNS responder for the captive portal. Answers EVERY A-record query with the
// AP's own IP, so the phone's connectivity-check hostname resolves to us and the setup page
// auto-opens. The phone uses us as its DNS because the DHCP offer sets DNS = the AP IP.
//
#ifndef _dnsd_h
#define _dnsd_h

#include <circle/sched/task.h>
#include <circle/net/netsubsystem.h>
#include <circle/types.h>

class CDNSD : public CTask
{
public:
	CDNSD (CNetSubSystem *pNet, const u8 *pServerIP);
	~CDNSD (void);

	void Run (void) override;

private:
	CNetSubSystem *m_pNet;
	u8 m_ServerIP[4];
};

#endif
