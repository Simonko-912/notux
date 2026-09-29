#pragma once
#include <stdint.h>
#include <stddef.h>

#define VFS_PATH_MAX  1024
#define PROC_MAX_FD     64
#define PROC_NAME_MAX   64

#define VFS_O_RDONLY  0x01
#define VFS_O_WRONLY  0x02
#define VFS_O_RDWR    0x03
#define VFS_O_CREATE  0x04
#define VFS_O_APPEND  0x08
#define VFS_O_TRUNC   0x10

#define VFS_DIR   0x01
#define VFS_EXEC  0x02
#define VFS_LINK  0x04

#define ENOENT   2
#define EACCES  13
#define EEXIST  17
#define ENOTDIR 20
#define EISDIR  21
#define EINVAL  22
#define ENOMEM  12
#define ENOSYS  38
#define ENOSPC  28
#define EBADF    9
#define EIO      5
#define EMFILE  24
#define ENODEV  19
#define EXDEV   18
#define EPERM    1

typedef struct {
    uint64_t size;
    uint32_t flags;
    uint64_t mtime;
    uint64_t ctime;
    uint32_t uid;
} VfsFileInfo;

typedef struct {
    char     name[256];
    uint64_t size;
    uint32_t flags;
} VfsDirent;

/* Forward declare VfsMount so VfsDriver can reference it */
typedef struct VfsMount VfsMount;

typedef struct VfsDriver {
    char name[16];
    int  (*mount)  (const char *device, void **fs_data_out);
    void (*umount) (void *fs_data);
    int     (*open)    (void *fs, const char *path, int flags, int mode, void **fdata_out);
    void    (*close)   (void *fdata);
    int64_t (*read)    (void *fdata, void *buf, size_t n, int64_t pos);
    int64_t (*write)   (void *fdata, const void *buf, size_t n, int64_t pos);
    int     (*stat)    (void *fs, const char *path, VfsFileInfo *info);
    int     (*stat_fd) (void *fdata, VfsFileInfo *info);
    int  (*opendir)  (void *fs, const char *path, void **dir_data_out);
    int  (*readdir)  (void *dir_data, VfsDirent *entry);
    void (*closedir) (void *dir_data);
    int  (*mkdir)    (void *fs, const char *path);
    int  (*unlink)   (void *fs, const char *path);
    int  (*rename)   (void *fs, const char *src, const char *dst);
    int  (*touch)    (void *fs, const char *path);
    int  (*chmod)    (void *fs, const char *path, uint32_t mode);
} VfsDriver;

/* Full VfsMount definition */
struct VfsMount {
    char       mountpoint[VFS_PATH_MAX];
    char       device[128];
    VfsDriver *driver;
    void      *fs_data;
    int        in_use;
};

typedef struct {
    VfsMount *mount;
    void     *dir_data;
} VfsDir;

void    vfs_init(void);
void    vfs_register_driver(VfsDriver *drv);
int     vfs_mount(const char *device, const char *mountpoint, const char *fstype);
int     vfs_umount(const char *mountpoint);
int     vfs_open(const char *path, int flags, int mode);
int     vfs_close(int fd);
int64_t vfs_read(int fd, void *buf, size_t n);
int64_t vfs_write(int fd, const void *buf, size_t n);
int64_t vfs_seek(int fd, int64_t offset, int whence);
int     vfs_stat(const char *path, VfsFileInfo *info);
int     vfs_mkdir(const char *path);
int     vfs_unlink(const char *path);
int     vfs_rename(const char *src, const char *dst);
int     vfs_create(const char *path, int mode);
int     vfs_touch(const char *path);
VfsDir *vfs_opendir(const char *path);
int     vfs_readdir(VfsDir *dir, VfsDirent *entry);
void    vfs_closedir(VfsDir *dir);

size_t kstrlen(const char *s);
int    kstrcmp(const char *a, const char *b);
int    kstrncmp(const char *a, const char *b, size_t n);
char  *kstrncpy(char *dst, const char *src, size_t n);
char  *kstrchr(const char *s, int c);

/* ── Path-compatible wrappers (auto-translate POSIX ↔ Notux) ── */
/* Defined in fs/pathconv.c — use these in syscall handlers       */
int   vfs_open_compat  (const char *path, int flags, int mode);
int   vfs_stat_compat  (const char *path, VfsFileInfo *info);
int   vfs_mkdir_compat (const char *path);
int   vfs_unlink_compat(const char *path);
int   vfs_rename_compat(const char *src, const char *dst);
void *vfs_opendir_compat(const char *path);
