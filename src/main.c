#include <stdint.h>
#include <stddef.h>
#include "printf.h"
#include "util.h"
#include "hw_types.h"
#include "globvar.h"
#include "mm_types.h"
#include "mm.h"

int user_test_function();
void init_boot();
extern void jump_usermode(void *rip, void *stack);
extern void flush_tss();

extern uint64_t gdt64_start_c[];
extern uint64_t boot_p4[];
struct tss_entry_t __attribute__((aligned(64))) tss_seg;
uint8_t __attribute__((aligned(16))) user_stack[16*32];

void user_test_func() {
    asm("int $0x80");
    asm("int $0x80");
    asm("int $0x80");
    return;
}
int main()
{
    pde_t *pde;
    pde_t *p1,*p2,*p3;
    pde_t **pe[3] = {&p1, &p2, &p3};
    printf("Calling init boot\n");
    init_boot();
    pde = addr_to_pde((void *)boot_p4, 0x200000, (uint64_t **)pe);
    p1->pde |= U_BIT;
    p2->pde |= U_BIT;
    p3->pde |= U_BIT; // need to update TLB maybe?
    //pde->pde |= U_BIT; // allow user access
    
    memcpy(0x200000, &user_test_func, 64);
    hexdump(0x200000, 24);
    printf("sof = %d\n", sizeof(user_test_func));
    printf("pde in  main = %llX\n", pde->pde);
    printf("Jumping to user func\n");
    jump_usermode(0x200000, 0x200000 + 0x1000);
    //jump_usermode(user_test_func, user_stack + sizeof(user_stack) - 16);
    printf("Not expected to be here");
    return 0;
}

void fill_tss(struct tss_entry_t *tss, struct sys_desc_t *tss_desc)
{
    int sz;

    // fill tss table
    memset(tss, 0, sizeof(*tss));
    tss->rsp0 = stack_start - 16; // keep some 16 bytes free
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
    hexdump(ucs, 8);
    init_DS(uds);
    uds->DS.DPL = 3;
    hexdump(uds, 8);
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
    for(i = 0; i <= 6; i++)
	printf("descriptor[%d] = %016llX\n", i, *(uint64_t *)&gdt_start[i]);    
}

