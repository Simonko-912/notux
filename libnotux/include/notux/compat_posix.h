/*
 * Notux OS — POSIX Compatibility Layer
 * libnotux/include/notux/compat_posix.h
 *
 * ═══════════════════════════════════════════════════════════════
 *  PORTING EXISTING C PROGRAMS TO NOTUX
 * ═══════════════════════════════════════════════════════════════
 *
 * Notux provides three tiers of compatibility for existing code:
 *
 * TIER 1 — Source-level POSIX compat (this file)
 *   #include <notux/compat_posix.h>
 *   Maps standard POSIX names → Notux nx_* equivalents.
 *   Most single-file programs compile without changes.
 *
 * TIER 2 — musl-notux (recommended for larger projects)
 *   A port of musl libc that targets the Notux syscall ABI.
 *   Provides full <stdio.h>, <stdlib.h>, <pthread.h>, etc.
 *   Compile with:  notux-gcc -lmusl_notux myprog.c
 *
 * TIER 3 — SDL2-notux / libnotux-sdl
 *   Framebuffer-backed SDL2 port.  Games and graphical apps that
 *   use SDL2 compile and run as-is:
 *   notux-gcc $(sdl2-config --cflags --libs) game.c
 *
 * ── What gets translated ───────────────────────────────────────
 *   stdio:   fopen, fclose, fread, fwrite, fprintf, printf, fgets, …
 *   stdlib:  malloc, free, exit, getenv, system, …
 *   string:  strlen, strcpy, strcmp, memcpy, memset, …
 *   unistd:  read, write, open, close, fork, exec*, getcwd, …
 *   dirent:  opendir, readdir, closedir, …
 *   socket:  socket, connect, send, recv, …
 *   time:    time, gettimeofday, sleep, usleep, …
 *   signal:  signal, raise, kill (basic)
 *
 * ── What is NOT supported (yet) ───────────────────────────────
 *   mmap with file backing (anonymous mmap works)
 *   ioctl (use Notux-specific APIs)
 *   POSIX threads (pthreads — use nx_proc instead for now)
 *   Shared libraries (everything is statically linked)
 *   epoll / select (planned)
 */

#pragma once

/* Only pull in the compat layer when compiling for Notux */
#ifdef __NOTUX__

#include <notux/libc.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* ═══════════════════════════════════════════════════════════════
   ERRNO
   ═══════════════════════════════════════════════════════════════ */
extern int errno;
#define ENOENT    2
#define EACCES   13
#define EEXIST   17
#define ENOTDIR  20
#define EINVAL   22
#define ENOMEM   12
#define ENOSYS   38
#define EBADF     9
#define EIO       5
#define ENOTSUP  95

/* ═══════════════════════════════════════════════════════════════
   TYPES
   ═══════════════════════════════════════════════════════════════ */
typedef int64_t  ssize_t;
typedef uint32_t mode_t;
typedef int32_t  pid_t;
typedef int32_t  uid_t;
typedef int64_t  off_t;
typedef uint64_t ino_t;
typedef uint32_t dev_t;

/* ═══════════════════════════════════════════════════════════════
   FILE ABSTRACTION (stdio)
   ═══════════════════════════════════════════════════════════════ */

typedef struct _NxFILE {
    int      fd;
    int      flags;
    uint8_t  buf[4096];
    int      buf_pos;
    int      buf_len;
    int      eof;
    int      err;
} FILE;

extern FILE *__nx_stdin;
extern FILE *__nx_stdout;
extern FILE *__nx_stderr;

#define stdin  __nx_stdin
#define stdout __nx_stdout
#define stderr __nx_stderr

#define EOF (-1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define O_RDONLY NX_O_RDONLY
#define O_WRONLY NX_O_WRONLY
#define O_RDWR   NX_O_RDWR
#define O_CREAT  NX_O_CREATE
#define O_TRUNC  NX_O_TRUNC
#define O_APPEND NX_O_APPEND

FILE   *fopen(const char *path, const char *mode);
int     fclose(FILE *f);
size_t  fread(void *buf, size_t sz, size_t n, FILE *f);
size_t  fwrite(const void *buf, size_t sz, size_t n, FILE *f);
int     fseek(FILE *f, long offset, int whence);
long    ftell(FILE *f);
void    rewind(FILE *f);
int     feof(FILE *f);
int     ferror(FILE *f);
int     fflush(FILE *f);
char   *fgets(char *buf, int n, FILE *f);
int     fputs(const char *s, FILE *f);
int     fputc(int c, FILE *f);
int     fgetc(FILE *f);
int     getc(FILE *f);
int     putc(int c, FILE *f);
int     fprintf(FILE *f, const char *fmt, ...);
int     vfprintf(FILE *f, const char *fmt, va_list ap);
int     fscanf(FILE *f, const char *fmt, ...);

#define putchar(c)  fputc(c, stdout)
#define getchar()   fgetc(stdin)

