/*
 * Notux OS — Framebuffer + Font Renderer
 * kernel/drivers/gfx/font.c
 *
 * Renders glyphs from any loaded bitmap font (including the
 * user-supplied SYSFONT 10×16 format) onto the linear framebuffer.
 *
 * Font format expected (matches the .h you provided):
 *   - 2 bytes per row, 16 rows per glyph, MSB = leftmost pixel
 *   - Width: FONT_W pixels, Height: FONT_H pixels
 *   - Glyph lookup via sorted codepoint → data table
 */

#include "font.h"
#include "framebuffer.h"
#include "../../kernel.h"
#include "../../../libnotux/include/notux/libc.h"
#include <stdint.h>
#include <stddef.h>

/* ── Active font state ───────────────────────────────────────── */
static FontDescriptor g_font = {0};

/* ── Terminal cursor (character grid) ───────────────────────── */
static int g_col = 0;
static int g_row = 0;
static uint32_t g_fg = 0xFFFFFFFF;   /* ARGB white  */
static uint32_t g_bg = 0xFF1E1E2E;   /* ARGB dark   */

static int g_cols = 80;   /* recalculated after fb_init */
static int g_rows = 25;

/* ── Register a font ─────────────────────────────────────────── */
void font_register(const FontDescriptor *desc) {
    g_font = *desc;

    /* Recalculate character grid dimensions */
    FramebufferInfo *fb = fb_get();
    if (fb && g_font.glyph_w > 0 && g_font.glyph_h > 0) {
        g_cols = (int)(fb->width  / (uint32_t)g_font.glyph_w);
        g_rows = (int)(fb->height / (uint32_t)g_font.glyph_h);
    }
}

/* ── Load the built-in SYSFONT (from the user-supplied .h) ────── */
/*
 * The SYSFONT glyphs are compiled into the kernel image
 * in the .font_data section.  This function just fills a
 * FontDescriptor pointing at that data.
 */
extern const uint8_t  __sysfont_data_start[];
extern const uint32_t __sysfont_cp_table[];   /* codepoint list */
extern const uint32_t __sysfont_count;


/* ── Binary search for glyph data ─────────────────────────────── */
static const uint8_t *glyph_lookup(uint32_t codepoint) {
    if (!g_font.data || g_font.glyph_count == 0)
        return NULL;

    int lo = 0, hi = (int)g_font.glyph_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint32_t cp = g_font.cp_table[mid];
        if (cp == codepoint)
            return g_font.data + mid * (size_t)(g_font.glyph_h * g_font.bytes_per_row);
        if (cp < codepoint) lo = mid + 1;
        else                hi = mid - 1;
    }
    return NULL; /* glyph not found — caller renders a box */
}

/* ── Render one glyph at pixel (px, py) ─────────────────────── */
void font_draw_glyph(uint32_t codepoint, int px, int py,
                     uint32_t fg, uint32_t bg) {
    const uint8_t *bits = glyph_lookup(codepoint);
    int W = g_font.glyph_w;
    int H = g_font.glyph_h;
    int BPR = (W + 7) / 8; /* bytes per row (2 for 10-px font) */

    for (int row = 0; row < H; row++) {

        /* Load up to 16 bits from font row data using actual BPR */
        uint16_t word = 0;
        if (bits) {
            if (BPR >= 2) word = (uint16_t)((bits[row * BPR] << 8)
                                           | bits[row * BPR + 1]);
            else          word = (uint16_t)(bits[row * BPR] << 8);
        }

        /* ── Start from MSB of 16-bit storage ── */
        uint16_t mask = (uint16_t)(1u << 15);
    
        /* ── Render only the visible glyph width (W = 10) ── */
        for (int col = 0; col < W; col++, mask >>= 1) {
    
            uint32_t color;
    
            if (word & mask)
                color = fg;
            else
                color = bg;
    
            fb_put_pixel(px + col, py + row, color);
        }
    }

}

/* ── Scroll the terminal up by one line ──────────────────────── */
static void scroll_up(void) {
    FramebufferInfo *fb = fb_get();
    uint32_t line_bytes = (uint32_t)g_font.glyph_h * fb->pitch;
    uint8_t *base = (uint8_t *)(uintptr_t)fb->base;

    /* Blit all lines one row up */
    uint32_t visible = (uint32_t)(g_rows - 1) * line_bytes;
    for (uint32_t i = 0; i < visible; i++)
        base[i] = base[i + line_bytes];

    /* Clear the last line with bg color */
    uint8_t r = (uint8_t)(g_bg >> 16);
    uint8_t g = (uint8_t)(g_bg >>  8);
    uint8_t b = (uint8_t)(g_bg      );
    uint32_t last_start = visible;
    for (uint32_t i = 0; i < line_bytes; i += 4) {
        base[last_start + i + 0] = b;
        base[last_start + i + 1] = g;
        base[last_start + i + 2] = r;
        base[last_start + i + 3] = 0xFF;
    }
}

/* ── Write a character to the terminal ──────────────────────── */
void fb_putc(char c) {
    if (c == '\n') {
        g_col = 0;
        g_row++;
        if (g_row >= g_rows) {
            scroll_up();
            g_row = g_rows - 1;
        }
        return;
    }
    if (c == '\r') { g_col = 0; return; }
    if (c == '\t') {
        int next = (g_col + 8) & ~7;
        for (; g_col < next && g_col < g_cols; g_col++)
            font_draw_glyph(' ', g_col * g_font.glyph_w,
                            g_row * g_font.glyph_h, g_fg, g_bg);
        return;
    }
    if (c == '\b') {
        if (g_col > 0) {
            g_col--;
            font_draw_glyph(' ', g_col * g_font.glyph_w,
                            g_row * g_font.glyph_h, g_fg, g_bg);
        }
        return;
    }

    font_draw_glyph((uint32_t)(unsigned char)c,
                    g_col * g_font.glyph_w,
                    g_row * g_font.glyph_h,
                    g_fg, g_bg);
    g_col++;
    if (g_col >= g_cols) {
        g_col = 0;
        g_row++;
        if (g_row >= g_rows) {
            scroll_up();
            g_row = g_rows - 1;
        }
    }
}

/* ── Write a string ──────────────────────────────────────────── */
void fb_puts(const char *s) {
    while (*s) fb_putc(*s++);
}

/* ── Color setters ───────────────────────────────────────────── */
void fb_set_fg(uint32_t color) { g_fg = color | 0xFF000000; }
void fb_set_bg(uint32_t color) { g_bg = color | 0xFF000000; }
void fb_set_color(uint32_t fg, uint32_t bg) {
    fb_set_fg(fg);
    fb_set_bg(bg);
}

/* ── Cursor position ─────────────────────────────────────────── */
void fb_set_cursor(int col, int row) {
    g_col = col;
    g_row = row;
}
void fb_get_cursor(int *col, int *row) { *col = g_col; *row = g_row; }
int  fb_get_cols(void) { return g_cols; }
int  fb_get_rows(void) { return g_rows; }
