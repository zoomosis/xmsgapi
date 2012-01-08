/*
 *  For determining correct struct sizes of XMSGAPI.
 */

#include <stdio.h>
#include "../src/msgapi.h"

static char *check_size(size_t size_have, size_t size_want)
{
    static char str[80];
    
    if (size_have == size_want)
    {
	sprintf(str, "%d (OK)", (int) size_have);
    }
    else
    {
	sprintf(str, "%d (Error: Should be %d!)", (int) size_have, (int) size_want);
    }
    
    return str;
}

int main(void)
{
    printf("checking size of XMSG... %s\n", check_size(sizeof(XMSG), 238));
    printf("checking size of struct _stamp... %s\n", check_size(sizeof(struct _stamp), 4));

    return 0;
}
