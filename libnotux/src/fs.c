/*
 * libnotux — filesystem API
 */

#include <notux/libc.h>
#include <notux/syscalls.h>
#include "nx.h"

int nx_open(const char *path, int flags) {
    return (int)nx_syscall(SYS_OPEN, (long)(uintptr_t)path, flags, 0);
}

int nx_close(int fd) {
    return (int)nx_syscall(SYS_CLOSE, fd, 0, 0);
}

int64_t nx_read(int fd, void *buf, size_t n) {
    return nx_syscall(SYS_READ, fd, (long)(uintptr_t)buf, (long)n);
}

int64_t nx_write(int fd, const void *buf, size_t n) {
    return nx_syscall(SYS_WRITE, fd, (long)(uintptr_t)buf, (long)n);
}

int64_t nx_seek(int fd, int64_t offset, int whence) {
    return nx_syscall(SYS_SEEK, fd, (long)offset, whence);
}

int nx_stat(const char *path, NxFileInfo *info) {
    return (int)nx_syscall(SYS_STAT, (long)(uintptr_t)path,
                           (long)(uintptr_t)info, 0);
}

int nx_mkdir(const char *path) {
    return (int)nx_syscall(SYS_MKDIR, (long)(uintptr_t)path, 0, 0);
}

int nx_unlink(const char *path) {
    return (int)nx_syscall(SYS_UNLINK, (long)(uintptr_t)path, 0, 0);
}

int nx_rename(const char *src, const char *dst) {
    return (int)nx_syscall(SYS_RENAME, (long)(uintptr_t)src,
                           (long)(uintptr_t)dst, 0);
}

NxDir nx_opendir(const char *path) {
    return (NxDir)nx_syscall(SYS_OPENDIR, (long)(uintptr_t)path, 0, 0);
}

int nx_readdir(NxDir dir, char *name_out, NxFileInfo *info_out) {
    return (int)nx_syscall(SYS_READDIR, (long)(uintptr_t)dir,
                           (long)(uintptr_t)name_out,
                           (long)(uintptr_t)info_out);
}

void nx_closedir(NxDir dir) {
    nx_syscall(SYS_CLOSEDIR, (long)(uintptr_t)dir, 0, 0);
}