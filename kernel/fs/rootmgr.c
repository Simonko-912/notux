/*
 * Notux OS — Root Partition Manager
 * kernel/fs/rootmgr.c
 *
 * Runs at kernel boot (before scheduler) to find or set up the root
 * NTFS partition.  Uses polled PS/2 keyboard so it needs no IRQs.
 *
 * Config file: notux.cfg in the partition root.
 * Format (minimal):  root=1\n   (just marks the partition as Notux root)
 *
 * Flow:
 *   1. Scan GPT on all ATA drives
 *   2. For each NTFS partition: read LBA 0 of partition and check NTFS boot sig
 *   3. Then try to find "notux.cfg" via the NTFS driver
 *   4. If exactly one partition has it → mount as #/ automatically
 *   5. If multiple → show list, user picks
 *   6. If none → show list of NTFS partitions, user picks
 *   7. After picking: if partition has no #/bin → run install_template()
 */
#include "rootmgr.h"
#include "vfs.h"
#include "ntfs/ntfs.h"
#include "../drivers/disk/ata.h"
#include "../drivers/disk/gpt.h"
#include "../drivers/gfx/font.h"
#include "../kernel.h"
#include "../kserial.h"
#include <stdint.h>
#include <stddef.h>


/* ── Config file name on root partition ─────────────────────── */
#define NOTUX_CFG "notux.cfg"
#define NOTUX_MARKER "root=1"

/* ── Result ─────────────────────────────────────────────────── */
static int g_drive = -1;
static int g_part  = -1;

int rootmgr_drive_idx(void) { return g_drive; }
int rootmgr_part_idx(void)  { return g_part;  }

/* ── Polled PS/2 keyboard (no IRQs needed) ──────────────────── */
static inline uint8_t kbd_inb(void) {
    uint8_t v; __asm__ volatile("inb $0x60,%0":"=a"(v)); return v;
}
static inline uint8_t kbd_status(void) {
    uint8_t v; __asm__ volatile("inb $0x64,%0":"=a"(v)); return v;
}

/* Scancode → ASCII (US layout, no shift) */
static const char sc_map[128] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0,
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,'7','8','9','-','4','5','6','+','1','2','3','0','.',
};

static char kbd_poll_char(void) {
    /* Block until a key is pressed and released */
    for (;;) {
        while (!(kbd_status() & 1)) {} /* wait for data */
        uint8_t sc = kbd_inb();
        if (sc & 0x80) continue;       /* key release, skip */
        if (sc < 128 && sc_map[sc])
            return sc_map[sc];
    }
}

/* Read a line of input (echo to screen) */
static int read_line(char *buf, int max) {
    int n = 0;
    for (;;) {
        char c = kbd_poll_char();
        if (c == '\n' || c == '\r') {
            buf[n] = '\0';
            char nl[2] = {'\n', 0};
            fb_puts(nl);
            return n;
        }
        if ((c == '\b' || c == 127) && n > 0) {
            n--;
            fb_putc('\b');
            continue;
        }
        if (c >= 0x20 && n < max - 1) {
            buf[n++] = c;
            fb_putc(c);
        }
    }
}

/* ── Size formatting ─────────────────────────────────────────── */
static void print_size(uint64_t sectors) {
    uint64_t mb = sectors / 2048;
    char buf[32];
    num_to_str(mb, buf, 10);
    fb_puts(buf);
    fb_puts(" MiB");
}

/* ── NTFS signature check ────────────────────────────────────── */
static int is_ntfs_boot(int drive, uint64_t start_lba) {
    uint8_t sector[512];
    kser_puts("ntfs_check: drive="); kser_putc((char)('0'+drive));
    kser_puts(" lba="); {char _b[16];int _i=0;uint64_t _v=start_lba;if(!_v){_b[_i++]='0';}else{char _t[16];int _ti=0;while(_v){_t[_ti++]=(char)('0'+_v%10);_v/=10;}for(int _x=_ti-1;_x>=0;_x--)_b[_i++]=_t[_x];}_b[_i]=0;kser_puts(_b);}
    kser_puts("\n");
    if (ata_read_sectors(drive, start_lba, 1, sector) < 0) { kser_puts("  read FAIL\n"); return 0; }
    kser_puts("  oem=["); kser_putc(sector[3]);kser_putc(sector[4]);kser_putc(sector[5]);kser_putc(sector[6]); kser_puts("]\n");
    return (sector[3]=='N' && sector[4]=='T' && sector[5]=='F' &&
            sector[6]=='S' && sector[7]==' ');
}

/* ── Try to mount a partition and check for notux.cfg ─────────── */
/*
 * We build a device string like "ata:0:5" (drive 0, start_lba 5) and
 * pass it to vfs_mount.  The NTFS driver understands this format.
 */
static char dev_str[32];

