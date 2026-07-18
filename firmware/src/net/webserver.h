//
// webserver.h — minimal HTTP server for the SoftAP spike (proves the phone can open a page
// served by the Pi). Grows into the v0.3 photo-conversion page.
//
#ifndef _webserver_h
#define _webserver_h

#include <circle/net/httpdaemon.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include <circle/types.h>

class CWebServer : public CHTTPDaemon
{
public:
	CWebServer (CNetSubSystem *pNet, CSocket *pSocket = 0);
	~CWebServer (void);

	CHTTPDaemon *CreateWorker (CNetSubSystem *pNet, CSocket *pSocket) override;

	THTTPStatus GetContent (const char *pPath, const char *pParams, const char *pFormData,
				u8 *pBuffer, unsigned *pLength, const char **ppContentType) override;
};

#endif
