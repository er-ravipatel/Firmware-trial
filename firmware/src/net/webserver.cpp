//
// webserver.cpp — Lumen Frame settings page (see webserver.h). GET renders a form of the current
// settings; POST saves the edits back to SD:/lumen.conf (restart to apply).
//
#include "webserver.h"
#include <circle/util.h>
#include <circle/string.h>

#define MAX_CONTENT_SIZE	16384

// Shared CSS + page head/foot (dark wine gradient, matching the frame).
static const char s_Head[] =
"<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>Frame settings</title><style>"
"*{box-sizing:border-box}html,body{margin:0;min-height:100%;font-family:-apple-system,Segoe UI,"
"Roboto,sans-serif;background:linear-gradient(135deg,#1c1234,#84264f 55%,#22102a);color:#eee}"
".w{max-width:420px;margin:0 auto;padding:26px 22px}"
"h1{font-size:24px;letter-spacing:3px;margin:8px 0 2px;font-weight:600;text-align:center}"
".s{color:#c9b8c6;font-size:13px;text-align:center;margin:0 0 22px}"
"label{display:block;color:#d8cbd2;font-size:13px;margin:14px 0 4px}"
"input{width:100%;padding:11px 12px;border-radius:10px;border:1px solid rgba(255,255,255,.18);"
"background:rgba(255,255,255,.08);color:#fff;font-size:15px}"
"button{margin-top:22px;width:100%;padding:13px;border:0;border-radius:12px;background:#e8c48a;"
"color:#2a1030;font-size:16px;font-weight:600}"
"button:disabled{background:rgba(255,255,255,.12);color:#b299a6}"
".r{background:#c0556f;color:#fff}"
".n{color:#b299a6;font-size:12px;text-align:center;margin-top:18px}"
".ok{color:#7fe3c0;font-size:16px;text-align:center;margin:26px 0 8px}"
"a{color:#e8c48a}</style></head><body><div class=\"w\">";

static const char s_Foot[] = "</div></body></html>";

CWebServer::CWebServer (CNetSubSystem *pNet, CConfig *pConfig, CSocket *pSocket)
:	CHTTPDaemon (pNet, pSocket, MAX_CONTENT_SIZE, HTTP_PORT),
	m_pNet (pNet),
	m_pConfig (pConfig)
{
}

volatile bool g_restartRequested = false;   // set by POST /restart; polled by the kernel

CWebServer::~CWebServer (void)
{
	m_pNet = 0;
	m_pConfig = 0;
}

// TRUE if pPath contains pNeedle (e.g. the "/restart" endpoint).
static boolean pathHas (const char *pPath, const char *pNeedle)
{
	if (pPath == 0) return FALSE;
	for (const char *h = pPath; *h; h++)
	{
		const char *a = h, *b = pNeedle;
		while (*a && *b && *a == *b) { a++; b++; }
		if (*b == '\0') return TRUE;
	}
	return FALSE;
}

CHTTPDaemon *CWebServer::CreateWorker (CNetSubSystem *pNet, CSocket *pSocket)
{
	return new CWebServer (pNet, m_pConfig, pSocket);
}

// --- small helpers ---
static char hexval (char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return 0;
}

// URL-decode src into dst: '+' -> space, %XX -> byte.
static void urldecode (char *dst, unsigned cap, const char *src)
{
	unsigned o = 0;
	while (*src && o + 1 < cap)
	{
		if (*src == '+') { dst[o++] = ' '; src++; }
		else if (*src == '%' && src[1] && src[2]) { dst[o++] = (char) ((hexval (src[1]) << 4) | hexval (src[2])); src += 3; }
		else dst[o++] = *src++;
	}
	dst[o] = '\0';
}

// Append src to a CString with HTML-escaping (& < > ").
static void appendEsc (CString &s, const char *src)
{
	for (; *src; src++)
	{
		switch (*src)
		{
		case '&': s.Append ("&amp;"); break;
		case '<': s.Append ("&lt;"); break;
		case '>': s.Append ("&gt;"); break;
		case '"': s.Append ("&quot;"); break;
		default: { char c[2] = { *src, 0 }; s.Append (c); } break;
		}
	}
}

