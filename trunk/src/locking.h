/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 */

#ifndef __LOCKING_H__
#define __LOCKING_H__

int waitlock(int handle, long ofs, long length);
int waitlock2(int handle, long ofs, long length, long t);

#ifndef __WATCOMC__
int lock(int handle, long ofs, long length);
int unlock(int handle, long ofs, long length);
#endif

#endif
