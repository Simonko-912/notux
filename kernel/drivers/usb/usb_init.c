/*
 * Notux OS — USB Initialization
 * kernel/drivers/usb/usb_init.c
 *
 * USB driver initialization and integration with the main system.
 */

#include "usb.h"
#include "../../kernel.h"
#include "../../drivers/input/ps2.h"
#include "../../drivers/gfx/framebuffer.h"
#include <stdint.h>

/* Forward declarations */
void usb_input_init(void);
void usb_kbd_integrate_with_ps2(void);
void usb_mouse_integrate_with_ps2(void);

void usb_system_init(void) {
    fb_puts("=== USB System Initialization ===\n");

    /* Initialize the basic USB subsystem */
    usb_init();

    /* Integrate with existing PS/2 input system */
    usb_input_init();

    /* Initialize USB keyboard integration */
    usb_kbd_integrate_with_ps2();

    /* Initialize USB mouse integration */
    usb_mouse_integrate_with_ps2();

    fb_puts("USB system initialized successfully.\n");
    fb_puts("===============================\n\n");
}

/* Integration with main kernel initialization */
void init_usb_drivers(void) {
    /* Called during kernel boot to initialize USB subsystem */
    fb_puts("Initializing USB drivers...\n");

    /* Initialize USB controller */
    usb_controller_init();

    /* Initialize keyboard and mouse support */
    usb_keyboard_init();
    usb_mouse_init();

    /* Initialize the full USB system */
    usb_system_init();
}
