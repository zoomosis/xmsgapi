/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>

#ifdef __WATCOMC__
#include <sys/locking.h>
#endif

#if defined(_MSC_VER) || defined(__WATCOMC__) || defined(__LCC__)
#include <direct.h>
#endif

#if defined(__TURBOC__)
#include <dir.h>
#endif

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMSG

#include <windows.h>

#include "compiler.h"
#include "osdep.h"
#include "prog.h"
#include "unused.h"

#ifndef S_ISDIR
#define S_ISDIR(m) ((m) & S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(m) ((m) & S_IFREG)
#endif

#ifndef mymkdir
#define mymkdir(a) mkdir((a))
#endif

/*
 *  This is the nice code that works on UNIX and every other decent
 *  platform.  It has been contributed by Alex S. Aganichev
 */

int fexist(const char *filename)
{
    struct stat s;

    if (stat(filename, &s))
    {
        return FALSE;
    }

    return S_ISREG(s.st_mode);
}

long fsize(const char *filename)
{
    struct stat s;

    if (stat(filename, &s))
    {
        return -1L;
    }

    return s.st_size;
}

int direxist(const char *directory)
{
    struct stat s;
    int rc;

#if !defined(__WATCOMC__) && !(defined(_MSC_VER) && (_MSC_VER >= 1200)) && !defined(__MINGW32__)
    rc = stat(directory, &s);
#else
    char *tempstr, *p;
    size_t l;

    tempstr = strdup(directory);

    if (tempstr == NULL)
    {
        return FALSE;
    }

    /* Root directory of any drive always exists! */

    if ((isalpha((int)tempstr[0]) && tempstr[1] == ':' &&
      (tempstr[2] == '\\' || tempstr[2] == '/') &&
      !tempstr[3]) || eqstr(tempstr, "\\"))
    {
        free(tempstr);
        return TRUE;
    }

    l = strlen(tempstr);

    if (tempstr[l - 1] == '\\' || tempstr[l - 1] == '/')
    {
        /* remove trailing backslash */
        tempstr[l - 1] = '\0';
    }

    for (p = tempstr; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\\';
        }
    }

    rc = stat(tempstr, &s);

    free(tempstr);
#endif

    if (rc)
    {
        return FALSE;
    }

    return S_ISDIR(s.st_mode);
}

int _createDirectoryTree(const char *pathName)
{
    char *start, *slash;
    char limiter = PATH_DELIM;
    int i;

    start = (char *)malloc(strlen(pathName) + 2);
    strcpy(start, pathName);

    i = strlen(start) - 1;

    if (start[i] != limiter)
    {
        start[i + 1] = limiter;
        start[i + 2] = '\0';
    }
    slash = start;

    /* if there is a drivename, jump over it */

    if (slash[1] == ':')
    {
        slash += 2;
    }

    /* jump over first limiter */
    slash++;

    while ((slash = strchr(slash, limiter)) != NULL)
    {
        *slash = '\0';

        if (!direxist(start))
        {
            if (!fexist(start))
            {
                /* this part of the path does not exist, create it */

                if (mymkdir(start) != 0)
                {
                    free(start);
                    return 1;
                }
            }
            else
            {
                free(start);
                return 1;
            }
        }

        *slash++ = limiter;
    }

    free(start);

    return 0;
}

