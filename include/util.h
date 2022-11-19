#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>
#include <stddef.h>

#include "hw_types.h"
#include "kvm_para.h"
#include "globvar.h"

#define ATOMIC_INCQ(x) asm("lock\n incq (%0)\n" ::"r"(x))
#define ATOMIC_DECQ(x) asm("lock\n decq (%0)\n" ::"r"(x))

extern uint64_t barrier_word[], barrier_flag[], barrier_mutex[];

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

volatile static inline uint8_t blog2(uint64_t x)
{
    uint8_t id = 0;
    while((x = x >> 1) != 0)
	id++;
    return id;
}
volatile static inline void mutex_init(mutex_t *m)
{
    asm volatile("movq $1, (%%rax)" :: "a"(&m->m));
}

volatile static inline void mutex_unlock(mutex_t *m)
{
    uint64_t rax = 1;
    asm volatile("xchgq %0, (%1)\n":"+a"(rax):"r"(&m->m));
}

volatile static inline void mutex_unlock_hlt(mutex_t *m)
{
    uint64_t rax = 1, id;
    uint8_t wake_cpu;
    asm volatile("xchgq %0, (%1)\n":"+a"(rax):"r"(&m->m));
    id = m->waiting_cpus;
    if(id != 0) {
	wake_cpu = blog2((id & (-id)));
	printf("waking cpu %d\n", wake_cpu);
	vmmcall(KVM_HC_KICK_CPU, 0, wake_cpu);
    }
}

volatile static inline void mutex_lock_hlt(mutex_t *m)
{
    uint64_t rax = 0;
    uint64_t id;

    asm volatile("xchgq %0, (%1)\n":"+a"(rax):"r"(&m->m));
    if(rax == 0) {
	id = ((uint64_t)0x1 << get_id());
	asm volatile ("lock\n"
		      "orq %0, (%1)\n"
		      "hlt": "+r"(id) : "r"(&m->waiting_cpus));
    }
    else
	goto out;

loop:
    asm volatile("xchgq %0, (%1)\n":"+a"(rax):"r"(&m->m));
    if(rax == 0) {
	// this case shouldnt happen
	goto loop;
    }
    else {
	// i got the lock, remove the waiting bit
	id = ~id;
	asm volatile ("lock\n"
		      "andq %0, (%1)": "+r"(id) : "r"(&m->waiting_cpus));	
    }
out:
    return;
}

volatile static inline void mutex_lock_busy_wait(mutex_t *m)
{
    uint64_t rax = 0;
    uint64_t id;
    id = ((uint64_t)0x1 << get_id());
    while(rax == 0) {
	//asm volatile("movq %0, %%r15"::"r"(m->m));
	//outb(PORT_HLT, 0);
	asm volatile("xchgq %0, (%1)":"+a"(rax):"r"(&m->m));
	/*
	if(rax != 0)
	    asm("hlt\n");
	*/
    }
    //asm volatile("orq %0, (%1)" :"+r"(id):"r"(&m->waiting_cpus));
	
}

// successful returns 1 else 0
volatile static inline uint64_t mutex_lock(mutex_t *m)
{
    uint64_t rax = 0;
    asm volatile("xchgq %0, (%1)\n":"+a"(rax):"r"(&m->m));
    return rax;
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