static void make_dev_str(int drive, uint64_t lba) {
    /* "ata:D:LLLLLLLLLLLLLLLL" */
    int i = 0;
    dev_str[i++] = 'a'; dev_str[i++] = 't'; dev_str[i++] = 'a';
    dev_str[i++] = ':';
    dev_str[i++] = (char)('0' + drive);
    dev_str[i++] = ':';
    /* Write LBA decimal */
    char tmp[24]; int ti = 0;
    uint64_t v = lba;
    if (!v) { tmp[ti++] = '0'; }
    else { while (v) { tmp[ti++] = (char)('0' + v % 10); v /= 10; } }
    /* reverse */
    for (int a = 0, b = ti-1; a < b; a++, b--) {
        char t = tmp[a]; tmp[a] = tmp[b]; tmp[b] = t;
    }
    for (int j = 0; j < ti; j++) dev_str[i++] = tmp[j];
    dev_str[i] = '\0';
}

static int check_notux_cfg(int drive, uint64_t start_lba) {
    make_dev_str(drive, start_lba);
    /* Try mount */
    int r = vfs_mount(dev_str, "#/", "ntfs");
    if (r < 0) return 0;

    /* Check for notux.cfg */
    VfsFileInfo fi;
    int found = (vfs_stat("#/notux.cfg", &fi) == 0);

    if (!found) vfs_umount("#/");
    return found;
}

/* ── Print directory listing (up to 16 entries) ──────────────── */
static void show_partition_files(int drive, uint64_t start_lba) {
    make_dev_str(drive, start_lba);
    if (vfs_mount(dev_str, "#/tmp_peek", "ntfs") < 0) {
        fb_puts("  (could not read partition)\n");
        return;
    }
    VfsDir *d = vfs_opendir("#/tmp_peek");
    if (!d) { vfs_umount("#/tmp_peek"); fb_puts("  (empty)\n"); return; }

    VfsDirent entry;
    int count = 0;
    fb_puts("  Files/dirs: ");
    while (count < 12 && vfs_readdir(d, &entry) == 0) {
        fb_puts(entry.name);
        if (entry.flags & VFS_DIR) fb_puts("/");
        fb_puts("  ");
        count++;
    }
    vfs_closedir(d);
    vfs_umount("#/tmp_peek");
    if (!count) fb_puts("(empty)");
    fb_putc('\n');
}

/* ── Install template onto a freshly selected partition ─────── */
static void install_template(int drive, uint64_t start_lba) {
    fb_set_color(0xA6E3A1, 0x1E1E2E); /* green */
    fb_puts("\nInstalling Notux on partition...\n");
    fb_set_color(0xCDD6F4, 0x1E1E2E);

    make_dev_str(drive, start_lba);
    if (vfs_mount(dev_str, "#/", "ntfs") < 0) {
        fb_puts("ERROR: could not mount partition for install\n");
        kser_puts("rootmgr: install mount FAILED\n");
        return;
    }
    kser_puts("rootmgr: install mount ok\n");

    /* Create directory tree */
    const char *dirs[] = {
        "#/bin", "#/etc", "#/usr", "#/svc",
        "#/var", "#/var/cache", "#/var/cache/opm",
        "#/var/log", "#/tmp",
        NULL
    };
    for (int i = 0; dirs[i]; i++) {
        kser_puts("rootmgr: mkdir "); kser_puts(dirs[i]); kser_puts("\n");
        vfs_mkdir(dirs[i]);
        kser_puts("rootmgr: mkdir done\n");
        fb_puts("  mkdir ");
        fb_puts(dirs[i]);
        fb_putc('\n');
    }

    /* Write notux.cfg marker */
    kser_puts("rootmgr: open notux.cfg\n");
    int fd = vfs_open("#/notux.cfg",
                      VFS_O_WRONLY | VFS_O_CREATE | VFS_O_TRUNC, 0);
    kser_puts("rootmgr: notux.cfg fd="); kser_dec(fd); kser_puts("\n");
    if (fd >= 0) {
        vfs_write(fd, "root=1\n", 7);
        vfs_write(fd, "version=0.1\n", 12);
        kser_puts("rootmgr: notux.cfg written\n");
        vfs_close(fd);
        kser_puts("rootmgr: notux.cfg closed\n");
        fb_puts("  created notux.cfg\n");
    }

    /* Write default admin password */
    kser_puts("rootmgr: open admin.pass\n");
    fd = vfs_open("#/etc/admin.pass",
                  VFS_O_WRONLY | VFS_O_CREATE | VFS_O_TRUNC, 0);
    kser_puts("rootmgr: admin.pass fd="); kser_dec(fd); kser_puts("\n");
    if (fd >= 0) {
        vfs_write(fd, "admin\n", 6);
        vfs_close(fd);
        kser_puts("rootmgr: admin.pass closed\n");
        fb_puts("  created admin.pass (default: 'admin')\n");
    }

    /* Write repos.conf */
    kser_puts("rootmgr: mkdir /etc/opm\n");
    vfs_mkdir("#/etc/opm");
    kser_puts("rootmgr: open repos.conf\n");
    fd = vfs_open("#/etc/opm/repos.conf",
                  VFS_O_WRONLY | VFS_O_CREATE | VFS_O_TRUNC, 0);
    kser_puts("rootmgr: repos fd="); kser_dec(fd); kser_puts("\n");
    if (fd >= 0) {
        const char *repo =
            "https://raw.githubusercontent.com/Simonko-912/notux-packages/main\n";
        kser_puts("rootmgr: repos write\n");
        vfs_write(fd, repo, kstrlen(repo));
        kser_puts("rootmgr: repos close\n");
        vfs_close(fd);
        kser_puts("rootmgr: repos closed\n");
        fb_puts("  created repos.conf\n");
    }

    /* Write welcome message */
    kser_puts("rootmgr: open motd\n");
    fd = vfs_open("#/etc/motd",
                  VFS_O_WRONLY | VFS_O_CREATE | VFS_O_TRUNC, 0);
    kser_puts("rootmgr: motd fd="); kser_dec(fd); kser_puts("\n");
    if (fd >= 0) {
        const char *motd =
            "Welcome to Notux 0.1!\n"
            "Type 'nfetch' for system info, 'dir' to list files.\n"
            "Admin password: admin  (change with: sudo usr newpass admin)\n";
        vfs_write(fd, motd, kstrlen(motd));
        vfs_close(fd);
        fb_puts("  created motd\n");
    }

    fb_set_color(0xA6E3A1, 0x1E1E2E);
    fb_puts("Installation complete!\n\n"); kser_puts("rootmgr: install done\n");
    fb_set_color(0xCDD6F4, 0x1E1E2E);
}

