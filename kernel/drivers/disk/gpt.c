/*
 * Notux OS — GPT parser
 * kernel/drivers/disk/gpt.c
 *
 * Reads the GPT header from LBA 1, then reads partition entries.
 * Builds a flat list of DiskPartition structs for all drives.
 */
#include "gpt.h"
#include "ata.h"
#include "../../kernel.h"
#include <stdint.h>
#include <stddef.h>

#define GPT_HEADER_LBA 1

/* GPT header (on-disk, 92 bytes) */
typedef struct __attribute__((packed)) {
    uint64_t signature;          /* "EFI PART" */
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t my_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t  disk_guid[16];
    uint64_t partition_entry_lba;
    uint32_t num_partition_entries;
    uint32_t partition_entry_size;  /* usually 128 */
    uint32_t partition_entry_crc32;
} GptHeader;

DiskPartition disk_partitions[GPT_MAX_PARTITIONS * ATA_MAX_DRIVES];
int           disk_partition_count = 0;

static const uint8_t ntfs_type_guid[16] = {
    /* Windows Basic Data: EBD0A0A2-B9E5-4433-87C0-68B6B72699C7 */
    0xA2,0xA0,0xD0,0xEB, 0xE5,0xB9, 0x33,0x44,
    0x87,0xC0,0x68,0xB6,0xB7,0x26,0x99,0xC7
};
static const uint8_t linux_data_guid[16] = {
    /* Linux filesystem data: 0FC63DAF-8483-4772-8E79-3D69D8477DE4 */
    0xAF,0x3D,0xC6,0x0F, 0x83,0x84, 0x72,0x47,
    0x8E,0x79,0x3D,0x69,0xD8,0x47,0x7D,0xE4
};
static const uint8_t ms_reserved_guid[16] = {
    /* Microsoft Reserved: E3C9E316-0B5C-4DB8-817D-F92DF00215AE */
    0x16,0xE3,0xC9,0xE3, 0x5C,0x0B, 0xB8,0x4D,
    0x81,0x7D,0xF9,0x2D,0xF0,0x02,0x15,0xAE
};
static const uint8_t esp_type_guid[16] = {
    /* EFI System: C12A7328-F81F-11D2-BA4B-00A0C93EC93B */
    0x28,0x73,0x2A,0xC1, 0x1F,0xF8, 0xD2,0x11,
    0xBA,0x4B,0x00,0xA0,0xC9,0x3E,0xC9,0x3B
};

static int guid_eq(const uint8_t *a, const uint8_t *b) {
    for (int i = 0; i < 16; i++) if (a[i] != b[i]) return 0;
    return 1;
}

static int guid_zero(const uint8_t *g) {
    for (int i = 0; i < 16; i++) if (g[i]) return 0;
    return 1;
}

/* Narrow UTF-16LE → ASCII (drop anything > 0x7E) */
static void utf16_narrow(const uint16_t *src, char *dst, int maxlen) {
    int i;
    for (i = 0; i < maxlen - 1 && src[i]; i++)
        dst[i] = (src[i] < 0x80) ? (char)src[i] : '?';
    dst[i] = '\0';
}

static void scan_drive(int drive_idx) {
    uint8_t sector[512];

    /* Read GPT header (LBA 1) */
    if (ata_read_sectors(drive_idx, GPT_HEADER_LBA, 1, sector) < 0)
        return;

    GptHeader *h = (GptHeader *)sector;
    if (h->signature != GPT_SIGNATURE) return; /* Not GPT */

    uint32_t entry_size = h->partition_entry_size;
    uint32_t num_entries = h->num_partition_entries;
    uint64_t entry_lba   = h->partition_entry_lba;

    if (entry_size < 128 || num_entries > 128) return;

    /* Read partition entries (4 per 512-byte sector at 128 bytes each) */
    uint32_t entries_per_sector = 512 / entry_size;
    uint32_t sectors_needed = (num_entries + entries_per_sector - 1)
                              / entries_per_sector;

    for (uint32_t s = 0; s < sectors_needed; s++) {
        if (ata_read_sectors(drive_idx, entry_lba + s, 1, sector) < 0)
            break;

        for (uint32_t e = 0;
             e < entries_per_sector && (s * entries_per_sector + e) < num_entries;
             e++) {
            GptEntry *ent = (GptEntry *)(sector + e * entry_size);

            if (guid_zero(ent->type_guid)) continue;
            if (!ent->start_lba || !ent->end_lba)   continue;

            DiskPartition *p = &disk_partitions[disk_partition_count];
            p->drive_idx     = drive_idx;
            p->part_idx      = (int)(s * entries_per_sector + e);
            p->start_lba     = ent->start_lba;
            p->end_lba       = ent->end_lba;
            p->size_sectors  = ent->end_lba - ent->start_lba + 1;
            /* Accept Windows Basic Data, Linux filesystem, or any non-ESP data partition
               (actual NTFS detection is done by reading the boot sector signature) */
            p->is_ntfs       = guid_eq(ent->type_guid, ntfs_type_guid)
                             || guid_eq(ent->type_guid, linux_data_guid)
                             || (!guid_eq(ent->type_guid, esp_type_guid)
                                 && !guid_eq(ent->type_guid, ms_reserved_guid));
            p->is_esp        = guid_eq(ent->type_guid, esp_type_guid);
            /* Copy UTF-16 name to local buffer to avoid packed-pointer warning */
            uint16_t name_copy[36];
            for(int ni=0;ni<36;ni++) name_copy[ni]=ent->name[ni];
            utf16_narrow(name_copy, p->name, 37);

            disk_partition_count++;
            if (disk_partition_count >= GPT_MAX_PARTITIONS * ATA_MAX_DRIVES)
                return;
        }
    }
}

void gpt_scan_all(void) {
    disk_partition_count = 0;
    for (int i = 0; i < ata_drive_count; i++)
        scan_drive(i);
}
