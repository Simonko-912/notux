/*
 * Notux OS — NTFS Filesystem Driver
 * kernel/fs/ntfs/ntfs.c
 *
 * Implements the VfsDriver interface for NTFS volumes.
 * Uses the Master File Table (MFT) to look up files and directories.
 *
 * Supported:
 *   - Read files and directories
 *   - Create/write files (small files in MFT resident or single run)
 *   - mkdir, unlink, rename
 *   - Long filenames (NTFS $FILE_NAME attribute, Unicode→ASCII narrow)
 *
 * Not yet supported:
 *   - Compression / sparse files
 *   - Alternate data streams
 *   - Journaling (writes are direct, no $LogFile)
 *   - Hardlinks beyond the primary name
 *
 * NTFS on-disk structures used:
 *   Boot sector → BPB (bytes per sector, sectors per cluster, MFT LCN)
 *   $MFT → array of FILE records (1 KB each by default)
 *   FILE record → attributes:
 *     0x10 $STANDARD_INFORMATION
 *     0x20 $ATTRIBUTE_LIST
 *     0x30 $FILE_NAME
 *     0x50 $SECURITY_DESCRIPTOR
 *     0x80 $DATA  (file content)
 *     0x90 $INDEX_ROOT   (directory entries, B-tree root)
 *     0xA0 $INDEX_ALLOCATION (directory B-tree overflow)
 */

#include "ntfs.h"
#include "../vfs.h"
#include "../../mm/heap.h"
#include "../../kernel.h"
#include "../../kserial.h"
#include <stdint.h>
#include <stddef.h>

static void dbg(const char *tag, uint64_t n) {
    kser_puts(tag);
    kser_dec(n);
    kser_puts("\n");
}

/* ── On-disk types (packed) ──────────────────────────────────── */
#pragma pack(push, 1)

typedef struct {
    uint8_t  jump[3];
    char     oem_id[8];       /* "NTFS    " */
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint8_t  _reserved[7];
    uint8_t  media_type;
    uint8_t  _reserved2[2];
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t _reserved3;
    uint32_t _reserved4;
    uint64_t total_sectors;
    uint64_t mft_lcn;          /* LCN of $MFT */
    uint64_t mft_mirror_lcn;
    int8_t   clusters_per_file_record; /* if negative: 2^|value| bytes */
    uint8_t  _pad[3];
    int8_t   clusters_per_index_block;
    uint8_t  _pad2[3];
    uint64_t volume_serial;
    uint32_t checksum;
    uint8_t  bootstrap[426];
    uint16_t end_marker;       /* 0x55AA */
} NtfsBoot;

typedef struct {
    char     magic[4];         /* "FILE" */
    uint16_t update_seq_offset;
    uint16_t update_seq_size;
    uint64_t log_seq;
    uint16_t sequence_number;
    uint16_t link_count;
    uint16_t attr_offset;      /* offset to first attribute */
    uint16_t flags;            /* 0=deleted 1=in-use 2=directory */
    uint32_t used_size;
    uint32_t alloc_size;
    uint64_t base_record;
    uint16_t next_attr_id;
    uint16_t _pad;
    uint32_t mft_record_number;
} NtfsFileRecord;

typedef struct {
    uint32_t type;             /* attribute type code */
    uint32_t length;           /* total length of this attribute */
    uint8_t  non_resident;
    uint8_t  name_length;
    uint16_t name_offset;
    uint16_t flags;
    uint16_t attr_id;
    union {
        struct { /* resident */
            uint32_t value_length;
            uint16_t value_offset;
            uint16_t _flags;
        } resident;
        struct { /* non-resident */
            uint64_t start_vcn;
            uint64_t last_vcn;
            uint16_t runlist_offset;
            uint16_t compression_unit;
            uint32_t _pad;
            uint64_t alloc_size;
            uint64_t data_size;
            uint64_t init_size;
        } nonresident;
    };
} NtfsAttr;

typedef struct {
    uint64_t parent_dir;       /* MFT reference of parent directory */
    uint64_t create_time;
    uint64_t modify_time;
    uint64_t mft_modify_time;
    uint64_t access_time;
    uint64_t alloc_size;
    uint64_t real_size;
    uint32_t file_attrs;       /* 0x10 = directory */
    uint32_t reparse_tag;
    uint8_t  name_length;      /* in UTF-16 characters */
    uint8_t  namespace;
    uint16_t name[1];          /* UTF-16LE filename */
} NtfsFileName;

typedef struct {
    uint32_t type;
    uint32_t collation;
    uint32_t bytes_per_index;
    uint8_t  clusters_per_index;
    uint8_t  _pad[3];
    /* followed by INDEX_HEADER then INDEX_ENTRYs */
} NtfsIndexRoot;

typedef struct {
    uint64_t mft_reference;
    uint16_t entry_length;
    uint16_t key_length;
    uint32_t flags;   /* 1=has child nodes, 2=last entry */
    /* followed by $FILE_NAME attribute data when key_length > 0 */
} NtfsIndexEntry;

#pragma pack(pop)

/* ── NTFS attribute type codes ───────────────────────────────── */
#define NTFS_AT_STANDARD_INFO  0x10
#define NTFS_AT_FILE_NAME      0x30
#define NTFS_AT_DATA           0x80
#define NTFS_AT_INDEX_ROOT     0x90
#define NTFS_AT_INDEX_ALLOC    0xA0
#define NTFS_AT_SECURITY       0x50
#define NTFS_AT_END            0xFFFFFFFF

#define NTFS_FILE_IN_USE       0x0001
#define NTFS_FILE_DIRECTORY    0x0002

/* ── Driver state ────────────────────────────────────────────── */
typedef struct {
    int      drive_idx;         /* ATA drive index */
    uint64_t part_start_lba;    /* partition start LBA (added to all reads) */
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_cluster;
    uint32_t bytes_per_record;
    uint64_t mft_offset;        /* byte offset of $MFT within partition */
    uint64_t total_clusters;    /* clusters in the volume (bitmap bound) */
    uint64_t mft_zone_end;      /* never allocate LCN < this (protects $MFT) */
    uint8_t *mft_cache;         /* cached MFT records (first 128) */
    uint32_t mft_cache_count;
} NtfsState;

/* ── Disk read helper ────────────────────────────────────────── */
static int disk_read(NtfsState *s, uint64_t byte_offset,
                     void *buf, size_t size) {
    extern int ata_read_sectors(int drive, uint64_t lba,
                                uint32_t count, void *buf);
    if (!s->bytes_per_sector) s->bytes_per_sector = 512;
    uint64_t off     = byte_offset % s->bytes_per_sector;
    uint64_t lba_rel = byte_offset / s->bytes_per_sector;
    /* Round UP and add a sector for the unaligned tail copy. */
    uint32_t sectors = (uint32_t)((off + size + s->bytes_per_sector - 1)
                                  / s->bytes_per_sector);
    /* Reject nonsense LCNs (e.g. a sign-extended runlist delta) before they
     * reach the bus; an out-of-range LBA otherwise looks like a device hang. */
    if (s->total_clusters) {
        uint64_t vbytes = s->total_clusters * (uint64_t)s->bytes_per_cluster;
        if (byte_offset > vbytes || vbytes - byte_offset < size) return -EIO;
    }
    uint64_t lba_abs = s->part_start_lba + lba_rel;
    uint8_t *tmp = (uint8_t *)kmalloc((size_t)sectors * s->bytes_per_sector);
    if (!tmp) return -ENOMEM;
    int r = ata_read_sectors(s->drive_idx, lba_abs, sectors, tmp);
    if (r == 0) kmemcpy(buf, tmp + off, size);
    kfree(tmp);
    return r < 0 ? -EIO : 0;
}

/* ── Disk write helper ───────────────────────────────────────── */
static int disk_write(NtfsState *s, uint64_t byte_offset,
                      const void *buf, size_t size) {
    extern int ata_write_sectors(int drive, uint64_t lba,
                                 uint32_t count, const void *buf);
    if (!s->bytes_per_sector) s->bytes_per_sector = 512;
    uint64_t off     = byte_offset % s->bytes_per_sector;
    uint64_t lba_rel = byte_offset / s->bytes_per_sector;
    uint32_t sectors = (uint32_t)((off + size + s->bytes_per_sector - 1)
                                  / s->bytes_per_sector);
    if (s->total_clusters) {
        uint64_t vbytes = s->total_clusters * (uint64_t)s->bytes_per_cluster;
        if (byte_offset > vbytes || vbytes - byte_offset < size) return -EIO;
    }
    uint64_t lba_abs = s->part_start_lba + lba_rel;
    uint8_t *tmp = (uint8_t *)kmalloc((size_t)sectors * s->bytes_per_sector);
    if (!tmp) return -ENOMEM;
    /* Preserve the leading/trailing partial sector: never zero-fill the
     * whole block, that would destroy the neighbouring data. */
    kmemcpy(tmp + off, buf, size);
    int r = ata_write_sectors(s->drive_idx, lba_abs, sectors, tmp);
    kfree(tmp);
    return r < 0 ? -EIO : 0;
}

