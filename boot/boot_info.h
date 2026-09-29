/*
 * Notux OS — Boot Information Structure
 * boot/boot_info.h  (included by both bootloader and kernel)
 */
#pragma once
#include <stdint.h>

#define NOTUX_BOOT_MAGIC  0x4E4F5458u   /* 'NOTX' */

typedef struct {
    uint64_t base;
    uint64_t size;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t format;
} FramebufferInfo;

typedef struct {
    void    *map;       /* EFI_MEMORY_DESCRIPTOR array */
    uint64_t map_size;
    uint64_t map_key;
    uint64_t desc_size;
    uint32_t desc_ver;
    uint32_t _pad;
} MemoryMapInfo;

typedef struct {
    uint32_t        magic;
    uint32_t        _pad0;
    FramebufferInfo fb;
    MemoryMapInfo   mmap;
    uint64_t        rsdp_phys;
    uint64_t        kernel_phys;
    uint64_t        kernel_virt;
    uint64_t        kernel_size;
} BootInfo;
