/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 *
 *  patmat.h
 *
 *  Needed for ffind.c, only in UNIX.
 */

#ifndef __PATMAT_H__
#define __PATMAT_H__

#ifdef __cplusplus
extern "C"
{
#endif

int patmat(char *raw, char *pat);

#ifdef __cplusplus
}
#endif

#endif
