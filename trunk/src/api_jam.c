/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 *
 *  api_jam.c
 *
 *  JAM messagebase access routines.
 *
 *  Adapted for MsgAPI by Fedor Lizunkov, 2:5020/960@fidonet.
 */

#ifndef MSGAPI_NO_JAM
 
#define MSGAPI_HANDLERS

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __UNIX__
#include <unistd.h>
#else
#include <io.h>
#include <share.h>
#endif

#include "ffind.h"
#include "prog.h"
#include "stamp.h"
#include "msgapi.h"
#include "progprot.h"
#include "api_jam.h"
#include "unused.h"

static sword JamCloseArea(MSGA * jm);
static MSGH *JamOpenMsg(MSGA * jm, word mode, dword msgnum);
static sword JamCloseMsg(MSGH * msgh);
static dword JamReadMsg(MSGH * msgh, XMSG * msg, dword offset,
  dword bytes, byte * text, dword clen, byte * ctxt);
static sword JamWriteMsg(MSGH * msgh, word append, XMSG * msg,
  byte * text, dword textlen, dword totlen, dword clen, byte * ctxt);
static sword JamKillMsg(MSGA * jm, dword msgnum);
static sword JamLock(MSGA * jm);
static sword JamUnlock(MSGA * jm);
static sword JamSetCurPos(MSGH * msgh, dword pos);
static dword JamGetCurPos(MSGH * msgh);
static UMSGID JamMsgnToUid(MSGA * jm, dword msgnum);
static dword JamUidToMsgn(MSGA * jm, UMSGID umsgid, word type);
static dword JamGetHighWater(MSGA * jm);
static sword JamSetHighWater(MSGA * sq, dword hwm);
static dword JamGetTextLen(MSGH * msgh);
static dword JamGetCtrlLen(MSGH * msgh);
static UMSGID JamGetNextUid(HAREA ha);
static dword JamGetHash(HAREA mh, dword msgnum);

static struct _apifuncs jm_funcs =
{
    JamCloseArea,
    JamOpenMsg,
    JamCloseMsg,
    JamReadMsg,
    JamWriteMsg,
    JamKillMsg,
    JamLock,
    JamUnlock,
    JamSetCurPos,
    JamGetCurPos,
    JamMsgnToUid,
    JamUidToMsgn,
    JamGetHighWater,
    JamSetHighWater,
    JamGetTextLen,
    JamGetCtrlLen,
    JamGetNextUid,
    JamGetHash
};

struct _msgh
{
    MSGA *sq;
    dword id;                   /* Must always equal MSGH_ID */

    dword bytes_written;
    dword cur_pos;

    /* For JAM only! */

    JAMIDXREC Idx;              /* Message index */
    JAMHDR Hdr;                 /* Message header */
    JAMSUBFIELD2LISTptr SubFieldPtr;  /* Pointer to Subfield structure */

    dword seek_idx;
    dword seek_hdr;

    dword clen;
    byte *ctrl;
    dword lclen;
    byte *lctrl;
    dword msgnum;

    word mode;
};

#define fop_wpb (O_CREAT | O_TRUNC | O_RDWR | O_BINARY)
#define fop_rpb (O_RDWR | O_BINARY)
#define fop_cpb (O_CREAT | O_EXCL | O_RDWR | O_BINARY)

static MSGH *Jam_OpenMsg(MSGA * jm, word mode, dword msgnum);
static sword MSGAPI Jam_OpenBase(MSGA * jm, word * mode,
  unsigned char *basename);
static int Jam_Lock(MSGA * jm, int force);
static void Jam_Unlock(MSGA * jm);
static dword Jam_JamAttrToMsg(MSGH * msgh);
static int Jam_OpenFile(JAMBASE * jambase, word * mode, mode_t permissions);
static void Jam_CloseFile(JAMBASE * jambase);
static JAMSUBFIELD2ptr Jam_GetSubField(struct _msgh *msgh, dword * SubPos, word what);
static dword Jam_HighMsg(JAMBASEptr jambase);
static void Jam_ActiveMsgs(JAMBASEptr jambase);
static dword Jam_PosHdrMsg(MSGA * jm, dword msgnum, JAMIDXREC * jamidx, JAMHDR * jamhdr);
static sword Jam_WriteHdrInfo(JAMBASEptr jambase);
static dword Jam_Crc32(unsigned char *buff, dword len);
static void MSGAPI ConvertXmsgToJamHdr(MSGH * msgh, XMSG * msg,  JAMHDRptr jamhdr, JAMSUBFIELD2LISTptr * subfield);
static void MSGAPI ConvertCtrlToSubf(JAMHDRptr jamhdr,  JAMSUBFIELD2LISTptr * subfield, dword clen, unsigned char *ctxt);
static unsigned char *DelimText(JAMHDRptr jamhdr,  JAMSUBFIELD2LISTptr * subfield, unsigned char *text, size_t textlen);
static void parseAddr(NETADDR * netAddr, unsigned char *str, dword len);
static void DecodeSubf(MSGH * msgh);

#define Jmd ((JAMBASE *)(jm->apidata))
#define MsghJm ((JAMBASE *)(((struct _msgh *)msgh)->sq->apidata))

#ifdef __TURBOC__
#pragma warn -pia*
#pragma warn -ucp
#pragma warn -sig
#endif

#define NOTH 3

#define MAXHDRINCORE  (1024*1024*10)  /* Maximum jam hdr size for incore, 10M */

static int write_hdr(int handle, JAMHDR * Hdr)
{
    byte buf[HDR_SIZE], *pbuf = buf;

    /* 04 bytes Signature */
    memmove(pbuf, Hdr->Signature, (size_t) 4);
    pbuf += 4;

    /* 02 bytes Revision */
    put_word(pbuf, Hdr->Revision);
    pbuf += 2;

    /* 02 bytes ReservedWord */
    put_word(pbuf, Hdr->ReservedWord);
    pbuf += 2;

    /* 04 bytes SubfieldLen */
    put_dword(pbuf, Hdr->SubfieldLen);
    pbuf += 4;

    /* 04 bytes TimesRead */
    put_dword(pbuf, Hdr->TimesRead);
    pbuf += 4;

    /* 04 bytes MsgIdCRC */
    put_dword(pbuf, Hdr->MsgIdCRC);
    pbuf += 4;

    /* 04 bytes ReplyCRC */
    put_dword(pbuf, Hdr->ReplyCRC);
    pbuf += 4;

    /* 04 bytes ReplyTo */
    put_dword(pbuf, Hdr->ReplyTo);
    pbuf += 4;

    /* 04 bytes Reply1st */
    put_dword(pbuf, Hdr->Reply1st);
    pbuf += 4;

    /* 04 bytes ReplyNext */
    put_dword(pbuf, Hdr->ReplyNext);
    pbuf += 4;

    /* 04 bytes DateWritten */
    put_dword(pbuf, Hdr->DateWritten);
    pbuf += 4;

    /* 04 bytes DateReceived */
    put_dword(pbuf, Hdr->DateReceived);
    pbuf += 4;

    /* 04 bytes DateProcessed */
    put_dword(pbuf, Hdr->DateProcessed);
    pbuf += 4;

    /* 04 bytes MsgNum */
    put_dword(pbuf, Hdr->MsgNum);
    pbuf += 4;

    /* 04 bytes Attribute */
    put_dword(pbuf, Hdr->Attribute);
    pbuf += 4;

    /* 04 bytes Attribute2 */
    put_dword(pbuf, Hdr->Attribute2);
    pbuf += 4;

    /* 04 bytes TxtOffset */
    put_dword(pbuf, Hdr->TxtOffset);
    pbuf += 4;

    /* 04 bytes TxtLen */
    put_dword(pbuf, Hdr->TxtLen);
    pbuf += 4;

    /* 04 bytes PasswordCRC */
    put_dword(pbuf, Hdr->PasswordCRC);
    pbuf += 4;

    /* 04 bytes Cost */
    put_dword(pbuf, Hdr->Cost);
    pbuf += 4;

    assert(pbuf - buf == HDR_SIZE);

    return (write(handle, (byte *) buf, HDR_SIZE) == HDR_SIZE);
}

static int write_idx(int handle, JAMIDXREC * Idx)
{
    byte buf[IDX_SIZE], *pbuf = buf;

    /* 04 bytes UserCRC */
    put_dword(pbuf, Idx->UserCRC);
    pbuf += 4;

    /* 04 bytes HdrOffset */
    put_dword(pbuf, Idx->HdrOffset);
    pbuf += 4;

    assert(pbuf - buf == IDX_SIZE);

    return (write(handle, (byte *) buf, IDX_SIZE) == IDX_SIZE);
}

static int copy_subfield(JAMSUBFIELD2LISTptr * to, JAMSUBFIELD2LISTptr from)
{
    dword i;

    *to = palloc(from->arraySize);
    if (*to == NULL)
        return 1;
    memcpy(*to, from, from->arraySize);
    for (i = 0; i < from->subfieldCount; i++)
        to[0]->subfield[i].Buffer += ((char *)*to - (char *)from);
    return 0;
}

static int write_subfield(int handle, JAMSUBFIELD2LISTptr * subfield, dword SubfieldLen)
{
    unsigned char *buf, *pbuf;
    dword datlen;
    int rc;
    dword i;
    JAMSUBFIELD2ptr subfieldNext;

    buf = (unsigned char *)palloc(SubfieldLen);
    pbuf = buf;
    subfieldNext = &(subfield[0]->subfield[0]);

    for (i = 0; i < subfield[0]->subfieldCount; i++, subfieldNext++)
    {
        /* 02 bytes LoID */
        put_word(pbuf, subfieldNext->LoID);
        pbuf += 2;
        /* 02 bytes HiID */
        put_word(pbuf, subfieldNext->HiID);
        pbuf += 2;
        /* 04 bytes DatLen */
        put_dword(pbuf, subfieldNext->DatLen);

        datlen = subfieldNext->DatLen;
        pbuf += 4;

        /* DatLen bytes Buffer */

        memmove(pbuf, subfieldNext->Buffer, datlen);
        pbuf += datlen;
    }

    rc = ((dword) write(handle, (byte *) buf, SubfieldLen) == SubfieldLen);

    pfree(buf);

    return rc;
}

static int read_hdrinfo(int handle, JAMHDRINFO * HdrInfo)
{
    byte buf[HDRINFO_SIZE], *pbuf = buf;

    if (read(handle, (byte *) buf, HDRINFO_SIZE) != HDRINFO_SIZE)
    {
        return 0;
    }

    /* 04 bytes Signature */
    memmove(HdrInfo->Signature, pbuf, (size_t) 4);
    pbuf += 4;

    /* 04 bytes DateCreated */
    HdrInfo->DateCreated = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes ModCounter */
    HdrInfo->ModCounter = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes ActiveMsgs */
    HdrInfo->ActiveMsgs = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes PasswordCRC */
    HdrInfo->PasswordCRC = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes BaseMsgNum */
    HdrInfo->BaseMsgNum = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes highwater */
    HdrInfo->highwater = get_dword(pbuf);
    pbuf += 4;

    /* 996 bytes RSRVD */
    memmove(HdrInfo->RSRVD, pbuf, (size_t) 996);
    pbuf += 996;

    assert(pbuf - buf == HDRINFO_SIZE);

    return 1;
}

static int write_hdrinfo(int handle, JAMHDRINFO * HdrInfo)
{
    byte buf[HDRINFO_SIZE], *pbuf = buf;

    /* 04 bytes Signature */
    memmove(pbuf, HdrInfo->Signature, (size_t) 4);
    pbuf += 4;

    /* 04 bytes DateCreated */
    put_dword(pbuf, HdrInfo->DateCreated);
    pbuf += 4;

    /* 04 bytes ModCounter */
    put_dword(pbuf, HdrInfo->ModCounter);
    pbuf += 4;

    /* 04 bytes ActiveMsgs */
    put_dword(pbuf, HdrInfo->ActiveMsgs);
    pbuf += 4;

    /* 04 bytes PasswordCRC */
    put_dword(pbuf, HdrInfo->PasswordCRC);
    pbuf += 4;

    /* 04 bytes BaseMsgNum */
    put_dword(pbuf, HdrInfo->BaseMsgNum);
    pbuf += 4;

    /* 04 bytes highwater */
    put_dword(pbuf, HdrInfo->highwater);
    pbuf += 4;

    /* 996 bytes RSRVD */
    memmove(pbuf, HdrInfo->RSRVD, (size_t) 996);
    pbuf += 996;

    assert(pbuf - buf == HDRINFO_SIZE);

    return (write(handle, (byte *) buf, HDRINFO_SIZE) == HDRINFO_SIZE);
}

