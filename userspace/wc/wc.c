/*
 * wc.c — count lines, words, and bytes of stdin/file(s).
 *
 * wc [PATH ...]
 */

#include <notux/libc.h>
#include <notux/fs.h>
#include <notux/proc.h>

static void count_one(const char *path, uint64_t *lines, uint64_t *words,
                      uint64_t *bytes) {
    int fd = nx_open(path, NX_O_RDONLY);
    if (fd < 0) {
        nx_puts("wc: cannot open ");
        nx_puts(path);
        nx_putchar('\n');
        return;
    }

    char buf[512];
    int in_word = 0;
    for (;;) {
        int64_t r = nx_read(fd, buf, sizeof(buf));
        if (r <= 0) break;
        for (int64_t i = 0; i < r; i++) {
            char c = buf[i];
            (*bytes)++;
            if (c == '\n') (*lines)++;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                (*words)++;
            }
        }
    }
    nx_close(fd);
}

int main(int argc, char **argv, char **envp) {
    (void)envp;

    int shown = 0;
    uint64_t tl = 0, tw = 0, tb = 0;

    if (argc <= 1) {
        count_one("#/dev/stdin", &tl, &tw, &tb);
        nx_printf("%llu %llu %llu\n", (unsigned long long)tl,
                  (unsigned long long)tw, (unsigned long long)tb);
        proc_exit(0);
        return 0;
    }

    char line[96];
    for (int i = 1; i < argc; i++) {
        uint64_t l = 0, w = 0, b = 0;
        count_one(argv[i], &l, &w, &b);
        nx_sprintf(line, "%llu %llu %llu %s\n", (unsigned long long)l,
                   (unsigned long long)w, (unsigned long long)b, argv[i]);
        nx_puts(line);
        tl += l; tw += w; tb += b;
        shown++;
    }

    if (shown > 1) {
        nx_sprintf(line, "%llu %llu %llu total\n", (unsigned long long)tl,
                   (unsigned long long)tw, (unsigned long long)tb);
        nx_puts(line);
    }
    proc_exit(0);
    return 0;
}
