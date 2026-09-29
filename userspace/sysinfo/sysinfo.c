/*
 * sysinfo.c — compact system summary.
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/proc.h>
#include <notux/terminal.h>
#include <notux/color.h>

int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv; (void)envp;

    nx_cprintf(color_rgb(0, 240, 220), 0, "Notux x86_64\n");
    nx_cprintf(color_rgb(255, 255, 255), 0, "pid:       %d\n", nx_getpid());
    nx_cprintf(color_rgb(255, 255, 255), 0, "ppid:      %d\n", nx_getppid());
    nx_cprintf(color_rgb(255, 255, 255), 0, "uid:       %u\n", nx_getuid());
    nx_cprintf(color_rgb(255, 255, 255), 0, "uptime:    %llu ms\n",
               (unsigned long long)nx_uptime_ms());
    nx_cprintf(color_rgb(255, 255, 255), 0, "terminal:  %d x %d\n",
               nx_term_rows(), nx_term_cols());

    char cwd[256];
    if (nx_getcwd(cwd, sizeof(cwd)))
        nx_cprintf(color_rgb(255, 255, 255), 0, "cwd:       %s\n", cwd);

    NxTime t;
    nx_gettime(&t);
    if (t.year > 2000)
        nx_cprintf(color_rgb(255, 255, 255), 0, "date:      %04u-%02u-%02u %02u:%02u:%02u\n",
                   t.year, t.month, t.day, t.hour, t.minute, t.second);
    return 0;
}
