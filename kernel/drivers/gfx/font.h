/*
 * Notux OS — Font Descriptor
 * kernel/drivers/gfx/font.h
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* ── Describes any bitmap font ───────────────────────────────── */
typedef struct {
    uint32_t        glyph_w;        /* pixel width of each glyph       */
    uint32_t        glyph_h;        /* pixel height of each glyph      */
    uint32_t        bytes_per_row;  /* ceil(glyph_w / 8)               */
    uint32_t        ascent;
    uint32_t        descent;
    const uint8_t  *data;           /* flat array: glyph_count * glyph_h * bytes_per_row */
    const uint32_t *cp_table;       /* sorted codepoint array, length = glyph_count */
    uint32_t        glyph_count;
} FontDescriptor;

/* ── API ─────────────────────────────────────────────────────── */
void font_register(const FontDescriptor *desc);
void font_load_builtin(void);
void font_draw_glyph(uint32_t codepoint, int px, int py,
                     uint32_t fg, uint32_t bg);

/* Text output (goes through the active font + framebuffer cursor) */
void fb_putc(char c);
void fb_puts(const char *s);
void fb_set_fg(uint32_t color);
void fb_set_bg(uint32_t color);
void fb_set_color(uint32_t fg, uint32_t bg);
void fb_set_cursor(int col, int row);
void fb_get_cursor(int *col, int *row);
int  fb_get_cols(void);
int  fb_get_rows(void);
void fb_clear(void);
