/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 *
 *  compiler.h
 *
 *  Determines compiler and platform.
 */

#ifndef __UNIX__
#if defined(__CYGWIN__) || defined(__linux__) || defined(__FreeBSD__) || \
  defined (__BEOS__)
#define __UNIX__ 1
#endif
#endif

#ifndef __WIN32__
#if defined(__MINGW32__) || defined(__NT__) || defined(NT) || \
  defined(WIN32) || defined(_WIN32) || defined(_WINDOWS)
#ifndef __CYGWIN__
#define __WIN32__ 1
#endif
#endif
#endif

#ifndef __DOS__
#if defined(DOS) || defined(__MSDOS__) || defined(MSDOS)
  #define __DOS__ 1
#endif
#endif

#ifndef __OS2__
#if defined(OS2) || defined(OS_2)
#define __OS2__ 1
#endif
#endif

#if !defined(__UNIX__) && !defined(__WIN32__) && !defined(__DOS__) && !defined(__OS2__)
#error Only __UNIX__, __WIN32__, __DOS__ & __OS2__ supported.
#endif

#ifdef __UNIX__
#define SH_DENYNONE 0
#define sopen(a, b, c, d)  open((a), (b), (d))
#ifndef __sun__
#define tell(a)  lseek((a), 0, SEEK_CUR)
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef stricmp
#define stricmp strcasecmp
#endif
#define setfsize ftruncate
#endif

#if defined(__DOS__) || defined(__WIN32__) || defined(__OS2__)
#define strncasecmp strnicmp
#define setfsize chsize
#endif

#if defined(__UNIX__)
#define PATH_DELIM  '/'
#else
#define PATH_DELIM  '\\'
#endif

#ifdef PATHLEN
#undef PATHLEN
#endif

#define PATHLEN  120  /* Max. length of a path */

#ifdef SH_DENYNO
#ifndef SH_DENYNONE
#define SH_DENYNONE  SH_DENYNO
#endif
#endif
