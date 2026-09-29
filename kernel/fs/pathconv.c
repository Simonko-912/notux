/*
 * Notux OS — Path Compatibility Layer
 * kernel/fs/pathconv.c
 */
#include "pathconv.h"
#include "vfs.h"        /* for VFS_PATH_MAX */
#include "../kernel.h"
#include <stddef.h>
#include <stdint.h>

/* ── Helpers ─────────────────────────────────────────────────── */
static int pstarts(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s != *prefix) return 0;
        s++; prefix++;
    }
    return 1;
}
static size_t plen(const char *s) { size_t n=0; while(s[n])n++; return n; }
static void pcat(char *dst, size_t *pos, size_t max,
                 const char *src) {
    while (*src && *pos < max-1) dst[(*pos)++] = *src++;
    dst[*pos] = '\0';
}
static void pcpy(char *dst, const char *src, size_t max) {
    size_t i;
    for(i=0;i<max-1&&src[i];i++) dst[i]=src[i];
    dst[i]='\0';
}

/* ── Path table ──────────────────────────────────────────────── */
/*
 * Each entry: { posix_prefix, notux_prefix, strip_posix_len }
 * The suffix after posix_prefix is appended to notux_prefix.
 *
 * Longer prefixes must come FIRST to take priority.
 */
typedef struct {
    const char *posix;   /* POSIX prefix to match                  */
    const char *notux;   /* Notux prefix to replace it with        */
} PathRule;

static const PathRule rules[] = {
    /* Most specific first */
    { "/usr/local/bin",   "#/bin"          },
    { "/usr/local/lib",   "#/usr/lib"      },
    { "/usr/local/share", "#/usr/share"    },
    { "/usr/local",       "#/usr/local"    },
    { "/usr/bin",         "#/bin"          },
    { "/usr/sbin",        "#/bin"          },
    { "/usr/lib",         "#/usr/lib"      },
    { "/usr/share",       "#/usr/share"    },
    { "/usr/include",     "#/usr/include"  },
    { "/home",            "#/usr"          },
    { "/root",            "#/usr/admin"    },
    { "/opt",             "#/usr/opt"      },
    { "/proc",            "#/sys/proc"     },
    { "/sys",             "#/sys"          },
    { "/bin",             "#/bin"          },
    { "/sbin",            "#/bin"          },
    { "/lib",             "#/usr/lib"      },
    { "/lib64",           "#/usr/lib"      },
    { "/etc",             "#/etc"          },
    { "/var",             "#/var"          },
    { "/tmp",             "#/tmp"          },
    { "/dev",             "#/dev"          },
    { "/mnt",             "#/mnt"          },
    { "/run",             "#/var/run"      },
    { "/srv",             "#/var/srv"      },
    { NULL, NULL }
};

/* ── Translate POSIX → Notux ─────────────────────────────────── */
char *path_to_notux(const char *src, char *dst, size_t dst_size) {
    if (!src || !dst || dst_size == 0) return dst;

    /* Already a Notux path */
    if (src[0] == '#') {
        pcpy(dst, src, dst_size);
        return dst;
    }

    /* Relative path — leave unchanged */
    if (src[0] != '/') {
        pcpy(dst, src, dst_size);
        return dst;
    }

    /* Root "/" alone */
    if (src[0] == '/' && src[1] == '\0') {
        pcpy(dst, "#/", dst_size);
        return dst;
    }

    /* Try each rule */
    for (int i = 0; rules[i].posix; i++) {
        size_t plen_rule = plen(rules[i].posix);
        if (!pstarts(src, rules[i].posix)) continue;
        /* Must be followed by '/' or end-of-string */
        char next = src[plen_rule];
        if (next != '/' && next != '\0') continue;

        size_t pos = 0;
        pcat(dst, &pos, dst_size, rules[i].notux);
        /* Append the suffix */
        const char *suffix = src + plen_rule;
        pcat(dst, &pos, dst_size, suffix);
        return dst;
    }

    /* No rule matched: prepend '#' to the path */
    if (dst_size < 2) { dst[0] = '\0'; return dst; }
    dst[0] = '#';
    pcpy(dst + 1, src, dst_size - 1);
    return dst;
}

/* ── Translate Notux → POSIX ─────────────────────────────────── */
char *path_to_posix(const char *src, char *dst, size_t dst_size) {
    if (!src || !dst || dst_size == 0) return dst;

    /* Not a Notux path */
    if (src[0] != '#') {
        pcpy(dst, src, dst_size);
        return dst;
    }

    /* "#/" → "/" */
    if (src[0] == '#' && src[1] == '/' && src[2] == '\0') {
        pcpy(dst, "/", dst_size);
        return dst;
    }

    /* Reverse-search rules */
    for (int i = 0; rules[i].notux; i++) {
        size_t nlen = plen(rules[i].notux);
        if (!pstarts(src, rules[i].notux)) continue;
        char next = src[nlen];
        if (next != '/' && next != '\0') continue;

        size_t pos = 0;
        pcat(dst, &pos, dst_size, rules[i].posix);
        pcat(dst, &pos, dst_size, src + nlen);
        return dst;
    }

    /* "#/..." → strip '#' */
    if (src[0] == '#') {
        pcpy(dst, src + 1, dst_size);
        return dst;
    }

    pcpy(dst, src, dst_size);
    return dst;
}

/* ── Syscall wrapper: normalise any path before passing to VFS ── */
/*
 * All VFS calls go through path_resolve() so user programs can
 * use either POSIX or Notux paths transparently.
 */
int vfs_open_compat(const char *path, int flags, int mode) {
    char buf[VFS_PATH_MAX];
    return vfs_open(path_to_notux(path, buf, VFS_PATH_MAX), flags, mode);
}
int vfs_stat_compat(const char *path, VfsFileInfo *info) {
    char buf[VFS_PATH_MAX];
    return vfs_stat(path_to_notux(path, buf, VFS_PATH_MAX), info);
}
int vfs_mkdir_compat(const char *path) {
    char buf[VFS_PATH_MAX];
    return vfs_mkdir(path_to_notux(path, buf, VFS_PATH_MAX));
}
int vfs_unlink_compat(const char *path) {
    char buf[VFS_PATH_MAX];
    return vfs_unlink(path_to_notux(path, buf, VFS_PATH_MAX));
}
int vfs_rename_compat(const char *src, const char *dst) {
    char s[VFS_PATH_MAX], d[VFS_PATH_MAX];
    return vfs_rename(
        path_to_notux(src, s, VFS_PATH_MAX),
        path_to_notux(dst, d, VFS_PATH_MAX));
}
void *vfs_opendir_compat(const char *path) {
    char buf[VFS_PATH_MAX];
    return vfs_opendir(path_to_notux(path, buf, VFS_PATH_MAX));
}