static void decode_subfield(byte * buf, JAMSUBFIELD2LISTptr * subfield, dword * SubfieldLen)
{
    JAMSUBFIELD2ptr subfieldNext;
    dword datlen;
    int i, len;
    byte *pbuf;

    pbuf = buf;
    i = 0;
    while ((dword) (pbuf - buf + 8) < *SubfieldLen)
    {
        i++;
        pbuf += get_dword(pbuf + 4) + sizeof(JAMBINSUBFIELD);
    }
    len = sizeof(JAMSUBFIELD2LIST) + i * (sizeof(JAMSUBFIELD2) - sizeof(JAMBINSUBFIELD) + 1) + *SubfieldLen;
    *subfield = palloc(len);
    subfield[0]->arraySize = len;
    subfield[0]->subfieldCount = 0;
    subfield[0]->subfield[0].Buffer = (byte *) & (subfield[0]->subfield[i + 1]);

    subfieldNext = subfield[0]->subfield;
    pbuf = buf;

    while ((dword) (pbuf - buf + 8) < *SubfieldLen)
    {
        /* 02 bytes LoID */
        subfieldNext->LoID = get_word(pbuf);
        pbuf += 2;

        /* 02 bytes HiID */
        subfieldNext->HiID = get_word(pbuf);
        pbuf += 2;

        /* 04 bytes DatLen */
        subfieldNext->DatLen = 0;
        subfieldNext->Buffer[0] = '\0';
        datlen = get_dword(pbuf);
        pbuf += 4;

        subfield[0]->subfieldCount++;

        if (pbuf - buf + datlen > *SubfieldLen)
            break;
        /* DatLen bytes Buffer */
        if ((long)datlen >= 0 && datlen < *SubfieldLen)
        {
            subfieldNext->DatLen = datlen;
            memmove(subfieldNext->Buffer, pbuf, datlen);
            subfieldNext[1].Buffer = subfieldNext->Buffer + subfieldNext->DatLen + 1;
            subfieldNext++;
            assert((byte *) (subfieldNext + 1) <= subfield[0]->subfield[0].Buffer);
            assert(subfieldNext->Buffer <= (byte *) * subfield + subfield[0]->arraySize);
        }
        else
            break;
        pbuf += datlen;

    }                           /* endwhile */

    *SubfieldLen = pbuf - buf;
}

static int read_subfield(int handle, JAMSUBFIELD2LISTptr * subfield, dword * SubfieldLen)
{
    char *buf;

    buf = (char *)palloc(*SubfieldLen);

    if ((dword) read(handle, (byte *) buf, *SubfieldLen) != *SubfieldLen)
    {
        pfree(buf);
        return 0;
    }

    decode_subfield((byte *) buf, subfield, SubfieldLen);

    pfree(buf);

    return 1;
}

static void decode_hdr(byte * pbuf, JAMHDR * Hdr)
{
    /* 04 bytes Signature */
    memmove(Hdr->Signature, pbuf, (size_t) 4);
    pbuf += 4;

    /* 02 bytes Revision */
    Hdr->Revision = get_word(pbuf);
    pbuf += 2;

    /* 02 bytes ReservedWord */
    Hdr->ReservedWord = get_word(pbuf);
    pbuf += 2;

    /* 04 bytes SubfieldLen */
    Hdr->SubfieldLen = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes TimesRead */
    Hdr->TimesRead = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes MsgIdCRC */
    Hdr->MsgIdCRC = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes ReplyCRC */
    Hdr->ReplyCRC = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes ReplyTo */
    Hdr->ReplyTo = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes Reply1st */
    Hdr->Reply1st = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes ReplyNext */
    Hdr->ReplyNext = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes DateWritten */
    Hdr->DateWritten = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes DateReceived */
    Hdr->DateReceived = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes DateProcessed */
    Hdr->DateProcessed = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes MsgNum */
    Hdr->MsgNum = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes Attribute */
    Hdr->Attribute = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes Attribute2 */
    Hdr->Attribute2 = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes TxtOffset */
    Hdr->TxtOffset = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes TxtLen */
    Hdr->TxtLen = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes PasswordCRC */
    Hdr->PasswordCRC = get_dword(pbuf);
    pbuf += 4;

    /* 04 bytes Cost */
    Hdr->Cost = get_dword(pbuf);
}

static int read_hdr(int handle, JAMHDR * Hdr)
{
    byte buf[HDR_SIZE];

    if (read(handle, (byte *) buf, HDR_SIZE) != HDR_SIZE)
    {
        return 0;
    }

    decode_hdr(buf, Hdr);

    return 1;
}

static int read_allidx(JAMBASEptr jmb)
{
    byte *buf, *pbuf, *hdrbuf = NULL;
    JAMACTMSGptr newptr;
    JAMHDR hbuf;
    int len;
    dword i, allocated, hlen;
    dword offset;

    lseek(jmb->IdxHandle, 0, SEEK_END);
    len = tell(jmb->IdxHandle);
    lseek(jmb->IdxHandle, 0, SEEK_SET);

    buf = (byte *) palloc(len);
    pbuf = buf;

    if (read(jmb->IdxHandle, (byte *) buf, len) != len)
    {
        pfree(buf);
        return 0;
    }

    lseek(jmb->HdrHandle, 0, SEEK_END);
    hlen = tell(jmb->HdrHandle);
    lseek(jmb->HdrHandle, 0, SEEK_SET);
    if (hlen < MAXHDRINCORE)
    {
        /* read all headers in core */
        hdrbuf = (byte *) palloc(hlen);

        if ((dword) read(jmb->HdrHandle, (byte *) hdrbuf, hlen) != hlen)
        {
            pfree(hdrbuf);
            pfree(buf);
            return 0;
        }

        jmb->actmsg_read = 1;
    }
    else
    {
        jmb->actmsg_read = 2;
    }
    
    allocated = jmb->HdrInfo.ActiveMsgs;
    if (allocated)
    {
        jmb->actmsg = (JAMACTMSGptr) malloc(allocated * sizeof(JAMACTMSG));
        if (jmb->actmsg == NULL)
        {
            if (hdrbuf)
                pfree(hdrbuf);
            pfree(buf);
            return 0;
        }
    }

    for (i = 0; (pbuf - buf) < len;)
    {
        offset = get_dword(pbuf + 4);
        if (offset != 0xFFFFFFFFUL)
        {
            if (offset + HDR_SIZE <= hlen)
            {
                if (hdrbuf)
                    decode_hdr(hdrbuf + offset, &hbuf);
                else
                {
                    lseek(jmb->HdrHandle, offset, SEEK_SET);
                    read_hdr(jmb->HdrHandle, &hbuf);
                }
                if (!(hbuf.Attribute & JMSG_DELETED))
                {
                    if (i >= allocated)
                    {
                        newptr = (JAMACTMSGptr) realloc(jmb->actmsg, sizeof(JAMACTMSG) * (allocated += 16));
                        if (newptr == NULL)
                        {
                            pfree(jmb->actmsg);
                            if (hdrbuf)
                                pfree(hdrbuf);
                            pfree(buf);
                            return 0;
                        }
                        jmb->actmsg = newptr;
                    }
                    jmb->actmsg[i].IdxOffset = pbuf - buf;
                    jmb->actmsg[i].TrueMsg = offset;
                    jmb->actmsg[i].UserCRC = get_dword(pbuf);
                    memcpy(&(jmb->actmsg[i].hdr), &hbuf, sizeof(hbuf));
                    if (hdrbuf && offset + HDR_SIZE + jmb->actmsg[i].hdr.SubfieldLen <= hlen)
                    {
                        decode_subfield(hdrbuf + offset + HDR_SIZE, &(jmb->actmsg[i].subfield), &(jmb->actmsg[i].hdr.SubfieldLen));
                        i++;
                    }
                    else
                        jmb->actmsg[i++].subfield = NULL;
                }
            }
        }
        pbuf += 8;
    }

    pfree(buf);

    if (hdrbuf)
    {
        pfree(hdrbuf);
    }

    if (i != jmb->HdrInfo.ActiveMsgs)
    {
        /* warning: database corrupted! */
        jmb->HdrInfo.ActiveMsgs = i;
        if (i != allocated)
        {
            newptr = (JAMACTMSGptr) realloc(jmb->actmsg, sizeof(JAMACTMSG) * i);
            if (newptr)
                jmb->actmsg = newptr;
        }
    }

    return 1;
}

/* Free's up a SubField-Chain */

static void freejamsubfield(JAMSUBFIELD2LIST * subfield)
{
    if (subfield)
        pfree(subfield);
}

MSGA *MSGAPI JamOpenArea(byte * name, word mode, word type)
{
    MSGA *jm;

    jm = palloc(sizeof(MSGA));
    if (jm == NULL)
    {
        msgapierr = MERR_NOMEM;
        return NULL;
    }

    memset(jm, '\0', sizeof(MSGA));

    jm->id = MSGAPI_ID;

    if (type & MSGTYPE_ECHO)
    {
        jm->isecho = TRUE;
    }

    if (type & MSGTYPE_NOTH)
        jm->isecho = NOTH;

    jm->api = (struct _apifuncs *)palloc(sizeof(struct _apifuncs));
    if (jm->api == NULL)
    {
        msgapierr = MERR_NOMEM;
        pfree(jm);
        return NULL;
    }

    memset(jm->api, '\0', sizeof(struct _apifuncs));

    jm->apidata = (void *)palloc(sizeof(JAMBASE));

    if (jm->apidata == NULL)
    {
        msgapierr = MERR_NOMEM;
        pfree(jm->api);
        pfree(jm);
        return NULL;
    }

    memset((byte *) jm->apidata, '\0', sizeof(JAMBASE));

    jm->len = sizeof(MSGA);

    jm->num_msg = 0;
    jm->high_msg = 0;
    jm->high_water = (dword) -1;

    if (!Jam_OpenBase(jm, &mode, name))
    {
        pfree(jm->api);
        pfree((char *)jm->apidata);
        pfree(jm);
        msgapierr = MERR_BADF;
        return NULL;
    }

    jm->high_water = Jmd->HdrInfo.highwater;
    jm->high_msg = Jam_HighMsg(Jmd);
    jm->num_msg = Jmd->HdrInfo.ActiveMsgs;

    jm->type = MSGTYPE_JAM;

    jm->sz_xmsg = sizeof(XMSG);
    *jm->api = jm_funcs;

    msgapierr = 0;
    return jm;
}

static sword JamCloseArea(MSGA * jm)
{
    dword i;

    if (InvalidMh(jm))
    {
        return -1;
    }

    if (Jmd->msgs_open)
    {
        msgapierr = MERR_EOPEN;
        return -1;
    }

    if (Jmd->modified || Jmd->HdrInfo.highwater != jm->high_water)
    {
        Jmd->HdrInfo.highwater = jm->high_water;
        Jmd->HdrInfo.ModCounter++;
        Jam_WriteHdrInfo(Jmd);
    }

    if (jm->locked)
        JamUnlock(jm);

    Jam_CloseFile(Jmd);

    pfree(Jmd->BaseName);
    if (Jmd->actmsg)
    {
        for (i = 0; i < Jmd->HdrInfo.ActiveMsgs; i++)
            freejamsubfield(Jmd->actmsg[i].subfield);
        pfree(Jmd->actmsg);
    }
    pfree(jm->api);
    pfree((char *)jm->apidata);
    jm->id = 0L;
    pfree(jm);

    return 0;
}

