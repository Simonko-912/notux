#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../fs/vfs.h"

#define PROC_NAME_MAX  64
#define PROC_MAX_FD    64
#define PROC_FLAG_KERNEL 0x01

typedef enum { PROC_READY, PROC_RUNNING, PROC_BLOCKED, PROC_ZOMBIE } ProcState;
typedef enum { WAIT_NONE, WAIT_IO, WAIT_SLEEP, WAIT_CHILD } WaitReason;
typedef enum { PRIO_LOW=0, PRIO_NORMAL=1, PRIO_HIGH=2 } Priority;

/* pid_t definition - using nx_pid_t from libnotux */
typedef int32_t pid_t;

/* Saved register state (matches IRQ stack frame layout) */
typedef struct {
    uint64_t r15,r14,r13,r12,r11,r10,r9,r8;
    uint64_t rbp,rdi,rsi,rdx,rcx,rbx,rax;
    uint64_t vector, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} CpuState;

struct Process {
    int32_t   pid;
    int32_t   ppid;
    ProcState state;
    WaitReason wait_reason;
    uint32_t  flags;
    Priority  priority;
    uint32_t  uid;
    uint32_t  euid;
    char      name[PROC_NAME_MAX];
    char      cwd[VFS_PATH_MAX];
    CpuState  ctx;
    uint64_t *page_table;
    uint64_t  kernel_stack_phys;
    uint64_t  mem_pages;
    uint64_t  mem_pages_max;
    uint64_t  mmap_bump;
    uint64_t  brk;
    int       fd[PROC_MAX_FD];
    int       exit_signal;
    int       tty_id;        /* console this process talks to (fds 0-2) */
};

typedef struct Process Process;

Process *proc_create_kernel(const char *name, void (*entry)(void));
Process *proc_create_user(const char *name, const char *path,
                          const char **argv, const char **envp, uint32_t uid);
Process *proc_fork(void);
void     proc_kill(Process *proc, int signal);
int      proc_kill_pid(int32_t pid, int signal);
void     proc_cleanup(Process *proc);
void     proc_init_main(void);
int      proc_load_elf(Process *p, const char *path, uint64_t *entry_out);
int      proc_exec_path(const char *path, const char **argv, const char **envp);
int      proc_do_exit(int code);
uint64_t proc_brk(Process *p, uint64_t new_brk);
void     proc_exit(int code);

const char *proc_getenv(const char *name);
uint32_t    proc_getuid(void);
int         proc_exec(const char *path, const char **argv, const char **envp, uint32_t uid);

/* Enhanced process management */
Process *proc_find_by_pid(pid_t pid);
int proc_add_to_table(Process *p);
int proc_remove_from_table(Process *p);
uint32_t proc_get_count(void);
void proc_list_processes(void);
uint64_t proc_get_memory_usage(Process *p);
void proc_cleanup_with_table(Process *proc);

void rtc_gettime(void *t);
