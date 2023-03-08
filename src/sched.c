#include <stdint.h>
#include <stddef.h>
#include "printf.h"
#include "util.h"
#include "sched.h"
#include "stack.h"

#include "../../kvm-start/runtime_if.h" // TBD

extern char user_trampoline_sz[], user_trampoline[];
extern char boot_p4[], kern_end[];

struct t_pg_tbls *pg_tbls = (struct t_pg_tbls *)boot_p4;
struct t_metadata *metadata = (struct t_metadata *)0x0008;

extern void jump_usermode(void *rip, void *stack, void *arg, uint64_t id);

// for the new scheduler
int64_t get_work();
static uint64_t bm_run_fn[MAX_FUNC/64] = {0}; // this is 4
static uint64_t num_free_vcpu = 0;
static uint64_t runnable_tasks = 0;
static uint8_t vcpu_pool_sz;
static uint64_t num_fin_fn = 0;
static uint64_t hlt_ts[MAX_CPUS] = {0};
static uint64_t kick_ts[MAX_CPUS] = {0};
static int64_t wake_num_vcpu = 0, sig_wake_num_vcpu_is_0 = 1;
static uint64_t bm_kicked_vcpus, bm_inactive_vcpus;
static const uint64_t __attribute__((aligned(64))) zero[2] = {0};
static const int64_t rekick_thd = 1000000; // 344 us at 2.9ghz
static uint64_t __attribute__((aligned(64))) ss_reflective_bm[2] = {0};
static uint64_t __attribute__((aligned(64))) ss_reflective_bm1[2] = {0};

void scheduler_init_pre(uint8_t pool_sz)
{   
    bm_kicked_vcpus = 0;
    bm_inactive_vcpus = (((uint64_t)1 << pool_sz) - 1);
    asm volatile("lock btrq $0, %1 \n\t"
		 :"+m"(bm_inactive_vcpus)
		 ::"memory");
    //printf("bm: %lX\n", metadata->bit_map_inactive_cpus);
    sig_wake_num_vcpu_is_0 = 1;
    num_free_vcpu = 1;
    vcpu_pool_sz = pool_sz;
    metadata->dag_ts = 0;
    metadata->dag_n = 0;
    //sched();
}

void scheduler_init_post(uint8_t pool_sz)
{
    int i;
    num_fin_fn = (typeof(num_fin_fn))(metadata->num_nodes);
    for(i = 1; i < pool_sz; i++) {
	hlt_ts[i] = tsc();
    }
}

static void jump2fn(uint8_t id, uint8_t fn)
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
		  (void *)(0x80000000), fn);
}

