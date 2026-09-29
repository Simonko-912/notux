#pragma once
void ps2_init(void);
void ps2_keyboard_irq(void);
void ps2_mouse_irq(void);
int  ps2_getchar(void);
int  ps2_getchar_block(void);
