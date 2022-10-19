#include <stdint.h>
#include <stddef.h>
#include "printf.h"
#include "util.h"
#include "globvar.h"
#include "pic.h"

struct stack_frame_err_t {
    uint64_t int_num;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));

extern void (*int_func[256])(struct stack_frame_err_t *sf);
void null_func(struct stack_frame_err_t *sf);
void page_fault_handler(struct stack_frame_err_t *sf);
void handler_0x80(struct stack_frame_err_t *sf);

void high_interrupt_handler(void *rdi)
{
    //uint64_t indicator;
    struct stack_frame_err_t *sf_err;
    //indicator = *(uint64_t *)rdi;
    sf_err = (struct stack_frame_err_t *)rdi;
//    if(sf_err->int_num == 0x81)
//	asm("hlt");
//    else
//	return;
    //printf("indicator  = %016llX\n", indicator);
    printf("int_num    = %016llX\n", sf_err->int_num);
    printf("error_code = %016llX\n", sf_err->error_code);
    printf("rip        = %016llX\n", sf_err->rip);
    printf("cs         = %016llX\n", sf_err->cs);
    printf("rflags     = %016llX\n", sf_err->rflags);
    printf("rsp        = %016llX\n", sf_err->rsp);
    printf("ss         = %016llX\n", sf_err->ss);
    int_func[sf_err->int_num](sf_err);
    return;
}

void null_func(struct stack_frame_err_t *sf) {
    printf("null_func\n");
    return;
}

static char pf_err_code[][128] =
{
    "0:nopresent",
    "@R/W",
    "@S/U",
    "1:RSV, reserved field read 1, cond:cr4.pse,pae",
    "@D/I,cond:cr4,nxe,pae",
    "1:MPK Violation,cond:cr4.pke",
    "1:shadow stack access, cr4.cet",
    "1:rmp violation,cond:syscgf[securenestedpaging]"
};
void page_fault_handler(struct stack_frame_err_t *sf)
{
    int i;
    uint64_t cr2;
    printf("Page Fault:\n");
    for(i = 0; i < SIZE_ARR_1D(pf_err_code); i++) {
	if((sf->error_code & (0x1 << i)) != 0) {
	    if(pf_err_code[i][0] != '0')
		printf("Bit %d = 1: %s\n", i, (char *)&pf_err_code[i][0]);
	}
	else
	    if(pf_err_code[i][0] == '0')
		printf("Bit %d = 0: %s\n", i, (char *)&pf_err_code[i][0]);
    }
    asm("movq %%cr2, %0" : "=r" (cr2));

    printf("CR2 = %016llX\n", cr2);
}

void pic_handler(struct stack_frame_err_t *sf)
{
    PIC_sendEOI(sf->int_num - PIC_REMAP);
}
void handler_0x80(struct stack_frame_err_t *sf)
{
    printf("0x80 handler\n");
    return;
}

void handler_0x81(struct stack_frame_err_t *sf)
{
    printf("Halting in 0x81 Handler\n");
    asm("hlt");
    return;
}

void handler_0x82(struct stack_frame_err_t *sf)
{
    return;
}

