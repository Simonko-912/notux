/*
 * hello.c — Notux ring-3 demonstration program.
 *
 * Exercises the full libnotux surface:
 *   - colored output / cprintf
 *   - argc/argv/envp from the user-stack frame
 *   - heap allocation
 *   - string / math helpers
 *   - filesystem and process probes
 */

#include <notux/libc.h>
#include <notux/proc.h>
#include <notux/fs.h>
#include <notux/terminal.h>
#include <notux/color.h>

#define WHITE  color_rgb(255, 255, 255)
#define CYAN   color_rgb(0,   240, 220)
#define GREEN  color_rgb(80,  255, 120)
#define YELLOW color_rgb(255, 220, 80)
#define RED    color_rgb(255, 90,  90)

int main(int argc, char **argv, char **envp) {
    nx_cprintf(CYAN, 0, "┌───────────────────────────────────────────────┐\n");
    nx_cprintf(CYAN, 0, "│        Hello from Notux ring 3!               │\n");
    nx_cprintf(CYAN, 0, "└───────────────────────────────────────────────┘\n\n");

    /* ── argc / argv / envp ─────────────────────────────────── */
    nx_cprintf(YELLOW, 0, "argv: %d argument(s)\n", argc);
    for (int i = 0; i < argc; i++)
        nx_cprintf(WHITE, 0, "  argv[%d] = \"%s\"\n", i, argv[i]);
    nx_cprintf(YELLOW, 0, "environment:\n");
    if (envp && envp[0]) {
        for (int i = 0; envp[i]; i++)
            nx_cprintf(WHITE, 0, "  %s\n", envp[i]);
    } else {
        nx_cprintf(WHITE, 0, "  (none set by kernel)\n");
    }

    /* ── process probes ─────────────────────────────────────── */
    nx_cprintf(CYAN, 0, "\nprocess info:\n");
    nx_cprintf(WHITE, 0, "  pid = %d, ppid = %d, uid = %u\n",
               nx_getpid(), nx_getppid(), nx_getuid());

    /* ── heap ───────────────────────────────────────────────── */
    nx_cprintf(CYAN, 0, "heap:\n");
    char *buf = (char *)nx_malloc(128);
    if (buf) {
        nx_memset(buf, 0, 128);
        nx_snprintf(buf, 128, "malloc'd string at %p (big-pickle test)",
                    (void *)(uintptr_t)buf);
        nx_cprintf(WHITE, 0, "  %s\n", buf);
        nx_free(buf);
    } else {
        nx_cprintf(RED, 0, "  malloc failed!\n");
    }

    /* ── math helpers ───────────────────────────────────────── */
    nx_cprintf(CYAN, 0, "math:\n");
    nx_cprintf(WHITE, 0, "  sqrt(2)  = %.4f\n", nx_sqrt(2.0));
    nx_cprintf(WHITE, 0, "  pow(2,10) = %.1f\n", nx_pow(2.0, 10.0));
    nx_cprintf(WHITE, 0, "  sin(pi/6) = %.4f\n", nx_sin(3.14159265358979 / 6.0));

    /* ── filesystem probe ───────────────────────────────────── */
    nx_cprintf(CYAN, 0, "filesystem:\n");
    char cwd[256];
    if (nx_getcwd(cwd, sizeof(cwd)))
        nx_cprintf(WHITE, 0, "  cwd = %s\n", cwd);
    NxFileInfo fi;
    if (nx_stat("#/bin", &fi) == 0) {
        nx_cprintf(WHITE, 0, "  #/bin exists (%s)\n",
                   (fi.flags & NX_FS_DIR) ? "directory" : "file");
    } else {
        nx_cprintf(RED, 0, "  #/bin not found\n");
    }

    /* ── time ───────────────────────────────────────────────── */
    NxTime t;
    nx_gettime(&t);
    nx_cprintf(CYAN, 0, "time:\n");
    if (t.year > 2000)
        nx_cprintf(WHITE, 0, "  %04u-%02u-%02u %02u:%02u:%02u\n",
                   t.year, t.month, t.day, t.hour, t.minute, t.second);
    else
        nx_cprintf(WHITE, 0, "  clock not set (uptime %llu ms)\n",
                   (unsigned long long)nx_uptime_ms());

    nx_cprintf(GREEN, 0, "\nhello: all checks done. bye.\n");
    proc_exit(0);
    return 0;
}