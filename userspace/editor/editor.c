/*
 * editor.c — Notux line editor.
 *
 *   editor <file>          edit a file (created if missing)
 *
 * Commands (first char = action):
 *   P                      print all lines
 *   G<line>                print one line
 *   R<line>[<text>]        replace a line (blank text -> prompt)
 *   I<line>[<text>]        insert before a line
 *   A[<text>]              append at end
 *   D<line>                delete a line
 *   F<text>                find lines containing text
 *   W                      save
 *   Q                      quit  (wq = save + quit)
 *   H                      help
 *
 * Lines may also be entered directly: a bare number at the start
 * is treated as "replace line N".
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/proc.h>
#include <notux/terminal.h>
#include <notux/color.h>

#define MAX_LINES    1024
#define MAX_LINE_LEN 512

#define CYAN   color_rgb(0,   240, 220)
#define YELLOW color_rgb(255, 220, 80)
#define WHITE  color_rgb(255, 255, 255)
#define GREEN  color_rgb(80,  255, 120)
#define RED    color_rgb(255, 90,  90)

static char     g_lines[MAX_LINES][MAX_LINE_LEN];
static int      g_count;
static char     g_path[256];
static int      g_dirty;

static int load_file(const char *path) {
    g_count = 0;
    int fd = fs_open(path, FS_O_RDONLY);
    if (fd < 0) return -1;
    char text[65536];
    int64_t n = fs_read(fd, text, sizeof(text) - 1);
    fs_close(fd);
    if (n <= 0) return 0;
    text[n] = '\0';

    char *p = text;
    while (*p && g_count < MAX_LINES) {
        char *nl = nx_strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)nx_strlen(p);
        if (len > MAX_LINE_LEN - 1) len = MAX_LINE_LEN - 1;
        nx_memcpy(g_lines[g_count], p, (size_t)len);
        g_lines[g_count][len] = '\0';
        g_count++;
        if (!nl) break;
        p = nl + 1;
    }
    return 0;
}

static int save_file(void) {
    int fd = nx_open(g_path, NX_O_WRONLY | NX_O_CREATE | NX_O_TRUNC);
    if (fd < 0) return -1;
    for (int i = 0; i < g_count; i++) {
        if (fs_write(fd, g_lines[i], nx_strlen(g_lines[i])) < 0) break;
        if (fs_write(fd, "\n", 1) < 0) break;
    }
    fs_close(fd);
    g_dirty = 0;
    return 0;
}

static void print_line(int i) {
    if (i < 0 || i >= g_count) {
        nx_cprintf(RED, 0, "no such line.\n");
        return;
    }
    nx_cprintf(YELLOW, 0, "%4d: ", i + 1);
    nx_cprintf(WHITE, 0, "%s\n", g_lines[i]);
}

static void print_all(void) {
    if (!g_count) {
        nx_cprintf(CYAN, 0, "(empty file)\n");
        return;
    }
    for (int i = 0; i < g_count; i++) print_line(i);
    char nbuf[16];
    nx_sprintf(nbuf, "%d", g_count);
    nx_cprintf(CYAN, 0, "--- %s line(s) ---\n", nbuf);
}

static void find_lines(const char *needle) {
    int found = 0;
    for (int i = 0; i < g_count; i++) {
        if (nx_strstr(g_lines[i], needle)) {
            print_line(i);
            found++;
        }
    }
    if (!found) nx_cprintf(RED, 0, "no matches.\n");
}

static void insert_line(int ln, const char *text) {
    if (g_count >= MAX_LINES) { nx_cprintf(RED, 0, "buffer full.\n"); return; }
    if (ln < 1) ln = 1;
    if (ln > g_count + 1) ln = g_count + 1;
    for (int j = g_count; j >= ln; j--)
        nx_strcpy(g_lines[j], g_lines[j - 1]);
    nx_strncpy(g_lines[ln - 1], text, MAX_LINE_LEN - 1);
    g_count++;
    g_dirty = 1;
}

static void delete_line(int ln) {
    if (ln < 1 || ln > g_count) { nx_cprintf(RED, 0, "no such line.\n"); return; }
    for (int i = ln - 1; i < g_count - 1; i++)
        nx_strcpy(g_lines[i], g_lines[i + 1]);
    g_count--;
    g_dirty = 1;
}

static void help(void) {
    nx_cprintf(CYAN, 0, "editor commands:\n");
    nx_cprintf(WHITE, 0,
        "  P         print all lines\n"
        "  G<line>   print one line\n"
        "  R<line> [text]  replace line\n"
        "  I<line> [text]  insert before line\n"
        "  A [text]  append at end\n"
        "  D<line>   delete line\n"
        "  F<text>   find text\n"
        "  W         save    Q  quit without saving\n"
        "  wq        save and quit\n"
        "  H         this help\n");
}

int main(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc < 2) {
        nx_cprintf(RED, 0, "usage: editor <file>\n");
        proc_exit(1);
        return 1;
    }
    nx_strncpy(g_path, argv[1], sizeof(g_path) - 1);

    if (load_file(g_path) == 0)
        nx_cprintf(GREEN, 0, "Loaded: %s (%d lines)\n", g_path, g_count);
    else
        nx_cprintf(GREEN, 0, "New file: %s\n", g_path);

    print_all();

    char line[560];
    char buf[560];
    for (;;) {
        nx_cprintf(CYAN, 0, "ed%s> ", g_dirty ? "*" : "");
        if (!nx_gets(line, sizeof(line))) break;
        if (nx_strcmp(line, "wq") == 0) {
            if (save_file() == 0) nx_cprintf(GREEN, 0, "saved.\n");
            goto quit;
        }
        char cmd = line[0];
        char *arg = line + 1;
        while (*arg == ' ') arg++;

        switch (cmd) {
        case 'P': case 'p': print_all(); break;

        case 'G': case 'g': {
            int ln = (int)nx_strtol(arg, NULL, 10);
            print_line(ln - 1);
            break;
        }
        case 'R': case 'r': {
            char *rest = arg;
            int ln = (int)nx_strtol(rest, &rest, 10);
            while (*rest == ' ') rest++;
            if (ln < 1 || ln > g_count) { nx_cprintf(RED, 0, "no such line.\n"); break; }
            if (*rest) {
                nx_strncpy(g_lines[ln - 1], rest, MAX_LINE_LEN - 1);
            } else {
                nx_cprintf(YELLOW, 0, "%4d: ", ln);
                nx_gets(g_lines[ln - 1], MAX_LINE_LEN);
            }
            g_dirty = 1;
            print_line(ln - 1);
            break;
        }
        case 'I': case 'i': {
            int ln = (int)nx_strtol(arg, NULL, 10);
            nx_strncpy(buf, arg, sizeof(buf) - 1);
            char *t = buf;
            while (*t && *t != ' ') t++;
            if (*t) { *t = '\0'; t++; while (*t == ' ') t++; }
            else { nx_cprintf(YELLOW, 0, "text: "); nx_gets(buf, sizeof(buf)); t = buf; }
            insert_line(ln, t);
            break;
        }
        case 'A': case 'a': {
            nx_strncpy(buf, arg, sizeof(buf) - 1);
            insert_line(g_count + 1, buf[0] ? buf : "");
            break;
        }
        case 'D': case 'd': {
            int ln = (int)nx_strtol(arg, NULL, 10);
            delete_line(ln);
            break;
        }
        case 'F': case 'f': {
            find_lines(arg);
            break;
        }
        case 'W': case 'w': {
            if (save_file() == 0)
                nx_cprintf(GREEN, 0, "saved.\n");
            else
                nx_cprintf(RED, 0, "save failed.\n");
            break;
        }
        case 'Q': case 'q': goto quit;
        case 'H': case 'h': help(); break;

        default:
            /* bare "<line>" e.g. "5" = replace line 5 */
            if (cmd >= '0' && cmd <= '9') {
                char *rest = line;
                int ln = (int)nx_strtol(rest, &rest, 10);
                while (*rest == ' ') rest++;
                if (ln < 1 || ln > g_count) { nx_cprintf(RED, 0, "no such line.\n"); break; }
                if (*rest) {
                    nx_strncpy(g_lines[ln - 1], rest, MAX_LINE_LEN - 1);
                    g_dirty = 1;
                    print_line(ln - 1);
                } else {
                    nx_cprintf(YELLOW, 0, "%4d: ", ln);
                    nx_gets(g_lines[ln - 1], MAX_LINE_LEN);
                    g_dirty = 1;
                }
            } else {
                nx_cprintf(RED, 0, "? (H for help)\n");
            }
        }
    }

quit:
    /* handle wq above: file saved if cmd was 'wq' (won't reach here) */
    if (g_dirty) nx_cprintf(YELLOW, 0, "(unsaved changes discarded)\n");
    nx_puts("bye.\n");
    proc_exit(0);
    return 0;
}