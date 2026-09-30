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
static inline uint8_t inb(uint16_t port) {
    uint8_t v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(port)); return v;
}

void pit_init(void) {
    outb(PIT_CMD,      PIT_CMD_INIT);
    outb(PIT_CHANNEL0, (uint8_t)(PIT_DIVISOR & 0xFF));        /* lo byte */
    outb(PIT_CHANNEL0, (uint8_t)((PIT_DIVISOR >> 8) & 0xFF)); /* hi byte */
}

/* Count real time off the channel-0 counter instead of iterating a fixed
 * loop: every read of the PS/2 status port outruns any preset iteration
 * count, so a counted loop paced differently on every host.  One full roll
 * of the counter covers PIT_DIVISOR ticks of the 1.193182 MHz input. */
static uint16_t pit_read_count(void) {
    outb(PIT_CMD, 0x00);                      /* latch channel 0 */
    uint8_t lo = inb(PIT_CHANNEL0);
    uint8_t hi = inb(PIT_CHANNEL0);
    return (uint16_t)lo | ((uint16_t)hi << 8);
}

void pit_delay_ms(uint32_t ms) {
    uint32_t want = ms * (uint32_t)(PIT_FREQUENCY / 1000UL);
    uint32_t acc = 0;
    uint16_t prev = pit_read_count();
    while (acc < want) {
        uint16_t cur = pit_read_count();
        acc += (prev >= cur) ? (uint32_t)(prev - cur)
                             : (uint32_t)(prev + PIT_DIVISOR - cur);
        prev = cur;
    }
}
