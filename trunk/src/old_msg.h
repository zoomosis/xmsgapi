/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 */

#ifndef __OLD_MSG_H__
#define __OLD_MSG_H__

#include "pack.h"

struct _omsg
{
    byte from[36];
    byte to[36];
    byte subj[72];
    byte date[20];               /* Obsolete/unused ASCII date information */
    word times;                  /* FIDO<tm>: Number of times read */
    sword dest;                  /* Destination node */
    sword orig;                  /* Origination node number */
    word cost;                   /* Unit cost charged to send the message */

    sword orig_net;              /* Origination network number */
    sword dest_net;              /* Destination network number */

    struct _stamp date_written;  /* When user wrote the msg */
    struct _stamp date_arrived;  /* When msg arrived on-line */

    word reply;                  /* Current msg is a reply to this msg nr */
    word attr;                   /* Attribute (behavior) of the message */
    word up;                     /* Next message in the thread */
};

#include "unpack.h"

#define OMSG_SIZE 190

#endif
