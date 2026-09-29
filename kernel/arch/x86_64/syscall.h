#pragma once
#include <stdint.h>
/* syscall numbers are defined in notux/syscalls.h, included via -I flags */
void    syscall_init(void);
int64_t syscall_handler(uint64_t nr, uint64_t a1, uint64_t a2,
                        uint64_t a3, uint64_t a4, uint64_t a5);

/* Per-CPU area used by the SYSCALL trampoline via swapgs.
 * KERNEL_GS_BASE points here so that:
 *   [gs:0] = current kernel stack top
 *   [gs:8] = scratch slot for the saved user RSP           */
typedef struct {
    uint64_t kernel_stack;
    uint64_t user_rsp_scratch;
    uint64_t user_rip_scratch;
    uint64_t kernel_rsp_scratch;
    uint64_t rip_copy;
    uint64_t diag_count;
} SysArea;

void    syscall_set_kernel_stack(uint64_t top);

/* Set by SYS_EXIT so the trampoline knows not to sysret back to user */
extern uint8_t g_user_exit;
