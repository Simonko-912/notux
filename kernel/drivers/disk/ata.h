/*
 * Notux OS — ATA PIO driver header
 * kernel/drivers/disk/ata.h
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* Maximum ATA devices we scan: 2 controllers × 2 drives */
#define ATA_MAX_DRIVES 4

typedef struct {
    int      present;       /* 1 if drive responded to IDENTIFY */
    int      controller;    /* 0 = primary (0x1F0), 1 = secondary (0x170) */
    int      slave;         /* 0 = master, 1 = slave */
    uint64_t sector_count;  /* total LBA48 sectors */
    char     model[41];     /* model string from IDENTIFY */
} AtaDrive;

extern AtaDrive ata_drives[ATA_MAX_DRIVES];
extern int      ata_drive_count;

void    ata_init(void);
int     ata_read_sectors(int drive_idx, uint64_t lba,
                         uint32_t count, void *buf);
int     ata_write_sectors(int drive_idx, uint64_t lba,
                          uint32_t count, const void *buf);
