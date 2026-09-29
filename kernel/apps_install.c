/* Notux OS — first-boot user-application installer.
 * kernel/apps_install.c
 *
 * Writes the embedded binary payload (apps_blob.asm -> incbin of
 * build/apps_blob.bin) into #/bin on the root NTFS partition, so
 * user-space programs exist without host-side loop mounts.
 *
 * A file is only written when missing or when its size differs
 * from the payload, so user-edited files are never clobbered.
 */

#include "fs/vfs.h"
#include "kserial.h"
#include "drivers/gfx/font.h"
#include "kernel.h"
#include <stdint.h>
#include <stddef.h>

extern const char apps_blob_start[];
extern const char apps_blob_end[];

int apps_install_blob(void) {
    const char *p   = apps_blob_start;
    const char *end = apps_blob_end;
    char dbg[16];

    int mkdir_rc = vfs_mkdir("#/bin");
    if (mkdir_rc < 0) mkdir_rc = -mkdir_rc;

    int installed = 0;
    int skipped   = 0;
    int failed    = 0;

    while (p + 8 <= end) {
        uint32_t name_len = *(const uint32_t *)p;
        uint32_t data_len = *(const uint32_t *)(p + 4);
        p += 8;
        if (name_len == 0 && data_len == 0) break;   /* terminator  */
        if ((ptrdiff_t)(end - p) < (ptrdiff_t)(name_len + data_len)) break;

        char name[128];
        if (name_len >= sizeof(name)) name_len = sizeof(name) - 1;
        for (uint32_t i = 0; i < name_len; i++) name[i] = p[i];
        name[name_len] = '\0';
        const char *data = p + name_len;

        VfsFileInfo fi;
        char path[160];
        kstrncpy(path, "#/bin/", sizeof(path) - 1);
        kstrncpy(path + 6, name, sizeof(path) - 7);

        int st = vfs_stat(path, &fi);
        if (st == 0 && fi.size == data_len) {
            skipped++;
            p = data + data_len;
            continue;
        }

        int fd = vfs_open(path, VFS_O_WRONLY | VFS_O_CREATE | VFS_O_TRUNC, 0);
        if (fd >= 0) {
            int64_t wr = vfs_write(fd, data, data_len);
            vfs_close(fd);
            if (wr == (int64_t)data_len) {
                kser_puts(".");
                installed++;
            } else {
                kser_puts("w");
                int ec = wr < 0 ? -wr : (int)(unsigned int)(uint64_t)data_len - wr;
                num_to_str((uint64_t)(unsigned int)ec, dbg, 10);
                kser_puts(dbg);
                kser_puts(" ");
                failed++;
            }
        } else {
            kser_puts("x");
            int ec = fd < 0 ? -fd : fd;
            num_to_str((uint64_t)(unsigned int)ec, dbg, 10);
            kser_puts(dbg);
            kser_puts(" ");
            failed++;
        }

        p = data + data_len;
    }

    kser_puts("apps: mkdir=");
    num_to_str((uint64_t)mkdir_rc, dbg, 10); kser_puts(dbg);
    kser_puts(" installed="); num_to_str((uint64_t)installed, dbg, 10); kser_puts(dbg);
    kser_puts(" skipped="); num_to_str((uint64_t)skipped, dbg, 10); kser_puts(dbg);
    kser_puts(" failed="); num_to_str((uint64_t)failed, dbg, 10); kser_puts(dbg);
    kser_puts("\n");
    return 0;
}
