/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 *
 *  api_brow.h
 */

#ifndef __API_BROW_H__
#define __API_BROW_H__

#define BROWSE_ACUR   0x0001  /* Scan current area only */
#define BROWSE_ATAG   0x0002  /* Scan only marked areas */
#define BROWSE_AALL   0x0004  /* Scan ALL areas */

#define BROWSE_ALL    0x0008  /* Scan all messages in area */
#define BROWSE_NEW    0x0010  /* Scan only msgs since lastread */
#define BROWSE_SEARCH 0x0020  /* Search all msgs in area based on criteria */
#define BROWSE_FROM   0x0040  /* Search from msg#XX and up */

#define BROWSE_READ   0x0100  /* Show messages in full form */
#define BROWSE_LIST   0x0200  /* Display msg headers only */
#define BROWSE_QWK    0x0400  /* QWK file format download */

#define BROWSE_GETTXT 0x0800  /* We have to read msg text when scanning */
#define BROWSE_EXACT  0x1000  /* Match search strings EXACTLY, ie. use
                               * stricmp(), not stristr(). */
#define BROWSE_HASH   0x2000  /* Use hash compare for this one only */

#define BROWSE_AREA     (BROWSE_ACUR | BROWSE_ATAG | BROWSE_AALL)
#define BROWSE_TYPE     (BROWSE_ALL | BROWSE_NEW | BROWSE_SEARCH | BROWSE_FROM)
#define BROWSE_DISPLAY  (BROWSE_READ | BROWSE_LIST | BROWSE_QWK)

#define SF_HAS_ATTR   0x01
#define SF_NOT_ATTR   0x02
#define SF_OR         0x04
#define SF_AND        0x08

#define WHERE_TO      0x01
#define WHERE_FROM    0x02
#define WHERE_SUBJ    0x04
#define WHERE_BODY    0x08

#define WHERE_ALL  (WHERE_TO | WHERE_FROM | WHERE_SUBJ | WHERE_BODY)

#define SCAN_BLOCK_SDM  48      /* # of msgs to read in from scanfile at
                                 * once * from SCANFILE.DAT. */
#define SCAN_BLOCK_SQUISH 512   /* Same as above, but for xxxxxxxx.SQI */

#include "pack.h"

typedef struct _search
{
    struct _search *next;

    long attr;
    int flag;

    char *txt;
    char where;
}
SEARCH;

struct _browse;
typedef struct _browse BROWSE;

struct _browse
{
    char *path;
    word type;

    word bflag;
    dword bdata;
    SEARCH *first;
    char *nonstop;

    dword msgn;

    MSGA *sq;
    MSGH *m;
    XMSG msg;
    word matched;

    int (*Begin_Ptr) (BROWSE * b);
    int (*Status_Ptr) (BROWSE * b, char *aname, int colour);
    int (*Idle_Ptr) (BROWSE * b);
    int (*Display_Ptr) (BROWSE * b);
    int (*After_Ptr) (BROWSE * b);
    int (*End_Ptr) (BROWSE * b);
    int (*Match_Ptr) (BROWSE * b);
};

#include "unpack.h"

#endif
