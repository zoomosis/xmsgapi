/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 */

#ifndef __OSDEP_H__
#define __OSDEP_H__

#include <stdio.h>

#include "compiler.h"

#ifdef __WIN32__
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMSG
#include <windows.h>
#endif

#ifdef __UNIX__
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#endif

#ifdef __DOS__
#ifdef DJGPP
#include <dir.h>
#else
#include <dos.h>
#endif
#endif

#ifdef __OS2__
#define INCL_NOPM
#define INCL_DOS

#include <os2.h>

#if defined(__HIGHC__)
#include <unistd.h>
#endif

#endif

typedef struct ffind
{
    /* public */

    char ff_attrib;
    unsigned short ff_ftime, ff_fdate;
    char ff_name[256];
    long ff_fsize;

    /* private */

#ifdef __WIN32__
    WIN32_FIND_DATA InfoBuf;
    HANDLE hDirA;
    char attrib_srch;
#endif

#ifdef __UNIX__
    DIR *dir;
    char firstbit[FILENAME_MAX];
    char lastbit[FILENAME_MAX];
#endif

#ifdef __DOS__
#ifdef DJGPP
    struct ffblk ffbuf;
#else
    struct find_t ffbuf;
    unsigned long hdir;
#endif
#endif

#ifdef __OS2__
    HDIR hdir;
#endif
}
FFIND;

/*
 *  FFindOpen; Use like MS-DOS "find first" function.  Be sure to
 *  release allocated system resources by caling FFindClose() with the
 *  handle returned by this function.
 *
 *  Returns: NULL == File not found.
 */

FFIND *FFindOpen(char *filespec, unsigned short attribute);

/* FFindNext: Returns 0 if next file was found, non-zero if it was not. */

int FFindNext(FFIND * ff);

/*
 *  FFindClose: End a directory search.  Failure to call this function
 *  will result in unclosed file handles and/or unreleased memory!
 */

void FFindClose(FFIND * ff);

#define MSDOS_READONLY  0x01
#define MSDOS_HIDDEN    0x02
#define MSDOS_SYSTEM    0x04
#define MSDOS_VOLUME    0x08
#define MSDOS_SUBDIR    0x10
#define MSDOS_ARCHIVE   0x20
#define MSDOS_RSVD1     0x40
#define MSDOS_RSVD2     0x80

int fexist(const char *filename);
long fsize(const char *filename);
int direxist(const char *directory);
int _createDirectoryTree(const char *pathName);
void flush_handle(FILE * fp);
void flush_handle2(int fd);
void tdelay(int msecs);

int waitlock(int handle, long ofs, long length);
int waitlock2(int handle, long ofs, long length, long t);

#ifndef __WATCOMC__
int lock(int handle, long ofs, long length);
int unlock(int handle, long ofs, long length);
#endif

#endif