/* ── Interactive partition selection ─────────────────────────── */
static int pick_partition(DiskPartition *ntfs_parts[], int count) {
    fb_set_color(0xFFD700, 0x1E1E2E); /* gold header */
    fb_puts("\n╔══════════════════════════════════════════════════╗\n");
    fb_puts("║       NOTUX — Select Root Partition              ║\n");
    fb_puts("╚══════════════════════════════════════════════════╝\n\n");
    fb_set_color(0xCDD6F4, 0x1E1E2E);
    fb_puts("Available NTFS partitions:\n\n");

    for (int i = 0; i < count; i++) {
        DiskPartition *p = ntfs_parts[i];
        fb_set_color(0x89DCEB, 0x1E1E2E); /* cyan number */
        char num[4] = {'[', (char)('1' + i), ']', ' '};
        fb_puts(num);
        fb_set_color(0xCDD6F4, 0x1E1E2E);

        fb_puts("Drive ");
        fb_putc((char)('0' + p->drive_idx));
        fb_puts("  Part ");
        char pnum[8]; num_to_str((uint64_t)p->part_idx + 1, pnum, 10);
        fb_puts(pnum);
        fb_puts("  ");
        print_size(p->size_sectors);
        fb_puts("  ");
        if (p->name[0]) {
            fb_puts("\""); fb_puts(p->name); fb_puts("\"");
        }
        fb_putc('\n');

        /* Show a few files so user can confirm */
        show_partition_files(p->drive_idx, p->start_lba);
        fb_putc('\n');
    }

    char buf[8];
    for (;;) {
        fb_set_color(0x89DCEB, 0x1E1E2E);
        fb_puts("Enter number (1-");
        char maxbuf[4]; num_to_str((uint64_t)count, maxbuf, 10);
        fb_puts(maxbuf);
        fb_puts("): ");
        fb_set_color(0xCDD6F4, 0x1E1E2E);

        read_line(buf, sizeof(buf));
        int choice = 0;
        for (int i = 0; buf[i] >= '0' && buf[i] <= '9'; i++)
            choice = choice * 10 + (buf[i] - '0');

        if (choice >= 1 && choice <= count) return choice - 1;
        fb_puts("Invalid. Try again.\n");
    }
}

