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
mutex_t mutex_sched_hlt_path;
void scheduler_init(uint8_t pool_sz)
{
    queue_init(&work_q, work_q_arr, ARR_SZ_1D(work_q_arr));
    metadata->bit_map_inactive_cpus = ~0;
    reset_bit(&(metadata->bit_map_inactive_cpus), 0);
    metadata->num_active_cpus = 1;
    mutex_init(&mutex_sched_hlt_path);
}

void inc_active_cpu() {
    ATOMIC_INCQ(&metadata->num_active_cpus);
}
static uint8_t get_work(uint8_t *fn)
{
    uint8_t extra_work;

    if(queue_pop(&work_q, fn) == -1) {
	*fn = NULL_FUNC;
    }
    extra_work = queue_size(&work_q);
    return extra_work;
}

static void jump2fn(uint8_t id, uint8_t fn)
{
    char *ins;;
    //printf("In Scheduler\n");
    // TBD perhaps rewrite cr3
    ins = metadata->func_info[fn].entry_addr;
    //hexdump(metadata->func_info[fn].pt_addr, 32);
    pg_tbls[id].tbl[1].e[2] = metadata->func_info[fn].pt_addr | 0x07;
    reload_cr3();
    /*
    printf("Jumping to address = %lX\n stack_addr = %lX	\
page_table = %lX\n",
	   metadata->func_info[fn].entry_addr,
	   metadata->func_info[fn].stack_load_addr,
	   pg_tbls[id].tbl[1].e[2]);
    */

    //ins = metadata->func_info[fn].entry_addr;
    //hexdump(metadata->func_info[fn].pt_addr, 32);
    //hexdump(ins, 32);
    jump_usermode(metadata->func_info[fn].entry_addr,
		  metadata->func_info[fn].stack_load_addr);    
}

void process_sched_dag(uint8_t id, uint8_t fn)
{
    // dec all the out edge fns in_count for fn
    // and if any in_count == 0, push that to work_q
    uint8_t num_nodes, s, e, i;
    uint8_t out_fn;

    printf("cpu %d:processing dag\n", id);
    num_nodes = metadata->num_nodes;
    s = metadata->dag[num_nodes + fn] + 2 * num_nodes + 1;
    e = metadata->dag[num_nodes + fn + 1] + 2 * num_nodes + 1;
    //printf("num_nodes = %d, s = %d, e = %d, %d\n", num_nodes, s, e, metadata->dag[num_nodes + fn]);
    for(i = s; i < e; i++) {
	out_fn = metadata->dag[i];
	printf("out_fn = %d\n", out_fn);
	asm volatile("lock decb (%0)\n\t":: "r"(&(metadata->dag[out_fn])));
	if(metadata->dag[out_fn] == 0) {
	    printf("cpu: pushing %d\n", out_fn);
	    queue_push(&work_q, &out_fn);
	}
    }
}

void scheduler()
{
    uint8_t id, fn, extra_work;
    uint64_t id64, bm_inactive_cpus;
    int cpus2wake, i;
    id = get_id();
    id64 = (uint64_t)id;

start:
    printf("current[%d] = %d\n", id, metadata->current[id]);
    if(metadata->current[id] != NULL_FUNC) { // previous invoc execed a fn
	process_sched_dag(id, metadata->current[id]);
	metadata->current[id] = NULL_FUNC;
    }
    extra_work = get_work(&fn);
    //printf("cpu %d: extra_work = %d, fn = %d\n", id, extra_work, fn);
    if(fn != NULL_FUNC) { // there is a work to do
	//printf("active_cpus = %d\n", (int)metadata->num_active_cpus);
	cpus2wake = (int)extra_work - (int)metadata->num_active_cpus + 1;
	if(cpus2wake > 0) {
	    bm_inactive_cpus = metadata->bit_map_inactive_cpus;
	    for(i = 0; i < cpus2wake; i++) {  // wake more cpus
		int64_t hlt_cpu_id;
		hlt_cpu_id = \
		    lowest_set_bit(&bm_inactive_cpus);
		if(hlt_cpu_id != -1) {
		    reset_bit(&bm_inactive_cpus, hlt_cpu_id);
		    // if prempted before hlt, give it a chance to
		    // hlt. Maybe can be remove TBD
		    //vmmcall(KVM_HC_SCHED_YIELD, hlt_cpu_id);
		    printf("waking %d\n", hlt_cpu_id);
		    vmmcall(KVM_HC_KICK_CPU, 0, hlt_cpu_id);
		} else
		    break; // no more cpus to wake
	    }
	}
	metadata->current[id] = fn;
	printf("cpu %d: executing %d\n", id, fn);
	jump2fn(id, fn);
	// perform the work
    }
    else { // no work to do, hlt yourself
	mutex_lock_hlt(&mutex_sched_hlt_path);
	if((extra_work == 0) && (metadata->num_active_cpus == 1)) {
	    // you are the last guy, dont halt
	    mutex_unlock_hlt(&mutex_sched_hlt_path);
	    printf("cpu %d:All work is finished\n", id);
	    outw(PORT_MSG, MSG_WAITING_FOR_WORK);
	    // executed by single guy
	    printf("cpu %d: pushing\n", id);
	    queue_push(&work_q, &(metadata->start_func));
	}
	else {
	    mutex_unlock_hlt(&mutex_sched_hlt_path);
	    printf("cpu %d: halting\n", id);
	    asm volatile ("lock decq (%0)\n\t"
			  "lock bts %2, (%1)\n\t"
			  "hlt\n\t"
			  "lock btr %2, (%1)\n\t"
			  "lock incq (%0)\n\t":
			  : "r"(&(metadata->num_active_cpus)),
			    "r"( &(metadata->bit_map_inactive_cpus)), "r"(id64));
	}
    }

    goto start;
}
