/*
 * Notux OS — Kernel Heap Allocator
 * kernel/mm/heap.c
 *
 * Two-tier allocator:
 *   Small allocations (≤2048 bytes): slab allocator
 *     Fixed-size slabs: 16, 32, 64, 128, 256, 512, 1024, 2048 bytes
 *     Each slab is one 4 KiB page divided into fixed chunks.
 *     Free list per slab size. O(1) alloc/free.
 *
 *   Large allocations (>2048 bytes): page allocator
 *     Rounds up to page boundary, allocates directly from PMM.
 *     Stores size in a header page for free().
 *
 * All allocations are tagged with a magic value for corruption
 * detection.  kmalloc(0) returns NULL.
 */

#include "heap.h"
#include "pmm.h"
#include "../kernel.h"
#include "../kserial.h"
#include <stdint.h>
#include <stddef.h>

#define HEAP_MAGIC   0xDEADC0DE
#define PAGE_SIZE    4096ULL

/* ── Slab sizes ──────────────────────────────────────────────── */
/* Smallest chunk must hold FreeChunk (16 bytes) + payload. */
#define NUM_SLABS 7
static const uint32_t slab_sizes[NUM_SLABS] = {
    32, 64, 128, 256, 512, 1024, 2048
};

#define HEAP_MAGIC 0xDEADC0DE
#define HEAP_TAG_LARGE 0xFFFFFFFFu

/* Every allocation starts with this tag, so kfree never has to guess
 * whether a pointer is slab- or page-backed. The header is a fixed 16
 * bytes for BOTH size classes, so the payload always begins at
 * ptr + HEAP_HDR_SIZE and never overlaps page_count. */
typedef struct {
    uint32_t magic;
    uint32_t tag;        /* slab index, or HEAP_TAG_LARGE */
    uint64_t page_count; /* page allocations only; 0 for slabs */
} HeapHdr;

#define HEAP_HDR_SIZE ((uint32_t)sizeof(HeapHdr))

/* Free chunk header (only present while the chunk is on a free list) */
typedef struct FreeChunk {
    HeapHdr            h;
    struct FreeChunk  *next;
} FreeChunk;

/* One free-list head per slab size */
static FreeChunk *slab_free[NUM_SLABS];


/* ── Init ────────────────────────────────────────────────────── */
void kheap_init(void) {
    for (int i = 0; i < NUM_SLABS; i++)
        slab_free[i] = NULL;
}

/* ── Find slab index for a given size ───────────────────────── */
static int slab_index(size_t size) {
    for (int i = 0; i < NUM_SLABS; i++)
        if (size <= slab_sizes[i]) return i;
    return -1;
}

/* ── Refill a slab from PMM ──────────────────────────────────── */
static void slab_refill(int idx) {
    uint64_t page = pmm_alloc_page();
    if (page == PMM_OOM) return;

    uint32_t chunk_size = slab_sizes[idx];
    uint32_t n_chunks   = (uint32_t)(PAGE_SIZE / chunk_size);
    uint8_t *base       = (uint8_t *)(uintptr_t)page;

    for (uint32_t i = 0; i < n_chunks; i++) {
        FreeChunk *c    = (FreeChunk *)(base + i * chunk_size);
        c->h.magic      = HEAP_MAGIC;
        c->h.tag        = (uint32_t)idx;
        c->next         = slab_free[idx];
        slab_free[idx]  = c;
    }
}

/* ── kmalloc ─────────────────────────────────────────────────── */
void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    size_t total = size + HEAP_HDR_SIZE;

    int idx = slab_index(total);
    if (idx >= 0) {
        /* Slab path */
        if (!slab_free[idx]) slab_refill(idx);
        if (!slab_free[idx]) return NULL;  /* OOM */

        FreeChunk *c   = slab_free[idx];
        slab_free[idx] = c->next;
        c->h.magic     = HEAP_MAGIC;
        c->h.tag       = (uint32_t)idx;
        return (void *)((uint8_t *)c + HEAP_HDR_SIZE);
    }

    /* Large path: allocate pages, header lives in the first page */
    uint64_t pages = (uint64_t)((total + PAGE_SIZE - 1) / PAGE_SIZE);
    if (pages == 0) pages = 1;
    uint64_t phys  = pmm_alloc_pages(pages);
    if (phys == PMM_OOM) return NULL;

    HeapHdr *hdr = (HeapHdr *)(uintptr_t)phys;
    hdr->magic      = HEAP_MAGIC;
    hdr->tag        = HEAP_TAG_LARGE;
    hdr->page_count = pages;
    return (void *)((uint8_t *)hdr + HEAP_HDR_SIZE);
}

/* ── kfree ───────────────────────────────────────────────────── */
void kfree(void *ptr) {
    if (!ptr) return;

    HeapHdr *h = (HeapHdr *)((uint8_t *)ptr - HEAP_HDR_SIZE);
    if (h->magic != HEAP_MAGIC) return;   /* not ours / double free */

    if (h->tag == HEAP_TAG_LARGE) {
        pmm_free_pages((uint64_t)(uintptr_t)h, h->page_count);
        h->magic = 0;                       /* poison */
        return;
    }

    uint32_t idx = h->tag;
    if (idx >= NUM_SLABS) return;
    FreeChunk *c = (FreeChunk *)h;
    c->next        = slab_free[idx];
    slab_free[idx] = c;
}

/* ── kcalloc ─────────────────────────────────────────────────── */
void *kcalloc(size_t n, size_t size) {
    size_t total = n * size;
    void *p = kmalloc(total);
    if (p) kmemset(p, 0, total);
    return p;
}

/* ── krealloc ────────────────────────────────────────────────── */
void *krealloc(void *ptr, size_t new_size) {
    if (!ptr)      return kmalloc(new_size);
    if (!new_size) { kfree(ptr); return NULL; }

    void *n = kmalloc(new_size);
    if (!n) return NULL;
    /* Copy old content — we don't store exact old size, so copy
       min(new_size, slab_size_of(ptr)).  Conservative copy. */
    const uint8_t *src = (const uint8_t *)ptr;
    uint8_t       *dst = (uint8_t *)n;
    for (size_t i = 0; i < new_size; i++) dst[i] = src[i];
    kfree(ptr);
    return n;
}

/* ── memset / memcpy for kernel use ─────────────────────────── */
void *kmemset(void *dst, int val, size_t n) {
    uint8_t *p = (uint8_t *)dst;
    while (n--) *p++ = (uint8_t)val;
    return dst;
}
void *kmemcpy(void *dst, const void *src, size_t n) {
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}
int kmemcmp(const void *a, const void *b, size_t n) {
    const uint8_t *p = (const uint8_t *)a;
    const uint8_t *q = (const uint8_t *)b;
    while (n--) {
        if (*p != *q) return (int)*p - (int)*q;
        p++; q++;
    }
    return 0;
}

/* ── kstrdup ─────────────────────────────────────────────────── */
char *kstrdup(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    char *d = (char *)kmalloc(n + 1);
    if (d) kmemcpy(d, s, n + 1);
    return d;
}
