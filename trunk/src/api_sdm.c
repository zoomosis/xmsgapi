/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 *
 *  api_sdm.c
 *
 *  *.MSG (Star Dot Message) messagebase access routines.
 */

#ifndef MSGAPI_NO_SDM
 
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "compiler.h"
#include "putword.h"
#include "qksort.h"

#ifdef __TURBOC__
#include <dos.h>
#endif

#if defined(__TURBOC__) || defined(__WATCOMC__) || defined(_MSC_VER) || defined(__LCC__)
#include <direct.h>
#endif

#ifdef __UNIX__
#include <unistd.h>
#else
#include <io.h>
#include <share.h>
#endif

#ifdef DJGPP
#include <unistd.h>
#endif

#ifdef __BEOS__
#include <be/kernel/fs_attr.h>
#include <be/support/TypeConstants.h>
#endif

#define MSGAPI_HANDLERS

#include "prog.h"
#include "osdep.h"
#include "old_msg.h"
#include "msgapi.h"
#include "api_sdm.h"
#include "unused.h"

#define SDM_BLOCK 256
#define Mhd ((struct _sdmdata *)(mh->apidata))
#define MsghMhd ((struct _sdmdata *)(((struct _msgh *)msgh)->sq->apidata))

static byte *hwm_from = (byte *) "-=|\xFFSquishMail\xFF|=-";

static sword SdmCloseArea(MSGA * mh);
static MSGH *SdmOpenMsg(MSGA * mh, word mode, dword msgnum);
static sword SdmCloseMsg(MSGH * msgh);
static dword SdmReadMsg(MSGH * msgh, XMSG * msg, dword offset,
  dword bytes, byte * text, dword clen, byte * ctxt);
static sword SdmWriteMsg(MSGH * msgh, word append, XMSG * msg,
  byte * text, dword textlen, dword totlen, dword clen, byte * ctxt);
static sword SdmKillMsg(MSGA * mh, dword msgnum);
static sword SdmLock(MSGA * mh);
static sword SdmUnlock(MSGA * mh);
static sword SdmSetCurPos(MSGH * msgh, dword pos);
static dword SdmGetCurPos(MSGH * msgh);
static UMSGID SdmMsgnToUid(MSGA * mh, dword msgnum);
static dword SdmUidToMsgn(MSGA * mh, UMSGID umsgid, word type);
static dword SdmGetHighWater(MSGA * mh);
static sword SdmSetHighWater(MSGA * sq, dword hwm);
static dword SdmGetTextLen(MSGH * msgh);
static dword SdmGetCtrlLen(MSGH * msgh);
static UMSGID SdmGetNextUid(HAREA ha);
static dword SdmGetHash(HAREA mh, dword msgnum);

static struct _apifuncs sdm_funcs =
{
    SdmCloseArea,
    SdmOpenMsg,
    SdmCloseMsg,
    SdmReadMsg,
    SdmWriteMsg,
    SdmKillMsg,
    SdmLock,
    SdmUnlock,
    SdmSetCurPos,
    SdmGetCurPos,
    SdmMsgnToUid,
    SdmUidToMsgn,
    SdmGetHighWater,
    SdmSetHighWater,
    SdmGetTextLen,
    SdmGetCtrlLen,
    SdmGetNextUid,
    SdmGetHash
};

static void Convert_Fmsg_To_Xmsg(struct _omsg *fmsg, XMSG * msg,
  word def_zone);

static void Convert_Xmsg_To_Fmsg(XMSG * msg, struct _omsg *fmsg);
static void Init_Xmsg(XMSG * msg);
static sword _SdmRescanArea(MSGA * mh);
static sword _Grab_Clen(MSGH * msgh);
static void WriteToFd(byte * str);

static void Get_Binary_Date(struct _stamp *todate,
  struct _stamp *fromdate, byte * asciidate);

static int statfd;  /* file handle for WriteToFd */

static byte *sd_msg = (byte *) "%s%u.msg";

static char *Add_Trailing(char *str, char add)
{
    int x;

    if (str && *str && str[x = strlen(str) - 1] != add)
    {
        str[x + 1] = add;
        str[x + 2] = '\0';
    }

    return str;
}


MSGA *MSGAPI SdmOpenArea(byte * name, word mode, word type)
{
    MSGA *mh;

    mh = palloc(sizeof(MSGA));

    if (mh == NULL)
    {
        msgapierr = MERR_NOMEM;
        goto ErrOpen;
    }

    memset(mh, '\0', sizeof(MSGA));

    mh->id = MSGAPI_ID;

    if (type & MSGTYPE_ECHO)
    {
        mh->isecho = TRUE;
    }

    mh->api = (struct _apifuncs *)palloc(sizeof(struct _apifuncs));

    if (mh->api == NULL)
    {
        msgapierr = MERR_NOMEM;
        goto ErrOpen;
    }

    memset(mh->api, '\0', sizeof(struct _apifuncs));

    mh->apidata = (void *)palloc(sizeof(struct _sdmdata));

    if (mh->apidata == NULL)
    {
        msgapierr = MERR_NOMEM;
        goto ErrOpen;
    }

    memset((byte *) mh->apidata, '\0', sizeof(struct _sdmdata));

    strcpy((char *)Mhd->base, (char *)name);
    Add_Trailing((char *)Mhd->base, PATH_DELIM);
    Mhd->hwm = (dword) -1;

    mh->len = sizeof(MSGA);
    mh->num_msg = 0;
    mh->high_msg = 0;
    mh->high_water = (dword) -1;

    if (!direxist((char *)name) && (mode == MSGAREA_NORMAL ||
      _createDirectoryTree((char *)name) != 0))
    {
        msgapierr = MERR_NOENT;
        goto ErrOpen;
    }

    if (!_SdmRescanArea(mh))
    {
        goto ErrOpen;
    }

    mh->type &= ~MSGTYPE_ECHO;

    *mh->api = sdm_funcs;
    mh->sz_xmsg = sizeof(XMSG);

    msgapierr = 0;
    return mh;

ErrOpen:

    if (mh)
    {
        if (mh->api)
        {
            if (mh->apidata)
            {
                pfree((char *)mh->apidata);
            }

            pfree(mh->api);
        }

        pfree(mh);
    }

    return NULL;
}

