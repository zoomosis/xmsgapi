/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 */

#ifndef MSGAPI_NO_SQUISH

#define MSGAPI_HANDLERS
#define MSGAPI_NO_OLD_TYPES

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include "prog.h"
#include "old_msg.h"
#include "msgapi.h"
#include "api_sq.h"
#include "api_sqp.h"
#include "sq_idx.h"
#include "putword.h"

#if defined(__UNIX__) || defined(DJGPP)
#include <unistd.h>
#else
#include <io.h>
#include <share.h>
#endif

static int read_sqbase(int handle, struct _sqbase *psqbase)
{
    byte buf[SQBASE_SIZE], *pbuf = buf;

    if (read(handle, (byte *) buf, SQBASE_SIZE) != SQBASE_SIZE)
    {
        return 0;
    }

    psqbase->len = get_word(pbuf);
    pbuf += 2;

    psqbase->rsvd1 = get_word(pbuf);
    pbuf += 2;

    psqbase->num_msg = get_dword(pbuf);
    pbuf += 4;

    psqbase->high_msg = get_dword(pbuf);
    pbuf += 4;

    psqbase->skip_msg = get_dword(pbuf);
    pbuf += 4;

    psqbase->high_water = get_dword(pbuf);
    pbuf += 4;

    psqbase->uid = get_dword(pbuf);
    pbuf += 4;

    memmove(psqbase->base, pbuf, 80);
    pbuf += 80;

    psqbase->begin_frame = get_dword(pbuf);
    pbuf += 4;

    psqbase->last_frame = get_dword(pbuf);
    pbuf += 4;

    psqbase->free_frame = get_dword(pbuf);
    pbuf += 4;

    psqbase->last_free_frame = get_dword(pbuf);
    pbuf += 4;

    psqbase->end_frame = get_dword(pbuf);
    pbuf += 4;

    psqbase->max_msg = get_dword(pbuf);
    pbuf += 4;

    psqbase->keep_days = get_word(pbuf);
    pbuf += 2;

    psqbase->sz_sqhdr = get_word(pbuf);
    pbuf += 2;

    memmove(psqbase->rsvd2, pbuf, 124);
    pbuf += 124;

    assert(pbuf - buf == SQBASE_SIZE);

    return 1;
}

/* Read the base header from the beginning of the .sqd file */

unsigned _SquishReadBaseHeader(HAREA ha, SQBASE * psqb)
{
    if (lseek(Sqd->sfd, 0L, SEEK_SET) != 0 ||
      read_sqbase(Sqd->sfd, psqb) != 1)
    {
        if (errno == EACCES || errno == -1)
        {
            msgapierr = MERR_SHARE;
        }
        else
        {
            msgapierr = MERR_BADF;
        }

        return FALSE;
    }

    return TRUE;
}

static int write_sqbase(int handle, struct _sqbase *psqbase)
{
    byte buf[SQBASE_SIZE], *pbuf = buf;

    put_word(pbuf, psqbase->len);
    pbuf += 2;

    put_word(pbuf, psqbase->rsvd1);
    pbuf += 2;

    put_dword(pbuf, psqbase->num_msg);
    pbuf += 4;

    put_dword(pbuf, psqbase->high_msg);
    pbuf += 4;

    put_dword(pbuf, psqbase->skip_msg);
    pbuf += 4;

    put_dword(pbuf, psqbase->high_water);
    pbuf += 4;

    put_dword(pbuf, psqbase->uid);
    pbuf += 4;

    memmove(pbuf, psqbase->base, 80);
    pbuf += 80;

    put_dword(pbuf, psqbase->begin_frame);
    pbuf += 4;

    put_dword(pbuf, psqbase->last_frame);
    pbuf += 4;

    put_dword(pbuf, psqbase->free_frame);
    pbuf += 4;

    put_dword(pbuf, psqbase->last_free_frame);
    pbuf += 4;

    put_dword(pbuf, psqbase->end_frame);
    pbuf += 4;

    put_dword(pbuf, psqbase->max_msg);
    pbuf += 4;

    put_word(pbuf, psqbase->keep_days);
    pbuf += 2;

    put_word(pbuf, psqbase->sz_sqhdr);
    pbuf += 2;

    memmove(pbuf, psqbase->rsvd2, 124);
    pbuf += 124;

    assert(pbuf - buf == SQBASE_SIZE);

    return (write(handle, (byte *) buf, SQBASE_SIZE) == SQBASE_SIZE);
}

/* Write the base header back to the beginning of the .sqd file */

unsigned _SquishWriteBaseHeader(HAREA ha, SQBASE * psqb)
{
    if (lseek(Sqd->sfd, 0L, SEEK_SET) != 0 ||
      write_sqbase(Sqd->sfd, psqb) != 1)
    {
        msgapierr = MERR_NODS;
        return FALSE;
    }

    return TRUE;
}

/*
 *  Release the specified frame to the free chain.  The frame is located at
 *  offset 'fo', and the header at that offset has already been loaded into
 *  *psqh.  This function does _not_ change the links of other messages
 *  that may point to this message; it simply threads the current message
 *  into the free chain.
 * 
 *  This function assumes that we have exclusive access to the Squish base.
 */

