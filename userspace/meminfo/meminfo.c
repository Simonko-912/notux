/*
 * meminfo.c — Notux memory/usage summary.
 *
 * Reports what the userspace API can observe: process identity,
 * uptime, terminal geometry, and the size of every app in #/bin.
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/proc.h>
#include <notux/terminal.h>
#include <notux/color.h>

int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv; (void)envp;

    nx_cprintf(color_rgb(0, 240, 220), 0, "Notux memory overview\n");
    nx_cprintf(color_rgb(255, 255, 255), 0, "  pid        %d\n", nx_getpid());
    nx_cprintf(color_rgb(255, 255, 255), 0, "  ppid       %d\n", nx_getppid());
    nx_cprintf(color_rgb(255, 255, 255), 0, "  uid        %u\n", nx_getuid());
    nx_cprintf(color_rgb(255, 255, 255), 0, "  uptime     %llu ms\n",
               (unsigned long long)nx_uptime_ms());
    nx_cprintf(color_rgb(255, 255, 255), 0, "  terminal   %d x %d\n",
               nx_term_rows(), nx_term_cols());

    char cwd[256];
    if (nx_getcwd(cwd, sizeof(cwd)))
        nx_cprintf(color_rgb(255, 255, 255), 0, "  cwd        %s\n", cwd);

    uint64_t total_app_bytes = 0;
    uint64_t total_app_pages = 0;
    void *dir = nx_opendir("#/bin");
    if (dir) {
        char nm[256];
        while (nx_readdir(dir, nm, NULL) == 0) {
            char p[320];
            NxFileInfo fi;
            nx_snprintf(p, sizeof(p), "#/bin/%s", nm);
            if (nx_stat(p, &fi) == 0 && (fi.flags & NX_FS_DIR) == 0) {
                total_app_bytes += fi.size;
                total_app_pages += (fi.size + 4095) / 4096;
            }
        }
        nx_closedir(dir);
    }

    nx_cprintf(color_rgb(0, 240, 220), 0, "  #/bin size: %u bytes (%u KiB, %u pages)\n",
               (unsigned)total_app_bytes,
               (unsigned)(total_app_bytes / 1024),
               (unsigned)total_app_pages);

    nx_cprintf(color_rgb(80, 255, 120), 0, "  (kernel memory statistics require a kernel API)\n");
    return 0;
}