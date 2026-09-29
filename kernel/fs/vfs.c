/*
 * Notux OS — Virtual Filesystem Layer
 * kernel/fs/vfs.c
 */
#include "vfs.h"
#include "../mm/heap.h"
#include "../kernel.h"
#include <stdint.h>
#include <stddef.h>

#define VFS_MAX_MOUNTS  16
#define VFS_MAX_DRIVERS  8
#define VFS_MAX_OPEN   256

static VfsDriver *g_drivers[VFS_MAX_DRIVERS];
static int        g_driver_count = 0;

void vfs_register_driver(VfsDriver *drv) {
    if (g_driver_count < VFS_MAX_DRIVERS)
        g_drivers[g_driver_count++] = drv;
}

static VfsDriver *find_driver(const char *fstype) {
    for (int i = 0; i < g_driver_count; i++)
        if (kstrcmp(g_drivers[i]->name, fstype) == 0)
            return g_drivers[i];
    return NULL;
}

static VfsMount g_mounts[VFS_MAX_MOUNTS];

int vfs_mount(const char *device, const char *mountpoint, const char *fstype) {
    VfsDriver *drv = find_driver(fstype);
    if (!drv) return -ENODEV;
    int slot = -1;
    for (int i = 0; i < VFS_MAX_MOUNTS; i++)
        if (!g_mounts[i].in_use) { slot = i; break; }
    if (slot < 0) return -ENOMEM;
    void *fs_data = NULL;
    int r = drv->mount(device, &fs_data);
    if (r < 0) return r;
    kstrncpy(g_mounts[slot].mountpoint, mountpoint, VFS_PATH_MAX);
    kstrncpy(g_mounts[slot].device,     device,     128);
    g_mounts[slot].driver  = drv;
    g_mounts[slot].fs_data = fs_data;
    g_mounts[slot].in_use  = 1;
    return 0;
}

int vfs_umount(const char *mountpoint) {
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (g_mounts[i].in_use && kstrcmp(g_mounts[i].mountpoint, mountpoint) == 0) {
            g_mounts[i].driver->umount(g_mounts[i].fs_data);
            g_mounts[i].in_use = 0;
            return 0;
        }
    }
    return -ENOENT;
}

static VfsMount *resolve_mount(const char *path, const char **rel_out) {
    VfsMount *best = NULL;
    int best_len = 0;
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!g_mounts[i].in_use) continue;
        int mlen = (int)kstrlen(g_mounts[i].mountpoint);
        if (mlen > best_len &&
            kstrncmp(path, g_mounts[i].mountpoint, (size_t)mlen) == 0) {
            best = &g_mounts[i];
            best_len = mlen;
        }
    }
    if (best && rel_out) {
        const char *rel = path + best_len;
        if (*rel == '/') rel++;
        if (*rel == '\0') rel = ".";
        *rel_out = rel;
    }
    return best;
}

typedef struct {
    int        in_use;
    VfsMount  *mount;
    void      *file_data;
    int        flags;
    int64_t    pos;
} VfsFile;

static VfsFile g_files[VFS_MAX_OPEN];

static int alloc_fd(void) {
    for (int i = 3; i < VFS_MAX_OPEN; i++)
        if (!g_files[i].in_use) return i;
    return -EMFILE;
}

void vfs_init(void) {
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) g_mounts[i].in_use = 0;
    for (int i = 0; i < VFS_MAX_OPEN;   i++) g_files[i].in_use  = 0;
}

int vfs_open(const char *path, int flags, int mode) {
    const char *rel = NULL;
    VfsMount   *mnt = resolve_mount(path, &rel);
    if (!mnt) return -ENOENT;
    int fd = alloc_fd();
    if (fd < 0) return fd;
    void *fdata = NULL;
    int r = mnt->driver->open(mnt->fs_data, rel, flags, mode, &fdata);
    if (r < 0) return r;
    g_files[fd].in_use    = 1;
    g_files[fd].mount     = mnt;
    g_files[fd].file_data = fdata;
    g_files[fd].flags     = flags;
    g_files[fd].pos       = 0;
    return fd;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_OPEN || !g_files[fd].in_use) return -EBADF;
    g_files[fd].mount->driver->close(g_files[fd].file_data);
    g_files[fd].in_use = 0;
    return 0;
}

int64_t vfs_read(int fd, void *buf, size_t n) {
    if (fd < 0 || fd >= VFS_MAX_OPEN || !g_files[fd].in_use) return -EBADF;
    VfsFile *f = &g_files[fd];
    int64_t r = f->mount->driver->read(f->file_data, buf, n, f->pos);
    if (r > 0) f->pos += r;
    return r;
}

