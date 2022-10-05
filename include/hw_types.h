#ifndef __HW_TYPES_H__
#define __HW_TYPES_H__

#include <stdint.h>

typedef union {
    struct {	
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
    } __attribute__((packed)) CS;

    struct {
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
    } __attribute__((packed)) DS;
} gdt_entry_t;

struct tss_entry_t {
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
} __attribute__((packed));


#endif
