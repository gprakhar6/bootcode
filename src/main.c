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

int user_test_function();
void init_boot();
extern void flush_tss();

extern char __syscall_entry[];
extern uint64_t gdt64_start_c[];
extern uint64_t boot_p4[];
extern uint64_t kern_stack[];
struct tss_entry_t __attribute__((aligned(64))) tss_seg;
uint8_t __attribute__((aligned(16))) user_stack[16*32];

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

int main()
{
    pde_t *pde;
    pde_t *p1,*p2,*p3;
    pde_t **pe[3] = {&p1, &p2, &p3};
    volatile uint64_t i;
    void (*fptrs[2])();
    return;
    //mutex_init(&mutex_printf);
    printf("Calling init boot\n");
    return;
    init_boot();
#if 0    
    pde = addr_to_pde((void *)boot_p4, 0x200000, (uint64_t **)pe);
    p1->pde |= U_BIT;
    p2->pde |= U_BIT;
    p3->pde |= U_BIT;
    memcpy(0x200100, &user_test_func, 0x1000);
//    PIC_remap(PIC_REMAP, PIC_REMAP + 0x08);
//    for(i = 0; i <= 0xf; i++)
//	IRQ_clear_mask(i);
    fptrs[0] = (void (*)())0x200100;
    fptrs[1] = (void (*)())0x200100;
#endif
    outb(PORT_WAIT_USER_CODE_MAPPING, 1);
    //outb(PORT_HLT, 0);
    while(1);
    scheduler_init(fptrs, 2);
    scheduler();
    printf("Jumping to user func\n");
    //jump_usermode(user_test_func, user_stack + sizeof(user_stack) - 16);
    printf("Not expected to be here");
    return 0;
}

void fill_tss(struct tss_entry_t *tss, struct sys_desc_t *tss_desc)
{
    int sz;

    // fill tss table
    memset(tss, 0, sizeof(*tss));
    // need to see its correctness
    tss->rsp0 = kern_stack - 16; // keep some 16 bytes free
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

    ucs = &gdt_start[3];
    uds = &gdt_start[4];

    init_CS(ucs);
    ucs->CS.DPL = 3;
    //hexdump(ucs, 8);
    init_DS(uds);
    uds->DS.DPL = 3;
    //hexdump(uds, 8);
}

void syscall_entry()
{
    register uint64_t nr;
    asm("nop" :"=a"(nr));
    
    switch(nr) {
    case 10:
	scheduler();
	break;
    default:
	break;
    }
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
    uint32_t ss_cs, star, addr_h, addr_l;
    printf("In %s\n", __func__);
    // 0x10 is ss 0x08 is code segment
    ss_cs = (0x08);
    star = ((ss_cs << 16) | ss_cs);
    set_msr(MSR_STAR, star, 0);
    addr = (uint64_t)&__syscall_entry;
    addr_h = addr >> 32;
    addr_l = addr & 0xFFFFFFFF;
    set_msr(MSR_LSTAR, addr_h, addr_l);
    set_msr(MSR_SFMASK, 0, 0);
}

void init_boot()
{
    int i;
    gdt_entry_t *gdt_start = (gdt_entry_t *)gdt64_start_c;
    // zero out the bss section
    memset((void *)bss_start, 0, bss_size);
    printf("Filling TSS\n");
    fill_tss(&tss_seg, (struct sys_desc_t *)&gdt_start[5]);
    printf("Flushing TSS\n");
    flush_tss();
    printf("Filling user mode gdt\n");
    fill_user_mode_gdt();
    printf("Setting up syscalls\n");
    set_syscall_msrs();
//    for(i = 0; i <= 6; i++)
//	printf("descriptor[%d] = %016llX\n", i, *(uint64_t *)&gdt_start[i]);    
}

