/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 *
 *  api_sqp.h
 */

#ifndef __API_SQP_H__
#define __API_SQP_H__

sword apiSquishCloseArea(HAREA sq);
HMSG apiSquishOpenMsg(HAREA sq, word mode, dword msgnum);
sword apiSquishCloseMsg(HMSG msgh);
dword apiSquishReadMsg(HMSG msgh, PXMSG msg, dword offset,
  dword bytes, byte * szText, dword clen, byte * ctxt);
sword apiSquishWriteMsg(HMSG msgh, word append, PXMSG msg,
  byte * text, dword textlen, dword totlen, dword clen, byte * ctxt);
sword apiSquishKillMsg(HAREA sq, dword msgnum);
sword apiSquishLock(HAREA sq);
sword apiSquishUnlock(HAREA sq);
sword apiSquishSetCurPos(HMSG msgh, dword pos);
dword apiSquishGetCurPos(HMSG msgh);
UMSGID apiSquishMsgnToUid(HAREA sq, dword msgnum);
dword apiSquishUidToMsgn(HAREA sq, UMSGID umsgid, word type);
dword apiSquishGetHash(HAREA sq, dword msgnum);
dword apiSquishGetHighWater(HAREA mh);
sword apiSquishSetHighWater(HAREA sq, dword hwm);
dword apiSquishGetTextLen(HMSG msgh);
dword apiSquishGetCtrlLen(HMSG msgh);
UMSGID apiSquishGetNextUid(HAREA ha);

/* Private functions */

unsigned _SquishReadMode(HMSG hmsg);
unsigned _SquishWriteMode(HMSG hmsg);
unsigned _SquishCopyBaseToData(HAREA ha, SQBASE * psqb);
unsigned _SquishWriteBaseHeader(HAREA ha, SQBASE * psqb);
unsigned _SquishReadBaseHeader(HAREA ha, SQBASE * psqb);
unsigned _SquishExclusiveBegin(HAREA ha);
unsigned _SquishExclusiveEnd(HAREA ha);
unsigned _SquishCopyDataToBase(HAREA ha, SQBASE * psqb);
unsigned _SquishReadHdr(HAREA ha, FOFS fo, SQHDR * psqh);
unsigned _SquishWriteHdr(HAREA ha, FOFS fo, SQHDR * psqh);
FOFS _SquishGetFrameOfs(HAREA ha, dword dwMsg);
unsigned _SquishSetFrameNext(HAREA ha, FOFS foModify, FOFS foValue);
unsigned _SquishSetFramePrev(HAREA ha, FOFS foModify, FOFS foValue);
unsigned _SquishInsertFreeChain(HAREA ha, FOFS fo, SQHDR * psqh);
HIDX _SquishOpenIndex(HAREA ha);
int _SquishBeginBuffer(HIDX hix);
int SidxGet(HIDX hix, dword dwMsg, SQIDX * psqi);
int SidxPut(HIDX hix, dword dwMsg, SQIDX * psqi);
unsigned _SquishRemoveIndexEntry(HIDX hix, dword dwMsg, SQIDX * psqiOut,
  SQHDR * psqh, int fFixPointers);
unsigned _SquishCloseIndex(HIDX hix);
int _SquishEndBuffer(HIDX hix);
int _SquishFreeBuffer(HIDX hix);
dword _SquishIndexSize(HIDX hix);
unsigned _SquishFixMemoryPointers(HAREA ha, dword dwMsg, SQHDR * psqh);

#endif
