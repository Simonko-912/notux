/*
 * Notux OS — Kernel Serial Output (COM1 debug)
 * kernel/kserial.h
 *
 * Used everywhere in the kernel for debug output that
 * also needs to be visible on serial (not just framebuffer).
 */
#pragma once
#include <stdint.h>

void kser_init(void);
void kser_putc(char c);
void kser_puts(const char *s);
void kser_hex64(uint64_t v);
void kser_dec(uint64_t v);
/* klog: writes to BOTH framebuffer AND serial */
void klog(const char *s);
