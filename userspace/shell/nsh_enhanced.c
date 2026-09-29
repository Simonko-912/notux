/*
 * nsh_enhanced.c — Notux shell: built-in command dispatch and
 * external program execution.
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/proc.h>
#include <notux/net.h>
#include <notux/terminal.h>
#include <notux/color.h>
#include "nsh.h"

#define CLR_NAME  color_rgb(255,220,80)
#define CLR_DIR   color_rgb(0,240,220)
#define CLR_OK    color_rgb(80,255,120)
#define CLR_ERR   color_rgb(255,90,90)
#define CLR_INFO  color_rgb(200,200,210)
#define CLR_HDR   color_rgb(0,240,220)

static void cmd_help(char **a);
static void cmd_version(char **a);
static void cmd_cd(char **a);
static void cmd_dir(char **a);
static void cmd_pwd(char **a);
static void cmd_touch(char **a);
static void cmd_mkdir(char **a);
static void cmd_rm(char **a);
static void cmd_mv(char **a);
static void cmd_cat(char **a);
static void cmd_edit(char **a);
static void cmd_ping(char **a);
static void cmd_color(char **a);
static void cmd_env(char **a);
static void cmd_history(char **a);
static void cmd_uptime(char **a);
static void cmd_uname(char **a);
static void cmd_clear(char **a);
static void cmd_usr(char **a);
static void cmd_unzip(char **a);
static void cmd_complete(char **a);
static void cmd_exit(char **a);

static struct { const char *name; void (*fn)(char **); } g_builtins[] = {
    { "help",     cmd_help     },
    { "?",        cmd_help     },
    { "version",  cmd_version  },
    { "v",        cmd_version  },
    { "cd",       cmd_cd       },
    { "chdir",    cmd_cd       },
    { "dir",      cmd_dir      },
    { "ls",       cmd_dir      },
    { "pwd",      cmd_pwd      },
    { "touch",    cmd_touch    },
    { "mkdir",    cmd_mkdir    },
    { "rm",       cmd_rm       },
    { "del",      cmd_rm       },
    { "mv",       cmd_mv       },
    { "ren",      cmd_mv       },
    { "cat",      cmd_cat      },
    { "edit",     cmd_edit     },
    { "ping",     cmd_ping     },
    { "color",    cmd_color    },
    { "env",      cmd_env      },
    { "setenv",   cmd_env      },
    { "getenv",   cmd_env      },
    { "history",  cmd_history  },
    { "uptime",   cmd_uptime   },
    { "time",     cmd_uptime   },
    { "uname",    cmd_uname    },
    { "clear",    cmd_clear    },
    { "cls",      cmd_clear    },
    { "usr",      cmd_usr      },
    { "unzip",    cmd_unzip    },
    { "complete", cmd_complete },
    { "exit",     cmd_exit     },
    { "logout",   cmd_exit     },
    { "quit",     cmd_exit     },
};

static const int g_num_builtins = (int)(sizeof(g_builtins) / sizeof(g_builtins[0]));

int nsh_is_builtin(const char *name) {
    for (int i = 0; i < g_num_builtins; i++)
        if (nx_strcmp(name, g_builtins[i].name) == 0) return 1;
    return 0;
}

/* ── path helpers ────────────────────────────────────────────── */
static void dir_join(char *out, size_t n, const char *base, const char *rel) {
    if (rel[0] == '#') { nx_strncpy(out, rel, n - 1); return; }
    if (rel[0] == '/') { nx_strncpy(out, rel, n - 1); return; }
    if (rel[0] == '\0') { nx_strncpy(out, base, n - 1); return; }
    char tmp[NSH_PATH_MAX];
    if (base[0] && nx_strcmp(base, "#/") != 0)
        nx_snprintf(tmp, sizeof(tmp), "%s/%s", base, rel);
    else if (rel[0] == '.')
        nx_strncpy(tmp, rel, sizeof(tmp) - 1);
    else
        nx_snprintf(tmp, sizeof(tmp), "#/%s", rel);

    /* collapse .. */
    char in[NSH_PATH_MAX], outbuf[NSH_PATH_MAX];
    nx_strncpy(in, tmp, sizeof(in) - 1);
    char *parts[64];
    int np = 0;
    char *tok = nx_strtok(in, "/");
    while (tok) {
        if (nx_strcmp(tok, ".") == 0) { /* skip */ }
        else if (nx_strcmp(tok, "..") == 0) { if (np > 0) np--; }
        else parts[np++] = tok;
        tok = nx_strtok(NULL, "/");
    }
    outbuf[0] = '\0';
    for (int i = 0; i < np; i++) {
        nx_strcat(outbuf, "/");
        nx_strcat(outbuf, parts[i]);
    }
    if (outbuf[0] == '\0') nx_strcpy(outbuf, "/");
    nx_snprintf(out, n, "#%s", outbuf);
}

