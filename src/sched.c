#include <stdint.h>
#include <stddef.h>
#include "printf.h"
#include "util.h"
#include "sched.h"

#include "../../kvm-start/runtime_if.h" // TBD

extern char user_trampoline_sz[], user_trampoline[];
extern char boot_p4[], kern_end[];

static struct sched_t s;
struct t_pg_tbls *pg_tbls = (struct t_pg_tbls *)boot_p4;
struct t_metadata *metadata = (struct t_metadata *)0x0008;

extern void jump_usermode(void *rip, void *stack);

void scheduler_init(void (*fptrs[])(), int sz)
{

}
void scheduler()
{
    uint8_t id;
    char *ins;
    id = get_id();
    if(id != 0) {
	//printf("halting %d\n", id);
	asm("hlt");
    }
    else
	outw(PORT_MSG, MSG_BOOTED);

    while(1);
    //printf("In Scheduler\n");
    pg_tbls[id].tbl[1].e[2] = metadata->func_info[0].pt_addr | 0x07;
    /*
    printf("Jumping to address = %lX\n stack_addr = %lX	\
page_table = %lX\n",
	   metadata->func_info[0].entry_addr,
	   metadata->func_info[0].stack_load_addr,
	   pg_tbls[id].tbl[1].e[2]);
    ins = metadata->func_info[0].entry_addr;
    hexdump(metadata->func_info[0].pt_addr, 32);
    hexdump(ins, 32);
    */
    jump_usermode(metadata->func_info[0].entry_addr,
		  metadata->func_info[0].stack_load_addr);

    while(1);
}
