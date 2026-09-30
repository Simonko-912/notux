/*
 * Notux OS — ISR Dispatcher
 * kernel/arch/x86_64/isr.c
 */
#include "isr.h"
#include "gdt.h"
#include "../../proc/scheduler.h"
#include "../../proc/process.h"
#include "../../drivers/gfx/font.h"
#include "../../drivers/input/ps2.h"
#include "../../kernel.h"
#include "../../kserial.h"
#include <stdint.h>

/* Signal numbers (from syscalls.h concepts) */
#define SIGSEGV 11
#define SIGILL   4
#define SIGFPE   8
#define SIGBUS   7

/* Ring-3 entry breadcrumbs repeat on every timer tick and keystroke, so
 * each vector only prints its first entry.  Exception vectors (<32) still
 * get their full dump below. */
static uint64_t g_seen_vectors = 0;

static const char *exception_names[22] = {
    "Divide Error","Debug","NMI","Breakpoint","Overflow","Bound Range",
    "Invalid Opcode","Device Not Available","Double Fault","Coprocessor Overrun",
    "Invalid TSS","Segment Not Present","Stack Fault","General Protection",
    "Page Fault","Reserved","x87 FPU Error","Alignment Check",
    "Machine Check","SIMD Exception","Virtualization","Control Protection"
};

#define PIC1_CMD  0x20
#define PIC2_CMD  0xA0
#define PIC_EOI   0x20

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));
}

void pic_init(void) {
    outb(0x20,0x11); outb(0xA0,0x11);
    outb(0x21,0x20); outb(0xA1,0x28);
    outb(0x21,0x04); outb(0xA1,0x02);
    outb(0x21,0x01); outb(0xA1,0x01);
    outb(0x21,0b11111100);
    outb(0xA1,0b11111111);
}

