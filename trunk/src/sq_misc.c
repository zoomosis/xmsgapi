/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 */

#ifndef MSGAPI_NO_SQUISH

#define MSGAPI_HANDLERS
#define MSGAPI_NO_OLD_TYPES

#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "prog.h"
#include "old_msg.h"
#include "msgapi.h"
#include "api_sq.h"
#include "api_sqp.h"

#ifndef __UNIX__
#include <io.h>
#include <share.h>
#endif

/* Set the "current position" pointer in a message handle */

sword apiSquishSetCurPos(HMSG hmsg, dword dwOfs)
{
    if (MsgInvalidHmsg(hmsg) || !_SquishReadMode(hmsg))
    {
        return -1;
    }

    hmsg->cur_pos = dwOfs;
    return 0;
}

/* Return the current read position within a message */

dword apiSquishGetCurPos(HMSG hmsg)
{
    if (MsgInvalidHmsg(hmsg) || !_SquishReadMode(hmsg))
    {
        return (dword) -1;
    }

    return hmsg->cur_pos;
}

/* Return the length of the text body of this message */

dword apiSquishGetTextLen(HMSG hmsg)
{
    if (MsgInvalidHmsg(hmsg) || !_SquishReadMode(hmsg))
    {
        return (dword) -1;
    }

    return hmsg->sqhRead.msg_length - XMSG_SIZE - hmsg->sqhRead.clen;
}

/* Return the length of this message's control information */

dword apiSquishGetCtrlLen(HMSG hmsg)
{
    if (MsgInvalidHmsg(hmsg) || !_SquishReadMode(hmsg))
    {
        return (dword) -1;
    }

    return hmsg->sqhRead.clen;
}

/* Return the number of the high water marker */

dword apiSquishGetHighWater(HAREA ha)
{
    if (MsgInvalidHarea(ha))
    {
        return (dword) -1;
    }

    return apiSquishUidToMsgn(ha, ha->high_water, UID_PREV);
}

/* Set the high water marker for this area */

sword apiSquishSetHighWater(HAREA ha, dword dwMsg)
{
    if (MsgInvalidHarea(ha))
    {
        return -1;
    }

    /* Make sure that the message exists */

    if (dwMsg > ha->num_msg)
    {
        msgapierr = MERR_NOENT;
        return -1;
    }

    if (!_SquishExclusiveBegin(ha))
    {
        return -1;
    }

    ha->high_water = apiSquishMsgnToUid(ha, dwMsg);

    if (!_SquishExclusiveEnd(ha))
    {
        return -1;
    }

    return 0;
}

/* Function to set the highest/skip message numbers for a *.SQ? base */

void apiSquishSetMaxMsg(HAREA ha, dword dwMaxMsgs, dword dwSkipMsgs, dword dwMaxDays)
{
    if (MsgInvalidHarea(ha))
    {
        return;
    }

    /* Update base only if max msg settings have changed */

    if ((dwMaxMsgs != (dword) -1 && dwMaxMsgs != Sqd->dwMaxMsg) ||
      (dwSkipMsgs != (dword) -1 && dwSkipMsgs != (dword) Sqd->wSkipMsg) ||
      (dwMaxDays != (dword) -1 && dwMaxDays != (dword) Sqd->wMaxDays))
    {
        if (!_SquishExclusiveBegin(ha))
        {
            return;
        }

        if (dwMaxMsgs != (dword) -1)
        {
            Sqd->dwMaxMsg = dwMaxMsgs;
        }

        if (dwSkipMsgs != (dword) -1)
        {
            Sqd->wSkipMsg = (word) dwSkipMsgs;
        }

        if (dwMaxDays != (dword) -1)
        {
            Sqd->wMaxDays = (word) dwMaxDays;
        }

        _SquishExclusiveEnd(ha);
    }
}

void apiSquishGetMaxMsg(HAREA ha, dword * dwMaxMsgs, dword * dwSkipMsgs, dword * dwMaxDays)
{
    if (MsgInvalidHarea(ha))
    {
        return;
    }

    if (dwMaxMsgs)
    {
        *dwMaxMsgs = Sqd->dwMaxMsg;
    }

    if (dwSkipMsgs)
    {
        *dwSkipMsgs = (dword) Sqd->wSkipMsg;
    }

    if (dwMaxDays)
    {
        *dwMaxDays = (dword) Sqd->wMaxDays;
    }
}

#endif
