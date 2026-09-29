/*
 * Notux OS — UEFI Bootloader
 * boot/main.c
 *
 * Runs as a PE32+ EFI application.
 * 1. Obtains the UEFI memory map
 * 2. Locates the GOP framebuffer
 * 3. Loads notux.elf from the ESP root
 * 4. Calls ExitBootServices()
 * 5. Sets up early 4-level page tables
 * 6. Jumps to kernel _start with BootInfo
 */



#include "efi.h"
#include "elf_loader.h"
#include "mem.h"
#include "boot_info.h"



/* ── EFI globals ────────────────────────────────────────────── */
EFI_SYSTEM_TABLE    *gST  = NULL;
EFI_BOOT_SERVICES   *gBS  = NULL;
EFI_HANDLE           gImageHandle = NULL;


/* ── GOP framebuffer helper ─────────────────────────────────── */
static EFI_STATUS acquire_framebuffer(FramebufferInfo *fb) {
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

    EFI_STATUS s = gBS->LocateProtocol(&gop_guid, NULL, (void **)&gop);
    if (EFI_ERROR(s)) return s;

    fb->base   = (uint64_t)gop->Mode->FrameBufferBase;
    fb->size   = gop->Mode->FrameBufferSize;
    fb->width  = gop->Mode->Info->HorizontalResolution;
    fb->height = gop->Mode->Info->VerticalResolution;
    fb->pitch  = gop->Mode->Info->PixelsPerScanLine * 4; /* 32bpp */
    fb->format = (uint32_t)gop->Mode->Info->PixelFormat;
    return EFI_SUCCESS;
}

/* ── Memory map helper ──────────────────────────────────────── */
static EFI_STATUS get_memory_map(MemoryMapInfo *out) {
    UINTN map_size = 0, map_key = 0, desc_size = 0;
    UINT32 desc_ver = 0;

    /* First call: get required buffer size */
    gBS->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_ver);
    map_size += 2 * desc_size; /* a little extra for AllocatePool itself */

    EFI_MEMORY_DESCRIPTOR *map = NULL;
    EFI_STATUS s = gBS->AllocatePool(EfiLoaderData, map_size, (void **)&map);
    if (EFI_ERROR(s)) return s;

    s = gBS->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_ver);
    if (EFI_ERROR(s)) return s;

    out->map      = map;
    out->map_size = map_size;
    out->map_key  = map_key;
    out->desc_size= desc_size;
    out->desc_ver = desc_ver;
    return EFI_SUCCESS;
}

/* ── Simple print helper (uses EFI ConOut) ──────────────────── */
static void efi_print(const CHAR16 *s) {
    gST->ConOut->OutputString(gST->ConOut, (CHAR16 *)s);
}

/* ── EFI entry point ────────────────────────────────────────── */
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    gST = st;
    gBS = st->BootServices;
    gImageHandle = image;

    gST->ConOut->ClearScreen(gST->ConOut);
    efi_print(u"Notux Bootloader v0.1\r\n");
    efi_print(u"Loading kernel...\r\n");

    /* ── Locate kernel on ESP ───────────────────────────────── */
    static BootInfo boot_info = {0};

    EFI_STATUS s = acquire_framebuffer(&boot_info.fb);
    if (EFI_ERROR(s)) {
        efi_print(u"ERROR: Could not acquire GOP framebuffer\r\n");
        return s;
    }

    /* Load ELF: reads notux.elf from the same volume as this EFI app */
    uint64_t kernel_entry = 0;
    s = elf_load(image, L"\\notux.elf", &kernel_entry, &boot_info);
    if (EFI_ERROR(s)) {
        efi_print(u"ERROR: Failed to load kernel ELF\r\n");
        return s;
    }

    efi_print(u"Kernel loaded. Acquiring memory map...\r\n");

    MemoryMapInfo mmap = {0};
    s = get_memory_map(&mmap);
    if (EFI_ERROR(s)) {
        efi_print(u"ERROR: GetMemoryMap failed\r\n");
        return s;
    }

    efi_print(u"About to exit boot services...\r\n");

    /* ── Exit boot services ─────────────────────────────────── */
    s = gBS->ExitBootServices(image, mmap.map_key);
    if (EFI_ERROR(s)) {
        /* Map may have changed: re-fetch and retry once */
        get_memory_map(&mmap);
        s = gBS->ExitBootServices(image, mmap.map_key);
        if (EFI_ERROR(s)) return s; /* fatal */
    }

    /* Fill remaining BootInfo fields */
    boot_info.mmap       = mmap;
    boot_info.magic      = NOTUX_BOOT_MAGIC;  /* 0x4E4F5458 = 'NOTX' */

    /* ── Jump to kernel ─────────────────────────────────────── */
    typedef void (*KernelEntry)(BootInfo *) __attribute__((sysv_abi));

    KernelEntry entry = (KernelEntry)kernel_entry;
    
    __asm__ volatile (
        "cli\n"
        "mov %0, %%rsp\n"
        "andq $-16, %%rsp\n"
        "mov %1, %%rdi\n"
        "jmp *%2\n"
        :
        : "r"(stack_top),
          "r"(&boot_info),
          "r"(entry)
        : "memory"
    );  

    /* Should never return */
    for (;;) __asm__ volatile("hlt");
    return EFI_SUCCESS;
}