static MSGH *JamOpenMsg(MSGA * jm, word mode, dword msgnum)
{
    struct _msgh *msgh;

    if (InvalidMh(jm))
    {
        return NULL;
    }

    if (mode == MOPEN_CREATE)
    {
        if ((sdword) msgnum < 0 || msgnum > jm->num_msg)
        {
            msgapierr = MERR_NOENT;
            return NULL;
        }

        if (msgnum != 0)
        {
            msgh = Jam_OpenMsg(jm, mode, msgnum);

            if (msgh == NULL)
            {
                msgapierr = MERR_NOENT;
                return NULL;
            }
        }
        else
        {
            msgh = palloc(sizeof(struct _msgh));
            if (msgh == NULL)
            {
                msgapierr = MERR_NOMEM;
                return NULL;
            }

            memset(msgh, '\0', sizeof(struct _msgh));

            msgh->sq = jm;
            msgh->bytes_written = 0L;
            msgh->cur_pos = 0L;
            msgh->msgnum = msgnum;
            msgh->Hdr.TxtLen = 0;

        }                       

    }
    else if (msgnum == 0)
    {
        msgapierr = MERR_NOENT;
        return NULL;
    }
    else
    {
        msgh = Jam_OpenMsg(jm, mode, msgnum);
        if (msgh == NULL)
        {
            msgapierr = MERR_NOENT;
            return NULL;
        }
    }

    msgh->mode = mode;
    msgh->id = MSGH_ID;

    MsghJm->msgs_open++;

    return (MSGH *) msgh;
}

static sword JamCloseMsg(MSGH * msgh)
{
    if (InvalidMsgh(msgh))
    {
        return -1;
    }


    MsghJm->msgs_open--;

    /* Fill the message out to the length that the app said it would be */

    msgh->id = 0L;
    pfree(msgh->ctrl);
    pfree(msgh->lctrl);
    pfree(msgh->SubFieldPtr);
    pfree(msgh);

    return 0;
}

static int openfilejm(char *name, word mode, mode_t permissions)
{
    return sopen(name, mode, SH_DENYNONE, permissions);
}

static int opencreatefilejm(char *name, word mode, mode_t permissions)
{
    int hF = sopen(name, mode, SH_DENYNONE, permissions);

    if ((hF == -1) && (mode & O_CREAT) && (errno == ENOENT))
    {
        char *slash = strrchr(name, PATH_DELIM);
        if (slash)
        {
            *slash = '\0';
            _createDirectoryTree(name);
            *slash = PATH_DELIM;
        }
        hF = sopen(name, mode, SH_DENYNONE, permissions);
    }
    return hF;
}

static int Jam_OpenTxtFile(JAMBASE * jambase)
{
    char *txt;

    txt = (char *)palloc(strlen((char *) jambase->BaseName) + 5);
    strcpy(txt, (char *) jambase->BaseName);
    strcat(txt, EXT_TXTFILE);
    if (jambase->mode == MSGAREA_CREATE)
        jambase->TxtHandle = openfilejm(txt, fop_wpb, jambase->permissions);
    else
        jambase->TxtHandle = openfilejm(txt, fop_rpb, jambase->permissions);
    if ((jambase->TxtHandle == -1) && (jambase->mode == MSGAREA_CRIFNEC))
    {
        jambase->mode = MSGAREA_CREATE;
        jambase->TxtHandle = opencreatefilejm(txt, fop_cpb, jambase->permissions);
    }
    pfree(txt);
    if (jambase->TxtHandle == -1)
    {
        Jam_CloseFile(jambase);
        msgapierr = MERR_NOENT;
        return 0;
    }
    return 1;
}

static dword JamReadMsg(MSGH * msgh, XMSG * msg, dword offset, dword bytes, byte * text, dword clen, byte * ctxt)
{
    JAMSUBFIELD2ptr SubField;
    dword SubPos, bytesread;
    struct tm *s_time;
    SCOMBO *scombo;

    if (InvalidMsgh(msgh))
    {
        return (dword) -1;
    }

    if (msgh->Hdr.Attribute & JMSG_DELETED)
    {
        return (dword) -1;
    }                           

    if (msg)
    {
        /* make msg */

        msg->attr = Jam_JamAttrToMsg(msgh);

        memset(msg->from, '\0', XMSG_FROM_SIZE);
        memset(msg->to, '\0', XMSG_TO_SIZE);
        memset(msg->subj, '\0', XMSG_SUBJ_SIZE);
        memset(&(msg->orig), 0, sizeof(msg->orig));
        memset(&(msg->dest), 0, sizeof(msg->dest));

        /* get "from name" line */
        SubPos = 0;

        SubField = Jam_GetSubField(msgh, &SubPos, JAMSFLD_SENDERNAME);
        
        if (SubField)
        {
            strncpy((char *) msg->from, (char *) SubField->Buffer, min(SubField->DatLen, sizeof(msg->from)));
        }

        /* get "to name" line */
        SubPos = 0;

        SubField = Jam_GetSubField(msgh, &SubPos, JAMSFLD_RECVRNAME);
        
        if (SubField)
        {
            strncpy((char *) msg->to, (char *) SubField->Buffer, min(SubField->DatLen, sizeof(msg->to)));
        }

        /* get "subj" line */
        SubPos = 0;

        SubField = Jam_GetSubField(msgh, &SubPos, JAMSFLD_SUBJECT);

        if (SubField)
        {
            strncpy((char *) msg->subj, (char *) SubField->Buffer, min(SubField->DatLen, sizeof(msg->subj)));
        }

        if (!msgh->sq->isecho)
        {
            /* get "orig address" line */

            SubPos = 0;

            SubField = Jam_GetSubField(msgh, &SubPos, JAMSFLD_OADDRESS);

            if (SubField)
            {
                parseAddr(&(msg->orig), SubField->Buffer, SubField->DatLen);
            }

            /* get "dest address" line */

            SubPos = 0;

            SubField = Jam_GetSubField(msgh, &SubPos, JAMSFLD_DADDRESS);

            if (SubField)
            {
                parseAddr(&(msg->dest), SubField->Buffer, SubField->DatLen);
            }                   
        }                       


        s_time = gmtime((time_t *) (&(msgh->Hdr.DateWritten)));
        scombo = (SCOMBO *) (&(msg->date_written));
        TmDate_to_DosDate(s_time, scombo);

        if (msgh->Hdr.DateProcessed)
        {
            s_time = gmtime((time_t *) (&(msgh->Hdr.DateProcessed)));

            /* so which is it?? */

            scombo = (SCOMBO *) (&(msg->date_arrived));
            TmDate_to_DosDate(s_time, scombo);
        }
        else
            ((SCOMBO *) (&(msg->date_arrived)))->ldate = 0;

        msg->replyto = msgh->Hdr.ReplyTo;
        msg->xmreply1st = msgh->Hdr.Reply1st;
        msg->replies[1] = 0;
        msg->xmreplynext = msgh->Hdr.ReplyNext;
        msg->xmtimesread = msgh->Hdr.TimesRead;
        msg->xmcost = msgh->Hdr.Cost;

    }                           

    bytesread = 0;

    if (bytes > 0 && text)
    {

        /* read text message */

        if (offset > (msgh->Hdr.TxtLen + msgh->lclen))
            offset = msgh->Hdr.TxtLen + msgh->lclen;

        msgh->cur_pos = offset;

        if (offset >= msgh->Hdr.TxtLen)
        {
            offset -= msgh->Hdr.TxtLen;
            if (bytes > msgh->lclen)
                bytes = msgh->lclen;
            if (offset < msgh->lclen)
            {
                bytesread = bytes - offset;
                strncpy((char *) text, (char *) msgh->lctrl + offset, bytesread);
            }
            else
            {
            }                   
            msgh->cur_pos += bytesread;
        }
        else
        {
            if (MsghJm->TxtHandle == 0)
                Jam_OpenTxtFile(MsghJm);
            lseek(MsghJm->TxtHandle, msgh->Hdr.TxtOffset + offset, SEEK_SET);
            if (bytes > msgh->Hdr.TxtLen - offset)
            {
                bytesread = read(MsghJm->TxtHandle, text, msgh->Hdr.TxtLen - offset);
                bytes -= (msgh->Hdr.TxtLen - offset);
                bytes -= offset;
                if (bytes > msgh->lclen)
                    bytes = msgh->lclen;
                strncpy((char *) text + bytesread, (char *) msgh->lctrl, bytes);
                bytesread += bytes;
            }
            else
            {
                bytesread = read(MsghJm->TxtHandle, text, bytes);
            }                   
            msgh->cur_pos += bytesread;
        }                       
        text[bytesread] = '\0';
    }

    if (clen && ctxt)
    {
        /* read first kludges */
        if (clen > msgh->clen)
            clen = msgh->clen;
        strncpy((char *) ctxt, (char *) msgh->ctrl, clen);
        ctxt[clen] = '\0';
    }

    msgapierr = MERR_NONE;
    return bytesread;
}

