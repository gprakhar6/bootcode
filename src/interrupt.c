#include <stdint.h>
#include <stddef.h>
#include "printf.h"
#include "util.h"

struct stack_frame_err_t {
    struct {
	uint16_t reserved1 : 16;	
	uint16_t int_num   : 16;
	uint32_t reserved0 : 32;
    } __attribute__((packed));
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));
struct stack_frame_no_err_t {
    struct {
	uint32_t reserved0 : 32;
	uint16_t int_num   : 16;
	uint16_t reserved1 : 16;
    } __attribute__((packed));
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));

void high_interrupt_handler(void *rdi)
{
    //uint64_t indicator;
    struct stack_frame_no_err_t *sf_no_err;
    struct stack_frame_err_t *sf_err;
    //indicator = *(uint64_t *)rdi;
    sf_no_err = (struct stack_frame_no_err_t *)rdi;
    sf_err = (struct stack_frame_err_t *)rdi;
    //printf("indicator  = %016llX\n", indicator);    
    printf("error_code = %016llX\n", sf_err->error_code);
    printf("int_num    = %016llX\n", sf_err->int_num);
    printf("rip        = %016llX\n", sf_err->rip);
    printf("cs         = %016llX\n", sf_err->cs);
    printf("rflags     = %016llX\n", sf_err->rflags);
    printf("rsp        = %016llX\n", sf_err->rsp);
    printf("ss         = %016llX\n", sf_err->ss);
       
    return;
}
