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
        m_Path = pPath;

        // Roll over if the existing log is already large: keep it as "<path>.old" (one
        // generation), then start a fresh file. New content is appended otherwise.
        FILINFO fi;
        if (f_stat (pPath, &fi) == FR_OK && fi.fsize >= kMaxBytes)
        {
            RotateFiles ();
        }
        return OpenAppend ();
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

        // Mid-run rollover: a boot-time check alone let a 10 h run grow the log to 6 MB (field
        // log 2026-09-05). Check the size every kCheckEvery writes and rotate when past the cap.
        if (++m_nSinceCheck >= kCheckEvery)
        {
            m_nSinceCheck = 0;
            if (f_size (&m_File) >= kMaxBytes)
            {
                f_close (&m_File);
                m_bOpen = FALSE;
                RotateFiles ();
                OpenAppend ();   // on failure m_bOpen stays FALSE and logging stops quietly
            }
        }
        return (int) nWritten;
    }

private:
    void RotateFiles (void)
    {
        CString OldFull, OldRel;
        OldFull.Format ("%s.old", (const char *) m_Path);               // full path for f_unlink
        OldRel.Format ("%s.old", DriveRelative ((const char *) m_Path)); // drive-relative for f_rename
        f_unlink ((const char *) OldFull);              // drop the previous .old (ignore errors)
        f_rename ((const char *) m_Path, (const char *) OldRel);   // current -> .old (ignore errors)
    }

    boolean OpenAppend (void)
    {
        // FA_OPEN_APPEND opens-or-creates and seeks to end, so writes extend the file.
        if (f_open (&m_File, (const char *) m_Path, FA_WRITE | FA_OPEN_APPEND) != FR_OK)
        {
            return FALSE;
        }
        m_bOpen = TRUE;
        return TRUE;
    }

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

    static const unsigned kMaxBytes  = 1u * 1024 * 1024;   // ~1 MB cap before rollover
    static const unsigned kCheckEvery = 256;               // writes between mid-run size checks

    FIL      m_File;
    boolean  m_bOpen;
    CString  m_Path;
    unsigned m_nSinceCheck = 0;
};

#endif
