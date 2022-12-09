#ifndef __STACK_H__
#define __STACK_H__

#include <stdint.h>
#include "globvar.h"
#include "hw_types.h"

#define CONCAT(x, y) x ## y

typedef struct {
    void *arr;
    void *head;
    mutex_t m;
    int n;
    int tsz;
} t_stack;

#define decl_stack_init(x) void CONCAT(stack_init_, x)(t_stack *q, x *h, int tsz)
#define decl_stack_push(x) int CONCAT(stack_push_, x)(t_stack *q, x *e)
#define decl_stack_pop(x) int CONCAT(stack_pop_, x)(t_stack *q, x *e)
static inline int stack_current_size(t_stack *q) {	
    return q->n;						       
}

#define decl_stack_funcs(x)			\
    decl_stack_init(x);				\
    decl_stack_push(x);				\
    decl_stack_pop(x);

decl_stack_funcs(uint8_t);

#define stack_init(q, h, tsz) _Generic((h),			\
				       uint8_t* : stack_init_uint8_t,	\
				       default : 0)(q, h, tsz)

#define stack_push(q, e) _Generic((e),				\
				  uint8_t* : stack_push_uint8_t,	\
				  default : 0)(q, e)

#define stack_pop(q, e) _Generic((e),			\
				 uint8_t* : stack_pop_uint8_t,	\
				 default : 0)(q, e)


#endif
