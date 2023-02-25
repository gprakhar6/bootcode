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
#include "ipi.h"

int send_ipi_to_all(uint8_t vector)
{
    int i, ret;    
    volatile struct apic_t *apic = 0xFEE00000;
    uint64_t wait_cnt = 1024;
    
    apic->esr = 0;
    //apic->icrh = (0x01) << 24;
    // 0xC -> all exclusing self
    // 0x40 is the ipi
    apic->icrl = ICRL(vector) | (0xC << 16);
    //printf("esr: %08X\n", apic->esr);
    while(((1 < 12) & apic->icrl == 1) && (wait_cnt--));
    ret = (((1 < 12) & apic->icrl == 0) == 0) ? 1 : 0;
    return ret;
}

int send_ipi_to(uint8_t id, uint8_t vector)
{
    int i, ret;
    volatile struct apic_t *apic = 0xFEE00000;
    uint64_t wait_cnt = 1024;
    
    apic->esr = 0;
    apic->icrh = ((uint32_t)id) << 24;
    apic->icrl = ICRL(vector);
    while(((1 < 12) & apic->icrl == 1) && (wait_cnt--));
    ret = (((1 < 12) & apic->icrl == 0) == 0) ? 1 : 0;
    return ret;
}

