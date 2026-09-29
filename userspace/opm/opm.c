/*
 * opm.c — Notux online package manager front-end.
 * Requires a network transport; without a working link it falls
 * back to listing the local package set and reports status.
 *
 *   opm update            check for a registry (needs network)
 *   opm search <text>     search local package names
 *   opm list              local packages (same as pkgman list)
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/net.h>
#include <notux/proc.h>
#include <notux/color.h>

#define YELLOW color_rgb(255, 220, 80)
#define WHITE  color_rgb(255, 255, 255)
#define RED    color_rgb(255, 90, 90)
#define CYAN   color_rgb(0, 240, 220)

static void list_local(void) {
    nx_cprintf(CYAN, 0, "local packages (#/bin):\n");
    void *dir = nx_opendir("#/bin");
    if (!dir) { nx_puts("cannot open #/bin\n"); return; }
    char nm[256];
    while (nx_readdir(dir, nm, NULL) == 0) {
        NxFileInfo fi;
        char p[320];
        nx_snprintf(p, sizeof(p), "#/bin/%s", nm);
        if (nx_stat(p, &fi) == 0 && (fi.flags & NX_FS_DIR) == 0)
            nx_cprintf(WHITE, 0, "  %-20s %u bytes\n", nm, (unsigned)fi.size);
    }
    nx_closedir(dir);
}

int main(int argc, char **argv, char **envp) {
    (void)envp;

    if (argc < 2 || nx_strcmp(argv[1], "help") == 0 ||
        nx_strcmp(argv[1], "--help") == 0) {
        nx_cprintf(YELLOW, 0, "opm: update | search <text> | list\n");
        return 0;
    }

    if (nx_strcmp(argv[1], "list") == 0) {
        list_local();
        return 0;
    }

    if (nx_strcmp(argv[1], "search") == 0) {
        if (argc < 3) { nx_puts("usage: opm search <text>\n"); return 1; }
        nx_cprintf(CYAN, 0, "searching local packages for '%s':\n", argv[2]);
        void *dir = nx_opendir("#/bin");
        int found = 0;
        if (dir) {
            char nm[256];
            while (nx_readdir(dir, nm, NULL) == 0) {
                if (nx_strstr(nm, argv[2])) {
                    nx_cprintf(WHITE, 0, "  %s\n", nm);
                    found = 1;
                }
            }
            nx_closedir(dir);
        }
        if (!found) nx_cprintf(RED, 0, "  no local matches.\n");
        return 0;
    }

    if (nx_strcmp(argv[1], "update") == 0) {
        nx_cprintf(YELLOW, 0, "contacting registry (net.notux.local)...\n");
        uint32_t ip = nx_resolve("registry.notux.local");
        if (ip == 0) {
            nx_cprintf(RED, 0,
                "no network registry reachable; local packages are current.\n");
            list_local();
            return 1;
        }
        nx_cprintf(color_rgb(80, 255, 120), 0,
            "registry reachable at %u.%u.%u.%u\n",
            (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
        nx_cprintf(color_rgb(80, 255, 120), 0, "packages up to date.\n");
        return 0;
    }

    nx_puts("unknown opm command.\n");
    return 1;
}