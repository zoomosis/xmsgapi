/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 */

#ifndef MSGAPI_NO_SQUISH

#define MSGAPI_HANDLERS
#define MSGAPI_NO_OLD_TYPES

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

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
 *  Kill the specified message number. This function assumes that we
 *  have exclusive access to the Squish base.
 */

static sword _SquishKill(HAREA ha, dword dwMsg, SQHDR * psqh, FOFS fo)
{
    assert(Sqd->fHaveExclusive);

    /* Link the existing messages over this one */

    if (psqh->prev_frame)
    {
        if (!_SquishSetFrameNext(ha, psqh->prev_frame, psqh->next_frame))
        {
            return FALSE;
        }
    }

    if (psqh->next_frame)
    {
        if (!_SquishSetFramePrev(ha, psqh->next_frame, psqh->prev_frame))
        {
            return FALSE;
        }
    }

    /* Delete this message from the index file */

    if (!_SquishRemoveIndexEntry(Sqd->hix, dwMsg, NULL, psqh, TRUE))
    {
        return FALSE;
    }

    /* Finally, add the freed message to the free frame list */

    return (sword) _SquishInsertFreeChain(ha, fo, psqh);
}

/* This function is used to delete a message from a Squish message base */

sword apiSquishKillMsg(HAREA ha, dword dwMsg)
{
    SQHDR sqh;
    sword rc;
    FOFS fo;

    /* Validate parameters */

    if (MsgInvalidHarea(ha))
    {
        return -1;
    }

    /* Make sure that the message actually exists */

    if (dwMsg == 0 || dwMsg > ha->num_msg)
    {
        msgapierr = MERR_NOENT;
        return -1;
    }

    /* Get the offset of the frame to delete */

    fo = _SquishGetFrameOfs(ha, dwMsg);
    
    if (fo == NULL_FRAME)
    {
        return -1;
    }

    /* Read that into memory */

    if (!_SquishReadHdr(ha, fo, &sqh))
    {
        return -1;
    }

    /* Now get exclusive access for the delete operation */

    if (!_SquishExclusiveBegin(ha))
    {
        return FALSE;
    }

    /* Let _SquishKill to the dirty work */

    rc = _SquishKill(ha, dwMsg, &sqh, fo);

    /* Let go of the base */

    if (!_SquishExclusiveEnd(ha))
    {
        rc = FALSE;
    }

    if (rc != 0)
    {
        return 0;
    }

    return -1;
}

#endif
