/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Based on the Squish MsgAPI written by Scott Dudley.
 *
 *  This source code is free software and distributed under the GNU Lesser
 *  General Public License.  Please refer to the file named LICENCE for
 *  copyright information.
 */

#ifndef __TYPEDEFS_H__
#define __TYPEDEFS_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef unsigned int bit;
typedef unsigned char byte;
typedef signed char sbyte;
typedef unsigned short word;
typedef signed short sword;
typedef signed short sshort;
typedef signed long slong;

#ifdef __TURBOC__
typedef unsigned int bits;
#else
typedef word bits;
#endif

#ifdef __alpha
/* add other 64 bit systems here */
typedef unsigned int dword;
typedef signed int sdword;
#else
/* 32 and 16 bit machines */
typedef unsigned long dword;
typedef signed long sdword;
#endif

#ifndef __UNIX__
typedef unsigned short ushort;
typedef unsigned long ulong;
#endif

#ifdef __cplusplus
}
#endif

#endif
