#include <stdint.h>
#include <stddef.h>
#include "printf.h"
#include "util.h"
#include "sched.h"
#include "stack.h"

#include "../../kvm-start/runtime_if.h" // TBD

#define WORK_Q_ARR_SZ (256)

#define MAX_TRY_COUNT 3
#define WAIT_FOR_WORK 128

extern char user_trampoline_sz[], user_trampoline[];
extern char boot_p4[], kern_end[];

struct t_pg_tbls *pg_tbls = (struct t_pg_tbls *)boot_p4;
struct t_metadata *metadata = (struct t_metadata *)0x0008;

extern void jump_usermode(void *rip, void *stack, void *arg);

static t_stack work_q;
static uint8_t work_q_arr[WORK_Q_ARR_SZ];
static mutex_t mutex_sched_hlt_path, mutex_process_dag;
static mutex_t mutex_active_cpu_chk;
static uint64_t dag_start_time[64] = {0};


// for the new scheduler
int64_t new_get_work();
static uint64_t bm_run_fn[MAX_FUNC/64] = {0}; // this is 4
static uint64_t num_free_vcpu = 0, num_active_vcpu = 0;
static uint64_t lock_to_wake_vcpu = 1;
static uint64_t runnable_tasks = 0;
static uint8_t vcpu_pool_sz;

void scheduler_init(uint8_t pool_sz)
{
    metadata->dag_time = 0;
    stack_init(&work_q, work_q_arr, ARR_SZ_1D(work_q_arr));
    metadata->bit_map_inactive_cpus = ~0;
    reset_bit(&(metadata->bit_map_inactive_cpus), 0);
    //printf("bm: %lX\n", metadata->bit_map_inactive_cpus);
    metadata->num_active_cpus = 1;
    num_active_vcpu = 1;
    num_free_vcpu = 1;
    vcpu_pool_sz = pool_sz;
    mutex_init(&mutex_sched_hlt_path);
    mutex_init(&mutex_process_dag);
    mutex_init(&mutex_active_cpu_chk);
    //new_sched();
}

void inc_active_cpu() {
    //printf("b:inc_active_cpu = %d\n", metadata->num_active_cpus);
    ATOMIC_INCQ(&(metadata->num_active_cpus));
    //ATOMIC_INCQ(&num_active_vcpu);
    //printf("a:inc_active_cpu = %d\n", metadata->num_active_cpus);
}
static uint8_t get_work(uint8_t *fn)
{
    uint8_t extra_work;

    if(stack_pop(&work_q, fn) == -1) {
	*fn = NULL_FUNC;
    }
    extra_work = stack_current_size(&work_q);
    return extra_work;
}

static void jump2fn(uint8_t id, uint8_t fn)
{
    char *ins;;
    //printf("In Scheduler\n");
    // TBD perhaps rewrite cr3
    ins = metadata->func_info[fn].entry_addr;
    //hexdump(metadata->func_info[fn].pt_addr, 32);
    //printf("%016lX %016lX\n", (pg_tbls[id].tbl[0].e[0]), metadata->func_info[fn].pt_addr);
    pg_tbls[id].tbl.e[0] = metadata->func_info[fn].pt_addr | 0x07;
    reload_cr3();

/*    
    printf("Jumping to address = %lX\n stack_addr = %lX	\
page_table = %lX\n",
	   metadata->func_info[fn].entry_addr,
	   metadata->func_info[fn].stack_load_addr,
	   pg_tbls[id].tbl[1].e[2]);
    

    ins = metadata->func_info[fn].entry_addr;
    hexdump(metadata->func_info[fn].pt_addr, 32);
    hexdump(ins, 32);    
*/  
    jump_usermode(metadata->func_info[fn].entry_addr,
		  metadata->func_info[fn].stack_load_addr,
		  (void *)(0x80000000));
}

void process_sched_dag(uint8_t id, uint8_t fn)
{
    // dec all the out edge fns in_count for fn
    // and if any in_count == 0, push that to work_q
    uint8_t num_nodes, s, e, i;
    uint8_t out_fn;
    //printf("cpu %d:processing dag\n", id);
    num_nodes = metadata->num_nodes;
    s = metadata->dag[num_nodes + fn] + 2 * num_nodes + 1;
    e = metadata->dag[num_nodes + fn + 1] + 2 * num_nodes + 1;
    //printf("num_nodes = %d, s = %d, e = %d, %d\n", num_nodes, s, e, metadata->dag[num_nodes + fn]);
    for(i = s; i < e; i++) {
	out_fn = metadata->dag[i];
	//printf("out_fn = %d\n", out_fn);
	mutex_lock_hlt(&mutex_process_dag);
	asm volatile("lock decb (%0)\n\t":: "r"(&(metadata->dag_in_count[out_fn])));
	if(metadata->dag_in_count[out_fn] == 0) {
	    //printf("cpu: pushing %d\n", out_fn);
	    stack_push(&work_q, &out_fn);
	}
	mutex_unlock_hlt(&mutex_process_dag);
    }
}

