# Notux OS

Notux is a modern, lightweight operating system designed for educational purposes and experimentation with kernel development concepts.

## Features

- 64-bit x86_64 architecture
- ELF loading infrastructure for user processes
- USB driver support (keyboard/mouse)
- Process management with table tracking
- NTFS filesystem integration 
- Build system automation for installing apps to NTFS partition
- Shell command execution and process spawning

## Building the System

### Prerequisites

Install required dependencies:
```bash
sudo apt install gcc clang lld nasm parted mtools \
                 dosfstools ntfs-3g qemu-system-x86 ovmf
```

### Build Commands

```bash
# Build everything (kernel + user applications)
make all

# Build and install user applications to NTFS partition
make apps

# Run in QEMU with display
make run

# Run headless with serial logging
make run-log

# Compile-check every source file
make check

# Clean build artifacts
make clean
```

## System Architecture

### Kernel Components
- `kernel/proc/` - Process management and ELF loading
- `kernel/drivers/usb/` - USB keyboard/mouse driver framework  
- `kernel/fs/ntfs/` - NTFS filesystem integration
- `kernel/mm/` - Memory management (PMM, VMM, heap)

### User Applications
- `userspace/hello/` - Hello world application
- `userspace/calc/` - Calculator application
- `userspace/filemgr/` - File manager application
- `userspace/editor/` - Text editor application
- `userspace/testapp/` - Test utility application

## Directory Structure

```
.
├── kernel/           # Kernel source code
├── userspace/        # User applications
├── libnotux/         # Standard library for user apps
├── boot/             # UEFI bootloader
├── build/            # Build output directory
└── Makefile          # Main build system
```

## How to Build the libnotux Library

The `libnotux` library is built automatically as part of the main build process, but if you need to manually build it:

```bash
cd libnotux
make clean && make all
```

If you encounter linking issues with the `libnotux.a` file:
1. Ensure you have built the kernel first (`make all`)
2. The system will automatically create a minimal `libnotux.a` in `build/lib/`
3. User applications are linked against this library

## Running Applications

After building with `make apps`, applications are installed to the NTFS partition and can be executed by:
1. Booting the system in QEMU (`make run`)
2. Using the built-in shell or application launcher
3. Direct execution from the NTFS filesystem

## Development

### Adding New Applications

To add a new user application:
1. Create a directory in `userspace/` with your app name
2. Add your source files (e.g., `main.c`)
3. Create a Makefile that follows the pattern of existing apps
4. Add your app to the `USER_APPS` list in `userapps.mk`

### Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Commit changes (`git commit -am 'Add new feature'`)
4. Push to the branch (`git push origin feature/my-feature`)
5. Create a Pull Request

## License

MIT License - see LICENSE file for details.