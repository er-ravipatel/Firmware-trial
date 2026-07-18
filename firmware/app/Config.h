//
// Config.h — simple key=value configuration loaded from SD:/lumen.conf.
//
// Values default to code constants, are overridden by the file, and can be edited at runtime
// (Set + Save) — e.g. from the settings web page. One place for all user-facing settings so text
// and behaviour are config-driven, not hardcoded (the "white-label / personalize" requirement).
//
#ifndef _config_h
#define _config_h

#include <circle/types.h>

class CConfig
{
public:
	CConfig (void);

	// Parse pPath (e.g. "SD:/lumen.conf"). A missing file leaves everything at its default.
	void Load (const char *pPath);

	const char *GetStr  (const char *pKey, const char *pDefault) const;
	int         GetInt  (const char *pKey, int  nDefault) const;
	boolean     GetBool (const char *pKey, boolean bDefault) const;

	void        Set  (const char *pKey, const char *pValue);   // update or add (in memory)
	boolean     Save (const char *pPath);                      // rewrite the whole file

	unsigned    Count (void) const { return m_nCount; }

private:
	int Find (const char *pKey) const;

	static const unsigned kMaxKeys = 24;
	static const unsigned kKeyLen  = 24;
	static const unsigned kValLen  = 80;

	char     m_Key[kMaxKeys][kKeyLen];
	char     m_Val[kMaxKeys][kValLen];
	unsigned m_nCount;
};

#endif