static void list_dir(const char *path) {
    void *dir = fs_opendir(path);
    if (!dir) {
        nsh_err("cannot open '%s'\n", path);
        return;
    }
    FsDirent e;
    int count = 0;
    while (fs_readdir(dir, &e) == 0) {
        if (e.flags & FS_DIR)
            nx_cprintf(CLR_DIR, 0, "  %s/\n", e.name);
        else
            nx_cprintf(CLR_INFO, 0, "  %-24s %10u\n", e.name, (unsigned)e.size);
        count++;
    }
    fs_closedir(dir);
    char nbuf[24];
    nx_sprintf(nbuf, "%d", count);
    nx_cprintf(CLR_NAME, 0, "  (%d entries)\n", count);
    (void)nbuf;
}

/* ── built-in implementations ────────────────────────────────── */
static void cmd_help(char **a) {
    (void)a;
    nx_cprintf(CLR_HDR, 0,
        "Built-ins: help version cd|chdir dir|ls pwd touch mkdir rm|del\n"
        "           mv|ren cat edit ping color env|setenv|getenv history\n"
        "           uptime|time uname clear cls usr unzip complete exit\n"
        "Apps in #/bin:  hello testapp calc filemgr editor nsh ps meminfo\n"
        "                nfetch notedit pkgman opm\n");
    nx_cprintf(CLR_INFO, 0,
        "  env            list environment\n"
        "  setenv K V     set variable\n"
        "  usr list|add|del <name>   user management\n"
        "  complete <p>   list #/bin and cwd entries starting with prefix\n"
        "  color <r> <g> <b>   set prompt accent (0-255 each)\n");
}

static void cmd_version(char **a) {
    (void)a;
    nsh_ok("Notux Shell %s\n", NSH_VERSION);
}

static void cmd_cd(char **a) {
    if (!a[0] || a[0][0] == '\0') { nx_strcpy(g_cwd, "#/"); return; }
    char out[NSH_PATH_MAX];
    dir_join(out, sizeof(out), g_cwd, a[0]);
    if (a[1]) { nsh_err("too many arguments\n"); return; }
    NxFileInfo fi;
    if (nx_stat(out, &fi) == 0 && (fi.flags & NX_FS_DIR)) {
        nx_strcpy(g_cwd, out);
        nx_chdir(out);
    } else {
        nsh_err("no such directory: %s\n", a[0]);
    }
}

static void cmd_dir(char **a) {
    char path[NSH_PATH_MAX];
    if (a[0] && a[0][0]) dir_join(path, sizeof(path), g_cwd, a[0]);
    else nx_strncpy(path, g_cwd, sizeof(path) - 1);
    list_dir(path);
}

static void cmd_pwd(char **a) {
    (void)a;
    nx_cprintf(CLR_INFO, 0, "%s\n", g_cwd);
}

static void cmd_touch(char **a) {
    if (!a[0]) { nsh_err("usage: touch <file>\n"); return; }
    char p[NSH_PATH_MAX];
    dir_join(p, sizeof(p), g_cwd, a[0]);
    int fd = nx_open(p, NX_O_WRONLY | NX_O_CREATE);
    if (fd >= 0) { nx_close(fd); nsh_ok("created %s\n", a[0]); }
    else nsh_err("touch failed\n");
}

static void cmd_mkdir(char **a) {
    if (!a[0]) { nsh_err("usage: mkdir <dir>\n"); return; }
    char p[NSH_PATH_MAX];
    dir_join(p, sizeof(p), g_cwd, a[0]);
    if (nx_mkdir(p) == 0) nsh_ok("created %s\n", a[0]);
    else nsh_err("mkdir failed\n");
}

static void cmd_rm(char **a) {
    if (!a[0]) { nsh_err("usage: rm <file>\n"); return; }
    char p[NSH_PATH_MAX];
    dir_join(p, sizeof(p), g_cwd, a[0]);
    NxFileInfo fi;
    if (nx_stat(p, &fi) != 0) { nsh_err("no such file: %s\n", a[0]); return; }
    if (fi.flags & NX_FS_DIR) { nsh_err("rm: is a directory: %s\n", a[0]); return; }
    if (nx_unlink(p) == 0) nsh_ok("deleted %s\n", a[0]);
    else nsh_err("delete failed\n");
}

static void cmd_mv(char **a) {
    if (!a[0] || !a[1]) { nsh_err("usage: mv <src> <dst>\n"); return; }
    char s[NSH_PATH_MAX], d[NSH_PATH_MAX];
    dir_join(s, sizeof(s), g_cwd, a[0]);
    dir_join(d, sizeof(d), g_cwd, a[1]);
    if (nx_rename(s, d) == 0) nsh_ok("renamed\n");
    else nsh_err("rename failed\n");
}

