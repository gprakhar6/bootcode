#ifndef __SCHED_H__
#define __SCHED_H__

#define MAX_FUNC_PTRS_SCHED (10)

struct sched_t {
    void (*fptrs[MAX_FUNC_PTRS_SCHED])();
    int fptrs_sz;
    int fidx;
};

void scheduler_init(void (*fptrs[])(), int sz);
void scheduler();
//void scheduler_init();

#endif
