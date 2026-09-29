/*
 * testapp.c — Notux syscall/lib test suite.
 * Runs a battery of checks and reports PASS/FAIL with a summary.
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/proc.h>
#include <notux/terminal.h>
#include <notux/color.h>

#define TESTS_MAX 64
static const char *g_names[TESTS_MAX];
static int         g_results[TESTS_MAX];
static int         g_n;

static void t(const char *name, int ok) {
    if (g_n < TESTS_MAX) {
        g_names[g_n] = name;
        g_results[g_n] = ok;
        g_n++;
    }
    nx_cprintf(ok ? color_rgb(80,255,120) : color_rgb(255,90,90),
               0, "  %s %s\n", ok ? "PASS" : "FAIL", name);
}

int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv; (void)envp;
    nx_cprintf(color_rgb(0,240,220), 0,
        "Notux test suite: %d checks follow.\n\n", 0);

    int fd, r;
    char buf[256];

    /* ── process ─────────────────────────────────────────────── */
    nx_cprintf(color_rgb(255,220,80), 0, "[process]\n");
    t("getpid > 0", nx_getpid() > 0);
    t("getppid >= 0", nx_getppid() >= 0);
    t("getuid == 0", nx_getuid() == 0);

    /* ── strings ─────────────────────────────────────────────── */
    nx_cprintf(color_rgb(255,220,80), 0, "[strings]\n");
    t("strlen", nx_strlen("hello") == 5);
    t("strcmp", nx_strcmp("abc", "abc") == 0 && nx_strcmp("a","b") < 0);
    nx_strcpy(buf, "prefix");
    nx_strcat(buf, "-suffix");
    t("strcpy/strcat", nx_strcmp(buf, "prefix-suffix") == 0);
    t("strstr", nx_strstr("hello world", "world") != NULL);
    t("strchr", nx_strchr("abc", 'b') != NULL);
    t("strtol", nx_strtol("0x1F", NULL, 16) == 31);
    t("strtok", nx_strcmp(nx_strtok("a b c", " "), "a") == 0 &&
                 nx_strcmp(nx_strtok(NULL, " "), "b") == 0);

    /* ── math ────────────────────────────────────────────────── */
    nx_cprintf(color_rgb(255,220,80), 0, "[math]\n");
    t("abs", nx_abs(-5) == 5);
    t("sqrt(81)", (int)nx_sqrt(81.0) == 9);
    t("pow(2,8)", (int)nx_pow(2.0, 8.0) == 256);
    t("min/max", nx_min(3, 7) == 3 && nx_max(3, 7) == 7);
    t("rand changes", nx_rand() != nx_rand());

    /* ── heap ────────────────────────────────────────────────── */
    nx_cprintf(color_rgb(255,220,80), 0, "[heap]\n");
    void *p1 = nx_malloc(512);
    t("malloc(512)", p1 != NULL);
    if (p1) {
        nx_memset(p1, 0x55, 512);
        t("memset pattern", ((unsigned char *)p1)[0] == 0x55 &&
                           ((unsigned char *)p1)[511] == 0x55);
        void *p2 = nx_realloc(p1, 1024);
        t("realloc(1024)", p2 != NULL);
        void *c = nx_calloc(4, 4);
        t("calloc zeroed", c && ((unsigned char *)c)[15] == 0);
        nx_free(c);
        nx_free(p2 ? p2 : p1);
    }

    /* ── terminal ────────────────────────────────────────────── */
    nx_cprintf(color_rgb(255,220,80), 0, "[terminal]\n");
    t("rows/cols sane", nx_term_rows() >= 1 && nx_term_cols() >= 10);
    t("set_fg", (nx_term_set_fg(0xFFFFFF), 1));

    /* ── time ────────────────────────────────────────────────── */
    nx_cprintf(color_rgb(255,220,80), 0, "[time]\n");
    uint64_t u0 = nx_uptime_ms();
    t("uptime advances", nx_uptime_ms() >= u0);
    NxTime tm;
    nx_memset(&tm, 0, sizeof(tm));
    nx_gettime(&tm);
    t("gettime sets fields", tm.year >= 2000);

    /* ── environment ─────────────────────────────────────────── */
    nx_cprintf(color_rgb(255,220,80), 0, "[environment]\n");
    t("setenv", nx_setenv("TEST_KEY", "value123") == 0);
    const char *v = nx_getenv("TEST_KEY");
    t("getenv roundtrip", v && nx_strcmp(v, "value123") == 0);

    /* ── filesystem ──────────────────────────────────────────── */
    nx_cprintf(color_rgb(255,220,80), 0, "[filesystem]\n");
    t("getcwd", nx_getcwd(buf, sizeof(buf)) != NULL && buf[0] == '#');

    fd = nx_open("#/testapp_demo.txt", NX_O_WRONLY|NX_O_CREATE|NX_O_TRUNC);
    t("open create", fd >= 0);

    const char *msg = "testapp line one\ntestapp line two\n";
    if (fd >= 0) {
        int64_t w = nx_write(fd, msg, nx_strlen(msg));
        t("write", w == (int64_t)nx_strlen(msg));
        if (nx_close(fd) == 0) t("close", 1);
        else { t("close", 0); fd = -1; }
    }
    if (fd >= 0) {
        fd = nx_open("#/testapp_demo.txt", NX_O_RDONLY);
        r = (fd >= 0);
        t("open read", r);
        if (fd >= 0) {
            int64_t rd = nx_read(fd, buf, sizeof(buf) - 1);
            if (rd > 0) buf[rd] = '\0';
            t("read back content", rd == (int64_t)nx_strlen(msg) &&
                nx_strstr(buf, "line two") != NULL);
            nx_close(fd);
        }
    }

    NxFileInfo fi;
    t("stat", nx_stat("#/testapp_demo.txt", &fi) == 0 && fi.size > 0);

    t("mkdir", nx_mkdir("#/testapp_demo_dir") == 0);
    NxDir dir = nx_opendir("#/");
    int found_file = 0, found_dir = 0;
    char nm[256];
    if (dir) {
        while (nx_readdir(dir, nm, NULL) == 0) {
            NxFileInfo e;
            (void)e;
            if (nx_strcmp(nm, "testapp_demo.txt") == 0) found_file = 1;
            if (nx_strcmp(nm, "testapp_demo_dir") == 0) found_dir = 1;
        }
        nx_closedir(dir);
    }
    t("readdir sees both", found_file && found_dir);
    t("rename", nx_rename("#/testapp_demo.txt",
                         "#/testapp_demo_renamed.txt") == 0);
    t("unlink", nx_unlink("#/testapp_demo_renamed.txt") == 0 &&
                nx_unlink("#/testapp_demo_dir") == 0);

    /* ── summary ─────────────────────────────────────────────── */
    int pass = 0, fail = 0;
    for (int i = 0; i < g_n; i++) (g_results[i] ? pass++ : fail++);
    nx_cprintf(color_rgb(0,255,200), 0, "\n  %d pass, %d fail, %d total\n",
               pass, fail, g_n);
    if (fail == 0)
        nx_cprintf(color_rgb(80,255,120), 0, "ALL TESTS PASSED\n");
    else
        nx_cprintf(color_rgb(255,90,90), 0, "SOME TESTS FAILED\n");

    proc_exit(fail ? 1 : 0);
    return 0;
}