static inline void add_runnable_fn(uint8_t fn) {
    asm volatile("xorq %%rax, %%rax         \n\t"
		 "movb  %1, %%al            \n\t"
		 "shrb  $6, %%al            \n\t"
		 "leaq  (%0,%%rax,8), %%rbx \n\t"
		 "movb  %1, %%al            \n\t"
		 "andb  $0x3F, %%al          \n\t"
		 "lock incq %2              \n\t"
		 "lock btsq  %%rax, (%%rbx)  \n\t"
		 "nz%=:                     \n\t"
		 :
		 : "r"(bm_run_fn),
		   "r"(fn),
		   "m"(runnable_tasks)
		 : "rax", "rbx", "memory");
    //printf("runnable: %d,%016lX\n", fn, bm_run_fn[0]);
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

int64_t get_work()
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

void wake_up_task()
{
    uint64_t __attribute__((aligned(64))) reflective_bm[2];
    uint64_t __attribute__((aligned(64))) reflective_bm1[2];
    int64_t cpuid, tmp, dt;
    typeof(wake_num_vcpu) old_wake_num_vcpu;

    reflective_bm1[0] = bm_kicked_vcpus;
    reflective_bm1[1] = reflective_bm1[0];
    DCAS(&ss_reflective_bm1, zero, reflective_bm1);

    while((cpuid = e_bsf(&ss_reflective_bm1[0])) >= 0) {
	dt = tsc() - kick_ts[cpuid];
	if(dt > rekick_thd) {
	    printf("Rekicked %d\n", cpuid);
	    kick_ts[cpuid] = tsc();
	    vmmcall(KVM_HC_KICK_CPU, 0, (uint64_t)cpuid);
	}
	asm volatile("lock btrq %1, %0      \n\t"
		     : "+m"(ss_reflective_bm1[1])
		     : "rm"(cpuid)
		     : "memory");
    }

    tmp = runnable_tasks - num_free_vcpu;
    if(tmp > 0) {
	if(CAS(&sig_wake_num_vcpu_is_0, 1, 0) == 1) {
	    wake_num_vcpu = tmp;
	}
    }
    reflective_bm[0] = bm_inactive_vcpus;
    reflective_bm[1] = reflective_bm[0];
    DCAS(&ss_reflective_bm, zero, reflective_bm);
    while((old_wake_num_vcpu = xadd(&wake_num_vcpu, -1)) > 0) {
	if(old_wake_num_vcpu == 1) {
	    sig_wake_num_vcpu_is_0 = 1;
	}
	try_kick_another_vcpu:
	if((cpuid = e_bsf(&ss_reflective_bm[0])) >= 0) {
	    //printf("tw:%d\n", cpuid);
	    // get_hlt_id non blocking
	    if(hlt_ts[cpuid] > kick_ts[cpuid]) { // first kick
		//printf("nk:%d\n", cpuid);
		ATOMIC_INCQ_M(num_free_vcpu);
		kick_ts[cpuid] = tsc();
		//printf("kicking %d\n", cpuid);
		asm volatile("lock btsq %1, %0      \n\t"
			     : "+m"(bm_kicked_vcpus)
			     : "rm"(cpuid)
			     : "memory");		
	        vmmcall(KVM_HC_KICK_CPU, 0, (uint64_t)cpuid);
		asm volatile("lock btrq %1, %0      \n\t"
			     : "+m"(ss_reflective_bm[1])
			     : "rm"(cpuid)
			     : "memory");
	    } else { // already kicked
		asm volatile("lock btrq %1, %0      \n\t"
		     : "+m"(ss_reflective_bm[1])
		     : "rm"(cpuid)
		     : "memory");
		goto try_kick_another_vcpu;
	    }
	}
	else
	    break; // no other vcpu I can wake
    }
}
void sched()
{
    uint8_t id;
    int64_t fn;
    uint64_t id64, tot_fn, popcnt;
    int64_t spurious_wake_up, spurious_wake_up_ident;
    uint64_t t1, t2, dt;

    t1 = tsc();
    id = get_id();
    id64 = (uint64_t)id;
    fn = metadata->current[id];
    //printf("%d: comp %d\n", id, fn);
    if(fn != NULL_FUNC) { // previous invoc execed a fn
	ATOMIC_INCQ_M(num_free_vcpu);
	//printf("id: %d, tsc: %d\n", id, tsc() / 1000);
	process_sched_dag(id, fn);
	metadata->current[id] = NULL_FUNC;
    }
    wake_up_task();
    if(fn != NULL_FUNC)
	ATOMIC_INCQ_M(num_fin_fn);

new_work:
    if((fn = get_work()) == -1) {
	ATOMIC_DECQ_M(num_free_vcpu);
	tot_fn = metadata->num_nodes;
	//printf("%d,%d\n", num_fin_fn, tot_fn);
	if(CAS(&num_fin_fn, tot_fn, 0) == tot_fn) {
	    //printf("CAS:%d",num_fin_fn);
 	    outw(PORT_MSG, MSG_WAITING_FOR_WORK);
	    ATOMIC_INCQ_M(num_free_vcpu);
	    // recovery code. To reset num_free_vcpu to 1
	    // this will definitely trigger if there is a long
	    // break. Else may not trigger, sometimes
	    if (bm_kicked_vcpus == 0) {
		asm volatile("popcntq %1, %0"
			     :"=rm"(popcnt)
			     : "m"(bm_inactive_vcpus));
		if((vcpu_pool_sz - popcnt) == 1) {
		    num_free_vcpu = 1;
		    runnable_tasks = 0;
		}
	    }
	    memcpy(metadata->dag_in_count, metadata->dag,
		   metadata->num_nodes * sizeof(metadata->dag_in_count[0]));
	    add_runnable_fn(metadata->start_func);
	}
	else {
	    spurious_wake_up = 1;
	    hlt_ts[id] = tsc();
	    t2 = hlt_ts[id];
	    dt = t2 - t1;
	    asm volatile("lock addq %1, %0 \n\t"
			 :"+m"(metadata->dag_ts)
			 : "rm"(dt)
			 : "memory");
	    asm volatile("lock incq %0     \n\t"
			 :"+m"(metadata->dag_n)
			 :: "memory");
	    asm volatile("         lock btsq  %4, %0   \n\t"
			 "              hlt            \n\t"
			 "         lock btrq  %4, %0   \n\t"
			 "         lock btrq  %4, %1   \n\t"
			 "              jc    ret%=    \n\t"
			 "         lock incq  %3       \n\t"
			 "              xorq  $1, %2   \n\t"
			 "ret%=:        xorq  $1, %2   \n\t"  
			 : "+m"(bm_inactive_vcpus),
			   "+m"(bm_kicked_vcpus),
			   "+m"(spurious_wake_up),
			   "+m"(num_free_vcpu)
			 : "rm"(id64)
			 : "memory" );
	    t1 = tsc();
	    if(spurious_wake_up == 1) {
		printf("spurious wake up %d,%d\n", id,num_free_vcpu);
	    }
	}
	goto new_work;
    }
    else {
	ATOMIC_DECQ_M(num_free_vcpu);
	ATOMIC_DECQ_M(runnable_tasks);
	// do work
	//printf("%d:f%d,%d,%d\n",id,fn, num_active_vcpu,num_free_vcpu);
	t2 = tsc();
	dt = t2 - t1;
	asm volatile("lock addq %1, %0 \n\t"
		     :"+m"(metadata->dag_ts)
		     : "rm"(dt)
		     : "memory");
	asm volatile("lock incq %0     \n\t"
		     :"+m"(metadata->dag_n)
		     :: "memory");	
	jump2fn(id, (uint8_t)fn);
    }

    printf("No return here\n");
}

// spuriously waking up cpus, maintaining correct
// num_active_vcpu count
void wake_up_event(uint64_t id)
{
    asm volatile("           lock btrq %3, %1    \n\t"
		 "           lock btrq %3, %2    \n\t"
		 "                jc no_inc%=    \n\t"
		 "           lock incq %0        \n\t"
		 "no_inc%=:                      \n\t"
		 : "+m"(num_free_vcpu), "+m"(bm_inactive_vcpus),
		   "+m"(bm_kicked_vcpus)
		 : "r"(id)
		 : "memory");

}