/* ── printf / scanf family ───────────────────────────────────── */
static inline int printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    char buf[4096];
    int n = nx_vsprintf(buf, fmt, ap);
    va_end(ap);
    nx_puts(buf);
    return n;
}
static inline int sprintf(char *buf, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = nx_vsprintf(buf, fmt, ap);
    va_end(ap);
    return n;
}
static inline int snprintf(char *buf, size_t max, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = nx_vsprintf(buf, fmt, ap); /* TODO: honor max */
    va_end(ap);
    (void)max;
    return n;
}
int scanf(const char *fmt, ...);
int sscanf(const char *s, const char *fmt, ...);

/* ── File-system (unistd / fcntl style) ──────────────────────── */
static inline int open(const char *path, int flags, ...) {
    return nx_open(path, flags);
}
static inline int close(int fd)                    { return nx_close(fd); }
static inline ssize_t read(int fd, void *b, size_t n) { return nx_read(fd, b, n); }
static inline ssize_t write(int fd, const void *b, size_t n) { return nx_write(fd, b, n); }
static inline off_t lseek(int fd, off_t off, int w) { return (off_t)nx_seek(fd, off, w); }
static inline int unlink(const char *p)            { return nx_unlink(p); }
static inline int rename(const char *s, const char *d) { return nx_rename(s, d); }
static inline int mkdir(const char *p, mode_t m)   { (void)m; return nx_mkdir(p); }
static inline char *getcwd(char *buf, size_t n)    { return nx_getcwd(buf, n) ? buf : NULL; }
static inline int chdir(const char *p)             { return nx_chdir(p); }
static inline int access(const char *p, int m)     {
    NxFileInfo fi; (void)m;
    return nx_stat(p, &fi) == 0 ? 0 : -1;
}

/* ── stat ────────────────────────────────────────────────────── */
struct stat {
    dev_t   st_dev;
    ino_t   st_ino;
    mode_t  st_mode;
    uint32_t st_nlink;
    uid_t   st_uid;
    uid_t   st_gid;
    uint64_t st_size;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
};
#define S_ISDIR(m)  (((m) & 0xF000) == 0x4000)
#define S_ISREG(m)  (((m) & 0xF000) == 0x8000)
#define S_IRWXU 0700
#define S_IRUSR 0400

int stat(const char *path, struct stat *st);
int fstat(int fd, struct stat *st);

/* ── dirent ──────────────────────────────────────────────────── */
struct dirent {
    ino_t  d_ino;
    char   d_name[256];
    int    d_type;
};
#define DT_REG  8
#define DT_DIR  4

typedef void DIR;
static inline DIR *opendir(const char *p)  { return nx_opendir(p); }
static inline void closedir(DIR *d)        { nx_closedir(d); }
struct dirent *readdir(DIR *d);  /* implemented in compat_posix.c */

/* ═══════════════════════════════════════════════════════════════
   STDLIB
   ═══════════════════════════════════════════════════════════════ */
static inline void *malloc(size_t n)            { return nx_malloc(n); }
static inline void *calloc(size_t n, size_t s)  { return nx_calloc(n, s); }
static inline void *realloc(void *p, size_t n)  { return nx_realloc(p, n); }
static inline void  free(void *p)               { nx_free(p); }
static inline void  exit(int code)              { nx_exit(code); }
static inline char *getenv(const char *n)       { return nx_getenv(n); }
int    system(const char *cmd);
int    atexit(void (*func)(void));
double atof(const char *s);
int    atoi(const char *s);
long   atol(const char *s);
static inline long strtol(const char *s, char **e, int b) { return nx_strtol(s, e, b); }
static inline double strtod(const char *s, char **e)      { return nx_strtod(s, e); }

/* Random */
static inline int    rand(void)             { return (int)(nx_rand() & 0x7FFFFFFF); }
static inline void   srand(unsigned int s)  { nx_srand(s); }
#define RAND_MAX 0x7FFFFFFF

/* ═══════════════════════════════════════════════════════════════
   STRING
   ═══════════════════════════════════════════════════════════════ */
static inline size_t strlen(const char *s)                 { return nx_strlen(s); }
static inline char  *strcpy(char *d, const char *s)        { return nx_strcpy(d, s); }
static inline char  *strncpy(char *d, const char *s, size_t n) { return nx_strncpy(d, s, n); }
static inline char  *strcat(char *d, const char *s)        { return nx_strcat(d, s); }
static inline int    strcmp(const char *a, const char *b)  { return nx_strcmp(a, b); }
static inline int    strncmp(const char *a, const char *b, size_t n) { return nx_strncmp(a, b, n); }
static inline char  *strchr(const char *s, int c)          { return nx_strchr(s, c); }
static inline char  *strstr(const char *h, const char *n)  { return nx_strstr(h, n); }
static inline char  *strtok(char *s, const char *d)        { return nx_strtok(s, d); }
char *strtok_r(char *s, const char *d, char **save);
char *strdup(const char *s);
char *strndup(const char *s, size_t n);
int   strcasecmp(const char *a, const char *b);
int   strncasecmp(const char *a, const char *b, size_t n);
char *strrchr(const char *s, int c);
char *strerror(int errnum);

