#pragma once
#include "efi.h"
#include "boot_info.h"
EFI_STATUS elf_load(EFI_HANDLE image, const CHAR16 *path,
                    uint64_t *entry_out, BootInfo *bi);