static void cmd_cat(char **a) {
    if (!a[0]) { nsh_err("usage: cat <file>\n"); return; }
    char p[NSH_PATH_MAX];
    dir_join(p, sizeof(p), g_cwd, a[0]);
    NxFileInfo fi;
    if (nx_stat(p, &fi) != 0) { nsh_err("no such file: %s\n", a[0]); return; }
    if (fi.flags & NX_FS_DIR) { nsh_err("cat: is a directory\n"); return; }
    int fd = nx_open(p, NX_O_RDONLY);
    if (fd < 0) { nsh_err("cannot open\n"); return; }
    uint64_t size = fi.size;
    if (size > 32000) size = 32000;
    char buf[32001];
    nx_memset(buf, 0, sizeof(buf));
    int64_t got = nx_read(fd, buf, (size_t)size);
    nx_close(fd);
    if (got > 0) {
        nx_puts(buf);
        if (buf[got - 1] != '\n') nx_puts("\n");
    }
}

static void cmd_edit(char **a) {
    if (!a[0]) { nsh_err("usage: edit <file>\n"); return; }
    char *argv[4] = { "editor", a[0], NULL };
    nx_pid_t pid = nx_exec("#/bin/editor", (const char **)argv, NULL);
    if (pid < 0) nsh_err("editor not found (#/bin/editor)\n");
    else nsh_ok("editor started (pid %d)\n", pid);
}