static sword JamWriteMsg(MSGH * msgh, word append, XMSG * msg,
       byte * text, dword textlen, dword totlen, dword clen, byte * ctxt)
{
    /* not supported append if JAM !!! the reason is that we need to know
       ALL of the text before we can set up the JAM headers. */

    JAMHDR jamhdrNew;
    JAMIDXREC jamidxNew;
    JAMSUBFIELD2LISTptr subfieldNew;
    XMSG msg_old;
    MSGA *jm;
    dword x;

    char ch = 0;
    unsigned char *onlytext = NULL;
    int didlock = FALSE;
    int rc = 0;

    assert(append == 0);

    if (InvalidMsgh(msgh))
    {
        return -1L;
    }

    if (msgh->mode != MOPEN_CREATE && msgh->mode != MOPEN_WRITE && msgh->mode != MOPEN_RW)
        return -1L;

    jm = msgh->sq;
    Jmd->modified = 1;

    memset(&jamidxNew, '\0', sizeof(JAMIDXREC));
    memset(&jamhdrNew, '\0', sizeof(JAMHDR));
    jamhdrNew.ReplyCRC = jamhdrNew.MsgIdCRC = 0xFFFFFFFFUL;

    if (!ctxt)
        clen = 0L;

    if (!text)
        textlen = 0L;

    if (textlen == 0L)
        text = NULL;

    if (clen == 0L)
        ctxt = NULL;

    if (msgh->mode != MOPEN_CREATE)
    {
        if (clen)
            clen = 0;
        if (ctxt)
            ctxt = NULL;
    }

    if (clen && ctxt)
    {
        x = strlen((char *)ctxt);
        if (clen < x)
            clen = x + 1;
    }

    subfieldNew = NULL;
    if (msg)
        ConvertXmsgToJamHdr(msgh, msg, &jamhdrNew, &subfieldNew);
    else if (msgh->mode != MOPEN_CREATE)
    {
        JamReadMsg(msgh, &msg_old, 0, 0, NULL, 0, NULL);
        ConvertXmsgToJamHdr(msgh, &msg_old, &jamhdrNew, &subfieldNew);
    }

    if (!jm->locked)
    {
        didlock = Jam_Lock(jm, 1);

        if (!didlock)
        {
            msgapierr = MERR_SHARE;
            return -1;
        }
    }

    if (clen && ctxt)
        ConvertCtrlToSubf(&jamhdrNew, &subfieldNew, clen, ctxt);

    if (textlen && text)
        onlytext = DelimText(&jamhdrNew, &subfieldNew, text, textlen);
    else if (msgh->mode != MOPEN_CREATE)
    {
        DelimText(&jamhdrNew, &subfieldNew, msgh->lctrl, msgh->lclen);
        jamhdrNew.TxtOffset = msgh->Hdr.TxtOffset;
        jamhdrNew.TxtLen = msgh->Hdr.TxtLen;
    }

    if (onlytext == NULL)
    {
        onlytext = palloc(1);
        *onlytext = '\0';
    }

    if (msgh->mode == MOPEN_CREATE)
    {
        /* no logic if msg not present */
        if (msg)
        {
            if (Jmd->TxtHandle == 0)
                Jam_OpenTxtFile(Jmd);
            if (msgh->msgnum == 0)
            {
                /* new message in end of position */
                lseek(Jmd->IdxHandle, 0, SEEK_END);
                lseek(Jmd->HdrHandle, 0, SEEK_END);
                lseek(Jmd->TxtHandle, 0, SEEK_END);
                jamhdrNew.MsgNum = Jmd->HdrInfo.BaseMsgNum + (tell(Jmd->IdxHandle) / IDX_SIZE);
                msgh->seek_idx = tell(Jmd->IdxHandle);
                msgh->seek_hdr = jamidxNew.HdrOffset = tell(Jmd->HdrHandle);
                jamidxNew.UserCRC = Jam_Crc32(msg->to, strlen((char *) msg->to));
                jamhdrNew.TxtOffset = tell(Jmd->TxtHandle);
                jamhdrNew.TxtLen = strlen((char *) onlytext);
                msgh->bytes_written = (dword) write(Jmd->TxtHandle, onlytext, jamhdrNew.TxtLen);
                if (msgh->bytes_written != jamhdrNew.TxtLen)
                {
                    setfsize(Jmd->TxtHandle, jamhdrNew.TxtOffset);
                    freejamsubfield(subfieldNew);
                    msgapierr = MERR_NODS;
                    return -1;
                }
                msgh->cur_pos = tell(Jmd->TxtHandle);
                if (!write_hdr(Jmd->HdrHandle, &jamhdrNew))
                {
                    setfsize(Jmd->HdrHandle, msgh->seek_hdr);
                    setfsize(Jmd->TxtHandle, jamhdrNew.TxtOffset);
                    freejamsubfield(subfieldNew);
                    msgapierr = MERR_NODS;
                    return -1;
                }
                if (!write_subfield(Jmd->HdrHandle, &subfieldNew, jamhdrNew.SubfieldLen))
                {
                    setfsize(Jmd->HdrHandle, msgh->seek_hdr);
                    setfsize(Jmd->TxtHandle, jamhdrNew.TxtOffset);
                    freejamsubfield(subfieldNew);
                    msgapierr = MERR_NODS;
                    return -1;
                }
                if (!write_idx(Jmd->IdxHandle, &jamidxNew))
                {
                    setfsize(Jmd->IdxHandle, msgh->seek_idx);
                    setfsize(Jmd->HdrHandle, msgh->seek_hdr);
                    setfsize(Jmd->TxtHandle, jamhdrNew.TxtOffset);
                    freejamsubfield(subfieldNew);
                    msgapierr = MERR_NODS;
                    return -1;
                }
                Jmd->HdrInfo.ActiveMsgs++;
#ifdef HARD_WRITE_HDR
                Jmd->HdrInfo.ModCounter++;
                if (Jam_WriteHdrInfo(Jmd))
                {
                    setfsize(Jmd->HdrHandle, msgh->seek_hdr);
                    setfsize(Jmd->IdxHandle, msgh->seek_idx);
                    setfsize(Jmd->TxtHandle, jamhdrNew.TxtOffset);
                    freejamsubfield(subfieldNew);
                    msgapierr = MERR_NODS;
                    return -1;
                }
#endif
                jm->high_msg++;
                if (Jmd->actmsg_read)
                {
                    Jmd->actmsg = (JAMACTMSGptr) realloc(Jmd->actmsg, sizeof(JAMACTMSG) * (jm->num_msg + 1));
                    Jmd->actmsg[jm->num_msg].IdxOffset = msgh->seek_idx;
                    Jmd->actmsg[jm->num_msg].TrueMsg = msgh->seek_hdr;
                    Jmd->actmsg[jm->num_msg].UserCRC = jamidxNew.UserCRC;
                    memcpy(&(Jmd->actmsg[jm->num_msg].hdr), &jamhdrNew, sizeof(jamhdrNew));
                    if (Jmd->actmsg_read == 1)
                        Jmd->actmsg[jm->num_msg].subfield = subfieldNew;
                    else
                    {           /* not incore subfields */
                        Jmd->actmsg[jm->num_msg].subfield = NULL;
                        freejamsubfield(subfieldNew);
                    }
                }
                else
                    freejamsubfield(subfieldNew);
                jm->num_msg++;
            }
            else
            {
                /* new message instead of old message position */
                msgh->Hdr.TxtLen = 0;
                msgh->Hdr.Attribute |= JMSG_DELETED;
                lseek(Jmd->HdrHandle, 0, SEEK_END);
                jamidxNew.HdrOffset = tell(Jmd->HdrHandle);
                jamhdrNew.MsgNum = msgh->Hdr.MsgNum;
                jamidxNew.UserCRC = Jam_Crc32(msg->to, strlen((char *) msg->to));
                lseek(Jmd->TxtHandle, 0, SEEK_END);
                jamhdrNew.TxtOffset = tell(Jmd->TxtHandle);
                jamhdrNew.TxtLen = strlen((char *) onlytext);
                msgh->bytes_written = (dword) write(Jmd->TxtHandle, onlytext, jamhdrNew.TxtLen);
                msgh->cur_pos = tell(Jmd->TxtHandle);
                lseek(Jmd->TxtHandle, jamhdrNew.TxtOffset + totlen - 1, SEEK_SET);
                write(Jmd->TxtHandle, &ch, 1);
                write_hdr(Jmd->HdrHandle, &jamhdrNew);
                write_subfield(Jmd->HdrHandle, &subfieldNew, jamhdrNew.SubfieldLen);
                lseek(Jmd->IdxHandle, msgh->seek_idx, SEEK_SET);
                write_idx(Jmd->IdxHandle, &jamidxNew);
                lseek(Jmd->HdrHandle, msgh->seek_hdr, SEEK_SET);
                write_hdr(Jmd->HdrHandle, &(msgh->Hdr));
                msgh->seek_hdr = jamidxNew.HdrOffset;
#ifdef HARD_WRITE_HDR
                Jmd->HdrInfo.ModCounter++;
                Jam_WriteHdrInfo(Jmd);
#endif
                /* info from new message to msgh srtuct */
                if (Jmd->actmsg_read)
                {
                    Jmd->actmsg[msgh->msgnum - 1].TrueMsg = msgh->seek_hdr;
                    Jmd->actmsg[msgh->msgnum - 1].UserCRC = jamidxNew.UserCRC;
                    memcpy(&(Jmd->actmsg[msgh->msgnum - 1].hdr), &jamhdrNew, sizeof(jamhdrNew));
                    if (Jmd->actmsg_read == 1)
                        Jmd->actmsg[msgh->msgnum - 1].subfield = subfieldNew;
                    else
                    {           /* subfields not incore */
                        Jmd->actmsg[msgh->msgnum - 1].subfield = NULL;
                        freejamsubfield(subfieldNew);
                    }
                }
                else
                    freejamsubfield(subfieldNew);
            }                   
        }                       
    }
    else
    {
        /* change text and SEEN_BY, PATH, VIA kludges posible only
           (message != create) */
        ConvertCtrlToSubf(&jamhdrNew, &subfieldNew, msgh->clen, msgh->ctrl);

        if (msg)
            jamidxNew.UserCRC = Jam_Crc32(msg->to, strlen((char *) msg->to));
        else
            jamidxNew.UserCRC = msgh->Idx.UserCRC;

        if (text && textlen)
        {
            if (Jmd->TxtHandle == 0)
                Jam_OpenTxtFile(Jmd);
            lseek(Jmd->TxtHandle, 0, SEEK_END);
            jamhdrNew.TxtOffset = tell(Jmd->TxtHandle);
            jamhdrNew.TxtLen = strlen((char *) onlytext);
            write(Jmd->TxtHandle, onlytext, jamhdrNew.TxtLen);
        }                       

        if (jamhdrNew.SubfieldLen > msgh->Hdr.SubfieldLen)
        {
            msgh->Hdr.TxtLen = 0;
            msgh->Hdr.Attribute |= JMSG_DELETED;
            lseek(Jmd->HdrHandle, msgh->seek_hdr, SEEK_SET);
            write_hdr(Jmd->HdrHandle, &(msgh->Hdr));
            lseek(Jmd->HdrHandle, 0, SEEK_END);
            msgh->seek_hdr = tell(Jmd->HdrHandle);
        }

        jamhdrNew.MsgNum = msgh->Hdr.MsgNum;
        jamidxNew.HdrOffset = msgh->seek_hdr;

        lseek(Jmd->HdrHandle, msgh->seek_hdr, SEEK_SET);
        write_hdr(Jmd->HdrHandle, &jamhdrNew);
        write_subfield(Jmd->HdrHandle, &subfieldNew, jamhdrNew.SubfieldLen);

        if (Jmd->actmsg_read &&
            Jmd->actmsg[msgh->msgnum - 1].TrueMsg == msgh->seek_hdr &&
            Jmd->actmsg[msgh->msgnum - 1].UserCRC == jamidxNew.UserCRC)
        {
            /* no index update needed */ ;
        }
        else
        {
            lseek(Jmd->IdxHandle, msgh->seek_idx, SEEK_SET);
            write_idx(Jmd->IdxHandle, &jamidxNew);
        }

#ifdef HARD_WRITE_HDR
        Jmd->HdrInfo.ModCounter++;
        Jam_WriteHdrInfo(Jmd);
#endif
        memmove(&(msgh->Idx), &(jamidxNew), sizeof(JAMIDXREC));
        memmove(&(msgh->Hdr), &(jamhdrNew), sizeof(JAMHDR));
        freejamsubfield(msgh->SubFieldPtr);
        msgh->SubFieldPtr = subfieldNew;
        DecodeSubf(msgh);
        if (Jmd->actmsg_read)
        {
            Jmd->actmsg[msgh->msgnum - 1].TrueMsg = msgh->seek_hdr;
            Jmd->actmsg[msgh->msgnum - 1].UserCRC = jamidxNew.UserCRC;
            memcpy(&(Jmd->actmsg[msgh->msgnum - 1].hdr), &jamhdrNew, sizeof(jamhdrNew));
            if (Jmd->actmsg_read == 1)
                copy_subfield(&(Jmd->actmsg[msgh->msgnum - 1].subfield), subfieldNew);
            else
                Jmd->actmsg[msgh->msgnum - 1].subfield = NULL;
        }

    }                           
    if (didlock)
    {
        Jam_Unlock(jm);
    }                           

    pfree(onlytext);

    return rc;
}


static sword JamKillMsg(MSGA * jm, dword msgnum)
{
    JAMIDXREC jamidx;
    JAMHDR jamhdr;
    if (InvalidMh(jm))
    {
        return -1;
    }

    if (jm->locked)
        return -1L;

    if (msgnum == 0 || msgnum > jm->num_msg)
    {
        msgapierr = MERR_NOENT;
        return -1L;
    }                           

    if (!Jam_PosHdrMsg(jm, msgnum - 1, &jamidx, &jamhdr))
    {
        msgapierr = MERR_BADF;
        return -1L;
    }                           

    if (JamLock(jm) == -1)
        return -1L;

    Jmd->modified = 1;
    Jmd->HdrInfo.ActiveMsgs--;
    jamhdr.TxtLen = 0;
    jamhdr.Attribute |= JMSG_DELETED;
    jamidx.UserCRC = 0xFFFFFFFFL;
    jamidx.HdrOffset = 0xFFFFFFFFL;
    lseek(Jmd->HdrHandle, -(HDR_SIZE), SEEK_CUR);
    lseek(Jmd->IdxHandle, -(IDX_SIZE), SEEK_CUR);
    write_idx(Jmd->IdxHandle, &jamidx);
    write_hdr(Jmd->HdrHandle, &jamhdr);
#ifdef HARD_WRITE_HDR
    Jmd->HdrInfo.ModCounter++;
    Jam_WriteHdrInfo(Jmd);
#endif

    if (Jmd->actmsg_read)
    {
        dword i;
        for (i = 0; i < Jmd->HdrInfo.ActiveMsgs; i++)
            freejamsubfield(Jmd->actmsg[i].subfield);
        pfree(Jmd->actmsg);
        Jmd->actmsg_read = 0;
        Jmd->actmsg = NULL;
    }

    Jam_ActiveMsgs(Jmd);
    jm->num_msg = Jmd->HdrInfo.ActiveMsgs;

    JamUnlock(jm);
    return 0;
}

