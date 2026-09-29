/*
 * libnotux — core libc: terminal I/O
 */

#include <notux/libc.h>
#include <notux/syscalls.h>
#include "nx.h"

int nx_putchar(int c) {
    char ch = (char)c;
    return (int)nx_syscall(SYS_WRITE, 1, (long)(uintptr_t)&ch, 1);
}

int nx_puts(const char *s) {
    if (!s) return 0;
    size_t n = nx_strlen(s);
    if (n && nx_syscall(SYS_WRITE, 1, (long)(uintptr_t)s, (long)n) < 0)
        return -1;
    return nx_putchar('\n');
}

int nx_getchar(void) {
    char ch;
    long r = nx_syscall(SYS_READ, 0, (long)(uintptr_t)&ch, 1);
    if (r <= 0) return -1;
    return (unsigned char)ch;
}

char *nx_gets(char *buf, int n) {
    if (!buf || n <= 0) return NULL;
    int i = 0;
    for (;;) {
        int c = nx_getchar();
        if (c < 0) break;
        if (c == '\r' || c == '\n') break;
        if (c == '\b') {
            if (i > 0) { i--; nx_putchar('\b'); }
            continue;
        }
        if (i < n - 1) buf[i++] = (char)c;
    }
    buf[i] = '\0';
    return buf;
}