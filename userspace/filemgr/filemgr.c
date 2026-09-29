/*
 * filemgr.c — Notux file manager.
 *
 * Built-ins:
 *   dir / ls [path]       list directory
 *   cd <path>             change directory
 *   pwd                   print working directory
 *   cat <file>            print file contents
 *   del / rm <file>       delete a file
 *   mv / ren <src> <dst>  rename/move
 *   mkdir <dir>           create directory
 *   touch <file>          create empty file
 *   up                    go to parent
 *   root                  go to #/
 *   ref                   refresh listing
 *   help                  usage
 *   quit / exit
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/proc.h>
#include <notux/terminal.h>
#include <notux/color.h>

#define CYAN   color_rgb(0,   240, 220)
#define YELLOW color_rgb(255, 220, 80)
#define WHITE  color_rgb(255, 255, 255)
#define GREEN  color_rgb(80,  255, 120)
#define RED    color_rgb(255, 90,  90)

static char g_cwd[256];

static void join_path(char *out, size_t n, const char *base, const char *rel) {
    if (rel[0] == '#') { nx_strncpy(out, rel, n - 1); return; }
    if (rel[0] == '\0') { nx_strncpy(out, base, n - 1); return; }
    if (nx_strcmp(base, "#/") == 0)
        nx_snprintf(out, n, "#/%s", rel);
    else
        nx_snprintf(out, n, "%s/%s", base, rel);
}

static void normalize_path(char *p) {
    char in[256], out[256];
    nx_strncpy(in, p, sizeof(in) - 1);
    char *part[128];
    int np = 0;
    char *tok = nx_strtok(in, "/");
    while (tok) {
        if (nx_strcmp(tok, ".") == 0) { /* skip */ }
        else if (nx_strcmp(tok, "..") == 0) { if (np > 0) np--; }
        else part[np++] = tok;
        tok = nx_strtok(NULL, "/");
    }
    out[0] = '\0';
    for (int i = 0; i < np; i++) {
        nx_strcat(out, "/");
        nx_strcat(out, part[i]);
    }
    if (out[0] == '\0') nx_strcpy(out, "/");
    nx_snprintf(p, 255, "#%s", out);
}

static void list_dir(const char *path) {
    void *dir = fs_opendir(path);
    if (!dir) {
        nx_cprintf(RED, 0, "cannot open: %s\n", path);
        return;
    }
    nx_cprintf(CYAN, 0, "\n  Directory: %s\n", path);
    nx_cprintf(CYAN, 0, "  %-10s %12s  %s\n", "type", "size", "name");
    nx_cprintf(CYAN, 0, "  ---------------------------------------\n");

    FsDirent e;
    int count = 0;
    while (fs_readdir(dir, &e) == 0) {
        char line[320];
        if (e.flags & FS_DIR) {
            nx_cprintf(GREEN, 0, "  %-10s %12s  %s/\n", "DIR", "-", e.name);
        } else {
            nx_sprintf(line, "  %-10s %12u  %s\n", "FILE", (unsigned)e.size, e.name);
            nx_cprintf(WHITE, 0, "%s", line);
        }
        count++;
    }
    fs_closedir(dir);
    char nbuf[32];
    nx_sprintf(nbuf, "%d", count);
    nx_cprintf(YELLOW, 0, "  (%s entries)\n", nbuf);
}

static void cat_file(const char *path) {
    NxFileInfo fi;
    if (nx_stat(path, &fi) != 0) {
        nx_cprintf(RED, 0, "no such file: %s\n", path);
        return;
    }
    if (fi.flags & NX_FS_DIR) {
        list_dir(path);
        return;
    }
    int fd = fs_open(path, FS_O_RDONLY);
    if (fd < 0) {
        nx_cprintf(RED, 0, "cannot open: %s\n", path);
        return;
    }
    uint64_t size = fi.size;
    if (size > 60000) size = 60000;
    char buf[60001];
    nx_memset(buf, 0, sizeof(buf));
    int64_t got = fs_read(fd, buf, (size_t)size);
    fs_close(fd);
    if (got <= 0) return;
    nx_cprintf(YELLOW, 0, "--- %s ---\n", path);
    nx_puts(buf);
    if (buf[got - 1] != '\n') nx_puts("\n");
}

static void usage(void) {
    nx_cprintf(CYAN, 0, "filemgr commands:\n");
    nx_cprintf(WHITE, 0,
        "  dir|ls [path]   list      cd <path>  change dir\n"
        "  pwd              cwd       cat <file> show file\n"
        "  del|rm <file>    delete    mv <s> <d> rename\n"
        "  mkdir <dir>      create    touch <f>  create file\n"
        "  up | root | ref  nav       help       this text\n"
        "  quit|exit        leave\n");
}