int SdmDeleteBase(char *name)
{
    FFIND *ff;
    char *temp;

    temp = malloc(strlen(name) + 6);
    sprintf(temp, "%s*.msg", name);

    ff = FFindOpen(temp, 0);

    free(temp);

    if (ff != 0)
    {
        do
        {
            temp = malloc(strlen(name) + strlen(ff->ff_name) + 1);
            sprintf(temp, "%s%s", name, ff->ff_name);
            unlink(temp);
            free(temp);
        }
        while (FFindNext(ff) == 0);

        FFindClose(ff);
    }

    rmdir(name);

    /* rmdir error is ok */
    
    return 1;
}

static sword SdmCloseArea(MSGA * mh)
{
    XMSG msg;
    MSGH *msgh;

    static byte *msgbody = (byte *)
      "NOECHO\r\rPlease ignore.  This message is only used by the SquishMail "
      "system to store\rthe high water mark for each conference area.\r\r\r\r"
      "\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r(Elvis was here!)\r\r\r";

    if (InvalidMh(mh))
    {
        return -1;
    }

    if (Mhd->hwm_chgd)
    {
        msgh = SdmOpenMsg(mh, MOPEN_CREATE, 1L);

        if (msgh != NULL)
        {
            Init_Xmsg(&msg);

            Get_Dos_Date((union stamp_combo *)&msg.date_arrived);
            Get_Dos_Date((union stamp_combo *)&msg.date_written);

            /* 
             *  Use high-bit chars in the to/from field, so that (l)users
             *  can't log on as this userid and delete the HWM.
             */

            strcpy((char *)msg.from, (char *)hwm_from);
            strcpy((char *)msg.to, (char *)msg.from);
            strcpy((char *)msg.subj, "High wadda' mark");

            /* To prevent "INTL 0:0/0 0:0/0" kludges */
            msg.orig.zone = msg.dest.zone = mi.def_zone;

            msg.replyto = mh->high_water;
            msg.attr = MSGPRIVATE | MSGREAD | MSGLOCAL | MSGSENT;

            SdmWriteMsg(msgh, FALSE, &msg, msgbody,
              strlen((char *)msgbody) + 1, strlen((char *)msgbody) + 1,
              0L, NULL);

            SdmCloseMsg(msgh);
        }
    }

    if (Mhd->msgs_open)
    {
        msgapierr = MERR_EOPEN;
        return -1;
    }

    if (Mhd->msgnum)
    {
        pfree(Mhd->msgnum);
    }

    pfree((char *)mh->apidata);
    pfree(mh->api);

    mh->id = 0L;
    pfree(mh);

    msgapierr = MERR_NONE;
    return 0;
}