/* ── Write one MFT record ────────────────────────────────────── */
static int write_mft_record(NtfsState *s, uint64_t record_num,
                            const NtfsFileRecord *rec) {
    if (record_num < s->mft_cache_count && s->mft_cache)
        kmemcpy(s->mft_cache + record_num * s->bytes_per_record,
                rec, s->bytes_per_record);
    uint64_t offset = s->mft_offset + record_num * s->bytes_per_record;
    return disk_write(s, offset, rec, s->bytes_per_record);
}

/* ── Read one MFT record ─────────────────────────────────────── */
static int read_mft_record(NtfsState *s, uint64_t record_num,
                            NtfsFileRecord *out) {
    /* Check cache first */
    if (s->mft_cache && record_num < s->mft_cache_count) {
        kmemcpy(out, s->mft_cache + record_num * s->bytes_per_record,
                s->bytes_per_record);
        return 0;
    }
    uint64_t offset = s->mft_offset + record_num * s->bytes_per_record;
    return disk_read(s, offset, out, s->bytes_per_record);
}

/* ── Find attribute in a FILE record ─────────────────────────── */
static NtfsAttr *find_attr(NtfsFileRecord *rec, uint32_t type) {
    uint8_t  *base = (uint8_t *)rec;
    uint8_t  *ptr  = base + rec->attr_offset;
    uint8_t  *end  = base + rec->used_size;

    while (ptr < end) {
        NtfsAttr *attr = (NtfsAttr *)ptr;
        if (attr->type == NTFS_AT_END || attr->length == 0) break;
        if (attr->type == type) return attr;
        ptr += attr->length;
    }
    return NULL;
}

/* ── Get resident attribute data ─────────────────────────────── */
static void *attr_resident_data(NtfsAttr *attr, uint32_t *size_out) {
    if (attr->non_resident) return NULL;
    if (size_out) *size_out = attr->resident.value_length;
    return (uint8_t *)attr + attr->resident.value_offset;
}

/* Forward declaration */
static int64_t read_nonresident(NtfsState *s, NtfsAttr *attr,
                                 void *buf, size_t size, int64_t pos);

static int write_nonresident(NtfsState *s, NtfsAttr *attr,
                             const void *buf, size_t size, int64_t pos) {
    if (!attr->non_resident) return -EINVAL;

    uint8_t *rl          = (uint8_t *)attr + attr->nonresident.runlist_offset;
    int64_t  lcn         = 0;
    uint64_t to_skip     = (uint64_t)pos;
    uint64_t to_write    = size;
    uint8_t *in          = (uint8_t *)buf;
    int64_t  total_written = 0;

    while (*rl && to_write) {
        uint8_t header       = *rl++;
        uint8_t len_bytes    = header & 0x0F;
        uint8_t offset_bytes = (header >> 4) & 0x0F;

        uint64_t run_len = 0;
        for (int i = 0; i < len_bytes; i++)
            run_len |= (uint64_t)(*rl++) << (i * 8);

        int64_t run_offset = 0;
        if (offset_bytes) {
            uint64_t raw = 0;
            for (int i = 0; i < offset_bytes; i++)
                raw |= (uint64_t)(*rl++) << (i * 8);
            uint64_t sign_bit = 1ULL << (offset_bytes * 8 - 1);
            if (raw & sign_bit)
                raw |= ~((sign_bit << 1) - 1);
            run_offset = (int64_t)raw;
        }
        lcn += run_offset;

        uint64_t run_bytes = run_len * s->bytes_per_cluster;
        if (to_skip >= run_bytes) { to_skip -= run_bytes; continue; }

        uint64_t start_in_run = to_skip;
        uint64_t avail        = run_bytes - start_in_run;
        uint64_t chunk        = avail < to_write ? avail : to_write;
        uint64_t disk_offset  = (uint64_t)lcn * s->bytes_per_cluster + start_in_run;

        if (disk_write(s, disk_offset, in, (size_t)chunk) < 0)
            return total_written > 0 ? 0 : -EIO;

        in            += chunk;
        total_written += (int64_t)chunk;
        to_write      -= chunk;
        to_skip        = 0;
    }

    return total_written == (int64_t)size ? 0 : -ENOSPC;
}

/* ── $Bitmap handling ─────────────────────────────────────────── */
/* $Bitmap is MFT record 6, unnamed $DATA attribute containing cluster bitmap */
static int read_bitmap(NtfsState *s, uint8_t **bitmap_out, uint64_t *bits_out) {
    uint8_t rec_buf[4096];
    NtfsFileRecord *rec = (NtfsFileRecord *)rec_buf;
    if (read_mft_record(s, 6, rec) < 0) return -EIO;

    NtfsAttr *data_attr = find_attr(rec, NTFS_AT_DATA);
    if (!data_attr) return -EIO;

    if (data_attr->non_resident) {
        /* Non-resident bitmap - need to read via runlist */
        uint64_t bmp_size = data_attr->nonresident.data_size;
        if (bmp_size > 1024 * 1024) return -ENOSPC;
        uint8_t *buf = (uint8_t *)kmalloc((size_t)bmp_size);
        if (!buf) return -ENOMEM;
        int64_t r = read_nonresident(s, data_attr, buf, (size_t)bmp_size, 0);
        if (r < 0) { kfree(buf); return (int)r; }
        *bitmap_out = buf;
        *bits_out = bmp_size * 8;
        return 0;
    } else {
        /* Resident bitmap */
        uint32_t dsize = 0;
        void *data = attr_resident_data(data_attr, &dsize);
        if (!data) return -EIO;
        uint8_t *buf = (uint8_t *)kmalloc(dsize);
        if (!buf) return -ENOMEM;
        kmemcpy(buf, data, dsize);
        *bitmap_out = buf;
        *bits_out = dsize * 8;
        return 0;
    }
}

static int write_bitmap(NtfsState *s, const uint8_t *bitmap, uint64_t bits) {
    uint8_t rec_buf[4096];
    NtfsFileRecord *rec = (NtfsFileRecord *)rec_buf;
    if (read_mft_record(s, 6, rec) < 0) return -EIO;

    NtfsAttr *data_attr = find_attr(rec, NTFS_AT_DATA);
    if (!data_attr) return -EIO;

    uint64_t bmp_bytes = (bits + 7) / 8;
    if (data_attr->non_resident) {
        return write_nonresident(s, data_attr, bitmap, (size_t)bmp_bytes, 0);
    } else {
        uint32_t dsize = 0;
        void *data = attr_resident_data(data_attr, &dsize);
        if (!data || bmp_bytes > dsize) return -ENOSPC;
        kmemcpy(data, bitmap, bmp_bytes);
        data_attr->resident.value_length = (uint32_t)bmp_bytes;
        return write_mft_record(s, 6, rec);
    }
}

static int find_free_clusters(NtfsState *s, uint64_t count, uint64_t *start_lcn_out) {
    uint8_t *bitmap = NULL;
    uint64_t bits = 0;
    int r = read_bitmap(s, &bitmap, &bits);
    if (r < 0) return r;

    /* Never hand out the MFT zone, and never run past the volume end. */
    uint64_t limit = s->total_clusters ? s->total_clusters : bits;
    if (limit > bits) limit = bits;
    uint64_t zone = s->mft_zone_end;

    uint64_t free_run = 0;
    uint64_t run_start = 0;
    for (uint64_t i = zone; i < limit; i++) {
        uint8_t byte = bitmap[i / 8];
        uint8_t bit = byte & (1 << (i % 8));
        if (!bit) {
            if (free_run == 0) run_start = i;
            free_run++;
            if (free_run >= count) {
                *start_lcn_out = run_start;
                kfree(bitmap);
                return 0;
            }
        } else {
            free_run = 0;
        }
    }
    kfree(bitmap);
    return -ENOSPC;
}

