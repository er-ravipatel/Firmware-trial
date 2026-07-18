//
// webserver.cpp — tiny custom HTTP/1.0 server (see webserver.h).
//
#include "webserver.h"
#include <circle/net/in.h>
#include <circle/net/ipaddress.h>
#include <circle/string.h>
#include <circle/util.h>
#include <circle/alloc.h>
#include <circle/logger.h>
#include <fatfs/ff.h>

static const char FromWeb[] = "web";

#define BUF_SIZE	(8 * 1024 * 1024)   // reused once; holds a served HEIC or an uploaded JPEG

volatile bool g_restartRequested = false;
volatile bool g_rescanRequested  = false;

// Dark wine-gradient theme + conversion-card styles.
static const char s_Head[] =
"<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>Frame</title><style>"
"*{box-sizing:border-box}html,body{margin:0;min-height:100%;font-family:-apple-system,Segoe UI,"
"Roboto,sans-serif;background:linear-gradient(135deg,#1c1234,#84264f 55%,#22102a);color:#eee}"
".w{max-width:440px;margin:0 auto;padding:24px 18px}"
"h1{font-size:23px;letter-spacing:3px;margin:6px 0 2px;font-weight:600;text-align:center}"
".s{color:#c9b8c6;font-size:13px;text-align:center;margin:0 0 20px}"
"h2{font-size:15px;color:#e8c48a;margin:22px 0 10px;letter-spacing:1px}"
"label{display:block;color:#d8cbd2;font-size:13px;margin:12px 0 4px}"
"input{width:100%;padding:11px 12px;border-radius:10px;border:1px solid rgba(255,255,255,.18);"
"background:rgba(255,255,255,.08);color:#fff;font-size:15px}"
"button{margin-top:8px;width:100%;padding:12px;border:0;border-radius:11px;background:#e8c48a;"
"color:#2a1030;font-size:15px;font-weight:600}"
"button:disabled{background:rgba(255,255,255,.12);color:#b299a6}"
".r{background:#c0556f;color:#fff}.n{color:#b299a6;font-size:12px;text-align:center;margin-top:16px}"
".ok{color:#7fe3c0;font-size:16px;text-align:center;margin:22px 0 8px}a{color:#e8c48a}"
".pc{position:relative;border-radius:14px;overflow:hidden;margin:10px 0;background:#000;"
"min-height:150px;display:flex;align-items:center;justify-content:center}.pc img{width:100%;display:block}"
".ov{position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;"
"justify-content:center;background:rgba(10,6,16,.32);gap:10px;padding:14px}"
".ov button{width:auto;padding:11px 26px;margin:0}"
".pb{width:82%;height:10px;border-radius:6px;background:rgba(255,255,255,.28);overflow:hidden}"
".pb>i{display:block;height:100%;width:0;background:#7fe3c0}"
".stop{background:rgba(255,255,255,.22);color:#fff;padding:8px 20px}"
".done{color:#7fe3c0;font-size:17px;font-weight:600}.cap{color:#eee;font-size:13px;text-align:center}"
".nm{color:#c9b8c6;font-size:12px;margin:2px 2px 10px}"
"</style></head><body><div class=\"w\">";
static const char s_Foot[] = "</div></body></html>";

CWebServer::CWebServer (CNetSubSystem *pNet, CConfig *pConfig, lf::IPhotoSource *pPhotos)
:	m_pNet (pNet), m_pConfig (pConfig), m_pPhotos (pPhotos), m_pBuf (0), m_nBufSize (BUF_SIZE)
{
}

CWebServer::~CWebServer (void)
{
	m_pNet = 0; m_pConfig = 0; m_pPhotos = 0;
}

// --- small helpers ---
static char lc (char c) { return (c >= 'A' && c <= 'Z') ? (char) (c + 32) : c; }
static bool ciMatch (const u8 *p, const char *s, unsigned n)
{ for (unsigned i = 0; i < n; i++) if (lc ((char) p[i]) != lc (s[i])) return false; return true; }
static bool pathEq (const char *a, const char *b)
{ while (*a && *b) { if (*a != *b) return false; a++; b++; } return *a == *b; }

static char hexv (char c)
{ if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10; return 0; }