static MSGH *SdmOpenMsg(MSGA * mh, word mode, dword msgnum)
{
    byte msgname[PATHLEN];
    int handle, filemode;
    int owrite = FALSE;
    dword msguid = 0;

    MSGH *msgh;

    if (InvalidMh(mh))
    {
        return NULL;
    }

    if (msgnum == MSGNUM_CUR)
    {
        msgnum = mh->cur_msg;
    }
    else if (msgnum == MSGNUM_PREV)
    {
        msgnum = mh->cur_msg - 1;

        if (msgnum == 0)
        {
            msgapierr = MERR_NOENT;
            return NULL;
        }
    }
    else if (msgnum == MSGNUM_NEXT)
    {
        msgnum = mh->num_msg + 1;

        if (msgnum > mh->num_msg)
        {
            msgapierr = MERR_NOENT;
            return NULL;
        }
    }
    else if (mode != MOPEN_CREATE)
    {
        /* 
         *  If we're not creating, make sure that the specified msg# can
         *  be found.
         */

        if (msgnum <= 0 || msgnum > mh->num_msg)
        {
            msgapierr = MERR_NOENT;
            return NULL;
        }
    }

    if (msgnum <= mh->num_msg && msgnum > 0)
    {
        msguid = SdmMsgnToUid(mh, msgnum);
    }

    if (mode == MOPEN_CREATE)
    {
        /* If we're creating a new message... */

        if (msgnum == 0L)
        {
            /* 
             *  If the base isn't locked, make sure that we avoid
             *  conflicts...
             */

            if (!mh->locked)
            {
                /* Check to see if the msg we're writing already exists */

                sprintf((char *)msgname, (char *)sd_msg, Mhd->base,
                  (int)mh->high_msg + 1);

                if (fexist((char *)msgname))
                {
                    /* If so, rescan the base, to find out which msg# it is. */

                    if (Mhd->msgnum && Mhd->msgnum_len)
                    {
                        pfree(Mhd->msgnum);
                    }

                    if (!_SdmRescanArea(mh))
                    {
                        return NULL;
                    }
                }
            }

            msgnum = ++mh->num_msg;
            msguid = ++mh->high_msg;

            /* 
             *  Make sure that we don't overwrite the high-water mark,
             *  unless we call with msgnum != 0L (a specific number).
             */

            if (mh->isecho && msgnum == 1)
            {
                msgnum = mh->high_msg = 2;
            }
        }
        else
        {
            /* otherwise, we're overwriting an existing message */

            owrite = TRUE;
        }

        filemode = O_CREAT | O_TRUNC | O_RDWR;
    }
    else if (mode == MOPEN_READ)
    {
        filemode = O_RDONLY;
    }
    else if (mode == MOPEN_WRITE)
    {
        filemode = O_WRONLY;
    }
    else
    {
        filemode = O_RDWR;
    }

    sprintf((char *)msgname, (char *)sd_msg, Mhd->base, (int)msguid);

    handle = sopen((char *)msgname, filemode | O_BINARY, SH_DENYNONE,
      FILEMODE(mh->isecho));

    if (handle == -1)
    {
        if (filemode & O_CREAT)
        {
            msgapierr = MERR_BADF;
        }
        else
        {
            msgapierr = MERR_NOENT;
        }
        return NULL;
    }

    mh->cur_msg = msgnum;

    msgh = palloc(sizeof(MSGH));

    if (msgh == NULL)
    {
        close(handle);
        msgapierr = MERR_NOMEM;
        return NULL;
    }

    memset(msgh, '\0', sizeof(MSGH));
    msgh->fd = handle;

    if (mode == MOPEN_CREATE)
    {
        if (mh->num_msg + 1 >= (dword) Mhd->msgnum_len)
        {
            Mhd->msgnum_len += (word) SDM_BLOCK;

            Mhd->msgnum = realloc(Mhd->msgnum,
              Mhd->msgnum_len * sizeof(unsigned));

            if (!Mhd->msgnum)
            {
                pfree(msgh);
                close(handle);
                msgapierr = MERR_NOMEM;
                return NULL;
            }
        }

        /* 
         *  If we're writing a new msg, this is easy -- just add to
         *  end of list.
         */

        if (!owrite)
        {
            Mhd->msgnum[(size_t) (mh->num_msg)] = (word) msguid;
            mh->num_msg++;
        }
        else
        {
            /* 
             *  If this message is already in the list then do nothing --
             *  simply overwrite it, keeping the same message number, so
             *  no action is required.  Otherwise, we have to shift
             *  everything up by one since we're adding this new message
             *  in between two others.
             */

            if ((dword) Mhd->msgnum[msgnum - 1] != msguid)
            {
                memmove(Mhd->msgnum + msgnum, Mhd->msgnum + msgnum - 1,
                  ((size_t) mh->num_msg - msgnum) * sizeof(Mhd->msgnum[0]));

                Mhd->msgnum[msgnum - 1] = (word) msguid;
                mh->num_msg++;
            }
        }
    }

    msgh->cur_pos = 0L;

    if (mode == MOPEN_CREATE)
    {
        msgh->msg_len = 0;
    }
    else
    {
        msgh->msg_len = (dword) -1;
    }

    msgh->sq = mh;
    msgh->id = MSGH_ID;
    msgh->ctrl = NULL;
    msgh->clen = -1;
    msgh->zplen = 0;

    msgapierr = MERR_NONE;

    /* Keep track of how many messages were opened for this area */

    MsghMhd->msgs_open++;

    return msgh;
}

static sword SdmCloseMsg(MSGH * msgh)
{
    if (InvalidMsgh(msgh))
    {
        return -1;
    }

    MsghMhd->msgs_open--;

    if (msgh->ctrl)
    {
        pfree(msgh->ctrl);
        msgh->ctrl = NULL;
    }

    close(msgh->fd);

    msgh->id = 0L;
    pfree(msgh);

    msgapierr = MERR_NONE;
    return 0;
}

static int read_omsg(int handle, struct _omsg *pomsg)
{
    byte buf[OMSG_SIZE], *pbuf = buf;
    word rawdate, rawtime;

    if (read(handle, (byte *) buf, OMSG_SIZE) != OMSG_SIZE)
    {
        return 0;
    }

    memmove(pomsg->from, pbuf, 36);
    pbuf += 36;

    memmove(pomsg->to, pbuf, 36);
    pbuf += 36;

    memmove(pomsg->subj, pbuf, 72);
    pbuf += 72;

    memmove(pomsg->date, pbuf, 20);
    pbuf += 20;

    pomsg->times = get_word(pbuf);
    pbuf += 2;

    pomsg->dest = get_word(pbuf);
    pbuf += 2;

    pomsg->orig = get_word(pbuf);
    pbuf += 2;

    pomsg->cost = get_word(pbuf);
    pbuf += 2;

    pomsg->orig_net = get_word(pbuf);
    pbuf += 2;

    pomsg->dest_net = get_word(pbuf);
    pbuf += 2;

    /* 4 bytes "date_written" */
    rawdate = get_word(pbuf);
    pbuf += 2;

    rawtime = get_word(pbuf);
    pbuf += 2;

    pomsg->date_written.date.da = rawdate & 31;
    pomsg->date_written.date.mo = (rawdate >> 5) & 15;
    pomsg->date_written.date.yr = (rawdate >> 9) & 127;
    pomsg->date_written.time.ss = rawtime & 31;
    pomsg->date_written.time.mm = (rawtime >> 5) & 63;
    pomsg->date_written.time.hh = (rawtime >> 11) & 31;

    /* 4 bytes "date_arrived" */
    rawdate = get_word(pbuf);
    pbuf += 2;

    rawtime = get_word(pbuf);
    pbuf += 2;

    pomsg->date_arrived.date.da = rawdate & 31;
    pomsg->date_arrived.date.mo = (rawdate >> 5) & 15;
    pomsg->date_arrived.date.yr = (rawdate >> 9) & 127;
    pomsg->date_arrived.time.ss = rawtime & 31;
    pomsg->date_arrived.time.mm = (rawtime >> 5) & 63;
    pomsg->date_arrived.time.hh = (rawtime >> 11) & 31;

    pomsg->reply = get_word(pbuf);
    pbuf += 2;

    pomsg->attr = get_word(pbuf);
    pbuf += 2;

    pomsg->up = get_word(pbuf);
    pbuf += 2;

    assert(pbuf - buf == OMSG_SIZE);

    return 1;
}