unsigned _SquishInsertFreeChain(HAREA ha, FOFS fo, SQHDR * psqh)
{
    SQHDR sqh = *psqh;

    assert(Sqd->fHaveExclusive);

    sqh.id = SQHDRID;
    sqh.frame_type = FRAME_FREE;
    sqh.msg_length = sqh.clen = 0L;

    /* If we have no existing free frames, then this is easy */

    if (Sqd->foLastFree == NULL_FRAME)
    {
        sqh.prev_frame = NULL_FRAME;
        sqh.next_frame = NULL_FRAME;

        /* Try to write this frame back to the file */

        if (!_SquishWriteHdr(ha, fo, &sqh))
        {
            return FALSE;
        }

        Sqd->foFree = Sqd->foLastFree = fo;
        return TRUE;
    }


    /* There is an existing frame, so we must append to the end of the chain. */

    sqh.prev_frame = Sqd->foLastFree;
    sqh.next_frame = NULL_FRAME;

    /* Update the last chain in the free list, pointing it to us */

    if (!_SquishSetFrameNext(ha, sqh.prev_frame, fo))
    {
        return FALSE;
    }

    /* Try to write the current frame to disk */

    if (_SquishWriteHdr(ha, fo, &sqh))
    {
        Sqd->foLastFree = fo;
        return TRUE;
    }
    else
    {
        /* The write failed, so just hope that we can undo what we did earlier. */

        _SquishSetFrameNext(ha, sqh.prev_frame, NULL_FRAME);

        return FALSE;
    }
}

/* Change the 'next' link of the foModify frame to the value foValue */

unsigned _SquishSetFrameNext(HAREA ha, FOFS foModify, FOFS foValue)
{
    SQHDR sqh;

    if (!_SquishReadHdr(ha, foModify, &sqh))
    {
        return FALSE;
    }

    sqh.next_frame = foValue;

    return _SquishWriteHdr(ha, foModify, &sqh);
}

/* Change the 'prior' link of the foModify frame to the value foValue */

unsigned _SquishSetFramePrev(HAREA ha, FOFS foModify, FOFS foValue)
{
    SQHDR sqh;

    if (!_SquishReadHdr(ha, foModify, &sqh))
    {
        return FALSE;
    }

    sqh.prev_frame = foValue;

    return _SquishWriteHdr(ha, foModify, &sqh);
}

/* This function ensures that the message handle is readable */

unsigned _SquishReadMode(HMSG hmsg)
{
    if (hmsg->wMode != MOPEN_READ && hmsg->wMode != MOPEN_RW)
    {
        msgapierr = MERR_EACCES;
        return FALSE;
    }

    return TRUE;
}

/* This function ensures that the message handle is writable */

unsigned _SquishWriteMode(HMSG hmsg)
{
    if (hmsg->wMode != MOPEN_CREATE && hmsg->wMode != MOPEN_WRITE &&
      hmsg->wMode != MOPEN_RW)
    {
        msgapierr = MERR_EACCES;
        return FALSE;
    }

    return TRUE;
}

/* Translate a message number into a frame offset for area 'ha' */

FOFS _SquishGetFrameOfs(HAREA ha, dword dwMsg)
{
    SQIDX sqi;

    msgapierr = MERR_NOENT;

    /*
     *  Check for simple stuff that we can handle by following our own
     *  linked list.
     */

    if (dwMsg == ha->cur_msg)
    {
        return Sqd->foCur;
    }
    else if (dwMsg == ha->cur_msg - 1)
    {
        return Sqd->foPrev;
    }
    else if (dwMsg == ha->cur_msg + 1)
    {
        return Sqd->foNext;
    }

    /*
     *  We couldn't just follow the linked list, so we will have to consult 
     *  the Squish index file to find it.
     */

    if (!SidxGet(Sqd->hix, dwMsg, &sqi))
    {
        return NULL_FRAME;
    }

    return sqi.ofs;
}

static int read_sqhdr(int handle, SQHDR * psqhdr)
{
    byte buf[SQHDR_SIZE], *pbuf = buf;

    if (read(handle, (byte *) buf, SQHDR_SIZE) != SQHDR_SIZE)
    {
        return 0;
    }

    /* 4 bytes "id" */
    psqhdr->id = get_dword(pbuf);
    pbuf += 4;

    /* 4 bytes "next_frame" */
    psqhdr->next_frame = get_dword(pbuf);
    pbuf += 4;

    /* 4 bytes "prev_frame" */
    psqhdr->prev_frame = get_dword(pbuf);
    pbuf += 4;

    /* 4 bytes "frame_length" */
    psqhdr->frame_length = get_dword(pbuf);
    pbuf += 4;

    /* 4 bytes "msg_length" */
    psqhdr->msg_length = get_dword(pbuf);
    pbuf += 4;

    /* 4 bytes "clen" */
    psqhdr->clen = get_dword(pbuf);
    pbuf += 4;

    /* 2 bytes "frame_type" */
    psqhdr->frame_type = get_word(pbuf);
    pbuf += 2;

    /* 4 bytes "rsvd" */
    psqhdr->rsvd = get_word(pbuf);
    pbuf += 2;

    assert(pbuf - buf == SQHDR_SIZE);

    return 1;
}

