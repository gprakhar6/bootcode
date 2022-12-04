#ifndef __QUEUE_H__
#define __QUEUE_H__

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
} t_queue;

#define decl_queue_init(x) void CONCAT(queue_init_, x)(t_queue *q, x *h, int tsz)
#define decl_queue_push(x) int CONCAT(queue_push_, x)(t_queue *q, x *e)
#define decl_queue_pop(x) int CONCAT(queue_pop_, x)(t_queue *q, x *e)

#define decl_queue_funcs(x)			\
    decl_queue_init(x);				\
    decl_queue_push(x);				\
    decl_queue_pop(x);

decl_queue_funcs(uint8_t);

#define queue_init(q, h, tsz) _Generic((h),			\
				       uint8_t* : queue_init_uint8_t,	\
				       default : 0)(q, h, tsz)

#define queue_push(q, e) _Generic((e),				\
				  uint8_t* : queue_push_uint8_t,	\
				  default : 0)(q, e)

#define queue_pop(q, e) _Generic((e),			\
				 uint8_t* : queue_pop_uint8_t,	\
				 default : 0)(q, e)

#endif
