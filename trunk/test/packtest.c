/*
 *  For determining correct struct sizes of XMSGAPI.
 */

#include <stdio.h>
#include "../src/msgapi.h"

int main(void)
{
    printf("sizeof(XMSG) == %d (should be %d)\n", sizeof(XMSG), 238);
    printf("sizeof(struct _stamp) == %d (should be %d)\n",
      sizeof(struct _stamp), 4);
    return 0;
}

