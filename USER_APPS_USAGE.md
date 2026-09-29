# Notux OS - User Applications Usage Guide

## Overview

This guide explains how to build and install user applications in Notux OS, including the complete workflow from building to installation on the NTFS partition.

## Complete Build Process

### 1. Building the Entire System with User Applications

```bash
# Build everything: kernel + user applications + install apps to NTFS
make apps

# Or build just the system (without user apps)
make all

# Run in QEMU 
make run
```

### 2. Individual Application Building

```bash
# Build all user applications manually
make build-user-apps

# Install applications to NTFS partition  
make install-user-apps

# Clean application builds
make clean-user-apps
```

## Application Directory Structure

All user applications are installed to:
```
#/bin/                 # Executable binaries (all apps go here)
#/usr/                 # User home directories
#/etc/                 # Configuration files  
#/var/                 # Variable data
```

## Available Applications

### 1. Hello World (`hello`)
```bash
# Run the hello application
hello
```

### 2. Calculator (`calc`) 
```bash
# Run calculator
calc
# Or with help
calc --help
```

### 3. File Manager (`filemgr`)
```bash
# Run file manager
filemgr
# List directory contents
filemgr dir
# Show current path  
filemgr pwd
```

### 4. Text Editor (`editor`)
```bash
# Edit a file
editor myfile.txt
# Add new lines with '+'
# Save and quit with 'wq' 
```

### 5. Test Application (`testapp`)
```bash
# Run test application
testapp
```

## Build System Integration

### Makefile Targets

| Target | Description |
|--------|-------------|
| `make all` | Build kernel and bootloader only |
| `make apps` | Build entire system + install user apps to NTFS |
| `make build-user-apps` | Build all user applications only |
| `make install-user-apps` | Install apps to NTFS partition |
| `make clean-user-apps` | Clean application build directory |
| `make run` | Run in QEMU with display |
| `make run-log` | Run headless with serial log |

## Installation Process

### Automatic Installation Flow

1. **Build Applications**: All user applications compiled to `build/bin/`
2. **Mount NTFS**: System mounts the NTFS data partition
3. **Create Directories**: Ensures `/bin`, `/etc`, `/usr` directories exist  
4. **Copy Binaries**: Copies all apps from `build/bin/` to `/bin/` in NTFS
5. **Set Permissions**: Makes apps executable (755)
6. **Install Configs**: Creates default configuration files if needed
7. **Unmount**: Safely unmounts the NTFS partition

### Manual Installation Steps

```bash
# Build applications
make build-user-apps

# Install to NTFS (if you have access to the image)
mkdir -p build/mnt
mount -o loop build/data.ntfs build/mnt
cp build/bin/* build/mnt/bin/
umount build/mnt
```

## Application Development

### Creating New Applications

1. **Create directory**: `userspace/myapp/`
2. **Write source**: `myapp.c` with proper headers  
3. **Add Makefile**: Include build instructions
4. **Test**: Run `make apps` to rebuild and install

### Sample Makefile Template

```makefile
# Makefile for myapp

CC = gcc
CFLAGS = -I../../libnotux/include -Wall -Wextra -O2 -ffreestanding
LDFLAGS = -L../../libnotux -lnotux -nostdlib -static

TARGET = myapp
SOURCE = myapp.c

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) ../../build/bin/
```

## Testing in Notux OS

After running `make apps`, you can test all applications from the Notux shell:

```bash
# From nsh shell
hello                    # Test basic hello app
calc                     # Test calculator  
filemgr                  # Test file manager
editor                   # Test text editor
testapp                  # Test utility app
```

## Troubleshooting

### Common Issues

1. **"NTFS image not found"**
   ```bash
   # Ensure you've built the system first
   make all
   make apps
   ```

2. **"mtools not found"** 
   ```bash
   sudo apt install mtools
   ```

3. **Permission denied during installation**
   ```bash
   # Run with proper permissions or check mount options
   sudo make apps
   ```

### Debugging Steps

1. **Check build directory structure**
   ```bash
   ls -la build/bin/
   ```

2. **Verify NTFS image exists**  
   ```bash
   ls -la build/data.ntfs
   ```

3. **Test individual app compilation**
   ```bash
   cd userspace/hello && make install
   ```

## Future Enhancements

### Planned Features

1. **Package Management Integration**
   - Install apps via `pkgman`
   - Dependency resolution for applications

2. **Advanced Build System** 
   - Cross-compilation support
   - Application versioning

3. **Application Repository**
   - Online package repository  
   - Application search and download

## Example Usage Session

```bash
# 1. Build the complete system with apps
make apps

# 2. Run in QEMU to test
make run

# 3. In Notux OS shell, try applications:
hello
calc 5 + 3 * 2
filemgr dir
editor myfile.txt
```

## System Integration

All applications are automatically integrated into the Notux OS:

- **Path Resolution**: Applications in `#/bin/` are executable directly
- **Error Handling**: Proper error reporting for missing applications  
- **Permission Model**: Respects existing user and admin permissions
- **File I/O**: Uses standard system calls for file operations

The complete system provides a robust foundation for running user applications while maintaining the security and stability of the core Notux OS.