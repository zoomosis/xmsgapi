/*
 *  XMSGAPI; eXtended MsgAPI
 *
 *  Please refer to the file named LICENCE for copyright information.
 */

#ifndef __PROG_H__
#define __PROG_H__

#include <stdio.h>
#include <time.h>
#include "compiler.h"
#include "typedefs.h"
#include "stamp.h"

#ifndef TRUE
#define FALSE  0
#define TRUE   1
#endif

/* Default separator for path specification */

#define ZONE_ALL  56685u
#define NET_ALL   56685u
#define NODE_ALL  56685u
#define POINT_ALL 56685u

#define eqstr(str1, str2)      (strcmp((str1), (str2)) == 0)
#define eqstri(str1, str2)     (stricmp((str1), (str2)) == 0)
#define eqstrn(str1, str2, n)  (strncmp((str1), (str2), (n)) == 0)

#ifndef min
#define min(a,b)              (((a) < (b)) ? (a) : (b))
#endif

#endif
