/*
 * Notux OS — PS/2 keyboard driver (IRQ-driven ring buffer)
 * kernel/drivers/input/ps2.c
 */

#include "ps2.h"
#include <stdint.h>

#define KBD_DATA_PORT 0x60
#define KBD_STATUS    0x64

#define RING_SIZE 256

static volatile char g_ring[RING_SIZE];
static volatile int  g_head = 0;
static volatile int  g_tail = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port)); return v;
}

/* Scancode (set 1) → ASCII, US layout, no shift */
static const char sc_map[128] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0,
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,'7','8','9','-','4','5','6','+','1','2','3','0','.',
};

void ps2_init(void) {
    g_head = 0;
    g_tail = 0;
    /* Flush any stale bytes */
    while (inb(KBD_STATUS) & 1) inb(KBD_DATA_PORT);
    /* Enable keyboard scanning */
    outb(KBD_DATA_PORT, 0xF4);
}

void ps2_keyboard_irq(void) {
    if (!(inb(KBD_STATUS) & 1)) return;
    uint8_t sc = inb(KBD_DATA_PORT);
    if (sc & 0x80) return;          /* key release — ignore */
    if (sc >= 128 || !sc_map[sc]) return;
    char c = sc_map[sc];
    int next = (g_head + 1) % RING_SIZE;
    if (next == (int)g_tail) return; /* buffer full — drop */
    g_ring[g_head] = c;
    g_head = next;
}

void ps2_mouse_irq(void) {}

int ps2_getchar(void) {
    if ((int)g_tail == (int)g_head) return -1;
    char c = g_ring[g_tail];
    g_tail = (g_tail + 1) % RING_SIZE;
    return (unsigned char)c;
}

/* Blocking read: waits for a key (interrupts remain enabled). */
int ps2_getchar_block(void) {
    for (;;) {
        int c = ps2_getchar();
        if (c >= 0) return c;
        __asm__ volatile("sti; hlt");
    }
}