#pragma once
#include <notux/libc.h>
#define color_rgb(r,g,b)        nx_color(r,g,b)
#define term_set_fg(c)          nx_term_set_fg(c)
#define term_set_bg(c)          nx_term_set_bg(c)
#define term_reset_color()      nx_term_reset_color()
