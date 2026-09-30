#include "syscall.h"
#include "gdt.h"
#include "../../proc/process.h"
#include "../../proc/scheduler.h"
#include "../../mm/vmm.h"
#include "../../mm/pmm.h"
#include "../../fs/vfs.h"
#include "../../drivers/gfx/font.h"
#include "../../drivers/input/ps2.h"
#include "../../tty/tty.h"
#include "../../drivers/net/tcp.h"
#include "../../kernel.h"
#include "../../fs/pathconv.h"
#include "../../kserial.h"
#include "../../../libnotux/include/notux/syscalls.h"
#include <stdint.h>

#define MSR_EFER          0xC0000080u
#define MSR_STAR          0xC0000081u
#define MSR_LSTAR         0xC0000082u
#define MSR_SFMASK        0xC0000084u
#define MSR_KERNEL_GS_BASE 0xC0000102u
#define MSR_GS_BASE       0xC0000100u

static SysArea g_sys_area;
uint8_t g_user_exit = 0;
/* R11 holds the user's RFLAGS at SYSCALL time, but R11 is caller-saved and
 * syscall_handler clobbers it.  The trampoline stashes the real value here so
 * the return path can restore the user's interrupt flag instead of garbage. */
volatile uint64_t g_user_rflags = 0x202;

static inline void wrmsr(uint32_t msr, uint64_t val) {
    __asm__ volatile("wrmsr"::"c"(msr),"a"((uint32_t)(val&0xFFFFFFFFu)),"d"((uint32_t)(val>>32)));
}
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo,hi;
    __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(msr));
    return ((uint64_t)hi<<32)|lo;
}

/* Defined in syscall_entry.asm */
extern void syscall_entry_asm(void);

/* Update the kernel stack used for ring-3 entry (syscall + interrupts) */
void syscall_set_kernel_stack(uint64_t top) {
    g_sys_area.kernel_stack = top;
    gdt_set_kernel_stack(top);
}

/* ── Terminal I/O for the standard descriptors ────────────────── */
/* Drawing stays on the direct framebuffer primitives — byte-for-byte the old
 * behaviour every app was tuned against.  The tty layer contributes the parts
 * the primitives never owned: named consoles, keyboard queues per console,
 * and the identity/plumbing around them (GETTTY / SETTTY / Ctrl+Alt+Fn). */
static void term_write(const void *buf, size_t n) {
    const char *p = (const char *)buf;
    tty_write_str(p, n);                     /* mirror into the console model */
    for (size_t i = 0; i < n; i++) { fb_putc(p[i]); kser_putc(p[i]); }
}
static int64_t term_read(void *buf, size_t n) {
    char *p = (char *)buf;
    size_t i = 0;
    while (i < n) {
        int c = ps2_getchar_block();         /* visible console's queue */
        if (c < 0) break;
        p[i++] = (char)c;
        if (c == '\n' || c == '\r') break;
    }
    return (int64_t)i;
}

