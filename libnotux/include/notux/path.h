/*
 * Notux OS — Path Compatibility (userspace)
 * libnotux/include/notux/path.h
 *
 * Userspace path translation — mirrors the kernel pathconv.c rules.
 * Use these in apps for portability or just use standard POSIX paths
 * and the kernel syscall layer will translate them automatically.
 *
 * POSIX path  →  Notux path:
 *   /                → #/
 *   /home/alice      → #/usr/alice
 *   /home            → #/usr
 *   /tmp             → #/tmp
 *   /etc             → #/etc
 *   /bin /usr/bin    → #/bin
 *   /lib /usr/lib    → #/usr/lib
 *   /var             → #/var
 *   /proc            → #/sys/proc
 *   /dev             → #/dev
 *   /root            → #/usr/admin
 *   /opt             → #/usr/opt
 *   Any other /...   → #/...
 *   #/... (Notux)    → unchanged
 *   Relative         → unchanged
 *
 * The kernel automatically translates ALL paths passed to syscalls,
 * so calling open("/home/alice/file.txt") works identically to
 * calling open("#/usr/alice/file.txt").
 */
#pragma once
#include <stddef.h>
#include <notux/libc.h>

/*
 * Translate a path to Notux format.
 * Returns dst. dst must be NX_PATH_MAX bytes.
 */
#define NX_PATH_MAX 1024

static inline char *nx_path_to_notux(const char *src, char *dst) {
    if (!src || !dst) return dst;
    /* Already Notux */
    if (src[0] == '#') { nx_strncpy(dst, src, NX_PATH_MAX); return dst; }
    /* Relative — unchanged */
    if (src[0] != '/') { nx_strncpy(dst, src, NX_PATH_MAX); return dst; }
    /* Root only */
    if (src[1] == '\0') { nx_strncpy(dst, "#/", NX_PATH_MAX); return dst; }

    /* Table of prefix mappings */
    static const struct { const char *p; const char *n; } tbl[] = {
        { "/usr/local/bin",  "#/bin"        },
        { "/usr/local/lib",  "#/usr/lib"    },
        { "/usr/local",      "#/usr/local"  },
        { "/usr/bin",        "#/bin"        },
        { "/usr/sbin",       "#/bin"        },
        { "/usr/lib",        "#/usr/lib"    },
        { "/usr/share",      "#/usr/share"  },
        { "/usr/include",    "#/usr/include"},
        { "/home",           "#/usr"        },
        { "/root",           "#/usr/admin"  },
        { "/opt",            "#/usr/opt"    },
        { "/proc",           "#/sys/proc"   },
        { "/sys",            "#/sys"        },
        { "/bin",            "#/bin"        },
        { "/sbin",           "#/bin"        },
        { "/lib64",          "#/usr/lib"    },
        { "/lib",            "#/usr/lib"    },
        { "/etc",            "#/etc"        },
        { "/var",            "#/var"        },
        { "/tmp",            "#/tmp"        },
        { "/dev",            "#/dev"        },
        { "/mnt",            "#/mnt"        },
        { "/run",            "#/var/run"    },
        { NULL, NULL }
    };

    for (int i = 0; tbl[i].p; i++) {
        size_t plen = nx_strlen(tbl[i].p);
        if (nx_strncmp(src, tbl[i].p, plen) != 0) continue;
        char nxt = src[plen];
        if (nxt != '/' && nxt != '\0') continue;
        nx_snprintf(dst, NX_PATH_MAX, "%s%s", tbl[i].n, src + plen);
        return dst;
    }

    /* No rule: prepend # */
    nx_snprintf(dst, NX_PATH_MAX, "#%s", src);
    return dst;
}

static inline char *nx_path_to_posix(const char *src, char *dst) {
    if (!src || !dst) return dst;
    if (src[0] != '#') { nx_strncpy(dst, src, NX_PATH_MAX); return dst; }
    if (src[1] == '/' && src[2] == '\0') { nx_strncpy(dst, "/", NX_PATH_MAX); return dst; }

    static const struct { const char *n; const char *p; } tbl[] = {
        { "#/bin",        "/usr/bin"    },
        { "#/usr/lib",    "/usr/lib"    },
        { "#/usr/share",  "/usr/share"  },
        { "#/usr/include","#/usr/include"},
        { "#/usr/admin",  "/root"       },
        { "#/usr/opt",    "/opt"        },
        { "#/usr",        "/home"       },
        { "#/sys/proc",   "/proc"       },
        { "#/sys",        "/sys"        },
        { "#/etc",        "/etc"        },
        { "#/var/run",    "/run"        },
        { "#/var",        "/var"        },
        { "#/tmp",        "/tmp"        },
        { "#/dev",        "/dev"        },
        { "#/mnt",        "/mnt"        },
        { NULL, NULL }
    };

    for (int i = 0; tbl[i].n; i++) {
        size_t nlen = nx_strlen(tbl[i].n);
        if (nx_strncmp(src, tbl[i].n, nlen) != 0) continue;
        char nxt = src[nlen];
        if (nxt != '/' && nxt != '\0') continue;
        nx_snprintf(dst, NX_PATH_MAX, "%s%s", tbl[i].p, src + nlen);
        return dst;
    }

    /* Strip leading '#' */
    nx_strncpy(dst, src + 1, NX_PATH_MAX);
    return dst;
}

/*
 * Convenience macros — use these in ported apps.
 * The kernel auto-translates so these are OPTIONAL,
 * but useful for constructing path strings at build time.
 *
 *   NX_HOME("alice")         → "#/usr/alice"
 *   NX_BIN("gcc")            → "#/bin/gcc"
 *   NX_ETC("passwd")         → "#/etc/passwd"
 *   NX_TMP("myfile.tmp")     → "#/tmp/myfile.tmp"
 *   POSIX_COMPAT("/etc/X")   → kernel auto-maps to "#/etc/X"
 */
#define NX_HOME(user)          "#/usr/" user
#define NX_BIN(name)           "#/bin/" name
#define NX_ETC(name)           "#/etc/" name
#define NX_VAR(name)           "#/var/" name
#define NX_TMP(name)           "#/tmp/" name
#define NX_LIB(name)           "#/usr/lib/" name
#define NX_SHARE(name)         "#/usr/share/" name

/* POSIX-style path macros that auto-translate via kernel */
#define POSIX_HOME(u)          "/home/" u    /* → #/usr/u */
#define POSIX_ETC(n)           "/etc/" n     /* → #/etc/n */
#define POSIX_BIN(n)           "/bin/" n     /* → #/bin/n */
#define POSIX_TMP(n)           "/tmp/" n     /* → #/tmp/n */
