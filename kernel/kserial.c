/*
 * Notux OS — Kernel Serial Driver
 * kernel/kserial.c
 */
#include "kserial.h"
#include "drivers/gfx/font.h"
#include <stdint.h>

static inline void _outb(uint16_t p,uint8_t v){
    __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));
}
static inline uint8_t _inb(uint16_t p){
    uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;
}

void kser_init(void) {
    _outb(0x3F9, 0x00); /* disable interrupts  */
    _outb(0x3FB, 0x80); /* DLAB on             */
    _outb(0x3F8, 0x01); /* 115200 baud lo      */
    _outb(0x3F9, 0x00); /* 115200 baud hi      */
    _outb(0x3FB, 0x03); /* 8N1                 */
    _outb(0x3FA, 0xC7); /* enable FIFO         */
    _outb(0x3FC, 0x03); /* RTS+DTR             */
}

void kser_putc(char c) {
    if (c == '\n') kser_putc('\r');
    while (!(_inb(0x3FD) & 0x20));
    _outb(0x3F8, (uint8_t)c);
}

void kser_puts(const char *s) { while (*s) kser_putc(*s++); }

void kser_hex64(uint64_t v) {
    kser_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        int n = (int)((v >> i) & 0xF);
        kser_putc((char)(n < 10 ? '0'+n : 'A'+n-10));
    }
}

void kser_dec(uint64_t v) {
    char buf[24]; int i = 0;
    if (!v) { kser_putc('0'); return; }
    while (v) { buf[i++] = (char)('0' + v % 10); v /= 10; }
    while (i--) kser_putc(buf[i]);
}

/* Write to both framebuffer and serial */
void klog(const char *s) {
    fb_puts(s);
    kser_puts(s);
}
