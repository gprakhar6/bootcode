#include <stdint.h>
#include <stddef.h>
#include "printf.h"
#include "util.h"
#include "stack.h"

#define define_stack_init(x) void CONCAT(stack_init_, x)(t_stack *q, x *h, int tsz) { \
	q->arr = (void *)h;						\
	q->head = (void *)h;						\
	q->n = 0;							\
	mutex_init(&(q->m));						\
	q->tsz = tsz;							\
    }

#define define_stack_push(x) int CONCAT(stack_push_, x)(t_stack *q, x *e) { \
	int ret;							\
	mutex_lock(&(q->m));    					\
	if(q->n < q->tsz) {						\
	    ret = 0;							\
	    *(x *)(q->head) = *e;					\
	    q->head = (void *)(((x *)q->head) + 1);			\
	    q->n += 1;							\
	} else {							\
	    ret = -1;							\
	}								\
	mutex_unlock(&(q->m));	           				\
	return ret;							\
    }


#define define_stack_pop(x) int CONCAT(stack_pop_, x)(t_stack *q, x *e) { \
	int ret;							\
	mutex_lock(&(q->m)); 	            				\
	if(q->n > 0) {							\
	    ret = 0;							\
	    q->head = (void *)(((x *)q->head) - 1);			\
	    *e = *(x *)(q->head);					\
	    q->n -= 1;							\
	} else {							\
	    ret = -1;							\
	}								\
	mutex_unlock(&(q->m));   					\	
	return ret;							\
    }


#define define_stack_funcs(x)			\
    define_stack_init(x);			\
    define_stack_push(x);			\
    define_stack_pop(x);			\


define_stack_funcs(uint8_t);