void CWebServer::ApplyForm (const char *pFormData)
{
	const char *p = pFormData;
	char key[24], val[96], dval[96];
	while (*p)
	{
		unsigned k = 0;
		while (*p && *p != '=' && *p != '&' && k + 1 < sizeof key) key[k++] = *p++;
		key[k] = '\0';
		unsigned v = 0;
		if (*p == '=') { p++; while (*p && *p != '&' && v + 1 < sizeof val) val[v++] = *p++; }
		val[v] = '\0';
		if (*p == '&') p++;
		if (k > 0) { urldecode (dval, sizeof dval, val); m_pConfig->Set (key, dval); }
	}
	m_pConfig->Save ("SD:/lumen.conf");
}

// One text field: label + input pre-filled with the current (escaped) config value.
static void field (CString &s, CConfig *cfg, const char *label, const char *key, const char *def)
{
	s.Append ("<label>"); s.Append (label);
	s.Append ("<input name=\""); s.Append (key); s.Append ("\" value=\"");
	appendEsc (s, cfg->GetStr (key, def));
	s.Append ("\"></label>");
}

THTTPStatus CWebServer::GetContent (const char *pPath, const char *pParams, const char *pFormData,
				    u8 *pBuffer, unsigned *pLength, const char **ppContentType)
{
	(void) pParams;

	CString Page (s_Head);
	const char *pName = m_pConfig != 0 ? m_pConfig->GetStr ("name", "LUMEN FRAME") : "LUMEN FRAME";

	if (pathHas (pPath, "restart"))
	{
		// Restart requested — the kernel's settings loop will reboot after flushing this reply.
		g_restartRequested = true;
		Page.Append ("<h1>"); appendEsc (Page, pName);
		Page.Append ("</h1><p class=\"ok\">&#10003; Restarting...</p>");
		Page.Append ("<p class=\"n\">The frame is restarting to apply your settings.</p>");
	}
	else if (pFormData != 0 && pFormData[0] != '\0' && m_pConfig != 0)
	{
		// POST save: store, then offer the (now enabled) Restart button.
		ApplyForm (pFormData);
		Page.Append ("<h1>"); appendEsc (Page, m_pConfig->GetStr ("name", "LUMEN FRAME"));
		Page.Append ("</h1><p class=\"ok\">&#10003; Saved</p>");
		Page.Append ("<p class=\"n\">Restart the frame to apply the new settings.</p>");
		Page.Append ("<form method=\"post\" action=\"/restart\"><button class=\"r\">Restart now</button></form>");
		Page.Append ("<p class=\"n\"><a href=\"/\">Back to settings</a></p>");
	}
	else if (m_pConfig != 0)
	{
		// GET: settings form. Restart button is disabled until a save happens.
		Page.Append ("<h1>"); appendEsc (Page, pName);
		Page.Append ("</h1><p class=\"s\">Frame settings</p><form method=\"post\">");
		field (Page, m_pConfig, "Display name",     "name",    "LUMEN FRAME");
		field (Page, m_pConfig, "Tagline",          "tagline", "Memory Lane Walkthrough");
		field (Page, m_pConfig, "Wi-Fi name (SSID)","ssid",    "LumenFrame");
		field (Page, m_pConfig, "Credits",          "credits", "");
		Page.Append ("<button>Save</button></form>");
		Page.Append ("<button class=\"r\" disabled>Restart (save first)</button>");
		Page.Append ("<p class=\"n\">Changes apply after you restart the frame.</p>");
	}

	Page.Append (s_Foot);

	const char *pStr = (const char *) Page;
	unsigned nLen = 0;
	while (pStr[nLen]) nLen++;
	if (nLen > *pLength) return HTTPInternalServerError;

	memcpy (pBuffer, pStr, nLen);
	*pLength = nLen;
	*ppContentType = "text/html; charset=utf-8";
	return HTTPOK;
}
