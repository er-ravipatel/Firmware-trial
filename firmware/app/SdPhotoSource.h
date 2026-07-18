// CSdPhotoSource — IPhotoSource backed by the SD/USB card via FatFs. Scans a directory for
// displayable photos (JPEG) and "needs-convert" files (HEIC/HEIF), classifying by extension.
#ifndef _sdphotosource_h
#define _sdphotosource_h

#include <fatfs/ff.h>
#include <circle/alloc.h>
#include "content/IPhotoSource.h"   // via EXTRAINCLUDE=-I../src

class CSdPhotoSource : public lf::IPhotoSource
{
public:
    CSdPhotoSource (void) {}
    ~CSdPhotoSource (void) { if (m_pBuffer) free (m_pBuffer); }

    // Scan a directory (after f_mount). Picks up JPEG (displayable) + HEIC/HEIF (needs convert).
    void Scan (const char *pDir = "SD:/")
    {
        m_nCount = 0;
        DIR Dir;
        FILINFO Info;
        FRESULT Result = f_findfirst (&Dir, &Info, pDir, "*.*");
        while (Result == FR_OK && Info.fname[0] != '\0' && m_nCount < kMaxPhotos)
        {
            if (!(Info.fattrib & (AM_DIR | AM_HID | AM_SYS)))
            {
                int nKind = Classify (Info.fname);   // -1 skip, 0 displayable, 1 needs-convert
                if (nKind >= 0)
                {
                    char *dst = m_Paths[m_nCount];
                    unsigned k = 0;
                    for (const char *p = pDir; *p && k < kMaxPath - 1; ++p) dst[k++] = *p;
                    if (k > 0 && dst[k - 1] != '/' && k < kMaxPath - 1) dst[k++] = '/';
                    m_NameOff[m_nCount] = (unsigned char) k;   // where the filename starts
                    for (const char *p = Info.fname; *p && k < kMaxPath - 1; ++p) dst[k++] = *p;
                    dst[k] = '\0';
                    m_Kind[m_nCount] = (unsigned char) nKind;
                    m_nCount++;
                }
            }
            Result = f_findnext (&Dir, &Info);
        }
        f_closedir (&Dir);
    }

    unsigned count (void) const override { return m_nCount; }

    bool needs_convert (unsigned nIndex) const override
    {
        return nIndex < m_nCount && m_Kind[nIndex] == 1;
    }

    const char *name (unsigned nIndex) const override
    {
        return nIndex < m_nCount ? &m_Paths[nIndex][m_NameOff[nIndex]] : "";
    }

    // Full drive path of a file (for serving/writing back over the web). "" if out of range.
    const char *path (unsigned nIndex) const
    {
        return nIndex < m_nCount ? m_Paths[nIndex] : "";
    }

    const uint8_t *jpeg (unsigned nIndex, unsigned &nLen) override
    {
        if (nIndex >= m_nCount)
        {
            return nullptr;
        }
        if (m_pBuffer)
        {
            free (m_pBuffer);
            m_pBuffer = nullptr;
        }

        FIL File;
        if (f_open (&File, m_Paths[nIndex], FA_READ | FA_OPEN_EXISTING) != FR_OK)
        {
            return nullptr;
        }
        FSIZE_t nSize = f_size (&File);
        m_pBuffer = (uint8_t *) malloc (nSize);
        if (m_pBuffer == nullptr)
        {
            f_close (&File);
            return nullptr;
        }
        UINT nRead = 0;
        FRESULT Result = f_read (&File, m_pBuffer, (UINT) nSize, &nRead);
        f_close (&File);
        if (Result != FR_OK || nRead != nSize)
        {
            free (m_pBuffer);
            m_pBuffer = nullptr;
            return nullptr;
        }
        nLen = (unsigned) nSize;
        return m_pBuffer;
    }

private:
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

    static const unsigned kMaxPhotos = 64;
    static const unsigned kMaxPath = 64;

    char          m_Paths[kMaxPhotos][kMaxPath];
    unsigned char m_Kind[kMaxPhotos];      // 0 = displayable, 1 = needs convert
    unsigned char m_NameOff[kMaxPhotos];   // offset of the filename within the path
    unsigned      m_nCount = 0;
    uint8_t      *m_pBuffer = nullptr;
};

#endif