int64_t vfs_write(int fd, const void *buf, size_t n) {
    if (fd < 0 || fd >= VFS_MAX_OPEN || !g_files[fd].in_use) return -EBADF;
    VfsFile *f = &g_files[fd];
    int64_t r = f->mount->driver->write(f->file_data, buf, n, f->pos);
    if (r > 0) f->pos += r;
    return r;
}

int64_t vfs_seek(int fd, int64_t offset, int whence) {
    if (fd < 0 || fd >= VFS_MAX_OPEN || !g_files[fd].in_use) return -EBADF;
    VfsFile *f = &g_files[fd];
    VfsFileInfo info;
    f->mount->driver->stat_fd(f->file_data, &info);
    int64_t np;
    switch (whence) {
        case 0: np = offset; break;
        case 1: np = f->pos + offset; break;
        case 2: np = (int64_t)info.size + offset; break;
        default: return -EINVAL;
    }
    if (np < 0) return -EINVAL;
    f->pos = np;
    return np;
}

int vfs_stat(const char *path, VfsFileInfo *info) {
    const char *rel = NULL;
    VfsMount   *mnt = resolve_mount(path, &rel);
    if (!mnt) return -ENOENT;
    return mnt->driver->stat(mnt->fs_data, rel, info);
}

int vfs_mkdir(const char *path) {
    const char *rel = NULL;
    VfsMount   *mnt = resolve_mount(path, &rel);
    if (!mnt) return -ENOENT;
    return mnt->driver->mkdir(mnt->fs_data, rel);
}

int vfs_unlink(const char *path) {
    const char *rel = NULL;
    VfsMount   *mnt = resolve_mount(path, &rel);
    if (!mnt) return -ENOENT;
    return mnt->driver->unlink(mnt->fs_data, rel);
}

int vfs_rename(const char *src, const char *dst) {
    const char *sr = NULL, *dr = NULL;
    VfsMount   *sm = resolve_mount(src, &sr);
    VfsMount   *dm = resolve_mount(dst, &dr);
    if (!sm || !dm) return -ENOENT;
    if (sm != dm) return -EXDEV;
    return sm->driver->rename(sm->fs_data, sr, dr);
}

VfsDir *vfs_opendir(const char *path) {
    const char *rel = NULL;
    VfsMount   *mnt = resolve_mount(path, &rel);
    if (!mnt) return NULL;
    VfsDir *dir = (VfsDir *)kmalloc(sizeof(VfsDir));
    if (!dir) return NULL;
    dir->mount    = mnt;
    dir->dir_data = NULL;
    int r = mnt->driver->opendir(mnt->fs_data, rel, &dir->dir_data);
    if (r < 0) { kfree(dir); return NULL; }
    return dir;
}

int vfs_readdir(VfsDir *dir, VfsDirent *entry) {
    if (!dir) return -EBADF;
    return dir->mount->driver->readdir(dir->dir_data, entry);
}

void vfs_closedir(VfsDir *dir) {
    if (!dir) return;
    dir->mount->driver->closedir(dir->dir_data);
    kfree(dir);
}

int vfs_create(const char *path, int mode) {
    return vfs_open(path, VFS_O_WRONLY|VFS_O_CREATE|VFS_O_TRUNC, mode);
}

int vfs_touch(const char *path) {
    const char *rel = NULL;
    VfsMount   *mnt = resolve_mount(path, &rel);
    if (!mnt) return -ENOENT;
    if (mnt->driver->touch) return mnt->driver->touch(mnt->fs_data, rel);
    VfsFileInfo fi;
    if (mnt->driver->stat(mnt->fs_data, rel, &fi) < 0) {
        void *fdata = NULL;
        int r = mnt->driver->open(mnt->fs_data, rel,
                                   VFS_O_WRONLY|VFS_O_CREATE, 0, &fdata);
        if (r >= 0) mnt->driver->close(fdata);
        return r;
    }
    return 0;
}

/* String helpers */
size_t kstrlen(const char *s) { size_t n=0; while(s[n])n++; return n; }
int kstrcmp(const char *a,const char *b){
    while(*a&&*b&&*a==*b){a++;b++;}
    return (unsigned char)*a-(unsigned char)*b;
}
int kstrncmp(const char *a,const char *b,size_t n){
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return (unsigned char)*a-(unsigned char)*b;
}
char *kstrncpy(char *d,const char *s,size_t n){
    size_t i; for(i=0;i<n-1&&s[i];i++)d[i]=s[i]; d[i]='\0'; return d;
}
char *kstrchr(const char *s,int c){
    while(*s){if((unsigned char)*s==c)return(char*)s;s++;} return NULL;
}