static sword JamLock(MSGA * jm)
{
    if (InvalidMh(jm))
    {
        return -1;
    }

    /* Don't do anything if already locked */

    if (jm->locked)
    {
        return 0;
    }

    if (!Jam_Lock(jm, 0))
        return -1;

    jm->locked = TRUE;

    return 0;
}

static sword JamUnlock(MSGA * jm)
{
    if (InvalidMh(jm))
    {
        return -1;
    }

    if (!jm->locked)
    {
        return -1;
    }

    jm->locked = FALSE;

    if (mi.haveshare)
    {
        unlock(Jmd->HdrHandle, 0L, 1L);
    }

    return 0;
}

static sword JamSetCurPos(MSGH * msgh, dword pos)
{
    if (InvalidMsgh(msgh))
        return -1;

    msgh->cur_pos = pos;
    return 0;
}

static dword JamGetCurPos(MSGH * msgh)
{
    if (InvalidMsgh(msgh))
        return (dword) -1;

    return msgh->cur_pos;
}

static UMSGID JamMsgnToUid(MSGA * jm, dword msgnum)
{
    if (InvalidMh(jm))
        return (UMSGID) -1;

    msgapierr = MERR_NONE;
    if (msgnum > jm->num_msg)
        return (UMSGID) -1;
    if (msgnum <= 0)
        return (UMSGID) 0;
    if (!Jmd->actmsg_read)
    {
        Jam_ActiveMsgs(Jmd);
        if (msgnum > jm->num_msg)
            return (UMSGID) - 1;
    }
    return (UMSGID) (Jmd->actmsg[msgnum - 1].IdxOffset / 8 + Jmd->HdrInfo.BaseMsgNum);
}

static dword JamUidToMsgn(MSGA * jm, UMSGID umsgid, word type)
{
    dword msgnum, left, right, new;
    UMSGID umsg;

    if (InvalidMh(jm))
        return (dword) -1;

    msgnum = umsgid - Jmd->HdrInfo.BaseMsgNum + 1;
    if (msgnum <= 0)
        return 0;
    if (!Jmd->actmsg_read)
        Jam_ActiveMsgs(Jmd);
    left = 1;
    right = jm->num_msg;
    while (left <= right)
    {
        new = (right + left) / 2;
        umsg = JamMsgnToUid(jm, new);
        if (umsg == (dword) -1)
            return 0;
        if (umsg < msgnum)
            left = new + 1;
        else if (umsg > msgnum)
        {
            if (new > 0)
                right = new - 1;
            else
                right = 0;
        }
        else
            return new;
    }
    if (type == UID_EXACT)
        return 0;
    if (type == UID_PREV)
        return right;
    return (left > jm->num_msg) ? jm->num_msg : left;
}

static dword JamGetHighWater(MSGA * jm)
{
    if (InvalidMh(jm))
        return (dword) -1;

    return JamUidToMsgn(jm, jm->high_water, UID_PREV);
}

static sword JamSetHighWater(MSGA * jm, dword hwm)
{
    if (InvalidMh(jm))
        return -1L;

    hwm = JamMsgnToUid(jm, hwm);
    if (hwm > jm->high_msg + Jmd->HdrInfo.BaseMsgNum)
        return -1L;

    jm->high_water = hwm;

    return 0;
}

static dword JamGetTextLen(MSGH * msgh)
{
    return (msgh->Hdr.TxtLen + msgh->lclen);
}

static dword JamGetCtrlLen(MSGH * msgh)
{
    return (msgh->clen);
}


sword MSGAPI JamValidate(byte * name)
{
    byte temp[PATHLEN];

    sprintf((char *)temp, "%s%s", name, EXT_HDRFILE);

    if (!fexist((char *)temp))
        return FALSE;

    sprintf((char *)temp, "%s%s", name, EXT_TXTFILE);

    if (!fexist((char *)temp))
        return FALSE;

    sprintf((char *)temp, "%s%s", name, EXT_IDXFILE);

    if (!fexist((char *)temp))
        return FALSE;

    return TRUE;
}

static void Jam_CloseFile(JAMBASE * jambase)
{
    if (jambase->HdrHandle != 0 && jambase->HdrHandle != -1)
    {
        close(jambase->HdrHandle);
        jambase->HdrHandle = 0;
    }                           

    if (jambase->TxtHandle != 0 && jambase->TxtHandle != -1)
    {
        close(jambase->TxtHandle);
        jambase->TxtHandle = 0;
    }                           

    if (jambase->IdxHandle != 0 && jambase->IdxHandle != -1)
    {
        close(jambase->IdxHandle);
        jambase->IdxHandle = 0;
    }                           

    if (jambase->LrdHandle != 0 && jambase->LrdHandle != -1)
    {
        close(jambase->LrdHandle);
        jambase->LrdHandle = 0;
    }                           
}

static int gettz(void)
{
    struct tm *tm;
    time_t t, gt;

    t = time(NULL);

#if !defined(__LCC__)
    tzset();
#endif

    tm = gmtime(&t);
    tm->tm_isdst = 0;
    gt = mktime(tm);
    tm = localtime(&t);
    tm->tm_isdst = 0;
    return (int)(((long)mktime(tm) - (long)gt));
}

int JamDeleteBase(char *name)
{
    char *hdr, *idx, *txt, *lrd;
    int x = strlen(name) + 5;
    int rc = 1;

    hdr = (char *)palloc(x);
    idx = (char *)palloc(x);
    txt = (char *)palloc(x);
    lrd = (char *)palloc(x);

    sprintf(hdr, "%s%s", name, EXT_HDRFILE);
    sprintf(txt, "%s%s", name, EXT_TXTFILE);
    sprintf(idx, "%s%s", name, EXT_IDXFILE);
    sprintf(lrd, "%s%s", name, EXT_LRDFILE);

    if (unlink(hdr))
        rc = 0;
    if (unlink(txt))
        rc = 0;
    if (unlink(idx))
        rc = 0;
    if (unlink(lrd))
        rc = 0;

    pfree(hdr);
    pfree(txt);
    pfree(idx);
    pfree(lrd);

    return rc;
}

static int Jam_OpenFile(JAMBASE * jambase, word * mode, mode_t permissions)
{
    char *hdr, *idx, *txt, *lrd;
    int x;

    x = strlen((char *) jambase->BaseName) + 5;

    hdr = (char *)palloc(x);
    idx = (char *)palloc(x);
    txt = (char *)palloc(x);
    lrd = (char *)palloc(x);

    sprintf(hdr, "%s%s", jambase->BaseName, EXT_HDRFILE);
    sprintf(txt, "%s%s", jambase->BaseName, EXT_TXTFILE);
    sprintf(idx, "%s%s", jambase->BaseName, EXT_IDXFILE);
    sprintf(lrd, "%s%s", jambase->BaseName, EXT_LRDFILE);

    if (*mode == MSGAREA_CREATE)
    {
        jambase->HdrHandle = opencreatefilejm(hdr, fop_wpb, permissions);
        jambase->TxtHandle = openfilejm(txt, fop_wpb, permissions);
        jambase->IdxHandle = openfilejm(idx, fop_wpb, permissions);
        /* jambase->LrdHandle = openfilejm(lrd, fop_wpb, permissions); */ jambase->LrdHandle = 0;

        memset(&(jambase->HdrInfo), '\0', sizeof(JAMHDRINFO));
        strcpy((char *) jambase->HdrInfo.Signature, HEADERSIGNATURE);

        jambase->HdrInfo.DateCreated = time(NULL) + gettz();
        jambase->HdrInfo.ModCounter = 1;
        jambase->HdrInfo.PasswordCRC = 0xffffffffUL;
        jambase->HdrInfo.BaseMsgNum = 1;

        write_hdrinfo(jambase->HdrHandle, &(jambase->HdrInfo));

    }
    else
    {
        jambase->HdrHandle = openfilejm(hdr, fop_rpb, permissions);
        /* jambase->TxtHandle = openfilejm(txt, fop_rpb, permissions); */ jambase->TxtHandle = 0;
        jambase->IdxHandle = openfilejm(idx, fop_rpb, permissions);
        /* jambase->LrdHandle = openfilejm(lrd, fop_rpb, permissions); */ jambase->LrdHandle = 0;
    }                           

    if (jambase->HdrHandle == -1 || jambase->TxtHandle == -1 || jambase->IdxHandle == -1)
    {
        if (*mode != MSGAREA_CRIFNEC)
        {
            Jam_CloseFile(jambase);
            pfree(hdr);
            pfree(txt);
            pfree(idx);
            pfree(lrd);
            msgapierr = MERR_NOENT;
            return 0;
        }
        *mode = MSGAREA_CREATE;
        jambase->HdrHandle = opencreatefilejm(hdr, fop_cpb, permissions);
        jambase->TxtHandle = openfilejm(txt, fop_cpb, permissions);
        jambase->IdxHandle = openfilejm(idx, fop_cpb, permissions);
        /* jambase->LrdHandle = openfilejm(lrd, fop_cpb, permissions); */ jambase->LrdHandle = 0;

        if (jambase->HdrHandle == -1 || jambase->TxtHandle == -1 || jambase->IdxHandle == -1)
        {
            Jam_CloseFile(jambase);
            pfree(hdr);
            pfree(txt);
            pfree(idx);
            pfree(lrd);
            msgapierr = MERR_NOENT;
            return 0;
        }                       


        memset(&(jambase->HdrInfo), '\0', sizeof(JAMHDRINFO));
        strcpy((char *) jambase->HdrInfo.Signature, HEADERSIGNATURE);
        jambase->HdrInfo.DateCreated = time(NULL) + gettz();
        jambase->HdrInfo.ModCounter = 1;
        jambase->HdrInfo.PasswordCRC = 0xffffffffUL;
        jambase->HdrInfo.BaseMsgNum = 1;

        write_hdrinfo(jambase->HdrHandle, &(jambase->HdrInfo));

    }                           

    pfree(hdr);
    pfree(txt);
    pfree(idx);
    pfree(lrd);

    jambase->mode = *mode;
    jambase->permissions = permissions;
    jambase->modified = 0;

    return 1;
}

static sword MSGAPI Jam_OpenBase(MSGA * jm, word * mode, unsigned char *basename)
{
    Jmd->BaseName = (unsigned char *)palloc(strlen((char *) basename) + 1);
    strcpy((char *) Jmd->BaseName, (char *) basename);

    if (!Jam_OpenFile(Jmd, mode, FILEMODE(jm->isecho)))
    {
        pfree(Jmd->BaseName);
        return 0;
    }                           

    lseek(Jmd->HdrHandle, 0, SEEK_SET);
    read_hdrinfo(Jmd->HdrHandle, &(Jmd->HdrInfo));

    return 1;
}

