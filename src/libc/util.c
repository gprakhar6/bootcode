#include <stdint.h>
#include "util.h"
#include "printf.h"

void *memset (void *dest, register int val, register size_t len)
{
    register unsigned char *ptr = (unsigned char*)dest;
    while (len-- > 0)
	*ptr++ = val;
    return dest;
}

void *memcpy (void *dest, void *src, register size_t len)
{
    register unsigned char *dptr = (unsigned char*)dest;
    register unsigned char *sptr = (unsigned char*)src;
    while (len-- > 0)
	*dptr++ = *sptr++;
    return dest;
}

void hexdump(void *ptr, int len)
{
    register unsigned char *cptr = (unsigned char*)ptr;
    while (len-- > 0) {
	printf("%02X ", *cptr++);
	if(len % 8 == 0)
	    printf("\n");
    }
    printf("\n");
}

