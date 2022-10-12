#include <stdint.h>
#include <stddef.h>
#include "util.h"
#include "printf.h"
#include "mm_types.h"
#include "mm.h"

#define EMASK (0x00FFFFFFFFFFFC00)

pde_t* addr_to_pde(void *cr3, uint64_t addr, uint64_t *pe[])
{
    uint8_t i, levels, bit_len;
    pde_t *pde;
    uint64_t base;
    uint64_t *eaddr;
    uint16_t entry_num;
    const uint16_t entry_mask = 0x1FF;
    base = (uint64_t)cr3;
    addr = addr >> (12+9);
    levels = 3;
    bit_len = 9;
    for(i = 0; i < levels; i++) {
	entry_num = (addr >> ((levels - 1 - i) * bit_len)) &
	    entry_mask;
	printf("entry_num = %d\n", entry_num);
	eaddr = (uint64_t *)((base & EMASK) +
			     entry_num * sizeof(pde_t));
	if(pe != NULL) {
	    if(pe[i] != NULL)
		*pe[i] = eaddr;
	}
	base = *eaddr;
	printf("base = %16llX\n", base);
    }
    pde = (pde_t *)eaddr;
    return pde;
}
