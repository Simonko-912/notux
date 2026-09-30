/*
 * Notux OS — TTY layer
 * kernel/tty/tty.h
 *
 * Each tty is a text console with its own grid, cursor, colours, and input
 * queue.  Processes are attached to exactly one tty (default 0); writes to
 * the standard descriptors land on the attached tty, keys typed on the
 * visible console enter that console's queue.  Ctrl+Alt+F1..F4 switches the
 * visible console and repaints it from its grid, like Linux virtual
 * consoles — but there is only one physical screen, so non-visible ttys
 * keep their content until they become visible again.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define TTY_COUNT      4
#define TTY_ROWS_MAX   60
#define TTY_COLS_MAX   160
#define TTY_INQ_SIZE   256

/* Console id (0..TTY_COUNT-1) owning the calling process, or -1. */
int  tty_of_caller(void);
/* Copy up to n chars written to the process's console (fds 0-2 path). */
void tty_write_str(const char *s, size_t n);
/* Pull one line from the console's keyboard queue into buf (echo is done
 * by the input path, not here). Returns chars copied (>0), or -1 empty-ish
 * short read mirroring the old term_read behaviour. */
int64_t tty_read_line(char *buf, size_t n);
/* Make console idx visible right away (repaint from its grid). */
void tty_show(int idx);
/* Which console is visible. */
int  tty_visible(void);
/* Attach or detach a pid to a console (metadata for future per-console
 * foreground rules). Pass pid<=0 to detach whatever held idx before. */
void tty_attach_pid(int idx, int pid);
/* Push one decoded character from PS/2 or USB HID into the visible queue. */
void tty_got_char(char c);
/* Colour/cursor helpers backing the SYS_TERM_* syscalls, per-console. */
void tty_set_pens(uint32_t fg, uint32_t bg);   /* future chars on caller's tty */
void tty_model_clear(void);                    /* blank the caller's grid      */
void tty_goto(int col, int row);               /* move the cursor              */
int  tty_width(void);
int  tty_height(void);
/* Single-sided pen updates keep the other colour as-is (SYS_TERM_SETFG/BG). */
void tty_set_fg_only(uint32_t fg);
void tty_set_bg_only(uint32_t bg);
/* Pop one queued byte from the visible console (-1 when empty). */
int tty_pop_visible(void);
/* Seed console sizes/state right after graphics comes up, so nobody has to
 * worry about which context happens to trip the lazy first-use path. */
void tty_start(void);
