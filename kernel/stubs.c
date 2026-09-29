/*
 * Notux OS — kernel stubs
 * kernel/stubs.c
 *
 * Stub implementations for drivers/subsystems not yet written.
 * Replace each stub with a real implementation as you build them.
 */
#include "kernel.h"
#include "drivers/gfx/framebuffer.h"
#include "drivers/gfx/font.h"
#include <stdint.h>
#include <stddef.h>

/* ── Framebuffer ─────────────────────────────────────────────── */
static FramebufferInfo g_fb_stub;
void fb_init(FramebufferInfo *fb) { g_fb_stub = *fb; }
FramebufferInfo *fb_get(void) { return &g_fb_stub; }
void fb_put_pixel(int x, int y, uint32_t color) {
    (void)x; (void)y; (void)color;
    if (!g_fb_stub.base || !g_fb_stub.pitch) return;
    uint32_t *px = (uint32_t*)(uintptr_t)(g_fb_stub.base
                   + (uint32_t)y * g_fb_stub.pitch
                   + (uint32_t)x * 4);
    *px = color;
}
void fb_clear(void) {
    if (!g_fb_stub.base) return;
    uint32_t *p = (uint32_t*)(uintptr_t)g_fb_stub.base;
    uint32_t n = g_fb_stub.pitch * g_fb_stub.height / 4;
    for (uint32_t i = 0; i < n; i++) p[i] = 0xFF1E1E2Eu;
}

/* ── Network ─────────────────────────────────────────────────── */
void nic_init(void) {}

int  tcp_socket (int f, int t)                             { (void)f;(void)t; return -1; }
int  tcp_connect(int s, uint32_t ip, uint16_t port)        { (void)s;(void)ip;(void)port; return -1; }
int  tcp_bind   (int s, uint32_t ip, uint16_t port)        { (void)s;(void)ip;(void)port; return -1; }
int  tcp_listen (int s, int bl)                            { (void)s;(void)bl; return -1; }
int  tcp_accept (int s, uint32_t *ci, uint16_t *cp)        { (void)s;(void)ci;(void)cp; return -1; }
int64_t tcp_send(int s, const void *b, size_t n)           { (void)s;(void)b;(void)n; return -1; }
int64_t tcp_recv(int s, void *b, size_t n)                 { (void)s;(void)b;(void)n; return -1; }
void tcp_close  (int s)                                    { (void)s; }
uint32_t dns_resolve(const char *h)                        { (void)h; return 0; }
int  net_ping   (uint32_t ip)                              { (void)ip; return -1; }

/* ── IPC / Services ──────────────────────────────────────────── */
void ipc_init(void) {}
void svc_init(void) {}

/* ── GUI ─────────────────────────────────────────────────────── */
void gui_init(FramebufferInfo *fb) { (void)fb; }

/* ATA disk: real implementation in kernel/drivers/disk/ata.c */

/* ── RTC ─────────────────────────────────────────────────────── */
typedef struct { uint16_t year; uint8_t month,day,hour,minute,second; } NxTime;
void rtc_gettime(void *t) {
    NxTime *nt = (NxTime*)t;
    nt->year=2026; nt->month=1; nt->day=1;
    nt->hour=0; nt->minute=0; nt->second=0;
}

/* ── Process helpers (stubs until full proc implemented) ─────── */
int proc_kill_pid(int32_t pid, int signal){ (void)pid;(void)signal; return -1; }
uint64_t proc_brk(Process *p, uint64_t nb){ (void)p;(void)nb; return 0; }

/* ── proc_exec stub ──────────────────────────────────────────── */
int proc_exec(const char *path, const char **argv,
              const char **envp, uint32_t uid){
    (void)path;(void)argv;(void)envp;(void)uid; return -1;
}
const char *proc_getenv(const char *name){ (void)name; return 0; }
uint32_t    proc_getuid(void){ return 0; }
void        proc_exit(int code){ (void)code; for(;;)__asm__("hlt"); }

/* ── sysfont data referenced by font_load_builtin ────────────── */
/* These are provided by fonts/sysfont.c instead.
   font_load_builtin is NOT used — myfont_register() is used instead. */
