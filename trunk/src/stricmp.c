/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 *
 *  stricmp.c
 *
 *  Released to the public domain.
 */

#include <ctype.h>
#include "stricmp.h"

int stricmp(const char *s, const char *t)
{
    while (*s != '\0')
    {
        int rc;
        rc = tolower((unsigned char)*s) - tolower((unsigned char)*t);
        if (rc != 0)
        {
            return rc;
        }
        s++;
        t++;
    }

    if (*t != '\0')
    {
        return -tolower((unsigned char)*t);
    }

    return 0;
}
