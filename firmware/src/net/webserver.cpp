//
// webserver.cpp — see webserver.h.
//
#include "webserver.h"
#include <circle/util.h>

#define MAX_CONTENT_SIZE	8192

// Self-contained branded page (no external assets). Served for any path so captive-portal
// probes land here too. ASCII-only source (HTML entities for the check mark / middot).
static const char s_Page[] =
"<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>Lumen Frame</title><style>"
"html,body{margin:0;height:100%;font-family:-apple-system,Segoe UI,Roboto,sans-serif;"
"background:linear-gradient(135deg,#1c1234,#84264f 55%,#22102a);color:#eee}"
".w{min-height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;"
"text-align:center;padding:24px;box-sizing:border-box}"
"h1{font-size:28px;letter-spacing:4px;margin:0 0 4px;font-weight:600}"
".ok{color:#7fe3c0;font-size:15px;margin:8px 0 22px}"
".c{background:rgba(255,255,255,.08);border-radius:16px;padding:22px 26px;max-width:360px}"
".m{color:#d8cbd2;font-size:13px;line-height:1.55}"
".v{margin-top:26px;color:#b299a6;font-size:11px;letter-spacing:1px}"
"</style></head><body><div class=\"w\">"
"<h1>LUMEN FRAME</h1>"
"<div class=\"ok\">&#10003; Connected to the frame</div>"
"<div class=\"c\"><div class=\"m\">You are on the frame's private Wi-Fi &mdash; no internet needed."
"<br><br>This page will soon list photos that need converting. Your phone will convert them and "
"save them back to the drive.</div></div>"
"<div class=\"v\">v0.3 &middot; SoftAP + DHCP + HTTP working</div>"
"</div></body></html>";

CWebServer::CWebServer (CNetSubSystem *pNet, CSocket *pSocket)
:	CHTTPDaemon (pNet, pSocket, MAX_CONTENT_SIZE, HTTP_PORT)
{
}

CWebServer::~CWebServer (void)
{
}

CHTTPDaemon *CWebServer::CreateWorker (CNetSubSystem *pNet, CSocket *pSocket)
{
	return new CWebServer (pNet, pSocket);
}

THTTPStatus CWebServer::GetContent (const char *pPath, const char *pParams, const char *pFormData,
				    u8 *pBuffer, unsigned *pLength, const char **ppContentType)
{
	(void) pPath; (void) pParams; (void) pFormData;

	unsigned nLen = 0;
	while (s_Page[nLen]) nLen++;
	if (nLen > *pLength) return HTTPInternalServerError;

	memcpy (pBuffer, s_Page, nLen);
	*pLength = nLen;
	*ppContentType = "text/html; charset=utf-8";

	return HTTPOK;
}