static int alloc_clusters(NtfsState *s, uint64_t start_lcn, uint64_t count) {
    uint8_t *bitmap = NULL;
    uint64_t bits = 0;
    int r = read_bitmap(s, &bitmap, &bits);
    if (r < 0) return r;

    if (start_lcn < s->mft_zone_end) { kfree(bitmap); return -EIO; }
    if (start_lcn + count > bits) { kfree(bitmap); return -ENOSPC; }

    for (uint64_t i = 0; i < count; i++) {
        uint64_t idx = start_lcn + i;
        uint8_t *byte = &bitmap[idx / 8];
        uint8_t mask = 1 << (idx % 8);
        if (*byte & mask) { kfree(bitmap); return -EIO; }
        *byte |= mask;
    }

    r = write_bitmap(s, bitmap, bits);
    kfree(bitmap);
    return r;
}

/* ── Runlist encoding ─────────────────────────────────────────── */
/* Encode a runlist: header (1 byte) + length (variable) + offset (variable) */
static uint32_t encode_runlist(uint8_t *out, uint32_t max_out,
                               uint64_t length, int64_t lcn, int64_t prev_lcn) {
    int64_t run_delta = lcn - prev_lcn;

    /* Size length field (unsigned) */
    uint8_t len_bytes = 0;
    uint64_t tlen = length;
    do { len_bytes++; tlen >>= 8; } while (tlen);
    if (len_bytes > 8) len_bytes = 8;

    /* Size offset field to hold a SIGNED run_delta (two's complement,
     * sign bit included). The decoder sign-extends this field, so the
     * encoder must produce the same little-endian two's complement. */
    uint8_t off_bytes = 0;
    if (run_delta != 0) {
        /* Smallest width whose SIGNED range holds run_delta. The decoder
         * sign-extends from bit (off_bytes*8 - 1), so a positive delta of
         * 0x80..0xFF must be widened to 2 bytes, never truncated to 1. */
        do {
            off_bytes++;
        } while (run_delta < -(int64_t)(1LL << (off_bytes * 8 - 1)) ||
                 run_delta >= (int64_t)(1LL << (off_bytes * 8 - 1)));
        if (off_bytes > 8) off_bytes = 8;
    } else {
        off_bytes = 0;
    }

    uint8_t header = (off_bytes << 4) | len_bytes;
    if (max_out < 1 + len_bytes + off_bytes) return 0;

    uint32_t pos = 0;
    out[pos++] = header;
    for (uint8_t i = 0; i < len_bytes; i++) {
        out[pos++] = (uint8_t)(length & 0xFF);
        length >>= 8;
    }
    uint64_t dv = (uint64_t)run_delta;
    for (uint8_t i = 0; i < off_bytes; i++) {
        out[pos++] = (uint8_t)(dv & 0xFF);
        dv >>= 8;
    }
    return pos;
}

/* Build runlist from contiguous LCN+len (single run for simplicity) */
static uint32_t build_runlist(uint8_t *out, uint32_t max_out,
                              uint64_t lcn, uint64_t length) {
    return encode_runlist(out, max_out, length, (int64_t)lcn, 0);
}

/* ── UTF-16LE → ASCII (narrow) ───────────────────────────────── */
static void utf16_to_ascii(const uint16_t *src, int len, char *dst) {
    for (int i = 0; i < len && i < 255; i++)
        dst[i] = (src[i] < 0x80) ? (char)src[i] : '?';
    dst[len < 255 ? len : 255] = '\0';
}

/* ── Walk runlist, read non-resident data ────────────────────── */
static int64_t read_nonresident(NtfsState *s, NtfsAttr *attr,
                                 void *buf, size_t size, int64_t pos) {
    if (!attr->non_resident) return -EINVAL;

    /* Parse run list: variable-length encoding */
    uint8_t *rl   = (uint8_t *)attr + attr->nonresident.runlist_offset;
    uint64_t vcn  = 0;
    int64_t  lcn  = 0;   /* signed for delta encoding */
    uint64_t to_skip = (uint64_t)pos;
    uint64_t to_read = size;
    uint8_t *out  = (uint8_t *)buf;
    int64_t  total_read = 0;

    while (*rl) {
        uint8_t header = *rl++;
        uint8_t len_bytes = header & 0x0F;
        uint8_t offset_bytes = (header >> 4) & 0x0F;

        uint64_t run_len = 0;
        for (int i = 0; i < len_bytes; i++)
            run_len |= (uint64_t)(*rl++) << (i * 8);

        int64_t run_offset = 0;
        if (offset_bytes) {
            uint64_t raw = 0;
            for (int i = 0; i < offset_bytes; i++)
                raw |= (uint64_t)(*rl++) << (i * 8);
            /* Sign-extend */
            uint64_t sign_bit = 1ULL << (offset_bytes * 8 - 1);
            if (raw & sign_bit)
                raw |= ~((sign_bit << 1) - 1);
            run_offset = (int64_t)raw;
        }
        lcn += run_offset;

        uint64_t run_bytes = run_len * s->bytes_per_cluster;
        (void)vcn;

        if (to_skip >= run_bytes) {
            to_skip -= run_bytes;
            vcn     += run_len;
            continue;
        }

        uint64_t start_in_run = to_skip;
        uint64_t avail        = run_bytes - start_in_run;
        uint64_t chunk        = avail < to_read ? avail : to_read;
        uint64_t disk_offset  = (uint64_t)lcn * s->bytes_per_cluster
                                + start_in_run;

        if (disk_read(s, disk_offset, out, (size_t)chunk) < 0)
            return total_read > 0 ? total_read : -EIO;

        out        += chunk;
        total_read += (int64_t)chunk;
        to_read    -= chunk;
        to_skip     = 0;
        vcn        += run_len;

        if (to_read == 0) break;
    }
    return total_read;
}

static uint64_t scan_index_entries(uint8_t *p, uint8_t *end, const char *component) {
    while (p + sizeof(NtfsIndexEntry) <= end) {
        NtfsIndexEntry *entry = (NtfsIndexEntry *)p;
        if (entry->flags & 2) break;
        if (entry->key_length > 0) {
            NtfsFileName *fn = (NtfsFileName *)(p + sizeof(NtfsIndexEntry));
            char name[256];
            utf16_to_ascii(fn->name, fn->name_length, name);
            if (kstrcmp(name, component) == 0)
                return entry->mft_reference & 0x0000FFFFFFFFFFFFULL;
        }
        if (entry->entry_length == 0) break;
        p += entry->entry_length;
    }
    return (uint64_t)-1;
}

static int ascii_name_char(uint8_t c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '$' || c == '.' || c == '_' ||
           c == '-';
}

static int plausible_index_entry(const uint8_t *p, const uint8_t *end) {
    if (p + sizeof(NtfsIndexEntry) > end) return 0;

    const NtfsIndexEntry *e = (const NtfsIndexEntry *)p;
    uint32_t len = e->entry_length;
    uint32_t key = e->key_length;

    if (len < sizeof(NtfsIndexEntry) || p + len > end) return 0;
    if ((e->flags & ~0x3u) != 0) return 0;

    /* Terminating entries may have an empty key. Other NTFS directory
     * entries carry a $FILE_NAME key, which is always at least 66 bytes. */
    if (key == 0) return (e->flags & 2) ? 1 : 0;
    if (key < 66 || key > len) return 0;
    if (key > len) return 0;

    const NtfsFileName *fn = (const NtfsFileName *)(p + sizeof(NtfsIndexEntry));
    if (fn->name_length == 0 || fn->name_length > 240) return 0;

    uint32_t max_name = (key > 66) ? ((key - 66) / 2) : key;
    if (fn->name_length > max_name) return 0;

    for (uint32_t i = 0; i < fn->name_length; i++) {
        uint16_t c = fn->name[i];
        if ((c >> 8) != 0) return 0;
        if (!ascii_name_char((uint8_t)c)) return 0;
    }

    return 1;
}

static uint8_t *find_first_index_entries(uint8_t *buf, uint8_t *end) {
    uint8_t *p = buf;

    while (p + sizeof(NtfsIndexEntry) <= end) {
        if (plausible_index_entry(p, end)) return p;
        p += 8;
    }

    return NULL;
}

