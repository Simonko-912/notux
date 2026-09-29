# Notux OS — Build, Run & Porting Guide

## Install Dependencies (Ubuntu 22.04 / 24.04)

```bash
sudo apt install \
    gcc clang lld nasm \
    parted mtools dosfstools ntfs-3g \
    qemu-system-x86 ovmf
```

## Build

```bash
make          # builds kernel + bootloader + 200 MiB bootable disk image
make check    # compile-check all sources without linking
make clean    # remove build/
```

Output: `build/notux.img` — 200 MiB GPT disk, ready to boot.

## Run in QEMU

```bash
make run          # SDL window + serial output on terminal
make run-log      # headless, serial saved to build/serial.log
```

Manual:
```bash
qemu-system-x86_64 \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive if=ide,format=raw,file=build/notux.img \
    -m 256M \
    -serial stdio \
    -no-reboot
```

## Write to USB (real hardware)

```bash
sudo dd if=build/notux.img of=/dev/sdX bs=4M status=progress && sync
```

Disable Secure Boot in UEFI firmware before booting.

---

## First Boot Behaviour

On first boot Notux:
1. Scans all ATA drives for GPT partitions
2. Checks each NTFS partition for `notux.cfg`
3. If none found, selects the available NTFS partition (auto if only one)
4. Installs the base directory layout:
   - `#/bin`, `#/etc`, `#/usr`, `#/var`, `#/tmp`, `#/svc`
   - `#/etc/admin.pass` (default password: `admin`)
   - `#/etc/opm/repos.conf` (default OPM repo)
   - `#/etc/motd`
   - `#/notux.cfg` (marks partition as Notux root)
5. On all subsequent boots, finds `notux.cfg` and mounts that partition as `#/`

If multiple NTFS partitions exist, a menu is shown to select which one.

---

## Path Compatibility

Notux uses `#/` as root instead of `/`. The kernel **automatically
translates** all POSIX paths at syscall time, so existing C programs
using standard paths just work:

| POSIX path         | Notux path          |
|--------------------|---------------------|
| `/`                | `#/`                |
| `/home/alice`      | `#/usr/alice`       |
| `/home`            | `#/usr`             |
| `/tmp`             | `#/tmp`             |
| `/etc`             | `#/etc`             |
| `/bin`, `/usr/bin` | `#/bin`             |
| `/lib`, `/usr/lib` | `#/usr/lib`         |
| `/var`             | `#/var`             |
| `/proc`            | `#/sys/proc`        |
| `/dev`             | `#/dev`             |
| `/root`            | `#/usr/admin`       |
| `/opt`             | `#/usr/opt`         |

No code changes needed for most programs. The translation happens
inside the kernel syscall layer transparently.

---

## Adding Your Font

Drop your `sysfont.h` into `kernel/drivers/gfx/fonts/sysfont.h`
and run `make`. The font is used for all screen output immediately.

Requirements for `sysfont.h`:
- Macro `SYSFONT_W` — glyph width in pixels
- Macro `SYSFONT_H` — glyph height in pixels
- Macro `SYSFONT_ASCENT`, `SYSFONT_DESCENT`
- Array `SYSFONT_glyphs[]` of `{ uint32_t cp; const uint8_t *data; }`
- Macro `SYSFONT_COUNT`
- Each glyph: `SYSFONT_H` rows × `ceil(SYSFONT_W/8)` bytes, MSB=left

---

## Porting C Programs to Notux

### Quickest method — no code changes needed:

```bash
# On Notux, in the shell:
gcc -compat-posix myprogram.c -o myprogram
```

The `-compat-posix` flag adds `#include <notux/compat.h>` automatically,
which provides `printf`, `malloc`, `FILE*`, `socket()`, `open()`, etc.

### Full porting header:

```c
#include <notux/compat.h>   // pulls in POSIX + path translation
```

This gives you:
- Full `<stdio.h>` (`printf`, `fopen`, `fread`, `fgets`, …)
- Full `<stdlib.h>` (`malloc`, `free`, `exit`, `getenv`, …)
- Full `<string.h>` (`strlen`, `strcpy`, `strdup`, …)
- Full `<math.h>` (`sqrt`, `sin`, `cos`, `pow`, …)
- BSD sockets (`socket`, `connect`, `send`, `recv`, …)
- POSIX file I/O (`open`, `read`, `write`, `stat`, `readdir`, …)
- Time (`time`, `gettimeofday`, `sleep`, `usleep`)
- Signals (`signal`, `raise`)
- Path utilities (`nx_path_to_notux`, `NX_HOME`, `NX_BIN`, …)

