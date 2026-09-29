#include "syscall.h"
#include "gdt.h"
#include "../../proc/process.h"
#include "../../proc/scheduler.h"
#include "../../mm/vmm.h"
#include "../../mm/pmm.h"
#include "../../fs/vfs.h"
#include "../../drivers/gfx/font.h"
#include "../../drivers/input/ps2.h"
#include "../../drivers/net/tcp.h"
#include "../../kernel.h"
#include "../../fs/pathconv.h"
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
static void term_write(const void *buf, size_t n) {
    const char *p = (const char *)buf;
    for (size_t i = 0; i < n; i++) fb_putc(p[i]);
}

static int64_t term_read(void *buf, size_t n) {
    char *p = (char *)buf;
    size_t i = 0;
    while (i < n) {
        int c = ps2_getchar_block();
        if (c < 0) break;
        p[i++] = (char)c;
        if (c == '\n' || c == '\r') break;
    }
    return (int64_t)i;
}

int64_t syscall_handler(uint64_t nr,uint64_t a1,uint64_t a2,
                         uint64_t a3,uint64_t a4,uint64_t a5){
    (void)a4;(void)a5;
    if (nr == SYS_WRITE && g_sys_area.diag_count < 3) {
        char d[24];
        g_sys_area.diag_count++;
        kser_puts("sc: nr=1 rsp=0x"); num_to_str(a5, d, 16); kser_puts(d);
        kser_puts("\n");
    }
    switch(nr){
    case SYS_READ:
        if ((int)a1 == 0) return term_read((void*)a2,(size_t)a3);
        return vfs_read((int)a1,(void*)a2,(size_t)a3);
    case SYS_WRITE:
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
    case SYS_EXEC:
        if (!a1) return -EINVAL;
        return proc_exec_path((const char*)a1,(const char**)a2,(const char**)a3);
    case SYS_EXIT:    return proc_do_exit((int)a1);
    case SYS_WAIT:    return 0;
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
    case SYS_TERM_SETFG: fb_set_fg((uint32_t)a1); return 0;
    case SYS_TERM_SETBG: fb_set_bg((uint32_t)a1); return 0;
    case SYS_TERM_CLEAR: fb_clear(); return 0;
    case SYS_TERM_MOVE:  fb_set_cursor((int)a1,(int)a2); return 0;
    case SYS_TERM_ROWS:  return fb_get_rows();
    case SYS_TERM_COLS:  return fb_get_cols();
    case SYS_UPTIME:     return (int64_t)sched_uptime_ms();
    case SYS_GETTIME:    rtc_gettime((void*)a1); return 0;
    case SYS_GETENV:     return 0;           /* env not implemented yet */
    case SYS_SETENV:     return 0;
    default: return -ENOSYS;
    }
}

void syscall_init(void){
    uint64_t efer=rdmsr(MSR_EFER); efer|=1; wrmsr(MSR_EFER,efer);
    /* STAR layout for this GDT: SYSCALL CS=0x08 SS=0x10, SYSRET CS base=0x18 (+3 RPL). */
    wrmsr(MSR_STAR,  ((uint64_t)0x08 << 40) | ((uint64_t)0x1B << 32));
    wrmsr(MSR_LSTAR, (uint64_t)(uintptr_t)syscall_entry_asm);
    /* Mask IF/DF on entry, but keep IF set after SYSRET. */
    wrmsr(MSR_SFMASK,(1u<<9)|(1u<<10)|(1u<<8));
    wrmsr(MSR_GS_BASE, 0);
    wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)(uintptr_t)&g_sys_area);
    g_sys_area.kernel_stack = 0;
    g_user_exit = 0;
}
