/*
 * Notux OS — IDT (Interrupt Descriptor Table)
 * kernel/arch/x86_64/idt.c
 */

#include "idt.h"
#include "gdt.h"
#include <stdint.h>

#define IDT_PRESENT    (1ULL << 47)
#define IDT_RING0      (0ULL << 45)
#define IDT_RING3      (3ULL << 45)
#define IDT_GATE_INT   (0xEULL << 40)
#define IDT_GATE_TRAP  (0xFULL << 40)

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} IDTEntry;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} IDTR;

static IDTEntry g_idt[256] __attribute__((aligned(16)));
static IDTR     g_idtr;


extern void (*isr_stubs[256])(void);

static void idt_set_gate(int vec, void (*handler)(void), int dpl, int ist_slot) {
    uint64_t addr = (uint64_t)(uintptr_t)handler;

    g_idt[vec].offset_low  = (uint16_t)(addr & 0xFFFF);
    g_idt[vec].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    g_idt[vec].offset_high = (uint32_t)(addr >> 32);

    g_idt[vec].selector  = 0x08;
    g_idt[vec].ist       = (uint8_t)(ist_slot & 7);

    g_idt[vec].type_attr = (uint8_t)(0x8E | ((dpl & 3) << 5));
    g_idt[vec].zero      = 0;
}

void idt_init(void) {
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (void (*)(void))isr_stubs[i], 0, 0);
    }

    idt_set_gate(8,   (void (*)(void))isr_stubs[8],   0, 1);
    idt_set_gate(2,   (void (*)(void))isr_stubs[2],   0, 2);
    idt_set_gate(0x80,(void (*)(void))isr_stubs[0x80], 3, 0);

    g_idtr.limit = sizeof(g_idt) - 1;
    g_idtr.base  = (uint64_t)(uintptr_t)g_idt;

    __asm__ volatile ("lidt (%0)" :: "r"(&g_idtr) : "memory");
}
