/*
 * Notux OS — Full POSIX/Linux Porting Compatibility Header
 * libnotux/include/notux/compat.h
 *
 * Include this at the top of programs written for Linux/POSIX
 * to compile them for Notux with minimal or zero changes.
 *
 * Usage:
 *   #include <notux/compat.h>
 *   // Now use standard C / POSIX as normal.
 *   // open(), fopen(), malloc(), printf(), etc. all work.
 *   // Paths: /home/user → #/usr/user (auto by kernel)
 *   //        /tmp       → #/tmp
 *   //        /etc       → #/etc
 *   //        /bin       → #/bin
 *
 * Alternatively, include notux/compat_posix.h for a fuller mapping
 * that also brings in FILE*, scanf, etc.
 */
#pragma once

/* Pull in the POSIX compatibility layer */
#include <notux/compat_posix.h>

/* Pull in path utilities */
#include <notux/path.h>

/*
 * Porting guide — common issues and how Notux handles them:
 *
 * 1. PATHS
 *    The kernel translates all POSIX paths automatically at syscall time.
 *    /home/user  →  #/usr/user
 *    /tmp        →  #/tmp
 *    /etc        →  #/etc
 *    /bin        →  #/bin
 *    /lib        →  #/usr/lib
 *    /proc       →  #/sys/proc
 *    No code changes needed for most programs.
 *    Exception: hardcoded absolute paths in string literals that
 *    aren't passed to syscalls (e.g. help text) — change manually.
 *
 * 2. THREADS
 *    pthread_create → use nx_fork() or ignore (single-threaded works fine)
 *    Mutex/semaphore → IPC shared memory (nx_shmget)
 *
 * 3. SIGNALS
 *    SIGINT, SIGTERM, SIGKILL, SIGSEGV → supported
 *    Most others → stubbed (returns success, no action)
 *
 * 4. ENVIRONMENT
 *    getenv() → works (POSIX env stored in process table)
 *    Common vars: PATH, HOME, USER, SHELL, TERM
 *
 * 5. NETWORKING
 *    BSD sockets → fully supported (TCP/UDP)
 *    Use AF_INET, SOCK_STREAM, SOCK_DGRAM as normal
 *    gethostbyname() → mapped to nx_resolve()
 *
 * 6. FILE I/O
 *    fopen/fread/fwrite/fclose → fully supported
 *    All mode strings (r, w, a, rb, wb, r+, etc.) → supported
 *    stdin/stdout/stderr → connected to Notux terminal
 *
 * 7. DYNAMIC LINKING
 *    Not supported — use static linking (default with notux-gcc)
 *    All libraries must be linked at compile time
 *
 * 8. IOCTL / EPOLL / INOTIFY
 *    Not supported → return ENOSYS
 *    Replace with Notux equivalents if needed
 *
 * 9. /PROC /SYS FILESYSTEMS
 *    /proc/PID/maps etc. → not implemented
 *    /sys → basic stub at #/sys
 *
 * 10. BUILD
 *     notux-gcc myapp.c -lmusl_notux -o myapp
 *     OR: gcc -D__NOTUX__ -include notux/compat.h myapp.c -lnotux
 */

/* ── gethostbyname compat ────────────────────────────────────── */
#ifdef __NOTUX__
typedef struct {
    char    *h_name;
    char   **h_aliases;
    int      h_addrtype;
    int      h_length;
    char   **h_addr_list;
    char    *h_addr;
} struct_hostent;

static inline struct_hostent *gethostbyname(const char *name) {
    static struct_hostent he;
    static uint32_t       addr;
    static char *addr_list[2] = { (char*)&addr, NULL };
    addr = nx_resolve(name);
    if (!addr) return NULL;
    he.h_name      = (char*)name;
    he.h_aliases   = NULL;
    he.h_addrtype  = AF_INET;
    he.h_length    = 4;
    he.h_addr_list = addr_list;
    he.h_addr      = (char*)&addr;
    return &he;
}

/* ── htons / htonl ───────────────────────────────────────────── */
static inline uint16_t htons(uint16_t v) {
    return (uint16_t)((v << 8) | (v >> 8));
}
static inline uint32_t htonl(uint32_t v) {
    return ((v & 0xFF000000) >> 24) | ((v & 0x00FF0000) >>  8)
         | ((v & 0x0000FF00) <<  8) | ((v & 0x000000FF) << 24);
}
static inline uint16_t ntohs(uint16_t v) { return htons(v); }
static inline uint32_t ntohl(uint32_t v) { return htonl(v); }

/* ── inet_addr ───────────────────────────────────────────────── */
static inline uint32_t inet_addr(const char *s) {
    uint32_t a=0,b=0,c=0,d=0;
    /* parse "A.B.C.D" */
    const char *p = s;
    while (*p >= '0' && *p <= '9') { a = a*10 + (*p - '0'); p++; }
    if (*p++ != '.') return 0xFFFFFFFF;
    while (*p >= '0' && *p <= '9') { b = b*10 + (*p - '0'); p++; }
    if (*p++ != '.') return 0xFFFFFFFF;
    while (*p >= '0' && *p <= '9') { c = c*10 + (*p - '0'); p++; }
    if (*p++ != '.') return 0xFFFFFFFF;
    while (*p >= '0' && *p <= '9') { d = d*10 + (*p - '0'); p++; }
    return htonl((a<<24)|(b<<16)|(c<<8)|d);
}

/* ── INET_ADDRSTRLEN ─────────────────────────────────────────── */
#define INET_ADDRSTRLEN  16
#define INET6_ADDRSTRLEN 46

/* ── FILE* extras ────────────────────────────────────────────── */
static inline int fileno(FILE *f) { return f ? f->fd : -1; }

/* ── strdup / strndup ────────────────────────────────────────── */
static inline char *strdup(const char *s) {
    size_t n = nx_strlen(s) + 1;
    char *d = (char*)nx_malloc(n);
    if (d) nx_memcpy(d, s, n);
    return d;
}
static inline char *strndup(const char *s, size_t n) {
    size_t l = nx_strlen(s);
    if (l > n) l = n;
    char *d = (char*)nx_malloc(l + 1);
    if (d) { nx_memcpy(d, s, l); d[l] = '\0'; }
    return d;
}

/* ── setenv / unsetenv ───────────────────────────────────────── */
static inline int setenv(const char *n, const char *v, int overwrite) {
    (void)overwrite; return nx_setenv(n, v);
}
static inline int unsetenv(const char *n) {
    return nx_setenv(n, "");
}

/* ── basename / dirname (simple versions) ────────────────────── */
static inline char *basename(char *path) {
    if (!path || !*path) return (char*)".";
    char *p = path;
    char *last = path;
    while (*p) { if (*p == '/' || *p == '#') last = p+1; p++; }
    return *last ? last : path;
}
static inline char *dirname(char *path) {
    static char buf[1024];
    if (!path || !*path) return (char*)".";
    nx_strncpy(buf, path, sizeof(buf));
    char *p = buf + nx_strlen(buf) - 1;
    while (p > buf && (*p == '/' || *p == '#')) p--;
    while (p > buf && *p != '/' && *p != '#') p--;
    if (p == buf) return (char*)"/";
    *p = '\0';
    return buf;
}

#endif /* __NOTUX__ */
