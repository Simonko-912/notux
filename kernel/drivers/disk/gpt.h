/*
 * Notux OS — GPT partition table
 * kernel/drivers/disk/gpt.h
 */
#pragma once
#include <stdint.h>

#define GPT_MAX_PARTITIONS 128
#define GPT_SIGNATURE      0x5452415020494645ULL  /* "EFI PART" */

/* Well-known partition type GUIDs (first 4 bytes for quick check) */
#define GPT_TYPE_NTFS_FIRST4  0xEBD0A0A2  /* Windows Basic Data (NTFS) */
#define GPT_TYPE_ESP_FIRST4   0x28732AC1  /* EFI System Partition       */

typedef struct __attribute__((packed)) {
    uint8_t  type_guid[16];   /* Partition type GUID */
    uint8_t  part_guid[16];   /* Unique partition GUID */
    uint64_t start_lba;
    uint64_t end_lba;
    uint64_t attributes;
    uint16_t name[36];        /* UTF-16LE partition name */
} GptEntry;

typedef struct {
    int      drive_idx;       /* ATA drive index */
    int      part_idx;        /* partition entry index in GPT */
    uint64_t start_lba;
    uint64_t end_lba;
    uint64_t size_sectors;
    char     name[37];        /* ASCII-narrowed partition name */
    int      is_ntfs;         /* 1 if type GUID matches NTFS/basic data */
    int      is_esp;          /* 1 if EFI System Partition */
} DiskPartition;

extern DiskPartition disk_partitions[];
extern int           disk_partition_count;

/* Scan all ATA drives for GPT partitions */
void gpt_scan_all(void);
