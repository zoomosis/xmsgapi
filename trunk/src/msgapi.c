/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "prog.h"
#include "msgapi.h"
#include "unused.h"

#ifdef __UNIX__
#include <signal.h>
#endif

#ifdef __DOS__
#include <dos.h>
#endif

static byte *intl = (byte *) "INTL";
static byte *fmpt = (byte *) "FMPT";
static byte *topt = (byte *) "TOPT";
static byte *area_colon = (byte *) "AREA:";

static char *copyright =
  "MSGAPI - Copyright 1991 by Scott J. Dudley.  All rights reserved.";

/* Global error value for message API routines */

word msgapierr = 0;
struct _minf mi;

#ifndef MSGAPI_NO_SQUISH
unsigned _SquishCloseOpenAreas(void);
#endif

void _MsgCloseApi(void)
{
#ifndef MSGAPI_NO_SQUISH
    _SquishCloseOpenAreas();
#endif
}

#ifdef __UNIX__
/* Just a dummy alarm-fnct */
static void alrm(int x)
{
    unused(x);
}
#endif

#ifdef __DOS__

static sword shareloaded(void)
{
    union REGS r;
    r.w.ax = 0x1000;
    int386(0x2f, &r, &r);
    return r.h.al == 0xff;
}

#endif

sword MsgOpenApi(struct _minf *minf)
{
#ifdef __UNIX__
    struct sigaction alrmact;
#endif

    unused(copyright);

    mi.req_version = minf->req_version;
    mi.def_zone = minf->def_zone;

#ifdef __DOS__
    minf->haveshare = shareloaded();
#else
    minf->haveshare = 1;
#endif

    mi.haveshare = minf->haveshare;

    /* Version 2 Requested */

    if (mi.req_version > 1 && mi.req_version < 50)
    {
        mi.xmsgapi_version = minf->xmsgapi_version = MSGAPI_VERSION;
        mi.xmsgapi_subversion = minf->xmsgapi_subversion = MSGAPI_SUBVERSION;
    }

    atexit(_MsgCloseApi);

    /* Set the dummy alarm-fcnt to supress stupid messages.  */

#ifdef __UNIX__
    memset(&alrmact, 0, sizeof alrmact);
    alrmact.sa_handler = alrm;
    sigaction(SIGALRM, &alrmact, 0);
#endif

    return 0;
}

sword MsgCloseApi(void)
{
    _MsgCloseApi();
    return 0;
}

MSGA *MsgOpenArea(byte * name, word mode, word type)
{
#ifndef MSGAPI_NO_SQUISH
    if (type & MSGTYPE_SQUISH)
    {
        return SquishOpenArea(name, mode, type);
    }
#endif

#ifndef MSGAPI_NO_JAM
    if (type & MSGTYPE_JAM)
    {
        return JamOpenArea(name, mode, type);
    }
#endif

#ifndef MSGAPI_NO_SDM
    if (type & MSGTYPE_SDM)
    {
        return SdmOpenArea(name, mode, type);
    }
#endif

    return NULL;
}

int MsgDeleteBase(char *name, word type)
{
#ifndef MSGAPI_NO_SQUISH
    if (type & MSGTYPE_SQUISH)
    {
        return SquishDeleteBase(name);
    }
#endif

#ifndef MSGAPI_NO_JAM
    if (type & MSGTYPE_JAM)
    {
        return JamDeleteBase(name);
    }
#endif

#ifndef MSGAPI_NO_SDM
    if (type & MSGTYPE_SDM)
    {
        return SdmDeleteBase(name);
    }
#endif

    return 0;
}

sword MsgValidate(word type, byte * name)
{
#ifndef MSGAPI_NO_SQUISH
    if (type & MSGTYPE_SQUISH)
    {
        return SquishValidate(name);
    }
#endif

#ifndef MSGAPI_NO_JAM
    if (type & MSGTYPE_JAM)
    {
        return JamValidate(name);
    }
#endif

#ifndef MSGAPI_NO_SDM
    if (type & MSGTYPE_SDM)
    {
        return SdmValidate(name);
    }
#endif

    return 0;
}

