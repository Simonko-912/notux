/*
 * tree.c — recursive directory listing under a path (default #/).
 *
 * tree [PATH]
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/proc.h>
#include <notux/color.h>

#define CLR_DIR  color_rgb(0, 240, 220)
#define CLR_FILE color_rgb(255, 255, 255)

static void indent(int depth) {
    for (int i = 0; i < depth; i++) nx_puts("  ");
}

static void walk(const char *path, int depth, int *count) {
    NxDir dir = nx_opendir(path);
    if (!dir) return;

    char name[256];
    while (nx_readdir(dir, name, NULL) == 0) {
        if (name[0] == '\0') continue;
        indent(depth);

        char child[512];
        if (path[nx_strlen(path) - 1] == '/')
            nx_snprintf(child, sizeof(child), "%s%s", path, name);
        else
            nx_snprintf(child, sizeof(child), "%s/%s", path, name);

        NxFileInfo fi;
        int is_dir = (nx_stat(child, &fi) == 0) && (fi.flags & NX_FS_DIR);

        nx_cprintf(is_dir ? CLR_DIR : CLR_FILE, 0, "%s\n", name);
        (*count)++;
        if (is_dir && depth < 8) walk(child, depth + 1, count);
    }
    nx_closedir(dir);
}

int main(int argc, char **argv, char **envp) {
    (void)envp;
    const char *root = (argc > 1) ? argv[1] : "#/";
    int count = 0;

    nx_cprintf(CLR_DIR, 0, "%s\n", root);
    walk(root, 1, &count);

    char nbuf[24];
    nx_sprintf(nbuf, "%d entries\n", count);
    nx_puts(nbuf);
    proc_exit(0);
    return 0;
}