static MSGH *Jam_OpenMsg(MSGA * jm, word mode, dword msgnum)
{
    struct _msgh *msgh;
/*   JAMIDXREC    idx; */
/*   JAMHDR       hdr; */

    unused(mode);

    if (msgnum == MSGNUM_CUR)
    {
        msgnum = jm->cur_msg;
    }
    else if (msgnum == MSGNUM_NEXT)
    {
        msgnum = jm->cur_msg + 1;
        if (msgnum > jm->num_msg)
        {
            msgapierr = MERR_NOENT;
            return NULL;
        }                       
        jm->cur_msg = msgnum;
    }
    else if (msgnum == MSGNUM_PREV)
    {
        msgnum = jm->cur_msg - 1;
        if (msgnum == 0)
        {
            msgapierr = MERR_NOENT;
            return NULL;
        }                       
        jm->cur_msg = msgnum;
    }
    else
    {
        if (msgnum > jm->num_msg)
        {
            msgapierr = MERR_NOENT;
            return NULL;
        }                       
    }

    msgh = palloc(sizeof(struct _msgh));
    if (msgh == NULL)
    {
        msgapierr = MERR_NOMEM;
        return NULL;
    }

    memset(msgh, '\0', sizeof(struct _msgh));

    msgh->sq = jm;
    msgh->msgnum = msgnum;

    msgh->mode = mode;
    msgh->id = MSGH_ID;

/*   msgh->Idx.HdrOffset = 0xffffffff; */
/*   msgh->Idx.UserCRC   = 0xffffffff; */

    if (!Jmd->actmsg_read)
    {
        Jam_ActiveMsgs(Jmd);
        if (msgnum > jm->num_msg)
        {
            msgapierr = MERR_NOENT;
            return NULL;
        }
    }

    if (Jmd->actmsg)
    {
        msgh->seek_idx = Jmd->actmsg[msgnum - 1].IdxOffset;
        msgh->Idx.HdrOffset = Jmd->actmsg[msgnum - 1].TrueMsg;
        msgh->Idx.UserCRC = Jmd->actmsg[msgnum - 1].UserCRC;
        if (msgh->Idx.HdrOffset != 0xffffffffUL)
        {
            msgh->seek_hdr = msgh->Idx.HdrOffset;
            memcpy(&(msgh->Hdr), &(Jmd->actmsg[msgnum - 1].hdr), sizeof(msgh->Hdr));

            if (!eqstri((char *)&msgh->Hdr, HEADERSIGNATURE))
            {
                pfree(msgh);
                return NULL;
            }
            else
            {
            }                   
            if (mode == MOPEN_CREATE)
                return (MSGH *) msgh;

            msgh->SubFieldPtr = 0;

            if (Jmd->actmsg[msgnum - 1].subfield)
                copy_subfield(&(msgh->SubFieldPtr), Jmd->actmsg[msgnum - 1].subfield);
            else
            {
                lseek(Jmd->HdrHandle, Jmd->actmsg[msgnum - 1].TrueMsg + HDR_SIZE, SEEK_SET);
                read_subfield(Jmd->HdrHandle, &(msgh->SubFieldPtr), &(Jmd->actmsg[msgnum - 1].hdr.SubfieldLen));
            }
            DecodeSubf(msgh);
            return (MSGH *) msgh;
        }
    }

    pfree(msgh);
    return NULL;
}

static JAMSUBFIELD2ptr Jam_GetSubField(struct _msgh * msgh, dword * SubPos, word what)
{
    JAMSUBFIELD2ptr SubField;
    JAMSUBFIELD2LISTptr SubFieldList;
    dword i;

    SubFieldList = msgh->SubFieldPtr;
    SubField = msgh->SubFieldPtr->subfield;
    for (i = *SubPos; i < SubFieldList->subfieldCount; i++, SubField++)
    {
        if (SubField->LoID == what)
        {
            *SubPos = i;
            return SubField;
        }
    }

    return NULL;
}

static dword Jam_HighMsg(JAMBASEptr jambase)
{
    dword highmsg;
    lseek(jambase->IdxHandle, 0, SEEK_END);
    highmsg = tell(jambase->IdxHandle);
    return (highmsg / IDX_SIZE);
}

static void Jam_ActiveMsgs(JAMBASEptr jambase)
{
    read_allidx(jambase);
}

static dword Jam_PosHdrMsg(MSGA * jm, dword msgnum, JAMIDXREC * jamidx, JAMHDR * jamhdr)
{
    if (!Jmd->actmsg_read)
        Jam_ActiveMsgs(Jmd);

    jamidx->HdrOffset = Jmd->actmsg[msgnum].TrueMsg;

    if (jamidx->HdrOffset == 0xffffffffUL)
        return 0;

    if (lseek(Jmd->HdrHandle, jamidx->HdrOffset, SEEK_SET) == -1)
        return 0;

    if (read_hdr(Jmd->HdrHandle, jamhdr) == 0)
        return 0;

    if (jamhdr->Attribute & JMSG_DELETED)
        return 0;

    return 1;
}

static int Jam_Lock(MSGA * jm, int force)
{
    return !(mi.haveshare && ((force) ?
      waitlock(Jmd->HdrHandle, 0L, 1L) : lock(Jmd->HdrHandle, 0L, 1L) == -1));
}

static void Jam_Unlock(MSGA * jm)
{
    if (mi.haveshare)
    {
        unlock(Jmd->HdrHandle, 0L, 1L);
    }
}

static sword Jam_WriteHdrInfo(JAMBASEptr jambase)
{
    if (lseek(jambase->HdrHandle, 0, SEEK_SET) == -1)
        return -1;
    if (write_hdrinfo(jambase->HdrHandle, &(jambase->HdrInfo)) == 0)
        return -1;
    jambase->modified = 0;
    return 0;
}

static dword Jam_JamAttrToMsg(MSGH * msgh)
{
    dword attr = 0;

    if (msgh->Hdr.Attribute & JMSG_LOCAL)
        attr |= MSGLOCAL;
    if (msgh->Hdr.Attribute & JMSG_PRIVATE)
        attr |= MSGPRIVATE;
    if (msgh->Hdr.Attribute & JMSG_READ)
        attr |= MSGREAD;
    if (msgh->Hdr.Attribute & JMSG_SENT)
        attr |= MSGSENT;
    if (msgh->Hdr.Attribute & JMSG_KILLSENT)
        attr |= MSGKILL;
    if (msgh->Hdr.Attribute & JMSG_HOLD)
        attr |= MSGHOLD;
    if (msgh->Hdr.Attribute & JMSG_CRASH)
        attr |= MSGCRASH;
    if (msgh->Hdr.Attribute & JMSG_FILEREQUEST)
        attr |= MSGFRQ;
    if (msgh->Hdr.Attribute & JMSG_FILEATTACH)
        attr |= MSGFILE;
    if (msgh->Hdr.Attribute & JMSG_INTRANSIT)
        attr |= MSGFWD;
    if (msgh->Hdr.Attribute & JMSG_RECEIPTREQ)
        attr |= MSGRRQ;
    if (msgh->Hdr.Attribute & JMSG_ORPHAN)
        attr |= MSGORPHAN;
    if (msgh->Hdr.Attribute & JMSG_CONFIRMREQ)
        attr |= MSGCPT;
    if (msgh->Hdr.Attribute & JMSG_LOCKED)
        attr |= MSGLOCKED;
    if (msgh->Hdr.Attribute & JMSG_DIRECT)
        attr |= MSGXX2;
    if (msgh->Hdr.Attribute & JMSG_IMMEDIATE)
        attr |= MSGIMM;

    return attr;
}

static dword Jam_MsgAttrToJam(XMSG * msg)
{
    dword attr = 0;

    if (msg->attr & MSGLOCAL)
        attr |= JMSG_LOCAL;
    if (msg->attr & MSGPRIVATE)
        attr |= JMSG_PRIVATE;
    if (msg->attr & MSGREAD)
        attr |= JMSG_READ;
    if (msg->attr & MSGSENT)
        attr |= JMSG_SENT;
    if (msg->attr & MSGKILL)
        attr |= JMSG_KILLSENT;
    if (msg->attr & MSGHOLD)
        attr |= JMSG_HOLD;
    if (msg->attr & MSGCRASH)
        attr |= JMSG_CRASH;
    if (msg->attr & MSGFRQ)
        attr |= JMSG_FILEREQUEST;
    if (msg->attr & MSGFILE)
        attr |= JMSG_FILEATTACH;
    if (msg->attr & MSGFWD)
        attr |= JMSG_INTRANSIT;
    if (msg->attr & MSGRRQ)
        attr |= JMSG_RECEIPTREQ;
    if (msg->attr & MSGORPHAN)
        attr |= JMSG_ORPHAN;
    if (msg->attr & MSGCPT)
        attr |= JMSG_CONFIRMREQ;
    if (msg->attr & MSGLOCKED)
        attr |= JMSG_LOCKED;
    if (msg->attr & MSGXX2)
        attr |= JMSG_DIRECT;
    if (msg->attr & MSGIMM)
        attr |= JMSG_IMMEDIATE;

    return attr;
}

static int StrToSubfield(unsigned char *str, dword lstr, dword * len, JAMSUBFIELD2ptr subf)
{
    /* warning: str is read only and NOT nul-terminated! */
    unsigned char *kludge;
    word subtypes;

    if (!subf)
        return 0;

    while (lstr > 0)
        if (str[lstr - 1] == '\r')
            lstr--;
        else
            break;

    kludge = str;
    subtypes = JAMSFLD_FTSKLUDGE;

    switch (*str)
    {
    case 'F':
        if (lstr > 5 && strncmp((char *) str, "FMPT ", 5) == 0)
            return 0;
        else if (lstr > 6 && strncmp((char *) str, "FLAGS ", 6) == 0)
        {
            kludge = str + 6;
            subtypes = JAMSFLD_FLAGS;
        }
        break;
    case 'I':
        if (lstr > 4 && strncmp((char *) str, "INTL ", 4) == 0)
            return 0;
        break;
    case 'M':
        if (lstr > 7 && strncmp((char *) str, "MSGID: ", 7) == 0)
        {
            kludge = str + 7;
            subtypes = JAMSFLD_MSGID;
        }
        break;
    case 'P':
        if (lstr > 6 && strncmp((char *) str, "PATH: ", 6) == 0)
        {
            kludge = str + 6;
            subtypes = JAMSFLD_PATH2D;
        }
        else if (lstr > 5 && strncmp((char *) str, "PID: ", 5) == 0)
        {
            kludge = str + 5;
            subtypes = JAMSFLD_PID;
        }
        break;
    case 'R':
        if (lstr > 7 && strncmp((char *) str, "REPLY: ", 7) == 0)
        {
            kludge = str + 7;
            subtypes = JAMSFLD_REPLYID;
        }
        break;
    case 'S':
        if (lstr > 9 && strncmp((char *) str, "SEEN-BY: ", 9) == 0)
        {
            kludge = str + 9;
            subtypes = JAMSFLD_SEENBY2D;
        }
        break;
    case 'T':
        if (lstr > 5 && strncmp((char *) str, "TOPT ", 5) == 0)
            return 0;
        else if (lstr > 7 && strncmp((char *) str, "TZUTC: ", 7) == 0)
        {
            kludge = str + 7;
            subtypes = JAMSFLD_TZUTCINFO;
        }
        break;
    case 'V':
        if (lstr > 4 && strncmp((char *) str, "Via ", 4) == 0)
        {
            kludge = str + 4;
            subtypes = JAMSFLD_TRACE;
        }
    }
    subf->LoID = subtypes;
    subf->DatLen = lstr - (kludge - str);
    memcpy(subf->Buffer, kludge, subf->DatLen);
    subf->Buffer[subf->DatLen] = '\0';
    *len = sizeof(JAMBINSUBFIELD) + subf->DatLen;
    return 1;
}

static int NETADDRtoSubf(NETADDR addr, dword * len, word opt, JAMSUBFIELD2ptr subf)
{
    /* opt = 0 origaddr */
    /* opt = 1 destaddr */

    if (!subf)
        return 0;

    if (addr.zone == 0 && addr.net == 0 && addr.node == 0 && addr.point == 0)
        return 0;
    if (addr.point)
    {
        sprintf((char *) subf->Buffer, "%d:%d/%d.%d", addr.zone, addr.net, addr.node, addr.point);
    }
    else
    {
        sprintf((char *) subf->Buffer, "%d:%d/%d", addr.zone, addr.net, addr.node);
    }                           

    subf->DatLen = *len = strlen((char *) subf->Buffer);
    *len += sizeof(JAMBINSUBFIELD);
    subf->LoID = opt ? JAMSFLD_DADDRESS : JAMSFLD_OADDRESS;

    return 1;
}

