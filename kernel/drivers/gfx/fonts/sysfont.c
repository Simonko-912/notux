/*
 * Notux OS — sysfont wrapper
 * kernel/drivers/gfx/fonts/sysfont.c
 *
 * Drop your sysfont.h into this same directory and build.
 * Replace the stub sysfont.h with your real generated font file.
 */
#include "../font.h"
#include "sysfont.h"

#define BYTES_PER_ROW   ((SYSFONT_W + 7) / 8)
#define BYTES_PER_GLYPH (SYSFONT_H * BYTES_PER_ROW)

static uint32_t g_cp_table[SYSFONT_COUNT];
static uint8_t  g_data[SYSFONT_COUNT * SYSFONT_H * BYTES_PER_ROW];

void myfont_register(void) {
    for (int i = 0; i < SYSFONT_COUNT; i++) {
        g_cp_table[i] = SYSFONT_glyphs[i].cp;
        const uint8_t *src = SYSFONT_glyphs[i].data;
        uint8_t       *dst = g_data + i * BYTES_PER_GLYPH;
        for (int b = 0; b < BYTES_PER_GLYPH; b++)
            dst[b] = src[b];
    }
    FontDescriptor d;
    d.glyph_w       = SYSFONT_W;
    d.glyph_h       = SYSFONT_H;
    d.bytes_per_row = BYTES_PER_ROW;
    d.ascent        = SYSFONT_ASCENT;
    d.descent       = SYSFONT_DESCENT;
    d.data          = g_data;
    d.cp_table      = g_cp_table;
    d.glyph_count   = SYSFONT_COUNT;
    font_register(&d);
}
