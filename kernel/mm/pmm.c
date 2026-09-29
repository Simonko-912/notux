/*
 * Notux OS — Physical Memory Manager
 * kernel/mm/pmm.c
 *
 * Bitmap allocator. Max 4 GiB physical RAM = 128 KiB bitmap.
 * (Expandable later; 4 GiB covers all QEMU test configs.)
 */
#include "pmm.h"
#include "../kernel.h"
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE           4096ULL
/* 4 GiB / 4096 bytes/page = 1048576 pages, / 8 bits = 131072 bytes */
#define PMM_MAX_PAGES       (4ULL * 1024 * 1024 * 1024 / 4096)
#define BITMAP_BYTES        (PMM_MAX_PAGES / 8)

static uint8_t  pmm_bitmap[BITMAP_BYTES]; /* 128 KiB in BSS */
static uint64_t pmm_total  = 0;
static uint64_t pmm_free   = 0;
static uint64_t pmm_cursor = 0;

static inline void bm_set(uint64_t p){pmm_bitmap[p/8] |=  (uint8_t)(1u<<(p%8));}
static inline void bm_clr(uint64_t p){pmm_bitmap[p/8] &= (uint8_t)~(1u<<(p%8));}
static inline int  bm_tst(uint64_t p){return(pmm_bitmap[p/8]>>(p%8))&1;}

static int uefi_usable(uint32_t t){return t==7||t==2||t==3;}

void pmm_init(void *map, uint64_t map_size, uint64_t desc_size) {
    /* Mark everything used */
    for(uint64_t i=0;i<BITMAP_BYTES;i++) pmm_bitmap[i]=0xFF;
    uint64_t n=map_size/desc_size;
    uint8_t *e=(uint8_t*)map;
    for(uint64_t i=0;i<n;i++,e+=desc_size){
        uint32_t type      =*(uint32_t*)(e+0);
        uint64_t phys      =*(uint64_t*)(e+8);
        uint64_t num_pages =*(uint64_t*)(e+24);
        pmm_total+=num_pages;
        if(!uefi_usable(type)||phys==0) continue;
        for(uint64_t p=0;p<num_pages;p++){
            uint64_t pn=phys/PAGE_SIZE+p;
            if(pn<PMM_MAX_PAGES){bm_clr(pn);pmm_free++;}
        }
    }
    /* Reserve low 2 MiB (BIOS, our kernel at 1MiB, etc.) */
    for(uint64_t p=0;p<512;p++) bm_set(p);
}

/* Mark a physical range as used. The kernel image is larger than the
 * hard-coded 2 MiB above, so anything living in BSS past that boundary
 * would otherwise be handed out to the heap and silently overwritten. */
void pmm_reserve(uint64_t phys, uint64_t size) {
    if (!size) return;
    uint64_t first = phys / PAGE_SIZE;
    uint64_t last  = (phys + size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (last > PMM_MAX_PAGES) last = PMM_MAX_PAGES;
    for (uint64_t p = first; p < last; p++) {
        if (!bm_tst(p)) { bm_set(p); pmm_free--; }
    }
}

uint64_t pmm_alloc_page(void){
    for(uint64_t i=0;i<PMM_MAX_PAGES;i++){
        uint64_t p=(pmm_cursor+i)%PMM_MAX_PAGES;
        if(!bm_tst(p)){bm_set(p);pmm_cursor=p+1;pmm_free--;return p*PAGE_SIZE;}
    }
    return PMM_OOM;
}

uint64_t pmm_alloc_pages(uint64_t count){
    if(!count)return PMM_OOM;
    uint64_t run=0,start=0;
    for(uint64_t p=1;p<PMM_MAX_PAGES;p++){
        if(!bm_tst(p)){if(!run)start=p;if(++run==count){
            for(uint64_t k=start;k<start+count;k++)bm_set(k);
            pmm_free-=count;return start*PAGE_SIZE;
        }}else run=0;
    }
    return PMM_OOM;
}

void pmm_free_page(uint64_t phys){
    uint64_t p=phys/PAGE_SIZE;
    if(p&&p<PMM_MAX_PAGES&&bm_tst(p)){bm_clr(p);pmm_free++;}
}
void pmm_free_pages(uint64_t phys,uint64_t count){
    for(uint64_t i=0;i<count;i++)pmm_free_page(phys+i*PAGE_SIZE);
}
uint64_t pmm_free_count(void) {return pmm_free;}
uint64_t pmm_total_count(void){return pmm_total;}