int64_t syscall_handler(uint64_t nr,uint64_t a1,uint64_t a2,
                         uint64_t a3,uint64_t a4,uint64_t a5){
    (void)a4;
    (void)a5;
    switch(nr){
    case SYS_READ:
        if ((int)a1 == 0) return term_read((void*)a2,(size_t)a3);
        return vfs_read((int)a1,(void*)a2,(size_t)a3);
    case SYS_WRITE:
        /* libnotux calls nx_syscall(SYS_WRITE, fd, buf, n):
         * a1=fd, a2=buf, a3=n. a4/a5 are padding. */
        if ((int)a1 >= 0 && (int)a1 <= 2) { term_write((const void*)a2,(size_t)a3); return (int64_t)a3; }
        return vfs_write((int)a1,(const void*)a2,(size_t)a3);
    case SYS_OPEN:    return vfs_open_compat((const char*)a1,(int)a2,(int)a3);
    case SYS_CLOSE:   return vfs_close((int)a1);
    case SYS_SEEK:    return vfs_seek((int)a1,(int64_t)a2,(int)a3);
    case SYS_STAT:    return vfs_stat_compat((const char*)a1,(VfsFileInfo*)a2);
    case SYS_MKDIR:   return vfs_mkdir_compat((const char*)a1);
    case SYS_UNLINK:  return vfs_unlink_compat((const char*)a1);
    case SYS_RENAME:  return vfs_rename_compat((const char*)a1,(const char*)a2);
    case SYS_OPENDIR: return (int64_t)(uintptr_t)vfs_opendir_compat((const char*)a1);
    case SYS_READDIR: {
        VfsDirent e;
        int r = vfs_readdir((VfsDir*)a1, &e);
        if (r < 0) return r;
        kstrncpy((char*)a2, e.name, 256);
        if (a3) {
            VfsFileInfo *fi = (VfsFileInfo*)a3;
            fi->size  = e.size;
            fi->flags = e.flags;
        }
        return 0;
    }
    case SYS_CLOSEDIR: vfs_closedir((VfsDir*)a1); return 0;
    case SYS_GETCWD: {
        if(!current_proc)return -1;
        kstrncpy((char*)a1,current_proc->cwd,(size_t)a2);
        return (int64_t)a1;
    }
    case SYS_CHDIR: {
        if(!current_proc)return -1;
        VfsFileInfo fi;
        if(vfs_stat((const char*)a1,&fi)<0)return -ENOENT;
        if(!(fi.flags&VFS_DIR))return -ENOTDIR;
        kstrncpy(current_proc->cwd,(const char*)a1,VFS_PATH_MAX);
        return 0;
    }
    case SYS_GETPID:  return current_proc?(int64_t)current_proc->pid:1;
    case SYS_GETPPID: return current_proc?(int64_t)current_proc->ppid:0;
    case SYS_GETUID:  return current_proc?(int64_t)current_proc->uid:0;
    case SYS_FORK:    { Process *c=proc_fork(); return c?(int64_t)c->pid:-ENOMEM; }
    case SYS_EXEC: {
        if (!a1) return -EINVAL;
        return proc_exec_path((const char*)a1,(const char**)a2,(const char**)a3);
    }
    case SYS_EXIT:    return proc_do_exit((int)a1);
    case SYS_WAIT: {
        /* wait(4): a1 = &status.  Returns the reaped child's pid, or -1
         * when there is nothing to collect yet so the caller can sleep
         * and try again.  Returning 0 unconditionally made init believe a
         * child had exited and re-exec nsh in a tight loop. */
        int st = 0;
        int pid = sched_reap_child(&st);
        if (pid < 0) return -1;
        if (a1) *(volatile int *)(uintptr_t)a1 = st;
        return pid;
    }
    case SYS_KILL:    return proc_kill_pid((int32_t)a1,(int)a2);
    case SYS_SLEEP:   sched_sleep_ms((uint32_t)a1); return 0;
    case SYS_YIELD:   sched_yield(); return 0;
    case SYS_MMAP: {
        size_t pages=((size_t)a2+4095)/4096;
        uint64_t phys=pmm_alloc_pages((uint64_t)pages);
        if(phys==PMM_OOM)return -ENOMEM;
        if(!current_proc)return -1;
        uint64_t virt=current_proc->mmap_bump;
        if(!virt)virt=0x0000100000000000ULL;
        current_proc->mmap_bump=virt+pages*4096;
        vmm_map_range(current_proc->page_table,virt,phys,
                      (uint64_t)pages,VMM_FLAG_RW|VMM_FLAG_USER);
        current_proc->mem_pages+=pages;
        return (int64_t)virt;
    }
    case SYS_MUNMAP:  return 0;
    case SYS_BRK:     return current_proc?(int64_t)proc_brk(current_proc,a1):0;
    case SYS_SOCKET:  return tcp_socket((int)a1,(int)a2);
    case SYS_CONNECT: return tcp_connect((int)a1,(uint32_t)a2,(uint16_t)a3);
    case SYS_BIND:    return tcp_bind((int)a1,(uint32_t)a2,(uint16_t)a3);
    case SYS_LISTEN:  return tcp_listen((int)a1,(int)a2);
    case SYS_ACCEPT:  return tcp_accept((int)a1,(uint32_t*)a2,(uint16_t*)a3);
    case SYS_SEND:    return tcp_send((int)a1,(const void*)a2,(size_t)a3);
    case SYS_RECV:    return tcp_recv((int)a1,(void*)a2,(size_t)a3);
    case SYS_SOCKCLOSE: tcp_close((int)a1); return 0;
    case SYS_RESOLVE: return (int64_t)dns_resolve((const char*)a1);
    case SYS_PING:    return (int64_t)net_ping((uint32_t)a1);
    case SYS_TERM_SETFG: fb_set_fg((uint32_t)a1); tty_set_fg_only((uint32_t)a1); return 0;
    case SYS_TERM_SETBG: fb_set_bg((uint32_t)a1); tty_set_bg_only((uint32_t)a1); return 0;
    case SYS_TERM_CLEAR: fb_clear(); tty_model_clear(); return 0;
    case SYS_TERM_MOVE:  fb_set_cursor((int)a1,(int)a2); tty_goto((int)a1,(int)a2); return 0;
    case SYS_TERM_ROWS:  return fb_get_rows();
    case SYS_TERM_COLS:  return fb_get_cols();
    case SYS_GETTTY:     return tty_of_caller();
    case SYS_SETTTY: {                       /* opt another console visible */
        int idx = (int)a1;
        tty_show(idx);
        return idx;
    }
    case SYS_UPTIME:     return (int64_t)sched_uptime_ms();
    case SYS_GETTIME:    rtc_gettime((void*)a1); return 0;
    case SYS_GETENV:     return 0;           /* env not implemented yet */
    case SYS_SETENV:     return 0;
    default: return -ENOSYS;
    }
}

void syscall_init(void){
    uint64_t efer=rdmsr(MSR_EFER); efer|=1; wrmsr(MSR_EFER,efer);
    /* STAR layout (Intel): STAR[47:32] is the SYSCALL CS and SS is that
     * value +8; STAR[63:48] is the SYSRET CS base with the real CS being
     * that value +16.  The previous write had the two halves backwards:
     * SYSCALL then entered the kernel at CS=0x18 / SS=0x20 instead of
     * 0x08 / 0x10, so every syscall ran on selectors the ISR stubs and the
     * scheduler never recognise, and a timer tick mid-syscall iretq'd a
     * "CS=0x818" frame into a #GP.  Note the SYSRET CS here is 0x0B only
     * because Intel adds the +16 itself; the 0x1B user CS appears after
     * that adjustment (with its RPL set by the CPU). */
    wrmsr(MSR_STAR,  ((uint64_t)0x0B << 48) | ((uint64_t)0x08 << 32));
    wrmsr(MSR_LSTAR, (uint64_t)(uintptr_t)syscall_entry_asm);
    /* Mask IF/DF on entry, but keep IF set after SYSRET. */
    wrmsr(MSR_SFMASK,(1u<<9)|(1u<<10)|(1u<<8));
    wrmsr(MSR_GS_BASE, 0);
    wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)(uintptr_t)&g_sys_area);
    g_sys_area.kernel_stack = 0;
    g_user_exit = 0;

    /* Ensure the active kernel stack is visible to swapgs from ring 3. */
    syscall_set_kernel_stack((uint64_t)(uintptr_t)&g_sys_area + sizeof(g_sys_area));
}
