#pragma once
#include <stdint.h>

/* Forward declaration of Process struct */
struct Process;
typedef struct Process Process;

/* Process table management */
Process *proc_find_by_pid(pid_t pid);
int proc_add_to_table(Process *p);
int proc_remove_from_table(Process *p);
uint32_t proc_get_count(void);
void proc_list_processes(void);
uint64_t proc_get_memory_usage(Process *p);
void proc_cleanup_with_table(Process *proc);

/* Enhanced process functions */
int proc_kill_pid_safe(int32_t pid, int signal);