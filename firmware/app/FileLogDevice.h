// CFileLogDevice — a CDevice that appends log output to a file on FatFs and f_sync()s after
// every write, so the log survives a subsequent hang/halt/panic. Used as the CLogger target
// so both our messages and Circle's internal panic/exception dumps land on the SD card.
//
// History is preserved across boots: the file is opened in APPEND mode (each boot's run is
// separated by the "==== Lumen Frame boot ====" banner). To keep it from ever filling the card,
// if the log has grown past kMaxBytes at boot it is rolled over to a single ".old" backup and a
// fresh file is started — so on-disk usage is bounded to ~2x the cap while still keeping the
// previous run(s) for post-mortem.
#ifndef _filelogdevice_h
#define _filelogdevice_h

#include <circle/device.h>
#include <circle/types.h>
#include <circle/string.h>
#include <fatfs/ff.h>

class CFileLogDevice : public CDevice
{
public:
    CFileLogDevice (void) : m_bOpen (FALSE) {}

    boolean Open (const char *pPath)
    {
        // Roll over if the existing log is already large: keep it as "<path>.old" (one
        // generation), then start a fresh file. New content is appended otherwise.
        FILINFO fi;
        if (f_stat (pPath, &fi) == FR_OK && fi.fsize >= kMaxBytes)
        {
            CString OldFull, OldRel;
            OldFull.Format ("%s.old", pPath);               // full path (with drive) for f_unlink
            OldRel.Format ("%s.old", DriveRelative (pPath)); // drive-relative for f_rename's new name
            f_unlink ((const char *) OldFull);              // drop the previous .old (ignore errors)
            f_rename (pPath, (const char *) OldRel);        // current -> .old  (ignore errors)
        }

        // FA_OPEN_APPEND opens-or-creates and seeks to end, so writes extend the file.
        if (f_open (&m_File, pPath, FA_WRITE | FA_OPEN_APPEND) != FR_OK)
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
    // f_rename requires the new name WITHOUT a drive prefix (the drive comes from the old name).
    // Return the path portion after "<drive>:" e.g. "SD:/lumenlog.txt" -> "/lumenlog.txt".
    static const char *DriveRelative (const char *pPath)
    {
        for (const char *p = pPath; *p; ++p)
        {
            if (*p == ':')
            {
                return p + 1;
            }
        }
        return pPath;
    }

    static const unsigned kMaxBytes = 1u * 1024 * 1024;   // ~1 MB cap before rollover

    FIL      m_File;
    boolean  m_bOpen;
};

#endif