void (*int_func[256])(struct stack_frame_err_t *sf) =
{
    [0x00] = null_func,
    [0x01] = null_func,
    [0x02] = null_func,
    [0x03] = null_func,
    [0x04] = null_func,
    [0x05] = null_func,
    [0x06] = null_func,
    [0x07] = null_func,
    [0x08] = null_func,
    [0x09] = null_func,
    [0x0a] = null_func,
    [0x0b] = null_func,
    [0x0c] = null_func,
    [0x0d] = null_func,
    [0x0e] = page_fault_handler,
    [0x0f] = null_func,
    [0x10] = null_func,
    [0x11] = null_func,
    [0x12] = null_func,
    [0x13] = null_func,
    [0x14] = null_func,
    [0x15] = null_func,
    [0x16] = null_func,
    [0x17] = null_func,
    [0x18] = null_func,
    [0x19] = null_func,
    [0x1a] = null_func,
    [0x1b] = null_func,
    [0x1c] = null_func,
    [0x1d] = null_func,
    [0x1e] = null_func,
    [0x1f] = null_func,
    [0x20] = pic_handler,
    [0x21] = null_func,
    [0x22] = null_func,
    [0x23] = null_func,
    [0x24] = null_func,
    [0x25] = null_func,
    [0x26] = null_func,
    [0x27] = null_func,
    [0x28] = null_func,
    [0x29] = null_func,
    [0x2a] = null_func,
    [0x2b] = null_func,
    [0x2c] = null_func,
    [0x2d] = null_func,
    [0x2e] = null_func,
    [0x2f] = null_func,
    [0x30] = null_func,
    [0x31] = null_func,
    [0x32] = null_func,
    [0x33] = null_func,
    [0x34] = null_func,
    [0x35] = null_func,
    [0x36] = null_func,
    [0x37] = null_func,
    [0x38] = null_func,
    [0x39] = null_func,
    [0x3a] = null_func,
    [0x3b] = null_func,
    [0x3c] = null_func,
    [0x3d] = null_func,
    [0x3e] = null_func,
    [0x3f] = null_func,
    [0x40] = null_func,
    [0x41] = null_func,
    [0x42] = null_func,
    [0x43] = null_func,
    [0x44] = null_func,
    [0x45] = null_func,
    [0x46] = null_func,
    [0x47] = null_func,
    [0x48] = null_func,
    [0x49] = null_func,
    [0x4a] = null_func,
    [0x4b] = null_func,
    [0x4c] = null_func,
    [0x4d] = null_func,
    [0x4e] = null_func,
    [0x4f] = null_func,
    [0x50] = null_func,
    [0x51] = null_func,
    [0x52] = null_func,
    [0x53] = null_func,
    [0x54] = null_func,
    [0x55] = null_func,
    [0x56] = null_func,
    [0x57] = null_func,
    [0x58] = null_func,
    [0x59] = null_func,
    [0x5a] = null_func,
    [0x5b] = null_func,
    [0x5c] = null_func,
    [0x5d] = null_func,
    [0x5e] = null_func,
    [0x5f] = null_func,
    [0x60] = null_func,
    [0x61] = null_func,
    [0x62] = null_func,
    [0x63] = null_func,
    [0x64] = null_func,
    [0x65] = null_func,
    [0x66] = null_func,
    [0x67] = null_func,
    [0x68] = null_func,
    [0x69] = null_func,
    [0x6a] = null_func,
    [0x6b] = null_func,
    [0x6c] = null_func,
    [0x6d] = null_func,
    [0x6e] = null_func,
    [0x6f] = null_func,
    [0x70] = null_func,
    [0x71] = null_func,
    [0x72] = null_func,
    [0x73] = null_func,
    [0x74] = null_func,
    [0x75] = null_func,
    [0x76] = null_func,
    [0x77] = null_func,
    [0x78] = null_func,
    [0x79] = null_func,
    [0x7a] = null_func,
    [0x7b] = null_func,
    [0x7c] = null_func,
    [0x7d] = null_func,
    [0x7e] = null_func,
    [0x7f] = null_func,
    [0x80] = handler_0x80,
    [0x81] = handler_0x81,
    [0x82] = handler_0x82,
    [0x83] = null_func,
    [0x84] = null_func,
    [0x85] = null_func,
    [0x86] = null_func,
    [0x87] = null_func,
    [0x88] = null_func,
    [0x89] = null_func,
    [0x8a] = null_func,
    [0x8b] = null_func,
    [0x8c] = null_func,
    [0x8d] = null_func,
    [0x8e] = null_func,
    [0x8f] = null_func,
    [0x90] = null_func,
    [0x91] = null_func,
    [0x92] = null_func,
    [0x93] = null_func,
    [0x94] = null_func,
    [0x95] = null_func,
    [0x96] = null_func,
    [0x97] = null_func,
    [0x98] = null_func,
    [0x99] = null_func,
    [0x9a] = null_func,
    [0x9b] = null_func,
    [0x9c] = null_func,
    [0x9d] = null_func,
    [0x9e] = null_func,
    [0x9f] = null_func,
    [0xa0] = null_func,
    [0xa1] = null_func,
    [0xa2] = null_func,
    [0xa3] = null_func,
    [0xa4] = null_func,
    [0xa5] = null_func,
    [0xa6] = null_func,
    [0xa7] = null_func,
    [0xa8] = null_func,
    [0xa9] = null_func,
    [0xaa] = null_func,
    [0xab] = null_func,
    [0xac] = null_func,
    [0xad] = null_func,
    [0xae] = null_func,
    [0xaf] = null_func,
    [0xb0] = null_func,
    [0xb1] = null_func,
    [0xb2] = null_func,
    [0xb3] = null_func,
    [0xb4] = null_func,
    [0xb5] = null_func,
    [0xb6] = null_func,
    [0xb7] = null_func,
    [0xb8] = null_func,
    [0xb9] = null_func,
    [0xba] = null_func,
    [0xbb] = null_func,
    [0xbc] = null_func,
    [0xbd] = null_func,
    [0xbe] = null_func,
    [0xbf] = null_func,
    [0xc0] = null_func,
    [0xc1] = null_func,
    [0xc2] = null_func,
    [0xc3] = null_func,
    [0xc4] = null_func,
    [0xc5] = null_func,
    [0xc6] = null_func,
    [0xc7] = null_func,
    [0xc8] = null_func,
    [0xc9] = null_func,
    [0xca] = null_func,
    [0xcb] = null_func,
    [0xcc] = null_func,
    [0xcd] = null_func,
    [0xce] = null_func,
    [0xcf] = null_func,
    [0xd0] = null_func,
    [0xd1] = null_func,
    [0xd2] = null_func,
    [0xd3] = null_func,
    [0xd4] = null_func,
    [0xd5] = null_func,
    [0xd6] = null_func,
    [0xd7] = null_func,
    [0xd8] = null_func,
    [0xd9] = null_func,
    [0xda] = null_func,
    [0xdb] = null_func,
    [0xdc] = null_func,
    [0xdd] = null_func,
    [0xde] = null_func,
    [0xdf] = null_func,
    [0xe0] = null_func,
    [0xe1] = null_func,
    [0xe2] = null_func,
    [0xe3] = null_func,
    [0xe4] = null_func,
    [0xe5] = null_func,
    [0xe6] = null_func,
    [0xe7] = null_func,
    [0xe8] = null_func,
    [0xe9] = null_func,
    [0xea] = null_func,
    [0xeb] = null_func,
    [0xec] = null_func,
    [0xed] = null_func,
    [0xee] = null_func,
    [0xef] = null_func,
    [0xf0] = null_func,
    [0xf1] = null_func,
    [0xf2] = null_func,
    [0xf3] = null_func,
    [0xf4] = null_func,
    [0xf5] = null_func,
    [0xf6] = null_func,
    [0xf7] = null_func,
    [0xf8] = null_func,
    [0xf9] = null_func,
    [0xfa] = null_func,
    [0xfb] = null_func,
    [0xfc] = null_func,
    [0xfd] = null_func,
    [0xfe] = null_func,
    [0xff] = null_func,
};



