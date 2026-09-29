#pragma once
#include <stdint.h>
/*
 * Root partition manager.
 * Call rootmgr_init() from kmain before starting the scheduler.
 * It will:
 *   1. Scan all NTFS partitions
 *   2. Look for notux.cfg on each
 *   3. If found → mount that partition as #/
 *   4. If not found → interactive selection UI
 *   5. If new partition selected with no Notux files → install template
 */
void rootmgr_init(void);

/* Returns the drive/partition that was selected as root (after rootmgr_init) */
int  rootmgr_drive_idx(void);
int  rootmgr_part_idx(void);    /* index into disk_partitions[] */