static void pic_eoi(int irq) {
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

/* simple hex print for page fault */
static void print_hex64(uint64_t v) {
    char buf[17]; buf[16]='\0';
    for(int i=15;i>=0;i--){
        int n=(int)(v&0xF);
        buf[i]=(char)(n<10?'0'+n:'A'+n-10);
        v>>=4;
    }
    fb_puts(buf);
}

static void handle_page_fault(CpuState *s) {
    uint64_t cr2;
    __asm__ volatile("mov %%cr2,%0":"=r"(cr2));
    fb_set_color(0xFFFF5555,0xFF1E1E2E);
    fb_puts("\n#PF Page Fault  CR2=0x");
    print_hex64(cr2);
    fb_puts(s->error_code&4?" user":" kernel");
    fb_puts(s->error_code&2?" write":" read");
    fb_puts(s->error_code&1?" prot":" not-present");
    fb_puts("\n");
    if (s->error_code & 4) {
        if (current_proc) {
            fb_puts("Killing: "); fb_puts(current_proc->name); fb_puts("\n");
            kser_puts("isr: user #PF killing "); kser_puts(current_proc->name); kser_puts("\n");
            proc_kill(current_proc, SIGSEGV);
            current_proc = NULL;
            sched_yield();
            return;
        }
    }
    fb_puts("KERNEL PAGE FAULT\n");
    char d[24];
    kser_puts("isr: KERNEL #PF cr2=0x");
    num_to_str(cr2, d, 16); kser_puts(d);
    kser_puts(" rip=0x");
    num_to_str(s->rip, d, 16); kser_puts(d);
    kser_puts(" err=0x");
    num_to_str(s->error_code, d, 16); kser_puts(d);
    kser_puts(" pid=");
    num_to_str(current_proc ? current_proc->pid : 0, d, 10); kser_puts(d);
    kser_puts("\n");
    for(;;) __asm__ volatile("cli;hlt");
}

void isr_dispatch(CpuState *s) {
    uint64_t vec = s->vector;
    /* A CPL-0 frame has no rsp/ss pushed, so those two words are stack
     * garbage.  Everything below reasons about "user vs kernel" from cs,
     * never from a ring-3 exception that happened to land in the kernel. */
    int from_user = (s->cs & 3) == 3;

    if (from_user && vec >= 32 && vec < 64 && !(g_seen_vectors & (1ULL << vec))) {
        g_seen_vectors |= (1ULL << vec);
        char d[24];
        kser_puts("R3 entry vec=");
        num_to_str(vec, d, 10); kser_puts(d);
        kser_puts(" cs=0x");
        num_to_str(s->cs, d, 16); kser_puts(d);
        kser_puts(" fl=0x");
        num_to_str(s->rflags, d, 16); kser_puts(d);
        kser_puts(" IF=");
        kser_putc((s->rflags & 0x200) ? '1' : '0');
        kser_puts(" err=0x");
        num_to_str(s->error_code, d, 16); kser_puts(d);
        kser_puts(" rsp=0x");
        num_to_str(s->rsp, d, 16); kser_puts(d);
        kser_puts(" rip=0x");
        num_to_str(s->rip, d, 16); kser_puts(d);
        kser_puts("\n");
    }

    if (vec < 32) {
        if (vec == 14) { handle_page_fault(s); return; }
        if (vec == 3)  return; /* breakpoint */
        const char *name = (vec < 22) ? exception_names[vec] : "Exception";
        fb_set_color(0xFFFF5555,0xFF1E1E2E);
        fb_puts("\nEXCEPTION: "); fb_puts(name); fb_puts("\n");
        if (from_user && current_proc && !(current_proc->flags & PROC_FLAG_KERNEL)) {
            char d[24];
            kser_puts("isr: user exception vec=");
            num_to_str(vec, d, 10); kser_puts(d);
            kser_puts(" rip=0x");
            num_to_str(s->rip, d, 16); kser_puts(d);
            kser_puts(" cs=0x");
            num_to_str(s->cs, d, 16); kser_puts(d);
            kser_puts(" rsp=0x");
            num_to_str(s->rsp, d, 16); kser_puts(d);
            kser_puts(" ss=0x");
            num_to_str(s->ss, d, 16); kser_puts(d);
            kser_puts(" rflags=0x");
            num_to_str(s->rflags, d, 16); kser_puts(d);
            kser_puts(" err=0x");
            num_to_str(s->error_code, d, 16); kser_puts(d);
            kser_puts(" killing ");
            kser_puts(current_proc->name); kser_puts("\n");
            proc_kill(current_proc, SIGILL);
            current_proc = NULL;
            sched_yield();
            return;
        }
        for(;;) {
            char d[24];
            kser_puts("isr: KERNEL EXCEPTION vec=");
            num_to_str(vec, d, 10); kser_puts(d);
            kser_puts(" rip=0x");
            num_to_str(s->rip, d, 16); kser_puts(d);
            kser_puts(" cs=0x");
            num_to_str(s->cs, d, 16); kser_puts(d);
            uint64_t cr2, cr3;
            __asm__ volatile("mov %%cr2,%0":"=r"(cr2));
            __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
            kser_puts(" cr2=0x"); num_to_str(cr2, d, 16); kser_puts(d);
            kser_puts(" cr3=0x"); num_to_str(cr3, d, 16); kser_puts(d);
            kser_puts(" rsp0=0x");
            num_to_str(gdt_tss_debug_rsp0(), d, 16); kser_puts(d);
            kser_puts(" pid=");
            num_to_str(current_proc ? current_proc->pid : 0, d, 10); kser_puts(d);
            /* The frame iretq would resume with; rsp/ss only exist on rings crossings. */
            kser_puts("\n  frame rip=0x"); kser_hex64(s->rip);
            kser_puts(" cs=0x");           kser_hex64(s->cs);
            kser_puts(" rfl=0x");          kser_hex64(s->rflags);
            if (from_user) {
                kser_puts(" rsp=0x");      kser_hex64(s->rsp);
                kser_puts(" ss=0x");       kser_hex64(s->ss);
            }
            kser_puts("\n");
            __asm__ volatile("cli;hlt");
        }
    }
    int irq = (int)(vec - 0x20);
    /* EOI first.  sched_tick() can enter a user process and iretq away
     * without ever returning, so an EOI sent after dispatch would be
     * skipped -- leaving IRQ0 permanently in-service, which masks every
     * later timer interrupt.  Acknowledging here guarantees the PIC is
     * told regardless of whether the handler returns. */
    pic_eoi(irq);
    switch(irq) {
        case 0:  sched_tick(s);      break;
        case 1:  ps2_keyboard_irq(); break;
        case 12: ps2_mouse_irq();    break;
        default: break;
    }
}
