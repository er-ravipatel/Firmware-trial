// CSdPhotoSource — IPhotoSource backed by the SD/USB card via FatFs. Scans a directory tree for
// displayable photos (JPEG) and "needs-convert" files (HEIC/HEIF), classifying by extension.
//
// Sized for large libraries (a pendrive with 10k+ photos): the path table and the file buffer are
// heap-allocated ONCE on first use and never freed (Circle's heap leaks freed blocks > 512 KB, and
// CKernel itself lives on the 128 KB kernel stack, so neither table can be an inline member array).
#ifndef _sdphotosource_h
#define _sdphotosource_h

#include <fatfs/ff.h>
#include <circle/alloc.h>
#include "content/IPhotoSource.h"   // via EXTRAINCLUDE=-I../src

class CSdPhotoSource : public lf::IPhotoSource
{
public:
    static const unsigned kMaxPhotos  = 12000;              // hard cap on indexed files
    static const unsigned kMaxPath    = 128;                // drive + folders + LFN file name
    static const unsigned kMaxDepth   = 4;                  // sub-folder recursion limit
    static const unsigned kFileBufMax = 32u * 1024 * 1024;  // largest single photo we will load

    CSdPhotoSource (void) {}
    ~CSdPhotoSource (void) {}   // tables are intentionally never freed (see header comment)

    // Scan a directory tree (after f_mount). Picks up JPEG (displayable) + HEIC/HEIF (needs
    // convert), recursing into sub-folders up to kMaxDepth. Stops silently at kMaxPhotos.
    void Scan (const char *pDir = "SD:/")
    {
        m_nCount = 0;
        m_bTruncated = false;
        if (!EnsureTables ()) return;
        ScanDir (pDir, 0);
        DedupConverted ();   // hide any HEIC whose converted .jpg twin is now present
    }

    unsigned count (void) const override { return m_nCount; }
    bool     truncated (void) const { return m_bTruncated; }   // hit kMaxPhotos during the scan
    unsigned convert_count (void) const
    {
        unsigned n = 0;
        for (unsigned i = 0; i < m_nCount; i++) if (m_pKind[i] == 1) n++;
        return n;
    }

    bool needs_convert (unsigned nIndex) const override
    {
        return nIndex < m_nCount && m_pKind[nIndex] == 1;
    }

    const char *name (unsigned nIndex) const override
    {
        return nIndex < m_nCount ? Path (nIndex) + m_pNameOff[nIndex] : "";
    }

    // Full drive path of a file (for serving/writing back over the web). "" if out of range.
    const char *path (unsigned nIndex) const override
    {
        return nIndex < m_nCount ? Path (nIndex) : "";
    }

    // Read the whole file into the (single, reused) file buffer. Files larger than kFileBufMax
    // are skipped (nullptr) rather than grown into — no per-photo malloc/free ever.
    const uint8_t *jpeg (unsigned nIndex, unsigned &nLen) override
    {
        if (nIndex >= m_nCount) return nullptr;
        if (m_pFileBuf == nullptr)
        {
            m_pFileBuf = (uint8_t *) malloc (kFileBufMax);
            if (m_pFileBuf == nullptr) return nullptr;
        }

        FIL File;
        if (f_open (&File, Path (nIndex), FA_READ | FA_OPEN_EXISTING) != FR_OK)
        {
            return nullptr;
        }
        FSIZE_t nSize = f_size (&File);
        if (nSize == 0 || nSize > kFileBufMax)
        {
            f_close (&File);
            return nullptr;
        }
        UINT nRead = 0;
        FRESULT Result = f_read (&File, m_pFileBuf, (UINT) nSize, &nRead);
        f_close (&File);
        if (Result != FR_OK || nRead != nSize)
        {
            return nullptr;
        }
        nLen = (unsigned) nSize;
        return m_pFileBuf;
    }

private:
    char       *Path (unsigned nIndex)       { return m_pPaths + (unsigned long) nIndex * kMaxPath; }
    const char *Path (unsigned nIndex) const { return m_pPaths + (unsigned long) nIndex * kMaxPath; }

    // Allocate the index tables once (~1.5 MB for 12k entries). Never freed.
    bool EnsureTables (void)
    {
        if (m_pPaths != nullptr) return true;
        m_pPaths   = (char *) malloc ((unsigned long) kMaxPhotos * kMaxPath);
        m_pKind    = (unsigned char *) malloc (kMaxPhotos);
        m_pNameOff = (unsigned char *) malloc (kMaxPhotos);
        return m_pPaths && m_pKind && m_pNameOff;
    }

    // Append one entry "<pDir>/<pName>" of kind nKind. Returns false when the table is full.
    bool Add (const char *pDir, const char *pName, int nKind)
    {
        if (m_nCount >= kMaxPhotos) { m_bTruncated = true; return false; }
        char *dst = Path (m_nCount);
        unsigned k = 0;
        for (const char *p = pDir; *p && k < kMaxPath - 1; ++p) dst[k++] = *p;
        if (k > 0 && dst[k - 1] != '/' && k < kMaxPath - 1) dst[k++] = '/';
        unsigned nameOff = k;
        for (const char *p = pName; *p && k < kMaxPath - 1; ++p) dst[k++] = *p;
        dst[k] = '\0';
        if (nameOff > 255) return true;     // path too long to index; skip this entry
        if (nameOff + StrLen (pName) >= kMaxPath) return true;   // name got truncated: f_open would fail
        m_pNameOff[m_nCount] = (unsigned char) nameOff;
        m_pKind[m_nCount] = (unsigned char) nKind;
        m_nCount++;
        return true;
    }

