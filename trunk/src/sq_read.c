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
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "prog.h"
#include "old_msg.h"
#include "msgapi.h"
#include "api_sq.h"
#include "api_sqp.h"
#include "putword.h"

#if defined(__UNIX__) || defined(DJGPP)
#include <unistd.h>
#else
#include <io.h>
#include <share.h>
#endif

static int read_xmsg(int handle, XMSG * pxmsg)
{
    byte buf[XMSG_SIZE], *pbuf = buf;
    word rawdate, rawtime;
    int i;

    if (read(handle, (byte *) buf, XMSG_SIZE) != XMSG_SIZE)
    {
        return 0;
    }

    /* 04 bytes "attr" */
    pxmsg->attr = get_dword(pbuf);
    pbuf += 4;

    /* 36 bytes "from" */
    memmove(pxmsg->from, pbuf, XMSG_FROM_SIZE);
    pbuf += XMSG_FROM_SIZE;

    /* 36 bytes "to"   */
    memmove(pxmsg->to, pbuf, XMSG_TO_SIZE);
    pbuf += XMSG_TO_SIZE;

    /* 72 bytes "subj" */
    memmove(pxmsg->subj, pbuf, XMSG_SUBJ_SIZE);
    pbuf += XMSG_SUBJ_SIZE;

    /* 8 bytes "orig"  */
    pxmsg->orig.zone = get_word(pbuf);
    pbuf += 2;

    pxmsg->orig.net = get_word(pbuf);
    pbuf += 2;

    pxmsg->orig.node = get_word(pbuf);
    pbuf += 2;

    pxmsg->orig.point = get_word(pbuf);
    pbuf += 2;

    /* 8 bytes "dest"  */
    pxmsg->dest.zone = get_word(pbuf);
    pbuf += 2;

    pxmsg->dest.net = get_word(pbuf);
    pbuf += 2;

    pxmsg->dest.node = get_word(pbuf);
    pbuf += 2;

    pxmsg->dest.point = get_word(pbuf);
    pbuf += 2;

    /* 4 bytes "date_written" */
    rawdate = get_word(pbuf);
    pbuf += 2;

    rawtime = get_word(pbuf);
    pbuf += 2;

    pxmsg->date_written.date.da = rawdate & 31;
    pxmsg->date_written.date.mo = (rawdate >> 5) & 15;
    pxmsg->date_written.date.yr = (rawdate >> 9) & 127;
    pxmsg->date_written.time.ss = rawtime & 31;
    pxmsg->date_written.time.mm = (rawtime >> 5) & 63;
    pxmsg->date_written.time.hh = (rawtime >> 11) & 31;

    /* 4 bytes "date_arrived" */
    rawdate = get_word(pbuf);
    pbuf += 2;

    rawtime = get_word(pbuf);
    pbuf += 2;

    pxmsg->date_arrived.date.da = rawdate & 31;
    pxmsg->date_arrived.date.mo = (rawdate >> 5) & 15;
    pxmsg->date_arrived.date.yr = (rawdate >> 9) & 127;
    pxmsg->date_arrived.time.ss = rawtime & 31;
    pxmsg->date_arrived.time.mm = (rawtime >> 5) & 63;
    pxmsg->date_arrived.time.hh = (rawtime >> 11) & 31;

    /* 2 byte "utc_ofs" */
    pxmsg->utc_ofs = get_word(pbuf);
    pbuf += 2;

    /* 4 bytes "replyto" */
    pxmsg->replyto = get_dword(pbuf);
    pbuf += 4;

    /* 10 times 4 bytes "replies" */

    for (i = 0; i < MAX_REPLY; i++)
    {
        pxmsg->replies[i] = get_dword(pbuf);
        pbuf += 4;
    }

    /* 4 bytes "umsgid" */
    pxmsg->umsgid = get_dword(pbuf);
    pbuf += 4;

    /* 20 times FTSC date stamp */
    memmove(pxmsg->__ftsc_date, pbuf, 20);
    pbuf += 20;

    assert(pbuf - buf == XMSG_SIZE);
    return 1;
}

/* Read in the binary message header from the data file */

static unsigned _SquishReadXmsg(HMSG hmsg, PXMSG pxm, dword * pdwOfs)
{
    long ofs = hmsg->foRead + HSqd->cbSqhdr;

    if (*pdwOfs != (dword) ofs)
    {
        if (lseek(HSqd->sfd, ofs, SEEK_SET) != ofs)
        {
            msgapierr = MERR_BADF;
            return FALSE;
        }
    }

    if (read_xmsg(HSqd->sfd, pxm) != 1)
    {
        msgapierr = MERR_BADF;
        return FALSE;
    }

    /* Update our position */

    *pdwOfs = (dword) ofs + (dword) XMSG_SIZE;

    /*
     *  If there is a UMSGID associated with this message, store it in
     *  memory in case we have to write the message later.  Blank it out so
     *  that the application cannot access it.
     */

    if (pxm->attr & MSGUID)
    {
        hmsg->uidUs = pxm->umsgid;
    }

    return TRUE;
}


