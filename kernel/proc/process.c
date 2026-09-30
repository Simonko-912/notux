/*
 * Notux OS — Process Management
 * kernel/proc/process.c
 *
 * Process creation, exec, exit, and cleanup.
 * Each user process gets:
 *   - A unique PID
 *   - Its own 4-level page table (kernel map shared, user regions private)
 *   - A 64 KiB kernel stack
 *   - A 256 KiB user stack with an argc/argv/envp frame
 */

#include "process.h"
#include "scheduler.h"
#include "elf_loader.h"
#include "../arch/x86_64/syscall.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../mm/heap.h"
#include "../fs/vfs.h"
#include "../kserial.h"
#include "../drivers/gfx/framebuffer.h"
#include "../fs/pathconv.h"
#include "../kernel.h"
#include <stddef.h>
#include <stdint.h>

#define KERNEL_STACK_PAGES 16    /* 64 KiB kernel stack per process */
/* Must be page-aligned and canonical: the mapped range is
 * [USER_STACK_TOP - PAGES*4096, USER_STACK_TOP). */
#define USER_STACK_TOP     0x00007FFFFFFFF000ULL
#define USER_STACK_PAGES   64    /* 256 KiB default user stack */

static pid_t next_pid = 1;

/* ── String helpers ──────────────────────────────────────────── */
static void basename_into(const char *path, char *out, size_t n) {
    const char *base = path;
    const char *p = path;
    while (*p) { if (*p == '/') base = p + 1; p++; }
    kstrncpy(out, base, n);
}

/* ── Create a kernel thread ──────────────────────────────────── */
Process *proc_create_kernel(const char *name, void (*entry)(void)) {
    Process *p = (Process *)kmalloc(sizeof(Process));
    if (!p) return NULL;
    kmemset(p, 0, sizeof(Process));

    p->pid        = next_pid++;
    p->ppid       = 0;
    p->state      = PROC_READY;
    p->flags      = PROC_FLAG_KERNEL;
    p->priority   = PRIO_NORMAL;
    p->mem_pages  = 0;
    p->mem_pages_max = UINT64_MAX;
    kstrncpy(p->name, name, PROC_NAME_MAX);

    uint64_t stack_phys = pmm_alloc_pages(KERNEL_STACK_PAGES);
    if (stack_phys == PMM_OOM) { kfree(p); return NULL; }
    p->kernel_stack_phys = stack_phys;
    p->mem_pages += KERNEL_STACK_PAGES;

    uint64_t stack_top = stack_phys + KERNEL_STACK_PAGES * 4096;
    p->ctx.rsp = stack_top;
    p->ctx.rip = (uint64_t)entry;
    p->ctx.rflags = 0x202;
    p->ctx.cs  = 0x08;
    p->ctx.ss  = 0x10;

    p->page_table = NULL;
    p->uid        = 0;
    for (int i = 0; i < PROC_MAX_FD; i++) p->fd[i] = -1;
    return p;
}

