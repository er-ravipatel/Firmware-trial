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
#include "Config.h"                   // app-layer config; via EXTRAINCLUDE=-I../app
#include "content/IPhotoSource.h"     // via EXTRAINCLUDE=-I../src

// Set TRUE when the settings page requests a restart (POST /restart); the kernel reboots.
extern volatile bool g_restartRequested;
// Set TRUE after a converted JPEG is written back; the kernel re-scans so the photo resolves
// into the slideshow (and the HEIC's QR slide disappears) with no reboot.
extern volatile bool g_rescanRequested;

class CWebServer : public CHTTPDaemon
{
public:
	CWebServer (CNetSubSystem *pNet, CConfig *pConfig, lf::IPhotoSource *pPhotos,
		    CSocket *pSocket = 0);
	~CWebServer (void);

	CHTTPDaemon *CreateWorker (CNetSubSystem *pNet, CSocket *pSocket) override;

	THTTPStatus GetContent (const char *pPath, const char *pParams, const char *pFormData,
				u8 *pBuffer, unsigned *pLength, const char **ppContentType) override;

private:
	void ApplyForm (const char *pFormData);     // parse urlencoded POST body -> config -> save
	THTTPStatus ServeHeic (const char *pParams, u8 *pBuffer, unsigned *pLength,
			       const char **ppContentType);   // GET /heic?i=N -> raw file bytes
	THTTPStatus SaveJpg (const char *pParams, u8 *pBuffer, unsigned *pLength,
			     const char **ppContentType);     // POST /jpg?i=N  -> write back

	CNetSubSystem    *m_pNet;
	CConfig          *m_pConfig;
	lf::IPhotoSource *m_pPhotos;
};

#endif
