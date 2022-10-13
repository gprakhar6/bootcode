#ifndef __MM_H__
#define __MM_H__

#define U_BIT 0x04
#define NX_BIT (0x1 << 63)
pde_t* addr_to_pde(void *cr3, uint64_t addr, uint64_t *pe[]);

#endif
