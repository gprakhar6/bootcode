#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>
#include <stddef.h>

#include "hw_types.h"
#include "kvm_para.h"
#include "globvar.h"

#define RETRY_COUNT (32)

#define ATOMIC_INCQ(x) asm("lock\n incq (%0)\n" ::"r"(x))
#define ATOMIC_DECQ(x) asm("lock\n decq (%0)\n" ::"r"(x))

extern uint64_t barrier_word[], barrier_cond[];

#define GET_VMMCALL_NAME(_1,_2,_3,_4, NAME, ...) NAME
// upto 4 arguments rbx, rcx, rdx, and rsi
// rax has system call and return
#define vmmcall(...) GET_VMMCALL_NAME(__VA_ARGS__,vmmcall4,vmmcall3,vmmcall2,vmmcall1)(__VA_ARGS__)

volatile inline uint64_t vmmcall3(uint64_t id, uint64_t rbx, uint64_t rcx)
{
    uint64_t ret;
    asm volatile ("vmmcall" :"=a"(ret): "a"(id), "b"(rbx) , "c"(rcx): "memory");
    return ret;
}

volatile inline uint64_t vmmcall2(uint64_t id, uint64_t rbx)
{
    uint64_t ret;
    asm volatile ("vmmcall" :"=a"(ret): "a"(id), "b"(rbx) : "memory");
    return ret;
}

volatile inline uint64_t vmmcall1(uint64_t id)
{
    uint64_t ret;
    asm volatile ("vmmcall" :"=a"(ret): "a"(id) : "memory");
    return ret;
}

