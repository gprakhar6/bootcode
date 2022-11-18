#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>
#include <stddef.h>

#include "hw_types.h"

#define ATOMIC_INCQ(x) asm("lock\n incq (%0)\n" ::"r"(x))
#define ATOMIC_DECQ(x) asm("lock\n decq (%0)\n" ::"r"(x))

extern uint64_t barrier_word[], barrier_flag[], barrier_mutex[];

volatile static inline uint16_t get_pool_and_id()
{
    uint64_t rax;
    asm("movq %%rsp, %0\n"
	"shrq $12, %0\n"
	"addq $1, %0\n"
	"shlq $12, %0\n"
	"movq (%0), %0" : "=r"(rax));
    return (uint16_t)rax;
}

volatile static inline uint8_t get_pool_sz()
{
    uint64_t rax;
    asm("movq %%rsp, %0\n"
	"shrq $12, %0\n"
	"addq $1, %0\n"
	"shlq $12, %0\n"
	"movq (%0), %0" : "=r"(rax));
    return (uint8_t)((rax & 0xFF00) >> 8);
}

volatile static inline uint8_t get_id()
{
    uint64_t rax;
    asm("movq %%rsp, %0\n"
	"shrq $12, %0\n"
	"addq $1, %0\n"
	"shlq $12, %0\n"
	"movq (%0), %0" : "=r"(rax));
    return (uint8_t)(rax & 0x00FF);
}

volatile static inline void mutex_init(mutex_t *m)
{
    asm volatile("movq $1, (%%rax)" :: "a"(m));
}

volatile static inline void mutex_unlock(mutex_t *m)
{
    uint64_t rax = 1;
    asm volatile("xchgq %0, (%1)\n":"+a"(rax):"r"(m));
}

volatile static inline void mutex_lock_pause(mutex_t *m)
{
    uint64_t rax = 0;
loop:
    asm volatile("xchgq %0, (%1)\n":"+a"(rax):"r"(m));
    if(rax == 0) {
	asm volatile("pause");
	goto loop;
    }
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

inline void outb(uint16_t port, uint8_t val)
{
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
    /* There's an outb %al, $imm8  encoding, for compile-time constant port numbers that fit in 8b.  (N constraint).
     * Wider immediate constants would be truncated at assemble-time (e.g. "i" constraint).
     * The  outb  %al, %dx  encoding is the only option for all other cases.
     * %1 expands to %dx because  port  is a uint16_t.  %w1 could be used if we had the port number a wider C type */
}

inline uint8_t inb(uint16_t port)
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

static inline void barrier()
{
    uint8_t ncpus = get_pool_sz();
    while(*barrier_flag != 0) asm("pause");

    ATOMIC_INCQ(barrier_word);
    if(*barrier_word == ncpus) {
	*barrier_flag = 1;
    }
    else {
	while(*barrier_flag != 1) asm("pause");
    }
    ATOMIC_DECQ(barrier_word);
    if(*barrier_word == 0)
	*barrier_flag = 0;
}

extern uint64_t tsc(); // defined in boot.S
void *memset(void *s, int c, size_t n);
void *memcpy (void *dest, void *src, register size_t len);
void hexdump(void *ptr, int len);
#endif