/* ── Build/user argc/argv/envp frame on the user stack ───────── */
static int build_user_stack(Process *p, uint64_t ustack_phys,
                            const char **argv, const char **envp) {
    int argc = 0;
    if (argv) while (argv[argc]) argc++;
    if (argc > 63) argc = 63;
    int envc = 0;
    if (envp) while (envp[envc]) envc++;
    if (envc > 63) envc = 63;

    /* phys_base backs the mapping; virt_base is what the user sees. */
    uint64_t phys_base = ustack_phys;
    uint64_t virt_base = USER_STACK_TOP - USER_STACK_PAGES * 4096;
    uint64_t top       = virt_base + USER_STACK_PAGES * 4096;

    /* Reserve + layout string area (written upward from low end) */
    uint64_t need = 16;
    for (int i = 0; i < argc; i++) need += kstrlen(argv[i]) + 1;
    for (int i = 0; i < envc; i++) need += kstrlen(envp[i]) + 1;

    /* String bytes are written at the PHYSICAL backing. */
    uint64_t string_phys = phys_base + (top - virt_base) - need;
    if (string_phys < phys_base) return -1;

    uint64_t argv_va[64], envp_va[64];
    uint64_t cur = string_phys;

    for (int i = 0; i < argc; i++) {
        size_t l = kstrlen(argv[i]) + 1;
        kmemcpy((void *)(uintptr_t)cur, argv[i], l);
        argv_va[i] = virt_base + (cur - phys_base);
        cur += l;
    }
    for (int i = 0; i < envc; i++) {
        size_t l = kstrlen(envp[i]) + 1;
        kmemcpy((void *)(uintptr_t)cur, envp[i], l);
        envp_va[i] = virt_base + (cur - phys_base);
        cur += l;
    }

    /* The vector is a VIRTUAL address; write it through the physical
     * backing, then hand the virtual address to the user context. */
    uint64_t rsp_va = top;
    uint64_t off    = rsp_va - virt_base;

    rsp_va -= 8 * (uint64_t)(envc + 1);
    uint64_t *envp_slots = (uint64_t *)(uintptr_t)(phys_base + (rsp_va - virt_base));
    envp_slots[envc] = 0;
    for (int i = 0; i < envc; i++) envp_slots[i] = envp_va[i];

    rsp_va -= 8 * (uint64_t)(argc + 1);
    uint64_t *argv_slots = (uint64_t *)(uintptr_t)(phys_base + (rsp_va - virt_base));
    argv_slots[argc] = 0;
    for (int i = 0; i < argc; i++) argv_slots[i] = argv_va[i];

    rsp_va -= 8;
    *(uint64_t *)(uintptr_t)(phys_base + (rsp_va - virt_base)) = (uint64_t)argc;
    (void)off;

    p->ctx.rsp = rsp_va;
    return 0;
}

/* ── Create a user process from an ELF binary ────────────────── */
Process *proc_create_user(const char *name, const char *path,
                          const char **argv, const char **envp,
                          uint32_t uid) {
    Process *p = (Process *)kmalloc(sizeof(Process));
    if (!p) return NULL;
    kmemset(p, 0, sizeof(Process));

    p->pid         = next_pid++;
    p->ppid        = current_proc ? current_proc->pid : 1;
    p->state       = PROC_READY;
    p->flags       = 0;
    p->priority    = PRIO_NORMAL;
    p->uid         = uid;
    p->mem_pages_max = 65536;
    if (name) kstrncpy(p->name, name, PROC_NAME_MAX);
    else      basename_into(path, p->name, PROC_NAME_MAX);
    for (int i = 0; i < PROC_MAX_FD; i++) p->fd[i] = -1;

    p->page_table = vmm_new_space();
    if (!p->page_table) { kfree(p); return NULL; }

    uint64_t kstack_phys = pmm_alloc_pages(KERNEL_STACK_PAGES);
    if (kstack_phys == PMM_OOM) goto fail;
    p->kernel_stack_phys = kstack_phys;
    p->mem_pages += KERNEL_STACK_PAGES;

    uint64_t ustack_phys = pmm_alloc_pages(USER_STACK_PAGES);
    if (ustack_phys == PMM_OOM) goto fail;
    uint64_t ustack_virt = USER_STACK_TOP - USER_STACK_PAGES * 4096;
    vmm_map_range(p->page_table, ustack_virt, ustack_phys,
                  USER_STACK_PAGES, VMM_FLAG_USER | VMM_FLAG_RW);
    p->mem_pages += USER_STACK_PAGES;

    uint64_t entry_point = 0;
    if (proc_load_elf(p, path, &entry_point) < 0) goto fail;

    if (build_user_stack(p, ustack_phys, argv, envp) < 0) goto fail;

    p->ctx.rip    = entry_point;
    p->ctx.rflags = 0x202;
    p->ctx.cs     = 0x1B;
    p->ctx.ss     = 0x23;

    if (current_proc)
        kstrncpy(p->cwd, current_proc->cwd, VFS_PATH_MAX);
    else
        kstrncpy(p->cwd, "#/", VFS_PATH_MAX);

    return p;

fail:
    if (p->page_table) vmm_free_space(p->page_table);
    kfree(p);
    return NULL;
}

