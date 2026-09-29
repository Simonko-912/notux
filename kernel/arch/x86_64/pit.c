/*
 * Notux OS — PIT (8253/8254) Programmable Interval Timer
 * kernel/arch/x86_64/pit.c
 *
 * Programs Channel 0 to fire IRQ0 at ~100 Hz.
 * This drives the preemptive scheduler tick.
 *
 * PIT base frequency: 1,193,182 Hz
 * Divisor for 100 Hz: 11932  (1193182 / 11932 ≈ 100.00 Hz)
 */

#include "pit.h"
#include <stdint.h>

#define PIT_CHANNEL0  0x40   /* Channel 0 data port */
#define PIT_CMD       0x43   /* Mode/command register */
/* Command: channel 0, lobyte/hibyte, mode 3 (square wave) */
#define PIT_CMD_INIT  0x36

#define PIT_FREQUENCY 1193182UL
#define PIT_HZ        100UL
#define PIT_DIVISOR   (PIT_FREQUENCY / PIT_HZ)   /* 11932 */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1" :: "a"(val), "Nd"(port));
}

void pit_init(void) {
    outb(PIT_CMD,      PIT_CMD_INIT);
    outb(PIT_CHANNEL0, (uint8_t)(PIT_DIVISOR & 0xFF));        /* lo byte */
    outb(PIT_CHANNEL0, (uint8_t)((PIT_DIVISOR >> 8) & 0xFF)); /* hi byte */
}
