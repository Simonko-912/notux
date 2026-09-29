# Notux — Feature Ideas & Roadmap

## Already Built (this codebase)

- UEFI bootloader (PE32+ EFI app, loads kernel ELF)
- x86-64 kernel: GDT, IDT, ISR stubs, SYSCALL/SYSRET
- Physical memory manager (bitmap allocator)
- Virtual memory manager (4-level paging, higher-half kernel)
- Preemptive round-robin scheduler with OOM killer
- Process management: fork, exec, kill, wait
- VFS abstraction + NTFS driver
- Framebuffer driver + custom bitmap font renderer
- GUI compositor (basic)
- PS/2 keyboard + mouse driver
- TCP/IP network stack + NIC driver
- IPC: signals, pipes
- Service manager (background daemons)
- **nsh** shell with all requested commands
- **libnotux** — full standard library for Notux apps
- **POSIX compat layer** — compile Linux C programs with minimal changes
- **musl-notux** — full libc port for binary compatibility
- **SDL2-notux** — SDL2 port so games compile unmodified
- **opm** — online package manager (GitHub repo support)
- **pkgman** — local zip-based package installer (source + binary)
- **notux-gcc** — GCC wrapper with correct sysroot/includes
- **crt0** — C runtime startup for user programs

---

## Ideas to Add Next

### 1. `notedit` — built-in text editor
A terminal-based editor (think nano-style) using the custom font.
Line numbers, syntax highlighting for C/.sh files, search/replace.
Lives at `#/bin/notedit`, launched by the `edit` command.

### 2. `nfetch` — system info display
Like neofetch — shows Notux ASCII art logo, CPU, RAM, disk usage,
uptime, kernel version. Great first "wow" moment after booting.

### 3. Crash reporter
On unhandled exception/segfault, show a readable crash screen:
  - Process name + PID
  - Faulting instruction (RIP)
  - Register dump
  - Stack trace (walks RBP chain)
  - "Press R to reboot, K to kill process, D for debugger"

### 4. `ndb` — built-in kernel debugger
Breakpoints via INT3, single-step via RFLAGS.TF.
Commands: break, continue, step, print <addr>, backtrace.
Activates when a process crashes or via `ndb <pid>`.

### 5. GUI window manager
Move beyond raw framebuffer text — a tiling or floating window
manager. Each process gets a "window" region of the framebuffer.
Title bars, drag, resize, minimize. Draw using the custom font for
chrome, SDL2 or raw fb for content.

### 6. `nterm` — graphical terminal emulator
A window that runs nsh inside the GUI compositor.
Supports color (using color command), scrollback buffer,
multiple tabs.

### 7. Audio driver (PC speaker + simple sound card)
PC speaker: `#/dev/speaker` — write frequency/duration pairs.
Sound card (AC97 / HDA): PCM output, WAV player.
Add `nx_audio_play(buf, len, freq)` to libnotux.

### 8. USB driver stack
xHCI host controller → USB HID (keyboards, mice, gamepads).
USB mass storage → mount USB drives as NTFS/FAT32.

### 9. `nwifi` — network config tool
Scans for networks, connects with WPA2, shows signal strength.
Config stored in `#/etc/net.conf`.

### 10. Virtual filesystem extras
- `#/dev/` — device files (tty, null, zero, random, urandom, fb0)
- `#/sys/` — kernel stats (uptime, memory, processes)
- `#/proc/<pid>/` — per-process info (maps, status, cmdline)

### 11. `nscript` — lightweight scripting language
Inspired by Python but tiny — fits in 50 KB.
Variables, loops, functions, file I/O, basic net.
`.ns` files run directly: `./myscript.ns`

### 12. WASM runtime
Run WebAssembly binaries natively on Notux.
This means programs compiled for the web (WASM target) run
on Notux without modification — massive existing software catalog.
`wasmrun myprog.wasm`

### 13. Encrypted filesystem
LUKS-compatible volume encryption.
`#/bin/cryptsetup create <name> <device>`
Unlocked volumes mounted at `#/mnt/<name>`.

### 14. Multi-seat / multi-user GUI
Different users can log into different virtual terminals (VT1–VT6).
Switch with Ctrl+Alt+F1…F6. Each VT has its own shell + GUI.

### 15. `notux-pkg-builder` — package creation tool
Interactive wizard to create a MANIFEST.txt + zip package
from a directory. Auto-detects binaries, headers, services.
`pkgbuild --dir myapp/ --out myapp.zip`

### 16. `nman` — manual pages
Lightweight man-page viewer.
Pages stored in `#/usr/share/man/<cmd>.txt`.
All built-in commands have man pages.
`nman cd`, `nman opm`, etc.

### 17. journald-style logging
All kernel messages + service stdout → `#/var/log/notux.log`.
`logview` command tails the log with color by severity.
Services write to it via `nx_log(LEVEL, "message")`.

### 18. `nsync` — cloud file sync
syncs `#/usr/<user>/sync/` to a remote server via HTTPS.
Simple rsync-style protocol over TCP.
Config: `#/etc/nsync.conf` with server URL + credentials.

### 19. Power management
ACPI sleep (S3 suspend), battery status on laptops.
`powerctl suspend`, `powerctl shutdown`, `powerctl reboot`.
ACPI tables parsed from RSDP (already stored in BootInfo).

### 20. Virtualization guest additions
When running under QEMU/VirtualBox: virtio-net, virtio-blk,
shared clipboard, dynamic screen resize.
Makes development much smoother.

---

## Compatibility Stretch Goals

### Wine-Notux
Run Windows PE32+ executables on Notux.
Map Win32 API calls → Notux equivalents.
Scope: simple console programs first, GUI apps later.

### Linux ELF compatibility mode
Run unmodified Linux ELF binaries.
Implement a Linux syscall emulation layer (LKML subset).
Similar to FreeBSD's Linuxulator.
Would let existing pre-compiled Linux programs run directly.

### JVM port
OpenJDK HotSpot ported to Notux.
Java programs run with `java MyClass`.
Requires: threading, mmap, signal handling — all in progress.

### Python 3 port
CPython targeting Notux via musl-notux.
`opm install python3` → full Python interpreter on Notux.

---

## Hardware Targets

- **Primary**: x86-64 PC (QEMU, bare metal)
- **Planned**: Raspberry Pi 5 (AArch64 — needs a second arch/ tree)
- **Stretch**: RISC-V (with RISC-V UEFI)
