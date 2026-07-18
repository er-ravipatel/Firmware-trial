//
// Config.cpp — see Config.h.
//
#include "Config.h"
#include <fatfs/ff.h>

// --- tiny local string helpers (freestanding; no libc str*) ---
static inline char cf_lc (char c) { return (c >= 'A' && c <= 'Z') ? (char) (c + 32) : c; }

static boolean cf_ieq (const char *a, const char *b)
{
	while (*a && *b) { if (cf_lc (*a) != cf_lc (*b)) return FALSE; a++; b++; }
	return *a == *b;
}

static void cf_copy (char *dst, const char *src, unsigned cap)
{
	unsigned i = 0;
	for (; src[i] && i + 1 < cap; i++) dst[i] = src[i];
	dst[i] = '\0';
}

CConfig::CConfig (void) : m_nCount (0)
{
}

int CConfig::Find (const char *pKey) const
{
	for (unsigned i = 0; i < m_nCount; i++)
	{
		if (cf_ieq (m_Key[i], pKey)) return (int) i;
	}
	return -1;
}

void CConfig::Load (const char *pPath)
{
	m_nCount = 0;

	FIL File;
	if (f_open (&File, pPath, FA_READ) != FR_OK) return;

	char Buf[1024];
	UINT nRead = 0;
	if (f_read (&File, Buf, sizeof (Buf) - 1, &nRead) != FR_OK) nRead = 0;
	f_close (&File);
	Buf[nRead] = '\0';

	const char *p = Buf;
	while (*p && m_nCount < kMaxKeys)
	{
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '#' || *p == '\r' || *p == '\n')       // comment / blank -> skip line
		{
			while (*p && *p != '\n') p++;
			if (*p) p++;
			continue;
		}

		// key
		char key[kKeyLen]; unsigned k = 0;
		while (*p && *p != '=' && *p != ' ' && *p != '\t' && *p != '\n' && k + 1 < kKeyLen)
			key[k++] = *p++;
		key[k] = '\0';

		while (*p == ' ' || *p == '\t') p++;
		if (*p != '=') { while (*p && *p != '\n') p++; if (*p) p++; continue; }
		p++;                                              // skip '='
		while (*p == ' ' || *p == '\t') p++;

		// value (to end of line, trimmed of trailing spaces / CR)
		char val[kValLen]; unsigned v = 0;
		while (*p && *p != '\n' && *p != '\r' && *p != '#' && v + 1 < kValLen)
			val[v++] = *p++;
		while (v > 0 && (val[v - 1] == ' ' || val[v - 1] == '\t')) v--;
		val[v] = '\0';

		while (*p && *p != '\n') p++;
		if (*p) p++;

		if (k > 0)
		{
			cf_copy (m_Key[m_nCount], key, kKeyLen);
			cf_copy (m_Val[m_nCount], val, kValLen);
			m_nCount++;
		}
	}
}

const char *CConfig::GetStr (const char *pKey, const char *pDefault) const
{
	int i = Find (pKey);
	return i < 0 ? pDefault : m_Val[i];
}

int CConfig::GetInt (const char *pKey, int nDefault) const
{
	int i = Find (pKey);
	if (i < 0) return nDefault;
	const char *s = m_Val[i];
	boolean neg = (*s == '-'); if (neg || *s == '+') s++;
	boolean any = FALSE; int n = 0;
	while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; any = TRUE; }
	if (!any) return nDefault;
	return neg ? -n : n;
}

boolean CConfig::GetBool (const char *pKey, boolean bDefault) const
{
	int i = Find (pKey);
	if (i < 0) return bDefault;
	const char *v = m_Val[i];
	if (cf_ieq (v, "on") || cf_ieq (v, "1") || cf_ieq (v, "true") || cf_ieq (v, "yes")) return TRUE;
	if (cf_ieq (v, "off") || cf_ieq (v, "0") || cf_ieq (v, "false") || cf_ieq (v, "no")) return FALSE;
	return bDefault;
}

void CConfig::Set (const char *pKey, const char *pValue)
{
	int i = Find (pKey);
	if (i < 0)
	{
		if (m_nCount >= kMaxKeys) return;
		i = (int) m_nCount++;
		cf_copy (m_Key[i], pKey, kKeyLen);
	}
	cf_copy (m_Val[i], pValue, kValLen);
}

boolean CConfig::Save (const char *pPath)
{
	FIL File;
	if (f_open (&File, pPath, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return FALSE;

	f_puts ("# Lumen Frame configuration (edited via the settings page)\n", &File);
	for (unsigned i = 0; i < m_nCount; i++)
	{
		char line[kKeyLen + kValLen + 8];
		unsigned o = 0;
		for (unsigned j = 0; m_Key[i][j] && o + 1 < sizeof line; j++) line[o++] = m_Key[i][j];
		if (o + 3 < sizeof line) { line[o++] = ' '; line[o++] = '='; line[o++] = ' '; }
		for (unsigned j = 0; m_Val[i][j] && o + 1 < sizeof line; j++) line[o++] = m_Val[i][j];
		if (o + 1 < sizeof line) line[o++] = '\n';
		line[o] = '\0';
		f_puts (line, &File);
	}

	f_sync (&File);
	f_close (&File);
	return TRUE;
}
