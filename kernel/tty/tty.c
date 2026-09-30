/*
 * Notux OS — TTY layer implementation
 * kernel/tty/tty.c
 */
#include "tty.h"

#include "../drivers/gfx/framebuffer.h"
#include "../proc/process.h"
#include "../kserial.h"
#include "../kernel.h"

#define PEN_FG_DEFAULT 0xCDD6F4u
#define PEN_BG_DEFAULT 0x1E1E2Eu

#define GRID_CELLS (TTY_ROWS_MAX * TTY_COLS_MAX)

typedef struct {
    unsigned char ch[GRID_CELLS];       /* 0 = untouched blank cell     */
    uint32_t      fg[GRID_CELLS];
    uint32_t      bg[GRID_CELLS];
    volatile unsigned char in_q[TTY_INQ_SIZE]; /* keyboard queue           */
    volatile int ih, it;                /* queue head / tail              */
    int  cols, rows;                    /* live grid size                 */
    int  cx, cy;                        /* cursor                         */
    uint32_t pen_fg, pen_bg;            /* current colours                */
    int  fg_pid;                        /* informational holder of console */
} Console;

static Console con[TTY_COUNT];
static int     vis       = 0;            /* visible console index          */
static int     dims_done = 0;            /* sizes seeded once from fb      */

static Console *con_at(int i) {
    if (i < 0) i = 0;
    if (i >= TTY_COUNT) i = TTY_COUNT - 1;
    return &con[i];
}
static int con_of(const Console *c) { return (int)(c - con); }

/* Fill the grid with blank cells in the console's current colours. */
static void wipe(Console *c) {
    int n = c->cols * c->rows;
    for (int i = 0; i < n; i++) {
        c->ch[i] = 0;
        c->fg[i] = c->pen_fg;
        c->bg[i] = c->pen_bg;
    }
    c->cx = 0;
    c->cy = 0;
}
static void prepare(void) {
    if (dims_done) return;
    int cols = fb_get_cols();
    int rows = fb_get_rows();
    if (cols < 1 || cols > TTY_COLS_MAX) cols = 80;
    if (rows < 1 || rows > TTY_ROWS_MAX) rows = 25;
    for (int i = 0; i < TTY_COUNT; i++) {
        Console *c = &con[i];
        c->cols = cols;
        c->rows = rows;
        c->ih = c->it = 0;
        c->fg_pid = 0;
        c->pen_fg = PEN_FG_DEFAULT;
        c->pen_bg = PEN_BG_DEFAULT;
        wipe(c);
    }
    dims_done = 1;                             /* publish once fully filled */
}
static void echo_cell(Console *c, int x, int y) {
    int ix = y * c->cols + x;
    fb_set_color(c->fg[ix], c->bg[ix]);
    fb_set_cursor(x, y);
    fb_putc(c->ch[ix] ? (char)c->ch[ix] : ' ');
}
static void repaint(Console *c) {
    if (con_of(c) != vis) return;          /* hidden consoles stay in the grid */
    for (int y = 0; y < c->rows; y++)
        for (int x = 0; x < c->cols; x++)
            echo_cell(c, x, y);
}
static void scroll_up(Console *c) {
    for (int y = 0; y < c->rows - 1; y++) {
        int dst = y * c->cols, src = dst + c->cols;
        for (int x = 0; x < c->cols; x++) {
            c->ch[dst + x] = c->ch[src + x];
            c->fg[dst + x] = c->fg[src + x];
            c->bg[dst + x] = c->bg[src + x];
        }
    }
    int base = (c->rows - 1) * c->cols;
    for (int x = 0; x < c->cols; x++) {
        c->ch[base + x] = 0;
        c->fg[base + x] = c->pen_fg;
        c->bg[base + x] = c->pen_bg;
    }
    repaint(c);
}
/* Track one graphic character at the cursor and advance (no control logic).
 * Drawing itself is done by the fb primitives inside the syscall handler, so
 * the grid only mirrors what was laid down — it exists so a console that goes
 * invisible can be painted back exactly when it returns. */
