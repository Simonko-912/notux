/*
 * nfetch.c — Notux system info banner (neofetch-style).
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/proc.h>
#include <notux/terminal.h>
#include <notux/color.h>

#define YELLOW color_rgb(255, 220, 80)
#define CYAN   color_rgb(0, 240, 220)
#define WHITE  color_rgb(255, 255, 255)
#define GREEN  color_rgb(80, 255, 120)

int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv; (void)envp;

    nx_cprintf(YELLOW, 0,
        "\n"
        "   .---.    `-:///-`.\n"
        " .     .   .-:::::::-\n"
        " -  ##  - .-::::::::`\n"
        "  `...`   -::::/:-+::`\n"
        "          -:::/`.-::/\n"
        "           `:/+:..::`\n"
        "             `%s`\"+:.\n",
        "==========");

    char usr[24];
    nx_sprintf(usr, "user%d", nx_getuid());
    nx_cprintf(GREEN, 0, "  %s", usr);
    nx_cprintf(WHITE, 0, "@notux\n");
    nx_cprintf(WHITE, 0, "  -------------\n");
    nx_cprintf(WHITE, 0, "  OS:       Notux x86_64\n");
    nx_cprintf(WHITE, 0, "  Kernel:   ring-3 capable\n");
    nx_cprintf(WHITE, 0, "  Uptime:   %llu s\n",
               (unsigned long long)(nx_uptime_ms() / 1000));
    nx_cprintf(WHITE, 0, "  UID:      %u\n", nx_getuid());
    nx_cprintf(WHITE, 0, "  Terminal: tty%d (%dx%d)\n", nx_gettty(), nx_term_rows(), nx_term_cols());

    char cwd[256];
    if (nx_getcwd(cwd, sizeof(cwd)))
        nx_cprintf(WHITE, 0, "  CWD:      %s\n", cwd);

    NxTime t;
    nx_gettime(&t);
    if (t.year > 2000)
        nx_cprintf(WHITE, 0, "  Date:     %04u-%02u-%02u %02u:%02u:%02u\n",
                   t.year, t.month, t.day, t.hour, t.minute, t.second);

    void *dir = nx_opendir("#/bin");
    int apps = 0;
    if (dir) {
        char nm[256];
        while (nx_readdir(dir, nm, NULL) == 0) apps++;
        nx_closedir(dir);
    }
    nx_cprintf(WHITE, 0, "  Apps:     %d in #/bin\n", apps);
    nx_puts("\n");
    return 0;
}
