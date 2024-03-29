#ifndef __CUSTOM_GLOBAL_H__
#define __CUSTOM_GLOBAL_H__

#include <stdint.h>
#include "hw_types.h"

#define SIZE_ARR_1D(a) (sizeof(a)/sizeof(a[0]))
// serial_port (0x3f9) in boot.S
#define PORT_WAIT_USER_CODE_MAPPING     (0x3f9)
#define PORT_HLT                        (0x3fa)
#define PORT_PRINT_REGS                 (0x3fb)
#define PORT_MY_ID                      (0x3fc)  // 0x3fd
#define PORT_MSG                        (0x3fe) // and 3ff

#define MAX_CPUS                        (64)
#define MAX_FUNC                        (256) // change get_work if change this, loop unroll
#define NULL_FUNC                       (255)

#define MSG_SENT_INIT_START             (1)
#define MSG_BOOTED                      (2)
#define MSG_WAITING_FOR_WORK            (3)

#define TSC_TO_US_DIV                   (3400)

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

extern mutex_t mutex_printf;
//extern uint64_t t1, t2;
#endif