int main(int argc, char **argv, char **envp) {
    (void)envp;
    char *cwd = nx_getcwd(g_cwd, sizeof(g_cwd));
    if (!cwd || g_cwd[0] == '\0') nx_strcpy(g_cwd, "#/");

    if (argc > 1) {
        /* CLI mode: filemgr dir|pwd|ls [path] */
        if (nx_strcmp(argv[1], "dir") == 0 || nx_strcmp(argv[1], "ls") == 0) {
            char p[256];
            if (argc > 2) { join_path(p, sizeof(p), g_cwd, argv[2]); normalize_path(p); }
            else nx_strcpy(p, g_cwd);
            list_dir(p);
            proc_exit(0);
            return 0;
        }
        if (nx_strcmp(argv[1], "pwd") == 0) {
            nx_puts(g_cwd);
            nx_puts("\n");
            proc_exit(0);
            return 0;
        }
        if (nx_strcmp(argv[1], "cat") == 0 && argc > 2) {
            char p[256];
            join_path(p, sizeof(p), g_cwd, argv[2]);
            normalize_path(p);
            cat_file(p);
            proc_exit(0);
            return 0;
        }
        if (nx_strcmp(argv[1], "help") == 0 || nx_strcmp(argv[1], "--help") == 0) {
            usage();
            proc_exit(0);
            return 0;
        }
        nx_cprintf(RED, 0, "usage: filemgr [dir|pwd|cat <file>|help]\n");
        proc_exit(1);
        return 1;
    }

    nx_cprintf(GREEN, 0, "Notux file manager.  'help' for commands.\n");
    char line[160];

    for (;;) {
        list_dir(g_cwd);
        nx_cprintf(YELLOW, 0, "%s> ", g_cwd);
        if (!nx_gets(line, sizeof(line))) break;

        char cmd[64];
        char a1[128], a2[128];
        int n = nx_snprintf(cmd, 64, "%s", nx_strtok(line, " "));
        (void)n;
        char *t = nx_strtok(NULL, " ");
        if (t) nx_strncpy(a1, t, sizeof(a1) - 1); else a1[0] = '\0';
        t = nx_strtok(NULL, " ");
        if (t) nx_strncpy(a2, t, sizeof(a2) - 1); else a2[0] = '\0';

        if (cmd[0] == '\0') continue;

        if (nx_strcmp(cmd, "dir") == 0 || nx_strcmp(cmd, "ls") == 0) {
            char p[256];
            if (a1[0]) { join_path(p, sizeof(p), g_cwd, a1); normalize_path(p); }
            else nx_strcpy(p, g_cwd);
            list_dir(p);
        }
        else if (nx_strcmp(cmd, "cd") == 0) {
            if (!a1[0]) { nx_strcpy(g_cwd, "#/"); continue; }
            char p[256];
            join_path(p, sizeof(p), g_cwd, a1);
            normalize_path(p);
            NxFileInfo fi;
            if (nx_stat(p, &fi) == 0) {
                nx_strcpy(g_cwd, p);
            } else {
                nx_cprintf(RED, 0, "no such dir: %s\n", a1);
            }
        }
        else if (nx_strcmp(cmd, "pwd") == 0) {
            nx_puts(g_cwd);
            nx_puts("\n");
        }
        else if (nx_strcmp(cmd, "up") == 0) {
            char *l = g_cwd + nx_strlen(g_cwd) - 1;
            if (nx_strcmp(g_cwd, "#/") != 0) {
                while (l > g_cwd && *l != '/') *l-- = '\0';
                if (l == g_cwd) nx_strcpy(g_cwd, "#/");
            }
        }
        else if (nx_strcmp(cmd, "root") == 0) {
            nx_strcpy(g_cwd, "#/");
        }
        else if (nx_strcmp(cmd, "cat") == 0) {
            if (!a1[0]) { nx_puts("usage: cat <file>\n"); continue; }
            char p[256];
            join_path(p, sizeof(p), g_cwd, a1);
            normalize_path(p);
            cat_file(p);
        }
        else if (nx_strcmp(cmd, "del") == 0 || nx_strcmp(cmd, "rm") == 0) {
            if (!a1[0]) { nx_puts("usage: del <file>\n"); continue; }
            char p[256];
            join_path(p, sizeof(p), g_cwd, a1);
            normalize_path(p);
            if (fs_unlink(p) == 0)
                nx_cprintf(GREEN, 0, "deleted %s\n", a1);
            else
                nx_cprintf(RED, 0, "delete failed: %s\n", a1);
        }
        else if (nx_strcmp(cmd, "mv") == 0 || nx_strcmp(cmd, "ren") == 0) {
            if (!a1[0] || !a2[0]) { nx_puts("usage: mv <src> <dst>\n"); continue; }
            char s[256], d[256];
            join_path(s, sizeof(s), g_cwd, a1); normalize_path(s);
            join_path(d, sizeof(d), g_cwd, a2); normalize_path(d);
            if (fs_rename(s, d) == 0)
                nx_cprintf(GREEN, 0, "renamed\n");
            else
                nx_cprintf(RED, 0, "rename failed\n");
        }
        else if (nx_strcmp(cmd, "mkdir") == 0) {
            if (!a1[0]) { nx_puts("usage: mkdir <dir>\n"); continue; }
            char p[256];
            join_path(p, sizeof(p), g_cwd, a1);
            normalize_path(p);
            if (fs_mkdir(p) == 0)
                nx_cprintf(GREEN, 0, "created %s\n", a1);
            else
                nx_cprintf(RED, 0, "mkdir failed\n");
        }
        else if (nx_strcmp(cmd, "touch") == 0) {
            if (!a1[0]) { nx_puts("usage: touch <file>\n"); continue; }
            char p[256];
            join_path(p, sizeof(p), g_cwd, a1);
            normalize_path(p);
            int fd = nx_open(p, NX_O_WRONLY | NX_O_CREATE);
            if (fd >= 0) {
                nx_close(fd);
                nx_cprintf(GREEN, 0, "touched %s\n", a1);
            } else {
                nx_cprintf(RED, 0, "touch failed\n");
            }
        }
        else if (nx_strcmp(cmd, "ref") == 0) {
            continue;
        }
        else if (nx_strcmp(cmd, "help") == 0) {
            usage();
        }
        else if (nx_strcmp(cmd, "quit") == 0 || nx_strcmp(cmd, "exit") == 0) {
            break;
        }
        else {
            nx_cprintf(RED, 0, "unknown command: %s (help)\n", cmd);
        }
    }

    nx_puts("bye.\n");
    proc_exit(0);
    return 0;
}