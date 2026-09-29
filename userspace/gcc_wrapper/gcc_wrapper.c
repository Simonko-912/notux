/*
 * gcc_wrapper.c — notux-side wrapper for the PC-side toolchain.
 *
 * On the PC, Notux programs are built with the host gcc + libnotux
 * (build_libnotux.sh / make build-user-apps).  This program exists in
 * the image so that 'gcc' resolves inside nsh; it documents the real
 * build path and reports the local toolchain status.
 *
 *   gcc_wrapper --version   toolchain identity
 *   gcc_wrapper <anything>  prints usage and returns 1
 */

#include <notux/libc.h>
#include <notux/proc.h>
#include <notux/fs.h>
#include <notux/color.h>

int main(int argc, char **argv, char **envp) {
    (void)envp;

    nx_cprintf(color_rgb(255, 220, 80), 0,
               "Notux gcc wrapper v1.0\n");
    if (argc > 1 && (nx_strcmp(argv[1], "--version") == 0 ||
                     nx_strcmp(argv[1], "-v") == 0)) {
        nx_cprintf(color_rgb(0, 240, 220), 0,
                   "  target:      x86_64-pc-notux\n");
        nx_cprintf(color_rgb(255, 255, 255), 0,
                   "  libc:        libnotux (static, crt0.ld)\n");
        nx_cprintf(color_rgb(255, 255, 255), 0,
                   "  model:       -mcmodel=large -no-pie -ffreestanding\n");
        return 0;
    }

    nx_cprintf(color_rgb(255, 255, 255), 0,
               "Building happens on the host (PC):\n"
               "  cd /root/opencode/notux4 && ./build_libnotux.sh && make build-user-apps\n"
               "  then: make install-user-apps  (copies binaries to #/bin)\n");
    if (argc > 1) {
        int fd = nx_open("#/etc/gcc_wrapper", NX_O_WRONLY | NX_O_CREATE | NX_O_TRUNC);
        if (fd >= 0) {
            for (int i = 1; i < argc; i++) {
                nx_write(fd, argv[i], nx_strlen(argv[i]));
                if (i + 1 < argc) nx_write(fd, " ", 1);
            }
            nx_write(fd, "\n", 1);
            nx_close(fd);
            nx_cprintf(color_rgb(80, 255, 120), 0,
                       "(arguments logged to #/etc/gcc_wrapper)\n");
        }
    }
    return 1;
}