#pragma once
#include <notux/libc.h>
#define net_resolve(host)       nx_resolve(host)
#define net_ping(ip)            nx_ping(ip)
#define color_rgb(r,g,b)        nx_color(r,g,b)