static uint64_t scan_index_alloc(NtfsState *s, NtfsAttr *attr, const char *component) {
    if (!attr) return (uint64_t)-1;

    if (!attr->non_resident) {
        uint32_t sz = 0;
        void *v = attr_resident_data(attr, &sz);
        if (!v) return (uint64_t)-1;

        uint8_t *val = (uint8_t *)v;
        uint8_t *end = val + sz;
        uint8_t *start = find_first_index_entries(val, end);
        if (!start) start = val;
        return scan_index_entries(start, end, component);
    }

    uint32_t sz = attr->nonresident.data_size;
    if (!sz) sz = attr->nonresident.alloc_size;
    if (!sz) return (uint64_t)-1;

    uint8_t *buf = (uint8_t *)kmalloc(sz);
    if (!buf) return (uint64_t)-1;

    if (read_nonresident(s, attr, buf, sz, 0) < 0) {
        kfree(buf);
        return (uint64_t)-1;
    }

    uint8_t *start = find_first_index_entries(buf, buf + sz);
    if (!start) start = buf;

    uint64_t found = scan_index_entries(start, buf + sz, component);
    kfree(buf);
    return found;
}

/* ── MFT record number by path ────────────────────────────────── */
/* Root directory is always MFT record 5 */
#define NTFS_ROOT_MFT  5ULL

static char fn_lower(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c + ('a' - 'A'));
    return c;
}

static int fname_ci_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (fn_lower(*a++) != fn_lower(*b++)) return 0;
    }
    return *a == '\0' && *b == '\0';
}

static int find_entry_in_record(NtfsFileRecord *rec, uint64_t parent_ref,
                                const char *component, VfsDirent *entry) {
    uint8_t *base = (uint8_t *)rec;
    uint8_t *end  = base + rec->used_size;
    uint8_t *p    = base + rec->attr_offset;

    while (p + sizeof(NtfsAttr) <= end) {
        NtfsAttr *a = (NtfsAttr *)p;
        if (a->type == NTFS_AT_END || a->length == 0) break;

        if (a->type == NTFS_AT_FILE_NAME && !a->non_resident) {
            uint32_t vlen = 0;
            void *val = attr_resident_data(a, &vlen);
            if (val) {
                NtfsFileName *fn = (NtfsFileName *)val;
                uint64_t fn_parent = fn->parent_dir & 0x0000FFFFFFFFFFFFULL;
                uint32_t need = (uint32_t)sizeof(NtfsFileName) -
                                (uint32_t)sizeof(uint16_t) +
                                2u * fn->name_length;

                if (fn_parent == parent_ref && fn->namespace != 2 &&
                    fn->name_length > 0 && fn->name_length <= 255 &&
                    need <= vlen) {
                    char name[256];
                    utf16_to_ascii(fn->name, fn->name_length, name);
                    if (name[0] == '$' || name[0] == '.') {
                        p += a->length;
                        continue;
                    }
                    if (!component || fname_ci_eq(name, component)) {
                        if (entry) {
                            utf16_to_ascii(fn->name, fn->name_length, entry->name);
                            entry->flags = (fn->file_attrs & NTFS_FILE_DIRECTORY)
                                             ? VFS_DIR : 0;
                            entry->size  = fn->real_size;
                        }
                        return (int)rec->mft_record_number;
                    }
                }
            }
        }

        p += a->length;
    }
    return -1;
}

static uint64_t lookup_child(NtfsState *s, uint64_t dir_mft,
                             const char *component) {
    uint8_t buf[4096];
    NtfsFileRecord *rec = (NtfsFileRecord *)buf;

    for (uint64_t n = 0; n < 4096; n++) {
        if (read_mft_record(s, n, rec) < 0) continue;
        if (kmemcmp(rec->magic, "FILE", 4) != 0) break;
        if (!(rec->flags & NTFS_FILE_IN_USE)) continue;

        int found = find_entry_in_record(rec, dir_mft, component, NULL);
        if (found >= 0) return (uint64_t)found;
    }
    return (uint64_t)-1;
}

static uint64_t lookup_path(NtfsState *s, const char *path) {
    uint64_t dir_mft = NTFS_ROOT_MFT;

    char path_copy[VFS_PATH_MAX];
    kstrncpy(path_copy, path, VFS_PATH_MAX);

    char *component = path_copy;
    while (*component == '/' || *component == '#') component++;
    if (*component == '\0') return dir_mft;

    char *slash;
    do {
        slash = kstrchr(component, '/');
        if (slash) *slash = '\0';

        uint64_t found = lookup_child(s, dir_mft, component);
        if (found == (uint64_t)-1) return (uint64_t)-1;
        dir_mft = found;

        if (slash) component = slash + 1;
        else       break;
    } while (slash && *component);

    return dir_mft;
}

/* ── Memmove for possible-overlap shifts ─────────────────────── */
static void memmove_fwd(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (n == 0 || d == s) return;
    if (d > s) {
        size_t i = n;
        while (i--) d[i] = s[i];
    } else {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    }
}

/* ── $FILE_NAME value helpers ────────────────────────────────── */
static uint32_t fn_value_size(uint32_t name_len) {
    return 66 + 2 * name_len;
}

static void fill_file_name(uint8_t *dst, uint64_t parent_ref,
                           const char *name, uint32_t name_len,
                           int is_dir, uint32_t size) {
    NtfsFileName *fn = (NtfsFileName *)dst;
    kmemset(fn, 0, 66 + 2 * name_len);
    fn->parent_dir  = parent_ref;
    fn->file_attrs  = is_dir ? 0x10 : 0x20;
    fn->real_size   = size;
    fn->name_length = (uint8_t)name_len;
    fn->namespace   = 1;
    for (uint32_t i = 0; i < name_len; i++)
        fn->name[i] = (uint16_t)(uint8_t)name[i];
}

/* ── Drop unused empty attributes (frees space for index growth) */
static void compact_record(NtfsFileRecord *rec) {
    uint8_t *base = (uint8_t *)rec;
    uint8_t *src  = base + rec->attr_offset;
    uint8_t *dst  = src;
    uint8_t *end  = base + rec->used_size;
    while (src < end) {
        NtfsAttr *a = (NtfsAttr *)src;
        if (a->type == NTFS_AT_END || a->length == 0) break;
        uint32_t len = a->length;
        int drop = (a->type == NTFS_AT_SECURITY);
        if (!drop && a->type == NTFS_AT_INDEX_ALLOC) {
            uint32_t idx_bytes = a->non_resident ? a->nonresident.data_size
                                                 : a->resident.value_length;
            drop = idx_bytes <= 64;
        }
        if (!drop && !a->non_resident && a->resident.value_length < 32 &&
            a->type != NTFS_AT_STANDARD_INFO &&
            a->type != NTFS_AT_FILE_NAME &&
            a->type != NTFS_AT_DATA &&
            a->type != NTFS_AT_INDEX_ROOT)
            drop = 1;
        if (drop) { src += len; continue; }
        if (dst != src) kmemcpy(dst, src, len);
        dst += len;
        src += len;
    }
    {
        NtfsAttr *e = (NtfsAttr *)dst;
        e->type  = NTFS_AT_END;
        e->length = 0;
    }
    dst += 4;
    dst = (uint8_t *)(((uintptr_t)dst + 7) & ~7UL);
    rec->used_size = (uint32_t)(dst - base);
}

/* ── Find a free MFT record ──────────────────────────────────── */
static int find_free_mft_record(NtfsState *s, uint64_t *out) {
    uint8_t rbuf[4096];
    if (s->bytes_per_record > sizeof(rbuf)) return -ENOSPC;
    for (uint64_t n = 16; n < 512; n++) {
        if (read_mft_record(s, n, (NtfsFileRecord *)rbuf) < 0) break;
        NtfsFileRecord *rec = (NtfsFileRecord *)rbuf;
        if (kmemcmp(rbuf, "FILE", 4) != 0) { *out = n; return 0; }
        if (!(rec->flags & NTFS_FILE_IN_USE)) { *out = n; return 0; }
    }
    return -1;
}

static uint8_t *find_end_marker(NtfsFileRecord *rec) {
    uint8_t *base = (uint8_t *)rec;
    uint8_t *end = base + rec->used_size;
    uint8_t *p = base + rec->attr_offset;
    while (p < end) {
        NtfsAttr *a = (NtfsAttr *)p;
        if (a->type == NTFS_AT_END || a->length == 0) return p;
        p += a->length;
    }
    return end;
}

static void fill_index_entry(uint8_t *p, uint64_t ref, uint32_t ent_len, uint32_t key_len, uint64_t parent_ref, const char *name, uint32_t name_len, int is_dir, uint32_t size) {
    kmemset(p, 0, ent_len);
    NtfsIndexEntry *ie = (NtfsIndexEntry *)p;
    ie->mft_reference = ref;
    ie->entry_length = (uint16_t)ent_len;
    ie->key_length = (uint16_t)key_len;
    ie->flags = 0;
    fill_file_name(p + sizeof(NtfsIndexEntry), parent_ref, name, name_len, is_dir, size);
}

