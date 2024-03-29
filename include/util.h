#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>
#include <stddef.h>

#include "hw_types.h"
#include "kvm_para.h"
#include "globvar.h"

#define RETRY_COUNT (32)

#define ATOMIC_INCQ_M(x) asm volatile("lock incq %0\n\t" ::"m"(x))
#define ATOMIC_DECQ_M(x) asm volatile("lock decq %0\n\t" ::"m"(x))
#define ATOMIC_INCQ(x) asm volatile("lock incq (%0)\n\t" ::"r"(x))
#define ATOMIC_DECQ(x) asm volatile("lock decq (%0)\n\t" ::"r"(x))

#define ARR_SZ_1D(x) (sizeof(x)/sizeof(x[0]))

#define GET_VMMCALL_NAME(_1,_2,_3,_4, NAME, ...) NAME
// upto 4 arguments rbx(a0), rcx(a1), rdx(a2), and rsi(a3)
// rax has system call and return
#define vmmcall(...) GET_VMMCALL_NAME(__VA_ARGS__,vmmcall4,vmmcall3,vmmcall2,vmmcall1)(__VA_ARGS__)

extern uint64_t barrier_word[], barrier_cond[];

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

volatile inline void reload_cr3()
{
    uint64_t rax = 0;
    asm volatile ("movq %%cr3, %0\n\t"
		  "movq %0, %%cr3" ::"a"(rax));
}

static inline void set_bit(uint64_t *r, uint64_t bn)
{
    asm volatile("lock bts %1, (%0)\n\t" :: "r"(r), "r"(bn));
}

// compiler for some reason, unable to see that this
// instruction modifies the *r location and does useless
// optimization
static inline void reset_bit(uint64_t *r, uint64_t bn)
{
    asm volatile("lock btr %1, (%0)\n\t" :: "r"(r), "r"(bn));
}

static inline int64_t lowest_set_bit(uint64_t *r)
{
    int64_t rax;
    asm volatile("bsf (%1), %0\n\t"
		 "jnz a_set_bit%=\n\t"
		 "mov $-1, %0\n\t"
		 "a_set_bit%=:\n\t": "=a"(rax) : "r"(r));
    return rax;
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
    asm volatile("movq %%rsp, %0\n"
		 "subq $1, %0\n"   // Temporary solution when rsp = X000 value
		 "shrq $12, %0\n"
		 "addq $1, %0\n"
		 "shlq $12, %0\n"
		 "movq -8(%0), %0\n": "=a"(rax));
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
		 "lock btr %0, 8(%2)\n\t"
		 "jmp check_v%=\n\t"
		 "skip_hlt%=: lock btr %0, (%2)\n\t"
		 :"+r"(id)
		 :"r"(v), "r"(c), "r"(retry_count));

    return;
}

volatile static inline void cond_set(cond_t *c, uint64_t v)
{
    uint64_t waiting_cpus, rbx, s = 0, intent_waiting;
    int64_t preempted_cpu, hlt_cpu;
    c->c = v;
    asm volatile ("sfence");
    while(c->intent_cpus != c->waiting_cpus) {
	intent_waiting = c->intent_cpus ^ c->waiting_cpus;
	preempted_cpu = lowest_set_bit(&intent_waiting);
	if(preempted_cpu != -1)
	    vmmcall(KVM_HC_SCHED_YIELD, preempted_cpu);
	asm volatile ("sfence");
    }
    waiting_cpus = c->waiting_cpus;
    while(waiting_cpus != 0) {
	hlt_cpu = lowest_set_bit(&waiting_cpus);
	if(hlt_cpu != -1)
	    vmmcall(KVM_HC_KICK_CPU, 0, hlt_cpu);
	reset_bit(&waiting_cpus, hlt_cpu);
    }

    /*
    asm volatile ("movq %0, 16(%2)\n\t"
		  // because bsf could cause load which
		  // can be reordered before movq store
		  "check_scan_for_intent%=:\n\t"
		  "sfence\n\t"
		  "bsf (%2), %1\n\t"
		  "movq %1, %4\n\t"
		  "jz no_cpu_with_intent%=\n\t"
		  "check_for_intent%=:\n\t"
		  "bt %1, (%2)\n\t"
		  "jnc check_scan_for_intent%=\n\t"
		  "bt %1, 8(%2)\n\t"
		  "jc yield_once_and_kick%=\n\t"
		  "mov %4, %3\n\t"
		  "mov $11, %0\n\t" // yield
		  "vmmcall\n\t"
		  "jmp check_for_intent%=\n\t"
		  "yield_once_and_kick%=:\n\t"
		  "mov %4, %3\n\t"
		  "mov $11, %0\n\t" // yield
		  "vmmcall\n\t"
		  "movq %4, %1\n\t"
		  "xorq %3, %3\n\t"
		  "mov $5, %0\n\t"  // kick
		  "vmmcall\n\t"
		  "jmp check_scan_for_intent%=\n\t"
		  "no_cpu_with_intent%=:\n\t"
		  :"+a"(v), "=c"(waiting_cpu)
		  : "d"(c), "b"(rbx), "r"(s));
    */
}

