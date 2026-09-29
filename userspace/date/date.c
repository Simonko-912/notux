/*
 * date.c — print the current date/time and uptime.
 */

#include <notux/libc.h>
#include <notux/proc.h>
#include <notux/color.h>

int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv; (void)envp;

    NxTime t;
    nx_gettime(&t);
    if (t.year > 2000)
        nx_cprintf(color_rgb(0, 240, 220), 0,
                   "%04u-%02u-%02u %02u:%02u:%02u\n",
                   t.year, t.month, t.day, t.hour, t.minute, t.second);
    else
        nx_cprintf(color_rgb(255, 220, 80), 0, "clock not set\n");

    nx_cprintf(color_rgb(200, 200, 210), 0, "uptime: %llu s\n",
               (unsigned long long)(nx_uptime_ms() / 1000));
    return 0;
}