/* Read the Squish header 'psqh' from the specified frame offset */

unsigned _SquishReadHdr(HAREA ha, FOFS fo, SQHDR * psqh)
{
    /* Ensure that we are reading a valid frame header */

    if (fo < SQBASE_SIZE)
    {
        msgapierr = MERR_BADA;
        return FALSE;
    }

    /* Seek and read the header */

    if (fo >= Sqd->foEnd || lseek(Sqd->sfd, fo, SEEK_SET) != fo ||
      read_sqhdr(Sqd->sfd, psqh) != 1 || psqh->id != SQHDRID)
    {
        msgapierr = MERR_BADF;
        return FALSE;
    }

    return TRUE;
}

static int write_sqhdr(int handle, SQHDR * psqhdr)
{
    byte buf[SQHDR_SIZE], *pbuf = buf;

    /* 4 bytes "id" */
    put_dword(pbuf, psqhdr->id);
    pbuf += 4;

    /* 4 bytes "next_frame" */
    put_dword(pbuf, psqhdr->next_frame);
    pbuf += 4;

    /* 4 bytes "prev_frame" */
    put_dword(pbuf, psqhdr->prev_frame);
    pbuf += 4;

    /* 4 bytes "frame_length" */
    put_dword(pbuf, psqhdr->frame_length);
    pbuf += 4;

    /* 4 bytes "msg_length" */
    put_dword(pbuf, psqhdr->msg_length);
    pbuf += 4;

    /* 4 bytes "clen" */
    put_dword(pbuf, psqhdr->clen);
    pbuf += 4;

    /* 2 bytes "frame_type" */
    put_word(pbuf, psqhdr->frame_type);
    pbuf += 2;

    /* 4 bytes "rsvd" */
    put_word(pbuf, psqhdr->rsvd);
    pbuf += 2;

    assert(pbuf - buf == SQHDR_SIZE);

    return write(handle, (byte *) buf, SQHDR_SIZE) == SQHDR_SIZE;
}

/* Write the Squish header 'psqh' to the specified frame offset */

unsigned _SquishWriteHdr(HAREA ha, FOFS fo, SQHDR * psqh)
{
    /* Make sure that we don't write over the file header */

    if (fo < SQBASE_SIZE)
    {
        msgapierr = MERR_BADA;
        return FALSE;
    }

    if (lseek(Sqd->sfd, fo, SEEK_SET) != fo ||
        write_sqhdr(Sqd->sfd, psqh) != 1)
    {
        msgapierr = MERR_NODS;
        return FALSE;
    }

    return TRUE;
}

/*
 *  This function fixes the in-memory pointers after message dwMsg was
 *  removed from the index file.
 *
 *  This function assumes that we have exclusive access to the Squish base.
 */

unsigned _SquishFixMemoryPointers(HAREA ha, dword dwMsg, SQHDR * psqh)
{
    assert(Sqd->fHaveExclusive);

    /* Adjust the first/last message pointers */

    if (dwMsg == 1)
    {
        Sqd->foFirst = psqh->next_frame;
    }

    if (dwMsg == ha->num_msg)
    {
        Sqd->foLast = psqh->prev_frame;
    }

    /* Now fix up the in-memory version of the prior/next links */

    if (dwMsg == ha->cur_msg + 1)
    {
        Sqd->foNext = psqh->next_frame;
    }

    if (dwMsg == ha->cur_msg - 1)
    {
        Sqd->foPrev = psqh->prev_frame;
    }

    /* If we killed the message that we are on, it's a special case */

    if (dwMsg == ha->cur_msg)
    {
        SQHDR sqh;

        /* Go to the header of the prior msg */

        if (!_SquishReadHdr(ha, psqh->prev_frame, &sqh))
        {
            /* That does not exist, so go to msg 0 */

            Sqd->foCur = Sqd->foPrev = NULL_FRAME;
            Sqd->foNext = Sqd->foFirst;
            ha->cur_msg = 0;
        }
        else
        {
            /* Otherwise, adjust pointers appropriately */

            Sqd->foCur = psqh->prev_frame;
            Sqd->foPrev = sqh.prev_frame;
            Sqd->foNext = sqh.next_frame;
            ha->cur_msg--;
        }
    }
    else
    {
        /*
         *  We didn't kill the current msg, so just decrement cur_msg if
         *  we were higher than the deleted message.
         */

        if (ha->cur_msg >= dwMsg)
        {
            ha->cur_msg--;
        }
    }

    /* Adjust the message numbers appropriately */

    ha->num_msg--;
    ha->high_msg--;

    if (ha->high_water >= dwMsg)
    {
        ha->high_water--;
    }

    return TRUE;
}

#endif
