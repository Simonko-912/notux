/*
 * Notux OS — USB Keyboard Driver
 * kernel/drivers/usb/usb_keyboard.c
 *
 * Implements USB keyboard support as an extension to the existing PS/2 keyboard system.
 */

#include "usb.h"
#include "../../drivers/input/ps2.h"
#include "../../kernel.h"
#include "../../drivers/gfx/framebuffer.h"
#include <stdint.h>
#include <stddef.h>

/* USB HID keyboard scan code mapping */
static const uint8_t usb_to_ps2_keymap[128] = {
    0, 0, 0, 0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
    50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44, 2, 3,
    4, 5, 6, 7, 8, 9, 10, 11, 28, 13, 14, 15, 26, 27, 29, 43,
    42, 41, 39, 40, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242,
    243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* USB keyboard state tracking */
static uint8_t usb_keyboard_state[256] = {0};
static uint8_t usb_keyboard_modifiers = 0;

void usb_kbd_init(void) {
    fb_puts("USB keyboard driver initializing...\n");

    /* Initialize the keyboard state */
    for (int i = 0; i < 256; i++) {
        usb_keyboard_state[i] = 0;
    }
    usb_keyboard_modifiers = 0;

    fb_puts("USB keyboard driver initialized.\n");
}

/* Read a key from USB keyboard */
int usb_kbd_read_key(uint8_t *keycode) {
    if (!keycode) return -1;

    /* In a real implementation, this would:
     * 1. Poll USB keyboard interrupt endpoint
     * 2. Parse HID keyboard report
     * 3. Convert to PS/2 scan codes or direct keycodes
     */

    // Placeholder for actual implementation
    return -1;  // Not implemented yet
}

/* Check if a key is currently pressed */
int usb_kbd_is_key_pressed(uint8_t keycode) {
    if (keycode >= 256) return 0;
    return usb_keyboard_state[keycode];
}

/* Get keyboard modifiers (shift, ctrl, alt, etc.) */
uint8_t usb_kbd_get_modifiers(void) {
    return usb_keyboard_modifiers;
}

/* Handle keyboard LED state changes */
void usb_kbd_set_leds(uint8_t leds) {
    /* In a real implementation:
     * 1. Send SET_REPORT command to USB keyboard
     * 2. Configure LED states for caps lock, num lock, scroll lock
     */

    fb_puts("USB keyboard LEDs set: ");
    char led_str[4];
    num_to_str(leds, led_str, 10);
    fb_puts(led_str);
    fb_putc('\n');
}

/* Process a USB keyboard interrupt (called from USB driver) */
void usb_kbd_interrupt_handler(void) {
    /* In a real implementation:
     * 1. Read HID report from keyboard endpoint
     * 2. Parse key press/release events
     * 3. Update internal state
     * 4. Call appropriate handlers or update global state
     */

    fb_puts("USB keyboard interrupt received.\n");
}

/* Integration with PS/2 keyboard system */
void usb_kbd_integrate_with_ps2(void) {
    /* This function would integrate USB keyboard with existing PS/2 system
     * by making both keyboards available to the input subsystem
     */

    fb_puts("Integrating USB keyboard with PS/2 system...\n");

    /* In a real implementation, this would:
     * 1. Register USB keyboard with input manager
     * 2. Set up interrupt handling for both keyboards
     * 3. Ensure both work seamlessly in the same system
     */
}
