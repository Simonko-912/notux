/*
 * echo.c — print arguments to stdout, one line.
 */

#include <notux/libc.h>
#include <notux/proc.h>

int main(int argc, char **argv, char **envp) {
    (void)envp;
    for (int i = 1; i < argc; i++) {
        if (i > 1) nx_putchar(' ');
        nx_puts(argv[i]);
    }
    proc_exit(0);
    return 0;
}
