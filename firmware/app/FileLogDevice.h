// CFileLogDevice — a CDevice that appends log output to a file on FatFs and f_sync()s after
// every write, so the log survives a subsequent hang/halt/panic. Used as the CLogger target
// so both our messages and Circle's internal panic/exception dumps land on the SD card.
#ifndef _filelogdevice_h
#define _filelogdevice_h

#include <circle/device.h>
#include <circle/types.h>
#include <fatfs/ff.h>

class CFileLogDevice : public CDevice
{
public:
    CFileLogDevice (void) : m_bOpen (FALSE) {}

    boolean Open (const char *pPath)
    {
        if (f_open (&m_File, pPath, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        {
            return FALSE;
        }
        m_bOpen = TRUE;
        return TRUE;
    }

    int Write (const void *pBuffer, size_t nCount) override
    {
        if (!m_bOpen)
        {
            return -1;
        }
        UINT nWritten = 0;
        f_write (&m_File, pBuffer, (UINT) nCount, &nWritten);
        f_sync (&m_File);   // flush now so a later halt/panic still leaves the log on disk
        return (int) nWritten;
    }

private:
    FIL      m_File;
    boolean  m_bOpen;
};

#endif