/*
 *  Check to see if a message handle is valid.  This function should work
 *  for ALL handlers tied into MsgAPI.  This also checks to make sure that
 *  the area which the message is from is also valid (ie. the message handle
 *  isn't valid unless the area handle of that message is also valid).
 */

sword MSGAPI InvalidMsgh(MSGH * msgh)
{
    if (msgh == NULL || msgh->id != MSGH_ID || InvalidMh(msgh->ha))
    {
        msgapierr = MERR_BADH;
        return TRUE;
    }

    return FALSE;
}

/* Check to ensure that a message area handle is valid. */

sword MSGAPI InvalidMh(MSGA * mh)
{
    if (mh == NULL || mh->id != MSGAPI_ID)
    {
        msgapierr = MERR_BADH;
        return TRUE;
    }

    return FALSE;
}

byte *StripNasties(byte * str)
{
    byte *p;

    p = str;
    while (*p != '\0')
    {
        if (*p < ' ')
        {
            *p = ' ';
        }
        p++;
    }

    return str;
}

/* Copy the text itself to a buffer, or count its length if out==NULL */

static word _CopyToBuf(byte * p, byte * out, byte ** end)
{
    word len = 0;

    if (out)
    {
        *out++ = '\001';
    }

    len++;

    while (*p == '\015' || *p == '\012' || *p == (byte) '\215')
    {
        p++;
    }

    while (*p == '\001' || strncmp((char *)p, (char *)area_colon, 5) == 0)
    {
        /* Skip over the first ^a */

        if (*p == '\001')
        {
            p++;
        }

        while (*p && *p != '\015' && *p != '\012' && *p != (byte) '\215')
        {
            if (out)
            {
                *out++ = *p;
            }

            p++;
            len++;
        }

        if (out)
        {
            *out++ = '\001';
        }

        len++;

        while (*p == '\015' || *p == '\012' || *p == (byte) '\215')
        {
            p++;
        }
    }

    /* Cap the string */

    if (out)
    {
        *out = '\0';
    }

    len++;

    /* Make sure to leave no trailing x01's. */

    if (out && out[-1] == '\001')
    {
        out[-1] = '\0';
    }

    /* Now store the new end location of the kludge lines */

    if (end)
    {
        *end = p;
    }

    return len;
}

byte *CopyToControlBuf(byte * txt, byte ** newtext, unsigned *length)
{
    byte *cbuf, *end;

    word clen;
    
    /* Figure out how long the control info is */

    clen = _CopyToBuf(txt, NULL, NULL);

    /* Allocate memory for it */

#define SAFE_CLEN 20

    cbuf = palloc(clen + SAFE_CLEN);
    if (cbuf == NULL)
    {
        return NULL;
    }

    memset(cbuf, '\0', clen + SAFE_CLEN);

    /* Now copy the text itself */

    _CopyToBuf(txt, cbuf, &end);

    if (length)
    {
        *length -= (size_t) (end - txt);
    }

    if (newtext)
    {
        *newtext = end;
    }

    return cbuf;
}

byte *GetCtrlToken(byte * where, byte * what)
{
    byte *end, *out;
    unsigned int len;

    if (where == NULL || what == NULL)
        return NULL;
    len = strlen((char *) what);

    do
    {
        where = (byte *) strchr((char *)where, '\001');
        if (where == NULL)
            break;
        where++;
    }
    while (strncmp((char *)where, (char *)what, len));

    if (where == NULL || strlen((char *) where) < len)
    {
        return NULL;
    }

    end = (byte *) strchr((char *)where, '\r');

    if (end == NULL)
    {
        end = (byte *) strchr((char *)where, '\001');
    }

    if (end == NULL)
    {
        end = where + strlen((char *)where);
    }

    out = palloc((size_t) (end - where) + 1);

    if (out == NULL)
    {
        return NULL;
    }

    memmove(out, where, (size_t) (end - where));
    out[(size_t) (end - where)] = '\0';

    return out;
}