static inline void *memcpy(void *d, const void *s, size_t n)  { return nx_memcpy(d, s, n); }
static inline void *memset(void *d, int v, size_t n)           { return nx_memset(d, v, n); }
static inline int   memcmp(const void *a, const void *b, size_t n) { return nx_memcmp(a, b, n); }
void *memmove(void *dst, const void *src, size_t n);
void *memchr(const void *s, int c, size_t n);

/* ═══════════════════════════════════════════════════════════════
   MATH  (maps to notux built-in math)
   ═══════════════════════════════════════════════════════════════ */
static inline double sqrt(double x)       { return nx_sqrt(x); }
static inline double pow(double b, double e) { return nx_pow(b, e); }
static inline double sin(double x)        { return nx_sin(x); }
static inline double cos(double x)        { return nx_cos(x); }
static inline double log(double x)        { return nx_log(x); }
static inline double log2(double x)       { return nx_log2(x); }
static inline double floor(double x)      { return nx_floor(x); }
static inline double ceil(double x)       { return nx_ceil(x); }
static inline double fabs(double x)       { return x < 0 ? -x : x; }
#define M_PI   3.14159265358979323846
#define M_E    2.71828182845904523536

/* ═══════════════════════════════════════════════════════════════
   TIME
   ═══════════════════════════════════════════════════════════════ */
typedef uint64_t time_t;
typedef uint64_t clock_t;
#define CLOCKS_PER_SEC 1000

struct timeval { long tv_sec; long tv_usec; };
struct timespec { long tv_sec; long tv_nsec; };

static inline time_t time(time_t *t) {
    uint64_t ms = nx_uptime_ms();
    time_t s = (time_t)(ms / 1000);
    if (t) *t = s;
    return s;
}
static inline int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    uint64_t ms = nx_uptime_ms();
    if (tv) { tv->tv_sec = (long)(ms / 1000); tv->tv_usec = (long)(ms % 1000) * 1000; }
    return 0;
}
static inline unsigned int sleep(unsigned int s) { nx_sleep(s * 1000); return 0; }
static inline int usleep(unsigned long us)        { nx_sleep((uint32_t)(us / 1000)); return 0; }
struct tm *localtime(const time_t *t);
char *ctime(const time_t *t);

/* ═══════════════════════════════════════════════════════════════
   SIGNAL  (minimal)
   ═══════════════════════════════════════════════════════════════ */
#define SIGINT   2
#define SIGKILL  9
#define SIGSEGV 11
#define SIGTERM 15
#define SIGPIPE 13
typedef void (*sighandler_t)(int);
sighandler_t signal(int sig, sighandler_t handler);
static inline int raise(int sig) { return nx_kill(nx_getpid(), sig); }

/* ═══════════════════════════════════════════════════════════════
   ASSERT
   ═══════════════════════════════════════════════════════════════ */
#ifdef NDEBUG
  #define assert(x) ((void)(x))
#else
  #define assert(expr) \
    do { if (!(expr)) { \
        nx_printf("Assertion failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        nx_exit(1); \
    } } while (0)
#endif

/* ═══════════════════════════════════════════════════════════════
   NETWORK  (BSD sockets compat)
   ═══════════════════════════════════════════════════════════════ */
typedef int socklen_t;
struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t  sin_zero[8];
};
#define AF_INET   NX_AF_INET
#define SOCK_STREAM NX_SOCK_TCP
#define SOCK_DGRAM  NX_SOCK_UDP
#define INADDR_ANY 0

static inline int socket(int fam, int type, int proto) {
    (void)proto;
    return nx_socket(fam, type);
}
static inline int connect(int s, const struct sockaddr_in *addr, socklen_t l) {
    (void)l;
    return nx_connect(s, addr->sin_addr,
                      __builtin_bswap16(addr->sin_port));
}
static inline int bind(int s, const struct sockaddr_in *addr, socklen_t l) {
    (void)l;
    return nx_bind(s, addr->sin_addr,
                   __builtin_bswap16(addr->sin_port));
}
static inline int listen(int s, int bl)  { return nx_listen(s, bl); }
static inline ssize_t send(int s, const void *b, size_t n, int f) {
    (void)f; return nx_send(s, b, n);
}
static inline ssize_t recv(int s, void *b, size_t n, int f) {
    (void)f; return nx_recv(s, b, n);
}
uint16_t htons(uint16_t v);
uint32_t htonl(uint32_t v);
uint16_t ntohs(uint16_t v);
uint32_t ntohl(uint32_t v);
uint32_t inet_addr(const char *s);

#endif /* __NOTUX__ */
