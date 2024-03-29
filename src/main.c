#include <stdint.h>
#include <stddef.h>
#include "printf.h"
#include "util.h"
#include "hw_types.h"
#include "globvar.h"
#include "mm_types.h"
#include "mm.h"
#include "pic.h"
#include "msrs.h"
#include "sched.h"
#include "apic.h"
#include "kvm_para.h"

int user_test_function();
void init_boot(uint8_t my_id, uint8_t pool_sz);
extern void flush_tss(uint8_t id);

extern char __syscall_entry[];
extern uint64_t gdt64_start_c[];
extern uint64_t boot_p4[];
extern uint64_t kern_stack[];
struct tss_entry_t __attribute__((aligned(64))) tss_seg[MAX_CPUS];
uint8_t __attribute__((aligned(16))) user_stack[16*32];

static uint64_t barrier_bm = ~0;

void user_test_func() {
    volatile uint64_t t1, t2, d, i, n, a, b;
    n = 100;
    d = 0;
    b =0;
#define utsc(x) asm("rdtscp\n\t" "mov %%rax, %0" : "=r"(x));
    utsc(t1);    
    for(i = 0; i < 10; i++) {
	//asm("int $0x80");
	//asm("int $0x80");
	//asm("int $0x80");
	//asm("int $0x80");
	//asm("int $0x80");
	//asm("int $0x80");
	//asm("int $0x80");
	//asm("int $0x80");
	//asm("int $0x80");
	//asm("int $0x80");	
    }    
    utsc(t2);
    d = t2 - t1;
    /*
    asm("mov %0, %%r15"::"r"(d));
    asm("movl $0xa, %eax\n"
	"syscall\n");
    while(1);
    */
    //asm("int $0x81");
    
    return;
}

/*
void send_ipi()
{
    int i;
    struct apic_t *apic = 0xFEE00000;

    //printf("id(%d) = apicid(%d)\n", (int)get_id(), apic->id >> 24);
    return;
    if(get_id() == 0) {
	apic->esr = 0;
	//apic->icrh = (0x01) << 24;
	// 0xC -> all exclusing self
	// 0x40 is the ipi
	apic->icrl = ICRL(0x40) | (0xC << 16);
	//while( (1 < 12) & apic->icrl == 0);
	//printf("ipi sent\n");
	//while(1)
	//    printf("esr: %08X\n", apic->esr);;
    }
    else {
	return;
	while(1) {
	    //printf("bit: %d\n", read_apic_bit_pack(apic->irr, 0x40));
	}
    }
}
*/


static volatile uint64_t new_mutex = 0, n_barrier = 0, s_barrier = 0, test_var = 0;
void test_new_mutex()
{
    volatile uint64_t i, j = 0;
    while(1) {
	mutex_lock(&new_mutex);
	for(i = 0; i < 1000; i++);
	mutex_unlock(&new_mutex);
	for(i = 0; i < 100000; i++);
	/*
	asm volatile("lock incq %0 \n\t": "+m"(n_barrier)::"memory");
	if(n_barrier == 4) {
	    s_barrier = 1;
	}
	else {
	    while(s_barrier != 1);
	}
	asm volatile("lock decq %0 \n\t": "+m"(n_barrier)::"memory");
	if(n_barrier == 0) {
	    s_barrier = 0;
	}
	else
	    while(s_barrier != 0);
        j++;
	if( j%100 == 0)
	    printf("j = %ld\n", j);
	*/
    }
}


