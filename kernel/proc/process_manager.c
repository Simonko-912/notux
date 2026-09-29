/*
 * Notux OS — Enhanced Process Manager
 * kernel/proc/process_manager.c
 *
 * Additional process management functionality to enhance the app execution system.
 */

#include "process.h"
#include "scheduler.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../mm/heap.h"
#include "../fs/vfs.h"
#include "../fs/pathconv.h"
#include "../kernel.h"
#include "../drivers/gfx/font.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/* Process table for tracking running processes */
#define MAX_PROCESSES 256
static Process *g_process_table[MAX_PROCESSES];
static uint32_t g_process_count = 0;

/* Find a process by PID */
Process *proc_find_by_pid(pid_t pid) {
    if (pid <= 0 || pid >= MAX_PROCESSES) return NULL;

    for (int i = 0; i < g_process_count; i++) {
        if (g_process_table[i] && g_process_table[i]->pid == pid) {
            return g_process_table[i];
        }
    }
    return NULL;
}

/* Add process to table */
int proc_add_to_table(Process *p) {
    if (!p || g_process_count >= MAX_PROCESSES) return -1;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!g_process_table[i]) {
            g_process_table[i] = p;
            g_process_count++;
            return 0;
        }
    }
    return -1;
}

/* Remove process from table */
int proc_remove_from_table(Process *p) {
    if (!p) return -1;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (g_process_table[i] == p) {
            g_process_table[i] = NULL;
            g_process_count--;
            return 0;
        }
    }
    return -1;
}

/* Get process count */
uint32_t proc_get_count(void) {
    return g_process_count;
}

/* List all running processes */
void proc_list_processes(void) {
    fb_puts("Running processes:\n");

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (g_process_table[i]) {
            Process *p = g_process_table[i];
            char pid_str[16];
            num_to_str(p->pid, pid_str, 10);

            fb_puts("PID: ");
            fb_puts(pid_str);
            fb_puts(" | Name: ");
            fb_puts(p->name);
            fb_puts(" | State: ");

            switch (p->state) {
                case PROC_READY:   fb_puts("READY"); break;
                case PROC_RUNNING: fb_puts("RUNNING"); break;
                case PROC_BLOCKED: fb_puts("BLOCKED"); break;
                case PROC_ZOMBIE:  fb_puts("ZOMBIE"); break;
                default:           fb_puts("UNKNOWN"); break;
            }
            fb_putc('\n');
        }
    }
}

/* Get process memory usage */
uint64_t proc_get_memory_usage(Process *p) {
    if (!p) return 0;
    return p->mem_pages * 4096; // Pages to bytes
}

/* Process cleanup with table management */
void proc_cleanup_with_table(Process *proc) {
    if (!proc) return;

    /* Remove from process table first */
    proc_remove_from_table(proc);

    /* Call original cleanup */
    proc_cleanup(proc);
}