# Notux OS - USB Support Features

## Overview

This document describes the USB driver infrastructure added to Notux OS, including keyboard and mouse support that extends the existing PS/2 input system.

## New USB Driver Components

### 1. Core USB Subsystem (`kernel/drivers/usb/usb.c`)
- USB controller initialization framework
- Device enumeration interface
- Endpoint management
- Transfer handling
- Basic device descriptor access

### 2. USB Keyboard Support (`kernel/drivers/usb/usb_keyboard.c`)
- HID keyboard protocol support
- Key code mapping from USB to PS/2 format
- Modifier key tracking (shift, ctrl, alt)
- LED state control
- Integration with existing PS/2 keyboard system

### 3. USB Mouse Support (`kernel/drivers/usb/usb_mouse.c`)
- HID mouse protocol support
- Movement data processing
- Button state tracking
- Wheel support
- Integration with existing PS/2 mouse system

### 4. Integration Components
- `usb_init()` - Main initialization function
- `usb_input_init()` - Input system integration
- `usb_kbd_integrate_with_ps2()` - Keyboard system integration
- `usb_mouse_integrate_with_ps2()` - Mouse system integration

## Architecture

```
+---------------------+
|   Notux Kernel      |
|                     |
|  +--------------+   |
|  |  USB Drivers |   |
|  |              |   |
|  |  +---------+ |   |
|  |  | Keyboard| |   |
|  |  +---------+ |   |
|  |  +---------+ |   |
|  |  |  Mouse  | |   |
|  |  +---------+ |   |
|  +--------------+   |
|                     |
|  +--------------+   |
|  | PS/2 Drivers |   |
|  +--------------+   |
+---------------------+
```

## USB Device Support

### Currently Supported:
- USB HID keyboards (PS/2 compatibility)
- USB HID mice (PS/2 compatibility)
- USB interrupt endpoints
- Basic USB device enumeration
- LED control for keyboards
- Mouse movement and button data

### Planned Enhancements:
- Full USB host controller support (xHCI, EHCI)
- Mass storage devices (USB drives)
- Audio devices (speakers, microphones)
- Network adapters (USB Ethernet)
- Game controllers
- Webcam support

## Integration with Existing System

The USB drivers are designed to work alongside the existing PS/2 input system:

1. **Dual Input Support**: Both USB and PS/2 keyboards/mice can be used simultaneously
2. **Seamless Switching**: No need to choose between input methods
3. **Shared State Management**: Input from both sources handled uniformly
4. **Backward Compatibility**: Existing PS/2 drivers continue to work

## API Reference

### USB System Functions:
```c
void usb_init(void);                    // Initialize USB subsystem
void usb_controller_init(void);         // Initialize USB controller
void usb_keyboard_init(void);           // Initialize USB keyboard
void usb_mouse_init(void);              // Initialize USB mouse
void usb_input_init(void);              // Initialize input system integration
```

### USB Keyboard Functions:
```c
int usb_kbd_read_key(uint8_t *keycode);         // Read a key from USB keyboard
int usb_kbd_is_key_pressed(uint8_t keycode);    // Check if key is pressed
uint8_t usb_kbd_get_modifiers(void);            // Get modifier keys
void usb_kbd_set_leds(uint8_t leds);            // Set keyboard LEDs
```

### USB Mouse Functions:
```c
int usb_mouse_read_data(int8_t *dx, int8_t *dy, uint8_t *buttons);  // Read mouse movement
int usb_mouse_get_info(uint8_t *buttons, uint16_t *x, uint16_t *y);  // Get mouse position
int usb_mouse_get_wheel(int8_t *wheel);             // Get wheel state
```

## Configuration Options

### Enable USB Support:
```c
// In kernel/drivers/usb/config.h
#define CONFIG_USB_KEYBOARD 1   // Enable USB keyboard support
#define CONFIG_USB_MOUSE 1      // Enable USB mouse support
#define CONFIG_USB_HID 1        // Enable HID protocol support
#define CONFIG_USB_HOST 1       // Enable USB host controller support
```

### Build Configuration:
The USB drivers are integrated into the kernel build system and will be compiled automatically when enabled.

## Usage

### Boot Process Integration:
The USB subsystem initializes automatically during kernel boot after PS/2 initialization:

```c
// In kernel/main.c
/* 8. PS/2 keyboard */
klog("PS/2 init...\n");
ps2_init();

/* 9. USB drivers (if available) */
klog("USB init...\n");
#ifdef CONFIG_USB
init_usb_drivers();
#endif
```

### Application Level Usage:
Applications can use the standard input system which will handle both USB and PS/2 devices transparently.

## Limitations

### Current Limitations:
1. **Simplified Implementation**: Most functions are placeholders for full implementation
2. **No Hardware Support**: Actual USB controller hardware access is not implemented yet
3. **Limited Device Types**: Only basic keyboard/mouse support currently
4. **No USB Storage**: Mass storage device support not implemented

### Future Work:
1. **USB Host Controller Drivers**: Implement xHCI/EHCI controllers  
2. **Full HID Protocol**: Complete HID parser and report handling
3. **Device Management**: Proper device detection and configuration
4. **Power Management**: USB power management features
5. **Security**: USB device authentication and access control

## Testing Considerations

### Development Testing:
1. **Unit Tests**: Individual driver components can be tested separately
2. **Integration Tests**: Test both USB and PS/2 together
3. **Compatibility Tests**: Ensure no regression with existing PS/2 functionality

### System Testing:
1. **Boot Process**: Verify USB initialization during boot
2. **Input Handling**: Test key presses and mouse movement
3. **Error Handling**: Test with disconnected devices

## Future Expansion

The USB subsystem is designed to be extensible:

1. **Additional Device Types**: Add support for more USB device classes
2. **Enhanced Features**: Implement advanced USB protocols
3. **Performance Improvements**: Optimize interrupt handling and data transfer
4. **Driver Framework**: Create a more robust driver framework

## Build System Integration

The USB drivers are fully integrated into the existing Notux OS build system:

- Added to `Makefile` compilation list
- Proper header inclusion  
- Configuration options for enabling/disabling features
- Consistent code style with existing kernel code

This infrastructure provides a solid foundation for future USB development in Notux OS.