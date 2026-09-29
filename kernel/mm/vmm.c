/*
 * Notux OS — Virtual Memory Manager
 * kernel/mm/vmm.c
 *
 * Manages 4-level page tables (PML4 → PDPT → PD → PT).
 * Kernel is mapped at higher half: 0xFFFFFFFF80000000
 * Framebuffer is identity-mapped.
 * User processes each get their own address space via vmm_new_space().
 *
 * Page table entry flags:
 *   Bit 0  = Present
 *   Bit 1  = Read/Write
 *   Bit 2  = User-accessible
 *   Bit 3  = Write-through
 *   Bit 4  = Cache-disable
 *   Bit 5  = Accessed
 *   Bit 6  = Dirty
 *   Bit 7  = Huge page (2 MiB)
 *   Bit 63 = No-execute
 */

#include "vmm.h"
#include "pmm.h"
#include "../kernel.h"
#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE       4096ULL
#define PTE_PRESENT     (1ULL << 0)
#define PTE_RW          (1ULL << 1)
#define PTE_USER        (1ULL << 2)
#define PTE_HUGE        (1ULL << 7)
#define PTE_NX          (1ULL << 63)

#define PML4_IDX(va)   (((va) >> 39) & 0x1FF)
#define PDPT_IDX(va)   (((va) >> 30) & 0x1FF)
#define PD_IDX(va)     (((va) >> 21) & 0x1FF)
#define PT_IDX(va)     (((va) >> 12) & 0x1FF)

#define PHYS_MASK       0x000FFFFFFFFFF000ULL

/* Kernel PML4 — set up during vmm_init, referenced by all kernel threads */
static uint64_t *g_kernel_pml4 = NULL;

/* Convert a physical address to a pointer (during early boot we have
   a 1:1 identity map for the lower 4 GiB; after higher-half switch
   we use the kernel offset). */
static inline uint64_t *phys_to_ptr(uint64_t phys) {
    return (uint64_t *)(uintptr_t)(phys);   /* identity during early init */
}

/* Allocate a zeroed page for a new page table level */
static uint64_t alloc_table(void) {
    uint64_t phys = pmm_alloc_page();
    if (phys == PMM_OOM) return 0;
    uint64_t *p = phys_to_ptr(phys);
    for (int i = 0; i < 512; i++) p[i] = 0;
    return phys;
}

/* ── Get or create a page table entry one level down ───────── */
static uint64_t *ensure_table(uint64_t *table, int idx, uint64_t flags) {
    if (!(table[idx] & PTE_PRESENT)) {
        uint64_t phys = alloc_table();
        if (!phys) return NULL;
        table[idx] = phys | flags | PTE_PRESENT;
    }
    return phys_to_ptr(table[idx] & PHYS_MASK);
}

/* ── Map one 4 KiB page ──────────────────────────────────────── */
int vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys,
                 uint32_t flags) {
    uint64_t tbl_flags = PTE_RW | PTE_PRESENT;
    if (flags & VMM_FLAG_USER) tbl_flags |= PTE_USER;

    uint64_t *pdpt = ensure_table(pml4, PML4_IDX(virt), tbl_flags);
    if (!pdpt) return -1;
    uint64_t *pd   = ensure_table(pdpt, PDPT_IDX(virt), tbl_flags);
    if (!pd)   return -1;
    uint64_t *pt   = ensure_table(pd,   PD_IDX(virt),   tbl_flags);
    if (!pt)   return -1;

    uint64_t pte = (phys & PHYS_MASK) | PTE_PRESENT;
    if (flags & VMM_FLAG_RW)   pte |= PTE_RW;
    if (flags & VMM_FLAG_USER) pte |= PTE_USER;
    if (flags & VMM_FLAG_NX)   pte |= PTE_NX;
    if (flags & VMM_FLAG_WT)   pte |= (1ULL << 3);

    pt[PT_IDX(virt)] = pte;
    return 0;
}

/* ── Map a contiguous range ──────────────────────────────────── */
int vmm_map_range(uint64_t *pml4, uint64_t virt, uint64_t phys,
                  uint64_t page_count, uint32_t flags) {
    for (uint64_t i = 0; i < page_count; i++) {
        if (vmm_map_page(pml4, virt + i * PAGE_SIZE,
                          phys + i * PAGE_SIZE, flags) < 0)
            return -1;
    }
    return 0;
}

