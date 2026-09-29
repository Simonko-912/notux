/*
 * ps.c — Notux process overview.
 *
 * The kernel exposes no live process table to users yet, so besides
 * this process's identity, we report the applications installed in
 * #/bin (the set the shell can spawn).
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/proc.h>
#include <notux/color.h>

int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv; (void)envp;

    nx_cprintf(color_rgb(0, 240, 220), 0,
              "PID  PPID  UID  NAME\n");
    nx_cprintf(color_rgb(255, 255, 255), 0,
              "%3d  %3d   %u   nsh (this shell)\n",
              nx_getpid(), nx_getppid(), nx_getuid());

    nx_cprintf(color_rgb(0, 240, 220), 0,
              "\nInstalled programs in #/bin (spawnable):\n");
    nx_cprintf(color_rgb(0, 240, 220), 0,
              "%-14s %10s  %s\n", "SIZE", "NAME");
    void *dir = nx_opendir("#/bin");
    int count = 0;
    if (dir) {
        char nm[256];
        while (nx_readdir(dir, nm, NULL) == 0) {
            NxFileInfo fi;
            nx_stat("#/bin/", &fi);
            char p[320];
            nx_snprintf(p, sizeof(p), "#/bin/%s", nm);
            if (nx_stat(p, &fi) == 0 && (fi.flags & NX_FS_DIR) == 0) {
                nx_cprintf(color_rgb(200, 200, 210), 0,
                           "%-14u %10s\n", (unsigned)fi.size, nm);
                count++;
            }
        }
        nx_closedir(dir);
    }
    char nbuf[24];
    nx_sprintf(nbuf, "%d", count);
    nx_puts(nbuf);
    nx_puts(" program(s) installed.\n");
    return 0;
}