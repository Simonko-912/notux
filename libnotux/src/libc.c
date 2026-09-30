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

/* Console identity helpers for the tty layer. */
int nx_gettty(void) { return (int)nx_syscall(SYS_GETTTY, 0, 0, 0); }
void nx_settty(int n) { nx_syscall(SYS_SETTTY, n, 0, 0); }

/* ── line editor shared bits for nx_gets ──────────────────────── */
#define NHIST 8
#define HIST_W 160
static char hpool[NHIST][HIST_W];
static int  hlen[NHIST];
static int  hn = 0;          /* entries currently stored        */
static int  hr = 0;          /* walk pointer into them          */

static void rewind_n(int cnt) { while (cnt-- > 0) nx_putchar('\b'); }
/* Put blanks then step back over them again. */
static void fwd_blanks(int nsp) { int i; for (i = 0; i < nsp; i++) nx_putchar(' '); rewind_n(nsp); }
/* Canonical repaint: assumes the cursor rests at column opos (the tracking
 * rule every edit path obeys), wipes the old line from column zero, writes the
 * new one and parks the cursor again — one predictable trail for all edits. */
static void render(const char *b, int olen, int opos, int nlen, int npos) {
    int i;
    rewind_n(opos);
    fwd_blanks(olen); rewind_n(olen);                 /* erase whole old line */
    for (i = 0; i < nlen; i++) nx_putchar(b[i]);
    rewind_n(nlen - npos);
}
/* Last submitted lines survive across prompts so Up/Down can recall them. */
static int hist_same(const char *a, const char *b, int len) {
    int i; for (i = 0; i < len; i++) if (a[i] != b[i]) return 0; return 1;
}
static void hist_store(const char *b, int len) {
    int i, dst, sl = len < HIST_W ? len : HIST_W - 1;
    if (!sl) return;                                   /* blank lines skipped */
    if (hn && hlen[hn - 1] == sl && hist_same(hpool[hn - 1], b, sl)) return; /* dup */
    if (hn < NHIST) { dst = hn++; }
    else { for (i = 1; i < NHIST; i++) { hlen[i - 1] = hlen[i]; for (int j = 0; j <= hlen[i]; j++) hpool[i - 1][j] = hpool[i][j]; } hn = NHIST; dst = NHIST - 1; }
    for (i = 0; i < sl; i++) hpool[dst][i] = b[i];
    hpool[dst][sl] = '\0'; hlen[dst] = sl; hr = hn;    /* browse starts freshest */
}
/* Load history slot si (clamped to fit dst[max-1]); returns chars copied. */
static int hist_load(int si, char *dst, int max) {
    int i, lim = hlen[si] < max - 1 ? hlen[si] : max - 1;
    for (i = 0; i < lim; i++) dst[i] = hpool[si][i];
    dst[i] = '\0'; return lim;
}
/* Walk history dir (-1 older / +1 newer); past the newest gives empty again. */
static int hist_step(int dir, char *buf, int n) {
    if (!hn) return 0;
    if (dir < 0) { if (hr > 0) hr--; } else if (hr < hn) hr++;
    if (hr >= hn) return 0;                            /* back at live line */
    return hist_load(hr, buf, n);
}
/* Line reader whose echo is purely voluntary: nx_gets repaints via putchar so
 * readers that want their typing visible show it themselves, while apps that
 * poll nx_getchar directly (games reading WASD etc.) just consume the queue —
 * typed keys never smear extra text onto whatever else shares the screen. The
 * tty layer hands arrows/Home/End/Page/Del over as short CSI sequences which
 * move the edit cursor or browse history here. */
char *nx_gets(char *buf, int n) {
    int len = 0, pos = 0, ol, op, i, c, nl;
    if (!buf || n <= 0) return NULL;
    buf[0] = '\0'; hr = hn;                             /* fresh browse window */
    for (;;) {
        c = nx_getchar();                                /* blocks until a key */
        if (c < 0) break;                                /* EOF-ish — hand back */ 
        if (c == '\r' || c == '\n') { hist_store(buf, len); nx_putchar('\n'); break; } 
        if (c == '\x1b') {                               /* CSI from tty_got_char() */ 
            int guard = 4, saw = 0; 
            while (guard-- > 0) { c = nx_getchar(); if (c < 0) break; if (c == '[') { saw = 1; break; } } 
            if (!saw) continue;                          /* odd burst — drop it */ 
            c = nx_getchar(); if (c < 0) break; 
            ol = len; op = pos;                          /* snapshot pre-edit */ 
            switch (c) {                                 /* CSI final bytes   */ 
            case 'A': len = hist_step(-1, buf, n); pos = len; break;              /* Up    */ 
            case 'B': len = hist_step(1, buf, n);  pos = len; break;              /* Down  */ 
            case 'C': if (pos < len) pos++; break;                                /* Right */ 
            case 'D': if (pos > 0) pos--; break;                                  /* Left  */ 
            case 'H': pos = 0; break;                                             /* Home  */ 
            case 'F': pos = len; break;                                            /* End   */ 
            case '3': nx_getchar();                 /* Del eats trailing '~' */ 
                      if (pos < len) { for (i = pos; i < len - 1; i++) buf[i] = buf[i + 1]; len--; } 
                      break;                                                        /*       */ 
            case '5': nx_getchar(); len = hist_step(-1, buf, n); pos = len; break;/*PgUp   */ 
            case '6': nx_getchar(); len = hist_step(1, buf, n);  pos = len; break;/*PgDn   */ 
            default: break;                              /* unknown final byte   */ 
            }                                           /* survives unharmed     */ 
            buf[len] = '\0'; 
            render(buf, ol, op, len, pos);              /* repaint after CSI too */ 
            continue; 
        } 
        ol = len; op = pos;  
        if (c == '\b') { if (pos > 0) { for (i = pos; i < len; i++) buf[i - 1] = buf[i]; len--; pos--; buf[len] = '\0'; } } 
        else if (c >= 0x20 && c < 0x7F) { if (len < n - 1) { for (i = len; i > pos; i--) buf[i] = buf[i - 1]; buf[pos++] = (char)c; len++; buf[len] = '\0'; } } 
        render(buf, ol, op, len, pos);                   /* keep screen matching buf */ 
    } 
    buf[len] = '\0';                                     /* EOF cut edit short */ 
    return buf; 
} 