/* ── Unmap one page ──────────────────────────────────────────── */
void vmm_unmap_page(uint64_t *pml4, uint64_t virt) {
    uint64_t *pdpt = phys_to_ptr(pml4[PML4_IDX(virt)] & PHYS_MASK);
    if (!pdpt) return;
    uint64_t *pd   = phys_to_ptr(pdpt[PDPT_IDX(virt)] & PHYS_MASK);
    if (!pd)   return;
    uint64_t *pt   = phys_to_ptr(pd[PD_IDX(virt)] & PHYS_MASK);
    if (!pt)   return;
    pt[PT_IDX(virt)] = 0;
    /* Flush TLB for this page */
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

/* ── Virtual → physical translation ─────────────────────────── */
uint64_t vmm_virt_to_phys(uint64_t *pml4, uint64_t virt) {
    if (!(pml4[PML4_IDX(virt)] & PTE_PRESENT)) return 0;
    uint64_t *pdpt = phys_to_ptr(pml4[PML4_IDX(virt)] & PHYS_MASK);

    if (!(pdpt[PDPT_IDX(virt)] & PTE_PRESENT)) return 0;
    /* Check for 1 GiB huge page */
    if (pdpt[PDPT_IDX(virt)] & PTE_HUGE)
        return (pdpt[PDPT_IDX(virt)] & PHYS_MASK) | (virt & 0x3FFFFFFF);

    uint64_t *pd = phys_to_ptr(pdpt[PDPT_IDX(virt)] & PHYS_MASK);
    if (!(pd[PD_IDX(virt)] & PTE_PRESENT)) return 0;
    /* Check for 2 MiB huge page */
    if (pd[PD_IDX(virt)] & PTE_HUGE)
        return (pd[PD_IDX(virt)] & PHYS_MASK) | (virt & 0x1FFFFF);

    uint64_t *pt = phys_to_ptr(pd[PD_IDX(virt)] & PHYS_MASK);
    if (!(pt[PT_IDX(virt)] & PTE_PRESENT)) return 0;
    return (pt[PT_IDX(virt)] & PHYS_MASK) | (virt & 0xFFF);
}

/* ── Switch address space (load PML4 into CR3) ───────────────── */
void vmm_switch(uint64_t *pml4) {
    uint64_t phys = (uint64_t)(uintptr_t)pml4;
    __asm__ volatile("mov %0, %%cr3" :: "r"(phys) : "memory");
}

/* ── Create a new address space ─────────────────────────────── */
uint64_t *vmm_new_space(void) {
    uint64_t phys = alloc_table();
    if (!phys) return NULL;
    uint64_t *pml4 = phys_to_ptr(phys);

    /* Share the kernel's lower identity map (needed so the kernel
       can keep running with this CR3 during syscalls/interrupts)
       and the kernel's higher-half mappings. */
    if (g_kernel_pml4) {
        pml4[0] = g_kernel_pml4[0];
        for (int i = 256; i < 512; i++)
            pml4[i] = g_kernel_pml4[i];
    }
    return pml4;
}

/* ── Clone an address space (copy-on-write) ─────────────────── */
uint64_t *vmm_clone_space(uint64_t *src) {
    uint64_t *dst = vmm_new_space();
    if (!dst) return NULL;

    /* Walk lower 256 PML4 entries (user space) */
    for (int i = 0; i < 256; i++) {
        if (!(src[i] & PTE_PRESENT)) continue;
        uint64_t *src_pdpt = phys_to_ptr(src[i] & PHYS_MASK);

        for (int j = 0; j < 512; j++) {
            if (!(src_pdpt[j] & PTE_PRESENT)) continue;
            uint64_t *src_pd = phys_to_ptr(src_pdpt[j] & PHYS_MASK);

            for (int k = 0; k < 512; k++) {
                if (!(src_pd[k] & PTE_PRESENT)) continue;
                if (src_pd[k] & PTE_HUGE) {
                    /* 2 MiB huge page — copy the physical frame */
                    uint64_t virt = ((uint64_t)i << 39) | ((uint64_t)j << 30) |
                                    ((uint64_t)k << 21);
                    uint64_t new_phys = pmm_alloc_pages(512); /* 512 × 4K = 2M */
                    if (new_phys == PMM_OOM) continue;
                    uint64_t old_phys = src_pd[k] & PHYS_MASK;
                    uint8_t *s = (uint8_t *)(uintptr_t)old_phys;
                    uint8_t *d = (uint8_t *)(uintptr_t)new_phys;
                    for (uint64_t b = 0; b < 2*1024*1024; b++) d[b] = s[b];
                    vmm_map_page(dst, virt, new_phys,
                                 VMM_FLAG_RW | VMM_FLAG_USER);
                    continue;
                }

                uint64_t *src_pt = phys_to_ptr(src_pd[k] & PHYS_MASK);
                for (int l = 0; l < 512; l++) {
                    if (!(src_pt[l] & PTE_PRESENT)) continue;
                    uint64_t old_phys = src_pt[l] & PHYS_MASK;
                    uint64_t new_phys = pmm_alloc_page();
                    if (new_phys == PMM_OOM) continue;

                    /* Copy page contents */
                    uint8_t *s = (uint8_t *)(uintptr_t)old_phys;
                    uint8_t *d = (uint8_t *)(uintptr_t)new_phys;
                    for (int b = 0; b < 4096; b++) d[b] = s[b];

                    uint64_t virt = ((uint64_t)i << 39) | ((uint64_t)j << 30) |
                                    ((uint64_t)k << 21) | ((uint64_t)l << 12);
                    uint32_t fl = VMM_FLAG_RW;
                    if (src_pt[l] & PTE_USER) fl |= VMM_FLAG_USER;
                    if (src_pt[l] & PTE_NX)   fl |= VMM_FLAG_NX;
                    vmm_map_page(dst, virt, new_phys, fl);
                }
            }
        }
    }
    return dst;
}

/* ── Free an address space (reclaim user-space pages) ─────────── */
void vmm_free_space(uint64_t *pml4) {
    /* Entry 0 (identity) and entries 256-511 (kernel) are shared with
       the kernel's PML4 and are never freed here. */
    for (int i = 1; i < 256; i++) { /* user space */
        if (!(pml4[i] & PTE_PRESENT)) continue;
        uint64_t *pdpt = phys_to_ptr(pml4[i] & PHYS_MASK);
        for (int j = 0; j < 512; j++) {
            if (!(pdpt[j] & PTE_PRESENT)) continue;
            uint64_t *pd = phys_to_ptr(pdpt[j] & PHYS_MASK);
            for (int k = 0; k < 512; k++) {
                if (!(pd[k] & PTE_PRESENT)) continue;
                if (pd[k] & PTE_HUGE) {
                    pmm_free_pages(pd[k] & PHYS_MASK, 512);
                    continue;
                }
                uint64_t *pt = phys_to_ptr(pd[k] & PHYS_MASK);
                for (int l = 0; l < 512; l++) {
                    if (pt[l] & PTE_PRESENT)
                        pmm_free_page(pt[l] & PHYS_MASK);
                }
                pmm_free_page(pd[k] & PHYS_MASK);
            }
            pmm_free_page(pdpt[j] & PHYS_MASK);
        }
        pmm_free_page(pml4[i] & PHYS_MASK);
    }
    pmm_free_page((uint64_t)(uintptr_t)pml4);
}

/* ── Kernel VMM init ─────────────────────────────────────────── */
void vmm_init(uint64_t kernel_phys, uint64_t kernel_virt, uint64_t kernel_size) {
    uint64_t pml4_phys = alloc_table();
    g_kernel_pml4 = phys_to_ptr(pml4_phys);

    /* Identity-map first 4 GiB (covers framebuffer, MMIO, etc.) */
    for (uint64_t addr = 0; addr < 0x100000000ULL; addr += 0x200000) {
        /* Use 2 MiB huge pages for speed */
        uint64_t *pdpt = ensure_table(g_kernel_pml4, PML4_IDX(addr),
                                       PTE_RW | PTE_PRESENT);
        uint64_t *pd   = ensure_table(pdpt, PDPT_IDX(addr),
                                       PTE_RW | PTE_PRESENT);
        pd[PD_IDX(addr)] = addr | PTE_HUGE | PTE_RW | PTE_PRESENT | PTE_USER;
    }

    /* Map kernel to higher half */
    uint64_t pages = (kernel_size + PAGE_SIZE - 1) / PAGE_SIZE;
    vmm_map_range(g_kernel_pml4, kernel_virt, kernel_phys,
                  pages, VMM_FLAG_RW | VMM_FLAG_USER);

    /* Load the new page tables */
    vmm_switch(g_kernel_pml4);
}

uint64_t *vmm_kernel_pml4(void) { return g_kernel_pml4; }
