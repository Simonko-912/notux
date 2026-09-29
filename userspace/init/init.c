/* Notux init - user-space PID 1 */

#include <notux/libc.h>
#include <notux/proc.h>

int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv; (void)envp;

    nx_puts("init: Notux PID 1 up.\n");

    const char *nsh_argv[] = { "nsh", NULL };
    nx_pid_t shell = nx_exec("#/bin/nsh", nsh_argv, NULL);
    if (shell < 0) {
        nx_puts("init: failed to start nsh\n");
        for (;;) nx_sleep(1000);
    }
    nx_puts("init: shell started, watching for exits.\n");

    for (;;) {
        int status;
        nx_pid_t child = nx_wait(&status);
        if (child >= 0) {
            nx_puts("init: shell exited, restarting.\n");
            nx_exec("#/bin/nsh", nsh_argv, NULL);
        } else {
            nx_sleep(50);
        }
    }
}
