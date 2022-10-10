#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>
#include <stddef.h>

extern uint64_t tsc(); // defined in boot.S
void *memset(void *s, int c, size_t n);
void *memcpy (void *dest, void *src, register size_t len);
void hexdump(void *ptr, int len);
#endif