static int insert_entry_in_buffer(uint8_t *buf, uint32_t sz, uint64_t dir_mft, uint64_t new_ref, const char *name, uint32_t name_len, int is_dir, uint32_t size, uint32_t *used_out) {
    uint8_t *end = buf + sz;
    uint8_t *start = find_first_index_entries(buf, end);
    uint8_t *p = start ? start : buf;
    uint8_t *last = p;
    uint8_t *term = NULL;
    uint32_t term_len = 0;
    uint32_t used = 0;

    while (p + sizeof(NtfsIndexEntry) <= end) {
        if (!plausible_index_entry(p, end)) break;

        NtfsIndexEntry *e = (NtfsIndexEntry *)p;
        last = p + e->entry_length;
        used = (uint32_t)(last - buf);

        if (e->flags & 2) {
            term = p;
            term_len = e->entry_length;
            used = (uint32_t)(term + term_len - buf);
            break;
        }

        p += e->entry_length;
    }

    uint32_t key_len = fn_value_size(name_len);
    uint32_t ent_len = (sizeof(NtfsIndexEntry) + key_len + 7) & ~7u;
    uint8_t *dst = term ? term : last;
    if (dst + ent_len > end) return -ENOSPC;

    if (term) {
        uint16_t new_len = (uint16_t)(term_len - ent_len);
        if (new_len < sizeof(NtfsIndexEntry)) return -ENOSPC;

        uint8_t *new_term = term + ent_len;
        memmove_fwd(new_term, term, term_len);
        fill_index_entry(dst, new_ref, ent_len, key_len, dir_mft, name, name_len, is_dir, size);

        ((NtfsIndexEntry *)new_term)->entry_length = new_len;
        ((NtfsIndexEntry *)new_term)->flags |= 2;

        used = (uint32_t)(new_term + new_len - buf);
    } else {
        fill_index_entry(dst, new_ref, ent_len, key_len, dir_mft, name, name_len, is_dir, size);
        used = (uint32_t)(dst + ent_len - buf);
    }

    if (used_out) *used_out = used;
    return 0;
}

static int ensure_index_alloc_attr(NtfsState *s, NtfsFileRecord *rec) {
    if (find_attr(rec, NTFS_AT_INDEX_ALLOC)) return 0;

    uint64_t rec_sz = s->bytes_per_record ? s->bytes_per_record : s->bytes_per_sector;
    uint64_t cluster_sz = s->bytes_per_cluster ? s->bytes_per_cluster : rec_sz;
    if (!rec_sz) rec_sz = 512;
    if (!cluster_sz) cluster_sz = rec_sz;

    uint64_t cap = rec_sz * 8;
    uint64_t min_cap = cluster_sz * 4;
    if (cap < min_cap) cap = min_cap;
    if (cap < rec_sz) cap = rec_sz;
    if (cap > 65536) cap = 65536;

    uint32_t clusters = (uint32_t)((cap + cluster_sz - 1) / cluster_sz);
    uint64_t start = 0;
    int found = -1;
    while (clusters) {
        found = find_free_clusters(s, clusters, &start);
        if (found == 0) break;
        clusters = (clusters + 1) / 2;
    }
    if (found != 0) return -ENOSPC;

    int ar = alloc_clusters(s, start, clusters);
    if (ar < 0) return ar;

    cap = (uint64_t)clusters * cluster_sz;

    uint8_t rl[64];
    uint32_t rl_len = build_runlist(rl, sizeof(rl), start, clusters);
    if (!rl_len) return -ENOSPC;

    uint32_t attr_len = (64 + rl_len + 7) & ~7u;
    uint8_t *base = (uint8_t *)rec;
    uint8_t *marker = find_end_marker(rec);

    if ((uint32_t)(marker - base) + attr_len + 8 > rec->alloc_size) {
        compact_record(rec);
        marker = find_end_marker(rec);
    }
    if ((uint32_t)(marker - base) + attr_len + 8 > rec->alloc_size)
        return -ENOSPC;

    kmemset(marker, 0, attr_len + 8);
    NtfsAttr *a = (NtfsAttr *)marker;
    a->type = NTFS_AT_INDEX_ALLOC;
    a->length = attr_len;
    a->non_resident = 1;
    a->attr_id = rec->next_attr_id++;
    kmemcpy((uint8_t *)a + 24, rl, rl_len);
    a->nonresident.start_vcn = 0;
    a->nonresident.last_vcn = clusters - 1;   /* highest VCN is inclusive */
    a->nonresident.runlist_offset = 64;
    kmemcpy((uint8_t *)a + a->nonresident.runlist_offset, rl, rl_len);
    a->nonresident.alloc_size = cap;
    a->nonresident.data_size = 0;
    a->nonresident.init_size = 0;
    *(uint32_t *)(marker + attr_len) = NTFS_AT_END;
    rec->used_size = ((uint32_t)(marker - base) + attr_len + 8 + 7) & ~7u;
    return 0;
}

static int append_index_entry_to_alloc(NtfsState *s, NtfsFileRecord *rec, uint64_t dir_mft, uint64_t new_ref, const char *name, uint32_t name_len, int is_dir, uint32_t size) {
    int rc = ensure_index_alloc_attr(s, rec);
    if (rc < 0) return rc;

    NtfsAttr *a = find_attr(rec, NTFS_AT_INDEX_ALLOC);
    if (!a) return -ENOSPC;

    uint32_t used = 0;

    if (a->non_resident) {
        uint32_t cap = a->nonresident.alloc_size ? a->nonresident.alloc_size
                                                   : a->nonresident.data_size;
        if (!cap) return -ENOSPC;

        uint8_t *buf = (uint8_t *)kmalloc(cap);
        if (!buf) return -ENOMEM;
        kmemset(buf, 0, cap);

        uint32_t old_size = a->nonresident.data_size;
        if (old_size > cap) old_size = cap;

        if (old_size && read_nonresident(s, a, buf, old_size, 0) < 0) {
            kfree(buf);
            return -EIO;
        }

        rc = insert_entry_in_buffer(buf, cap, dir_mft, new_ref, name, name_len, is_dir, size, &used);
        if (rc < 0) {
            kfree(buf);
            return rc;
        }

        rc = write_nonresident(s, a, buf, cap, 0);
        kfree(buf);
        if (rc < 0) return rc;

        if (used > a->nonresident.data_size) a->nonresident.data_size = used;
        return 0;
    }

    uint32_t old_size = 0;
    void *val = attr_resident_data(a, &old_size);
    if (!val) return -EIO;

    uint32_t cap = a->length - a->resident.value_offset;
    rc = insert_entry_in_buffer((uint8_t *)val, cap, dir_mft, new_ref, name, name_len, is_dir, size, &used);
    if (rc < 0) return rc;

    if (used > old_size) a->resident.value_length = used;
    return 0;
}

/* ── Insert an index entry into a parent directory ───────────── */
static int add_index_entry(NtfsState *s, uint64_t dir_mft, uint64_t new_ref,
                           const char *name, uint32_t name_len,
                           int is_dir, uint32_t size) {
    uint8_t rec_buf[4096];
    if (s->bytes_per_record > sizeof(rec_buf)) return -ENOSPC;

    NtfsFileRecord *rec = (NtfsFileRecord *)rec_buf;
    int rr = read_mft_record(s, dir_mft, rec);
    if (rr < 0) return -EIO;

    NtfsAttr *attr = find_attr(rec, NTFS_AT_INDEX_ROOT);
    if (!attr || attr->non_resident) return -EIO;

    uint32_t vlen = attr->resident.value_length;
    void *val = attr_resident_data(attr, &vlen);
    if (!val) return -EIO;

    uint8_t  *base      = (uint8_t *)rec;
    uint8_t  *attrp     = (uint8_t *)attr;
    uint32_t  attr_start = (uint32_t)(attrp - base);
    uint32_t  key_len    = fn_value_size(name_len);
    uint32_t  ent_len    = ((sizeof(NtfsIndexEntry) + key_len + 7) & ~7u);
    uint32_t  want_used  = ((vlen + ent_len + 7) & ~7u);
    uint32_t  want_len   = attr->resident.value_offset + want_used;
    uint32_t  next_attr  = attr_start + attr->length;
    int       expanded   = 0;

    /* Newly-created directories usually have a tiny $INDEX_ROOT. When it
     * is the final attribute, grow it into the record's unused tail instead
     * of forcing the entry through $INDEX_ALLOC. */
    if (attr->length < want_len &&
        (next_attr >= rec->used_size || *(uint32_t *)(base + next_attr) == NTFS_AT_END)) {
        uint32_t max_len = rec->alloc_size > attr_start + 160
                             ? rec->alloc_size - attr_start - 160
                             : attr->length;
        if (max_len > attr->length) {
            attr->length = want_len < max_len ? want_len : max_len;
            expanded = 1;
        }
    }

    uint32_t cap = attr->length - attr->resident.value_offset;
    uint32_t used = 0;
    int rc = insert_entry_in_buffer((uint8_t *)val, cap, dir_mft, new_ref, name, name_len, is_dir, size, &used);
    if (rc >= 0) {
        if (used > vlen) attr->resident.value_length = used;
        if (expanded) {
            uint32_t marker = attr_start + attr->length;
            *(uint32_t *)(base + marker) = NTFS_AT_END;
            uint32_t next_used = (marker + 8 + 7) & ~7u;
            if (next_used > rec->used_size) rec->used_size = next_used;
        }
        return write_mft_record(s, dir_mft, rec);
    }

    rc = append_index_entry_to_alloc(s, rec, dir_mft, new_ref, name, name_len, is_dir, size);
    if (rc < 0) return rc;

    return write_mft_record(s, dir_mft, rec);
}

