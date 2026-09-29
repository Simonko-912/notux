/*
 * head.c — print the first lines of a file (default 10).
 *
 * head [-n NUM] PATH
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/proc.h>

static void emit(const char *path, int max_lines) {
    int fd = nx_open(path, NX_O_RDONLY);
    if (fd < 0) {
        nx_puts("head: cannot open ");
        nx_puts(path);
        nx_putchar('\n');
        return;
    }

    char buf[512];
    int lines = 0;
    for (;;) {
        int64_t r = nx_read(fd, buf, sizeof(buf) - 1);
        if (r <= 0) break;
        for (int64_t i = 0; i < r && lines < max_lines; i++) {
            nx_putchar(buf[i]);
            if (buf[i] == '\n') lines++;
        }
        if (lines >= max_lines) break;
    }
    nx_close(fd);
}

int main(int argc, char **argv, char **envp) {
    (void)envp;
    int max_lines = 10;
    int first_path = 1;

    for (int i = 1; i < argc; i++) {
        if (nx_strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_lines = (int)nx_strtol(argv[++i], NULL, 10);
            if (max_lines <= 0) max_lines = 10;
        } else {
            first_path = i;
            break;
        }
    }

    if (first_path >= argc) {
        nx_puts("usage: head [-n NUM] PATH\n");
        proc_exit(1);
        return 0;
    }

    emit(argv[first_path], max_lines);
    proc_exit(0);
    return 0;
}
