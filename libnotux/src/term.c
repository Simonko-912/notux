/*
 * libnotux — terminal API
 */

#include <notux/libc.h>
#include <notux/syscalls.h>
#include "nx.h"

void nx_term_set_fg(uint32_t color) { nx_syscall(SYS_TERM_SETFG, color, 0, 0); }
void nx_term_set_bg(uint32_t color) { nx_syscall(SYS_TERM_SETBG, color, 0, 0); }
void nx_term_clear(void)             { nx_syscall(SYS_TERM_CLEAR, 0, 0, 0); }
void nx_term_move(int row, int col)  { nx_syscall(SYS_TERM_MOVE, row, col, 0); }
int  nx_term_rows(void)              { return (int)nx_syscall(SYS_TERM_ROWS, 0, 0, 0); }
int  nx_term_cols(void)              { return (int)nx_syscall(SYS_TERM_COLS, 0, 0, 0); }

void nx_term_reset_color(void) {
    nx_syscall(SYS_TERM_SETFG, 0xFFFFFFFF, 0, 0);
    nx_syscall(SYS_TERM_SETBG, 0xFF1E1E2E, 0, 0);
}

int nx_term_read_password(char *buf, int max) {
    if (!buf || max <= 0) return -1;
    int i = 0;
    for (;;) {
        int c = nx_getchar();
        if (c < 0) break;
        if (c == '\r' || c == '\n') break;
        if (c == '\b') {
            if (i > 0) { i--; nx_putchar('\b'); }
            continue;
        }
        if (i < max - 1) {
            buf[i++] = (char)c;
            nx_putchar('*');
        }
    }
    buf[i] = '\0';
    return i;
}