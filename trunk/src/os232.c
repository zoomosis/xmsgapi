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

#if defined(_MSC_VER) || defined(__WATCOMC__)
#include <direct.h>
#endif

#if defined(__BORLANDC__)
#include <dir.h>
#endif

#include "compiler.h"
#include "osdep.h"
#include "prog.h"

#ifndef S_ISDIR
#define S_ISDIR(m) ((m) & S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(m) ((m) & S_IFREG)
#endif

#ifndef mymkdir
#ifdef __EMX__
#define mymkdir(a) mkdir((a), 0)
#else
#define mymkdir(a) mkdir((a))
#endif
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

FFIND * FFindOpen(char *filespec, unsigned short attribute)
{
    FFIND *ff;
    ULONG SearchCount = 1;
    FILEFINDBUF3 findbuf;

    ff = malloc(sizeof *ff);

    if (ff == NULL)
    {
        return NULL;
    }

    ff->hdir = HDIR_CREATE;

    if (!DosFindFirst((PBYTE) filespec, &ff->hdir, attribute, &findbuf, sizeof(findbuf), &SearchCount, 1L))
    {
        ff->ff_attrib = (char)findbuf.attrFile;
        ff->ff_fsize = findbuf.cbFile;
        ff->ff_ftime = *((USHORT *) &findbuf.ftimeLastWrite);
        ff->ff_fdate = *((USHORT *) &findbuf.fdateLastWrite);

        strncpy(ff->ff_name, findbuf.achName, sizeof(ff->ff_name));
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
    ULONG SearchCount = 1;
    FILEFINDBUF3 findbuf;

    if (ff == NULL)
    {
        return rc;
    }
    
    if (ff->hdir && !DosFindNext(ff->hdir, &findbuf, sizeof(findbuf), &SearchCount))
    {
        ff->ff_attrib = (char)findbuf.attrFile;
        ff->ff_ftime = *((USHORT *) & findbuf.ftimeLastWrite);
        ff->ff_fdate = *((USHORT *) & findbuf.fdateLastWrite);
        ff->ff_fsize = findbuf.cbFile;

        strncpy(ff->ff_name, findbuf.achName, sizeof(ff->ff_name));
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

    if (ff->hdir)
    {
        DosFindClose(ff->hdir);
    }

    free(ff);
}

int lock(int handle, long ofs, long length)
{
    FILELOCK urange, lrange;

    lrange.lOffset = ofs;
    lrange.lRange = length;
    urange.lRange = urange.lOffset = 0;

    if (DosSetFileLocks((HFILE)handle, &urange, &lrange, 0, 0) != 0)
    {
        return -1;
    }

    return 0;
}

int unlock(int handle, long ofs, long length)
{
    FILELOCK urange, lrange;

    urange.lOffset = ofs;
    urange.lRange = length;
    lrange.lRange = lrange.lOffset = 0;

    if (DosSetFileLocks((HFILE)handle, &urange, &lrange, 0, 0) != 0)
    {
        return -1;
    }

    return 0;
}

int waitlock(int handle, long ofs, long length)
{
    FILELOCK urange, lrange;

    lrange.lOffset = ofs;
    lrange.lRange = length;
    urange.lRange = urange.lOffset = 0;

    while (DosSetFileLocks((HFILE)handle, &urange, &lrange, 60000, 0) != 0)
    {
        /* do nothing */
    }

    return 0;
}

int waitlock2(int handle, long ofs, long length, long t)
{
    FILELOCK urange, lrange;

    lrange.lOffset = ofs;
    lrange.lRange = length;
    urange.lRange = urange.lOffset = 0;

    return DosSetFileLocks((HFILE) handle, &urange, &lrange, t * 1000L, 0);
}

void tdelay(int msecs)
{
    DosSleep((ULONG) msecs);
}

void flush_handle2(int fd)
{
    DosResetBuffer((HFILE) fd);
}

void flush_handle(FILE * fp)
{
    fflush(fp);
    flush_handle2(fileno(fp));
}
