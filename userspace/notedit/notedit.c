/*
 * notedit.c — quick notes tool.
 * Notes live in #/usr/notes.txt.
 *
 *   notedit add <text>    append a note
 *   notedit list          print all notes
 *   notedit clear         wipe the file
 *   notedit del <n>       delete note number n
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/proc.h>
#include <notux/color.h>

#define NOTES_PATH "#/usr/notes.txt"
#define MAX_BUF 32768

static int ensure_dir(void) {
    NxFileInfo fi;
    if (nx_stat("#/usr", &fi) == 0) return 0;
    return nx_mkdir("#/usr");
}

int main(int argc, char **argv, char **envp) {
    (void)envp;
    ensure_dir();

    if (argc < 2 || nx_strcmp(argv[1], "help") == 0 ||
        nx_strcmp(argv[1], "--help") == 0) {
        nx_cprintf(color_rgb(0, 240, 220), 0,
                   "notedit: add <text> | list | del <n> | clear | help\n");
        return 0;
    }

    if (nx_strcmp(argv[1], "add") == 0) {
        if (argc < 3) { nx_puts("usage: notedit add <text>\n"); return 1; }
        int fd = nx_open(NOTES_PATH, NX_O_WRONLY | NX_O_CREATE | NX_O_APPEND);
        if (fd < 0) { nx_puts("cannot open notes\n"); return 1; }
        for (int i = 2; i < argc; i++) {
            nx_write(fd, argv[i], nx_strlen(argv[i]));
            if (i + 1 < argc) nx_write(fd, " ", 1);
        }
        nx_write(fd, "\n", 1);
        nx_close(fd);
        nx_cprintf(color_rgb(80, 255, 120), 0, "note added.\n");
        return 0;
    }

    if (nx_strcmp(argv[1], "clear") == 0) {
        int fd = nx_open(NOTES_PATH, NX_O_WRONLY | NX_O_CREATE | NX_O_TRUNC);
        if (fd < 0) { nx_puts("cannot open notes\n"); return 1; }
        nx_close(fd);
        nx_cprintf(color_rgb(80, 255, 120), 0, "notes cleared.\n");
        return 0;
    }

    /* need file contents for list/del */
    int fd = nx_open(NOTES_PATH, NX_O_RDONLY);
    if (fd < 0) { nx_puts("no notes yet.\n"); return 0; }
    char buf[MAX_BUF];
    int64_t n = nx_read(fd, buf, sizeof(buf) - 1);
    nx_close(fd);
    if (n <= 0) { nx_puts("no notes yet.\n"); return 0; }
    buf[n] = '\0';

    if (nx_strcmp(argv[1], "list") == 0) {
        nx_cprintf(color_rgb(0, 240, 220), 0, "notes:\n");
        char *p = buf;
        int i = 0;
        while (*p) {
            char *nl = nx_strchr(p, '\n');
            int len = nl ? (int)(nl - p) : (int)nx_strlen(p);
            char line[MAX_BUF];
            nx_memcpy(line, p, (size_t)len);
            line[len] = '\0';
            nx_cprintf(color_rgb(255, 255, 255), 0, "  %2d. %s\n", ++i, line);
            if (!nl) break;
            p = nl + 1;
        }
        return 0;
    }

    if (nx_strcmp(argv[1], "del") == 0 && argc >= 3) {
        int target = (int)nx_strtol(argv[2], NULL, 10);
        char out[MAX_BUF];
        int outlen = 0;
        char *p = buf;
        int i = 0;
        while (*p) {
            char *nl = nx_strchr(p, '\n');
            int len = nl ? (int)(nl - p) : (int)nx_strlen(p);
            i++;
            if (i != target) {
                nx_memcpy(out + outlen, p, (size_t)len);
                outlen += len;
                out[outlen++] = '\n';
            }
            if (!nl) break;
            p = nl + 1;
        }
        (void)outlen;
        int wfd = nx_open(NOTES_PATH, NX_O_WRONLY | NX_O_CREATE | NX_O_TRUNC);
        if (wfd < 0) return 1;
        nx_write(wfd, out, (size_t)(out[0] ? outlen : 0));
        nx_close(wfd);
        nx_cprintf(color_rgb(80, 255, 120), 0, "note %d deleted.\n", target);
        return 0;
    }

    nx_puts("usage: notedit add <text> | list | del <n> | clear | help\n");
    return 1;
}