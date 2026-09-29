/*
 * Notux OS — ATA PIO 28/48-bit driver
 * kernel/drivers/disk/ata.c
 *
 * Supports:
 *   - Primary   (base 0x1F0, ctrl 0x3F6): master + slave
 *   - Secondary (base 0x170, ctrl 0x376): master + slave
 *   - LBA48 for drives > 137 GB
 *   - IDENTIFY to detect drives and get sector count
 *
 * All I/O is PIO (polling).  No DMA.  Enough for boot + setup.
 */
#include "ata.h"
#include "../../kernel.h"
#include "../../kserial.h"
#include "../gfx/font.h"
#include <stdint.h>
#include <stddef.h>

/* ── Port bases ────────────────────────────────────────────── */
static const uint16_t ata_base[2]  = { 0x1F0, 0x170 };
static const uint16_t ata_ctrl[2]  = { 0x3F6, 0x376 };

/* ── Register offsets from base ────────────────────────────── */
#define ATA_REG_DATA      0
#define ATA_REG_ERROR     1
#define ATA_REG_FEATURES  1
#define ATA_REG_SECCOUNT0 2
#define ATA_REG_LBA0      3
#define ATA_REG_LBA1      4
#define ATA_REG_LBA2      5
#define ATA_REG_HDDEVSEL  6
#define ATA_REG_STATUS    7
#define ATA_REG_COMMAND   7
#define ATA_REG_SECCOUNT1 8   /* LBA48 high byte — sent via 2nd write to SECCOUNT0 */
#define ATA_REG_LBA3      9
#define ATA_REG_LBA4      10
#define ATA_REG_LBA5      11

/* ── Status bits ────────────────────────────────────────────── */
#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01
/* NOTE: status bit 5 (0x20) is NOT a device-fault indicator here. QEMU's
 * hw/ide reports 0x78 (READY|SEEK|DRQ) during a normal PIO transfer, so
 * testing 0x20 would flag every successful transfer as a fault. Only ERR
 * (0x01) signals a real failure. */

/* ── Commands ───────────────────────────────────────────────── */
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_READ_PIO48  0x24
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_WRITE_PIO48 0x34
#define ATA_CMD_IDENTIFY    0xEC


static void kser_hex8(uint8_t v){
    kser_putc("0123456789ABCDEF"[v>>4]);
    kser_putc("0123456789ABCDEF"[v&0xF]);
}
AtaDrive ata_drives[ATA_MAX_DRIVES];
int      ata_drive_count = 0;

