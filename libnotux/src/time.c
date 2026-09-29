/*
 * libnotux — time API
 */

#include <notux/libc.h>
#include <notux/syscalls.h>
#include "nx.h"

uint64_t nx_uptime_ms(void) {
    return (uint64_t)nx_syscall(SYS_UPTIME, 0, 0, 0);
}

void nx_gettime(NxTime *t) {
    if (!t) return;
    t->year = 2026;
    t->month = 1;
    t->day = 1;
    t->hour = 0;
    t->minute = 0;
    t->second = 0;
}