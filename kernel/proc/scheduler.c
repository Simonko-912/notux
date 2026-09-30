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
#include "../arch/x86_64/gdt.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../drivers/gfx/font.h"
#include "../kserial.h"
#define SIGKILL 9
#include "../kernel.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_PROCESSES       256
#define DEFAULT_TIMESLICE   100      /* timer ticks per slice (~1 s at 100 Hz) */
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
        if (p->state != PROC_READY && p->state != PROC_RUNNING) continue;
        if (p->mem_pages > max_pages) {
            max_pages = p->mem_pages;
            victim    = p;
        }
    }

    if (victim) {
        char d[24];
        fb_puts("[OOM] Killing process: ");
        fb_puts(victim->name);
        fb_puts("\n");
        kser_puts("OOM: free=");
        num_to_str(pmm_free_count(), d, 10);
        kser_puts(d);
        kser_puts(" pages, killing pid=");
        num_to_str(victim->pid, d, 10);
        kser_puts(d);
        kser_puts(" ");
        kser_puts(victim->name);
        kser_puts(" mem_pages=");
        num_to_str(victim->mem_pages, d, 10);
        kser_puts(d);
        kser_puts("\n");
        proc_kill(victim, SIGKILL);
    }
}

/* ── Round-robin pick of the next runnable process ───────────── */
static Process *pick_next(void) {
    int start = current_index;
    do {
        current_index = (current_index + 1) % MAX_PROCESSES;
        if (run_queue[current_index] &&
            run_queue[current_index]->state == PROC_READY)
            return run_queue[current_index];
    } while (current_index != start);
    return NULL;
}

/* ── Reap one exited child of the calling process ────────────────
 * Exited and signalled processes stay in the run queue as zombies so
 * that this can find them.  The Process is freed here, which is the
 * only point at which its page table, kernel stack and fds go away. */
int sched_reap_child(int *status) {
    if (!current_proc) return -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        Process *p = run_queue[i];
        if (!p || p == current_proc) continue;
        if (p->ppid != current_proc->pid) continue;
        if (p->state != PROC_ZOMBIE) continue;
        if (status) *status = p->exit_signal;
        int pid = p->pid;
        sched_remove(p);
        proc_cleanup(p);
        return pid;
    }
    return -1;                       /* nothing to reap yet */
}

/* Point the SYSCALL trampoline and the TSS at a process's kernel stack.
 * They must agree: SYSCALL reads the stack top out of gs:0, while an
 * interrupt arriving from ring 3 lands on TSS.RSP0.  Leaving RSP0 at
 * the boot stack means the two disagree and IRQs clobber whatever
 * happens to be there. */
static void arm_kernel_stack(Process *p) {
    uint64_t top = p->kernel_stack_phys + KSTACK_BYTES;
    syscall_set_kernel_stack(top);
    gdt_set_kernel_stack(top);
}

static void __attribute__((noreturn)) enter_process(Process *next);
static void sched_reschedule(CpuState *state);

/* ── Is this ctx safe to iretq into? ─────────────────────────── */
static int ctx_sane(const Process *p) {
    uint64_t cs = p->ctx.cs;
    uint64_t rip = p->ctx.rip;

    /* Only the real kernel/user code selectors carry any meaning here:
     * 0x08 (kernel), 0x18/0x1B (user, RPL 0/3).  Anything else, notably a
     * shifted or LDT-bit value, is a torn field, not a selector. */
    if (cs != 0x08 && (cs & ~3ULL) != 0x18)
        return 0;

    /* RIP must be canonical: within the lower or upper half, not the
     * non-canonical void in between. */
    if (!(rip < 0x0000800000000000ULL || rip >= 0xFFFF800000000000ULL))
        return 0;
    return 1;
}

/* Same checks, but against a raw saved frame instead of a Process. */

static void bad_ctx_diag(const Process *next, uint64_t from) {
    char d[24];
    kser_puts("!BADCTX pid=");
    num_to_str(next->pid, d, 10); kser_puts(d);
    kser_puts(" rip=0x"); num_to_str(next->ctx.rip, d, 16); kser_puts(d);
    kser_puts(" cs=0x");   num_to_str(next->ctx.cs, d, 16);  kser_puts(d);
    kser_puts(" ss=0x");   num_to_str(next->ctx.ss, d, 16);  kser_puts(d);
    kser_puts(" rsp=0x");  num_to_str(next->ctx.rsp, d, 16); kser_puts(d);
    kser_puts(" fl=0x");   num_to_str(next->ctx.rflags, d, 16); kser_puts(d);
    kser_puts(" st=");     num_to_str(next->state, d, 10); kser_puts(d);
    kser_puts(" at-rip=0x"); num_to_str(from, d, 16); kser_puts(d);
    kser_puts("\n");
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

    sched_reschedule(state);
}