static char *firstchar(char *strng, char *delim, int findword)
{
    int x, isw, sl_d, sl_s, wordno;
    char *string, *oldstring;

    /* We can't do *anything* if the string is blank... */

    if (!*strng)
    {
        return NULL;
    }

    oldstring = strng;

    sl_d = strlen(delim);

    for (string = strng; *string; string++)
    {
        for (x = 0, isw = 0; x <= sl_d; x++)
        {
            if (*string == delim[x])
            {
                isw = 1;
            }
        }

        if (isw == 0)
        {
            oldstring = string;
            break;
        }
    }

    sl_s = strlen(string);

    for (wordno = 0; string - oldstring < sl_s; string++)
    {
        for (x = 0, isw = 0; x <= sl_d; x++)
        {
            if (*string == delim[x])
            {
                isw = 1;
                break;
            }
        }

        if (!isw && string == oldstring)
        {
            wordno++;
        }

        if (isw && string != oldstring)
        {
            for (x = 0, isw = 0; x <= sl_d; x++)
            {
                if (*(string + 1) == delim[x])
                {
                    isw = 1;
                    break;
                }
            }

            if (isw == 0)
            {
                wordno++;
            }
        }

        if (wordno == findword)
        {
            if (string == oldstring || string == oldstring + sl_s)
            {
                return string;
            }
            else
            {
                return string + 1;
            }
        }
    }

    return NULL;
}

static void ParseNN(char *netnode, word * zone, word * net, word * node,
  word * point, word all)
{
    char *p;

    p = netnode;

    if (all && point)
    {
        *point = POINT_ALL;
    }

    if (all && toupper(*netnode) == 'W')
    {
        /* World */

        if (zone)
        {
            *zone = ZONE_ALL;
        }

        if (net)
        {
            *net = NET_ALL;
        }

        if (node)
        {
            *node = NODE_ALL;
        }

        return;
    }


    /* if we have a zone (and the caller wants the zone to be passed back) */

    if (strchr(netnode, ':'))
    {
        if (zone)
        {
            if (all && toupper(*p) == 'A')
            {
                /* All */
                *zone = ZONE_ALL;
            }
            else
            {
                *zone = (word) atoi(p);
            }
        }

        p = firstchar(p, ":", 2);
    }

    /* if we have a net number */

    if (p && *p)
    {
        if (strchr(netnode, '/'))
        {
            if (net)
            {
                if (all && toupper(*p) == 'A')
                {
                    /* All */
                    *net = NET_ALL;
                }
                else
                {
                    *net = (word) atoi(p);
                }
            }

            p = firstchar(p, "/", 2);
        }
        else if (all && toupper(*p) == 'A')
        {
            /* If it's in the form "1:All" or "All" */

            if (strchr(netnode, ':') == NULL && zone)
            {
                *zone = ZONE_ALL;
            }

            *net = NET_ALL;
            *node = NODE_ALL;
            p += 3;
        }
    }

    /* If we got a node number... */

    if (p && *p && node && *netnode != '.')
    {
        if (all && toupper(*p) == 'A')
        {
            /* All */

            *node = NODE_ALL;

            /* 1:249/All implies 1:249/All.All too... */

            if (point && all)
            {
                *point = POINT_ALL;
            }
        }
        else
        {
            *node = (word) atoi(p);
        }
    }

    if (p)
    {
        while (*p && isdigit((int)(*p)))
        {
            p++;
        }
    }

    /* And finally check for a point number... */

    if (p && *p == '.')
    {
        p++;

        if (point)
        {
            if (!p && *netnode == '.')
            {
                p = netnode + 1;
            }

            if (p && *p)
            {
                *point = (word) atoi(p);

                if (all && toupper(*p) == 'A')
                {
                    /* All */
                    *point = POINT_ALL;
                }
            }
            else
            {
                *point = 0;
            }
        }
    }
}

