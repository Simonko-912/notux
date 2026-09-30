/*
 * Notux OS — Kernel Entry Point
 * kernel/main.c
 */
#include "kernel.h"
#include "kserial.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/pit.h"
#include "arch/x86_64/syscall.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "proc/scheduler.h"
#include "proc/process.h"
#include "fs/vfs.h"
#include "fs/ntfs/ntfs.h"
#include "fs/rootmgr.h"
#include "drivers/gfx/framebuffer.h"
#include "drivers/gfx/font.h"
#include "drivers/input/ps2.h"
#include "drivers/net/nic.h"
#include "ipc/ipc.h"
#include "svc/services.h"
#include "../boot/boot_info.h"

__attribute__((noreturn))
void kpanic(const char *msg) {
    fb_set_color(0xFFFF5555, 0xFF1E1E2E);
    fb_puts("\nKERNEL PANIC: ");
    fb_puts(msg);
    fb_puts("\nSystem halted.\n");
    kser_puts("\nKERNEL PANIC: ");
    kser_puts(msg);
    kser_puts("\n");
    for (;;) __asm__ volatile("cli; hlt");
}

void kprintf(const char *fmt, ...) { (void)fmt; }

void kmain(BootInfo *bi) {

    /* 1. GDT / IDT */
    gdt_init();
    idt_init();

    /* 2. PMM */
    pmm_init(bi->mmap.map, bi->mmap.map_size, bi->mmap.desc_size);
    /* The kernel image outgrew the hard-coded 2 MiB reserved inside
     * pmm_init; claim the real extent or the heap lands on our BSS. */
    pmm_reserve(bi->kernel_phys, bi->kernel_size);
    /* Belt and braces: entry.asm already moved us onto this stack, and the
     * loader allocated it as EfiBootServicesData so the PMM skips it anyway.
     * Claim it explicitly so no future change to uefi_usable() can quietly
     * recycle the memory we are standing on. */
    if (bi->kstack_phys)
        pmm_reserve(bi->kstack_phys, bi->kstack_bytes);

    /* 3. VMM (identity-mapped for now) */
    vmm_init(bi->kernel_phys, bi->kernel_virt, bi->kernel_size);
    vmm_map_low_identity();

    /* gdt_init() ran before the PMM existed, so the TSS still has a zero
     * ist[] and rsp0.  Give the IST vectors (NMI, #DF) and ring-0 entries
     * real stacks now that we can allocate them. */
    gdt_setup_stacks();

    /* 4. Heap */
    kheap_init();

    /* 5. Serial first so we can debug everything that follows */
    kser_init();

    /* 6. Framebuffer + font */
    fb_init(&bi->fb);
    extern void myfont_register(void);
    myfont_register();

    klog("Notux " NOTUX_VERSION "\n");

    /* 7. PIC remap + PIT timer @ 100 Hz */
    klog("PIC/PIT init...\n");
    extern void pic_init(void);
    pic_init();
    pit_init();

    /* 8. PS/2 keyboard */
    klog("PS/2 init...\n");
    ps2_init();

    /* 9. USB drivers (if available) */
    klog("USB init...\n");
    #ifdef CONFIG_USB
    init_usb_drivers();
    #endif

    /* 10. VFS + NTFS */
    klog("VFS init...\n");
    vfs_init();
    ntfs_register();

    /* 10. Root partition manager — finds/installs Notux root NTFS */
    klog("Root manager...\n");
    rootmgr_init();

    /* 10b. First-boot: materialize user-space binaries into #/bin */
    klog("Installing user applications...\n");
    apps_install_blob();

    /* 11. Network */
    klog("NIC init...\n");
    nic_init();

    /* 12. Syscall (SYSCALL/SYSRET MSRs) */
    klog("Syscall init...\n");
    syscall_init();

    /* 13. IPC + services */
    klog("IPC/SVC init...\n");
    ipc_init();
    svc_init();

    /* 14. Scheduler */
    klog("Scheduler init...\n");
    sched_init();

    /* 15. Spawn init process */
    klog("Spawning init...\n");
    Process *init = proc_create_kernel("init", proc_init_main);
    if (!init) kpanic("failed to create init process");
    sched_add(init);
    klog("Init queued.\n");

    /* 16. Enable interrupts — PIT fires, scheduler takes over */
    klog("Enabling interrupts...\n");
    __asm__ volatile("sti");

    klog("Idle loop.\n");
    sched_idle();  /* never returns */
}
