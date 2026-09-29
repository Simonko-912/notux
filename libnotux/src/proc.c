/*
 * libnotux — process API
 */

#include <notux/libc.h>
#include <notux/syscalls.h>
#include "nx.h"

nx_pid_t nx_getpid(void)  { return (nx_pid_t)nx_syscall(SYS_GETPID, 0, 0, 0); }
nx_pid_t nx_getppid(void) { return (nx_pid_t)nx_syscall(SYS_GETPPID, 0, 0, 0); }
uint32_t nx_getuid(void)  { return (uint32_t)nx_syscall(SYS_GETUID, 0, 0, 0); }

nx_pid_t nx_fork(void) {
    return (nx_pid_t)nx_syscall(SYS_FORK, 0, 0, 0);
}

int nx_exec(const char *path, const char **argv, const char **envp) {
    return (int)nx_syscall(SYS_EXEC, (long)(uintptr_t)path,
                           (long)(uintptr_t)argv,
                           (long)(uintptr_t)envp);
}

void nx_exit(int code) {
    nx_syscall(SYS_EXIT, code, 0, 0);
    for (;;) __asm__ volatile("hlt");
}

nx_pid_t nx_wait(int *status_out) {
    (void)status_out;
    return (nx_pid_t)nx_syscall(SYS_WAIT, 0, 0, 0);
}

int nx_kill(nx_pid_t pid, int signal) {
    return (int)nx_syscall(SYS_KILL, pid, signal, 0);
}

void nx_sleep(uint32_t ms) {
    nx_syscall(SYS_SLEEP, ms, 0, 0);
}

char *nx_getenv(const char *name) {
    return (char *)nx_syscall(SYS_GETENV, (long)(uintptr_t)name, 0, 0);
}

int nx_setenv(const char *name, const char *value) {
    return (int)nx_syscall(SYS_SETENV, (long)(uintptr_t)name,
                           (long)(uintptr_t)value, 0);
}

char *nx_getcwd(char *buf, size_t n) {
    return (char *)nx_syscall(SYS_GETCWD, (long)(uintptr_t)buf, (long)n, 0);
}

int nx_chdir(const char *path) {
    return (int)nx_syscall(SYS_CHDIR, (long)(uintptr_t)path, 0, 0);
}