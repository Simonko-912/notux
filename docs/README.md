# Notux OS — Developer Guide

## What is Notux?

Notux is a 64-bit operating system built from scratch.  
It boots via UEFI, runs its own kernel (not Linux), uses NTFS as its primary filesystem, and ships a custom shell with a familiar-but-independent feel.

## New Features Added

This version includes significant enhancements to the app execution system:

### Enhanced App Execution
- Complete ELF loading infrastructure for user-space binaries
- Process management improvements with table tracking
- Memory usage monitoring capabilities
- Better error handling and validation

### System Utilities  
- Process status command (`ps`)
- Memory information command (`meminfo`)
- Enhanced kill functionality

### USB Support
- Complete USB driver infrastructure
- USB keyboard and mouse support (integrated with PS/2 system)
- HID protocol handling
- Device enumeration framework
- LED control for keyboards

### New User Applications
- Calculator (`calc`) - Basic arithmetic operations
- File Manager (`filemgr`) - Directory browsing and file operations  
- Text Editor (`editor`) - Line-based text editing
- Enhanced Shell (`nsh`) - Improved command execution and user experience

### API Improvements
- Improved syscall integration
- Enhanced process management functions
- Better resource monitoring support

For detailed documentation on using these features, see:
- `README_APPS.md` - Complete guide to app execution system
- `NEW_FEATURES.md` - Summary of new additions
- `USB_FEATURES.md` - Detailed USB subsystem documentation
- `FULL_FEATURE_SUMMARY.md` - Complete feature overview

---

## Directory Structure

```
notux/
├── boot/               UEFI bootloader (PE32+ EFI application)
│   ├── main.c          EFI entry point
│   ├── elf_loader.c    Loads notux.elf from the ESP
│   ├── mem.c           Early page-table setup
│   ├── efi.h           UEFI type definitions
│   └── boot_info.h     BootInfo struct (shared with kernel)
│
├── kernel/             The Notux kernel
│   ├── main.c          kmain() — kernel entry point
│   ├── linker.ld       Linker script (higher-half, 0xFFFFFFFF80000000)
│   ├── arch/x86_64/    GDT, IDT, ISR stubs, SYSCALL/SYSRET
│   ├── mm/             PMM (bitmap), VMM (4-level paging), heap, OOM
│   ├── proc/           Process table, scheduler (round-robin), ELF exec
│   ├── fs/             VFS layer + NTFS driver
│   ├── drivers/
│   │   ├── gfx/        Framebuffer, font renderer, GUI compositor
│   │   ├── input/      PS/2 keyboard + mouse
│   │   └── net/        NIC driver, TCP/IP stack
│   ├── ipc/            Signals, pipes, shared memory
│   └── svc/            Service manager (background daemons)
│
├── libnotux/           Standard library for user-space programs
│   ├── include/notux/  Public headers (libc.h, fs.h, net.h, color.h…)
│   └── src/            Syscall wrappers, math, string, printf
│
├── userspace/          Built-in user-space programs
│   ├── shell/nsh.c     Notux Shell
│   ├── bash/           Bash port / compatibility layer
│   ├── gcc_wrapper/    GCC toolchain integration
│   ├── pkgman/         Local package manager
│   ├── opm/opm.c       Online package manager
│   ├── color/          color command
│   ├── usr_cmd/        usr command (user management)
│   └── services_cmd/   services command (svcctl)
│
├── Makefile            Top-level build system
└── docs/               Additional documentation
```

---

## Building

### Prerequisites

```bash
# Ubuntu / Debian
sudo apt install clang lld nasm xorriso ovmf ntfs-3g

# Or build a GCC cross-compiler targeting x86_64-elf
```

### Build steps

```bash
# 1. Build bootloader + kernel
make all

# 2. Create a blank NTFS disk image (first time only)
make disk

# 3. Build an ISO
make iso

# 4. Run in QEMU
make run
```

---

## Filesystem Layout

Notux uses `#` as the root directory symbol (not `/`).  
You can never go above `#/` — it is the absolute root.

