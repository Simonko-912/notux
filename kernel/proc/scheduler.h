#pragma once
#include <stdint.h>
#include "process.h"

void     sched_init(void);
int      sched_add(Process *proc);
void     sched_remove(Process *proc);
int      sched_reap_child(int *status);
void     sched_tick(CpuState *state);
__attribute__((noreturn)) void sched_idle(void);
__attribute__((noreturn)) void sched_enter_from_exit(void);
void     sched_yield(void);
void     sched_block(WaitReason reason);
void     sched_unblock(Process *proc);
void     sched_sleep_ms(uint32_t ms);
uint64_t sched_uptime_ms(void);
