/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 */

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

#ifndef MSGAPI_NO_SQUISH

dword apiSquishGetHash(HAREA ha, dword dwMsg)
{
    SQIDX sqi;

    if (!SidxGet(Sqd->hix, dwMsg, &sqi))
    {
        return (dword) 0L;
    }

    msgapierr = MERR_NONE;
    return sqi.hash;
}

#endif

/* Hash function used for calculating the hashes in the .sqi file */

dword SquishHash(byte * f)
{
    dword hash = 0, g;

    while (*f)
    {
        hash = (hash << 4) + (dword) tolower(*f);

        if ((g = (hash & 0xf0000000L)) != 0L)
        {
            hash |= g >> 24;
            hash |= g;
        }
        f++;
    }

    /* Strip off high bit */

    return hash & 0x7fffffffL;
}