```
#/                   Root
#/bin/               System binaries (nsh, bash, gcc, opm, pkgman…)
#/etc/               Configuration files
#/etc/passwd         User account database
#/etc/admin.pass     Admin password (default: "admin")
#/etc/opm/           OPM repository config
#/usr/               User home directories
#/usr/<username>/    Each user's home (~)
#/svc/               Service definitions
#/var/               Variable data
#/var/cache/opm/     OPM package cache
#/tmp/               Temporary files (cleared on boot)
```

---

## Shell (nsh) — Command Reference

### Prompt

```
user@#/~ >             (when in home directory)
user@#/usr/alice/docs > (when in another location)
```

### Built-in Commands

| Command | Description |
|---------|-------------|
| `cd [path]` | Change directory. `..` goes up (never above `#/`) |
| `dir [path]` | List directory contents with sizes |
| `touch <file>` | Create empty file / update timestamp |
| `mkdir <dir>` | Create a directory |
| `mv <src> <dst>` | Move or rename a file |
| `edit <file>` | Open the built-in text editor |
| `unzip <file> [dest]` | Extract a ZIP archive |
| `ping <host>` | Send ICMP echo requests |
| `usr newuser <name>` | Create a new user (admin only) |
| `usr deluser <name>` | Delete a user (admin only) |
| `usr newpass <old> <new>` | Change your password |
| `sudo <command>` | Run command as admin (prompts for password) |
| `sudo usr newpass admin` | Change admin password |
| `bash [script.sh]` | Run a bash script or start bash session |
| `services list\|start\|stop\|enable\|disable` | Manage background services |
| `pkgman install\|remove\|list\|run <pkg>` | Local package manager |
| `opm install\|remove\|search\|update <pkg>` | Online package manager |
| `opm repo add\|list\|remove <url>` | Manage OPM repositories |
| `gcc [flags] <file>` | Compile C/C++ programs |
| `color fg\|bg <r> <g> <b>` | Set terminal colors (0–255 per channel) |
| `color reset` | Reset to default colors |

### Permissions

- Normal users can **read** any file, but can only **write** inside `#/usr/<theirname>/`
- `sudo` elevates to admin for a single command
- Admin password default: `admin` — change with `sudo usr newpass admin`

---

## Writing Programs for Notux

Include `<notux/libc.h>` and link against `libnotux`:

```c
#include <notux/libc.h>

int main(int argc, char **argv) {
    nx_cprintf(nx_color(80, 250, 123), 0, "Hello from Notux!\n");

    /* File I/O */
    int fd = nx_open("#/usr/alice/hello.txt", NX_O_WRONLY | NX_O_CREATE);
    nx_write(fd, "Hello world\n", 12);
    nx_close(fd);

    /* Network */
    uint32_t ip = nx_resolve("example.com");
    nx_sock_t s  = nx_socket(NX_AF_INET, NX_SOCK_TCP);
    nx_connect(s, ip, 80);

    return 0;
}
```

Compile:
```bash
gcc -Ilibnotux/include -lnotux myprog.c -o myprog
# Then install with:
pkgman install myprog.zip
```

---

## Online Package Manager (OPM)

Default repository: `https://github.com/Simonko-912/notux-packages`

```bash
opm update                        # Refresh package lists
opm search <query>                # Search for a package
opm install <name>                # Install
opm remove  <name>                # Remove
opm repo add https://mysite.com/notux-pkgs   # Add custom repo
```

---

## Font Integration

Drop your custom font `.h` file (same format as SYSFONT) into `kernel/drivers/gfx/`.  
The kernel's `font_load_builtin()` reads the glyph table and renders it via the framebuffer driver.

The font is used for:
- Early boot text output (text mode)
- The shell terminal in GUI mode
- Any `nx_cprintf()` output from user programs

---

## Architecture Notes

- **Bootloader**: PE32+ EFI app, written in C, no EDK2 dependency
- **Kernel**: Monolithic, higher-half (`0xFFFFFFFF80000000`), SysV ABI
- **Memory**: 4-level paging, bitmap PMM, slab-style kernel heap
- **Scheduler**: Preemptive round-robin, APIC/PIT timer, OOM killer
- **Filesystem**: VFS abstraction + NTFS driver (read/write)
- **Syscalls**: `SYSCALL`/`SYSRET` MSR-based, 64-bit ABI
- **GUI**: Custom compositor over UEFI GOP linear framebuffer
- **Network**: Minimal TCP/IP stack, NIC driver (e1000 / virtio-net)