/* ── I/O helpers ────────────────────────────────────────────── */
static inline void outb(uint16_t p, uint8_t v)
    { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t inb(uint16_t p)
    { uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline uint16_t inw(uint16_t p)
    { uint16_t v; __asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void outw(uint16_t p, uint16_t v)
    { __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }

/* 400ns delay: read alt-status 4 times */
static void ata_delay(uint16_t ctrl) {
    inb(ctrl); inb(ctrl); inb(ctrl); inb(ctrl);
}

/* Poll budget. Under TCG an inb() is ~150ns, so 1e6 is ~150ms: long enough
 * for a real disk, short enough that a wedged drive reports instead of
 * appearing to hang. */
#define ATA_SPIN_LIMIT 1000000u

/* Wait until BSY clears; returns status, or 0xFF on timeout. */
static uint8_t ata_wait(uint16_t base, uint16_t ctrl) {
    (void)base;
    for (uint32_t i = 0; i < ATA_SPIN_LIMIT; i++) {
        uint8_t s = inb(ctrl);
        if (!(s & ATA_SR_BSY)) return s;
    }
    return 0xFF;
}

/* Wait for DRQ. Returns 0 on success, negative on error/timeout. */
static int ata_wait_drq(uint16_t base) {
    for (uint32_t i = 0; i < ATA_SPIN_LIMIT; i++) {
        uint8_t s = inb(base + ATA_REG_STATUS);
        if (s & (ATA_SR_ERR)) {
            kser_puts("ATA err status="); kser_hex64(s);
            kser_puts(" error="); kser_hex64(inb(base + ATA_REG_ERROR));
            kser_puts("\n");
            return -1;
        }
        if (!(s & ATA_SR_BSY) && (s & ATA_SR_DRQ)) return 0;
    }
    kser_puts("ATA timeout waiting DRQ status=");
    kser_hex64(inb(base + ATA_REG_STATUS));
    kser_puts("\n");
    return -1;
}

/* ── IDENTIFY ───────────────────────────────────────────────── */
static int ata_identify(int ctrl_idx, int slave, AtaDrive *drv) {
    uint16_t base = ata_base[ctrl_idx];
    uint16_t ctrl = ata_ctrl[ctrl_idx];

    /* Select drive */
    outb(base + ATA_REG_HDDEVSEL, (uint8_t)(slave ? 0xB0 : 0xA0));
    ata_delay(ctrl);

    /* Zero LBA/count registers */
    outb(base + ATA_REG_SECCOUNT0, 0);
    outb(base + ATA_REG_LBA0, 0);
    outb(base + ATA_REG_LBA1, 0);
    outb(base + ATA_REG_LBA2, 0);

    /* Send IDENTIFY */
    outb(base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay(ctrl);

    uint8_t s = inb(base + ATA_REG_STATUS);
    if (s == 0) return 0; /* no drive */

    /* Wait for BSY to clear */
    if (ata_wait_drq(base) < 0) return 0;

    /* Check if this is actually ATA (not ATAPI) */
    /* Read LBA1/LBA2 to drain registers - ATAPI check removed */
    (void)inb(base + ATA_REG_LBA1);
    (void)inb(base + ATA_REG_LBA2);
    /* ATAPI: lba1=0x14 lba2=0xEB or lba1=0x69 lba2=0x96 */

    /* Read 256 words of IDENTIFY data */
    uint16_t id[256];
    for (int i = 0; i < 256; i++) id[i] = inw(base + ATA_REG_DATA);

    /* Parse model string (words 27-46, big-endian byte swap) */
    char *m = drv->model;
    for (int i = 27; i <= 46; i++) {
        m[(i-27)*2]   = (char)(id[i] >> 8);
        m[(i-27)*2+1] = (char)(id[i] & 0xFF);
    }
    m[40] = '\0';
    /* Trim trailing spaces */
    for (int i = 39; i >= 0 && m[i] == ' '; i--) m[i] = '\0';

    /* LBA48 sector count (words 100-103) */
    drv->sector_count = ((uint64_t)id[103] << 48) |
                        ((uint64_t)id[102] << 32) |
                        ((uint64_t)id[101] << 16) |
                        ((uint64_t)id[100]);
    if (!drv->sector_count) {
        /* Fall back to LBA28 (word 60-61) */
        drv->sector_count = ((uint64_t)id[61] << 16) | id[60];
    }

    drv->present    = 1;
    drv->controller = ctrl_idx;
    drv->slave      = slave;
    return 1;
}

/* ── Init: scan all 4 positions ─────────────────────────────── */
void ata_init(void) {
    ata_drive_count = 0;
    kser_puts("ATA: init scanning\n");
    for (int c = 0; c < 2; c++) {
        for (int s = 0; s < 2; s++) {
            AtaDrive *d = &ata_drives[ata_drive_count];
            kmemset(d, 0, sizeof(AtaDrive));
            if (ata_identify(c, s, d)) {
                ata_drive_count++;
                kser_puts("ATA: found drive ["); kser_puts(d->model); kser_puts("] sectors="); 
                char _sb[20]; int _si=0; uint64_t _sv=d->sector_count;
                if(!_sv){_sb[_si++]='0';}else{char _st[20];int _sti=0;while(_sv){_st[_sti++]=(char)('0'+_sv%10);_sv/=10;}for(int _x=_sti-1;_x>=0;_x--)_sb[_si++]=_st[_x];}
                _sb[_si]=0; kser_puts(_sb); kser_puts("\n");
                fb_puts("ATA: found "); fb_puts(d->model); fb_puts("\n");
            }
        }
    }
    if (ata_drive_count == 0) { fb_puts("ATA: no drives found\n"); kser_puts("ATA: no drives found\n"); }
    else { char _dc[4]; _dc[0]=(char)('0'+ata_drive_count); _dc[1]=0; kser_puts("ATA: total drives="); kser_puts(_dc); kser_puts("\n"); }
}

/* ── Read sectors (LBA48 always for simplicity) ─────────────── */
int ata_read_sectors(int drive_idx, uint64_t lba,
                     uint32_t count, void *buf) {
    if (drive_idx < 0 || drive_idx >= ata_drive_count) return -1;
    if (!count || count > 256) return -1;
    AtaDrive *drv = &ata_drives[drive_idx];

    /* Reject out-of-range LBAs. A bad LCN sign-extended into a huge LBA
     * otherwise reaches the bus and looks like a device hang. */
    if (lba > drv->sector_count || count > drv->sector_count - lba) {
        kser_puts("ATA read out of range lba=");
        kser_dec(lba);
        kser_puts(" count=");
        kser_dec(count);
        kser_puts(" total=");
        kser_dec(drv->sector_count);
        kser_puts("\n");
        return -1;
    }

    uint16_t base = ata_base[drv->controller];
    uint16_t ctrl = ata_ctrl[drv->controller];
    uint16_t *dst = (uint16_t *)buf;

    /* Select drive with LBA mode bit */
    outb(base + ATA_REG_HDDEVSEL,
         (uint8_t)(drv->slave ? 0x50 : 0x40));
    ata_delay(ctrl);

    /* LBA48: send high bytes first */
    outb(base + ATA_REG_SECCOUNT0, (uint8_t)((count >> 8) & 0xFF));
    outb(base + ATA_REG_LBA0,      (uint8_t)((lba >> 24) & 0xFF));
    outb(base + ATA_REG_LBA1,      (uint8_t)((lba >> 32) & 0xFF));
    outb(base + ATA_REG_LBA2,      (uint8_t)((lba >> 40) & 0xFF));
    /* Then low bytes */
    outb(base + ATA_REG_SECCOUNT0, (uint8_t)(count & 0xFF));
    outb(base + ATA_REG_LBA0,      (uint8_t)(lba & 0xFF));
    outb(base + ATA_REG_LBA1,      (uint8_t)((lba >> 8) & 0xFF));
    outb(base + ATA_REG_LBA2,      (uint8_t)((lba >> 16) & 0xFF));

    outb(base + ATA_REG_COMMAND, ATA_CMD_READ_PIO48);

    for (uint32_t s = 0; s < count; s++) {
        ata_wait(base, ctrl);
        if (ata_wait_drq(base) < 0) return -1;
        for (int w = 0; w < 256; w++)
            dst[s * 256 + w] = inw(base + ATA_REG_DATA);
    }
    return 0;
}

/* ── Write sectors (LBA48) ──────────────────────────────────── */
int ata_write_sectors(int drive_idx, uint64_t lba,
                      uint32_t count, const void *buf) {
    if (drive_idx < 0 || drive_idx >= ata_drive_count) return -1;
    if (!count || count > 256) return -1;
    AtaDrive *drv = &ata_drives[drive_idx];

    if (lba > drv->sector_count || count > drv->sector_count - lba) {
        kser_puts("ATA write out of range lba=");
        kser_dec(lba);
        kser_puts(" count=");
        kser_dec(count);
        kser_puts(" total=");
        kser_dec(drv->sector_count);
        kser_puts("\n");
        return -1;
    }

    uint16_t base = ata_base[drv->controller];
    uint16_t ctrl = ata_ctrl[drv->controller];
    const uint16_t *src = (const uint16_t *)buf;

    outb(base + ATA_REG_HDDEVSEL,
         (uint8_t)(drv->slave ? 0x50 : 0x40));
    ata_delay(ctrl);

    outb(base + ATA_REG_SECCOUNT0, (uint8_t)((count >> 8) & 0xFF));
    outb(base + ATA_REG_LBA0,      (uint8_t)((lba >> 24) & 0xFF));
    outb(base + ATA_REG_LBA1,      (uint8_t)((lba >> 32) & 0xFF));
    outb(base + ATA_REG_LBA2,      (uint8_t)((lba >> 40) & 0xFF));
    outb(base + ATA_REG_SECCOUNT0, (uint8_t)(count & 0xFF));
    outb(base + ATA_REG_LBA0,      (uint8_t)(lba & 0xFF));
    outb(base + ATA_REG_LBA1,      (uint8_t)((lba >> 8) & 0xFF));
    outb(base + ATA_REG_LBA2,      (uint8_t)((lba >> 16) & 0xFF));

    outb(base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO48);

    for (uint32_t s = 0; s < count; s++) {
        ata_wait(base, ctrl);
        if (ata_wait_drq(base) < 0) return -1;
        for (int w = 0; w < 256; w++)
            outw(base + ATA_REG_DATA, src[s * 256 + w]);
    }

    /* Flush the drive cache once per batch. */
    outb(base + ATA_REG_COMMAND, 0xE7);
    ata_wait(base, ctrl);
    return 0;
}

/* ── Legacy stubs (used by ntfs.c / vfs.c via device string) ── */
/* These map a "/dev/sdX" string to an ATA drive index          */
int ata_open(const char *dev) {
    /* "/dev/sda" = drive 0, "/dev/sdb" = drive 1, etc. */
    if (!dev || dev[0] != '/') return -1;
    /* Find last char */
    int len = 0;
    while (dev[len]) len++;
    if (len < 1) return -1;
    char last = dev[len-1];
    int idx = last - 'a';
    if (idx < 0 || idx >= ata_drive_count) return -1;
    return ata_drives[idx].present ? idx : -1;
}

int64_t ata_read(int fd, uint64_t lba, void *buf, uint32_t sectors) {
    return ata_read_sectors(fd, lba, sectors, buf) == 0
           ? (int64_t)sectors : -1;
}
