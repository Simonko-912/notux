#pragma once

/* USB driver configuration options */

/* Enable USB keyboard support */
#define CONFIG_USB_KEYBOARD 1

/* Enable USB mouse support */
#define CONFIG_USB_MOUSE 1

/* Enable USB HID support */
#define CONFIG_USB_HID 1

/* Enable USB host controller support */
#define CONFIG_USB_HOST 1

/* USB interrupt handling */
#define CONFIG_USB_INTERRUPTS 1

/* USB device enumeration */
#define CONFIG_USB_ENUMERATION 1

/* Maximum number of USB devices */
#define USB_MAX_DEVICES 8

/* USB endpoint configuration */
#define USB_MAX_ENDPOINTS 16

/* USB buffer sizes */
#define USB_BUFFER_SIZE 512
#define USB_PACKET_SIZE 64