/* ── Create a file or directory ──────────────────────────────── */
static int ntfs_create(NtfsState *s, const char *path, int is_dir) {
    if (!path || *path == '\0') return -EINVAL;

    char pcopy[VFS_PATH_MAX];
    kstrncpy(pcopy, path, VFS_PATH_MAX);

    char *c = pcopy;
    while (*c == '/' || *c == '#') c++;
    char *slash = NULL;
    for (char *q = c; *q; q++) if (*q == '/') slash = q;

    const char *parent;
    const char *name;
    if (slash) {
        *slash = '\0';
        parent = c;
        name   = slash + 1;
    } else {
        parent = "";
        name   = c;
    }

    uint64_t parent_ref = NTFS_ROOT_MFT;
    if (*parent) {
        parent_ref = lookup_path(s, parent);
        if (parent_ref == (uint64_t)-1) return -ENOENT;
    }

    uint32_t name_len = 0;
    while (name[name_len]) {
        if ((uint8_t)name[name_len] > 0x7F) return -EINVAL;
        name_len++;
    }
    if (name_len == 0 || name_len > 240) return -EINVAL;

    {
        char childpath[VFS_PATH_MAX];
        if (*parent) {
            kstrncpy(childpath, parent, VFS_PATH_MAX);
            size_t pl = kstrlen(childpath);
            if (pl + 1 + name_len + 1 < VFS_PATH_MAX) {
                childpath[pl] = '/';
                kstrncpy(childpath + pl + 1, name, VFS_PATH_MAX - pl - 1);
            }
            if (lookup_path(s, childpath) != (uint64_t)-1) return 0;
        } else {
            if (lookup_path(s, name) != (uint64_t)-1) return 0;
        }
    }

    uint64_t new_num = 0;
    if (find_free_mft_record(s, &new_num) < 0) return -ENOSPC;

    uint8_t rbuf[4096];
    uint32_t bpr = s->bytes_per_record;
    if (bpr > sizeof(rbuf)) return -ENOSPC;
    kmemset(rbuf, 0, bpr);
    NtfsFileRecord *rec = (NtfsFileRecord *)rbuf;
    rec->magic[0] = 'F'; rec->magic[1] = 'I';
    rec->magic[2] = 'L'; rec->magic[3] = 'E';
    rec->sequence_number = 1;
    rec->link_count      = 1;
    rec->attr_offset     = 56;
    rec->flags           = NTFS_FILE_IN_USE
                           | (is_dir ? NTFS_FILE_DIRECTORY : 0);
    rec->used_size       = bpr;
    rec->alloc_size      = bpr;
    rec->mft_record_number = (uint32_t)new_num;

    uint32_t off = 56;

    NtfsAttr *a = (NtfsAttr *)(rbuf + off);
    a->type = NTFS_AT_STANDARD_INFO;
    a->length = 24 + 48;
    a->non_resident = 0;
    a->name_length = 0;
    a->name_offset = 0;
    a->flags = 0;
    a->attr_id = 0;
    a->resident.value_length = 48;
    a->resident.value_offset = 24;
    a->resident._flags = 0;
    {
        uint8_t *v = (uint8_t *)a + 24;
        kmemset(v, 0, 48);
        if (is_dir) *(uint32_t *)(v + 32) = 0x10;
    }
    off += a->length;

    a = (NtfsAttr *)(rbuf + off);
    {
        uint32_t key = fn_value_size(name_len);
        a->type = NTFS_AT_FILE_NAME;
        a->length = (24 + key + 7) & ~7u;
        a->non_resident = 0;
        a->name_length = 0;
        a->name_offset = 0;
        a->flags = 0;
        a->attr_id = 0;
        a->resident.value_length = key;
        a->resident.value_offset = 24;
        a->resident._flags = 0;
        fill_file_name((uint8_t *)a + 24, parent_ref, name,
                       name_len, is_dir, 0);
    }
    off += a->length;

    if (is_dir) {
        a = (NtfsAttr *)(rbuf + off);
        a->type = NTFS_AT_INDEX_ROOT;
        a->length = 24 + 56;
        a->non_resident = 0;
        a->name_length = 0;
        a->name_offset = 0;
        a->flags = 0;
        a->attr_id = 0;
        a->resident.value_length = 56;
        a->resident.value_offset = 24;
        a->resident._flags = 0;
        {
            uint8_t *v = (uint8_t *)a + 24;
            *(uint32_t *)(v + 0)  = 0x30;
            *(uint32_t *)(v + 4)  = 1;
            *(uint32_t *)(v + 8)  = 4096;
            v[12] = 1;
            *(uint32_t *)(v + 16) = 16;
            *(uint32_t *)(v + 20) = 24;
            *(uint32_t *)(v + 24) = 24;
            v[28] = 0;
            NtfsIndexEntry *t = (NtfsIndexEntry *)(v + 32);
            t->mft_reference = 0;
            t->entry_length  = 24;
            t->key_length    = 0;
            t->flags         = 3;
        }
        off += a->length;
    } else {
        uint64_t start_lcn = 0;
        int r = find_free_clusters(s, 1, &start_lcn);
        if (r < 0) return r;
        r = alloc_clusters(s, start_lcn, 1);
        if (r < 0) return r;

        a = (NtfsAttr *)(rbuf + off);
        a->type = NTFS_AT_DATA;
        a->non_resident = 1;
        a->name_length = 0;
        a->name_offset = 0;
        a->flags = 0;
        a->attr_id = 0;
        a->nonresident.start_vcn = 0;
        a->nonresident.last_vcn = 0;
        a->nonresident.runlist_offset = 24;
        a->nonresident.runlist_offset = 64;
        a->nonresident.compression_unit = 0;
        a->nonresident._pad = 0;
        a->nonresident.alloc_size = s->bytes_per_cluster;
        a->nonresident.data_size = 0;
        a->nonresident.init_size = 0;

        uint8_t runlist[64];
        uint32_t rl_len = build_runlist(runlist, sizeof(runlist), start_lcn, 1);
        a->length = (64 + rl_len + 7) & ~7u;
        kmemcpy((uint8_t *)a + 64, runlist, rl_len);
        off += a->length;
    }

    off = (off + 7) & ~7u;
    *(uint32_t *)(rbuf + off) = 0xFFFFFFFF;
    off = (off + 8) & ~7u;
    rec->used_size = off;

    if (write_mft_record(s, new_num, rec) < 0) return -EIO;

    (void)parent_ref;
    return 0;
}

/* ── Open file handle ────────────────────────────────────────── */
typedef struct {
    NtfsState      *state;
    uint64_t        mft_record;
    uint64_t        file_size;
    int             is_dir;
    /* For non-resident $DATA: cache the attribute location */
    uint8_t         rec_buf[4096];
} NtfsFileHandle;

/* ══════════════════════════════════════════════════════════════ */
/*  VFS DRIVER IMPLEMENTATION                                    */
/* ══════════════════════════════════════════════════════════════ */