static dword SdmReadMsg(MSGH * msgh, XMSG * msg, dword offset, dword bytes, byte * text, dword clen, byte * ctxt)
{
    unsigned len;
    dword realbytes, got;
    struct _omsg fmsg;
    word need_ctrl;
    NETADDR *orig, *dest;
    byte *fake_msgbuf = NULL, *newtext;

    if (InvalidMsgh(msgh))
    {
        return (dword) -1;
    }

    if (!(clen && ctxt))
    {
        clen = 0L;
        ctxt = NULL;
    }

    if (!(text && bytes))
    {
        bytes = 0L;
        text = NULL;
    }

    orig = &msg->orig;
    dest = &msg->dest;

    if (msg)
    {
        lseek(msgh->fd, 0L, SEEK_SET);

        if (!read_omsg(msgh->fd, &fmsg))
        {
            msgapierr = MERR_BADF;
            return (dword) -1;
        }

        fmsg.to[sizeof(fmsg.to) - 1] = '\0';
        fmsg.from[sizeof(fmsg.from) - 1] = '\0';
        fmsg.subj[sizeof(fmsg.subj) - 1] = '\0';
        fmsg.date[sizeof(fmsg.date) - 1] = '\0';

        Convert_Fmsg_To_Xmsg(&fmsg, msg, mi.def_zone);

        StripNasties(msg->from);
        StripNasties(msg->to);
        StripNasties(msg->subj);
    }

    /* 
     *  If we weren't instructed to read some message text (ie. only the
     *  header, read a block anyway.  We need to scan for kludge lines,
     *  to pick out the appropriate zone/point info.)
     */

    if (msgh->ctrl == NULL && ((msg || ctxt || text) ||
      (msg || ctxt || text) == 0))
    {
        need_ctrl = TRUE;
    }
    else
    {
        need_ctrl = FALSE;
    }

    realbytes = bytes;
    unused(realbytes);

    /* 
     *  If we need to read the control information, and the user hasn't
     *  requested a read operation, we'll need to do one anyway.
     */

    if (need_ctrl && text == NULL)
    {
        struct stat st;

        fstat(msgh->fd, &st);

        text = fake_msgbuf = palloc(st.st_size - OMSG_SIZE + 1);

        if (text == NULL)
        {
            msgapierr = MERR_NOMEM;
            return (dword) -1;
        }

        text[st.st_size - OMSG_SIZE] = '\0';
        bytes = st.st_size - OMSG_SIZE;
    }

    /* If we need to read in some text... */

    if (text)
    {
        /* Seek is superfluous if we just read msg header */

        if (!msg || msgh->msgtxt_start != 0)
        {
            lseek(msgh->fd, (dword) OMSG_SIZE + msgh->msgtxt_start + offset,
              SEEK_SET);

            msgh->cur_pos = offset;
        }

        got = (dword) read(msgh->fd, text, (unsigned int)bytes);
        text[(unsigned int)got] = '\0';

        /* 
         *  Update counter only if we got some text, and only if we're
         *  doing a read requested by the user (as opposed to reading
         *  ahead to find kludge lines).
         */

        if (got > 0 && !fake_msgbuf)
        {
            msgh->cur_pos += got;
        }
    }
    else
    {
        got = 0;
    }

    /* Convert the kludges into 'ctxt' format */

    if (need_ctrl && got && offset == 0L)
    {
        len = got;

        msgh->ctrl = CopyToControlBuf(text, &newtext, &len);
        if (msgh->ctrl != NULL)
        {
            msgh->clen = (dword) strlen((char *)msgh->ctrl) + 1;
            msgh->msgtxt_start = newtext - text;

            /* Shift back the text buffer to counter absence of ^a strings */

            memmove(text, newtext, (size_t) (bytes - (newtext - text)));

            got -= (dword) (msgh->clen - 1);
        }
    }

    /* Scan the ctxt ourselves to find zone/point info */

    if (msg && msgh->ctrl)
    {
        ConvertControlInfo(msgh->ctrl, orig, dest);
    }

    /* And if the app requested ctrlinfo, put it in its place. */

    if (ctxt && msgh->ctrl)
    {
        size_t slen;

        slen = strlen((char *)msgh->ctrl) + 1;
        memmove(ctxt, msgh->ctrl, min(slen, (size_t) clen));
        ctxt[min(slen, (size_t) clen)] = '\0';
    }

    if (fake_msgbuf)
    {
        pfree(fake_msgbuf);
        got = 0;
    }

    msgapierr = MERR_NONE;
    return got;
}

