#ifndef __CUSTOM_GLOBAL_H__
#define __CUSTOM_GLOBAL_H__

#include <stdint.h>

#define SIZE_ARR_1D(a) (sizeof(a)/sizeof(a[0]))

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
