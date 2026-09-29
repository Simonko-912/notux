/*
 * libnotux — printf family
 */

#include <notux/libc.h>
#include <notux/syscalls.h>
#include <notux/terminal.h>
#include "nx.h"

typedef struct {
    char  *buf;
    size_t len;
    size_t used;
    int    to_stdout;
} Fmt;

static void fmt_putc(Fmt *f, char c) {
    if (f->to_stdout) {
        char ch = c;
        nx_syscall(SYS_WRITE, 1, (long)(uintptr_t)&ch, 1);
    } else if (f->used + 1 < f->len) {
        f->buf[f->used++] = c;
    }
}

static void fmt_puts(Fmt *f, const char *s) {
    if (!s) s = "(null)";
    while (*s) fmt_putc(f, *s++);
}

static void fmt_num(Fmt *f, uint64_t v, int base, int upper) {
    char tmp[24];
    int i = 0;
    if (v == 0) tmp[i++] = '0';
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    while (v) { tmp[i++] = dig[v % base]; v /= base; }
    while (i > 0) fmt_putc(f, tmp[--i]);
}

static void format(Fmt *f, const char *fmt, va_list ap) {
    while (*fmt) {
        if (*fmt != '%') { fmt_putc(f, *fmt++); continue; }
        fmt++;

        if (*fmt == '\0') break;

        /* width + '0' pad */
        int width = 0, pad_zero = 0;
        if (*fmt == '0') { pad_zero = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        int is_long = 0;
        while (*fmt == 'l') { is_long = 1; fmt++; }

        char spec = *fmt++;
        switch (spec) {
        case 'd':
        case 'i': {
            long long v = is_long ? va_arg(ap, long long) : va_arg(ap, int);
            if (v < 0) { fmt_putc(f, '-'); v = -v; }
            fmt_num(f, (uint64_t)v, 10, 0);
            break;
        }
        case 'u':
            fmt_num(f, is_long ? va_arg(ap, unsigned long long)
                               : (unsigned)va_arg(ap, unsigned int), 10, 0);
            break;
        case 'x':
            fmt_num(f, is_long ? va_arg(ap, unsigned long long)
                               : (unsigned)va_arg(ap, unsigned int), 16, 0);
            break;
        case 'X':
            fmt_num(f, is_long ? va_arg(ap, unsigned long long)
                               : (unsigned)va_arg(ap, unsigned int), 16, 1);
            break;
        case 'p':
            fmt_num(f, (uint64_t)(uintptr_t)va_arg(ap, void *), 16, 0);
            break;
        case 'c':
            fmt_putc(f, (char)va_arg(ap, int));
            break;
        case 's':
            fmt_puts(f, va_arg(ap, const char *));
            break;
        case '%':
            fmt_putc(f, '%');
            break;
        default:
            fmt_putc(f, '%');
            fmt_putc(f, spec);
            break;
        }
        (void)width; (void)pad_zero;
    }
}

int nx_vsprintf(char *buf, const char *fmt, va_list ap) {
    Fmt f = { buf, (size_t)-1, 0, 0 };
    format(&f, fmt, ap);
    if (f.used + 1 < f.len) f.buf[f.used] = '\0';
    return (int)f.used;
}

int nx_sprintf(char *buf, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = nx_vsprintf(buf, fmt, ap);
    va_end(ap);
    return r;
}

int nx_snprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    Fmt f = { buf, n, 0, 0 };
    format(&f, fmt, ap);
    if (f.used + 1 < n) f.buf[f.used] = '\0';
    va_end(ap);
    return (int)f.used;
}

int nx_printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    Fmt f = { NULL, 0, 0, 1 };
    format(&f, fmt, ap);
    va_end(ap);
    return (int)f.used;
}

int nx_cprintf(uint32_t fg, uint32_t bg, const char *fmt, ...) {
    char buf[4096];
    va_list ap; va_start(ap, fmt);
    Fmt f = { buf, sizeof(buf), 0, 0 };
    format(&f, fmt, ap);
    va_end(ap);
    buf[f.used] = '\0';

    if (fg) nx_syscall(SYS_TERM_SETFG, fg, 0, 0);
    if (bg) nx_syscall(SYS_TERM_SETBG, bg, 0, 0);
    int r = (int)nx_syscall(SYS_WRITE, 1, (long)(uintptr_t)buf, (long)f.used);
    if (fg) nx_syscall(SYS_TERM_SETFG, 0xFFFFFFFF, 0, 0);
    if (bg) nx_syscall(SYS_TERM_SETBG, 0xFF1E1E2E, 0, 0);
    return r;
}