static int write_omsg(int handle, struct _omsg *pomsg)
{
    byte buf[OMSG_SIZE], *pbuf = buf;
    word rawdate, rawtime;

    memmove(pbuf, pomsg->from, 36);
    pbuf += 36;

    memmove(pbuf, pomsg->to, 36);
    pbuf += 36;

    memmove(pbuf, pomsg->subj, 72);
    pbuf += 72;

    memmove(pbuf, pomsg->date, 20);
    pbuf += 20;

    put_word(pbuf, pomsg->times);
    pbuf += 2;

    put_word(pbuf, pomsg->dest);
    pbuf += 2;

    put_word(pbuf, pomsg->orig);
    pbuf += 2;

    put_word(pbuf, pomsg->cost);
    pbuf += 2;

    put_word(pbuf, pomsg->orig_net);
    pbuf += 2;

    put_word(pbuf, pomsg->dest_net);
    pbuf += 2;

    /* 4 bytes "date_written" */
    rawdate = rawtime = 0;

    rawdate |= (word) (((word) pomsg->date_written.date.da) & 31);
    rawdate |= (word) ((((word) pomsg->date_written.date.mo) & 15) << 5);
    rawdate |= (word) ((((word) pomsg->date_written.date.yr) & 127) << 9);

    rawtime |= (word) (((word) pomsg->date_written.time.ss) & 31);
    rawtime |= (word) ((((word) pomsg->date_written.time.mm) & 63) << 5);
    rawtime |= (word) ((((word) pomsg->date_written.time.hh) & 31) << 11);

    put_word(pbuf, rawdate);
    pbuf += 2;

    put_word(pbuf, rawtime);
    pbuf += 2;

    /* 4 bytes "date_arrvied" */
    rawdate = rawtime = 0;

    rawdate |= (word) (((word) pomsg->date_arrived.date.da) & 31);
    rawdate |= (word) ((((word) pomsg->date_arrived.date.mo) & 15) << 5);
    rawdate |= (word) ((((word) pomsg->date_arrived.date.yr) & 127) << 9);

    rawtime |= (word) (((word) pomsg->date_arrived.time.ss) & 31);
    rawtime |= (word) ((((word) pomsg->date_arrived.time.mm) & 63) << 5);
    rawtime |= (word) ((((word) pomsg->date_arrived.time.hh) & 31) << 11);

    put_word(pbuf, rawdate);
    pbuf += 2;

    put_word(pbuf, rawtime);
    pbuf += 2;

    put_word(pbuf, pomsg->reply);
    pbuf += 2;

    put_word(pbuf, pomsg->attr);
    pbuf += 2;

    put_word(pbuf, pomsg->up);
    pbuf += 2;

    assert(pbuf - buf == OMSG_SIZE);

    return (write(handle, (byte *) buf, OMSG_SIZE) == OMSG_SIZE);
}

static sword SdmWriteMsg(MSGH * msgh, word append, XMSG * msg, byte * text, dword textlen, dword totlen, dword clen, byte * ctxt)
{
    struct _omsg fmsg;
    byte *s;

    unused(totlen);

    if (clen == 0L || ctxt == NULL || *ctxt == '\0')
    {
        ctxt = NULL;
        clen = 0L;
    }

    if (InvalidMsgh(msgh))
    {
        return -1;
    }

    lseek(msgh->fd, 0L, SEEK_SET);

    if (msg)
    {
        Convert_Xmsg_To_Fmsg(msg, &fmsg);

        if (!write_omsg(msgh->fd, &fmsg))
        {
            msgapierr = MERR_NODS;
            return -1;
        }

        if (!append && msgh->clen <= 0 && msgh->zplen == 0 && !msgh->sq->isecho)
        {
            statfd = msgh->fd;
            msgh->zplen = (word) WriteZPInfo(msg, WriteToFd, ctxt);
        }

        /* Use Attributes under BeOS */

#ifdef __BEOS__
        {
            struct tm tmdate;
            time_t ttime;

            fs_write_attr(msgh->fd, "BEOS:TYPE", B_MIME_TYPE, 0l, "message/fmsg", 13);
            fs_write_attr(msgh->fd, "XMSG:FROM", B_STRING_TYPE, 0l, msg->from, strlen(msg->from));
            fs_write_attr(msgh->fd, "XMSG:TO", B_STRING_TYPE, 0l, msg->to, strlen(msg->to));
            fs_write_attr(msgh->fd, "XMSG:SUBJ", B_STRING_TYPE, 0l, msg->subj, strlen(msg->subj));
            ttime = mktime(DosDate_to_TmDate((union stamp_combo *)&msg->date_written, &tmdate));
            fs_write_attr(msgh->fd, "XMSG:DATE", B_TIME_TYPE, 0l, &ttime, 4l);
            ttime = mktime(DosDate_to_TmDate((union stamp_combo *)&msg->date_arrived, &tmdate));
            fs_write_attr(msgh->fd, "XMSG:DTAR", B_TIME_TYPE, 0l, &ttime, 4l);
            /* ... and so on ... not fully implemented ! (Yet) */
        }
#endif
    }
    else if (!append || ctxt)
    {
        /* Skip over old message header */
        lseek(msgh->fd, (dword) OMSG_SIZE + (dword) msgh->zplen, SEEK_SET);
    }

    /* Now write the control info / kludges */

    if (clen && ctxt)
    {
        if (!msg)
        {
            lseek(msgh->fd, (dword) OMSG_SIZE + (dword) msgh->zplen, SEEK_SET);
        }

        s = CvtCtrlToKludge(ctxt);

        if (s)
        {
            unsigned sl_s = (unsigned)strlen((char *)s);
            int ret;

            ret = write(msgh->fd, s, sl_s);
            pfree(s);
            if (ret != (int)sl_s)
            {
                msgapierr = MERR_NODS;
                return -1;
            }
        }
    }

    if (append)
    {
        lseek(msgh->fd, 0L, SEEK_END);
    }

    if (text)
    {
        if (write(msgh->fd, text, (unsigned)textlen) != (signed) textlen)
        {
            msgapierr = MERR_NODS;
            return -1;
        }
        if (text[textlen])
        {
            if (write(msgh->fd, "", 1) != 1)
            {
                msgapierr = MERR_NODS;
                return -1;
            }
        }
    }

    msgapierr = MERR_NONE;
    return 0;
}

