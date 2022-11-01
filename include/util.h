#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>
#include <stddef.h>

#include "hw_types.h"

volatile static inline void mutex_init(mutex_t *m)
{
    asm volatile("movq $1, (%%rax)" :: "a"(m));
}

volatile static inline void mutex_unlock(mutex_t *m)
{
    uint64_t rax = 1;
    asm volatile("xchgq %0, (%1)\n":"+a"(rax):"r"(m));
}

volatile static inline void mutex_lock_busy_wait(mutex_t *m)
{
    uint64_t rax = 0;
    while(rax == 0)
	asm volatile("xchgq %0, (%1)\n":"+a"(rax):"r"(m));
}

// successful returns 1 else 0
volatile static inline uint64_t mutex_lock(mutex_t *m)
{
    uint64_t rax = 0;
    asm volatile("xchgq %0, (%1)\n":"+a"(rax):"r"(m));
    return rax;
}

static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
    /* There's an outb %al, $imm8  encoding, for compile-time constant port numbers that fit in 8b.  (N constraint).
     * Wider immediate constants would be truncated at assemble-time (e.g. "i" constraint).
     * The  outb  %al, %dx  encoding is the only option for all other cases.
     * %1 expands to %dx because  port  is a uint16_t.  %w1 could be used if we had the port number a wider C type */
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile ( "inb %1, %0"
                   : "=a"(ret)
                   : "Nd"(port) );
    return ret;
}

static inline void io_wait(void)
{
    outb(0x3f8, '\n');
}

extern uint64_t tsc(); // defined in boot.S
void *memset(void *s, int c, size_t n);
void *memcpy (void *dest, void *src, register size_t len);
void hexdump(void *ptr, int len);
#endif
