/*
 * Notux OS — USB Mouse Driver
 * kernel/drivers/usb/usb_mouse.c
 *
 * Implements USB mouse support as an extension to the existing PS/2 mouse system.
 */

#include "usb.h"
#include "../../drivers/input/ps2.h"
#include "../../kernel.h"
#include "../../drivers/gfx/framebuffer.h"
#include <stdint.h>
#include <stddef.h>

/* USB mouse state */
static struct {
    int8_t dx, dy;
    uint8_t buttons;
    uint16_t x, y;
    uint8_t wheel;
} usb_mouse_state = {0};

/* USB mouse configuration */
static struct {
    uint8_t resolution;  // DPI resolution
    uint8_t sample_rate; // Samples per second
    uint8_t scaling;     // 1:1 or 2:1 scaling
} usb_mouse_config = {0};

void usb_mouse_init(void) {
    fb_puts("USB mouse driver initializing...\n");

    /* Initialize mouse state */
    usb_mouse_state.dx = 0;
    usb_mouse_state.dy = 0;
    usb_mouse_state.buttons = 0;
    usb_mouse_state.x = 0;
    usb_mouse_state.y = 0;
    usb_mouse_state.wheel = 0;

    /* Initialize configuration */
    usb_mouse_config.resolution = 4;  // Default 400 DPI
    usb_mouse_config.sample_rate = 100; // Default 100 samples/sec
    usb_mouse_config.scaling = 1;       // 1:1 scaling

    fb_puts("USB mouse driver initialized.\n");
}

/* Read mouse movement data from USB mouse */
int usb_mouse_read_data(int8_t *dx, int8_t *dy, uint8_t *buttons) {
    if (!dx || !dy || !buttons) return -1;

    /* In a real implementation:
     * 1. Read HID report from mouse interrupt endpoint
     * 2. Parse movement data and button states
     * 3. Update internal state
     */

    // Placeholder - would read actual mouse data in real system
    *dx = usb_mouse_state.dx;
    *dy = usb_mouse_state.dy;
    *buttons = usb_mouse_state.buttons;

    return 0;  // Success
}

/* Get complete mouse information */
int usb_mouse_get_info(uint8_t *buttons, uint16_t *x, uint16_t *y) {
    if (!buttons || !x || !y) return -1;

    *buttons = usb_mouse_state.buttons;
    *x = usb_mouse_state.x;
    *y = usb_mouse_state.y;

    return 0;  // Success
}

/* Get mouse wheel state */
int usb_mouse_get_wheel(int8_t *wheel) {
    if (!wheel) return -1;

    *wheel = usb_mouse_state.wheel;
    return 0;  // Success
}

/* Set mouse configuration */
void usb_mouse_set_config(uint8_t resolution, uint8_t sample_rate, uint8_t scaling) {
    /* In a real implementation:
     * 1. Send SET_REPORT command to USB mouse
     * 2. Configure device settings
     */

    usb_mouse_config.resolution = resolution;
    usb_mouse_config.sample_rate = sample_rate;
    usb_mouse_config.scaling = scaling;

    fb_puts("USB mouse configuration updated.\n");
}

/* Handle USB mouse interrupt */
void usb_mouse_interrupt_handler(void) {
    /* In a real implementation:
     * 1. Read HID report from mouse endpoint
     * 2. Parse movement and button data
     * 3. Update state variables
     * 4. Call appropriate handlers
     */

    fb_puts("USB mouse interrupt received.\n");
}

/* Process movement data (called by USB subsystem) */
void usb_mouse_process_movement(int8_t dx, int8_t dy, uint8_t buttons) {
    /* In a real implementation:
     * 1. Update internal mouse state
     * 2. Handle coordinate transformations
     * 3. Call GUI or input handlers
     */

    usb_mouse_state.dx = dx;
    usb_mouse_state.dy = dy;
    usb_mouse_state.buttons = buttons;

    // Update position (simplified)
    usb_mouse_state.x += dx;
    usb_mouse_state.y += dy;
}

/* Integration with existing PS/2 mouse system */
void usb_mouse_integrate_with_ps2(void) {
    /* This function would integrate USB mouse with existing PS/2 system
     * by making both mice available to the input subsystem
     */

    fb_puts("Integrating USB mouse with PS/2 system...\n");

    /* In a real implementation, this would:
     * 1. Register USB mouse with input manager
     * 2. Set up interrupt handling for both mice
     * 3. Ensure both work seamlessly in the same system
     */
}