int main()
{
    uint8_t my_id, pool_sz;
    uint16_t my_id_pool_sz;
    int i;
    uint64_t *addr = (uint64_t *)0x0008;
    uint64_t t2, t1, id64, preempted_cpu, tbm;
    volatile int wait_i;
    struct apic_t *apic = 0xFEE00000;
    
    my_id_pool_sz = get_pool_and_id();
    my_id = my_id_pool_sz & 0xFF;
    id64 = my_id;
    pool_sz = (uint8_t)((my_id_pool_sz & 0xFF00) >> 8);
    //printf("Calling init boot\n");
    //t1 = tsc();
    init_boot(my_id, pool_sz);
    //printf("id(%d) = apicid(%d)\n", (int)get_id(), apic->id >> 24);
    //test_new_mutex();
    //t2 = tsc();
    //printf("dt = %d\n", (t2 - t1) / TSC_TO_US_DIV);
    //asm("sti");
    //printf("Barrier waiting %d\n", get_id());
    if(id64 == 0) {
	tbm = (((uint64_t)1 << pool_sz) - 1);
	asm volatile("movq %0, %%rax    \n\t"
		     "lock andq %%rax, %1  \n\t"
		     :: "m"(tbm),
		      "m"(barrier_bm)
		     : "rax", "memory");
	asm volatile("lock btr %0, %1  \n\t"
		     :: "r"(id64), "m"(barrier_bm));	
	while((barrier_bm) != 0) {
	    asm volatile("bsf %1, %%rax     \n\t"
			 "movq %%rax, %0    \n\t"
			 : "=m" (preempted_cpu)
			 : "m"(barrier_bm)
			 : "rax", "memory");
	    //printf("prem %016lX,%016lX\n", preempted_cpu, barrier_bm);
	    vmmcall(KVM_HC_SCHED_YIELD, preempted_cpu);
	}
	//printf("barbm %016lX\n", barrier_bm);
	scheduler_init_pre(pool_sz);
	outw(PORT_MSG, MSG_BOOTED);
	scheduler_init_post(pool_sz);
    }
    else {
	/*
	asm volatile("lock btrq %0, %1  \n\t"
		     :: "r"(id64), "m"(barrier_bm));
	*/
	//while( read_apic_bit_pack(apic->irr, 0x40) == 0);
	//printf("%d:bit: %d\n", my_id,read_apic_bit_pack(apic->irr, 0x40) == 0);
	//while(1);
	asm volatile("lock btrq %0, %1  \n\t"
		     "hlt               \n\t"
		     :: "r"(id64), "m"(barrier_bm));
	wake_up_event(id64);
    }

    //barrier();
    //printf("Barier in %d\n", my_id);
    //outb(PORT_HLT, 0);
    /*
    asm("	rdtscp\n"
	"xchgq %rax, %r15\n"
	"subq %rax, %r15\n"
	"movw    $0x3fa, %dx\n"
	"movb    $0, %al\n"
	"out     %al, (%dx)\n");
    */

    //scheduler_init(pool_sz);
    //outw(PORT_MSG, MSG_BOOTED);

    //printf("swapgs sp = %016X\n", get_kern_stack());
#if 0
    if(my_id != 0) {
	//printf("halting %d\n", my_id);
	asm volatile("hlt\n\t");
	inc_active_cpu();
	printf("1-Woke %d\n", my_id);
	//for(wait_i = 0; wait_i < 1000000; wait_i++);
	//printf("H\n");
	//asm volatile("hlt");
	//printf("2-Woke %d\n", my_id);
    }
    else {
	scheduler_init(pool_sz);
	outw(PORT_MSG, MSG_BOOTED);
    }
#endif    
    //for(wait_i = 0; wait_i < 1000000; wait_i++);
    //vmmcall(KVM_HC_KICK_CPU, 0, 1);
    //vmmcall(KVM_HC_KICK_CPU, 0, 1);
    //while(1);
    sched();
    
    printf("cpu %d: No return here\n", my_id);
    while(1);
#if 0    
    //pde = addr_to_pde((void *)boot_p4, 0x200000, (uint64_t **)pe);
    //p1->pde |= U_BIT;
    //p2->pde |= U_BIT;
    //p3->pde |= U_BIT;
    //memcpy(0x200100, &user_test_func, 0x1000);
//    PIC_remap(PIC_REMAP, PIC_REMAP + 0x08);
//    for(i = 0; i <= 0xf; i++)
//	IRQ_clear_mask(i);
#endif
    //outb(PORT_WAIT_USER_CODE_MAPPING, 1);
    //outb(PORT_HLT, 0);
    return 0;
}

void fill_tss(struct tss_entry_t *tss, struct sys_desc_t *tss_desc,
	      uint64_t stack_addr)
{
    int sz;
    // fill tss table
    memset(tss, 0, sizeof(*tss));
    // need to see its correctness
    
    tss->rsp0 = stack_addr;
    
    tss->iomap_base = sizeof(*tss); // no iomap

    // fill tss desc
    memset(tss_desc, 0, sizeof(*tss_desc));
    sz = sizeof(*tss) - 1;
    tss_desc->limit_15_0 = sz & 0xFFFF;
    tss_desc->limit_19_16 = (sz & 0xF0000) >> 16;
    tss_desc->base_23_0 = (uint64_t)tss & 0xFFFFFF;
    tss_desc->base_31_24 = ((uint64_t)tss & 0xFF000000) >> 24;
    tss_desc->base_64_32 = ((uint64_t)tss & 0xFFFFFFFF00000000) >> 32;
    tss_desc->dpl = 0;
    tss_desc->present = 1;
    tss_desc->type = 0x9; // available tss
    tss_desc->granularity = 0; // in bytes
}

