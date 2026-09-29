/*
 * libnotux — memory allocator
 *
 * Simple first-fit free-list allocator over SYS_MMAP'd regions.
 */

#include <notux/libc.h>
#include <notux/syscalls.h>
#include "nx.h"

#define POOL_PAGES    64
#define CHUNK_ALIGN   16
#define HEADER_OVERHEAD 16

typedef struct Chunk {
    size_t size;
    struct Chunk *next;
} Chunk;

static Chunk *g_free_list = NULL;
static uint64_t g_arena_used = 0;
static uint64_t g_arena_size = 0;
static uint64_t g_arena_base = 0;

static void *arena_alloc(size_t bytes) {
    bytes = (bytes + CHUNK_ALIGN - 1) & ~(size_t)(CHUNK_ALIGN - 1);
    if (g_arena_used + bytes > g_arena_size) {
        long r = nx_syscall(SYS_MMAP, 0, POOL_PAGES * 4096, 0);
        if (r <= 0 || r == -12) return NULL;
        g_arena_base = (uint64_t)r;
        g_arena_used = 0;
        g_arena_size = POOL_PAGES * 4096;
    }
    void *p = (void *)(uintptr_t)(g_arena_base + g_arena_used);
    g_arena_used += bytes;
    return p;
}

void *nx_malloc(size_t size) {
    if (size == 0) size = 1;
    size = (size + CHUNK_ALIGN - 1) & ~(size_t)(CHUNK_ALIGN - 1);

    Chunk **pp = &g_free_list;
    while (*pp) {
        Chunk *c = *pp;
        if (c->size >= size) {
            *pp = c->next;
            c->size = size;
            return (void *)(c + 1);
        }
        pp = &c->next;
    }

    void *raw = arena_alloc(size + HEADER_OVERHEAD);
    if (!raw) return NULL;
    Chunk *c = (Chunk *)raw;
    c->size = size;
    return (void *)(c + 1);
}

void *nx_calloc(size_t n, size_t size) {
    size_t total = n * size;
    void *p = nx_malloc(total);
    if (p) nx_memset(p, 0, total);
    return p;
}

void *nx_realloc(void *ptr, size_t new_size) {
    if (!ptr) return nx_malloc(new_size);
    Chunk *c = ((Chunk *)ptr) - 1;
    if (c->size >= new_size) return ptr;
    void *np = nx_malloc(new_size);
    if (!np) return NULL;
    nx_memcpy(np, ptr, c->size);
    nx_free(ptr);
    return np;
}

void nx_free(void *ptr) {
    if (!ptr) return;
    Chunk *c = ((Chunk *)ptr) - 1;
    c->next = g_free_list;
    g_free_list = c;
}

void *nx_memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *nx_memset(void *dst, int val, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    while (n--) *d++ = (unsigned char)val;
    return dst;
}

int nx_memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;
    while (n--) {
        if (*x != *y) return (int)*x - (int)*y;
        x++; y++;
    }
    return 0;
}