static sword SdmKillMsg(MSGA * mh, dword msgnum)
{
    dword hwm;
    byte temp[PATHLEN];
    UMSGID msguid;

    if (InvalidMh(mh))
    {
        return -1;
    }

    if (msgnum > mh->num_msg || msgnum <= 0)
    {
        msgapierr = MERR_NOENT;
        return -1;
    }

    msguid = SdmMsgnToUid(mh, msgnum);

    /* Remove the message number from our private index */

    memmove(Mhd->msgnum + msgnum - 1, Mhd->msgnum + msgnum,
      (int)(mh->num_msg - msgnum) * sizeof(Mhd->msgnum[0]));

    /* If we couldn't find it, return an error message */

    sprintf((char *)temp, (char *)sd_msg, Mhd->base, (unsigned int)msguid);

    if (unlink((char *)temp) == -1)
    {
        msgapierr = MERR_NOENT;
        return -1;
    }

    mh->num_msg--;

    /* Adjust the high message number */

    if (msguid == mh->high_msg)
    {
        if (mh->num_msg)
        {
            mh->high_msg = SdmMsgnToUid(mh, mh->num_msg);
        }
        else
        {
            mh->high_msg = 0;
        }
    }

    /* Now adjust the high-water mark, if necessary */

    hwm = SdmGetHighWater(mh);

    if (hwm != (dword) -1 && hwm > 0 && hwm >= msgnum)
    {
        SdmSetHighWater(mh, msgnum - 1);
    }

    if (mh->cur_msg >= msgnum)
    {
        mh->cur_msg--;
    }

    msgapierr = MERR_NONE;
    return 0;
}

static sword SdmLock(MSGA * mh)
{
    if (InvalidMh(mh))
    {
        return -1;
    }

    msgapierr = MERR_NONE;
    return 0;
}

static sword SdmUnlock(MSGA * mh)
{
    if (InvalidMh(mh))
    {
        return -1;
    }

    msgapierr = MERR_NONE;
    return 0;
}

sword MSGAPI SdmValidate(byte * name)
{
    msgapierr = MERR_NONE;
    return ((sword) (direxist((char *)name) != FALSE));
}

static sword SdmSetCurPos(MSGH * msgh, dword pos)
{
    if (InvalidMsgh(msgh))
    {
        return 0;
    }

    lseek(msgh->fd, msgh->cur_pos = pos, SEEK_SET);

    msgapierr = MERR_NONE;
    return 0;
}

static dword SdmGetCurPos(MSGH * msgh)
{
    if (InvalidMsgh(msgh))
    {
        return (dword) -1;
    }

    msgapierr = MERR_NONE;
    return msgh->cur_pos;
}

static UMSGID SdmMsgnToUid(MSGA * mh, dword msgnum)
{
    if (InvalidMh(mh))
    {
        return (UMSGID) -1;
    }

    msgapierr = MERR_NONE;

    if (msgnum > mh->num_msg)
    {
        return (UMSGID) -1;
    }
    
    if (msgnum <= 0)
    {
        return 0;
    }

    return (UMSGID) Mhd->msgnum[msgnum - 1];
}

static dword SdmUidToMsgn(MSGA * mh, UMSGID umsgid, word type)
{
    dword left, right, new;
    UMSGID umsg;

    if (InvalidMh(mh))
    {
        return (dword) -1;
    }

    if (umsgid <= 0)
    {
        return 0;
    }

    left = 1;
    right = mh->num_msg;

    while (left <= right)
    {
        new = (right + left) / 2;
        umsg = SdmMsgnToUid(mh, new);

        if (umsg == (dword) -1)
        {
            return 0;
        }

        if (umsg < umsgid)
        {
            left = new + 1;
        }
        else if (umsg > umsgid)
        {
            if (new > 0)
            {
                right = new - 1;
            }
            else
            {
                right = 0;
            }
        }
        else
        {
            return new;
        }
    }

    if (type == UID_EXACT)
    {
        return 0;
    }
    if (type == UID_PREV)
    {
        return right;
    }

    return (left > mh->num_msg) ? mh->num_msg : left;
}

static dword SdmGetHighWater(MSGA * mh)
{
    MSGH *msgh;
    XMSG msg;

    if (InvalidMh(mh))
    {
        return (dword) -1;
    }

    /* If we've already fetched the highwater mark... */

    if (mh->high_water != (dword) -1)
    {
        return SdmUidToMsgn(mh, mh->high_water, UID_PREV);
    }

    msgh = SdmOpenMsg(mh, MOPEN_READ, 1L);

    if (msgh == NULL)
    {
        return 0L;
    }

    if (SdmReadMsg(msgh, &msg, 0L, 0L, NULL, 0L, NULL) == (dword) -1 ||
      !eqstr((char *)msg.from, (char *)hwm_from))
    {
        mh->high_water = 0L;
    }
    else
    {
        mh->high_water = (dword) msg.replyto;
    }

    SdmCloseMsg(msgh);

    return SdmUidToMsgn(mh, mh->high_water, UID_PREV);
}

