/*
 * libnotux — dynamic library loading (stubs for now)
 */

#include <notux/libc.h>

void *nx_dlopen(const char *path) {
    (void)path;
    return NULL;
}

void *nx_dlsym(void *handle, const char *symbol) {
    (void)handle;
    (void)symbol;
    return NULL;
}

void nx_dlclose(void *handle) {
    (void)handle;
}