volatile void scheduler()
{
    uint8_t id, fn, extra_work;
    uint64_t id64, bm_inactive_cpus;
    int cpus2wake, i;
    volatile int wait_i;
    int try_work;
    id = get_id();
    id64 = (uint64_t)id;    
start:
    //printf("current[%d] = %d\n", id, metadata->current[id]);
    //printf("id: %d, tsc: %d\n", id, tsc() / 1000);
    if(metadata->current[id] != NULL_FUNC) { // previous invoc execed a fn
	//printf("id: %d, tsc: %d\n", id, tsc() / 1000);
	process_sched_dag(id, metadata->current[id]);
	metadata->current[id] = NULL_FUNC;
    }
    try_work = 0;
get_work_path:
    mutex_lock_hlt(&mutex_sched_hlt_path);
    extra_work = get_work(&fn);
    try_work++;
    if((fn == NULL_FUNC) && (try_work < MAX_TRY_COUNT)) {
	mutex_unlock_hlt(&mutex_sched_hlt_path);
	for(wait_i = 0; wait_i < WAIT_FOR_WORK; wait_i++);
	goto get_work_path;
    }
    //printf("cpu %d: extra_work = %d, fn = %d\n", id, extra_work, fn);
    if(fn != NULL_FUNC) { // there is a work to do
	//printf("active_cpus = %d\n", (int)metadata->num_active_cpus);
	asm volatile("mfence\n\t");
	cpus2wake = (int)extra_work - (int)metadata->num_active_cpus + 1;
	if(cpus2wake > 0) {
	    //printf("cpus2wake=%d\n", cpus2wake);
	    asm volatile("mfence\n\t");
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
		    //if(hlt_cpu_id > 1)
		    //printf("cpu%d:ew:%d,ac:%d,waking %d\n", id64, extra_work, metadata->num_active_cpus, hlt_cpu_id);
		    vmmcall(KVM_HC_KICK_CPU, 0, hlt_cpu_id);
		} else
		    break; // no more cpus to wake
	    }
	}
	mutex_unlock_hlt(&mutex_sched_hlt_path);
	
	metadata->current[id] = fn;
	//printf("cpu %d: executing %d\n", id, fn);
	jump2fn(id, fn);
	// perform the work
    }
    else { // no work to do, hlt yourself	
	mutex_unlock_hlt(&mutex_sched_hlt_path);
	mutex_lock_hlt(&mutex_active_cpu_chk);
	asm volatile("mfence\n\t");
	if(metadata->num_active_cpus == 1) {
	    // you are the last guy, dont halt
	    mutex_unlock_hlt(&mutex_active_cpu_chk);
	    //printf("cpu %d:All work is finished\n", id);
	    if(dag_start_time[id] != 0)
		metadata->dag_time += tsc() - dag_start_time[id];
	    outw(PORT_MSG, MSG_WAITING_FOR_WORK);
	    dag_start_time[id] = tsc();
	    // copy in count for the dag, it will be modified
	    memcpy(metadata->dag_in_count, metadata->dag, sizeof(metadata->dag_in_count));
	    // executed by single guy
	    //printf("cpu %d: pushing\n", id);
	    stack_push(&work_q, &(metadata->start_func));
	}
	else {
	    ATOMIC_DECQ(&(metadata->num_active_cpus));
	    mutex_unlock_hlt(&mutex_active_cpu_chk);
	    //printf("cpu %d: halting,extra_work=%d,active_cpu=%d,bm=%lX\n", id64, extra_work,metadata->num_active_cpus,metadata->bit_map_inactive_cpus);
	    asm volatile ("lock bts %2, (%1)\n\t"
			  "hlt\n\t"
			  "marker:\n\t"
			  "lock btr %2, (%1)\n\t"
			  "lock incq (%0)\n\t":
			  : "r"(&(metadata->num_active_cpus)),
			    "r"( &(metadata->bit_map_inactive_cpus)), "r"(id64));
	    //printf("\n");
	    //printf("woke: active_cpu = %d\n", metadata->num_active_cpus);
	}
    }

    goto start;
}

