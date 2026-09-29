/*
 * Notux OS — ELF64 Kernel Loader
 * boot/elf_loader.c
 *
 * Reads notux.elf from the EFI System Partition, validates the
 * ELF header, maps each PT_LOAD segment into physical memory via
 * AllocatePages, and returns the entry point virtual address.
 */

#include "efi.h"
#include "elf_loader.h"
#include "boot_info.h"
#include <stddef.h>
#include <stdint.h>

/* ELF64 types */
#define ELF_MAGIC       0x464C457Fu  /* 0x7F 'E' 'L' 'F' */
#define ET_EXEC         2
#define ET_DYN          3
#define EM_X86_64       62
#define PT_LOAD         1
#define PT_NULL         0

typedef struct {
    uint32_t e_ident_magic;
    uint8_t  e_ident_class;   /* 2 = 64-bit */
    uint8_t  e_ident_data;    /* 1 = little-endian */
    uint8_t  e_ident_version;
    uint8_t  e_ident_osabi;
    uint8_t  e_ident_pad[8];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

/* ── helpers ─────────────────────────────────────────────────── */
extern EFI_SYSTEM_TABLE  *gST;
extern EFI_BOOT_SERVICES *gBS;

static void memset8(void *dst, uint8_t val, uint64_t n) {
    uint8_t *p = (uint8_t *)dst;
    while (n--) *p++ = val;
}
static void memcpy8(void *dst, const void *src, uint64_t n) {
    const uint8_t *s = (const uint8_t *)src;
    uint8_t       *d = (uint8_t *)dst;
    while (n--) *d++ = *s++;
}

/* ── Open file from same volume as our EFI image ─────────────── */
static EFI_STATUS open_file(EFI_HANDLE image, const CHAR16 *path,
                             EFI_FILE_PROTOCOL **out) {
    EFI_GUID lip_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_GUID sfsp_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

    EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
    EFI_STATUS s = gBS->OpenProtocol(image, &lip_guid, (void **)&li,
                                     image, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(s)) return s;

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    s = gBS->OpenProtocol(li->DeviceHandle, &sfsp_guid, (void **)&fs,
                           image, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(s)) return s;

    EFI_FILE_PROTOCOL *root = NULL;
    s = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(s)) return s;

    s = root->Open(root, out, (CHAR16 *)path,
                   EFI_FILE_MODE_READ, 0);
    root->Close(root);
    return s;
}

/* ── Read entire file into a heap buffer ─────────────────────── */
static EFI_STATUS read_file(EFI_FILE_PROTOCOL *file,
                             void **buf_out, uint64_t *size_out) {
    EFI_GUID fi_guid = EFI_FILE_INFO_ID;
    EFI_FILE_INFO *fi = NULL;
    UINTN fi_size = sizeof(EFI_FILE_INFO) + 256;

    EFI_STATUS s = gBS->AllocatePool(EfiLoaderData, fi_size, (void **)&fi);
    if (EFI_ERROR(s)) return s;

    s = file->GetInfo(file, &fi_guid, &fi_size, fi);
    if (EFI_ERROR(s)) return s;

    uint64_t size = fi->FileSize;
    gBS->FreePool(fi);

    void *buf = NULL;
    s = gBS->AllocatePool(EfiLoaderData, size, &buf);
    if (EFI_ERROR(s)) return s;

    UINTN read_size = (UINTN)size;
    s = file->Read(file, &read_size, buf);
    if (EFI_ERROR(s)) return s;

    *buf_out  = buf;
    *size_out = size;
    return EFI_SUCCESS;
}

/* ── Main ELF loader ─────────────────────────────────────────── */
EFI_STATUS elf_load(EFI_HANDLE image, const CHAR16 *path,
                    uint64_t *entry_out, BootInfo *bi) {
    EFI_FILE_PROTOCOL *file = NULL;
    EFI_STATUS s = open_file(image, path, &file);
    if (EFI_ERROR(s)) return s;

    void     *raw  = NULL;
    uint64_t  size = 0;
    s = read_file(file, &raw, &size);
    file->Close(file);
    if (EFI_ERROR(s)) return s;

    /* Validate ELF header */
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)raw;
    if (ehdr->e_ident_magic != ELF_MAGIC ||
        ehdr->e_ident_class != 2        ||
        ehdr->e_machine     != EM_X86_64) {
        gBS->FreePool(raw);
        return EFI_LOAD_ERROR;
    }

    /* Walk program headers, map PT_LOAD segments */
    uint64_t load_min = UINT64_MAX, load_max = 0;

    Elf64_Phdr *phdr = (Elf64_Phdr *)((uint8_t *)raw + ehdr->e_phoff);
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;

        uint64_t vaddr  = phdr[i].p_vaddr;
        uint64_t memsz  = phdr[i].p_memsz;
        uint64_t filesz = phdr[i].p_filesz;
        uint64_t offset = phdr[i].p_offset;

        /* Align to page (4 KiB) */
        uint64_t page_base  = vaddr & ~0xFFFull;
        uint64_t page_end   = (vaddr + memsz + 0xFFF) & ~0xFFFull;
        UINTN    page_count = (UINTN)((page_end - page_base) / 4096);

        /* Allocate physical pages at the virtual address
           (kernel is identity-mapped during early boot)  */
        EFI_PHYSICAL_ADDRESS paddr = (EFI_PHYSICAL_ADDRESS)page_base;
        s = gBS->AllocatePages(AllocateAddress, EfiLoaderData,
                                page_count, &paddr);
        if (EFI_ERROR(s)) return s;

        /* Zero the region, then copy file data */
        memset8((void *)paddr, 0, page_count * 4096);
        memcpy8((void *)(paddr + (vaddr - page_base)),
                (uint8_t *)raw + offset, filesz);

        if (page_base < load_min) load_min = page_base;
        if (page_end  > load_max) load_max = page_end;
    }

    bi->kernel_phys = load_min;
    bi->kernel_virt = load_min;   /* early identity mapping */
    bi->kernel_size = load_max - load_min;

    *entry_out = ehdr->e_entry;

    gBS->FreePool(raw);
    return EFI_SUCCESS;
}
