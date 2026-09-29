#pragma once
#include <stdint.h>
#include <stddef.h>

long nx_syscall6(long nr, long a1, long a2, long a3, long a4, long a5);

static inline long nx_syscall(long nr, long a1, long a2, long a3) {
    return nx_syscall6(nr, a1, a2, a3, 0, 0);
}