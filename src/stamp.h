/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Based on the Squish MsgAPI written by Scott Dudley.
 *
 *  This source code is free software and distributed under the GNU Lesser
 *  General Public License.  Please refer to the file named LICENCE for
 *  copyright information.
 */

#ifndef __STAMP_H__
#define __STAMP_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <time.h>

#include "typedefs.h"
#include "pack.h"

#ifdef _MSC_VER
#pragma warning(disable:4214)
#endif

/* DOS-style datestamp */

struct _stamp
{
    struct
    {
        bits da:5;
        bits mo:4;
        bits yr:7;
    }
    date;

    struct
    {
        bits ss:5;
        bits mm:6;
        bits hh:5;
    }
    time;
};

struct _dos_st
{
    word date;
    word time;
};

/* Union so we can access stamp as "int" or by individual components */

union stamp_combo
{
    dword ldate;
    struct _stamp msg_st;
    struct _dos_st dos_st;
};

typedef union stamp_combo SCOMBO;

#include "unpack.h"

void ASCII_Date_To_Binary(char *msgdate, union stamp_combo *d_written);
union stamp_combo *Get_Dos_Date(union stamp_combo *st);
struct tm *DosDate_to_TmDate(union stamp_combo *dosdate, struct tm *tmdate);
union stamp_combo *TmDate_to_DosDate(struct tm *tmdate, union stamp_combo *dosdate);
char *sc_time(union stamp_combo *sc, char *string);

#ifdef __cplusplus
}
#endif

#endif
