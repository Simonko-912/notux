#pragma once
#include <stdint.h>
#include <stddef.h>
#define VMM_FLAG_RW   0x01
#define VMM_FLAG_USER 0x02
#define VMM_FLAG_NX   0x04
#define VMM_FLAG_WT   0x08
void      vmm_init(uint64_t kernel_phys, uint64_t kernel_virt, uint64_t kernel_size);
int       vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint32_t flags);
int       vmm_map_range(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t pages, uint32_t flags);
void      vmm_unmap_page(uint64_t *pml4, uint64_t virt);
uint64_t  vmm_virt_to_phys(uint64_t *pml4, uint64_t virt);
void      vmm_switch(uint64_t *pml4);
uint64_t *vmm_new_space(void);
uint64_t *vmm_clone_space(uint64_t *src);
void      vmm_free_space(uint64_t *pml4);
uint64_t *vmm_kernel_pml4(void);
