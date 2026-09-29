/*
 * Notux OS — GDT (Global Descriptor Table)
 * kernel/arch/x86_64/gdt.c
 *
 * Sets up the minimal 64-bit GDT:
 *   0x00  null descriptor
 *   0x08  kernel code  (ring 0, 64-bit)
 *   0x10  kernel data  (ring 0)
 *   0x18  user code    (ring 3, 64-bit)  — note: selector 0x1B (RPL=3)
 *   0x20  user data    (ring 3)          — note: selector 0x23 (RPL=3)
 *   0x28  TSS (low 8 bytes)
 *   0x30  TSS (high 8 bytes)             — 64-bit TSS is 16 bytes
 *
 * Also sets up the TSS with RSP0 (kernel stack for syscall/interrupt).
 */

#include "gdt.h"
#include <stdint.h>

/* ── Descriptor types ────────────────────────────────────────── */
#define GDT_PRESENT     (1ULL << 47)
#define GDT_DPL0        (0ULL << 45)
#define GDT_DPL3        (3ULL << 45)
#define GDT_CODE        (1ULL << 43)   /* executable */
#define GDT_DATA        (0ULL << 43)
#define GDT_NON_SYSTEM  (1ULL << 44)
#define GDT_LONG        (1ULL << 53)   /* 64-bit code */
#define GDT_DB          (0ULL << 54)   /* must be 0 for 64-bit */
#define GDT_GRANULARITY (1ULL << 55)
#define GDT_READWRITE   (1ULL << 41)

/* TSS type = 0x9 (64-bit available TSS) */
#define TSS_PRESENT     (1ULL << 47)
#define TSS_TYPE        (0x9ULL << 40)

/* ── GDT entries ─────────────────────────────────────────────── */
static uint64_t gdt[8] __attribute__((aligned(8)));

/* ── Task State Segment ──────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;          /* kernel stack for ring-0 entry */
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];        /* interrupt stack table */
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} TSS64;

static TSS64 g_tss __attribute__((aligned(16)));

/* ── GDT pointer ─────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} GDTR;

static GDTR g_gdtr;

/* ── Build a 64-bit code/data descriptor ────────────────────── */
static uint64_t make_desc(int dpl, int is_code) {
    uint64_t d = GDT_PRESENT | GDT_NON_SYSTEM | GDT_GRANULARITY | GDT_READWRITE;
    d |= (uint64_t)(dpl & 3) << 45;
    if (is_code) d |= GDT_CODE | GDT_LONG;
    return d;
}

/* ── Load GDT and reload segment registers ────────────────────  */
static void lgdt(void *gdtr_ptr) {
    __asm__ volatile(
        "lgdt (%0)          \n"
        /* Reload CS via a far return */
        "lea  1f(%%rip), %%rax \n"
        "push $0x08         \n"  /* kernel CS */
        "push %%rax         \n"
        "lretq              \n"
        "1:                 \n"
        /* Reload data segments */
        "mov $0x10, %%ax    \n"  /* kernel DS */
        "mov %%ax, %%ds     \n"
        "mov %%ax, %%es     \n"
        "mov %%ax, %%fs     \n"
        "mov %%ax, %%gs     \n"
        "mov %%ax, %%ss     \n"
        :: "r"(gdtr_ptr) : "rax", "memory"
    );
}

void gdt_init(void) {
    /* 0: null */
    gdt[0] = 0;

    /* 1: kernel code (0x08) */
    gdt[1] = make_desc(0, 1);

    /* 2: kernel data (0x10) */
    gdt[2] = make_desc(0, 0);

    /* 3: user code  (0x18, use selector 0x1B with RPL=3) */
    gdt[3] = make_desc(3, 1);

    /* 4: user data  (0x20, use selector 0x23 with RPL=3) */
    gdt[4] = make_desc(3, 0);

    /* 5+6: TSS descriptor (16 bytes = two 8-byte slots) */
    uint64_t tss_base  = (uint64_t)(uintptr_t)&g_tss;
    uint64_t tss_limit = sizeof(TSS64) - 1;

    gdt[5] = TSS_PRESENT | TSS_TYPE
           | ((tss_base  & 0xFFFFFFULL) << 16)
           | ((tss_base  >> 24 & 0xFFULL) << 56)
           | ((tss_limit & 0xFFFFULL))
           | ((tss_limit >> 16 & 0xFULL) << 48);
    gdt[6] = tss_base >> 32;   /* upper 32 bits of base */

    /* TSS: iomap beyond TSS length = no IOPM */
    g_tss.iomap_base = sizeof(TSS64);

    g_gdtr.limit = sizeof(gdt) - 1;
    g_gdtr.base  = (uint64_t)(uintptr_t)gdt;

    lgdt(&g_gdtr);

    /* Load TSS */
    __asm__ volatile("ltr %0" :: "r"((uint16_t)0x28));
}

/* Update RSP0 in TSS when switching to a new process */
void gdt_set_kernel_stack(uint64_t rsp0) {
    g_tss.rsp0 = rsp0;
}
