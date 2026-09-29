/*
 * Notux OS — Process Utilities
 * kernel/proc/proc_utils.c
 *
 * Utility functions to support process management and app execution.
 */

#include "process.h"
#include "process_manager.h"
#include "proc_utils.h"
#include "../fs/vfs.h"
#include "../kernel.h"
#include "../drivers/gfx/font.h"
#include <stdint.h>
#include <stddef.h>

/* Enhanced kill function with process table cleanup */
int proc_kill_pid_safe(int32_t pid, int signal) {
    Process *proc = proc_find_by_pid(pid);
    if (!proc) return -1;

    /* Use the enhanced cleanup that also removes from table */
    proc_kill(proc, signal);
    return 0;
}

/* Check if a process has a specific permission */
int proc_has_permission(Process *p, uint32_t required_uid) {
    if (!p) return 0;

    /* Root can do anything */
    if (p->uid == 0) return 1;

    /* Same user or admin */
    return (p->uid == required_uid || required_uid == 0);
}

/* Get process info as string for debugging - simplified version without global access */
void proc_get_info_string(Process *p, char *buf, size_t buf_size) {
    if (!p || !buf || buf_size == 0) return;

    /* Clear buffer first */
    kmemset(buf, 0, buf_size);

    /* Build info string - simplified version */
    char pid_str[16];
    num_to_str((uint64_t)(uint32_t)p->pid, pid_str, 10);

    char mem_str[16];
    num_to_str(proc_get_memory_usage(p), mem_str, 10);

    /* Format: PID: 123 | Name: appname | Mem: 1024 bytes */
    const char *parts[] = {
        "PID: ", pid_str, " | Name: ", p->name,
        " | Mem: ", mem_str, " bytes", NULL
    };
    size_t used = 0;
    for (int i = 0; parts[i] && used + 1 < buf_size; i++) {
        const char *s = parts[i];
        while (*s && used + 1 < buf_size) buf[used++] = *s++;
    }
    buf[used] = '\0';
}

/* Get system-wide process statistics - simplified version */
void proc_get_stats(ProcStats *stats) {
    if (!stats) return;

    kmemset(stats, 0, sizeof(ProcStats));

    /* This function would normally access the global process table,
     * but since we can't access it directly from this file,
     * we'll just return empty stats */
    // In a full implementation, this would iterate through process table
}

/* List all processes with detailed info - simplified version */
void proc_list_detailed(void) {
    /* This function would normally list all processes,
     * but since we can't access the global process table,
     * we'll just show a message */
    fb_puts("Detailed Process List:\n");
    fb_puts("=====================\n");
    fb_puts("Process listing requires access to global process table\n");
    fb_puts("This is a simplified implementation.\n");
}