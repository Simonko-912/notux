/*
 * Notux OS — musl-notux port
 * libnotux/musl_notux/README_PORT.c  (build instructions as a C comment file)
 *
 * musl-notux is a port of musl libc (https://musl.libc.org) targeting
 * the Notux syscall ABI.  It gives programs compiled for Linux full
 * libc compatibility — <stdio.h>, <stdlib.h>, <pthread.h>, math, etc.
 *
 * This means you can take a program written for Linux and compile it
 * for Notux with minimal or zero changes.
 *
 * ── How it works ──────────────────────────────────────────────
 *
 * musl is already a freestanding libc — it just needs its syscall
 * layer replaced.  The Notux syscall numbers intentionally match
 * Linux x86-64 for the common calls (read, write, open, mmap, etc.)
 * so the port is mostly:
 *
 *   1. Replace musl's arch/x86_64/syscall_arch.h with one that
 *      issues Notux syscalls (which are already compatible).
 *   2. Add #/  as the path root (musl's vfs_root = "#/").
 *   3. Stub out Linux-specific calls Notux doesn't have
 *      (epoll, inotify, etc.).
 *   4. Replace musl's thread implementation with Notux nx_proc forks
 *      (simple but functional).
 *
 * ── Building musl-notux ────────────────────────────────────────
 *
 *   # 1. Clone musl
 *   git clone https://git.musl-libc.org/cgit/musl
 *   cd musl
 *
 *   # 2. Apply the Notux patch
 *   patch -p1 < ../notux/libnotux/musl_notux/notux_musl.patch
 *
 *   # 3. Configure for Notux target
 *   ./configure \
 *       --prefix=#/usr \
 *       --target=x86_64-notux \
 *       --with-sysroot=#/usr/lib/notux-sysroot \
 *       CFLAGS="-D__NOTUX__ -I../libnotux/include" \
 *       CC=clang \
 *       LD=ld.lld
 *
 *   # 4. Build and install into sysroot
 *   make -j4
 *   make install DESTDIR=../build/sysroot
 *
 * ── Using musl-notux in programs ──────────────────────────────
 *
 *   # Option A: notux-gcc auto-picks it with -lmusl_notux
 *   notux-gcc -lmusl_notux my_linux_program.c -o my_program
 *
 *   # Option B: explicit sysroot
 *   gcc -target x86_64-notux-elf \
 *       --sysroot=#/usr/lib/notux-sysroot \
 *       -D__NOTUX__ \
 *       my_linux_program.c -o my_program
 *
 * ── Syscall compatibility table ───────────────────────────────
 *
 *   Notux SYS_* numbers match Linux x86-64 for:
 *     read(0), write(1), open(2), close(3), stat(4), fstat(5),
 *     lseek(8), mmap(9), munmap(11), brk(12), fork(57), exec*(59),
 *     exit(60), wait4(61), kill(62), getcwd(79), chdir(80),
 *     mkdir(83), unlink(87), rename(82), socket(41), connect(42),
 *     accept(43), send(44), recv(45), bind(49), listen(50), ...
 *
 *   Notux-only extensions (no Linux equivalent):
 *     SYS_TERM_SETFG (400), SYS_TERM_SETBG (401),
 *     SYS_TERM_CLEAR (402), SYS_TERM_MOVE  (403),
 *     SYS_UPTIME     (410), SYS_GETTIME    (411),
 *     SYS_PING       (420), SYS_RESOLVE    (421)
 *
 * ── What works out of the box with musl-notux ─────────────────
 *
 *   ✓ printf / fprintf / sprintf family
 *   ✓ malloc / free / realloc
 *   ✓ fopen / fread / fwrite / fclose
 *   ✓ fork / exec / wait
 *   ✓ sockets (TCP/UDP — BSD API)
 *   ✓ math.h (full)
 *   ✓ string.h (full)
 *   ✓ Most of POSIX.1-2017
 *   ✓ C11 and C17 standard library
 *   ~ pthreads (works via nx_proc, limited to 64 threads)
 *   ~ mmap with file backing (anonymous mmap only, file-backed TODO)
 *   ✗ epoll / inotify / signalfd (stub, returns ENOSYS)
 *   ✗ /proc / /sys filesystem (Notux uses #/sys instead)
 *
 * ── Example: compile an unmodified Linux program ──────────────
 *
 *   // hello.c — standard POSIX C, no changes
 *   #include <stdio.h>
 *   #include <stdlib.h>
 *   int main() {
 *       printf("Hello from Notux!\n");
 *       return 0;
 *   }
 *
 *   // Compile on Notux:
 *   gcc -lmusl_notux hello.c -o hello
 *   ./hello
 *   // Hello from Notux!
 *
 * ── Example: a more complex program with files + network ──────
 *
 *   // No #includes changed, no code changed
 *   #include <stdio.h>
 *   #include <string.h>
 *   #include <sys/socket.h>
 *   #include <netinet/in.h>
 *   #include <arpa/inet.h>
 *
 *   int main() {
 *       // File I/O works normally
 *       FILE *f = fopen("#/usr/myuser/test.txt", "w");
 *       fprintf(f, "written from a Linux-style program\n");
 *       fclose(f);
 *
 *       // Note: on Notux, paths start with #/ not /
 *       // For portability, check __NOTUX__ and adjust paths
 *       return 0;
 *   }
 *
 * ── Path portability tip ───────────────────────────────────────
 *
 *   The only thing most programs need changing is absolute paths.
 *   On Linux: /home/user/  →  On Notux: #/usr/user/
 *   On Linux: /tmp/        →  On Notux: #/tmp/
 *   On Linux: /etc/        →  On Notux: #/etc/
 *
 *   You can add a thin shim for full portability:
 *
 *   #ifdef __NOTUX__
 *     #define NOTUX_PATH(p) ("#" p)   // "#" + "/etc/..." = "#/etc/..."
 *   #else
 *     #define NOTUX_PATH(p) (p)
 *   #endif
 *
 *   FILE *f = fopen(NOTUX_PATH("/etc/config.txt"), "r");
 */

/* This file is documentation — nothing to compile. */
void musl_notux_readme(void) {}
