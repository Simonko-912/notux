#pragma once
#include <stdint.h>
void ntfs_register(void);
/* Total/free MiB of the NTFS volume mounted most recently (-1 if none). */
int ntfs_usage(uint64_t *total_mb, uint64_t *free_mb);