static int FromToSubjTOSubf(word jamsfld, unsigned char *txt, dword * len, JAMSUBFIELD2ptr subf)
{
    if (!subf)
        return 0;
    subf->LoID = jamsfld;
    memmove(subf->Buffer, txt, *len = subf->DatLen = strlen((char *) txt));
    *len += sizeof(JAMBINSUBFIELD);

    return 1;
}

static void MSGAPI ConvertXmsgToJamHdr(MSGH * msgh, XMSG * msg, JAMHDRptr jamhdr, JAMSUBFIELD2LISTptr * subfield)
{
    JAMSUBFIELD2ptr SubFieldCur;
    struct tm stm, *ptm;
    dword clen, sublen;

    clen = msgh->sq->isecho ? 3 : 5;
    sublen = sizeof(JAMSUBFIELD2LIST) + sizeof(JAMSUBFIELD2) * clen + 37 + 37 + 73 +
        (msgh->sq->isecho ? 0 : 30 * 2);
    *subfield = palloc(sublen);
    subfield[0]->arraySize = sublen;
    subfield[0]->subfieldCount = 0;
    subfield[0]->subfield[0].Buffer = (byte *) &(subfield[0]->subfield[clen + 1]);

    memset(jamhdr, '\0', sizeof(JAMHDR));

    jamhdr->Attribute = Jam_MsgAttrToJam(msg);
    if (msgh->sq->isecho != NOTH)
    {
        if (msgh->sq->isecho)
        {
            jamhdr->Attribute |= JMSG_TYPEECHO;
        }
        else
        {
            jamhdr->Attribute |= JMSG_TYPENET;
        }                       
    }
    strcpy((char *) jamhdr->Signature, HEADERSIGNATURE);
    jamhdr->Revision = CURRENTREVLEV;
    if (((SCOMBO *) & (msg->date_arrived))->ldate)
    {
        /* save arrived date for sqpack */
        ptm = &stm;
        ptm = DosDate_to_TmDate((SCOMBO *) (&(msg->date_arrived)), ptm);
        jamhdr->DateProcessed = mktime(ptm) + gettz();
    }
    else
        jamhdr->DateProcessed = time(NULL) + gettz();
    ptm = &stm;
    ptm = DosDate_to_TmDate((SCOMBO *) (&(msg->date_written)), ptm);
    jamhdr->DateWritten = mktime(ptm) + gettz();

    sublen = 0;

    /* From Name */

    SubFieldCur = subfield[0]->subfield;
    if (FromToSubjTOSubf(JAMSFLD_SENDERNAME, msg->from, &clen, SubFieldCur))
    {
        SubFieldCur[1].Buffer = SubFieldCur->Buffer + SubFieldCur->DatLen + 1;
        subfield[0]->subfieldCount++;
        SubFieldCur++;
        sublen += clen;
    }                           

    /* To Name */

    if (FromToSubjTOSubf(JAMSFLD_RECVRNAME, msg->to, &clen, SubFieldCur))
    {
        SubFieldCur[1].Buffer = SubFieldCur->Buffer + SubFieldCur->DatLen + 1;
        subfield[0]->subfieldCount++;
        SubFieldCur++;
        sublen += clen;
    }                           

    /* Subject */

    if (FromToSubjTOSubf(JAMSFLD_SUBJECT, msg->subj, &clen, SubFieldCur))
    {
        SubFieldCur[1].Buffer = SubFieldCur->Buffer + SubFieldCur->DatLen + 1;
        subfield[0]->subfieldCount++;
        SubFieldCur++;
        sublen += clen;
    }                           

    if (!msgh->sq->isecho)
    {

        /* Orig Address */

        if (NETADDRtoSubf(msg->orig, &clen, 0, SubFieldCur))
        {
            SubFieldCur[1].Buffer = SubFieldCur->Buffer + SubFieldCur->DatLen + 1;
            subfield[0]->subfieldCount++;
            SubFieldCur++;
            sublen += clen;
        }                       

        /* Dest Address */

        if (NETADDRtoSubf(msg->dest, &clen, 1, SubFieldCur))
        {
            SubFieldCur[1].Buffer = SubFieldCur->Buffer + SubFieldCur->DatLen + 1;
            subfield[0]->subfieldCount++;
            SubFieldCur++;
            sublen += clen;
        }                       

    }
    assert(SubFieldCur->Buffer <= (byte *) * subfield + subfield[0]->arraySize);
    assert((byte *) (SubFieldCur + 1) <= subfield[0]->subfield[0].Buffer);

    jamhdr->SubfieldLen = sublen;
    jamhdr->PasswordCRC = 0xFFFFFFFFUL;

    jamhdr->ReplyTo = msg->replyto;
    jamhdr->Reply1st = msg->xmreply1st;
    jamhdr->ReplyNext = msg->xmreplynext;
    jamhdr->TimesRead = msg->xmtimesread;
    jamhdr->Cost = msg->xmcost;
}

static void resize_subfields(JAMSUBFIELD2LISTptr * subfield, dword newcount,
                             dword len)
{
    dword offs;
    int i;
    JAMSUBFIELD2LISTptr SubField;

    SubField = palloc(len);
    SubField->arraySize = len;
    SubField->subfieldCount = subfield[0]->subfieldCount;
    if (subfield[0]->subfieldCount == 0)
        SubField->subfield[0].Buffer = (byte *)&(SubField->subfield[SubField->subfieldCount + newcount]);
    else
    {
        memcpy(SubField->subfield, subfield[0]->subfield,
               SubField->subfieldCount * sizeof(JAMSUBFIELD2));
        SubField->subfield[SubField->subfieldCount].Buffer =
            subfield[0]->subfield[SubField->subfieldCount - 1].Buffer +
            subfield[0]->subfield[SubField->subfieldCount - 1].DatLen;
    }
    offs = (byte *) & (SubField->subfield[newcount]) - subfield[0]->subfield[0].Buffer;
    for (i = subfield[0]->subfieldCount; i >= 0; i--)
        SubField->subfield[i].Buffer += offs;
    memcpy(SubField->subfield[0].Buffer, subfield[0]->subfield[0].Buffer,
           subfield[0]->arraySize - ((char *)(subfield[0]->subfield[0].Buffer) - (char *)(*subfield)));

    freejamsubfield(*subfield);
    *subfield = SubField;
    assert(subfield[0]->subfield[subfield[0]->subfieldCount].Buffer <= (byte *) * subfield + subfield[0]->arraySize);
    assert((byte *) & (subfield[0]->subfield[newcount]) == subfield[0]->subfield[0].Buffer);
}

static void MSGAPI ConvertCtrlToSubf(JAMHDRptr jamhdr, JAMSUBFIELD2LISTptr
                             * subfield, dword clen, unsigned char *ctxt)
{
    JAMSUBFIELD2ptr SubField;
    dword len, i;
    unsigned char *ptr;

    /* count new subfields */
    i = *ctxt ? 2 : 1;
    for (ptr = ctxt, len = 0; len < clen; len++)
        if (*ptr++ == '\001')
            i++;
    resize_subfields(subfield, subfield[0]->subfieldCount + i,
           subfield[0]->arraySize + clen + i + i * sizeof(JAMSUBFIELD2));
    SubField = &(subfield[0]->subfield[subfield[0]->subfieldCount]);

    for (ptr = ctxt, len = 0; len <= clen; len++, ptr++)
    {
        if (*ptr == '\0' || len == clen || *ptr == '\001')
        {
            if (*ctxt && *ctxt != '\001' &&
                StrToSubfield(ctxt, ptr - ctxt, &i, SubField))
            {
                SubField[1].Buffer = SubField->Buffer + SubField->DatLen + 1;
                jamhdr->SubfieldLen += i;
                subfield[0]->subfieldCount++;
                if (SubField->LoID == JAMSFLD_MSGID)
                    jamhdr->MsgIdCRC = Jam_Crc32(SubField->Buffer, SubField->DatLen);
                else if (SubField->LoID == JAMSFLD_REPLYID)
                    jamhdr->ReplyCRC = Jam_Crc32(SubField->Buffer, SubField->DatLen);
                SubField++;
            }
            if (*ptr == '\0' || len == clen)
                break;
            ctxt = ptr + 1;
        }
    }
    assert(SubField->Buffer <= (byte *) * subfield + subfield[0]->arraySize);
    assert((byte *) (SubField + 1) <= subfield[0]->subfield[0].Buffer);
}

static unsigned char *DelimText(JAMHDRptr jamhdr,
  JAMSUBFIELD2LISTptr * subfield, unsigned char *text, size_t textlen)
{
    JAMSUBFIELD2ptr SubField;
    dword clen, firstlen, i, len;
    unsigned char *onlytext, *first, *ptr, *curtext;

    if (textlen)
    {
        if (text[textlen - 1] != '\r')
            textlen++;
        onlytext = curtext = (unsigned char *)palloc(textlen + 1);
        *onlytext = '\0';

        /* count subfields */
        i = 1;
        len = 0;
        for (first = text; first < text + textlen; first = ptr + 1)
        {
            ptr = (byte *) strchr((char *) first, '\r');
            if (ptr == NULL)
                ptr = text + textlen;
            if (*first == '\001' || strncmp((char *) first, "SEEN-BY: ", 9) == 0)
            {
                if (*first == '\001')
                    first++;
                i++;
                len += (ptr - first);
            }
        }
        resize_subfields(subfield, subfield[0]->subfieldCount + i,
            subfield[0]->arraySize + len + i + i * sizeof(JAMSUBFIELD2));
        SubField = &(subfield[0]->subfield[subfield[0]->subfieldCount]);

        first = text;
        while (*first)
        {
            ptr = (unsigned char *)strchr((char *) first, '\r');
            if (ptr)
                *ptr = '\0';
            firstlen = ptr ? (ptr - first) : strlen((char *) first);
            if ((firstlen > 9 && strncmp((char *) first, "SEEN-BY: ", 9) == 0) ||
                *first == '\001')
            {

                if (*first == '\001')
                {
                    first++;
                    firstlen--;
                }

                if (StrToSubfield(first, firstlen, &clen, SubField))
                {
                    SubField[1].Buffer = SubField->Buffer + SubField->DatLen + 1;
                    jamhdr->SubfieldLen += clen;
                    subfield[0]->subfieldCount++;
                    if (SubField->LoID == JAMSFLD_MSGID)
                        jamhdr->MsgIdCRC = Jam_Crc32(SubField->Buffer, SubField->DatLen);
                    else if (SubField->LoID == JAMSFLD_REPLYID)
                        jamhdr->ReplyCRC = Jam_Crc32(SubField->Buffer, SubField->DatLen);
                    SubField++;
                }
            }
            else
            {
                assert((curtext - onlytext) + firstlen + 1 <= textlen);
                strcpy((char *) curtext, (char *) first);
                curtext += firstlen;
                *curtext++ = '\r';
                *curtext = '\0';
            }                   
            if (ptr)
            {
                *ptr = '\r';
                first = ptr + 1;
            }
            else
            {
                first += firstlen;
            }                   
        }
        assert(SubField->Buffer <= (byte *) * subfield + subfield[0]->arraySize);
        assert((byte *) (SubField + 1) <= subfield[0]->subfield[0].Buffer);
    }
    else
    {
        onlytext = NULL;
    }

    return onlytext;
}

static void parseAddr(NETADDR * netAddr, unsigned char *str, dword len)
{
    char *strAddr, *ptr, *tmp, ch[10];

    strAddr = (char *)calloc(len + 1, sizeof(char *));
    if (!strAddr)
        return;

    memset(netAddr, '\0', sizeof(NETADDR));


    strncpy(strAddr, (char *) str, len);

    ptr = strchr(strAddr, '@');

    if (ptr)
        *ptr = '\0';

    ptr = strchr(strAddr, ':');
    if (ptr)
    {
        memset(ch, '\0', sizeof(ch));
        strncpy(ch, strAddr, ptr - strAddr);
        netAddr->zone = atoi(ch);
        tmp = ++ptr;
    }
    else
    {
        tmp = strAddr;
        netAddr->zone = 0;
    }                           

    ptr = strchr(tmp, '/');
    if (ptr)
    {
        memset(ch, '\0', sizeof(ch));
        strncpy(ch, tmp, ptr - tmp);
        netAddr->net = atoi(ch);
        tmp = ++ptr;
    }
    else
    {
        netAddr->net = 0;
    }                           

    ptr = strchr(tmp, '.');
    if (ptr)
    {
        memset(ch, '\0', sizeof(ch));
        strncpy(ch, tmp, ptr - tmp);
        netAddr->node = atoi(ch);
        ptr++;
        netAddr->point = atoi(ptr);
    }
    else
    {
        netAddr->node = atoi(tmp);
        netAddr->point = 0;
    }                           
}

