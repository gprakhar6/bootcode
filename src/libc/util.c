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

void mutex_unlock_hlt(mutex_t *m)
{
    uint64_t rax = 1, waiting_cpu, rbx;

    asm volatile ("mutex_unlock_hlt%=:\n\t"
		  "lock xchgq %0, 16(%2)\n\t"
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
    // TBD rbx is clobber register to remove warning
}

void mutex_lock_hlt(mutex_t *m)
{
    uint64_t rax, rbx = 0;
    uint64_t id, retry_count = RETRY_COUNT; // TBD Tuning

    id = get_id();
    asm volatile("mutex_lock_hlt%=:\n\t"
		 "mfence\n\t"
		 "lock bts %0, (%2)\n\t"
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

// interrupt safety is an issue right now. Ah!
uint64_t get_kern_stack()
{
    uint64_t rax;
    asm volatile("swapgs\n\t"
		 "movq %%gs:0, %0\n\t"
		 "swapgs\n\t": "=a"(rax));
    
    return rax;
}
