#ifndef __SCHED_H__
#define __SCHED_H__

void scheduler_init_pre(uint8_t pool_sz);
void scheduler_init_post(uint8_t pool_sz);
void inc_active_cpu();
void scheduler();
void new_sched();
void wake_up_event(uint64_t id);
#endif
