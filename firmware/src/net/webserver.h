//
// webserver.h — Lumen Frame web server. A tiny custom HTTP/1.0 server (a scheduler task) that
// serves everything from ONE reused buffer allocated once (never freed -> no leak), so it can move
// multi-MB HEIC/JPEG files that CHTTPDaemon's per-connection buffer could not (Circle's heap leaks
// freed blocks > 512 KB). Single-threaded: one connection at a time, so the buffer is race-free.
//
// Routes:  GET  /            settings page (also the captive-portal landing)
//          POST /            save settings -> confirm + Restart button
//          POST /restart     reboot the frame
//          GET  /photos      conversion page (opened in full Safari via the photo-slide URL QR)
//          GET  /heic?i=N    stream the raw HEIC file N (phone's browser decodes it)
//          POST /jpg?i=N     write the phone-converted JPEG back next to file N, then re-scan
//
#ifndef _webserver_h
#define _webserver_h

#include <circle/sched/task.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include <circle/types.h>
#include "Config.h"                   // via EXTRAINCLUDE=-I../app
#include "content/IPhotoSource.h"     // via EXTRAINCLUDE=-I../src

extern volatile bool g_restartRequested;   // settings page -> kernel reboots
extern volatile bool g_rescanRequested;    // conversion write-back -> kernel re-scans

class CWebServer : public CTask
{
public:
	CWebServer (CNetSubSystem *pNet, CConfig *pConfig, lf::IPhotoSource *pPhotos);
	~CWebServer (void);

	void Run (void) override;   // listen on :80, accept + handle connections forever

private:
	void Handle (CSocket *pConn, const u8 clientIP[4]);
	// true if this phone already saw the captive portal once (so we now report "online" and keep
	// it joined); false the first time (marks it, and the caller lets the portal auto-open).
	bool CaptiveSeenAndMark (const u8 ip[4]);
	void SendHead (CSocket *pConn, const char *pStatus, const char *pType, unsigned nLen);
	void SendPage (CSocket *pConn, const char *pHtml);   // text/html 200
	void ServeHeic (CSocket *pConn, const char *pQuery);
	void SaveJpg   (CSocket *pConn, const char *pQuery, const u8 *pBody, unsigned nBodyLen);

	CNetSubSystem    *m_pNet;
	CConfig          *m_pConfig;
	lf::IPhotoSource *m_pPhotos;
	u8               *m_pBuf;      // reused buffer (request/body/file); allocated once, never freed
	unsigned          m_nBufSize;

	u8                m_Seen[8][4];   // recent phones shown the captive portal (ring)
	unsigned          m_nSeen;        // valid entries in m_Seen (<= 8)
	unsigned          m_SeenNext;     // next ring slot to overwrite
};

#endif
