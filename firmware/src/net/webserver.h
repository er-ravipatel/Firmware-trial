//
// webserver.h — HTTP server for the Lumen Frame settings page (served over the SoftAP). Renders
// the current settings from CConfig and saves edits back to SD:/lumen.conf (restart to apply).
//
#ifndef _webserver_h
#define _webserver_h

#include <circle/net/httpdaemon.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include <circle/types.h>
#include "Config.h"    // app-layer config; reachable via EXTRAINCLUDE=-I../app

class CWebServer : public CHTTPDaemon
{
public:
	CWebServer (CNetSubSystem *pNet, CConfig *pConfig, CSocket *pSocket = 0);
	~CWebServer (void);

	CHTTPDaemon *CreateWorker (CNetSubSystem *pNet, CSocket *pSocket) override;

	THTTPStatus GetContent (const char *pPath, const char *pParams, const char *pFormData,
				u8 *pBuffer, unsigned *pLength, const char **ppContentType) override;

private:
	void ApplyForm (const char *pFormData);     // parse urlencoded POST body -> config -> save

	CNetSubSystem *m_pNet;
	CConfig       *m_pConfig;
};

#endif