/* ── Give up the CPU ────────────────────────────────────────────
 * `state` is the frame the IRQ stub is about to iretq out of, or NULL when
 * the kernel re-entered the scheduler itself (a syscall that wants to let
 * someone else run) and there is therefore no frame to save or fill in. */
static void sched_reschedule(CpuState *state) {
    uint64_t fromcs = state ? state->cs : 0;

    /* A tick that lands while the CPU is in the kernel, running on behalf
     * of a ring-3 process, describes the interrupted *kernel* function --
     * its rip/cs/rflags are CPL-0 values, not the user thread's.  Saving
     * that over ctx and letting the scheduler "restore" it resumes the
     * process at a kernel address with a garbage CS.  The syscall return
     * path owns the switch in this case, so drop the tick entirely. */
    if (state && current_proc && ((current_proc->ctx.cs & 3) == 3) &&
        ((fromcs & 3) != 3))
        return;

    /* Save current process context */
    if (current_proc && current_proc->state == PROC_RUNNING) {
        if (state) {
            /* Keep the stack pointers from the last ring-3 snapshot: a
             * ring-0 shaped frame (three words) carries no meaningful
             * rsp/ss, and zeroing them strands the task on a null stack
             * at its next resume — the push-at-minus-eight fault. */
            uint64_t ursp = current_proc->ctx.rsp;
            uint64_t uss  = current_proc->ctx.ss;
            current_proc->ctx = *state;  /* CpuState is the IRQ frame */
            current_proc->ctx.rflags &= ~(1ULL << 8); /* never leak TF */
            if ((fromcs & 3) == 0) {
                current_proc->ctx.rsp = ursp;
                current_proc->ctx.ss  = uss;
            }
        }
        current_proc->state = PROC_READY;
    }

    Process *next = pick_next();
    if (!next) {
        /* Nothing runnable — stay idle */
        return;
    }

    /* Re-entered from the kernel with no frame to come back to: switching
     * to ourselves would rebuild our own context and never return to the
     * loop that called us, so only yield when there is someone else. */
    if (!state && next == current_proc) {
        current_proc->state = PROC_RUNNING;
        return;
    }

    /* Never build an iretq frame out of a torn ctx: a selector that is
     * not one of the kernel/user code selectors (with any RPL), or a
     * non-canonical RIP, means something has already stomped the process
     * record.  Refusing the switch keeps the corrupt frame off the stack
     * where iretq would turn it into a #GP -> triple fault, and the print
     * names the culprit.  (A data segment slot for `ss` is allowed to be
     * 0 for a ring-0 save.) */
    if (!ctx_sane(next)) {
        bad_ctx_diag(next, state ? state->rip : 0);
        return;
    }
    {
        /* Quiet like a stock Linux console: one short line when a pid first
         * takes the cpu, silence while the same task keeps bouncing along. */
        static int shown_pid = -1;
        char dm[24];
        if ((int)next->pid != shown_pid) {
            shown_pid = (int)next->pid;
            kser_puts("[sch] pid "); num_to_str(next->pid, dm, 10); kser_puts(dm);
            kser_puts("\n");
        }
    }

    /* Nothing to iretq back into when the kernel re-entered us, and a
     * switch in which the target's privilege differs from the interrupted
     * frame's must go to the exit path as well: the frame shape is
     * different (a ring-3 entry pushed five words, a ring-0 entry three),
     * and patching e.g. a ring-3 frame with a ring-0 target's rip/cs
     * leaves an iretq that pops an RSP/SS the ring-0 target never had.
     * enter_process builds a whole fresh frame instead. */
    /* Same reasoning covers a switch to another process: the hardware
     * frame the stub is about to iretq out of belongs to the interrupted
     * task — its rsp/ss describe that task's stack.  Dressing that frame
     * up for whoever we picked next resumes the new task against stack
     * state it never owned, so every cross-task hop goes through
     * enter_process, which lays down a complete frame of its own. */
    if (!state || next != current_proc ||
        ((fromcs & 3) != (next->ctx.cs & 3)))
        enter_process(next);                    /* never returns */

    current_proc        = next;
    current_proc->state = PROC_RUNNING;

    /* Prime the ring-3 entry stack (syscall trampoline + TSS.RSP0) */
    arm_kernel_stack(current_proc);

    /* Switch page tables if process has its own address space */
    if (current_proc->page_table)
        vmm_switch_to_process(current_proc->page_table);

    state->rip    = next->ctx.rip;
    state->cs     = next->ctx.cs;
    state->rflags = next->ctx.rflags & ~(1ULL << 8);
    if ((next->ctx.cs & 3) == 3) {
        state->rsp = next->ctx.rsp;
        state->ss  = next->ctx.ss;
    }
}