static void Parse_NetNode(char *netnode, word * zone, word * net, word * node, word * point)
{
    ParseNN(netnode, zone, net, node, point, FALSE);
}

void ConvertControlInfo(byte * ctrl, NETADDR * orig, NETADDR * dest)
{
    byte *p, *s;

    s = GetCtrlToken(ctrl, intl);

    if (s != NULL)
    {
        NETADDR norig, ndest;

        p = s;

        /* Copy the defaults from the original address */

        norig = *orig;
        ndest = *dest;

        /* Parse the destination part of the kludge */

        s += 5;

        Parse_NetNode((char *)s, &ndest.zone, &ndest.net, &ndest.node,
          &ndest.point);

        while (*s != ' ' && *s)
        {
            s++;
        }

        if (*s)
        {
            s++;
        }

        Parse_NetNode((char *)s, &norig.zone, &norig.net, &norig.node,
          &norig.point);

        pfree(p);

        /* 
         *  Only use this as the "real" zonegate address if the net/node
         *  addresses in the INTL line match those in the message body.
         *  Otherwise, it's probably a gaterouted message!
         */

        if (ndest.net == dest->net && ndest.node == dest->node &&
          norig.net == orig->net && norig.node == orig->node)
        {
            *dest = ndest;
            *orig = norig;
        }
    }

    /* Handle the FMPT kludge */

    s = GetCtrlToken(ctrl, fmpt);

    if (s != NULL)
    {
        orig->point = (word) atoi((char *)s + 5);
        pfree(s);
    }

    /* Handle TOPT too */

    s = GetCtrlToken(ctrl, topt);

    if (s != NULL)
    {
        dest->point = (word) atoi((char *)s + 5);
        pfree(s);
    }
}

word NumKludges(char *txt)
{
    word nk = 0;

    if (txt == NULL)
    {
        return 0;
    }

    while (*txt != '\0')
    {
        if (*txt == '\001')
        {
            nk++;
        }

        txt++;
    }

    return nk;
}

byte *CvtCtrlToKludge(byte * ctrl)
{
    byte *from, *to, *buf;
    size_t clen;

    clen = strlen((char *)ctrl) + NumKludges((char *)ctrl) + 20;

    buf = palloc(clen);

    if (buf == NULL)
    {
        return NULL;
    }

    to = buf;

    /* Convert ^aKLUDGE^aKLUDGE... into ^aKLUDGE\r^aKLUDGE\r... */

    from = ctrl;

    while (*from == '\001' && from[1])
    {
        /* Only copy out the ^a if it's NOT the area: line */

        if (!eqstrn((char *)from + 1, (char *)area_colon, 5))
        {
            *to++ = *from;
        }

        from++;

        while (*from && *from != '\001')
        {
            *to++ = *from++;
        }

        *to++ = '\r';
    }

    *to = '\0';

    return buf;
}

static char *strocpy(char *d, char *s)
{
    char *orig;
    orig = s;
    memmove(d, s, strlen(s) + 1);
    return orig;
}

void RemoveFromCtrl(byte * ctrl, byte * what)
{
    byte *p;
    unsigned int len = strlen((char *) what);

    while (1)
    {
        ctrl = (byte *) strchr((char *)ctrl, '\001');

        if (ctrl == NULL)
        {
            return;
        }
        
        if (strncmp((char *)ctrl + 1, (char *)what, len))
        {
            ctrl++;
            continue;
        }
        
        if (strlen((char *)ctrl + 1) < len)
        {
            return;
        }
        
        /* found */

        p = (byte *) strchr((char *)ctrl + 1, '\001');

        if (p == NULL)
        {
            *ctrl = '\0';
            return;
        }

        strocpy((char *)ctrl, (char *)p);
    }
}

char months[][10] =
{
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

char months_ab[][4] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

char weekday[][10] =
{
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};

char weekday_ab[][4] =
{
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
