/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 */

#ifndef MSGAPI_NO_SQUISH

#define MSGAPI_HANDLERS
#define MSGAPI_NO_OLD_TYPES

#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "prog.h"
#include "old_msg.h"
#include "msgapi.h"
#include "api_sq.h"
#include "api_sqp.h"

#ifndef __UNIX__
#include <io.h>
#include <share.h>
#endif

/*
 *  This function returns the UMSGID that will be used by the next message
 *  to be created.
 */

UMSGID apiSquishGetNextUid(HAREA ha)
{
    return Sqd->uidNext;
}

/*
 *  This function converts the message number 'dwMsg' into a unique message
 *  idenfitier (UMSGID).
 */

UMSGID apiSquishMsgnToUid(HAREA ha, dword dwMsg)
{
    SQIDX sqi;

    if (MsgInvalidHarea(ha))
    {
        return 0;
    }

    /* Make sure that it's a valid message number */

    if (dwMsg == 0 || dwMsg > ha->num_msg)
    {
        msgapierr = MERR_NOENT;
        return 0;
    }

    if (!SidxGet(Sqd->hix, dwMsg, &sqi))
    {
        return 0;
    }

    return sqi.umsgid;
}

/* This function converts the UMSGID in 'uid' into a real message number */

dword apiSquishUidToMsgn(HAREA ha, UMSGID uid, word wType)
{
    SQIDX sqi;
    dword rc = 0;
    sdword stLow, stHigh, stTry;
    dword dwMax;

    if (MsgInvalidHarea(ha))
    {
        return 0;
    }

    /* Don't let the user access msg 0 */

    if (uid == 0)
    {
        msgapierr = MERR_NOENT;
        return 0L;
    }

    /* Read the index into memory */

    if (!_SquishBeginBuffer(Sqd->hix))
    {
        return (dword) 0;
    }

    /* Set up intial bounds (inclusive) */

    dwMax = _SquishIndexSize(Sqd->hix) / SQIDX_SIZE;
    stLow = 1;
    stHigh = (sdword) dwMax;
    stTry = 1;

    /* Start off with a 0 umsgid */

    memset(&sqi, 0, sizeof sqi);

    /* While we still have a search range... */

    while (stLow <= stHigh)
    {
        stTry = (stLow + stHigh) / 2;

        /* If we got an exact match */

        if (!SidxGet(Sqd->hix, (dword) stTry, &sqi))
        {
            break;
        }

        if (sqi.umsgid == uid)
        {
            rc = (dword) stTry;
            break;
        }
        else if (uid > sqi.umsgid)
        {
            stLow = stTry + 1;
        }
        else
        {
            stHigh = stTry - 1;
        }
    }

    /* If we couldn't find it exactly, try the next/prior match */

    if (!rc)
    {
        if (wType == UID_PREV)
        {
            if (sqi.umsgid < uid)
            {
                rc = (dword) stTry;
            }
            else if (stTry == 1)
            {
                rc = 0;
            }
            else
            {
                rc = (dword) (stTry - 1L);
            }
        }
        else if (wType == UID_NEXT)
        {
            if (sqi.umsgid > uid || stTry == (long)dwMax)
            {
                rc = (dword) stTry;
            }
            else
            {
                rc = (dword) (stTry + 1L);
            }
        }
        else
        {
            msgapierr = MERR_NOENT;
        }
    }

    /* Free the memory used by the index */

    if (!_SquishFreeBuffer(Sqd->hix))
    {
        rc = 0;
    }

    return rc;
}

#endif