static void cmd_ping(char **a) {
    if (!a[0]) { nsh_err("usage: ping <host>\n"); return; }
    uint32_t ip = net_resolve(a[0]);
    if (ip == 0) { nsh_err("cannot resolve '%s'\n", a[0]); return; }
    nsh_ok("ping %s (%u.%u.%u.%u)\n", a[0],
           (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
           (ip >> 8) & 0xFF, ip & 0xFF);
    int ms = net_ping(ip);
    if (ms >= 0) nsh_ok("reply in %d ms\n", ms);
    else nsh_err("no reply\n");
}

static void cmd_color(char **a) {
    if (a[0] && nx_strcmp(a[0], "reset") == 0) {
        nx_term_reset_color();
        return;
    }
    if (!a[0] || !a[1] || !a[2]) {
        nsh_err("usage: color <r> <g> <b>  |  color reset\n");
        return;
    }
    uint32_t r = (uint32_t)nx_strtol(a[0], NULL, 10) & 0xFF;
    uint32_t g = (uint32_t)nx_strtol(a[1], NULL, 10) & 0xFF;
    uint32_t b = (uint32_t)nx_strtol(a[2], NULL, 10) & 0xFF;
    nx_term_set_fg(color_rgb(r, g, b));
}

static void cmd_env(char **a) {
    if (!a[0]) {
        /* list entire env */
        int i = 0;
        const char *keys[] = { "PATH", "HOST", "USER", "SHELL", NULL };
        for (i = 0; keys[i]; i++) {
            const char *v = nx_getenv(keys[i]);
            nx_cprintf(CLR_INFO, 0, "%s=%s\n", keys[i], v ? v : "(unset)");
        }
        return;
    }
    if (nx_strcmp(a[0], "setenv") == 0 && a[1] && a[2]) {
        nx_setenv(a[1], a[2]);
        return;
    }
    if (nx_strcmp(a[0], "getenv") == 0 && a[1]) {
        const char *v = nx_getenv(a[1]);
        nx_cprintf(CLR_INFO, 0, "%s=%s\n", a[1], v ? v : "(unset)");
        return;
    }
    /* setenv K V */
    if (a[1]) { nx_setenv(a[0], a[1]); return; }
    const char *v = nx_getenv(a[0]);
    nx_cprintf(CLR_INFO, 0, "%s=%s\n", a[0], v ? v : "(unset)");
}

static void cmd_history(char **a) {
    (void)a;
    for (int i = 0; i < g_history_count; i++) {
        char nbuf[16];
        nx_sprintf(nbuf, "%2d", i + 1);
        nx_cprintf(CLR_NAME, 0, " %s ", nbuf);
        nx_cprintf(CLR_INFO, 0, "%s\n", g_history[i]);
    }
    if (!g_history_count) nx_puts("(no history yet)\n");
}

static void cmd_uptime(char **a) {
    (void)a;
    uint64_t ms = nx_uptime_ms();
    nx_cprintf(CLR_INFO, 0, "up %llu ms (%llu s)\n",
               (unsigned long long)ms, (unsigned long long)(ms / 1000));
    NxTime t;
    nx_gettime(&t);
    if (t.year > 2000)
        nx_cprintf(CLR_INFO, 0, "date %04u-%02u-%02u %02u:%02u:%02u\n",
                   t.year, t.month, t.day, t.hour, t.minute, t.second);
}

static void cmd_uname(char **a) {
    (void)a;
    nx_cprintf(CLR_INFO, 0, "Notux  x86_64  ring-3 userspace\n");
}

static void cmd_clear(char **a) {
    (void)a;
    nx_term_clear();
}

static void cmd_usr(char **a) {
    if (!a[0]) { nsh_err("usage: usr list|add <name>|del <name>\n"); return; }
    if (nx_strcmp(a[0], "list") == 0) {
        int fd = nx_open("#/etc/users", NX_O_RDONLY);
        if (fd < 0) { nsh_ok("no users yet (#/etc/users)\n"); return; }
        char buf[2048];
        int64_t n = nx_read(fd, buf, sizeof(buf) - 1);
        nx_close(fd);
        if (n > 0) { buf[n] = '\0'; nx_puts(buf); }
        return;
    }
    if (nx_strcmp(a[0], "add") == 0 && a[1]) {
        int fd = nx_open("#/etc/users", NX_O_WRONLY | NX_O_CREATE | NX_O_APPEND);
        if (fd < 0) { nsh_err("cannot open #/etc/users\n"); return; }
        static const char ok_[] = "user\n";
        (void)ok_;
        char line[128];
        nx_snprintf(line, sizeof(line), "%s\n", a[1]);
        nx_write(fd, line, nx_strlen(line));
        nx_close(fd);
        nsh_ok("added user '%s'\n", a[1]);
        return;
    }
    nsh_err("usage: usr list|add <name>|del <name>\n");
}

static void cmd_unzip(char **a) {
    (void)a;
    nsh_err("unzip: not available in this build\n");
}

static void cmd_complete(char **a) {
    const char *prefix = a[0] ? a[0] : "";
    int len = (int)nx_strlen(prefix);
    int found = 0;
    void *dir = fs_opendir("#/bin");
    if (dir) {
        FsDirent e;
        nx_cprintf(CLR_HDR, 0, "#/bin:\n");
        while (fs_readdir(dir, &e) == 0) {
            if (nx_strncmp(e.name, prefix, (size_t)len) == 0) {
                nx_cprintf(CLR_DIR, 0, "  %s\n", e.name);
                found = 1;
            }
        }
        fs_closedir(dir);
    }
    dir = fs_opendir(g_cwd);
    if (dir) {
        FsDirent e;
        nx_cprintf(CLR_HDR, 0, "%s:\n", g_cwd);
        while (fs_readdir(dir, &e) == 0) {
            if (nx_strncmp(e.name, prefix, (size_t)len) == 0) {
                if (e.flags & FS_DIR) nx_cprintf(CLR_DIR, 0, "  %s/\n", e.name);
                else nx_cprintf(CLR_INFO, 0, "  %s\n", e.name);
                found = 1;
            }
        }
        fs_closedir(dir);
    }
    if (!found) nsh_err("no matches for '%s'\n", prefix);
}

static void cmd_exit(char **a) {
    (void)a;
    /* returns 99 -> main loop breaks */
}

/* ── dispatch ────────────────────────────────────────────────── */
static int try_builtin(char **args) {
    for (int i = 0; i < g_num_builtins; i++)
        if (nx_strcmp(args[0], g_builtins[i].name) == 0) {
            g_builtins[i].fn(args + 1);
            return (nx_strcmp(args[0], "exit") == 0) ||
                   (nx_strcmp(args[0], "logout") == 0) ||
                   (nx_strcmp(args[0], "quit") == 0) ? 99 : 0;
        }
    return -1;
}

int nsh_dispatch(char *line) {
    char *args[8];
    int argc = 0;
    nsh_split(line, args, &argc, 8);
    if (argc == 0) return 0;

    int rc = try_builtin(args);
    if (rc != -1) return rc;

    /* external program execution */
    char path[NSH_PATH_MAX];
    int has_path = (args[0][0] == '.' || args[0][0] == '/' || args[0][0] == '#');
    if (has_path) {
        dir_join(path, sizeof(path), g_cwd, args[0]);
    } else if (nsh_find_program(args[0], path, sizeof(path))) {
        /* found in #/bin */
    } else {
        nsh_err("command not found: %s\n", args[0]);
        return 0;
    }

    char *argv[9];
    int i = 0;
    argv[i++] = args[0];
    for (; i <= argc && i < 9; i++) argv[i] = args[i];
    argv[i] = NULL;

    nx_pid_t pid = nx_exec(path, (const char **)argv, NULL);
    if (pid < 0) nsh_err("failed to start %s\n", args[0]);
    else nsh_ok("started %s (pid %d)\n", args[0], pid);
    return 0;
}

void nsh_complete(const char *prefix) {
    char *a[2] = { (char *)prefix, NULL };
    cmd_complete(a);
}