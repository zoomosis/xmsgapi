/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 *
 *  api_sdm.h
 *
 *  *.MSG (Star Dot Message) messagebase access routines.
 */

#ifndef __API_SDM_H__
#define __API_SDM_H__

#include "pack.h"

struct _msgh
{
    MSGA *sq;
    dword id;  /* Must always equal MSGH_ID */

    dword bytes_written;
    dword cur_pos;

    /* For *.msg only! */

    sdword clen;
    byte *ctrl;
    sdword msg_len;
    sdword msgtxt_start;
    word zplen;
    int fd;
};

/*
 *  This following junk is unique to *.msg!
 *  NO APPLICATIONS SHOULD USE THESE!
 */

struct _sdmdata
{
    byte base[80];

    unsigned *msgnum;           /* has to be of type 'int' for qksort() fn */
    word msgnum_len;

    dword hwm;
    word hwm_chgd;

    word msgs_open;
};

#include "unpack.h"

int WriteZPInfo(XMSG * msg, void (* wfunc) (byte * str), byte * kludges);

#endif