    // Walk one directory: files first (so the top folder's photos come before sub-folders'),
    // then recurse into sub-folders. A DIR + FILINFO pair costs ~1 KB of stack per level.
    void ScanDir (const char *pDir, unsigned nDepth)
    {
        DIR Dir;
        FILINFO Info;
        if (f_opendir (&Dir, pDir) != FR_OK) return;
        while (f_readdir (&Dir, &Info) == FR_OK && Info.fname[0] != '\0')
        {
            if (Info.fattrib & (AM_HID | AM_SYS)) continue;
            if (Info.fattrib & AM_DIR) continue;   // second pass below
            int nKind = Classify (Info.fname);     // -1 skip, 0 displayable, 1 needs-convert
            if (nKind >= 0 && !Add (pDir, Info.fname, nKind)) break;
        }
        f_closedir (&Dir);

        if (nDepth + 1 >= kMaxDepth || m_bTruncated) return;
        if (f_opendir (&Dir, pDir) != FR_OK) return;
        while (f_readdir (&Dir, &Info) == FR_OK && Info.fname[0] != '\0')
        {
            if (!(Info.fattrib & AM_DIR) || (Info.fattrib & (AM_HID | AM_SYS))) continue;
            if (Info.fname[0] == '.') continue;    // ".", "..", ".Trashes", "._x" etc.
            if (ExtEq (Info.fname, "System Volume Information")) continue;
            char sub[kMaxPath];
            unsigned k = 0;
            for (const char *p = pDir; *p && k < kMaxPath - 1; ++p) sub[k++] = *p;
            if (k > 0 && sub[k - 1] != '/' && k < kMaxPath - 1) sub[k++] = '/';
            for (const char *p = Info.fname; *p && k < kMaxPath - 1; ++p) sub[k++] = *p;
            sub[k] = '\0';
            ScanDir (sub, nDepth + 1);
            if (m_bTruncated) break;
        }
        f_closedir (&Dir);
    }

    static unsigned StrLen (const char *p) { unsigned n = 0; while (p[n]) n++; return n; }

    // Classify by extension (case-insensitive). -1 = ignore, 0 = displayable, 1 = needs convert.
    static int Classify (const char *pName)
    {
        const char *pExt = nullptr;
        for (const char *p = pName; *p; ++p) if (*p == '.') pExt = p + 1;
        if (pExt == nullptr) return -1;
        if (ExtEq (pExt, "jpg") || ExtEq (pExt, "jpeg")) return 0;
        if (ExtEq (pExt, "heic") || ExtEq (pExt, "heif")) return 1;
        return -1;   // (png/gif/bmp become 0 once stb decoders are enabled)
    }

    static bool ExtEq (const char *a, const char *b)
    {
        while (*a && *b)
        {
            char ca = (*a >= 'A' && *a <= 'Z') ? (char) (*a + 32) : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? (char) (*b + 32) : *b;
            if (ca != cb) return false;
            a++; b++;
        }
        return *a == '\0' && *b == '\0';
    }

    // Length of a filename up to (not including) its last '.'.
    static unsigned BaseLen (const char *pName)
    {
        unsigned len = 0, dot = 0;
        for (const char *p = pName; *p; ++p) { if (*p == '.') dot = len; len++; }
        return dot ? dot : len;
    }

    // Do two entries share the same folder and base name (case-insensitive, ignoring extension)?
    bool SameBase (unsigned a, unsigned b) const
    {
        if (m_pNameOff[a] != m_pNameOff[b]) return false;
        const char *pa = Path (a), *pb = Path (b);
        unsigned la = BaseLen (pa + m_pNameOff[a]), lb = BaseLen (pb + m_pNameOff[b]);
        if (la != lb) return false;
        unsigned n = m_pNameOff[a] + la;   // compare folder prefix + base name in one go
        for (unsigned i = 0; i < n; ++i)
        {
            char ca = (pa[i] >= 'A' && pa[i] <= 'Z') ? (char) (pa[i] + 32) : pa[i];
            char cb = (pb[i] >= 'A' && pb[i] <= 'Z') ? (char) (pb[i] + 32) : pb[i];
            if (ca != cb) return false;
        }
        return true;
    }

    // Drop any needs-convert file (kind 1) that already has a displayable twin (kind 0) — i.e. it
    // has been converted — so it no longer shows a QR slide. Mark, then compact in one pass.
    void DedupConverted (void)
    {
        unsigned nWrite = 0;
        for (unsigned a = 0; a < m_nCount; ++a)
        {
            bool drop = false;
            if (m_pKind[a] == 1)
                for (unsigned b = 0; b < m_nCount; ++b)
                    if (b != a && m_pKind[b] == 0 && SameBase (a, b)) { drop = true; break; }
            if (drop) continue;
            if (nWrite != a)
            {
                char *d = Path (nWrite); const char *s = Path (a);
                for (unsigned c = 0; c < kMaxPath; ++c) d[c] = s[c];
                m_pKind[nWrite] = m_pKind[a];
                m_pNameOff[nWrite] = m_pNameOff[a];
            }
            nWrite++;
        }
        m_nCount = nWrite;
    }

    char          *m_pPaths   = nullptr;   // kMaxPhotos * kMaxPath, allocated once
    unsigned char *m_pKind    = nullptr;   // 0 = displayable, 1 = needs convert
    unsigned char *m_pNameOff = nullptr;   // offset of the filename within the path
    unsigned       m_nCount = 0;
    bool           m_bTruncated = false;
    uint8_t       *m_pFileBuf = nullptr;   // kFileBufMax, allocated once, reused per photo
};

#endif