FFIND *FFindOpen(char *filespec, unsigned short attribute)
{
    FFIND *ff;

    ff = malloc(sizeof *ff);

    if (ff == NULL)
    {
        return NULL;
    }

    ff->hDirA = FindFirstFile(filespec, &(ff->InfoBuf));
    ff->attrib_srch = (char)attribute;

    while (ff->hDirA != INVALID_HANDLE_VALUE)
    {
        if (strlen(ff->InfoBuf.cFileName) < sizeof(ff->ff_name))
        {
            if ((!(ff->InfoBuf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ||
              (ff->attrib_srch & MSDOS_SUBDIR))
            {
                break;
            }
        }

        /* skip file for some reason */

        if (!FindNextFile(ff->hDirA, &(ff->InfoBuf)))
        {
            if (ff->hDirA != INVALID_HANDLE_VALUE)
            {
                FindClose(ff->hDirA);
            }

            ff->hDirA = INVALID_HANDLE_VALUE;
        }
    }

    if (ff->hDirA != INVALID_HANDLE_VALUE)
    {
        strcpy(ff->ff_name, ff->InfoBuf.cFileName);

        ff->ff_fsize = ff->InfoBuf.nFileSizeLow;

        ff->ff_attrib = 0;

        if (ff->InfoBuf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            ff->ff_attrib |= MSDOS_SUBDIR;
        }
    }
    else
    {
        free(ff);
        ff = NULL;
    }

    return ff;
}

int FFindNext(FFIND * ff)
{
    int rc = -1;

    if (ff == NULL)
    {
        return rc;
    }

    do
    {
        if (!FindNextFile(ff->hDirA, &(ff->InfoBuf)))
        {
            if (ff->hDirA != INVALID_HANDLE_VALUE)
            {
                FindClose(ff->hDirA);
            }
            ff->hDirA = INVALID_HANDLE_VALUE;
        }
        else
        {

            if (strlen(ff->InfoBuf.cFileName) < sizeof(ff->ff_name))
            {
                if ((!(ff->InfoBuf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ||
                  (ff->attrib_srch & MSDOS_SUBDIR))
                {
                    break;
                }
            }
        }
    }
    while (ff->hDirA != INVALID_HANDLE_VALUE);

    if (ff->hDirA != INVALID_HANDLE_VALUE)
    {
        strcpy(ff->ff_name, ff->InfoBuf.cFileName);

        ff->ff_fsize = ff->InfoBuf.nFileSizeLow;

        ff->ff_attrib = 0;

        if (ff->InfoBuf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            ff->ff_attrib |= MSDOS_SUBDIR;
        }

        rc = 0;
    }

    return rc;
}

void FFindClose(FFIND * ff)
{
    if (ff == NULL)
    {
        return;
    }

    if (ff->hDirA != INVALID_HANDLE_VALUE)
    {
        FindClose(ff->hDirA);
    }

    free(ff);
}

void flush_handle2(int fd)
{
    FlushFileBuffers((HANDLE) fd);
}

void flush_handle(FILE * fp)
{
    fflush(fp);
    flush_handle2(fileno(fp));
}

void tdelay(int msecs)
{
    Sleep((DWORD) msecs);
}

#ifndef __WATCOMC__

int lock(int handle, long ofs, long length)
{
    long offset;
    int rc;

    offset = tell(handle);

    if (offset == -1)
    {
        return -1;
    }

    lseek(handle, ofs, SEEK_SET);

#ifdef __MINGW32__
    rc = _locking(handle, 2, length);
#else
    rc = locking(handle, 2, length);
#endif

    lseek(handle, offset, SEEK_SET);

    if (rc)
    {
        return -1;
    }

    return 0;
}

int unlock(int handle, long ofs, long length)
{
    long offset;
    int rc;

    offset = tell(handle);
    
    if (offset == -1)
    {
        return -1;
    }

    lseek(handle, ofs, SEEK_SET);

#ifdef __MINGW32__
    rc = _locking(handle, 0, length);
#else
    rc = locking(handle, 0, length);
#endif

    lseek(handle, offset, SEEK_SET);

    if (rc)
    {
        return -1;
    }

    return 0;
}

#endif

#ifdef __MINGW32__

int waitlock(int handle, long ofs, long length)
{
    long offset;

    offset = tell(handle);

    if (offset == -1)
    {
        return -1;
    }

    lseek(handle, ofs, SEEK_SET);
    _locking(handle, 1, length);
    lseek(handle, offset, SEEK_SET);

    return 0;
}

int waitlock2(int handle, long ofs, long length, long t)
{
/*
 *  For some reason _get_osfhandle(3) is returning -1 on my system.
 *  If it works on yours, #define FIXED_GET_OSFHANDLE and recompile!
 *
 *  - Andrew Clarke, 2003-01-15
 */

#if FIXED_GET_OSFHANDLE
    BOOL rc;
    int forever = 0;

    if (t == 0)
    {
        forever = 1;
    }

    t *= 10;

    while ((rc = LockFile((HANDLE) _get_osfhandle(handle), ofs, 0L,
      length, 0L)) == 0 && (t > 0 || forever))
    {
        tdelay(100);
        t--;
    }

    return rc;
#else
    unused(handle);
    unused(ofs);
    unused(length);
    unused(t);
    return 0;
#endif
}

#else

int waitlock(int handle, long ofs, long length)
{
    while (lock(handle, ofs, length) == -1)
    {
        tdelay(100);
    }
    return 0;
}

int waitlock2(int handle, long ofs, long length, long t)
{
    int rc, forever = 0;

    if (t == 0)
    {
        forever = 1;
    }

    t *= 10;

    while ((rc = lock(handle, ofs, length)) == -1 && (t > 0 || forever))
    {
        tdelay(100);
        t--;
    }

    return rc;
}

#endif

