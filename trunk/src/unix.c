#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "compiler.h"
#include "prog.h"
#include "osdep.h"
#include "patmat.h"
#include "unused.h"

#define mymkdir(a) mkdir((a), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

#ifndef S_ISDIR
#define S_ISDIR(m) ((m) & S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(m)  ((m) & S_IFREG)
#endif

#ifdef __BEOS__
#include <be/kernel/scheduler.h>
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

    rc = stat(directory, &s);

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
    char *p;
    int fin = 0;
    struct dirent *de;

    unused(attribute);

    ff = malloc(sizeof(FFIND));
	
    if (ff == NULL)
    {
    	return NULL;
    }

    p = strrchr(filespec, '/');

    if (p == NULL)
    {
        strcpy(ff->firstbit, ".");
        strcpy(ff->lastbit, filespec);
    }
    else if (p == filespec)
    {
        strcpy(ff->firstbit, "/");
        strcpy(ff->lastbit, filespec+1);
    }
    else
    {
        memcpy(ff->firstbit, filespec, p - filespec);
        ff->firstbit[p - filespec] = '\0';
        strcpy(ff->lastbit, p + 1);
   }

    ff->dir = opendir(ff->firstbit);

    if (ff->dir != NULL)
    {
        while (!fin)
        {
            de = readdir(ff->dir);

            if (de == NULL)
            {
                closedir(ff->dir);
                free(ff);
                ff = NULL;
                fin = 1;
            }
            else
            {
                if (patmat(de->d_name, ff->lastbit))
                {
                    strncpy(ff->ff_name, de->d_name, sizeof ff->ff_name);

                    /* All who wants to know it's size must read it by himself */
                    ff->ff_fsize = -1L;  

                    fin = 1;
                }
            }
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
    int fin = 0;
    struct dirent *de;

    while (!fin)
    {
        de = readdir(ff->dir);

        if (de == NULL)
        {
            closedir(ff->dir);
            ff->dir = NULL;
            fin = 1;
        }
        else
        {
            if (patmat(de->d_name, ff->lastbit))
            {
                strncpy(ff->ff_name, de->d_name, sizeof ff->ff_name);

                /* All who wants to know it's size must read it by himself */
                ff->ff_fsize = -1L;  

                fin = 1;
                rc = 0;
            }
        }
    }

    return rc;
}

void FFindClose(FFIND * ff)
{
    if (ff == NULL)
    {
        return;
    }

    if (ff->dir)
    {
        closedir(ff->dir);
    }

    free(ff);
}

void flush_handle2(int fd)
{
    unused(fd);
}

void flush_handle(FILE * fp)
{
    int nfd;
    fflush(fp);
    nfd = dup(fileno(fp));
    if (nfd != -1)
    {
        close(nfd);
    }
}

#ifndef __BEOS__

static struct flock *file_lock(short type, long ofs, long length, struct flock *ret)
{
    ret->l_type = type;
    ret->l_start = ofs;
    ret->l_whence = SEEK_SET;
    ret->l_len = length;
    ret->l_pid = getpid();
    return ret;
}

#endif

int lock(int handle, long ofs, long length)
{
#ifndef __BEOS__
    struct flock fl;
    return fcntl(handle, F_SETLK, file_lock(F_WRLCK, ofs, length, &fl));
#else
    unused(handle);
    unused(ofs);
    unused(length);
    return 0;
#endif   
}

int waitlock(int handle, long ofs, long length)
{
#ifndef __BEOS__
    struct flock fl;
    return fcntl(handle, F_SETLKW, file_lock(F_WRLCK, ofs, length, &fl));
#else
    unused(handle);
    unused(ofs);
    unused(length);
    return 0;
#endif
}

/*
 * waitlock2() wait <t> seconds for a lock
 */

int waitlock2(int handle, long ofs, long length, long t)
{
#ifndef __BEOS__
    int rc;
    struct flock fl;
	
    alarm(t);
    rc = fcntl(handle, F_SETLKW, file_lock(F_WRLCK, ofs, length, &fl));
    alarm(0);

    return rc;
#else
    unused(handle);
    unused(ofs);
    unused(length);
    unused(t);
    return 0;
#endif
}

int unlock(int handle, long ofs, long length)
{
#ifndef __BEOS__
    struct flock fl;
    return fcntl(handle, F_SETLK, file_lock(F_UNLCK, ofs, length, &fl));
#else
    unused(handle);
    unused(ofs);
    unused(length);
    return 0;
#endif
}

#ifdef __BEOS__

void tdelay(int msecs)
{
    snooze(msecs * 1000L);
}

#else

void tdelay(int msecs)
{
    usleep(msecs * 1000L);
}

#endif

