#pragma once
#include <stdint.h>
/* FramebufferInfo is defined in boot/boot_info.h */
#include "../../../boot/boot_info.h"

void             fb_init(FramebufferInfo *fb);
FramebufferInfo *fb_get(void);
void             fb_put_pixel(int x, int y, uint32_t color);
void             fb_clear(void);
void fb_putc(char c);
void fb_puts(const char *s);
void fb_set_fg(uint32_t color);
void fb_set_bg(uint32_t color);
void fb_set_color(uint32_t fg, uint32_t bg);
void fb_set_cursor(int col, int row);
void fb_get_cursor(int *col, int *row);
int  fb_get_cols(void);
int  fb_get_rows(void);