static void new_jump2fn(uint8_t id, uint8_t fn)
{
    char *ins;
    metadata->current[id] = fn;
    //printf("In Scheduler\n");
    // TBD perhaps rewrite cr3
    ins = metadata->func_info[fn].entry_addr;
    //hexdump(metadata->func_info[fn].pt_addr, 32);
    //printf("%016lX %016lX\n", (pg_tbls[id].tbl[0].e[0]), metadata->func_info[fn].pt_addr);
    pg_tbls[id].tbl.e[0] = metadata->func_info[fn].pt_addr | 0x07;
    reload_cr3();

/*    
    printf("Jumping to address = %lX\n stack_addr = %lX	\
page_table = %lX\n",
	   metadata->func_info[fn].entry_addr,
	   metadata->func_info[fn].stack_load_addr,
	   pg_tbls[id].tbl[1].e[2]);
    

    ins = metadata->func_info[fn].entry_addr;
    hexdump(metadata->func_info[fn].pt_addr, 32);
    hexdump(ins, 32);    
*/  
    jump_usermode(metadata->func_info[fn].entry_addr,
		  metadata->func_info[fn].stack_load_addr,
		  (void *)(0x80000000));
}

inline void add_runnable_fn(uint8_t fn) {
    asm volatile("xorq %%rax, %%rax         \n\t"
		 "movb  %1, %%al            \n\t"
		 "shrb  $6, %%al            \n\t"
		 "leaq  (%0,%%rax,8), %%rbx \n\t"
		 "movb  %1, %%al            \n\t"
		 "andb  $0x3F, %%al          \n\t"
		 "lock incq %2              \n\t"
		 "lock bts  %%rax, (%%rbx)  \n\t"
		 "nz%=:                     \n\t"
		 :
		 : "r"(bm_run_fn),
		   "r"(fn),
		   "m"(runnable_tasks)
		 : "rax", "rbx", "memory");
    //printf("runnable: %d,%016lX\n", fn, bm_run_fn[0]);
}
void new_process_sched_dag(uint8_t id, uint8_t fn)
{
    // dec all the out edge fns in_count for fn
    // and if any in_count == 0, push that to work_q
    uint8_t num_nodes, s, e, i;
    uint8_t out_fn;
    //printf("cpu %d:processing dag\n", id);
    num_nodes = metadata->num_nodes;
    s = metadata->dag[num_nodes + fn] + 2 * num_nodes + 1;
    e = metadata->dag[num_nodes + fn + 1] + 2 * num_nodes + 1;
    //printf("num_nodes = %d, s = %d, e = %d, %d\n", num_nodes, s, e, metadata->dag[num_nodes + fn]);
    for(i = s; i < e; i++) {
	out_fn = metadata->dag[i];
	asm goto volatile("lock decb (%0)            \n\t"
			  "jnz   %l[skip]                \n\t"
			  :
			  : "r"(&(metadata->dag_in_count[out_fn]))
			  : "memory"
			  : skip);
	add_runnable_fn(out_fn);
    skip:
	continue;
    }
}

