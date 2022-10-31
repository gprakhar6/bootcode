#ifndef __CUSTOM_GLOBAL_H__
#define __CUSTOM_GLOBAL_H__

#include <stdint.h>

#define SIZE_ARR_1D(a) (sizeof(a)/sizeof(a[0]))
// serial_port (0x3f9) in boot.S
#define PORT_WAIT_USER_CODE_MAPPING (0x3f9)
#define  PORT_HLT                        (0x3fa)
#define  PORT_PRINT_REGS                 (0x3fb)

extern const uint64_t code_start;
extern const uint64_t code_size;
extern const uint64_t layout_start;
extern const uint64_t layout_size;
extern const uint64_t user_code_start;
extern const uint64_t user_code_size;
extern const uint64_t page_ds_start;
extern const uint64_t page_ds_size;
extern const uint64_t data_start;
extern const uint64_t data_size;
extern const uint64_t bss_start;
extern const uint64_t bss_size;
extern const uint64_t stack_start;
extern const uint64_t stack_size;


//extern uint64_t t1, t2;
#endif
