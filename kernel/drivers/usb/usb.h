#pragma once
#include <stdint.h>

/* USB Device Descriptor */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed)) UsbDeviceDesc;

/* USB Configuration Descriptor */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed)) UsbConfigDesc;

/* USB Interface Descriptor */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} __attribute__((packed)) UsbInterfaceDesc;

/* USB Endpoint Descriptor */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed)) UsbEndpointDesc;

/* USB HID Descriptor */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdHID;
    uint8_t  bCountryCode;
    uint8_t  bNumDescriptors;
    struct {
        uint8_t bDescriptorType;
        uint16_t wDescriptorLength;
    } __attribute__((packed)) desc[1];
} __attribute__((packed)) UsbHidDesc;

/* USB Controller interface */
typedef struct {
    void (*init)(void);
    int (*enumerate)(void);
    int (*setup_endpoint)(uint8_t endpoint, uint8_t type, uint16_t max_packet_size);
    int (*transfer)(uint8_t endpoint, void* buffer, uint16_t length);
    int (*get_device_descriptor)(UsbDeviceDesc *desc);
} UsbController;

/* USB HID keyboard interface */
typedef struct {
    void (*init)(void);
    int (*read_key)(uint8_t *keycode);
    int (*is_key_pressed)(uint8_t keycode);
    void (*set_leds)(uint8_t leds);
} UsbHidKeyboard;

/* USB HID mouse interface */
typedef struct {
    void (*init)(void);
    int (*read_mouse)(int8_t *dx, int8_t *dy, uint8_t *buttons);
    int (*get_mouse_info)(uint8_t *buttons, uint16_t *x, uint16_t *y);
} UsbHidMouse;

/* USB subsystem initialization */
void usb_init(void);
void usb_controller_init(void);

/* USB HID keyboard functions */
void usb_keyboard_init(void);
int usb_keyboard_read_key(uint8_t *keycode);
int usb_keyboard_is_key_pressed(uint8_t keycode);
void usb_keyboard_set_leds(uint8_t leds);

/* USB HID mouse functions */
void usb_mouse_init(void);
int usb_mouse_read_data(int8_t *dx, int8_t *dy, uint8_t *buttons);
int usb_mouse_get_info(uint8_t *buttons, uint16_t *x, uint16_t *y);