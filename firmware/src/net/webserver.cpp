//
// webserver.cpp — Lumen Frame web page (settings + offline photo conversion). Served over the
// SoftAP; never stops the slideshow. See webserver.h.
//
// Endpoints:  GET /            settings form + "photos to convert" list (+ conversion JS)
//             POST /           save settings
//             POST /restart    reboot the frame
//             GET /heic?i=N     stream the raw HEIC file N (the phone's browser decodes it)
//             POST /jpg?i=N     write the phone-converted JPEG back next to file N, then re-scan
//
#include "webserver.h"
#include <circle/util.h>
#include <circle/string.h>
#include <fatfs/ff.h>

// Keep buffers <= 512 KB: Circle's heap LEAKS any freed block larger than that, and CHTTPDaemon
// allocates+frees these per worker/connection -> serving multi-MB HEIC here OOMs the frame. Big-file
// serving/upload is being moved to a reused-buffer path; until then /heic + /jpg fail gracefully.
#define MAX_CONTENT_SIZE	(256 * 1024)
#define MAX_MULTIPART_SIZE	(256 * 1024)

volatile bool g_restartRequested = false;   // settings page -> reboot (see webserver.h)
volatile bool g_rescanRequested = false;    // conversion write-back -> kernel re-scans

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
"h2{font-size:15px;color:#e8c48a;margin:26px 0 10px;letter-spacing:1px}"
"label{display:block;color:#d8cbd2;font-size:13px;margin:12px 0 4px}"
"input{width:100%;padding:11px 12px;border-radius:10px;border:1px solid rgba(255,255,255,.18);"
"background:rgba(255,255,255,.08);color:#fff;font-size:15px}"
"button{margin-top:8px;width:100%;padding:12px;border:0;border-radius:11px;background:#e8c48a;"
"color:#2a1030;font-size:15px;font-weight:600}"
"button:disabled{background:rgba(255,255,255,.12);color:#b299a6}"
".r{background:#c0556f;color:#fff}.n{color:#b299a6;font-size:12px;text-align:center;margin-top:16px}"
".ok{color:#7fe3c0;font-size:16px;text-align:center;margin:22px 0 8px}a{color:#e8c48a}"
".pc{position:relative;border-radius:14px;overflow:hidden;margin:10px 0;background:#000;"
"min-height:150px;display:flex;align-items:center;justify-content:center}"
".pc img{width:100%;display:block}"
".ov{position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;"
"justify-content:center;background:rgba(10,6,16,.35);gap:10px;padding:14px}"
".ov button{width:auto;padding:11px 26px;margin:0}"
".pb{width:80%;height:10px;border-radius:6px;background:rgba(255,255,255,.25);overflow:hidden}"
".pb>i{display:block;height:100%;width:0;background:#7fe3c0;transition:width .2s}"
".stop{background:rgba(255,255,255,.2);color:#fff;padding:8px 20px}"
".done{color:#7fe3c0;font-size:17px;font-weight:600}.cap{color:#eee;font-size:13px}"
".nm{color:#c9b8c6;font-size:12px;margin:2px 2px 0}"
"</style></head><body><div class=\"w\">";

static const char s_Foot[] = "</div></body></html>";

CWebServer::CWebServer (CNetSubSystem *pNet, CConfig *pConfig, lf::IPhotoSource *pPhotos,
			CSocket *pSocket)
:	CHTTPDaemon (pNet, pSocket, MAX_CONTENT_SIZE, HTTP_PORT, MAX_MULTIPART_SIZE),
	m_pNet (pNet),
	m_pConfig (pConfig),
	m_pPhotos (pPhotos)
{
}

CWebServer::~CWebServer (void)
{
	m_pNet = 0; m_pConfig = 0; m_pPhotos = 0;
}

CHTTPDaemon *CWebServer::CreateWorker (CNetSubSystem *pNet, CSocket *pSocket)
{
	return new CWebServer (pNet, m_pConfig, m_pPhotos, pSocket);
}

// --- helpers ---
static char hexval (char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return 0;
}

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