void init_CS(gdt_entry_t *s)
{
    memset(s, 0, sizeof(*s));
    //s->CS.seg_limit_15_0 = 0xFFFF;
    s->CS.hardcode0_1	= 1;
    s->CS.hardcode1_1	= 1;
    s->CS.hardcode2_0	= 0;
    s->CS.long_mode	= 1;
    s->CS.present	= 1;
}
void init_DS(gdt_entry_t *s)
{
    memset(s, 0, sizeof(*s));
    //s->DS.seg_limit_15_0 = 0xFFFF;
    s->DS.hardcode0_0	= 0;
    s->DS.hardcode1_1	= 1;
    s->DS.present	= 1;
    s->DS.write         = 1;
}
void fill_user_mode_gdt()
{
    gdt_entry_t *gdt_start = (gdt_entry_t *)gdt64_start_c;
    gdt_entry_t *ucs, *uds;

    ucs = &gdt_start[4];
    uds = &gdt_start[3];

    init_CS(ucs);
    ucs->CS.DPL = 3;
    //hexdump(ucs, 8);
    init_DS(uds);
    uds->DS.DPL = 3;
    //hexdump(uds, 8);
}

volatile void empty() {
}
// dont do anything funny here. Or be cautious. Register clobbering
// could be an issue
int syscall_entry()
{
    register uint64_t nr;
    asm("nop" :"=a"(nr));
    switch(nr) {
    case 0:
	printf("Bad system call\n");
	break;
    case 0x20a:
	//scheduler();
	sched();
	break;
    case 0x20b:
	asm volatile("callq printf_\n\t");
	//printf("voila\n");
	break;
    default:
	break;
    }
    return nr;
}

/* 
 * The syscall instruction, puts rip in rcx
 r11 has the rflags. 
   
 * STAR MSR
 63-48	| 47-32 | 31-0
 SS_CS	| SS_CS | 32 bit syscall rip
 
 here, ss is assumed to be 0x08 above the cs. 
 this seems to be implicity assumption in the code.
 see https://www.felixcloutier.com/x86/syscall.html,
 for operation of the syscall

 * LSTAR MSR
 63-0
 SYSCALL target RIP in 64 bit mode

*/
void set_syscall_msrs()
{
    uint64_t addr;
    uint32_t ss_cs_kernel_call, ss_cs_user_ret;
    uint32_t star, addr_h, addr_l;
    //printf("In %s\n", __func__);
    // 0x10 is ss 0x08 is code segment
    ss_cs_kernel_call = (0x08);
    // sysret load cs.selector as ss_cs_user_ret+16
    // and ss.selector as ss_cs_user_ret+8
    ss_cs_user_ret = 0x10 | 0x3; // user cs
    star = ((ss_cs_user_ret << 16) | ss_cs_kernel_call);
    set_msr(MSR_STAR, star, 0);
    addr = (uint64_t)&__syscall_entry;
    addr_h = addr >> 32;
    addr_l = addr & 0xFFFFFFFF;
    set_msr(MSR_LSTAR, addr_h, addr_l);
    set_msr(MSR_SFMASK, 0, 0);
}

void enable_interrupts()
{
    struct apic_t *apic = 0xFEE00000;
    asm volatile("sti \n\t");
}

//static mutex_t mutex_bss_load = {0,0,1};
static mutex_t mutex_tss_fill_flush = {0};
void init_boot(uint8_t my_id, uint8_t pool_sz)
{
    int i;
    uint64_t stack_addr;
    gdt_entry_t *gdt_start = (gdt_entry_t *)gdt64_start_c;
    //my_id = inb(PORT_MY_ID);
    //my_id = get_id();
    //pool_sz = get_pool_sz();
    //printf("my id = %d, pool_sz = %d\n", my_id, pool_sz);
    // zero out the bss section
    // below code is commented because, kernel already gives
    // zero out pages (TBD verify)
    /*
    mutex_lock_hlt(&mutex_bss_load);
    memset((void *)bss_start, 0, bss_size);
    mutex_unlock_hlt(&mutex_bss_load);
    */
    //printf("Filling TSS\n");
    // my_id+5 because first 4 entries are occupied in gdt
    stack_addr = *(uint64_t *)(kern_stack + my_id); // keep some 16 bytes free
    //printf("stack_addr[%d] = %llX\n", my_id, stack_addr);

    // perhaps the tss filling and flushing
    // must happen as atomic
    // if one processor modified tss, while other
    // does ltr, perhaps that is the issue
    mutex_lock(&mutex_tss_fill_flush);
    fill_tss(&tss_seg[my_id], (struct sys_desc_t *)&gdt_start[my_id+5],
	stack_addr);
    //printf("Flushing TSS\n");
    flush_tss(my_id);
    mutex_unlock(&mutex_tss_fill_flush);
    //printf("Filling user mode gdt\n");
    fill_user_mode_gdt();
    //printf("Setting up syscalls\n");
    set_syscall_msrs();
    enable_interrupts();
//    for(i = 0; i <= 6; i++)
//	printf("descriptor[%d] = %016llX\n", i, *(uint64_t *)&gdt_start[i]);    
}

