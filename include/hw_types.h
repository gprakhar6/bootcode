#ifndef __HW_TYPES_H__
#define __HW_TYPES_H__

#include <stdint.h>

typedef union {
    struct __attribute__((packed)) {	
	uint32_t ignored0       : 16;
	uint32_t ignored1       : 24; 
	uint32_t ignored2       :  1;
	uint32_t ignored3       :  1;
	uint32_t conforming     :  1; // conforming for code
	uint32_t hardcode0_1    :  1;
	uint32_t hardcode1_1    :  1;
	uint32_t DPL            :  2; // privilege level
	uint32_t present        :  1;
	uint32_t ignored4       :  4;
	uint32_t ignored5       :  1;
	uint32_t long_mode      :  1;
	uint32_t hardcode2_0    :  1;
	uint32_t ignored6       :  1;
	uint32_t ignored7       :  8;	
    } CS;

    struct __attribute__((packed)) {
	uint32_t ignored0       : 16;
	uint32_t ignored1       : 24; 
	uint32_t ignored2       :  1;
	uint32_t ignored3       :  1;
	uint32_t ignored4	:  1;
	uint32_t hardcode0_0    :  1;
	uint32_t hardcode1_1    :  1;
	uint32_t ignored5       :  2;
	uint32_t present        :  1;
	uint32_t ignored6       :  4;
	uint32_t ignored7       :  1;
	uint32_t ignored8	:  1;
	uint32_t ignored9	:  1;
	uint32_t ignored10      :  1;
	uint32_t ignored11      :  8;
    } DS;
} gdt_entry_t;

struct __attribute__((packed)) tss_entry_t {
    uint32_t reserved0	: 32;
    uint64_t rsp0	: 64;
    uint64_t rsp1	: 64;
    uint64_t rsp2	: 64;
    uint64_t reserved1	: 64;
    uint64_t ist1	: 64;
    uint64_t ist2	: 64;
    uint64_t ist3	: 64;
    uint64_t ist4	: 64;
    uint64_t ist5	: 64;
    uint64_t ist6	: 64;
    uint64_t ist7	: 64;
    uint64_t reserved2	: 64;
    uint16_t reserved3	: 16;
    uint16_t iomap_base : 16;
};

struct __attribute__((packed)) sys_desc_t {
    uint16_t limit_15_0		: 16;
    uint32_t base_23_0		: 24;
    uint16_t type		: 4;
    uint16_t hardcode0_0	: 1;
    uint16_t dpl		: 2;
    uint16_t present		: 1;
    uint16_t limit_19_16	: 4;
    uint16_t avl		: 1;
    uint16_t reserved0		: 2;
    uint16_t granularity	: 1;
    uint16_t base_31_24		: 8;
    uint32_t base_64_32		: 32;
    uint16_t reserved1		: 8;
    uint16_t hardcode1_0	: 5;
    uint32_t reserved2		: 19;
};
#endif
