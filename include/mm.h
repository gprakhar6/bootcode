#ifndef __MM_H__
#define __MM_H__

#define U_BIT 0x40

pde_t* addr_to_pde(void *cr3, uint64_t addr);

#endif