static void urldecode (char *dst, unsigned cap, const char *src)
{
	unsigned o = 0;
	while (*src && o + 1 < cap)
	{
		if (*src == '+') { dst[o++] = ' '; src++; }
		else if (*src == '%' && src[1] && src[2]) { dst[o++] = (char) ((hexv (src[1]) << 4) | hexv (src[2])); src += 3; }
		else dst[o++] = *src++;
	}
	dst[o] = '\0';
}

static void appendEsc (CString &s, const char *src)
{
	for (; *src; src++)
		switch (*src)
		{
		case '&': s.Append ("&amp;"); break;
		case '<': s.Append ("&lt;"); break;
		case '>': s.Append ("&gt;"); break;
		case '"': s.Append ("&quot;"); break;
		default: { char c[2] = { *src, 0 }; s.Append (c); } break;
		}
}

static int paramInt (const char *q, const char *key, int def)
{
	if (q == 0) return def;
	unsigned kl = 0; while (key[kl]) kl++;
	for (const char *p = q; *p; )
	{
		bool m = true; for (unsigned i = 0; i < kl; i++) if (p[i] != key[i]) { m = false; break; }
		if (m && p[kl] == '=')
		{
			const char *v = p + kl + 1; int n = 0; bool any = false;
			while (*v >= '0' && *v <= '9') { n = n * 10 + (*v - '0'); v++; any = true; }
			return any ? n : def;
		}
		while (*p && *p != '&') p++; if (*p == '&') p++;
	}
	return def;
}

static void sendAll (CSocket *pConn, const u8 *pBuf, unsigned nLen)
{
	unsigned sent = 0;
	while (sent < nLen)
	{
		int r = pConn->Send (pBuf + sent, nLen - sent, 0);
		if (r <= 0) break;
		sent += (unsigned) r;
	}
}

void CWebServer::SendHead (CSocket *pConn, const char *pStatus, const char *pType, unsigned nLen)
{
	CString H;
	H.Format ("HTTP/1.0 %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",
		  pStatus, pType, nLen);
	sendAll (pConn, (const u8 *) (const char *) H, H.GetLength ());
}

void CWebServer::SendPage (CSocket *pConn, const char *pHtml)
{
	unsigned n = 0; while (pHtml[n]) n++;
	SendHead (pConn, "200 OK", "text/html; charset=utf-8", n);
	sendAll (pConn, (const u8 *) pHtml, n);
}

// --- page builders ---
static void field (CString &s, CConfig *cfg, const char *label, const char *key, const char *def)
{
	s.Append ("<label>"); s.Append (label);
	s.Append ("<input name=\""); s.Append (key); s.Append ("\" value=\"");
	appendEsc (s, cfg->GetStr (key, def));
	s.Append ("\"></label>");
}

static void settingsPage (CString &P, CConfig *cfg)
{
	P.Append (s_Head);
	P.Append ("<h1>"); appendEsc (P, cfg->GetStr ("name", "LUMEN FRAME"));
	P.Append ("</h1><p class=\"s\">Frame settings</p><form method=\"post\" action=\"/\">");
	field (P, cfg, "Display name",      "name",    "LUMEN FRAME");
	field (P, cfg, "Tagline",           "tagline", "Memory Lane Walkthrough");
	field (P, cfg, "Wi-Fi name (SSID)", "ssid",    "LumenFrame");
	field (P, cfg, "Credits",           "credits", "");
	P.Append ("<button>Save</button></form>");
	P.Append ("<button class=\"r\" disabled>Restart (save first)</button>");
	P.Append ("<p class=\"n\">Settings apply after you restart the frame.</p>");
	P.Append (s_Foot);
}

