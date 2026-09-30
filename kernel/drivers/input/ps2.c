/*
 * Notux OS — PS/2 keyboard driver (IRQ-driven, tty-backed)
 * kernel/drivers/input/ps2.c
 *
 * Scancodes decode here so every reader sees the same bytes a real terminal
 * would produce: plain characters honour Shift/CapsLock, arrow-family keys
 * arrive as short CSI sequences, Ctrl+Alt+F1..F4 flips the visible console —
 * exactly the arrangement Linux vt10x-style consoles trained everyone on.
 * Decoded bytes are handed to the tty layer, which queues them for whichever
 * console is visible and lets the foreground reader decide how to echo.
 */

#include "ps2.h"
#include "../../tty/tty.h"
#include <stdint.h>

#define KBD_DATA_PORT 0x60
#define KBD_STATUS    0x64

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port)); return v;
}

/* Scancode set 1 → ASCII, US layout, unshifted and shifted pairs so that
 * Shift+digits-style punctuation ('!' '?' ':' '_' '{') actually arrives and
 * CapsLock behaves like a real console rather than doing nothing. */
static const char sc_plain[128] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0,
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,'7','8','9','-','4','5','6','+','1','2','3','0','.',
};
static const char sc_shift[128] = {
    0,0,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,' ',0,
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,'7','8','9','-','4','5','6','+','1','2','3','0','.',
};

#define K_SHIFT_L  0x2A   /* left shift                      */
#define K_SHIFT_R  0x36   /* right shift                     */
#define K_CTRL     0x1D   /* left ctrl                       */
#define K_CTRL_R   0x11   /* ctrl on some extended layouts   */
#define K_ALT      0x38   /* alt / altgr                     */
#define K_CAPS     0x3A   /* caps lock                       */
#define K_EXT      0xE0   /* introduces the arrow/paging row */
#define FN_FIRST   0x3B   /* F1..F4 row used for console hops */

static int kb_shift, kb_caps, kb_ctrl, kb_alt, kb_ext;

/* Feed a whole CSI sequence byte by byte into the visible console. */
static void csii(const char *s) { while (*s) tty_got_char(*s++); }

void ps2_init(void) {
    kb_shift = kb_caps = kb_ctrl = kb_alt = kb_ext = 0;
    /* Flush stale bytes sitting in the controller output buffer. */
    while (inb(KBD_STATUS) & 1) inb(KBD_DATA_PORT);
    outb(KBD_DATA_PORT, 0xF4);            /* enable scanning */
} 
void ps2_keyboard_irq(void) { 
    if (!(inb(KBD_STATUS) & 1)) return; 
    uint8_t raw = inb(KBD_DATA_PORT); 
    int     up  = raw & 0x80;             /* bit7 = release frame      */ 
    uint8_t sc  = raw & 0x7F;             /* bit7 carried no key index */ 
 
    if (sc == K_EXT) { kb_ext = 1; return; } 
 
    /* Modifier latches: presses raise them, releases lower them again. */ 
    if (sc == K_SHIFT_L || sc == K_SHIFT_R) { kb_shift = !up; return; } 
    if (sc == K_CTRL    || sc == K_CTRL_R)  { kb_ctrl  = !up; return; } 
    if (sc == K_ALT)                        { kb_alt   = !up; return; } 
    if (sc == K_CAPS)   { if (!up) kb_caps = !kb_caps; return; } 
 
    if (up) return;                       /* a lone release says nothing */ 
 
    /* Ctrl+Alt+F1..F4 hops to that console, like Linux's virtual terminals. */ 
    if (kb_ctrl && kb_alt && sc >= FN_FIRST && sc <= FN_FIRST + 3) { 
        tty_show(sc - FN_FIRST); 
        return; 
    } 
 
    if (kb_ext) {                         /* cursor/paging block */ 
        kb_ext = 0; 
        switch (sc) { 
        case 0x48: csii("\x1b[A");   break;    /* Up     */ 
        case 0x50: csii("\x1b[B");   break;    /* Down   */ 
        case 0x4D: csii("\x1b[C");   break;    /* Right  */ 
        case 0x4B: csii("\x1b[D");   break;    /* Left   */ 
        case 0x47: csii("\x1b[H");   break;    /* Home   */ 
        case 0x4F: csii("\x1b[F");   break;    /* End    */ 
        case 0x49: csii("\x1b[5~");  break;    /* PgUp   */ 
        case 0x51: csii("\x1b[6~");  break;    /* PgDn   */ 
        case 0x53: csii("\x1b[3~");  break;    /* Del    */ 
        default:   break;                      /* other extended keys stay silent */ 
        }                                        /* until someone learns them     */ 
        return; 
    } 
 
    /* CapsLock acts on letters like a soft Shift; punctuation keeps whatever 
     * the physical Shift asked for, matching everyday terminal habits. */ 
    char g = (kb_shift ^ kb_caps) ? sc_shift[sc] : sc_plain[sc]; 
    if (!g) return;                                /* hole in the map: silence */ 
    tty_got_char(g); 
} 
void ps2_mouse_irq(void) {} 
 
/* Readers pull typed bytes straight from the visible console's queue. */ 
int ps2_getchar(void) { return tty_pop_visible(); } 
int ps2_getchar_block(void) { 
    for (;;) {                                        /* sleeps between polls */ 
        int c = ps2_getchar(); 
        if (c >= 0) return c; 
        __asm__ volatile("sti; hlt"); 
    } 
} 
