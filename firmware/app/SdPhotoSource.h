// CSdPhotoSource — IPhotoSource backed by the SD card via FatFs. Scans the root for *.jpg
// and loads each on demand into a single reused buffer.
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

    // Scan the SD root directory for *.jpg files (call after f_mount).
    void Scan (const char *pDir = "SD:/")
    {
        m_nCount = 0;
        DIR Dir;
        FILINFO Info;
        FRESULT Result = f_findfirst (&Dir, &Info, pDir, "*.jpg");
        while (Result == FR_OK && Info.fname[0] != '\0' && m_nCount < kMaxPhotos)
        {
            if (!(Info.fattrib & (AM_DIR | AM_HID | AM_SYS)))
            {
                // Build "SD:/<name>".
                char *dst = m_Paths[m_nCount];
                unsigned k = 0;
                for (const char *p = pDir; *p && k < kMaxPath - 1; ++p) dst[k++] = *p;
                for (const char *p = Info.fname; *p && k < kMaxPath - 1; ++p) dst[k++] = *p;
                dst[k] = '\0';
                m_nCount++;
            }
            Result = f_findnext (&Dir, &Info);
        }
        f_closedir (&Dir);
    }

    unsigned count (void) const override { return m_nCount; }

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
    static const unsigned kMaxPhotos = 64;
    static const unsigned kMaxPath = 64;

    char m_Paths[kMaxPhotos][kMaxPath];
    unsigned m_nCount = 0;
    uint8_t *m_pBuffer = nullptr;
};

#endif