// Inline conversion JS: preview each HEIC via native decode, convert to JPEG (raw POST body) on tap.
static const char s_ConvJs[] =
"<script>function q(c,s){return c.querySelector(s)}"
"function conv(b){var c=b.closest('.pc'),i=c.dataset.i,im=q(c,'img');c._stop=false;"
"q(c,'.ov').innerHTML='<div class=pb><i></i></div><button class=stop onclick=\"stopc(this)\">Stop</button>';"
"var cv=document.createElement('canvas'),w=im.naturalWidth,h=im.naturalHeight,"
"s=Math.min(1,1920/Math.max(w,h));cv.width=(w*s)|0;cv.height=(h*s)|0;"
"cv.getContext('2d').drawImage(im,0,0,cv.width,cv.height);"
"cv.toBlob(function(bl){if(c._stop||!bl)return;var x=new XMLHttpRequest();"
"x.upload.onprogress=function(e){if(e.lengthComputable)q(c,'.pb>i').style.width=(e.loaded/e.total*100)+'%'};"
"x.onload=function(){q(c,'.ov').innerHTML='<div class=done>Converted \\u2713</div>'};"
"x.onerror=function(){q(c,'.ov').innerHTML='<button onclick=\"conv(this)\">Retry</button>'};"
"x.open('POST','/jpg?i='+i);x.send(bl)},'image/jpeg',0.85)}"
"function stopc(b){var c=b.closest('.pc');c._stop=true;"
"q(c,'.ov').innerHTML='<button onclick=\"conv(this)\">Convert</button>'}"
"function noprev(im){q(im.closest('.pc'),'.ov').innerHTML='<div class=cap>Open this page in Safari to preview</div>'}"
"</script>";

static void photosPage (CString &P, CConfig *cfg, lf::IPhotoSource *photos)
{
	P.Append (s_Head);
	P.Append ("<h1>"); appendEsc (P, cfg->GetStr ("name", "LUMEN FRAME"));
	P.Append ("</h1><p class=\"s\">Photo conversion</p>");

	unsigned nPending = 0;
	if (photos) for (unsigned i = 0; i < photos->count (); i++) if (photos->needs_convert (i)) nPending++;

	if (nPending == 0)
	{
		P.Append ("<p class=\"cap\" style=\"margin-top:40px\">&#10003; No photos need converting.</p>");
	}
	else
	{
		CString H2; H2.Format ("<h2>Photos to convert (%u)</h2>", nPending);
		P.Append ((const char *) H2);
		for (unsigned i = 0; i < photos->count (); i++)
		{
			if (!photos->needs_convert (i)) continue;
			CString Card;
			Card.Format ("<div class=\"pc\" data-i=\"%u\"><img src=\"/heic?i=%u\" onerror=\"noprev(this)\">"
				     "<div class=\"ov\"><button onclick=\"conv(this)\">Convert</button></div></div>", i, i);
			P.Append ((const char *) Card);
			P.Append ("<div class=\"nm\">"); appendEsc (P, photos->name (i)); P.Append ("</div>");
		}
		P.Append (s_ConvJs);
	}
	P.Append ("<p class=\"n\"><a href=\"/\">Frame settings</a></p>");
	P.Append (s_Foot);
}

// --- request routing ---
void CWebServer::ServeHeic (CSocket *pConn, const char *pQuery)
{
	int i = paramInt (pQuery, "i", -1);
	const char *pPath = (m_pPhotos && i >= 0) ? m_pPhotos->path ((unsigned) i) : "";
	FIL File;
	if (pPath[0] == '\0' || f_open (&File, pPath, FA_READ) != FR_OK)
	{
		SendHead (pConn, "404 Not Found", "text/plain", 0);
		return;
	}
	FSIZE_t nSize = f_size (&File);
	if (nSize > m_nBufSize) { f_close (&File); SendHead (pConn, "500 Too Large", "text/plain", 0); return; }
	UINT nRead = 0;
	f_read (&File, m_pBuf, (UINT) nSize, &nRead);   // reuse the big buffer (request already parsed)
	f_close (&File);
	SendHead (pConn, "200 OK", "image/heic", (unsigned) nRead);
	sendAll (pConn, m_pBuf, (unsigned) nRead);
}

