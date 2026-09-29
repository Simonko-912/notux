# Notux OS - Build System and Application Installation

## Overview

This document describes how to build and install user applications in Notux OS, including the automated installation process that places applications into the NTFS partition.

## Building Applications

### Individual App Builds
Each application has its own Makefile in `userspace/<appname>/`:

```bash
cd userspace/hello
make clean && make install
```

### Bulk Application Build
Use the enhanced build script:
```bash
./build_apps.sh
```

This will automatically build all applications and place them in `build/bin/`.

## Application Installation Process

The build system follows this installation flow:

1. **Compile Applications** - All user applications are compiled with proper flags
2. **Install to Build Directory** - Binaries placed in `build/bin/`  
3. **Automated NTFS Integration** - During full build, applications are copied to the NTFS partition

## NTFS Partition Structure

Applications are installed to:
```
#/bin/                 # Executable binaries (all apps go here)
#/usr/                 # User home directories
#/etc/                 # Configuration files  
#/var/                 # Variable data
```

## Automated Installation Script

During the full build process, the system uses a script that:

1. **Mounts NTFS partition** if not already mounted
2. **Creates directory structure** in NTFS root (`#/bin`, `#/etc`, etc.)
3. **Copies applications** from `build/bin/` to `#/bin/`
4. **Sets proper permissions**
5. **Creates configuration files**

## Example Installation Process

```bash
# 1. Build all applications  
./build_apps.sh

# 2. Build full system
make

# 3. Applications are automatically installed to NTFS partition
```

## Application Directory Structure

Each application directory contains:
- `*.c` - Source code files  
- `Makefile` - Build instructions
- `README.md` - Documentation (if applicable)

## System Integration

The shell automatically discovers applications in `#/bin/`:

```bash
# From nsh shell
hello                    # Runs hello app from #/bin/hello
calc                     # Runs calculator from #/bin/calc  
filemgr                  # Runs file manager from #/bin/filemgr
editor                   # Runs text editor from #/bin/editor
```

## Build Dependencies

To build applications, you need:
```bash
sudo apt install gcc clang lld nasm parted mtools \
                 dosfstools ntfs-3g qemu-system-x86 ovmf
```

## Testing Applications

After building and running:
```bash
# From the Notux OS shell
hello                    # Test basic hello app
calc                     # Test calculator  
filemgr                  # Test file manager
editor                   # Test text editor
```

## Configuration Files

Configuration files are placed in:
- `#/etc/` - System configuration
- `#/usr/<user>/` - User-specific data  

## Future Enhancements

The build system can be extended to support:
- Package managers (like pkgman)
- Automatic dependency resolution  
- Version control integration
- Cross-compilation for different architectures