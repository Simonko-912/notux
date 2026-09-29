/*
 * pkgman.c — Notux local package manager.
 * Manages the programs installed in #/bin.
 *
 *   pkgman list            installed programs + sizes
 *   pkgman info <app>      details on one program
 *   pkgman remove <app>    uninstall (unlink from #/bin)
 *   pkgman <app> [args]    run a program
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/proc.h>
#include <notux/color.h>

#define YELLOW color_rgb(255, 220, 80)
#define WHITE  color_rgb(255, 255, 255)
#define GREEN  color_rgb(80, 255, 120)
#define RED    color_rgb(255, 90, 90)
#define CYAN   color_rgb(0, 240, 220)

static int is_valid_name(const char *n) {
    if (!n || !n[0]) return 0;
    for (const char *p = n; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' || *p == '.'))
            return 0;
    return 1;
}

int main(int argc, char **argv, char **envp) {
    (void)envp;

    if (argc < 2 || nx_strcmp(argv[1], "help") == 0 ||
        nx_strcmp(argv[1], "--help") == 0) {
        nx_cprintf(YELLOW, 0, "pkgman: list | info <app> | remove <app> | <app> [args]\n");
        return 0;
    }

    if (nx_strcmp(argv[1], "list") == 0) {
        nx_cprintf(CYAN, 0, "%-14s %10s\n", "SIZE", "PROGRAM");
        void *dir = nx_opendir("#/bin");
        if (!dir) { nx_puts("cannot open #/bin\n"); return 1; }
        char nm[256];
        int count = 0;
        while (nx_readdir(dir, nm, NULL) == 0) {
            NxFileInfo fi;
            char p[320];
            nx_snprintf(p, sizeof(p), "#/bin/%s", nm);
            if (nx_stat(p, &fi) == 0 && (fi.flags & NX_FS_DIR) == 0) {
                nx_cprintf(WHITE, 0, "%-14u %s\n", (unsigned)fi.size, nm);
                count++;
            }
        }
        nx_closedir(dir);
        char nb[24];
        nx_sprintf(nb, "%d", count);
        nx_cprintf(GREEN, 0, "%s package(s) installed.\n", nb);
        return 0;
    }

    if (nx_strcmp(argv[1], "info") == 0) {
        if (argc < 3 || !is_valid_name(argv[2])) {
            nx_puts("usage: pkgman info <app>\n");
            return 1;
        }
        char p[320];
        NxFileInfo fi;
        nx_snprintf(p, sizeof(p), "#/bin/%s", argv[2]);
        if (nx_stat(p, &fi) != 0) {
            nx_cprintf(RED, 0, "not installed: %s\n", argv[2]);
            return 1;
        }
        nx_cprintf(WHITE, 0, "  package:  %s\n", argv[2]);
        nx_cprintf(WHITE, 0, "  size:     %u bytes\n", (unsigned)fi.size);
        nx_cprintf(WHITE, 0, "  path:     %s\n", p);
        return 0;
    }

    if (nx_strcmp(argv[1], "remove") == 0 || nx_strcmp(argv[1], "rm") == 0) {
        if (argc < 3 || !is_valid_name(argv[2])) {
            nx_puts("usage: pkgman remove <app>\n");
            return 1;
        }
        char p[320];
        nx_snprintf(p, sizeof(p), "#/bin/%s", argv[2]);
        if (nx_unlink(p) == 0) {
            nx_cprintf(GREEN, 0, "removed %s\n", argv[2]);
            return 0;
        }
        nx_cprintf(RED, 0, "could not remove %s\n", argv[2]);
        return 1;
    }

    /* pkgman <app> [args] — run */
    if (is_valid_name(argv[1])) {
        char p[320];
        nx_snprintf(p, sizeof(p), "#/bin/%s", argv[1]);
        NxFileInfo fi;
        if (nx_stat(p, &fi) != 0) {
            nx_cprintf(RED, 0, "not installed: %s\n", argv[1]);
            return 1;
        }
        char **nargv = (char **)nx_malloc((size_t)argc * sizeof(char *));
        for (int i = 0; i < argc; i++) nargv[i] = argv[i];
        nx_pid_t pid = nx_exec(p, (const char **)nargv, NULL);
        nx_free(nargv);
        if (pid < 0) {
            nx_cprintf(RED, 0, "failed to start %s\n", argv[1]);
            return 1;
        }
        return 0;
    }

    nx_puts("unknown pkgman command.\n");
    return 1;
}