// Read integer value of `key` from "k=v&k2=v2". Returns def if absent.
static int paramInt (const char *pParams, const char *pKey, int def)
{
	if (pParams == 0) return def;
	unsigned kl = 0; while (pKey[kl]) kl++;
	for (const char *p = pParams; *p; )
	{
		boolean match = TRUE;
		for (unsigned i = 0; i < kl; i++) if (p[i] != pKey[i]) { match = FALSE; break; }
		if (match && p[kl] == '=')
		{
			const char *v = p + kl + 1;
			boolean neg = (*v == '-'); if (neg) v++;
			int n = 0; boolean any = FALSE;
			while (*v >= '0' && *v <= '9') { n = n * 10 + (*v - '0'); v++; any = TRUE; }
			return any ? (neg ? -n : n) : def;
		}
		while (*p && *p != '&') p++;
		if (*p == '&') p++;
	}
	return def;
}

static void field (CString &s, CConfig *cfg, const char *label, const char *key, const char *def)
{
	s.Append ("<label>"); s.Append (label);
	s.Append ("<input name=\""); s.Append (key); s.Append ("\" value=\"");
	appendEsc (s, cfg->GetStr (key, def));
	s.Append ("\"></label>");
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

// GET /heic?i=N — stream the raw HEIC bytes so the phone's browser can decode them.
THTTPStatus CWebServer::ServeHeic (const char *pParams, u8 *pBuffer, unsigned *pLength,
				   const char **ppContentType)
{
	int i = paramInt (pParams, "i", -1);
	if (m_pPhotos == 0 || i < 0) return HTTPNotFound;
	const char *pPath = m_pPhotos->path ((unsigned) i);
	if (pPath[0] == '\0') return HTTPNotFound;

	FIL File;
	if (f_open (&File, pPath, FA_READ) != FR_OK) return HTTPNotFound;
	FSIZE_t nSize = f_size (&File);
	if (nSize > *pLength) { f_close (&File); return HTTPInternalServerError; }   // exceeds buffer
	UINT nRead = 0;
	f_read (&File, pBuffer, (UINT) nSize, &nRead);
	f_close (&File);

	*pLength = (unsigned) nRead;
	*ppContentType = "image/heic";
	return HTTPOK;
}

// POST /jpg?i=N — the phone-converted JPEG (multipart) is written next to file N as "<base>.jpg".
THTTPStatus CWebServer::SaveJpg (const char *pParams, u8 *pBuffer, unsigned *pLength,
				const char **ppContentType)
{
	int i = paramInt (pParams, "i", -1);
	if (m_pPhotos == 0 || i < 0) return HTTPNotFound;
	const char *pSrc = m_pPhotos->path ((unsigned) i);
	if (pSrc[0] == '\0') return HTTPNotFound;

	// Target = source path with the extension replaced by "jpg".
	char tgt[80];
	unsigned k = 0, dot = 0;
	for (; pSrc[k] && k + 5 < sizeof tgt; k++) { tgt[k] = pSrc[k]; if (pSrc[k] == '.') dot = k; }
	if (dot == 0) dot = k;              // no extension -> append
	tgt[dot] = '.'; tgt[dot + 1] = 'j'; tgt[dot + 2] = 'p'; tgt[dot + 3] = 'g'; tgt[dot + 4] = '\0';

	// Pull the uploaded file from the multipart body.
	const char *pHdr; const u8 *pData; unsigned nLen = 0;
	if (!GetMultipartFormPart (&pHdr, &pData, &nLen) || nLen == 0) return HTTPBadRequest;

	FIL File;
	if (f_open (&File, tgt, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return HTTPInternalServerError;
	UINT nWr = 0;
	f_write (&File, pData, nLen, &nWr);
	f_sync (&File);
	f_close (&File);
	if (nWr != nLen) return HTTPInternalServerError;

	g_rescanRequested = true;   // frame re-scans -> the photo resolves into the slideshow

	const char ok[] = "{\"ok\":1}";
	unsigned ol = sizeof ok - 1;
	memcpy (pBuffer, ok, ol);
	*pLength = ol;
	*ppContentType = "application/json";
	return HTTPOK;
}

// Inline conversion JS: preview each HEIC via native <img> decode, convert to JPEG on tap.
static const char s_ConvJs[] =
"<script>"
"function q(c,s){return c.querySelector(s)}"
"function conv(b){var c=b.closest('.pc'),i=c.dataset.i,im=q(c,'img');c._stop=false;"
"q(c,'.ov').innerHTML='<div class=pb><i></i></div><button class=stop onclick=\"stopc(this)\">Stop</button>';"
"var cv=document.createElement('canvas'),w=im.naturalWidth,h=im.naturalHeight,"
"s=Math.min(1,1920/Math.max(w,h));cv.width=(w*s)|0;cv.height=(h*s)|0;"
"cv.getContext('2d').drawImage(im,0,0,cv.width,cv.height);"
"cv.toBlob(function(bl){if(c._stop||!bl)return;var f=new FormData();f.append('f',bl,'p.jpg');"
"var x=new XMLHttpRequest();x.upload.onprogress=function(e){if(e.lengthComputable)"
"q(c,'.pb>i').style.width=(e.loaded/e.total*100)+'%'};"
"x.onload=function(){q(c,'.ov').innerHTML='<div class=done>Converted \\u2713</div>'};"
"x.onerror=function(){q(c,'.ov').innerHTML='<button onclick=\"conv(this)\">Retry</button>'};"
"x.open('POST','/jpg?i='+i);x.send(f)},'image/jpeg',0.85)}"
"function stopc(b){var c=b.closest('.pc');c._stop=true;"
"q(c,'.ov').innerHTML='<button onclick=\"conv(this)\">Convert</button>'}"
"function noprev(im){var c=im.closest('.pc');"
"q(c,'.ov').innerHTML='<div class=cap>Preview needs iPhone Safari</div>'}"
"</script>";

THTTPStatus CWebServer::GetContent (const char *pPath, const char *pParams, const char *pFormData,
				    u8 *pBuffer, unsigned *pLength, const char **ppContentType)
{
	// Binary/API endpoints first.
	if (pathHas (pPath, "heic")) return ServeHeic (pParams, pBuffer, pLength, ppContentType);
	if (pathHas (pPath, "jpg"))  return SaveJpg (pParams, pBuffer, pLength, ppContentType);

	CString Page (s_Head);
	const char *pName = m_pConfig ? m_pConfig->GetStr ("name", "LUMEN FRAME") : "LUMEN FRAME";

	if (pathHas (pPath, "restart"))
	{
		g_restartRequested = true;
		Page.Append ("<h1>"); appendEsc (Page, pName);
		Page.Append ("</h1><p class=\"ok\">&#10003; Restarting...</p>");
		Page.Append ("<p class=\"n\">The frame is restarting to apply your settings.</p>");
	}
	else if (pFormData != 0 && pFormData[0] != '\0' && m_pConfig != 0)
	{
		ApplyForm (pFormData);
		Page.Append ("<h1>"); appendEsc (Page, m_pConfig->GetStr ("name", "LUMEN FRAME"));
		Page.Append ("</h1><p class=\"ok\">&#10003; Saved</p>");
		Page.Append ("<form method=\"post\" action=\"/restart\"><button class=\"r\">Restart now</button></form>");
		Page.Append ("<p class=\"n\"><a href=\"/\">Back</a></p>");
	}
	else
	{
		Page.Append ("<h1>"); appendEsc (Page, pName);
		Page.Append ("</h1><p class=\"s\">Frame settings</p>");

		// Conversion section: one card per pending (needs-convert) photo.
		unsigned nPending = 0;
		if (m_pPhotos != 0)
			for (unsigned i = 0; i < m_pPhotos->count (); i++)
				if (m_pPhotos->needs_convert (i)) nPending++;

		if (nPending > 0)
		{
			CString H2; H2.Format ("<h2>Photos to convert (%u)</h2>", nPending);
			Page.Append ((const char *) H2);
			for (unsigned i = 0; i < m_pPhotos->count (); i++)
			{
				if (!m_pPhotos->needs_convert (i)) continue;
				CString Card;
				Card.Format ("<div class=\"pc\" data-i=\"%u\"><img src=\"/heic?i=%u\" onerror=\"noprev(this)\">"
					     "<div class=\"ov\"><button onclick=\"conv(this)\">Convert</button></div></div>", i, i);
				Page.Append ((const char *) Card);
				Page.Append ("<div class=\"nm\">");
				appendEsc (Page, m_pPhotos->name (i));
				Page.Append ("</div>");
			}
			Page.Append (s_ConvJs);
		}

		// Settings form.
		Page.Append ("<h2>Settings</h2><form method=\"post\">");
		field (Page, m_pConfig, "Display name",      "name",    "LUMEN FRAME");
		field (Page, m_pConfig, "Tagline",           "tagline", "Memory Lane Walkthrough");
		field (Page, m_pConfig, "Wi-Fi name (SSID)", "ssid",    "LumenFrame");
		field (Page, m_pConfig, "Credits",           "credits", "");
		Page.Append ("<button>Save</button></form>");
		Page.Append ("<button class=\"r\" disabled>Restart (save first)</button>");
		Page.Append ("<p class=\"n\">Settings apply after you restart the frame.</p>");
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