volatile static inline void mutex_init(mutex_t *m)
{
    asm volatile("movq $0, %0" :"+m"(m->m)::"memory");
}

static inline void mutex_lock(mutex_t *m)
{
    uint64_t id, retry_cntr;
    id = (uint64_t)1 << get_id();
    asm volatile("inire%=:        movq     $3072, %1      \n\t" //  RETRY COUNT
		 "retry%=:        xorq     %%rax, %%rax   \n\t"
                 "           lock cmpxchgq %2, %0         \n\t"
                 "                jz       ret%=          \n\t"
                 "                decq     %1             \n\t"
		 "                pause                   \n\t"
                 "                jnz      retry%=        \n\t"
                 "                bsfq     %0, %%rbx      \n\t"
                 "                movq     $11,%%rax      \n\t"
                 "                vmmcall                 \n\t" // yield to Lock Holder
                 "                jmp      inire%=        \n\t"
                 "ret%=:                                  \n\t"
                 : "+m"(m->m), "+m"(retry_cntr)
                 : "rm"(id)
                 : "rax", "rbx", "memory");
}

static inline void mutex_unlock(mutex_t *m)
{
    asm volatile("movq $0, %0 \n\t": "+m"(m->m));
}

void mutex_unlock_hlt(mutex_t *m);
void mutex_lock_hlt(mutex_t *m);

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
	//printf("%d: c->intent_cpus %lX, waiting_cpus = %lX\n", id, ((cond_t *)barrier_cond)->intent_cpus, ((cond_t *)barrier_cond)->waiting_cpus);
	cond_set(barrier_cond, 1);
	//printf("kick_everyone\n");
	//printf("%d: c->intent_cpus %lX, waiting_cpus = %lX\n", id, ((cond_t *)barrier_cond)->intent_cpus, ((cond_t *)barrier_cond)->waiting_cpus);
    }
    else {
	//printf("cond_wait :%d\n", id);
	cond_wait(barrier_cond, 1);
    }
    ATOMIC_DECQ(barrier_word);
    if(*barrier_word == 0) {
	//printf("%d lset\n", id);
	cond_set(barrier_cond, 0);
    }
}


// extract bit and clear set bit from 64 bit memory area,
// without lock
static inline int64_t e_bsf(uint64_t *m) {
    int64_t rax,f;
    f = -1;
    asm volatile("new%=:     bsf     %0, %1       \n\t"
                 "           jz      ret_ns%=     \n\t"
                 "      lock btrq    %1, %0       \n\t"
                 "           jnc     new%=        \n\t"
                 "ret_ns%=:  cmovz   %2, %1       \n\t"
                 : "+m" (*m), "=a"(rax)
                 : "r"(f)
                 : "memory");

    return rax;
}

static inline uint64_t CAS(uint64_t *dst, uint64_t rax, uint64_t src) {
    asm volatile("       lock cmpxchgq    %2, %0    \n\t"
		 : "+m"(*dst), "+a"(rax)
		 : "r"(src)
		 : "memory");
    return rax;
}

// double cas
static inline uint64_t DCAS(uint64_t dst[2], uint64_t rdx_rax[2],
			    uint64_t rcx_rbx[2]) {
    uint64_t rax, f, s;
    f = 0;
    s = 1;
    rax = rdx_rax[0];
    asm volatile("       lock cmpxchg16b  (%1)    \n\t"
		 "            cmovz       %6, %0  \n\t"
		 "            cmovnz      %5, %0  \n\t"
		 : "+a"(rax)
		 : "r" (dst), "d"(rdx_rax[1]), "c"(rcx_rbx[1]),
		   "b"(rcx_rbx[0]),
		   "r"(f), "r"(s)
		 : "memory");
    return rax;
    
}

static inline uint64_t xadd(int64_t *m, int64_t rax) {
    asm volatile("lock xaddq %1, %0    \n\t"
		 :"+m"(*m), "+a"(rax)
		 :
		 : "memory");

    return rax;
}
static inline void halt()
{
    asm volatile("movw    $0x3fa, %%dx\n\t"
		 "out     %%al, (%%dx)\n\t"::: "dx");
}

uint64_t get_kern_stack();
extern uint64_t tsc(); // defined in boot.S
void *memset(void *s, int c, size_t n);
void *memcpy (void *dest, void *src, register size_t len);
void hexdump(void *ptr, int len);
#endif
