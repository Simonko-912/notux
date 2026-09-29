#pragma once
#include <stdint.h>

/* Forward declaration of Process struct */
struct Process;
typedef struct Process Process;

/* Enhanced process functions */
int proc_kill_pid_safe(int32_t pid, int signal);
void proc_get_info_string(Process *p, char *buf, size_t buf_size);
int proc_has_permission(Process *p, uint32_t required_uid);

/* Process statistics */
typedef struct {
    uint32_t process_count;
    uint64_t total_memory;
    uint64_t max_memory;
    uint64_t avg_memory;
} ProcStats;

void proc_get_stats(ProcStats *stats);
void proc_list_detailed(void);