/* ── Main entry point ────────────────────────────────────────── */
void rootmgr_init(void) {
    fb_puts("Scanning disks...\n"); kser_puts("rootmgr: scanning disks\n");
    ata_init();
    gpt_scan_all();
    for(int _gi=0;_gi<disk_partition_count;_gi++){
        kser_puts("  part["); kser_putc((char)('0'+_gi)); kser_puts("]: ntfs=");
        kser_putc(disk_partitions[_gi].is_ntfs?'1':'0'); kser_puts(" name=");
        kser_puts(disk_partitions[_gi].name); kser_puts("\n");
    }

    if (disk_partition_count == 0) {
        fb_set_color(0xF38BA8, 0x1E1E2E);
        fb_puts("No partitions found. Running in RAM-only mode.\n"); kser_puts("rootmgr: no partitions\n");
        fb_set_color(0xCDD6F4, 0x1E1E2E);
        return;
    }

    /* ── Step 1: look for partitions with notux.cfg ──────────── */
    DiskPartition *found[GPT_MAX_PARTITIONS * ATA_MAX_DRIVES];
    int found_count = 0;

    for (int i = 0; i < disk_partition_count; i++) {
        DiskPartition *p = &disk_partitions[i];
        if (!p->is_ntfs) continue;
        if (!is_ntfs_boot(p->drive_idx, p->start_lba)) continue;

        if (check_notux_cfg(p->drive_idx, p->start_lba)) {
            fb_puts("Found Notux root on drive ");
            fb_putc((char)('0' + p->drive_idx));
            fb_puts(" partition ");
            char pn[8]; num_to_str((uint64_t)p->part_idx+1, pn, 10);
            fb_puts(pn);
            fb_putc('\n');
            found[found_count++] = p;
        }
    }

    /* ── Step 2: auto-mount if exactly one match ─────────────── */
    if (found_count == 1) {
        /* Already mounted by check_notux_cfg */
        g_drive = found[0]->drive_idx;
        g_part  = (int)(found[0] - disk_partitions);
        fb_set_color(0xA6E3A1, 0x1E1E2E);
        fb_puts("Root mounted OK\n"); kser_puts("rootmgr: root mounted OK\n");
        fb_set_color(0xCDD6F4, 0x1E1E2E);
        return;
    }

    /* Multiple found: ask user to pick */
    if (found_count > 1) {
        fb_set_color(0xFFD700, 0x1E1E2E);
        fb_puts("Multiple Notux partitions found. Please choose:\n");
        fb_set_color(0xCDD6F4, 0x1E1E2E);
        int choice = pick_partition(found, found_count);
        DiskPartition *p = found[choice];
        make_dev_str(p->drive_idx, p->start_lba);
        vfs_mount(dev_str, "#/", "ntfs");
        g_drive = p->drive_idx;
        g_part  = (int)(p - disk_partitions);
        return;
    }

    /* ── Step 3: no notux.cfg anywhere — collect NTFS partitions */
    DiskPartition *ntfs_list[GPT_MAX_PARTITIONS * ATA_MAX_DRIVES];
    int ntfs_count = 0;

    for (int i = 0; i < disk_partition_count; i++) {
        DiskPartition *p = &disk_partitions[i];
        if (!p->is_ntfs) continue;
        if (!is_ntfs_boot(p->drive_idx, p->start_lba)) continue;
        ntfs_list[ntfs_count++] = p;
    }

    if (ntfs_count == 0) {
        fb_set_color(0xF38BA8, 0x1E1E2E);
        fb_puts("No NTFS partitions found.\n"); kser_puts("rootmgr: no NTFS\n");
        fb_puts("Create an NTFS partition and reboot.\n");
        fb_set_color(0xCDD6F4, 0x1E1E2E);
        return;
    }

    /* Auto-pick if only one NTFS partition */
    int chosen = 0;
    if (ntfs_count > 1) {
        fb_set_color(0xFFD700, 0x1E1E2E);
        fb_puts("No Notux installation found.\n");
        fb_set_color(0xCDD6F4, 0x1E1E2E);
        chosen = pick_partition(ntfs_list, ntfs_count);
    } else {
        fb_set_color(0xFFD700, 0x1E1E2E);
        fb_puts("No Notux installation found.\n");
        fb_puts("Auto-selecting the only NTFS partition.\n\n");
        fb_set_color(0xCDD6F4, 0x1E1E2E);
        kser_puts("rootmgr: auto-selecting single NTFS partition\n");
        show_partition_files(ntfs_list[0]->drive_idx,
                             ntfs_list[0]->start_lba);
        /* Single partition: auto-confirm after showing contents */
        fb_puts("\nInstalling Notux on this partition...\n");
        fb_puts("(Press any key during setup to choose a different partition.)\n");
        /* Brief delay then auto-proceed if no key */
        int got_key = 0;
        while(kbd_status() & 1) { kbd_inb(); }   /* drain stale buffer */
        for(int t=0; t<30000000 && !got_key; t++) {
            if(kbd_status() & 1) { kbd_inb(); got_key=1; }
        }
        if(got_key) {
            fb_puts("Cancelled. Running in RAM-only mode.\n");
            kser_puts("rootmgr: cancelled -> RAM-only mode\n");
            return;
        }
    }

    DiskPartition *sel = ntfs_list[chosen];
    g_drive = sel->drive_idx;
    g_part  = (int)(sel - disk_partitions);

    /* Install template onto new partition */
    install_template(sel->drive_idx, sel->start_lba);
}
