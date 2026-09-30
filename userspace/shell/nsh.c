/*
 * nsh.c — Notux shell: main loop, input, prompt, helpers.
 * Command logic lives in nsh_enhanced.c.
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/proc.h>
#include <notux/terminal.h>
#include <notux/color.h>
#include "nsh.h"

char g_cwd[NSH_PATH_MAX];
char g_history[NSH_HISTORY_MAX][NSH_PATH_MAX];
int  g_history_count = 0;
int  g_history_pos   = -1;

#define CLR_NAME   color_rgb(255,220,80)
#define CLR_DIR    color_rgb(0,240,220)
#define CLR_OK     color_rgb(80,255,120)
#define CLR_ERR    color_rgb(255,90,90)
#define CLR_INFO   color_rgb(200,200,210)
#define CLR_PROMPT color_rgb(0,255,200)

void nsh_err(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char tmp[384];
    nx_vsprintf(tmp, fmt, ap);
    va_end(ap);
    nx_cprintf(CLR_ERR, 0, "nsh: %s", tmp);
}

void nsh_ok(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char tmp[384];
    nx_vsprintf(tmp, fmt, ap);
    va_end(ap);
    nx_cprintf(CLR_OK, 0, "%s", tmp);
}

void nsh_print_banner(void) {
    nx_cprintf(CLR_NAME, 0, "Notux Shell %s  (built-in commands; apps in #/bin)\n",
               NSH_VERSION);
    nx_cprintf(CLR_INFO, 0, "Type 'help' for a command list. 'exit' leaves.\n");
}

/* Split a line into whitespace-separated arguments. */
void nsh_split(char *line, char **args, int *argc, int max) {
    int n = 0;
    char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t') *p++ = '\0';
        if (!*p) break;
        if (n < max) args[n++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    *argc = n;
}

/* Resolve a command name to a path under #/bin (or absolute). */
int nsh_find_program(const char *name, char *out, int outn) {
    if (name[0] == '.' || name[0] == '/' || name[0] == '#')
        return 0;   /* caller handles explicit paths */
    nx_snprintf(out, outn, "#/bin/%s", name);
    NxFileInfo fi;
    if (nx_stat(out, &fi) == 0 && !(fi.flags & NX_FS_DIR)) return 1;
    return 0;
}

/* Read a line WITHOUT consuming control sequences we can't handle.
   The kernel's terminal driver returns a newline-terminated line
   via nx_gets; we use it directly and keep the shell history safe. */
int nsh_getline(char *buf, int n) {
    buf[0] = '\0';
    if (!nx_gets(buf, n)) return -1;
    return (int)nx_strlen(buf);
}

int nsh_getline_noecho(char *buf, int n) {
    return nx_term_read_password(buf, n);
}

void nsh_save_history_line(const char *line) {
    if (!line || line[0] == '\0') return;
    if (g_history_count > 0 &&
        nx_strcmp(g_history[g_history_count - 1], line) == 0)
        return;
    if (g_history_count == NSH_HISTORY_MAX) {
        for (int i = 1; i < g_history_count; i++)
            nx_strcpy(g_history[i - 1], g_history[i]);
        g_history_count--;
    }
    nx_strncpy(g_history[g_history_count], line, NSH_PATH_MAX - 1);
    g_history_count++;
    g_history_pos = g_history_count;
}

static int g_prompt_w;                  /* visible width of the last prompt */

static void print_prompt(void) {
    char uname[24];
    char shown[NSH_PATH_MAX];
    char full[NSH_PATH_MAX + 48];
    size_t len;

    nx_sprintf(uname, "user%d", nx_getuid());
    /* Hide the root's trailing slash and end with '>' so the root prompt
     * reads "#> " instead of the doubled-looking "#/# ". */
    nx_strncpy(shown, g_cwd, sizeof(shown) - 1);
    shown[sizeof(shown) - 1] = '\0';
    len = nx_strlen(shown);
    while (len > 1 && shown[len - 1] == '/') shown[--len] = '\0';

    /* Measure the whole line first; fragments then go out coloured. */
    nx_snprintf(full, sizeof(full), "%s@notux:%s> ", uname, shown);
    g_prompt_w = (int)nx_strlen(full);

    nx_cprintf(CLR_NAME, 0, "%s", uname);
    nx_cprintf(CLR_INFO, 0, "@notux:");
    nx_cprintf(CLR_DIR, 0, "%s", shown);
    nx_cprintf(CLR_PROMPT, 0, "> ");
}

/* Collapse the previously drawn prompt before laying a fresh one so idle
 * poll cycles heal in place instead of marching rightward across the row. */
static void nsh_clear_old_prompt(void) {
    int i;
    nx_putchar('\r');
    for (i = 0; i < g_prompt_w; i++) nx_putchar(' ');
    nx_putchar('\r');
}

static void seed_path(void) {
    if (nx_getenv("PATH") == NULL)
        nx_setenv("PATH", "#/bin");
}

int main(void) {
    nx_puts("[nsh] in\n");                 /* one-shot boot-progress marker */
    char *cwd = nx_getcwd(g_cwd, sizeof(g_cwd));
    if (!cwd || g_cwd[0] == '\0') nx_strcpy(g_cwd, "#/");

    nx_term_reset_color();
    nsh_print_banner();
    seed_path();

    char line[300];
    int first = 1;
    for (;;) {
        if (!first) nsh_clear_old_prompt();     /* repaint in place */
        first = 0;
        print_prompt();
        if (nsh_getline(line, sizeof(line)) < 0) break;

        /* trim trailing whitespace/newline */
        int len = (int)nx_strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == ' ' ||
                           line[len - 1] == '\t' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        nsh_save_history_line(line);

        if (nsh_dispatch(line) == 99) break;  /* exit command */
    }

    nx_cprintf(CLR_INFO, 0, "\nlogout.\n");
    proc_exit(0);
    return 0;
}
