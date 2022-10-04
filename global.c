#ifndef __CUSTOM_GLOBAL_H__
#define __CUSTOM_GLOBAL_H__

/*
= : page align
  -------------- CODE_START
  -------------- LAYOUT_START
    LAYOUT_DATA
  -------------- LAYOUT_END = LAYOUT_START + LAYOUT_SIZE
        =   
  -------------- USER_CODE_START
       .text
  -------------- USER_CODE_END = USER_CODE_START + USER_CODE_SIZE
        = 
  -------------- RODATA_START
       .rodata
  -------------- RODATA_END = RODATA_START + RODATA_SIZE
  -------------- CODE_END = CODE_START + CODE_SIZE
        = 
  -------------- DATA_START
       .bss
       .data
  -------------- DATA_END = DATA_START + DATA_SIZE
       =
       STACK_SPACE
  -------------- STACK_START     

*/
#include <stdint.h>
extern char CODE_START[], CODE_SIZE[];
extern char LAYOUT_START[], LAYOUT_SIZE[];
extern char USER_CODE_START[], USER_CODE_SIZE[];
extern char DATA_START[], DATA_SIZE[];
extern char STACK_START[], STACK_SIZE[];

__attribute__((section(".layout"))) const uint64_t code_start = (uint64_t)&CODE_START;
__attribute__((section(".layout"))) const uint64_t code_size = (uint64_t)&CODE_SIZE;
__attribute__((section(".layout"))) const uint64_t layout_start = (uint64_t)&LAYOUT_START;
__attribute__((section(".layout"))) const uint64_t layout_size = (uint64_t)&LAYOUT_SIZE;
__attribute__((section(".layout"))) const uint64_t user_code_start = (uint64_t)&USER_CODE_START;
__attribute__((section(".layout"))) const uint64_t user_code_size = (uint64_t)&USER_CODE_SIZE;
__attribute__((section(".layout"))) const uint64_t data_start = (uint64_t)&DATA_START;
__attribute__((section(".layout"))) const uint64_t data_size = (uint64_t)&DATA_SIZE;
__attribute__((section(".layout"))) const uint64_t stack_start = (uint64_t)&STACK_START;
__attribute__((section(".layout"))) const uint64_t stack_size = (uint64_t)&STACK_SIZE;


    
#endif