inline void outw(uint16_t port, uint16_t val)
{
    asm volatile ( "outw %0, %1" : : "a"(val), "Nd"(port) );
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

volatile static inline uint16_t get_pool_and_id()
{
    uint64_t rax;
    asm("movq %%rsp, %0\n"
	"shrq $12, %0\n"
	"addq $1, %0\n"
	"shlq $12, %0\n"
	"movq -8(%0), %0" : "=r"(rax));
    return (uint16_t)rax;
}

volatile static inline uint8_t get_pool_sz()
{
    uint64_t rax;
    asm("movq %%rsp, %0\n"
	"shrq $12, %0\n"
	"addq $1, %0\n"
	"shlq $12, %0\n"
	"movq -8(%0), %0" : "=r"(rax));
    return (uint8_t)((rax & 0xFF00) >> 8);
}

volatile static inline uint8_t get_id()
{
    uint64_t rax;
    asm("movq %%rsp, %0\n"
	"shrq $12, %0\n"
	"addq $1, %0\n"
	"shlq $12, %0\n"
	"movq -8(%0), %0" : "=r"(rax));
    return (uint8_t)(rax & 0x00FF);
}

// use bsf instead or other bit search  instruction
volatile static inline uint8_t blog2(uint64_t x)
{
    uint8_t id = 0;
    while((x = x >> 1) != 0)
	id++;
    return id;
}

volatile static inline void cond_wait(cond_t *c, uint64_t v)
{
    uint64_t id, retry_count = RETRY_COUNT; // TBD Tuning
    id = get_id();
    asm volatile("lock bts %0, (%2)\n\t"
		 "check_v%=: \n\t"
		 "cmp 16(%2), %1\n\t"
		 "je skip_hlt%=\n\t"
		 "test %3, %3\n\t"
		 "jz hlt_path%=\n\t"
		 "decq %3\n\t"
		 "jmp check_v%=\n\t"
		 "hlt_path%=:\n\t"
		 "lock bts %0, 8(%2)\n\t"
		 "hlt\n\t"
		 "jmp check_v%=\n\t"
		 "skip_hlt%=: lock btr %0, 8(%2)\n\t"
		 "lock btr %0, (%2)\n\t"
		 :"+r"(id)
		 :"r"(v), "r"(c), "r"(retry_count));

    return;
}

volatile static inline void cond_set(cond_t *c, uint64_t v)
{
    uint64_t waiting_cpu, rbx;

    asm volatile ("movq %0, 16(%2)\n\t"
		  // because bsf could cause load which
		  // can be reordered bedore movq store
		  "sfence\n\t"
		  "check_scan_for_intent%=:\n\t"
		  "bsf (%2), %1\n\t"
		  "jz no_cpu_with_intent%=\n\t"
		  "check_for_intent%=:\n\t"
		  "bt %1, (%2)\n\t"
		  "jnc check_scan_for_intent%=\n\t"
		  "bt %1, 8(%2)\n\t"
		  "jc yield_once_and_kick%=\n\t"
		  "mov %1, %3\n\t"
		  "mov $11, %0\n\t" // yield
		  "vmmcall\n\t"
		  "jmp check_for_intent%=\n\t"
		  "yield_once_and_kick%=:\n\t"
		  "mov %1, %3\n\t"		  
		  "mov $11, %0\n\t" // yield
		  "vmmcall\n\t"
		  "mov $5, %0\n\t"  // kick
		  "vmmcall\n\t"
		  "jmp check_scan_for_intent%=\n\t"
		  "no_cpu_with_intent%=:\n\t"
		  :"+a"(v), "=c"(waiting_cpu)
		  : "d"(c), "b"(rbx));
}

volatile static inline void mutex_init(mutex_t *m)
{
    asm volatile("movq $1, (%%rax)" :: "a"(&m->m));
}

volatile static inline void mutex_unlock_hlt(mutex_t *m)
{
    uint64_t rax = 1, waiting_cpu, rbx;

    asm volatile ("lock xchgq %0, 16(%2)\n\t"
		  "bsf (%2), %1\n\t"
		  "jz no_cpu_with_intent%=\n\t"
		  "check_for_intent%=:\n\t"
		  "bt %1, (%2)\n\t"
		  "jnc cpu_no_intent%=\n\t"
		  "bt %1, 8(%2)\n\t"
		  "jc yield_once_and_kick%=\n\t"
		  "mov %1, %3\n\t"
		  "mov $11, %0\n\t" // yield
		  "vmmcall\n\t"
		  "jmp check_for_intent%=\n\t"
		  "yield_once_and_kick%=:\n\t"
		  "mov %1, %3\n\t"
		  "mov $11, %0\n\t" // yield
		  "vmmcall\n\t"
		  "mov $5, %0\n\t"  // kick
		  "vmmcall\n\t"	 
		  "cpu_no_intent%=:\n\t"
		  "no_cpu_with_intent%=:\n\t"
		  :"+a"(rax), "=c"(waiting_cpu)
		  : "r"(m), "b"(rbx));
}

volatile static inline void mutex_lock_hlt(mutex_t *m)
{
    uint64_t rax, rbx = 0;
    uint64_t id, retry_count = RETRY_COUNT; // TBD Tuning

    id = get_id();
    asm volatile("lock bts %0, (%2)\n\t"
		 "try_lock%=: \n\t"
		 "movq $1, %1\n\t"
		 "lock cmpxchgq %4, 16(%2)\n\t"
		 "jz skip_hlt%=\n\t"
		 "test %3, %3\n\t"
		 "jz hlt_path%=\n\t"
		 "decq %3\n\t"
		 "jmp try_lock%=\n\t"
		 "hlt_path%=:\n\t"
		 "lock bts %0, 8(%2)\n\t"
		 "hlt\n\t"
		 "jmp try_lock%=\n\t"
		 "skip_hlt%=: lock btr %0, 8(%2)\n\t"
		 "lock btr %0, (%2)\n\t"
		 :"+r"(id)
		 :"a"(rax), "r"(m), "r"(retry_count), "b"(rbx));
    
    return;
}


static inline void io_wait(void)
{
    outb(0x3f8, '\n');
}

static inline void barrier()
{
    uint8_t ncpus = get_pool_sz();
    uint8_t id = get_id();
    cond_wait(barrier_cond, 0);

    ATOMIC_INCQ(barrier_word);
    if(*barrier_word == ncpus) {
	//printf("set called %d\n", id);
	cond_set(barrier_cond, 1);
    }
    else {
	//printf("cond_wait :%d\n", id);
	cond_wait(barrier_cond, 1);
    }
    //printf("%d: c->intent_cpus %lX\n", id, ((cond_t *)barrier_cond)->intent_cpus);
    ATOMIC_DECQ(barrier_word);
    if(*barrier_word == 0)
	cond_set(barrier_cond, 0);
}

extern uint64_t tsc(); // defined in boot.S
void *memset(void *s, int c, size_t n);
void *memcpy (void *dest, void *src, register size_t len);
void hexdump(void *ptr, int len);
#endif
