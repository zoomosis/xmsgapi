#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <io.h>

#ifdef __WATCOMC__
#include <dos.h>
#include <direct.h>
#endif

#ifdef DJGPP
#include <unistd.h>
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
#ifdef DJGPP
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

    ff = malloc(sizeof *ff);

    if (ff == NULL)
    {
        return NULL;
    }

#ifdef DJGPP
    if (findfirst(filespec, &(ff->ffbuf), attribute) != 0)
#else
    if (_dos_findfirst(filespec, attribute, &(ff->ffbuf)) != 0)
#endif
    {
        free(ff);
        return NULL;
    }

#ifdef DJGPP
    ff->ff_attrib = ff->ffbuf.ff_attrib;
    ff->ff_ftime = ff->ffbuf.ff_ftime;
    ff->ff_fdate = ff->ffbuf.ff_fdate;
    ff->ff_fsize = ff->ffbuf.ff_fsize;
    memcpy(ff->ff_name, ff->ffbuf.ff_name, sizeof ff->ff_name);
#else
    ff->ff_attrib = ff->ffbuf.attrib;
    ff->ff_ftime = ff->ffbuf.wr_time;
    ff->ff_fdate = ff->ffbuf.wr_date;
    ff->ff_fsize = ff->ffbuf.size;
    memcpy(ff->ff_name, ff->ffbuf.name, sizeof ff->ff_name);
#endif

    ff->ff_name[sizeof(ff->ff_name) - 1] = '\0';

    return ff;
}

int FFindNext(FFIND * ff)
{
    int rc = -1;

    if (ff == NULL)
    {
        return rc;
    }

#ifdef DJGPP
    rc = findnext(&(ff->ffbuf));

    ff->ff_attrib = ff->ffbuf.ff_attrib;
    ff->ff_ftime = ff->ffbuf.ff_ftime;
    ff->ff_fdate = ff->ffbuf.ff_fdate;
    ff->ff_fsize = ff->ffbuf.ff_fsize;
    memcpy(ff->ff_name, ff->ffbuf.ff_name, sizeof ff->ff_name);
#else    
    rc = _dos_findnext(&(ff->ffbuf));

    ff->ff_attrib = ff->ffbuf.attrib;
    ff->ff_ftime = ff->ffbuf.wr_time;
    ff->ff_fdate = ff->ffbuf.wr_date;
    ff->ff_fsize = ff->ffbuf.size;
    memcpy(ff->ff_name, ff->ffbuf.name, sizeof ff->ff_name);
#endif

    ff->ff_name[sizeof(ff->ff_name) - 1] = '\0';

    return rc;
}

void FFindClose(FFIND * ff)
{
    if (ff == NULL)
    {
        return;
    }
    
#ifndef DJGPP
    _dos_findclose(&(ff->ffbuf));
#endif

    free(ff);
}

void tdelay(int msecs)
{
#ifdef DJGPP
    usleep(msecs * 1000L);
#else
    clock_t ctEnd;

    ctEnd = clock() + (long) msecs * (long) CLK_TCK / 1000L;

    while (clock() < ctEnd)
    {
        /* do nothing */
    }
#endif
}

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