static void lay_char(Console *c, unsigned char g) {
    int ix = c->cy * c->cols + c->cx;
    c->ch[ix] = g ? g : ' ';
    c->fg[ix] = c->pen_fg;
    c->bg[ix] = c->pen_bg;
    if (++c->cx >= c->cols) { c->cx = 0; c->cy++; }
}
/* One character through the cursor/wrap/scroll rules of this console. */
static void put_ch(Console *c, char ch) {
    if (ch == '\n')                                  { if (++c->cy >= c->rows) scroll_up(c); c->cx = 0; }
    else if (ch == '\r')                             { c->cx = 0; }
    else if (ch == '\t')                             { do lay_char(c, ' '); while (c->cx & 7); }
    else if (ch == '\b')                             { if (c->cx > 0) { c->cx--; int ix = c->cy * c->cols + c->cx; c->ch[ix] = 0; } }
    else                                             { lay_char(c, (unsigned char)ch); }
    if (c->cy >= c->rows) scroll_up(c);
    if (c->cy >= c->rows) c->cy = c->rows - 1;       /* clamp after scroll */
}
void tty_write_str(const char *s, size_t n) {
    prepare();
    Console *t = con_at(tty_of_caller());
    for (size_t i = 0; i < n; i++) put_ch(t, s[i]);
}
void tty_got_char(char c) {
    prepare();                                        /* visible queue only */
    Console *t = &con[vis];
    int next = (t->ih + 1) % TTY_INQ_SIZE;
    /* Queued only: the reader (libnotux nx_gets) does the echoing, matching
     * how typed text behaved before this layer existed. */
    if (next != t->it) { t->in_q[t->ih] = (char)c; t->ih = next; }
}
int64_t tty_read_line(char *buf, size_t n) {
    prepare();
    Console *t = con_at(tty_of_caller());
    size_t i = 0;
    while (i < n) {
        /* Drain whatever has arrived, then either hand it straight back or —
         * only while absolutely nothing has come yet — snooze for one key.
         * Dwelling in the syscall handler across many wakes invites ticks to
         * land mid-frame and skew the iretq unwind, so once bytes exist the
         * call returns promptly and readers just call again. */
        while (t->it != t->ih && i < n) {
            unsigned char ch = t->in_q[t->it];
            t->it = (t->it + 1) % TTY_INQ_SIZE;
            buf[i++] = (char)ch;
            if (ch == '\n' || ch == '\r') return (int64_t)i;   /* end of line */
        }
        if (i > 0) break;                              /* served what was there */
        /* Spin briefly (never hlt inside the SYSCALL handler: ticks landing
         * mid-transition skew the iretq unwind).  Keys queued before or just
         * after entry are picked up within this loop; a truly idle terminal
         * returns 0 quickly and the reader simply calls again. */
        int laps = 2000;
        while (laps-- > 0 && t->it == t->ih) __asm__ volatile("pause");
    }
    return (int64_t)i;
}
void tty_show(int idx) {
    prepare();
    vis = con_at(idx) - con;                          /* becomes the visible index */
    repaint(&con[vis]);
}
int tty_visible(void) { return vis; }
/* Every caller shares the visible console today; kept as a seam so future
 * per-console foreground rules can plug in without touching callers. */
int tty_of_caller(void) { return vis; }
void tty_attach_pid(int idx, int pid) { prepare(); con_at(idx)->fg_pid = pid; }
void tty_set_pens(uint32_t fg, uint32_t bg) {
    prepare();
    Console *t = con_at(tty_of_caller());
    t->pen_fg = fg;
    t->pen_bg = bg;                    /* applies to characters laid after */
}
/* Model-side companion to SYS_TERM_CLEAR: fb_clear() already painted the
 * screen, so only the grid bookkeeping has to follow. */
void tty_model_clear(void) { prepare(); wipe(con_at(tty_of_caller())); }
void tty_goto(int col, int row) {
    prepare();
    Console *t = con_at(tty_of_caller());
    if (col < 0) col = 0;
    if (row < 0) row = 0;
    if (col >= t->cols) col = t->cols - 1;
    if (row >= t->rows) row = t->rows - 1;
    t->cx = col;                       /* cursor only: no repaint of glyphs */
    t->cy = row;
}
int tty_width(void)  { prepare(); return con_at(tty_of_caller())->cols; }
int tty_height(void) { prepare(); return con_at(tty_of_caller())->rows; }
void tty_set_fg_only(uint32_t fg) { prepare(); con_at(tty_of_caller())->pen_fg = fg; }
void tty_set_bg_only(uint32_t bg) { prepare(); con_at(tty_of_caller())->pen_bg = bg; }
int tty_pop_visible(void) {
    Console *t = &con[vis];
    if (t->it == t->ih) return -1;
    unsigned char ch = t->in_q[t->it];
    t->it = (t->it + 1) % TTY_INQ_SIZE;
    return (int)ch;
}
void tty_start(void) {
    prepare();
    kser_puts("[tty] grid ");
    char dm[24];
    num_to_str(con[0].cols, dm, 10); kser_puts(dm);
    kser_puts("x");
    num_to_str(con[0].rows, dm, 10); kser_puts(dm);
    kser_puts("\n");
}