static sword SdmSetHighWater(MSGA * mh, dword hwm)
{
    if (InvalidMh(mh))
    {
        return -1;
    }

    /* 
     *  Only write it to memory for now.  We'll do a complete update of
     *  the real HWM in 1.MSGA only when doing a MsgCloseArea(), to save
     *  time.
     */

    if (hwm != mh->high_water)
    {
        Mhd->hwm_chgd = TRUE;
    }

    mh->high_water = hwm;
    return 0;
}

static dword SdmGetTextLen(MSGH * msgh)
{
    dword pos, end;

    /* Figure out the physical length of the message */

    if (msgh->msg_len == -1)
    {
        pos = (dword) tell(msgh->fd);
        end = lseek(msgh->fd, 0L, SEEK_END);

        if (end < OMSG_SIZE)
        {
            msgh->msg_len = 0L;
        }
        else
        {
            msgh->msg_len = end - (dword) OMSG_SIZE;
        }

        lseek(msgh->fd, pos, SEEK_SET);
    }

    /* If we've already figured out the length of the control info */

    if (msgh->clen == -1 && _Grab_Clen(msgh) == -1)
    {
        return 0;
    }
    else
    {
        return (dword) (msgh->msg_len - msgh->msgtxt_start);
    }
}

static dword SdmGetCtrlLen(MSGH * msgh)
{
    /* If we've already figured out the length of the control info */

    if (msgh->clen == -1 && _Grab_Clen(msgh) == -1)
    {
        return 0;
    }
    else
    {
        return (dword) msgh->clen;
    }
}

static sword _Grab_Clen(MSGH * msgh)
{
    if ((sdword) SdmReadMsg(msgh, NULL, 0L, 0L, NULL, 0L, NULL) < (sdword) 0)
    {
        return (sword) -1;
    }
    else
    {
        return (sword) 0;
    }
}

static sword _SdmRescanArea(MSGA * mh)
{
    FFIND *ff;
    char *temp;
    word mn, thismsg;

    mh->num_msg = 0;

    Mhd->msgnum = palloc(SDM_BLOCK * sizeof(unsigned));

    if (Mhd->msgnum == NULL)
    {
        msgapierr = MERR_NOMEM;
        return FALSE;
    }

    Mhd->msgnum_len = SDM_BLOCK;

    temp = malloc(strlen((char *)Mhd->base) + 6);
    sprintf(temp, "%s*.msg", Mhd->base);

    ff = FFindOpen(temp, 0);

    free(temp);

    if (ff != 0)
    {
        mn = 0;

        do
        {
            /* Don't count zero-length or invalid messages */

#ifndef __UNIX__
            if (ff->ff_fsize < OMSG_SIZE)
            {
                continue;
            }
#endif

            if (mn >= Mhd->msgnum_len)
            {
                Mhd->msgnum_len += (word) SDM_BLOCK;
                Mhd->msgnum = realloc(Mhd->msgnum,
                  Mhd->msgnum_len * sizeof(unsigned));

                if (!Mhd->msgnum)
                {
                    msgapierr = MERR_NOMEM;
                    return FALSE;
                }
            }

            thismsg = (word) atoi(ff->ff_name);

            if (thismsg != 0)
            {
                Mhd->msgnum[mn++] = thismsg;

                if ((dword) thismsg > mh->high_msg)
                {
                    mh->high_msg = (dword) thismsg;
                }

                mh->num_msg = (dword) mn;
            }

            if ((mn % 128) == 127)
            {
                tdelay(1);     /* give up cpu */
            }
        }
        while (FFindNext(ff) == 0);

        FFindClose(ff);

        /* Now sort the list of messages */

        qksort((int *)Mhd->msgnum, (word) mh->num_msg);
    }

    return TRUE;
}

static void MSGAPI Init_Xmsg(XMSG * msg)
{
    memset(msg, '\0', sizeof(XMSG));
}

static void MSGAPI Convert_Fmsg_To_Xmsg(struct _omsg *fmsg, XMSG * msg,
  word def_zone)
{
    NETADDR *orig, *dest;

    Init_Xmsg(msg);

    orig = &msg->orig;
    dest = &msg->dest;

    fmsg->to[sizeof(fmsg->to) - 1] = '\0';
    fmsg->from[sizeof(fmsg->from) - 1] = '\0';
    fmsg->subj[sizeof(fmsg->subj) - 1] = '\0';
    fmsg->date[sizeof(fmsg->date) - 1] = '\0';

    strcpy((char *)msg->from, (char *)fmsg->from);
    strcpy((char *)msg->to, (char *)fmsg->to);
    strcpy((char *)msg->subj, (char *)fmsg->subj);

    orig->zone = dest->zone = def_zone;
    orig->point = dest->point = 0;

    orig->net = fmsg->orig_net;
    orig->node = fmsg->orig;

    dest->net = fmsg->dest_net;
    dest->node = fmsg->dest;

    Get_Binary_Date(&msg->date_written, &fmsg->date_written, fmsg->date);
    Get_Binary_Date(&msg->date_arrived, &fmsg->date_arrived, fmsg->date);

    strcpy((char *)msg->__ftsc_date, (char *)fmsg->date);

    msg->utc_ofs = 0;

    msg->replyto = fmsg->reply;
    msg->replies[0] = fmsg->up;
    msg->attr = (dword) fmsg->attr;
    msg->xmtimesread = fmsg->times;
    msg->xmcost = fmsg->cost;

    /* Convert 4D pointnets */

    if (fmsg->times == ~fmsg->cost && fmsg->times)
    {
        msg->orig.point = fmsg->times;
    }
}

