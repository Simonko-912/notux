#pragma once
#include <stdint.h>
#include <stddef.h>
#define PMM_OOM 0ULL
void     pmm_init(void *map, uint64_t map_size, uint64_t desc_size);
void     pmm_reserve(uint64_t phys, uint64_t size);
uint64_t pmm_alloc_page(void);
uint64_t pmm_alloc_pages(uint64_t count);
void     pmm_free_page(uint64_t phys);
void     pmm_free_pages(uint64_t phys, uint64_t count);
uint64_t pmm_free_count(void);
uint64_t pmm_total_count(void);
