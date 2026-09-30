#pragma once
/* Userspace FS API - thin wrappers over libnotux syscalls */
#include <notux/libc.h>

/* These are aliases to the nx_* functions in libc.h */
#define fs_open(path,flags)         nx_open(path, flags)
#define fs_close(fd)                nx_close(fd)
#define fs_read(fd,buf,n)           nx_read(fd,buf,n)
#define fs_write(fd,buf,n)          nx_write(fd,buf,n)
#define fs_stat(path,info)          nx_stat(path,info)
#define fs_mkdir(path)              nx_mkdir(path)
#define fs_unlink(path)             nx_unlink(path)
#define fs_rename(src,dst)          nx_rename(src,dst)
#define fs_create(path,flags)       nx_open(path,(flags)|NX_O_CREATE)
#define fs_touch(path)              nx_open(path,NX_O_WRONLY|NX_O_CREATE)
#define fs_opendir(path)            nx_opendir(path)
#define fs_closedir(d)              nx_closedir(d)

#define FsInfo    NxFileInfo
#define FsDir     void
#define FsDirent  struct _FsDirent_t

#define FS_DIR    NX_FS_DIR
#define FS_EXEC   NX_FS_EXEC
#define FS_O_RDONLY NX_O_RDONLY
#define FS_O_WRONLY NX_O_WRONLY

struct _FsDirent_t {
    char     name[256];
    uint64_t size;
    uint32_t flags;
};

static inline int fs_readdir(void *dir, struct _FsDirent_t *entry) {
    NxFileInfo fi;
    int r = nx_readdir(dir, entry->name, &fi);
    if (r == 0) { entry->size = fi.size; entry->flags = fi.flags; }
    return r;
}