static void addkludge(char **line, char *kludge, char *ent, char *lf, dword len)
{
    strcpy(*line, kludge);
    *line += strlen(kludge);
    strncpy(*line, ent, len);
    (*line)[len] = '\0';
    *line += strlen(*line);
    strcpy(*line, lf);
    *line += strlen(*line);
}

static void DecodeSubf(MSGH * msgh)
{
    dword SubPos;
    JAMSUBFIELD2ptr SubField;
    JAMSUBFIELD2LISTptr sfl;
    char *ptr, *fmpt, *topt, *pctrl, *plctrl, orig[30], dest[30];
    dword i;

    msgh->ctrl = (unsigned char *)palloc(msgh->SubFieldPtr->arraySize + 65);
    msgh->lctrl = (unsigned char *)palloc(msgh->SubFieldPtr->arraySize + 65);
    *(msgh->ctrl) = *(msgh->lctrl) = '\0';
    pctrl = (char *) msgh->ctrl;
    plctrl = (char *) msgh->lctrl;
    orig[0] = dest[0] = '\0';

    if (!msgh->sq->isecho)
    {
        SubPos = 0;
        SubField = Jam_GetSubField(msgh, &SubPos, JAMSFLD_OADDRESS);
        if (SubField)
            strncpy(orig, (char *) SubField->Buffer, min(SubField->DatLen, sizeof(orig)));
        SubPos = 0;
        SubField = Jam_GetSubField(msgh, &SubPos, JAMSFLD_DADDRESS);
        if (SubField)
            strncpy(dest, (char *) SubField->Buffer, min(SubField->DatLen, sizeof(dest)));
        fmpt = topt = NULL;
        if (orig[0])
        {
            ptr = strchr(orig, '@');
            if (ptr)
                *ptr = '\0';
            ptr = strchr(orig, '.');
            if (ptr)
            {
                *ptr++ = '\0';
                if (atoi(ptr) != 0)
                    fmpt = ptr;
            }
        }
        if (dest[0])
        {
            ptr = strchr(dest, '@');
            if (ptr)
                *ptr = '\0';
            ptr = strchr(dest, '.');
            if (ptr)
            {
                *ptr++ = '\0';
                if (atoi(ptr) != 0)
                    topt = ptr;
            }
        }
        if (orig[0] && dest[0])
        {
            strcpy(pctrl, "\001" "INTL ");
            pctrl += strlen(pctrl);
            strcpy(pctrl, dest);
            pctrl += strlen(pctrl);
            strcpy(pctrl, " ");
            pctrl++;
            strcpy(pctrl, orig);
            pctrl += strlen(pctrl);
        }
        if (fmpt)
            addkludge(&pctrl, "\001" "FMPT ", "", fmpt, 0);
        if (topt)
            addkludge(&pctrl, "\001" "TOPT ", "", topt, 0);
        orig[0] = dest[0] = '\0';
    }

    SubPos = 0;

    sfl = msgh->SubFieldPtr;

    SubField = &(sfl->subfield[0]);
    for (i = 0; i < sfl->subfieldCount; i++, SubField++)
    {

        if (SubField->LoID == JAMSFLD_MSGID)
            addkludge(&pctrl, "\001MSGID: ", (char *) SubField->Buffer, "", SubField->DatLen);
        else if (SubField->LoID == JAMSFLD_REPLYID)
            addkludge(&pctrl, "\001REPLY: ", (char *) SubField->Buffer, "", SubField->DatLen);
        else if (SubField->LoID == JAMSFLD_PID)
            addkludge(&pctrl, "\001PID: ", (char *) SubField->Buffer, "", SubField->DatLen);
        else if (SubField->LoID == JAMSFLD_TRACE)
            addkludge(&plctrl, "\001Via ", (char *) SubField->Buffer, "\r", SubField->DatLen);
        else if (SubField->LoID == JAMSFLD_FTSKLUDGE)
        {
            if (strncasecmp((char *) SubField->Buffer, "Via", 3) == 0 ||
                strncasecmp((char *) SubField->Buffer, "Recd", 4) == 0)
                addkludge(&plctrl, "\001", (char *) SubField->Buffer, "\r", SubField->DatLen);
            else
                addkludge(&pctrl, "\001", (char *) SubField->Buffer, "", SubField->DatLen);
        }
        else if (SubField->LoID == JAMSFLD_FLAGS)
            addkludge(&pctrl, "\001" "FLAGS ", (char *) SubField->Buffer, "", SubField->DatLen);
        else if (SubField->LoID == JAMSFLD_PATH2D)
            addkludge(&plctrl, "\001PATH: ", (char *) SubField->Buffer, "\r", SubField->DatLen);
        else if (SubField->LoID == JAMSFLD_SEENBY2D)
            addkludge(&plctrl, "SEEN-BY: ", (char *) SubField->Buffer, "\r", SubField->DatLen);
        else if (SubField->LoID == JAMSFLD_TZUTCINFO)
            addkludge(&pctrl, "\001TZUTC: ", (char *) SubField->Buffer, "", SubField->DatLen);
    }
    msgh->clen = pctrl - (char *)msgh->ctrl;
    msgh->lclen = plctrl - (char *)msgh->lctrl;
    assert(msgh->clen < msgh->SubFieldPtr->arraySize + 65);
    assert(msgh->lclen < msgh->SubFieldPtr->arraySize + 65);
    msgh->ctrl = (unsigned char *)realloc(msgh->ctrl, msgh->clen + 1);
    msgh->lctrl = (unsigned char *)realloc(msgh->lctrl, msgh->lclen + 1);
}

/***********************************************************************
**
**  Crc32 lookup table
**
***********************************************************************/
static long crc32tab[256] =
{
    0L, 1996959894L, -301047508L, -1727442502L, 124634137L,
    1886057615L, -379345611L, -1637575261L, 249268274L, 2044508324L,
    -522852066L, -1747789432L, 162941995L, 2125561021L, -407360249L,
    -1866523247L, 498536548L, 1789927666L, -205950648L, -2067906082L,
    450548861L, 1843258603L, -187386543L, -2083289657L, 325883990L,
    1684777152L, -43845254L, -1973040660L, 335633487L, 1661365465L,
    -99664541L, -1928851979L, 997073096L, 1281953886L, -715111964L,
    -1570279054L, 1006888145L, 1258607687L, -770865667L, -1526024853L,
    901097722L, 1119000684L, -608450090L, -1396901568L, 853044451L,
    1172266101L, -589951537L, -1412350631L, 651767980L, 1373503546L,
    -925412992L, -1076862698L, 565507253L, 1454621731L, -809855591L,
    -1195530993L, 671266974L, 1594198024L, -972236366L, -1324619484L,
    795835527L, 1483230225L, -1050600021L, -1234817731L, 1994146192L,
    31158534L, -1731059524L, -271249366L, 1907459465L, 112637215L,
    -1614814043L, -390540237L, 2013776290L, 251722036L, -1777751922L,
    -519137256L, 2137656763L, 141376813L, -1855689577L, -429695999L,
    1802195444L, 476864866L, -2056965928L, -228458418L, 1812370925L,
    453092731L, -2113342271L, -183516073L, 1706088902L, 314042704L,
    -1950435094L, -54949764L, 1658658271L, 366619977L, -1932296973L,
    -69972891L, 1303535960L, 984961486L, -1547960204L, -725929758L,
    1256170817L, 1037604311L, -1529756563L, -740887301L, 1131014506L,
    879679996L, -1385723834L, -631195440L, 1141124467L, 855842277L,
    -1442165665L, -586318647L, 1342533948L, 654459306L, -1106571248L,
    -921952122L, 1466479909L, 544179635L, -1184443383L, -832445281L,
    1591671054L, 702138776L, -1328506846L, -942167884L, 1504918807L,
    783551873L, -1212326853L, -1061524307L, -306674912L, -1698712650L,
    62317068L, 1957810842L, -355121351L, -1647151185L, 81470997L,
    1943803523L, -480048366L, -1805370492L, 225274430L, 2053790376L,
    -468791541L, -1828061283L, 167816743L, 2097651377L, -267414716L,
    -2029476910L, 503444072L, 1762050814L, -144550051L, -2140837941L,
    426522225L, 1852507879L, -19653770L, -1982649376L, 282753626L,
    1742555852L, -105259153L, -1900089351L, 397917763L, 1622183637L,
    -690576408L, -1580100738L, 953729732L, 1340076626L, -776247311L,
    -1497606297L, 1068828381L, 1219638859L, -670225446L, -1358292148L,
    906185462L, 1090812512L, -547295293L, -1469587627L, 829329135L,
    1181335161L, -882789492L, -1134132454L, 628085408L, 1382605366L,
    -871598187L, -1156888829L, 570562233L, 1426400815L, -977650754L,
    -1296233688L, 733239954L, 1555261956L, -1026031705L, -1244606671L,
    752459403L, 1541320221L, -1687895376L, -328994266L, 1969922972L,
    40735498L, -1677130071L, -351390145L, 1913087877L, 83908371L,
    -1782625662L, -491226604L, 2075208622L, 213261112L, -1831694693L,
    -438977011L, 2094854071L, 198958881L, -2032938284L, -237706686L,
    1759359992L, 534414190L, -2118248755L, -155638181L, 1873836001L,
    414664567L, -2012718362L, -15766928L, 1711684554L, 285281116L,
    -1889165569L, -127750551L, 1634467795L, 376229701L, -1609899400L,
    -686959890L, 1308918612L, 956543938L, -1486412191L, -799009033L,
    1231636301L, 1047427035L, -1362007478L, -640263460L, 1088359270L,
    936918000L, -1447252397L, -558129467L, 1202900863L, 817233897L,
    -1111625188L, -893730166L, 1404277552L, 615818150L, -1160759803L,
    -841546093L, 1423857449L, 601450431L, -1285129682L, -1000256840L,
    1567103746L, 711928724L, -1274298825L, -1022587231L, 1510334235L,
    755167117L
};


/***********************************************************************
**
**  JAM_Crc32 - Calculate CRC32 on a data block
**
**  Note: This function returns data, NOT a status code!
**
***********************************************************************/

static dword Jam_Crc32(unsigned char *buff, dword len)
{
    dword crc = 0xffffffffUL;
    unsigned char *ptr = buff;

    for (; len; len--, ptr++)
        crc = (crc >> 8) ^ crc32tab[(int)((crc ^ tolower(*ptr)) & 0xffUL)];
    return crc;
}


static dword JamGetHash(HAREA mh, dword msgnum)
{
    XMSG xmsg;
    HMSG msgh;
    dword rc = 0l;

    if ((msgh = JamOpenMsg(mh, MOPEN_READ, msgnum)) == NULL)
        return (dword) 0l;

    if (JamReadMsg(msgh, &xmsg, 0L, 0L, NULL, 0L, NULL) != (dword) -1)
    {
        rc = SquishHash(xmsg.to) | (xmsg.attr & MSGREAD) ? 0x80000000l : 0;
    }

    JamCloseMsg(msgh);

    msgapierr = MERR_NONE;
    return rc;
}

static UMSGID JamGetNextUid(HAREA ha)
{
    if (InvalidMh(ha))
        return 0L;

    if (!ha->locked)
    {
        msgapierr = MERR_NOLOCK;
        return 0L;
    }

    msgapierr = MERR_NONE;
    return ha->high_msg + 1;
}

#endif