void CWebServer::SaveJpg (CSocket *pConn, const char *pQuery, const u8 *pBody, unsigned nBodyLen)
{
	int i = paramInt (pQuery, "i", -1);
	const char *pSrc = (m_pPhotos && i >= 0) ? m_pPhotos->path ((unsigned) i) : "";
	if (pSrc[0] == '\0' || nBodyLen == 0) { SendHead (pConn, "400 Bad Request", "text/plain", 0); return; }

	char tgt[80]; unsigned k = 0, dot = 0;
	for (; pSrc[k] && k + 5 < sizeof tgt; k++) { tgt[k] = pSrc[k]; if (pSrc[k] == '.') dot = k; }
	if (dot == 0) dot = k;
	tgt[dot] = '.'; tgt[dot + 1] = 'j'; tgt[dot + 2] = 'p'; tgt[dot + 3] = 'g'; tgt[dot + 4] = '\0';

	FIL File;
	if (f_open (&File, tgt, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
	{ SendHead (pConn, "500 Write Failed", "text/plain", 0); return; }
	UINT nWr = 0;
	f_write (&File, pBody, nBodyLen, &nWr);
	f_sync (&File); f_close (&File);
	if (nWr != nBodyLen) { SendHead (pConn, "500 Write Failed", "text/plain", 0); return; }

	g_rescanRequested = true;
	CLogger::Get ()->Write (FromWeb, LogNotice, "converted -> %s (%u bytes)", tgt, nBodyLen);
	const char ok[] = "{\"ok\":1}";
	SendHead (pConn, "200 OK", "application/json", sizeof ok - 1);
	sendAll (pConn, (const u8 *) ok, sizeof ok - 1);
}

void CWebServer::Handle (CSocket *pConn)
{
	// Read the request headers (small) into the front of the buffer.
	unsigned total = 0;
	int hdrEnd = -1;
	while (hdrEnd < 0 && total < 8192)
	{
		int r = pConn->Receive (m_pBuf + total, 8192 - total, 0);
		if (r <= 0) return;
		total += (unsigned) r;
		for (unsigned i = 3; i < total; i++)
			if (m_pBuf[i-3]=='\r' && m_pBuf[i-2]=='\n' && m_pBuf[i-1]=='\r' && m_pBuf[i]=='\n')
			{ hdrEnd = (int) (i + 1); break; }
	}
	if (hdrEnd < 0) return;

	// Method + path (stack copies, so we may reuse m_pBuf for a served file afterwards).
	char method[8] = {0}, path[160] = {0};
	unsigned p = 0, o = 0;
	while (p < (unsigned) hdrEnd && m_pBuf[p] != ' ' && o + 1 < sizeof method) method[o++] = m_pBuf[p++];
	method[o] = 0;
	while (p < (unsigned) hdrEnd && m_pBuf[p] == ' ') p++;
	o = 0;
	while (p < (unsigned) hdrEnd && m_pBuf[p] != ' ' && o + 1 < sizeof path) path[o++] = m_pBuf[p++];
	path[o] = 0;

	char *query = path; while (*query && *query != '?') query++;
	if (*query == '?') { *query = 0; query++; } else query = (char *) "";

	// Content-Length.
	unsigned contentLen = 0;
	for (unsigned i = 0; i + 16 < (unsigned) hdrEnd; i++)
		if (ciMatch (m_pBuf + i, "content-length:", 15))
		{ unsigned j = i + 15; while (m_pBuf[j] == ' ') j++;
		  while (m_pBuf[j] >= '0' && m_pBuf[j] <= '9') { contentLen = contentLen * 10 + (m_pBuf[j] - '0'); j++; } break; }

	bool isPost = (method[0] == 'P');

	// OS captive-detection probes. Answering each with its vendor's expected "you have internet"
	// response makes the phone BELIEVE the AP is online, so it STAYS joined to the frame instead of
	// dropping back to cellular/home Wi-Fi. That is what makes the convert-slide URL QR reach us in
	// full Safari (the phone must still be on 192.168.1.1's network when it scans). GETs, no body.
	if (!isPost)
	{
		if (pathEq (path, "/generate_204") || pathEq (path, "/gen_204"))
		{ SendHead (pConn, "204 No Content", "text/plain", 0); return; }        // Android
		if (pathEq (path, "/hotspot-detect.html") || pathEq (path, "/library/test/success.html"))
		{ static const char ok[] = "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>";
		  SendHead (pConn, "200 OK", "text/html", sizeof ok - 1);
		  sendAll (pConn, (const u8 *) ok, sizeof ok - 1); return; }            // iOS / macOS
		if (pathEq (path, "/ncsi.txt"))
		{ static const char t[] = "Microsoft NCSI"; SendHead (pConn, "200 OK", "text/plain", sizeof t - 1);
		  sendAll (pConn, (const u8 *) t, sizeof t - 1); return; }              // Windows
		if (pathEq (path, "/connecttest.txt"))
		{ static const char t[] = "Microsoft Connect Test"; SendHead (pConn, "200 OK", "text/plain", sizeof t - 1);
		  sendAll (pConn, (const u8 *) t, sizeof t - 1); return; }              // Windows
	}

	// Read the body (if any) right after the headers.
	u8 *body = m_pBuf + hdrEnd;
	unsigned bodyHave = total - (unsigned) hdrEnd;
	while (isPost && bodyHave < contentLen && (unsigned) hdrEnd + contentLen <= m_nBufSize)
	{
		int r = pConn->Receive (body + bodyHave, m_nBufSize - hdrEnd - bodyHave, 0);
		if (r <= 0) break;
		bodyHave += (unsigned) r;
	}

	// Route.
	if (!isPost && pathEq (path, "/heic")) { ServeHeic (pConn, query); return; }
	if (isPost && pathEq (path, "/jpg"))   { SaveJpg (pConn, query, body, bodyHave); return; }

	CString Page;
	if (isPost && pathEq (path, "/restart"))
	{
		g_restartRequested = true;
		Page.Append (s_Head); Page.Append ("<h1>"); appendEsc (Page, m_pConfig->GetStr ("name", "LUMEN FRAME"));
		Page.Append ("</h1><p class=\"ok\">&#10003; Restarting...</p>");
		Page.Append ("<p class=\"n\">The frame is restarting to apply your settings.</p>"); Page.Append (s_Foot);
	}
	else if (isPost && pathEq (path, "/"))
	{
		// Save settings (urlencoded body).
		char kbuf[24], vbuf[96], dval[96];
		const char *bp = (const char *) body; unsigned left = bodyHave;
		while (left > 0)
		{
			unsigned kl = 0; while (left && *bp != '=' && *bp != '&' && kl + 1 < sizeof kbuf) { kbuf[kl++] = *bp++; left--; } kbuf[kl] = 0;
			unsigned vl = 0; if (left && *bp == '=') { bp++; left--; while (left && *bp != '&' && vl + 1 < sizeof vbuf) { vbuf[vl++] = *bp++; left--; } } vbuf[vl] = 0;
			if (left && *bp == '&') { bp++; left--; }
			if (kl) { urldecode (dval, sizeof dval, vbuf); m_pConfig->Set (kbuf, dval); }
		}
		m_pConfig->Save ("SD:/lumen.conf");
		Page.Append (s_Head); Page.Append ("<h1>"); appendEsc (Page, m_pConfig->GetStr ("name", "LUMEN FRAME"));
		Page.Append ("</h1><p class=\"ok\">&#10003; Saved</p>");
		Page.Append ("<form method=\"post\" action=\"/restart\"><button class=\"r\">Restart now</button></form>");
		Page.Append ("<p class=\"n\"><a href=\"/\">Back</a></p>"); Page.Append (s_Foot);
	}
	else if (!isPost && pathEq (path, "/photos"))
	{
		photosPage (Page, m_pConfig, m_pPhotos);
	}
	else
	{
		settingsPage (Page, m_pConfig);   // GET / and captive-portal probes -> settings
	}
	SendPage (pConn, (const char *) Page);
}

void CWebServer::Run (void)
{
	m_pBuf = (u8 *) malloc (m_nBufSize);   // once, never freed -> no >512KB free leak
	if (m_pBuf == 0) { CLogger::Get ()->Write (FromWeb, LogError, "no buffer"); return; }

	CSocket Listener (m_pNet, IPPROTO_TCP);
	if (Listener.Bind (80) < 0 || Listener.Listen (8) < 0)
	{ CLogger::Get ()->Write (FromWeb, LogError, "cannot listen on :80"); return; }
	CLogger::Get ()->Write (FromWeb, LogNotice, "web server up on :80");

	for (;;)
	{
		CIPAddress ForeignIP; u16 nForeignPort;
		CSocket *pConn = Listener.Accept (&ForeignIP, &nForeignPort);
		if (pConn != 0)
		{
			Handle (pConn);
			delete pConn;   // close (small CSocket object, safe to free)
		}
	}
}