/* ── Switch into an already-selected process and never come back ──
 * `next` must be the process the caller already installed TSS.RSP0, the
 * GS syscall stack and (if it has one) the page table for.  Picking here
 * as well could pick a *different* process and leave the kernel stacks
 * and CR3 matching neither. */
static void __attribute__((noreturn))
enter_process(Process *next) {
    current_proc        = next;
    current_proc->state = PROC_RUNNING;
    arm_kernel_stack(current_proc);

    if (current_proc->page_table)
        vmm_switch_to_process(current_proc->page_table);

    /* Never carry TF into ring 3: it would raise #DB after the first
     * instruction of every syscall return. */
    current_proc->ctx.rflags &= ~(1ULL << 8);

    /* A torn ctx here would be iretq'd straight into a #GP.  Refuse and
     * stop rather than corrupt the machine further: the BADCTX line above
     * has already named the process. */
    if (!ctx_sane(current_proc)) {
        bad_ctx_diag(current_proc, 0);
        for (;;) __asm__ volatile("hlt");
    }

    /* Build the frame iretq expects, then jump into user space.
     *
     * A plain `uint64_t frame[5];` on the stack is NOT safe here.  This
     * function is entered through paths that never establish the SysV
     * "rsp is 16-byte aligned at entry" invariant (raw iretq, the
     * SYSCALL trampoline, and an int $0x20 re-entry), so gcc vectorises
     * the array stores with `movaps`, which demands a 16-byte-aligned
     * destination.  On a misaligned stack that raises #GP the instant we
     * try to enter a process.  Building the frame with plain `mov`
     * removes the alignment requirement entirely. */
    uint64_t f_rip = current_proc->ctx.rip;
    uint64_t f_cs  = current_proc->ctx.cs;
    uint64_t f_fl  = current_proc->ctx.rflags;
    uint64_t f_rsp = current_proc->ctx.rsp;
    uint64_t f_ss  = current_proc->ctx.ss;

    __asm__ volatile(
        "sub  $40, %%rsp\n\t"
        "mov  %0, 0(%%rsp)\n\t"    /* rip    */
        "mov  %1, 8(%%rsp)\n\t"    /* cs     */
        "mov  %2, 16(%%rsp)\n\t"   /* rflags */
        "mov  %3, 24(%%rsp)\n\t"   /* user rsp */
        "mov  %4, 32(%%rsp)\n\t"   /* user ss  */
        "iretq\n\t"
        :
        : "r"(f_rip), "r"(f_cs), "r"(f_fl), "r"(f_rsp), "r"(f_ss)
        : "memory");
    __builtin_unreachable();
}

/* ── Enter the next process after one has exited ────────────────
 * Reached from the SYSCALL exit trampoline, which must never come
 * back to user space.  Spinning on hlt there would keep the CPU at
 * CPL0, and the interrupt stubs only know how to unwind a ring-3
 * frame — a CPL0 interrupt pushes three quadwords where iretq pops
 * five, so the next tick tears the stack apart.  Pick the next
 * runnable process and enter it properly instead. */
__attribute__((noreturn))
void sched_enter_from_exit(void) {
    Process *next = pick_next();
    if (!next) {
        __asm__ volatile("cli");
        for (;;) __asm__ volatile("hlt");
    }
    enter_process(next);
}

/* ── Idle loop (entered from kmain after everything is running) ─*/
__attribute__((noreturn))
void sched_idle(void) {
    for (;;) __asm__ volatile("hlt");
}

/* ── Yield (voluntary context switch) ───────────────────────── */
void sched_yield(void) {
    slice_remaining = 0;
    /* Dispatch immediately.  Deferring to the next tick left the shell
     * unscheduled for long stretches when init parked in its wait loop,
     * and the prompt never appeared; yielding from a syscall has no IRQ
     * frame to hand back, which sched_reschedule(NULL) handles directly. */
    sched_reschedule(NULL);
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