### Path macros for code clarity:

```c
#include <notux/path.h>

FILE *f = fopen(NX_ETC("myapp.conf"), "r");  // → "#/etc/myapp.conf"
FILE *g = fopen(NX_HOME("alice") "/docs/file.txt", "r");
// OR just use POSIX — kernel translates automatically:
FILE *h = fopen("/etc/myapp.conf", "r");     // works identically
```

### What doesn't work (yet):
- `pthread_create` — use `nx_fork()` instead
- `mmap` with file backing — anonymous mmap works
- `epoll` / `inotify` / `signalfd` — return `ENOSYS`
- Dynamic linking (`.so`) — everything is statically linked

---

## Source Layout

```
boot/                   UEFI PE32+ bootloader
  efi_main.c            Entry point, ELF loader, GOP framebuffer setup
  efi.h                 Verified UEFI struct definitions (all offsets tested)
  boot_info.h           BootInfo struct shared with kernel

kernel/
  main.c                kmain() — full init sequence with klog() at each step
  kserial.h/c           COM1 serial debug output + klog() (screen+serial)
  linker.ld             Single PT_LOAD, loads at physical 0x100000

  arch/x86_64/
    entry.asm           _start: sets kernel stack, calls kmain
    gdt.c               GDT with TSS (ring 0/3 segments)
    idt.c               IDT: 256 interrupt gate setup
    isr_stubs.asm       ISR stubs: push regs → isr_dispatch → pop → iretq
    isr.c               Exception/IRQ C dispatcher
    pit.c               PIT 8253 @ 100 Hz → drives scheduler
    syscall.c           SYSCALL/SYSRET MSRs, 60+ syscall handlers
    syscall_entry.asm   Ring-3 → ring-0 trampoline

  mm/
    pmm.c               Bitmap physical memory manager (4 GiB max = 128 KB bitmap)
    vmm.c               4-level page tables, clone, free
    heap.c              Slab allocator (16–2048 B) + large page allocator

  proc/
    process.c           Fork, exec, kill, cleanup, proc_init_main
    scheduler.c         Round-robin preemptive scheduler, OOM killer

  fs/
    vfs.c               VFS mount table, open/read/write/stat/dir
    vfs.h               VfsDriver interface, VfsMount, VfsDir
    pathconv.c/h        POSIX ↔ Notux path translation (kernel side)
    ntfs/ntfs.c         NTFS read driver (MFT, run lists, index B-tree)
    rootmgr.c/h         Partition scanner, first-boot installer, partition picker

  drivers/
    gfx/font.c          Bitmap font renderer onto linear framebuffer
    gfx/fonts/sysfont.c Your font wrapper (replace sysfont.h to change font)
    disk/ata.c          ATA PIO driver (LBA48, primary+secondary controllers)
    disk/gpt.c          GPT partition table parser (all drives)
    input/ps2.h         PS/2 keyboard stub
    net/tcp.h           Network stub (TCP/UDP API ready)

  stubs.c               Stub implementations for unfinished subsystems

libnotux/include/notux/
  libc.h                nx_* standard library API
  syscalls.h            All syscall numbers (Linux-compatible where possible)
  path.h                Userspace path translation utilities + macros
  compat.h              Full POSIX porting header (include to port Linux apps)
  compat_posix.h        POSIX type/function mappings
  fs.h net.h color.h    Thin userspace wrappers
```

---

## Serial Debug

Every kernel subsystem logs to both screen and COM1 serial.
In QEMU: `make run` prints serial to your terminal.
Log file:  `make run-log` saves to `build/serial.log`.

Example boot log (successful first-time boot):
```
=== NOTUX BOOTLOADER ===
Notux 0.1.0-dev
PIC/PIT init...
VFS init...
Root manager...
ATA: QEMU HARDDISK
rootmgr: auto-selecting single NTFS partition
Installing Notux on partition...
  mkdir #/bin ... (etc)
  created notux.cfg
Installation complete!
Scheduler init...
Spawning init...
Idle loop.
```
