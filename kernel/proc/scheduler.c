/*
 * Notux OS — Process Scheduler + OOM Killer
 * kernel/proc/scheduler.c
 *
 * Simple round-robin preemptive scheduler.
 * Each process gets a configurable time slice (default 10 ms).
 * The OOM killer is invoked when physical memory falls below
 * OOM_THRESHOLD_PAGES; it picks the largest non-privileged process
 * and kills it.
 *
 * The scheduler is driven by the PIT/APIC timer interrupt (IRQ0).
 */

#include "scheduler.h"
#include "process.h"
#include "../arch/x86_64/syscall.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../drivers/gfx/font.h"
#define SIGKILL 9
#include "../kernel.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_PROCESSES       256
#define DEFAULT_TIMESLICE   10       /* timer ticks per slice   */
#define OOM_THRESHOLD_PAGES 512      /* ~2 MiB free triggers OOM */
#define KSTACK_BYTES        (16 * 4096)

/* ── Run queue ───────────────────────────────────────────────── */
static Process *run_queue[MAX_PROCESSES];
static int      proc_count    = 0;
static int      current_index = 0;
Process        *current_proc  = NULL;

/* ── Tick counter ────────────────────────────────────────────── */
static volatile uint64_t sched_ticks = 0;
static int               slice_remaining = DEFAULT_TIMESLICE;

/* ── Init ────────────────────────────────────────────────────── */
void sched_init(void) {
    proc_count    = 0;
    current_index = 0;
    current_proc  = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++)
        run_queue[i] = NULL;
}

/* ── Add process to run queue ────────────────────────────────── */
int sched_add(Process *proc) {
    if (proc_count >= MAX_PROCESSES) return -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!run_queue[i]) {
            run_queue[i] = proc;
            proc->state  = PROC_READY;
            proc_count++;
            return 0;
        }
    }
    return -1;
}

/* ── Remove process from run queue ──────────────────────────────*/
void sched_remove(Process *proc) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (run_queue[i] == proc) {
            run_queue[i] = NULL;
            proc_count--;
            if (current_index == i) current_index = 0;
            return;
        }
    }
}

/* ── OOM Killer ──────────────────────────────────────────────── */
static void oom_kill(void) {
    /* Find the process with the most allocated pages that is NOT
       privileged (kernel threads and init are exempt).          */
    Process *victim = NULL;
    uint64_t max_pages = 0;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        Process *p = run_queue[i];
        if (!p) continue;
        if (p->flags & PROC_FLAG_KERNEL)   continue;
        if (p->pid == 1)                   continue; /* init */
        if (p->mem_pages > max_pages) {
            max_pages = p->mem_pages;
            victim    = p;
        }
    }

    if (victim) {
        fb_puts("[OOM] Killing process: ");
        fb_puts(victim->name);
        fb_puts("\n");
        proc_kill(victim, SIGKILL);
    }
}

/* ── Timer tick — called from IRQ0 handler ───────────────────── */
void sched_tick(CpuState *state) {
    sched_ticks++;

    /* Check OOM condition */
    if (pmm_free_count() < OOM_THRESHOLD_PAGES)
        oom_kill();

    /* Decrement slice; switch if expired */
    if (--slice_remaining > 0) return;
    slice_remaining = DEFAULT_TIMESLICE;

    /* Save current process context */
    if (current_proc && current_proc->state == PROC_RUNNING) {
        current_proc->ctx = *state;   /* CpuState is the IRQ frame */
        current_proc->state = PROC_READY;
    }

    /* Next runnable process (round-robin) */
    int start = current_index;
    do {
        current_index = (current_index + 1) % MAX_PROCESSES;
        if (run_queue[current_index] &&
            run_queue[current_index]->state == PROC_READY) {
            break;
        }
    } while (current_index != start);

    Process *next = run_queue[current_index];
    if (!next || next->state != PROC_READY) {
        /* Nothing runnable — stay idle */
        return;
    }

    current_proc        = next;
    current_proc->state = PROC_RUNNING;

    /* Prime the ring-3 entry stack (syscall trampoline + TSS.RSP0) */
    syscall_set_kernel_stack(next->kernel_stack_phys + KSTACK_BYTES);

    /* Switch page tables if process has its own address space */
    if (current_proc->page_table)
        vmm_switch(current_proc->page_table);

    /* Restore context — iretq will pop the saved frame */
    *state = current_proc->ctx;
}

/* ── Idle loop (entered from kmain after everything is running) ─*/
__attribute__((noreturn))
void sched_idle(void) {
    for (;;) __asm__ volatile("hlt");
}

/* ── Yield (voluntary context switch) ───────────────────────── */
void sched_yield(void) {
    slice_remaining = 0;
    __asm__ volatile("int $0x20");  /* trigger scheduler tick */
}

/* ── Block current process ───────────────────────────────────── */
void sched_block(WaitReason reason) {
    if (current_proc) {
        current_proc->state       = PROC_BLOCKED;
        current_proc->wait_reason = reason;
    }
    sched_yield();
}

/* ── Unblock a process ───────────────────────────────────────── */
void sched_unblock(Process *proc) {
    if (proc && proc->state == PROC_BLOCKED)
        proc->state = PROC_READY;
}

/* ── Sleep for n milliseconds ────────────────────────────────── */
void sched_sleep_ms(uint32_t ms) {
    uint64_t wake_tick = sched_ticks + ms; /* 1 tick ≈ 1 ms */
    while (sched_ticks < wake_tick)
        sched_yield();
}

uint64_t sched_uptime_ms(void) { return sched_ticks; }
