#include <stdint.h>
#include <stddef.h>
#include "printf.h"
#include "util.h"
#include "sched.h"
#include "queue.h"

#include "../../kvm-start/runtime_if.h" // TBD

#define WORK_Q_ARR_SZ (256)
extern char user_trampoline_sz[], user_trampoline[];
extern char boot_p4[], kern_end[];

struct t_pg_tbls *pg_tbls = (struct t_pg_tbls *)boot_p4;
struct t_metadata *metadata = (struct t_metadata *)0x0008;

extern void jump_usermode(void *rip, void *stack);

t_queue work_q;
uint8_t work_q_arr[WORK_Q_ARR_SZ];

void scheduler_init()
{
    queue_init(&work_q, work_q_arr, ARR_SZ_1D(work_q_arr));
    metadata->bit_map_inactive_cpus = ~0;
    reset_bit(&(metadata->bit_map_inactive_cpus), 0);
    metadata->num_active_cpus = 1;
}

static uint8_t get_work(uint8_t *fn)
{
    uint8_t extra_work;

    if(queue_pop(&work_q, fn) == -1) {
	*fn = -1;
    }
    extra_work = queue_size(&work_q);
    return extra_work;
}

static void jump2fn(uint8_t id, uint8_t fn)
{
    //printf("In Scheduler\n");
    // TBD perhaps reqrite cr3
    pg_tbls[id].tbl[1].e[2] = metadata->func_info[fn].pt_addr | 0x07;
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
    jump_usermode(metadata->func_info[fn].entry_addr,
		  metadata->func_info[fn].stack_load_addr);    
}

void process_sched_dag(uint8_t fn)
{
    // dec all the out edge fns in_count for fn
    // and if any in_count == 0, push that to work_q
    uint8_t num_nodes, s, e, i;
    uint8_t out_fn;
    
    num_nodes = metadata->num_nodes;
    s = metadata->dag[num_nodes + fn + 1];
    e = metadata->dag[num_nodes + fn + 2];

    for(i = s; i < e; i++) {
	out_fn = metadata->dag[i];
	asm volatile("lock decb (%0)\n\t":: "r"(&(metadata->dag[out_fn + 1])));
	if(metadata->dag[out_fn + 1] == 0) {
	    printf("cpu %d: pushing %d\n", out_fn);
	    queue_push(&work_q, &out_fn);
	}
    }
}

void scheduler()
{
    uint8_t id, fn, extra_work;
    int cpus2wake, i;
    char *ins;
    id = get_id();

start:
    if(metadata->current[id] != -1) { // previous invoc execed a fn
	process_sched_dag(fn);
	metadata->current[id] = -1;
    }
    extra_work = get_work(&fn);
    printf("extra_work = %d, fn = %d\n", extra_work, fn);
    if(fn != -1) { // there is a work to do
	cpus2wake = (int)extra_work - (int)metadata->num_active_cpus;
	if(cpus2wake > 0) {
	    for(i = 0; i < cpus2wake; i++) {  // wake more cpus
		int64_t hlt_cpu_id;
		hlt_cpu_id = \
		    lowest_set_bit(metadata->bit_map_inactive_cpus);
		if(hlt_cpu_id != -1) {
		    // if prempted before hlt, give it a chance to
		    // hlt. Maybe can be remove TBD
		    vmmcall(KVM_HC_SCHED_YIELD, hlt_cpu_id);
		    vmmcall(KVM_HC_KICK_CPU, 0, hlt_cpu_id);
		} else {
		    break; // no more cpus to wake
		}
	    }
	}
	metadata->current[id] = fn;
	printf("cpu %d: executing %d\n", id, fn);
	jump2fn(id, fn);
	// perform the work
    }
    else { // no work to do, hlt yourself
	if((metadata->num_active_cpus == 1) &&
	   extra_work == 0) {
	    printf("All work is finished\n");
	}
	else { // you are the last guy, dont halt
	    asm volatile ("lock decq (%0)\n\t"
			  "lock bts %2, (%1)\n\t"
			  "hlt\n\t"
			  "lock btr %2, (%1)\n\t"
			  "lock incq (%0)\n\t":
			  : "r"(&(metadata->num_active_cpus)),
			    "r"( &(metadata->bit_map_inactive_cpus)), "r"(id));
	}
    }

    goto start;
}
