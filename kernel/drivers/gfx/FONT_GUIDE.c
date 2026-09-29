/*
 * ═══════════════════════════════════════════════════════════════
 *  HOW TO ADD YOUR FONT TO NOTUX
 *  kernel/drivers/gfx/FONT_GUIDE.md  (read this as plain text)
 * ═══════════════════════════════════════════════════════════════
 *
 * Your font .h file uses the exact format the kernel expects.
 * Here is the complete step-by-step process.
 *
 * ── STEP 1: Drop your font file into the source tree ──────────
 *
 *   Copy your font header, e.g.  myfont.h, into:
 *       kernel/drivers/gfx/fonts/myfont.h
 *
 *   Your file already looks like this (from what you pasted):
 *
 *       #define SYSFONT_W   10
 *       #define SYSFONT_H   16
 *       #define SYSFONT_ASCENT  12
 *       #define SYSFONT_DESCENT  4
 *
 *       static const uint8_t SYSFONT_0020[32] = { ... };
 *       ...
 *       typedef struct { uint32_t cp; const uint8_t *data; } SYSFONT_Glyph;
 *       static const SYSFONT_Glyph SYSFONT_glyphs[] = { ... };
 *       #define SYSFONT_COUNT 32
 *
 *   That is perfect. No changes needed to the .h file itself.
 *
 *
 * ── STEP 2: Create a font wrapper .c file ─────────────────────
 *
 *   Create:  kernel/drivers/gfx/fonts/myfont.c
 *   Content (fill in YOUR values):
 *
 *       #include "../font.h"
 *       #include "myfont.h"      // <-- your file name here
 *
 *       // Builds the cp_table[] and data[] arrays the kernel needs
 *       // from the SYSFONT_glyphs[] table in your header.
 *
 *       static uint32_t myfont_cp_table[SYSFONT_COUNT];
 *       static uint8_t  myfont_data[SYSFONT_COUNT * SYSFONT_H * 2];
 *
 *       void myfont_register(void) {
 *           // Flatten the glyph table into contiguous arrays
 *           for (int i = 0; i < SYSFONT_COUNT; i++) {
 *               myfont_cp_table[i] = SYSFONT_glyphs[i].cp;
 *               for (int r = 0; r < SYSFONT_H * 2; r++)
 *                   myfont_data[i * SYSFONT_H * 2 + r] =
 *                       SYSFONT_glyphs[i].data[r];
 *           }
 *
 *           FontDescriptor d;
 *           d.glyph_w       = SYSFONT_W;
 *           d.glyph_h       = SYSFONT_H;
 *           d.bytes_per_row = 2;          // 2 bytes per row for 10px wide
 *           d.ascent        = SYSFONT_ASCENT;
 *           d.descent       = SYSFONT_DESCENT;
 *           d.data          = myfont_data;
 *           d.cp_table      = myfont_cp_table;
 *           d.glyph_count   = SYSFONT_COUNT;
 *           font_register(&d);
 *       }
 *
 *
 * ── STEP 3: Call your registration function from kernel/main.c ─
 *
 *   In kernel/main.c, find this section:
 *
 *       // 5. Framebuffer — text-mode output using SYSFONT
 *       fb_init(&bi->fb);
 *       font_load_builtin();   // <-- REPLACE THIS LINE
 *
 *   Replace font_load_builtin() with:
 *
 *       fb_init(&bi->fb);
 *       extern void myfont_register(void);
 *       myfont_register();      // <-- your function
 *
 *
 * ── STEP 4: Add the .c file to the Makefile ───────────────────
 *
 *   In Makefile, find the KERNEL_SRCS list and add:
 *
 *       kernel/drivers/gfx/fonts/myfont.c \
 *
 *
 * ── STEP 5: Build ─────────────────────────────────────────────
 *
 *       make all
 *
 *   That's it. Your font will be used everywhere:
 *     - Early boot text
 *     - The shell prompt
 *     - notedit
 *     - nfetch
 *     - All nx_cprintf() output from user programs
 *
 *
 * ═══════════════════════════════════════════════════════════════
 *  ADDING MORE CHARACTERS TO YOUR FONT
 * ═══════════════════════════════════════════════════════════════
 *
 *  Your current font has 32 glyphs (U+0020–U+003F).
 *  To add more (letters, numbers you're missing, etc.):
 *
 *  1. Add more entries to SYSFONT_glyphs[] in your .h file:
 *
 *       // 'A' U+0041
 *       static const uint8_t SYSFONT_0041[32] = {
 *           0x00,0x00,   // row 0  (top padding)
 *           0x00,0x00,   // row 1
 *           0x0C,0x00,   // row 2   ##
 *           0x12,0x00,   // row 3  #  #
 *           0x21,0x00,   // row 4  #   #   (remember: 10px, MSB=left)
 *           0x21,0x00,   // row 5
 *           0x3F,0x00,   // row 6  #####  (0x3F = 0011 1111)
 *           0x21,0x00,   // row 7  #   #
 *           0x21,0x00,   // row 8
 *           0x21,0x00,   // row 9
 *           0x00,0x00,   // row 10
 *           0x00,0x00,   // row 11
 *           0x00,0x00,   // row 12
 *           0x00,0x00,   // row 13
 *           0x00,0x00,   // row 14
 *           0x00,0x00,   // row 15
 *       };
 *
 *  2. Add the entry to SYSFONT_glyphs[]:
 *       { 65u, SYSFONT_0041 },   // 65 = 'A'
 *
 *  3. Increment SYSFONT_COUNT.
 *
 *  !! IMPORTANT: Keep glyphs sorted by codepoint (ascending).
 *     The kernel uses binary search on the cp_table.
 *
 *
 * ═══════════════════════════════════════════════════════════════
 *  UNDERSTANDING THE BIT FORMAT
 * ═══════════════════════════════════════════════════════════════
 *
 *  Each glyph is 16 rows × 2 bytes.
 *  Width is 10 pixels, MSB (most significant bit) = leftmost pixel.
 *
 *  Example: row bytes 0x0C, 0x00
 *
 *    Byte 0: 0x0C = 0000 1100
 *    Byte 1: 0x00 = 0000 0000
 *
 *    Combined 16-bit word: 0000 1100  0000 0000
 *    Pixel positions 0–9:  0 0 0 0 1 1 0 0  0 0
 *                                   ^ ^
 *                           pixels 4 and 5 are ON
 *
 *  So bit 15 = pixel 0 (leftmost), bit 6 = pixel 9 (rightmost of 10).
 *  Bits 5–0 of byte 1 are unused for a 10-pixel-wide font.
 *
 *
 * ═══════════════════════════════════════════════════════════════
 *  USING A DIFFERENT FONT SIZE
 * ═══════════════════════════════════════════════════════════════
 *
 *  Your font wrapper just needs to set the right values:
 *
 *       d.glyph_w       = YOUR_FONT_W;     // pixel width per glyph
 *       d.glyph_h       = YOUR_FONT_H;     // pixel height per glyph
 *       d.bytes_per_row = (YOUR_FONT_W + 7) / 8;   // round up to bytes
 *
 *  For example, an 8×16 font:  bytes_per_row = 1
 *              a 12×20 font:   bytes_per_row = 2
 *              a 16×32 font:   bytes_per_row = 2
 *
 *
 * ═══════════════════════════════════════════════════════════════
 *  USING MULTIPLE FONTS (e.g. GUI + shell different fonts)
 * ═══════════════════════════════════════════════════════════════
 *
 *  font_register() replaces the active font globally.
 *  To use different fonts in different contexts:
 *
 *       FontDescriptor shell_font = { ... };
 *       FontDescriptor gui_font   = { ... };
 *
 *       // When switching to shell:
 *       font_register(&shell_font);
 *
 *       // When switching to GUI widget:
 *       font_register(&gui_font);
 *
 *  You can also call font_draw_glyph() directly with any
 *  FontDescriptor pointer, bypassing the global active font.
 *
 */

/* This file is documentation only — nothing to compile here. */
void font_guide_placeholder(void) {}
