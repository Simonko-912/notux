#pragma once
#include <notux/libc.h>
#define proc_getenv(name)       nx_getenv(name)
#define proc_getuid()           nx_getuid()
#define proc_exec(path,argv,envp,uid) nx_exec(path,argv,envp)
#define proc_exit(code)         nx_exit(code)
