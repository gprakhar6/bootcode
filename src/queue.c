#include <stdint.h>
#include <stddef.h>
#include "printf.h"
#include "util.h"
#include "queue.h"

#define define_queue_init(x) void CONCAT(queue_init_, x)(t_queue *q, x *h, int tsz) { \
	q->arr = (void *)h;						\
	q->head = (void *)h;						\
	q->n = 0;							\
	mutex_init(&(q->m));						\
	q->tsz = tsz;							\
    }

#define define_queue_push(x) int CONCAT(queue_push_, x)(t_queue *q, x *e) { \
	int ret;							\
	mutex_lock_hlt(&(q->m)); 					\
	if(q->n < q->tsz) {						\
	    ret = 0;							\
	    *(x *)(q->head) = *e;					\
	    q->head = (void *)(((x *)q->head) + 1);			\
	    q->n += 1;							\
	} else {							\
	    ret = -1;							\
	}								\
	mutex_unlock_hlt(&(q->m));					\
	return ret;							\
    }


#define define_queue_pop(x) int CONCAT(queue_pop_, x)(t_queue *q, x *e) { \
	int ret;							\
	mutex_lock_hlt(&(q->m)); 					\
	if(q->n > 0) {							\
	    ret = 0;							\
	    q->head = (void *)(((x *)q->head) - 1);			\
	    *e = *(x *)(q->head);					\
	    q->n -= 1;							\
	} else {							\
	    ret = -1;							\
	}								\
	mutex_unlock_hlt(&(q->m));					\	
	return ret;							\
    }


#define define_queue_funcs(x)			\
    define_queue_init(x);			\
    define_queue_push(x);			\
    define_queue_pop(x);


define_queue_funcs(uint8_t);