static void MSGAPI Convert_Xmsg_To_Fmsg(XMSG * msg, struct _omsg *fmsg)
{
    NETADDR *orig, *dest;

    memset(fmsg, '\0', sizeof(struct _omsg));

    orig = &msg->orig;
    dest = &msg->dest;

    strncpy((char *)fmsg->from, (char *)msg->from, sizeof(fmsg->from));
    strncpy((char *)fmsg->to, (char *)msg->to, sizeof(fmsg->to));
    strncpy((char *)fmsg->subj, (char *)msg->subj, sizeof(fmsg->subj));

    fmsg->from[sizeof(fmsg->from) - 1] = '\0';
    fmsg->to[sizeof(fmsg->to) - 1] = '\0';
    fmsg->subj[sizeof(fmsg->subj) - 1] = '\0';

    fmsg->orig_net = orig->net;
    fmsg->orig = orig->node;

    fmsg->dest_net = dest->net;
    fmsg->dest = dest->node;

    if (*msg->__ftsc_date)
    {
        strncpy((char *)fmsg->date, (char *)msg->__ftsc_date,
          sizeof(fmsg->date));
        fmsg->date[sizeof(fmsg->date) - 1] = '\0';
    }
    else
    {
        sprintf((char *)fmsg->date, "%02d %s %02d  %02d:%02d:%02d",
          msg->date_written.date.da ? msg->date_written.date.da : 1,
          months_ab[msg->date_written.date.mo ? msg->date_written.date.mo - 1 : 0],
        (msg->date_written.date.yr + 80) % 100, msg->date_written.time.hh,
          msg->date_written.time.mm, msg->date_written.time.ss << 1);
    }

    fmsg->date_written = msg->date_written;
    fmsg->date_arrived = msg->date_arrived;

    fmsg->reply = (word) msg->replyto;
    fmsg->up = (word) msg->replies[0];
    fmsg->attr = (word) (msg->attr & 0xffffL);
    fmsg->times = (word) msg->xmtimesread;
    fmsg->cost = (word) msg->xmcost;

    /* 
     *  Non-standard point kludge to ensure that 4D pointmail works
     *  correctly.
     */

    if (orig->point)
    {
        fmsg->times = orig->point;
        fmsg->cost = (word) ~ fmsg->times;
    }
}

int WriteZPInfo(XMSG * msg, void (*wfunc) (byte * str), byte * kludges)
{
    byte temp[PATHLEN], *null = (byte *) "";
    int bytes = 0;

    if (!kludges)
    {
        kludges = null;
    }

    if ((msg->dest.zone != mi.def_zone || msg->orig.zone != mi.def_zone) &&
      !strstr((char *)kludges, "\001INTL"))
    {
        sprintf((char *)temp, "\001INTL %hu:%hu/%hu %hu:%hu/%hu\r", msg->dest.zone, msg->dest.net,
          msg->dest.node, msg->orig.zone, msg->orig.net, msg->orig.node);

        (*wfunc) (temp);
        bytes += strlen((char *)temp);
    }

    if (msg->orig.point && !strstr((char *)kludges, "\001" "FMPT"))
    {
        sprintf((char *)temp, "\001" "FMPT %hu\r", msg->orig.point);
        (*wfunc) (temp);
        bytes += strlen((char *)temp);
    }

    if (msg->dest.point && !strstr((char *)kludges, "\001" "TOPT"))
    {
        sprintf((char *)temp, "\001" "TOPT %hu\r", msg->dest.point);
        (*wfunc) (temp);
        bytes += strlen((char *)temp);
    }

    return bytes;
}

static void WriteToFd(byte * str)
{
    write(statfd, str, strlen((char *)str));
}

static void Get_Binary_Date(struct _stamp *todate, struct _stamp *fromdate, byte * asciidate)
{
    if (fromdate->date.da == 0 || fromdate->date.da > 31 ||
      fromdate->date.yr > 50 || fromdate->time.hh > 23 ||
      fromdate->time.mm > 59 || fromdate->time.ss > 59 ||
      ((union stamp_combo *)&fromdate)->ldate == 0)
    {
        ASCII_Date_To_Binary((char *)asciidate, (union stamp_combo *)todate);
    }
    else
    {
        *todate = *fromdate;
    }
}

static dword SdmGetHash(HAREA mh, dword msgnum)
{
    XMSG xmsg;
    HMSG msgh;
    dword rc = 0l;

    msgh = SdmOpenMsg(mh, MOPEN_READ, msgnum);
    
    if (msgh == NULL)
    {
        return (dword) 0l;
    }

    if (SdmReadMsg(msgh, &xmsg, 0L, 0L, NULL, 0L, NULL) != (dword) -1)
    {
        rc = SquishHash(xmsg.to) | (xmsg.attr & MSGREAD) ? 0x80000000l : 0;
    }

    SdmCloseMsg(msgh);

    msgapierr = MERR_NONE;
    return rc;
}

static UMSGID SdmGetNextUid(HAREA ha)
{
    if (InvalidMh(ha))
    {
        return 0L;
    }

    if (!ha->locked)
    {
        msgapierr = MERR_NOLOCK;
        return 0L;
    }

    msgapierr = MERR_NONE;
    return ha->high_msg + 1;
}

#endif