/* ── Fork ────────────────────────────────────────────────────── */
Process *proc_fork(void) {
    Process *parent = current_proc;
    if (!parent) return NULL;

    Process *child = (Process *)kmalloc(sizeof(Process));
    if (!child) return NULL;
    kmemcpy(child, parent, sizeof(Process));

    child->pid   = next_pid++;
    child->ppid  = parent->pid;
    child->state = PROC_READY;

    child->page_table = vmm_clone_space(parent->page_table);
    if (!child->page_table) { kfree(child); return NULL; }

    uint64_t kstack = pmm_alloc_pages(KERNEL_STACK_PAGES);
    if (kstack == PMM_OOM) {
        vmm_free_space(child->page_table);
        kfree(child);
        return NULL;
    }
    child->kernel_stack_phys = kstack;

    child->ctx.rax = 0;

    sched_add(child);
    return child;
}

/* ── Exit ────────────────────────────────────────────────────── */
int proc_do_exit(int code) {
    if (current_proc) {
        /* Stay in the run queue as a zombie.  pick_next() only ever hands
         * back PROC_READY, so a zombie is never scheduled again, but the
         * parent can still find it in wait() and collect the status.  An
         * earlier sched_remove() here dropped the only reference, which is
         * why SYS_WAIT could never report anything. */
        current_proc->state       = PROC_ZOMBIE;
        current_proc->exit_signal = code;
        current_proc = NULL;
    }
    g_user_exit = 1;
    return 0;
}

/* ── Kill ────────────────────────────────────────────────────── */
void proc_kill(Process *proc, int signal) {
    if (!proc) return;
    proc->exit_signal = signal;
    proc->state       = PROC_ZOMBIE;
    /* Left for the parent to reap, exactly as proc_do_exit does.  The old
     * sched_remove() + proc_cleanup() freed the Process immediately, so a
     * parent still blocked in wait() had nothing left to find. */
    if (proc == current_proc) current_proc = NULL;
}

/* ── Cleanup ─────────────────────────────────────────────────── */
void proc_cleanup(Process *proc) {
    if (!proc) return;

    if (proc->page_table)
        vmm_free_space(proc->page_table);

    if (proc->kernel_stack_phys)
        pmm_free_pages(proc->kernel_stack_phys, KERNEL_STACK_PAGES);

    for (int i = 0; i < PROC_MAX_FD; i++) {
        if (proc->fd[i] >= 0)
            vfs_close(proc->fd[i]);
    }

    kfree(proc);
}

/* ── Spawn a user program (used by execve) ───────────────────── */
int proc_exec_path(const char *path, const char **argv, const char **envp) {
    if (!path) return -EINVAL;
    uint32_t uid = current_proc ? current_proc->uid : 0;

    Process *child = proc_create_user(NULL, path, argv, envp, uid);
    if (!child) return -ENOENT;
    sched_add(child);
    return (int)child->pid;
}

/* ── Init process ────────────────────────────────────────────── */
void proc_init_main(void) {
    const char *init_argv[] = { "init", NULL };
    Process *init = proc_create_user("init", "#/bin/init",
                                     init_argv, NULL, 0);
    if (init) {
        sched_add(init);
        fb_puts("[init] Init queued.\n");
    } else {
        fb_puts("[init] Failed to spawn #/bin/init.\n");
        fb_puts("[init] Boot complete.\n");
    }

    /* This kernel thread has done its one job: spawn the userspace init
     * and hand the machine over.  Block forever instead of spinning on
     * sched_yield(), which keeps it PROC_RUNNING/READY so a later timer
     * tick can pick it and try to resume a mid-function kernel frame the
     * scheduler has no way to restore.  Blocked is never picked. */
    sched_block(WAIT_CHILD);
}