/* Read in the control information for the current message */

static unsigned _SquishReadCtrl(HMSG hmsg, byte * szCtrl, dword dwCtrlLen,
  dword * pdwOfs)
{
    long ofs = hmsg->foRead + HSqd->cbSqhdr + XMSG_SIZE;
    unsigned uMaxLen = (unsigned)min(dwCtrlLen, hmsg->sqhRead.clen);

    /*
     *  Read the specified amount of text, but no more than specified in
     *  the frame header.
     */

    *szCtrl = 0;

    if (*pdwOfs != (dword) ofs)
    {
        if (lseek(HSqd->sfd, ofs, SEEK_SET) != ofs)
        {
            msgapierr = MERR_BADF;
            return FALSE;
        }
    }

    if (read(HSqd->sfd, (char *)szCtrl, uMaxLen) != (int)uMaxLen)
    {
        msgapierr = MERR_BADF;
        return FALSE;
    }

    *pdwOfs = (dword) ofs + (dword) uMaxLen;

    return TRUE;
}

/* Read in the text body for the current message */

static dword _SquishReadTxt(HMSG hmsg, byte * szTxt, dword dwTxtLen,
  dword * pdwOfs)
{
    /* Start reading from the cur_pos offset */

    long ofs = hmsg->foRead + (long)HSqd->cbSqhdr + (long)XMSG_SIZE +
      (long)hmsg->sqhRead.clen;

    /* Max length is the size of the msg text inside the frame */

    unsigned uMaxLen = (unsigned)(hmsg->sqhRead.msg_length -
      hmsg->sqhRead.clen - XMSG_SIZE);

    *szTxt = 0;

    /* Make sure that we don't try to read beyond the end of the msg */

    if (hmsg->cur_pos > uMaxLen)
    {
        hmsg->cur_pos = uMaxLen;
    }

    /* Now seek to the position that we are supposed to read from */

    ofs += (long)hmsg->cur_pos;

    /*
     *  Decrement the max length by the seek posn, but don't read more
     *  than the user asked for.
     */

    uMaxLen -= (unsigned)hmsg->cur_pos;
    uMaxLen = min(uMaxLen, (unsigned)dwTxtLen);

    /* Now try to read that information from the file */

    if (ofs != (long)*pdwOfs)
    {
        if (lseek(HSqd->sfd, ofs, SEEK_SET) != ofs)
        {
            msgapierr = MERR_BADF;
            return (dword) -1;
        }
    }

    if (read(HSqd->sfd, (char *)szTxt, uMaxLen) != (int)uMaxLen)
    {
        msgapierr = MERR_BADF;
        return (dword) -1;
    }

    *pdwOfs = (dword) ofs + (dword) uMaxLen;

    /* Increment the current position by the number of bytes that we read */

    hmsg->cur_pos += (dword) uMaxLen;

    return (dword) uMaxLen;
}

/* Read a message from the Squish base */

dword apiSquishReadMsg(HMSG hmsg, PXMSG pxm, dword dwOfs,
  dword dwTxtLen, byte * szTxt, dword dwCtrlLen, byte * szCtrl)
{
    dword dwSeekOfs = (dword) -1;  /* Current offset */
    unsigned fOkay = TRUE;      /* Any errors so far? */
    dword dwGot = 0;            /* Bytes read from file */

    /* Make sure that we have a valid handle (and that it's in read mode) */

    if (MsgInvalidHmsg(hmsg) || !_SquishReadMode(hmsg))
    {
        return (dword) -1;
    }

    /*
     *  Make sure that we can use szTxt and szCtrl as flags controlling
     *  what to read.
     */

    if (!dwTxtLen)
    {
        szTxt = NULL;
    }

    if (!dwCtrlLen)
    {
        szCtrl = NULL;
    }

    /*
     *  Now read in the message header, the control information, and the
     *  message text.
     */

    if (pxm)
    {
        fOkay = _SquishReadXmsg(hmsg, pxm, &dwSeekOfs);
    }

    if (fOkay && szCtrl)
    {
        fOkay = _SquishReadCtrl(hmsg, szCtrl, dwCtrlLen, &dwSeekOfs);
    }

    if (fOkay && szTxt)
    {
        hmsg->cur_pos = dwOfs;

        dwGot = _SquishReadTxt(hmsg, szTxt, dwTxtLen, &dwSeekOfs);
        
        if (dwGot == (dword) -1)
        {
            fOkay = FALSE;
        }
    }

    /*
     *  If everything worked okay, return the number bytes that we read
     *  from the message body.
     */

    return fOkay ? dwGot : (dword) -1;
}

#endif