static int ntfs_mount(const char *device, void **fs_data_out) {
    /*
     * Device string formats:
     *   "ata:D:LBA"  — drive index D, partition start LBA (preferred)
     *   "/dev/sdX"   — legacy: drive index from last char
     */
    extern int ata_read_sectors(int drive, uint64_t lba,
                                uint32_t count, void *buf);
    int      drive_idx    = 0;
    uint64_t part_start   = 0;

    if (device[0] == 'a' && device[1] == 't' && device[2] == 'a'
        && device[3] == ':') {
        /* "ata:D:LBA" */
        drive_idx = device[4] - '0';
        const char *p = device + 6;
        while (*p >= '0' && *p <= '9')
            part_start = part_start * 10 + (uint64_t)(*p++ - '0');
    } else {
        /* "/dev/sdX" legacy */
        int len = 0; while (device[len]) len++;
        drive_idx = len > 0 ? (device[len-1] - 'a') : 0;
        part_start = 0;
    }

    NtfsState *s = (NtfsState *)kmalloc(sizeof(NtfsState));
    if (!s) return -ENOMEM;
    kmemset(s, 0, sizeof(NtfsState));
    s->drive_idx      = drive_idx;
    s->part_start_lba = part_start;

    /* Read boot sector (LBA 0 of partition) */
    NtfsBoot boot;
    uint8_t sector[512];
    if (ata_read_sectors(drive_idx, part_start, 1, sector) < 0) {
        kfree(s); return -EIO;
    }
    kmemcpy(&boot, sector, sizeof(NtfsBoot));

    if (kmemcmp(boot.oem_id, "NTFS    ", 8) != 0) {
        kfree(s); return -EINVAL; /* not NTFS */
    }

    s->bytes_per_sector    = boot.bytes_per_sector;
    s->sectors_per_cluster = boot.sectors_per_cluster;
    s->bytes_per_cluster   = s->bytes_per_sector * s->sectors_per_cluster;

    /* File record size: if clusters_per_file_record < 0, it's 2^|value| */
    if (boot.clusters_per_file_record < 0)
        s->bytes_per_record = 1u << (uint8_t)(-boot.clusters_per_file_record);
    else
        s->bytes_per_record = (uint32_t)boot.clusters_per_file_record
                              * s->bytes_per_cluster;

    s->mft_offset = boot.mft_lcn * s->bytes_per_cluster;

    /* Volume geometry bounds for the cluster allocator. total_sectors is the
     * hidden-sector-relative count written by mkntfs. */
    s->total_clusters = s->sectors_per_cluster
                      ? boot.total_sectors / s->sectors_per_cluster
                      : boot.total_sectors;

    /* NTFS reserves an MFT zone at the start of the volume. mkntfs sizes it
     * to hold the initial $MFT; keep the whole zone off-limits to file data
     * so a grown $MFT can never be clobbered by a data allocation. */
    {
        uint64_t mft_lcn  = boot.mft_lcn;
        uint64_t reserve = 0;
        /* $MFTMirr immediately follows the reserve window mkntfs created. */
        if (boot.mft_mirror_lcn > mft_lcn)
            reserve = boot.mft_mirror_lcn - mft_lcn;
        s->mft_zone_end = mft_lcn + (reserve ? reserve : 16);
        if (s->mft_zone_end > s->total_clusters)
            s->mft_zone_end = s->total_clusters;
    }

    /* Cache first 128 MFT records (covers system files) */
    s->mft_cache = (uint8_t *)kmalloc(128 * s->bytes_per_record);
    if (s->mft_cache) {
        s->mft_cache_count = 128;
        /* Limit MFT cache to avoid kmalloc of huge chunk */
        uint32_t cache_size = 128 * s->bytes_per_record;
        if (cache_size <= 256*1024)
            disk_read(s, s->mft_offset, s->mft_cache, cache_size);
    }

    *fs_data_out = s;
    return 0;
}

static void ntfs_umount(void *fs_data) {
    NtfsState *s = (NtfsState *)fs_data;
    if (s->mft_cache) kfree(s->mft_cache);
    kfree(s);
}

static int ntfs_open(void *fs, const char *path, int flags,
                     int mode, void **file_data_out) {
    (void)mode;
    NtfsState *s = (NtfsState *)fs;

    uint64_t mft_num = lookup_path(s, path);
    if (mft_num == (uint64_t)-1) {
        if (!(flags & VFS_O_CREATE)) return -ENOENT;
        int cr = ntfs_create(s, path, 0);
        if (cr < 0) return cr;
        mft_num = lookup_path(s, path);
        if (mft_num == (uint64_t)-1) return -EIO;
    }

    NtfsFileHandle *fh = (NtfsFileHandle *)kmalloc(sizeof(NtfsFileHandle));
    if (!fh) return -ENOMEM;
    kmemset(fh, 0, sizeof(NtfsFileHandle));

    fh->state      = s;
    fh->mft_record = mft_num;
    read_mft_record(s, mft_num, (NtfsFileRecord *)fh->rec_buf);

    NtfsFileRecord *rec = (NtfsFileRecord *)fh->rec_buf;
    fh->is_dir = (rec->flags & NTFS_FILE_DIRECTORY) ? 1 : 0;

    /* Get file size from $DATA */
    NtfsAttr *data_attr = find_attr(rec, NTFS_AT_DATA);
    if (data_attr) {
        if (data_attr->non_resident)
            fh->file_size = data_attr->nonresident.data_size;
        else
            fh->file_size = data_attr->resident.value_length;
    }

    *file_data_out = fh;
    return 0;
}

static void ntfs_close(void *file_data) {
    kfree(file_data);
}

static int64_t ntfs_read(void *file_data, void *buf, size_t n, int64_t pos) {
    NtfsFileHandle *fh  = (NtfsFileHandle *)file_data;
    NtfsFileRecord *rec = (NtfsFileRecord *)fh->rec_buf;

    if (pos >= (int64_t)fh->file_size) return 0;
    if ((uint64_t)pos + n > fh->file_size)
        n = (size_t)(fh->file_size - (uint64_t)pos);

    NtfsAttr *data_attr = find_attr(rec, NTFS_AT_DATA);
    if (!data_attr) return -EIO;

    if (!data_attr->non_resident) {
        /* Resident: data is inline in the MFT record */
        uint32_t dsize = 0;
        void *data = attr_resident_data(data_attr, &dsize);
        if (pos >= dsize) return 0;
        size_t avail = dsize - (size_t)pos;
        if (n > avail) n = avail;
        kmemcpy(buf, (uint8_t *)data + pos, n);
        return (int64_t)n;
    }

    /* Non-resident: walk the run list */
    return read_nonresident(fh->state, data_attr, buf, n, pos);
}

static uint32_t encoded_runlist_len(const uint8_t *rl) {
    uint32_t len = 0;
    while (*rl) {
        uint8_t h  = *rl++;
        uint8_t lb = h & 0x0F;
        uint8_t ob = (h >> 4) & 0x0F;
        len += 1u + lb + ob;
        rl += lb + ob;
    }
    return len;
}

static int decode_next_run(const uint8_t **pp, uint64_t *len_out, int64_t *off_out) {
    const uint8_t *rl = *pp;
    if (!*rl) return 0;

    uint8_t h  = *rl++;
    uint8_t lb = h & 0x0F;
    uint8_t ob = (h >> 4) & 0x0F;

    uint64_t len = 0;
    for (uint8_t i = 0; i < lb; i++)
        len |= (uint64_t)(*rl++) << (i * 8);

    int64_t off = 0;
    if (ob) {
        uint64_t raw = 0;
        for (uint8_t i = 0; i < ob; i++)
            raw |= (uint64_t)(*rl++) << (i * 8);
        uint64_t sign_bit = 1ULL << (ob * 8 - 1);
        if (raw & sign_bit)
            raw |= ~((sign_bit << 1) - 1);
        off = (int64_t)raw;
    }

    *pp = rl;
    if (len_out) *len_out = len;
    if (off_out) *off_out = off;
    return 1;
}

static void scan_runlist(const uint8_t *rl, uint64_t *count_out, int64_t *last_start_out) {
    uint64_t count = 0;
    int64_t cur = 0;
    int seen = 0;

    uint64_t len;
    int64_t off;
    while (decode_next_run(&rl, &len, &off)) {
        cur += off;
        count += len;
        seen = 1;
    }

    if (count_out) *count_out = count;
    if (last_start_out) *last_start_out = seen ? cur : 0;
}