int64_t new_get_work()
{
    int64_t rax;
    uint64_t chk_idx = 0;
    // loop is unrilled 4 time shere
    //printf("%016lX\n", bm_run_fn[0]);
    /*
    asm volatile("bsf (%1), %0\n\t"
		 : "=a"(rax)
		 : "r"(bm_run_fn));
    */
    rax= 0;
    //printf("rax = %d\n", rax);
    asm volatile("F0l:     bsfq (%2), %0  \n\t"
		 "         jz    F1        \n\t"
		 "    lock btrq  %0, (%2)  \n\t"
		 "         jc    RP0       \n\t"
		 "         jmp   F0l       \n\t"
		 "F1:      addq  $8, %2    \n\t"
		 "F1l:     bsfq  (%2), %0  \n\t"
		 "         jz    F2        \n\t"
		 "    lock btrq  %0, (%2)  \n\t"
		 "         jc    RP1       \n\t"
		 "         jmp   F1l       \n\t"
		 "F2:      addq  $8,  %2   \n\t"
		 "F2l:     bsfq  (%2), %0  \n\t"
		 "         jz    F3        \n\t"
		 "    lock btrq  %0, (%2)  \n\t"
		 "         jc    RP2       \n\t"
		 "         jmp   F2l       \n\t"
		 "F3:      addq  $8,  %2   \n\t"
		 "F3l:     bsfq  (%2), %0  \n\t"
		 "         jz    fp        \n\t"
		 "    lock btrq  %0, (%2)  \n\t"
		 "         jc    RP3       \n\t"
		 "         jmp   F3l       \n\t"
		 "RP0:     movq  $0, %1    \n\t"
		 "         jmp   sp        \n\t"
		 "RP1:     movq  $1, %1    \n\t"
		 "         jmp   sp        \n\t"
		 "RP2:     movq  $2, %1    \n\t"
		 "         jmp   sp        \n\t"
		 "RP3:     movq  $3, %1    \n\t"
		 "         jmp   sp        \n\t"
		 "fp:      movq $(-1), %0\n\t"
		 "sp:      \n\t"
		 : "+a"(rax), "=r"(chk_idx)
		 : "r"(bm_run_fn)
		 : "memory");

    //printf("rax = %d,%d\n", rax,chk_idx);
    if(rax != -1) {
	rax = (64*chk_idx + rax);
    }
    return rax;
    
}
void new_sched()
{
    uint8_t id;
    int64_t fn, cpuid, cpus2wake;
    uint64_t id64;
    
    id = get_id();
    id64 = (uint64_t)id;

    fn = metadata->current[id];
    //printf("%d: comp %d\n", id, fn);
    if(fn != NULL_FUNC) { // previous invoc execed a fn
	ATOMIC_INCQ(&num_free_vcpu);
	//printf("id: %d, tsc: %d\n", id, tsc() / 1000);
	new_process_sched_dag(id, fn);
	metadata->current[id] = NULL_FUNC;
    }
	
start:
    if(num_active_vcpu >= vcpu_pool_sz)
	goto new_work;
    asm goto volatile("lock btrq $0, (%0)\n\t"
		      "jnc %l[new_work]"
		      :
		      : "r"(&lock_to_wake_vcpu)
		      :
		      : new_work);

// wake up the vcpus, you have the lock
    cpus2wake = runnable_tasks - num_free_vcpu;
    //printf("cpus2wake = %d\n", cpus2wake);
    if(cpus2wake <= 0)
	goto release_lock;
    //printf("W\n");
    while(cpus2wake--) {
	asm volatile("bsf (%3), %0            \n\t"
		     "jz zero%=               \n\t"
		     "lock btrq %0, (%3)      \n\t"
		     "lock incq %1            \n\t"
		     "lock incq %2            \n\t"
		     "jmp nz%=                \n\t"
		     "zero%=: movq $(-1), %0  \n\t"
		     "nz%=:                   \n\t"
		     : "+r"(cpuid), "+m"(num_free_vcpu),
		       "+m"(num_active_vcpu)
		     : "r"(&(metadata->bit_map_inactive_cpus))
		     : "memory");
	if(cpuid != -1) {
	    //printf("w%d\n",cpuid);
	    vmmcall(KVM_HC_SCHED_YIELD, (uint64_t)cpuid);
	    vmmcall(KVM_HC_KICK_CPU, 0, (uint64_t)cpuid);
	}
	else
	    break;
    }
release_lock:
    // release the lock
    asm volatile("lock btsq $0, (%0)\n\t"
		 :
		 : "r"(&lock_to_wake_vcpu));
    
new_work:
    if((fn = new_get_work()) == -1) {
	asm goto volatile("lock decq %0\n\t"
			  "lock decq %1\n\t"
			  "jz %l[if_num_active_vcpu_is_0]\n\t"
			  "jnz %l[else_num_active_vcpu_is_not_0]\n\t"
			  : 
			  : "m"(num_free_vcpu), "m"(num_active_vcpu)
			  :
			  :if_num_active_vcpu_is_0,
			   else_num_active_vcpu_is_not_0);
    if_num_active_vcpu_is_0:
	//printf("P%d,%d\n",id,num_active_vcpu);
	outw(PORT_MSG, MSG_WAITING_FOR_WORK);
	ATOMIC_INCQ(&num_active_vcpu);
	ATOMIC_INCQ(&num_free_vcpu);
	add_runnable_fn(0);
	// optimize this TBD
	memcpy(metadata->dag_in_count, metadata->dag, sizeof(metadata->dag_in_count));
	goto new_work;
    else_num_active_vcpu_is_not_0:
	//printf("h%d,%d,%d\n",id, num_active_vcpu,num_free_vcpu);
	asm volatile("lock bts %0, (%1)\n\t"
		     "hlt"
		     :
		     :"r"(id64),
		      "r"(&(metadata->bit_map_inactive_cpus)));
	goto new_work;
    }
    else {
	ATOMIC_DECQ(&num_free_vcpu);
	ATOMIC_DECQ(&runnable_tasks);
	// do work
	//printf("%d:f%d,%d,%d\n",id,fn, num_active_vcpu,num_free_vcpu);
	new_jump2fn(id, (uint8_t)fn);
    }
    return;
}
