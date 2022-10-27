#include <stdint.h>
#include <stddef.h>
#include "printf.h"
#include "util.h"
#include "sched.h"

extern char user_trampoline_sz[], user_trampoline[];
static struct sched_t s;

extern void jump_usermode(void *rip, void *stack);

void scheduler_init(void (*fptrs[])(), int sz)
{
    int i;
    memset(&s, 0, sizeof(s));
    for(i = 0; i < sz; i++)
	s.fptrs[i] = fptrs[i];
    s.fptrs_sz = sz;
}
void scheduler()
{
    int current;
    printf("In Scheduler\n");

    current = s.fidx;
    s.fidx++;
    if(s.fidx > s.fptrs_sz) {
	printf("Completed\n");
	while(1);
    }
    memcpy(0x200000, (void *)user_trampoline,
	   (int)user_trampoline_sz);
    *(uint64_t *)(0x200000) = s.fptrs[current];
    hexdump(0x200000, 32);
    jump_usermode(0x200000 + 0x08, 0x200000 + 0x3000);
}