static int64_t ntfs_write(void *file_data, const void *buf,
                          size_t n, int64_t pos) {
    NtfsFileHandle *fh  = (NtfsFileHandle *)file_data;
    NtfsFileRecord *rec = (NtfsFileRecord *)fh->rec_buf;
    NtfsAttr *data_attr = find_attr(rec, NTFS_AT_DATA);
    if (!data_attr) return -EIO;
    if (pos < 0) return -EINVAL;

    NtfsState *s = fh->state;

    if (!data_attr->non_resident) {
        uint32_t dsize = 0;
        void *data = attr_resident_data(data_attr, &dsize);
        if (!data) return -EIO;

        uint32_t cap = data_attr->length - data_attr->resident.value_offset;
        if ((uint64_t)pos >= cap) return 0;
        if ((uint64_t)pos + n > cap) n = cap - (uint64_t)pos;

        kmemcpy((uint8_t *)data + pos, buf, n);
        if ((uint64_t)pos + n > dsize)
            data_attr->resident.value_length = (uint32_t)((uint64_t)pos + n);
        if ((uint64_t)pos + n > fh->file_size)
            fh->file_size = (uint64_t)pos + (uint64_t)n;

        if (write_mft_record(s, fh->mft_record, rec) < 0) return -EIO;
        return (int64_t)n;
    }

    uint8_t  *base        = (uint8_t *)rec;
    uint64_t cluster_size = s->bytes_per_cluster ? s->bytes_per_cluster : s->bytes_per_sector;
    uint64_t file_end = (uint64_t)pos + n;
    uint8_t  *rl_start    = (uint8_t *)data_attr + data_attr->nonresident.runlist_offset;
    uint32_t old_rl_len   = encoded_runlist_len(rl_start);
    uint64_t clusters     = 0;
    int64_t  last_start   = 0;

    scan_runlist(rl_start, &clusters, &last_start);

    uint64_t needed = (file_end + cluster_size - 1) / cluster_size;
    uint64_t add    = needed > clusters ? needed - clusters : 0;

    uint8_t  new_rl[1024];
    uint32_t new_rl_len = 0;
    if (old_rl_len > sizeof(new_rl)) return -ENOSPC;
    kmemcpy(new_rl, rl_start, old_rl_len);
    new_rl_len = old_rl_len;

    if (add) {
        uint64_t start = 0;
        int r = find_free_clusters(s, add, &start);
        if (r < 0) return r;
        r = alloc_clusters(s, start, add);
        if (r < 0) return r;

        uint8_t tmp[24];
        uint32_t enc = encode_runlist(tmp, sizeof(tmp), add,
                                      (int64_t)start, last_start);
        if (!enc || new_rl_len + enc > sizeof(new_rl)) return -ENOSPC;
        kmemcpy(new_rl + new_rl_len, tmp, enc);
        new_rl_len += enc;
        clusters   += add;
    }

    uint32_t need_attr_len =
        (data_attr->nonresident.runlist_offset + new_rl_len + 1 + 7) & ~7u;

    if (need_attr_len > data_attr->length) {
        uint8_t *marker = find_end_marker(rec);
        uint8_t *attr_end = (uint8_t *)data_attr + data_attr->length;
        if (marker != attr_end) return -ENOSPC;

        uint32_t attr_start = (uint32_t)((uint8_t *)data_attr - base);
        uint32_t max_len = rec->alloc_size > attr_start + 8
                             ? rec->alloc_size - attr_start - 8
                             : attr_start;
        if (max_len < need_attr_len) return -ENOSPC;

        data_attr->length = need_attr_len;
        uint32_t marker_off = attr_start + data_attr->length;
        *(uint32_t *)(base + marker_off) = NTFS_AT_END;
        uint32_t next_used = (marker_off + 8 + 7) & ~7u;
        if (next_used > rec->used_size) rec->used_size = next_used;
    }

    kmemcpy(rl_start, new_rl, new_rl_len);
    rl_start[new_rl_len] = 0;
    data_attr->nonresident.last_vcn   = clusters ? clusters - 1 : 0;
    data_attr->nonresident.alloc_size = clusters * cluster_size;

    /* Persist the extended runlist/sizes BEFORE touching data, so a later
     * read can decode the attribute that now describes those clusters. */
    if (write_mft_record(s, fh->mft_record, rec) < 0) return -EIO;

    int wr = write_nonresident(s, data_attr, buf, n, pos);
    if (wr < 0) return wr;

    if (file_end > data_attr->nonresident.data_size)
        data_attr->nonresident.data_size = file_end;
    /* Uncompressed data is initialized up to data_size. */
    if (data_attr->nonresident.data_size > data_attr->nonresident.init_size)
        data_attr->nonresident.init_size = data_attr->nonresident.data_size;
    if (file_end > fh->file_size)
        fh->file_size = file_end;

    if (write_mft_record(s, fh->mft_record, rec) < 0) return -EIO;
    return (int64_t)n;
}

static int ntfs_stat(void *fs, const char *path, VfsFileInfo *info) {
    NtfsState *s = (NtfsState *)fs;
    uint64_t mft_num = lookup_path(s, path);
    if (mft_num == (uint64_t)-1) return -ENOENT;

    uint8_t rec_buf[4096];
    NtfsFileRecord *rec = (NtfsFileRecord *)rec_buf;
    if (read_mft_record(s, mft_num, rec) < 0) return -EIO;

    info->flags = (rec->flags & NTFS_FILE_DIRECTORY) ? VFS_DIR : 0;
    NtfsAttr *da = find_attr(rec, NTFS_AT_DATA);
    info->size = da ? (da->non_resident ? da->nonresident.data_size
                                        : da->resident.value_length) : 0;
    /* Times from $STANDARD_INFORMATION — simplified */
    info->mtime = 0;
    info->ctime = 0;
    return 0;
}

static int ntfs_stat_fd(void *file_data, VfsFileInfo *info) {
    NtfsFileHandle *fh = (NtfsFileHandle *)file_data;
    info->size  = fh->file_size;
    info->flags = fh->is_dir ? VFS_DIR : 0;
    info->mtime = 0;
    return 0;
}

/* ── Directory listing ───────────────────────────────────────── */
typedef struct {
    NtfsState      *state;
    uint64_t        dir_mft;
    uint64_t        next_record;
} NtfsDirHandle;

static int ntfs_opendir(void *fs, const char *path, void **dir_data_out) {
    NtfsState *s = (NtfsState *)fs;
    uint64_t mft_num = lookup_path(s, path);
    if (mft_num == (uint64_t)-1) return -ENOENT;

    NtfsDirHandle *dh = (NtfsDirHandle *)kmalloc(sizeof(NtfsDirHandle));
    if (!dh) return -ENOMEM;
    dh->state       = s;
    dh->dir_mft     = mft_num;
    dh->next_record = 0;

    *dir_data_out = dh;
    return 0;
}

static int ntfs_readdir(void *dir_data, VfsDirent *entry) {
    NtfsDirHandle *dh = (NtfsDirHandle *)dir_data;
    uint8_t buf[4096];
    NtfsFileRecord *rec = (NtfsFileRecord *)buf;

    while (dh->next_record < 4096) {
        uint64_t n = dh->next_record++;
        if (read_mft_record(dh->state, n, rec) < 0) continue;
        if (kmemcmp(rec->magic, "FILE", 4) != 0) break;
        if (!(rec->flags & NTFS_FILE_IN_USE)) continue;

        if (find_entry_in_record(rec, dh->dir_mft, NULL, entry) >= 0)
            return 0;
    }
    return -1; /* EOF */
}

static void ntfs_closedir(void *dir_data) { kfree(dir_data); }

static int ntfs_mkdir(void *fs, const char *path) {
    NtfsState *s = (NtfsState *)fs;
    return ntfs_create(s, path, 1) < 0 ? -EIO : 0;
}

static int ntfs_unlink(void *fs, const char *path) {
    (void)fs; (void)path;
    /* TODO: mark MFT record as not-in-use, remove from parent index */
    return -ENOSYS;
}

static int ntfs_rename(void *fs, const char *src, const char *dst) {
    (void)fs; (void)src; (void)dst;
    /* TODO: update $FILE_NAME in MFT record, re-insert in parent index */
    return -ENOSYS;
}

/* ── Driver descriptor ───────────────────────────────────────── */
static VfsDriver g_ntfs_driver = {
    .name       = "ntfs",
    .mount      = ntfs_mount,
    .umount     = ntfs_umount,
    .open       = ntfs_open,
    .close      = ntfs_close,
    .read       = ntfs_read,
    .write      = ntfs_write,
    .stat       = ntfs_stat,
    .stat_fd    = ntfs_stat_fd,
    .opendir    = ntfs_opendir,
    .readdir    = ntfs_readdir,
    .closedir   = ntfs_closedir,
    .mkdir      = ntfs_mkdir,
    .unlink     = ntfs_unlink,
    .rename     = ntfs_rename,
    .touch      = NULL,
    .chmod      = NULL,
};

void ntfs_register(void) {
    vfs_register_driver(&g_ntfs_driver);
}
