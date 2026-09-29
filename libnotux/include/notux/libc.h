/*
 * libnotux — Notux Standard Library
 * libnotux/include/notux/libc.h
 *
 * This is the primary header for user-space programs compiled
 * for Notux.  It wraps the kernel syscall ABI into a clean C API.
 *
 * Programs link against libnotux.a (static) or libnotux.so (shared).
 *
 * Compile with:
 *   gcc -Ilibnotux/include -lnotux myprog.c -o myprog
 */

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────
   MATH
   ───────────────────────────────────────────────────────────── */

int64_t  nx_abs(int64_t x);
double   nx_sqrt(double x);
double   nx_pow(double base, double exp);
double   nx_sin(double x);
double   nx_cos(double x);
double   nx_log(double x);
double   nx_log2(double x);
double   nx_floor(double x);
double   nx_ceil(double x);
int64_t  nx_min(int64_t a, int64_t b);
int64_t  nx_max(int64_t a, int64_t b);
uint64_t nx_rand(void);
void     nx_srand(uint64_t seed);

/* ─────────────────────────────────────────────────────────────
   MEMORY
   ───────────────────────────────────────────────────────────── */

void  *nx_malloc(size_t size);
void  *nx_calloc(size_t n, size_t size);
void  *nx_realloc(void *ptr, size_t new_size);
void   nx_free(void *ptr);
void  *nx_memcpy(void *dst, const void *src, size_t n);
void  *nx_memset(void *dst, int val, size_t n);
int    nx_memcmp(const void *a, const void *b, size_t n);

/* ─────────────────────────────────────────────────────────────
   STRING
   ───────────────────────────────────────────────────────────── */

size_t  nx_strlen(const char *s);
char   *nx_strcpy(char *dst, const char *src);
char   *nx_strncpy(char *dst, const char *src, size_t n);
char   *nx_strcat(char *dst, const char *src);
int     nx_strcmp(const char *a, const char *b);
int     nx_strncmp(const char *a, const char *b, size_t n);
char   *nx_strchr(const char *s, int c);
char   *nx_strstr(const char *hay, const char *needle);
char   *nx_strtok(char *s, const char *delim);
long    nx_strtol(const char *s, char **end, int base);
double  nx_strtod(const char *s, char **end);
double  nx_atof(const char *s);

int     nx_sprintf(char *buf, const char *fmt, ...);
int     nx_snprintf(char *buf, size_t n, const char *fmt, ...);
int     nx_vsprintf(char *buf, const char *fmt, va_list ap);

/* ─────────────────────────────────────────────────────────────
   I/O  (writes to stdout / current terminal)
   ───────────────────────────────────────────────────────────── */

int   nx_putchar(int c);
int   nx_puts(const char *s);
int   nx_printf(const char *fmt, ...);
int   nx_getchar(void);
char *nx_gets(char *buf, int n);       /* reads one line */

/*
 * Output with color:
 *   nx_cprintf(fg, bg, fmt, ...)
 *   Colors are 0xRRGGBB (0 = no change / transparent).
 */
int   nx_cprintf(uint32_t fg, uint32_t bg, const char *fmt, ...);

/* Convenience: make a packed color */
static inline uint32_t nx_color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

/* ─────────────────────────────────────────────────────────────
   FILE SYSTEM
   ───────────────────────────────────────────────────────────── */

#define NX_O_RDONLY  0x01
#define NX_O_WRONLY  0x02
#define NX_O_RDWR    0x03
#define NX_O_CREATE  0x04
#define NX_O_APPEND  0x08
#define NX_O_TRUNC   0x10

typedef struct {
    uint64_t size;
    uint32_t flags;       /* NX_FS_DIR, NX_FS_EXEC, etc. */
    uint64_t ctime;
    uint64_t mtime;
} NxFileInfo;

#define NX_FS_DIR   0x01
#define NX_FS_EXEC  0x02

int      nx_open(const char *path, int flags);
int      nx_close(int fd);
int64_t  nx_read(int fd, void *buf, size_t n);
int64_t  nx_write(int fd, const void *buf, size_t n);
int64_t  nx_seek(int fd, int64_t offset, int whence);
int      nx_stat(const char *path, NxFileInfo *info);
int      nx_mkdir(const char *path);
int      nx_unlink(const char *path);
int      nx_rename(const char *src, const char *dst);

typedef void *NxDir;
NxDir    nx_opendir(const char *path);
int      nx_readdir(NxDir dir, char *name_out, NxFileInfo *info_out);
void     nx_closedir(NxDir dir);

/* ─────────────────────────────────────────────────────────────
   PROCESS
   ───────────────────────────────────────────────────────────── */

typedef int32_t nx_pid_t;

nx_pid_t nx_getpid(void);
nx_pid_t nx_getppid(void);
uint32_t nx_getuid(void);
nx_pid_t nx_fork(void);
int      nx_exec(const char *path, const char **argv, const char **envp);
void     nx_exit(int code) __attribute__((noreturn));
nx_pid_t nx_wait(int *status_out);
int      nx_kill(nx_pid_t pid, int signal);
void     nx_sleep(uint32_t ms);
char    *nx_getenv(const char *name);
int      nx_setenv(const char *name, const char *value);
char    *nx_getcwd(char *buf, size_t n);
int      nx_chdir(const char *path);

/* ─────────────────────────────────────────────────────────────
   NETWORK
   ───────────────────────────────────────────────────────────── */

typedef int nx_sock_t;

#define NX_AF_INET  1
#define NX_SOCK_TCP 1
#define NX_SOCK_UDP 2

nx_sock_t nx_socket(int family, int type);
int       nx_connect(nx_sock_t s, uint32_t ip, uint16_t port);
int       nx_bind(nx_sock_t s, uint32_t ip, uint16_t port);
int       nx_listen(nx_sock_t s, int backlog);
nx_sock_t nx_accept(nx_sock_t s, uint32_t *client_ip, uint16_t *client_port);
int64_t   nx_send(nx_sock_t s, const void *buf, size_t n);
int64_t   nx_recv(nx_sock_t s, void *buf, size_t n);
void      nx_closesock(nx_sock_t s);
uint32_t  nx_resolve(const char *hostname);  /* DNS lookup */
int       nx_ping(uint32_t ip);              /* returns ms or -1 */

/* ─────────────────────────────────────────────────────────────
   TERMINAL / COLOR
   ───────────────────────────────────────────────────────────── */

void  nx_term_set_fg(uint32_t color);   /* 0xRRGGBB           */
void  nx_term_set_bg(uint32_t color);
void  nx_term_reset_color(void);
void  nx_term_clear(void);
void  nx_term_move(int row, int col);
int   nx_term_rows(void);
int   nx_term_cols(void);

/* Password input (disables echo) */
int   nx_term_read_password(char *buf, int max);

/* ─────────────────────────────────────────────────────────────
   TIME
   ───────────────────────────────────────────────────────────── */

typedef struct {
    uint16_t year;
    uint8_t  month, day;
    uint8_t  hour, minute, second;
} NxTime;

uint64_t nx_uptime_ms(void);
void     nx_gettime(NxTime *t);

/* ─────────────────────────────────────────────────────────────
   PACKAGE / LIBRARY LOADING
   ───────────────────────────────────────────────────────────── */

void *nx_dlopen(const char *path);
void *nx_dlsym(void *handle, const char *symbol);
void  nx_dlclose(void *handle);

#ifdef __cplusplus
}
#endif
