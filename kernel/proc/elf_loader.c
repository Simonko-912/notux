/*
 * Notux OS — ELF Loader for User Processes
 * kernel/proc/elf_loader.c
 *
 * Loads PT_LOAD segments of a 64-bit ELF file into a process
 * address space (text at 0x8000000000, grows upward).
 */

#include "process.h"
#include "../fs/vfs.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"
#include "../kserial.h"
#include "../kernel.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#define ELF_MAGIC       0x464C457Fu  /* 0x7F 'E' 'L' 'F' */
#define EM_X86_64       62
#define PT_LOAD         1

static void elf_log(const char *msg) {
    kser_puts("elf: "); kser_puts(msg); kser_puts("\n");
}

typedef struct {
    uint32_t e_ident_magic;
    uint8_t  e_ident_class;   /* 2 = 64-bit */
    uint8_t  e_ident_data;    /* 1 = little-endian */
    uint8_t  e_ident_version;
    uint8_t  e_ident_osabi;
    uint8_t  e_ident_pad[8];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

/* vfs_read legitimately returns short (a runlist boundary or a partial
 * cluster). Loop until the full request is satisfied or the file ends. */
static int64_t read_exact(int fd, void *buf, uint64_t n) {
    uint8_t *p = (uint8_t *)buf;
    uint64_t done = 0;
    while (done < n) {
        int64_t r = vfs_read(fd, p + done, n - done);
        if (r < 0) return r;          /* propagate -EIO verbatim */
        if (r == 0) return -EIO;      /* EOF before n bytes */
        done += (uint64_t)r;
    }
    return (int64_t)done;
}

int proc_load_elf(Process *p, const char *path, uint64_t *entry_out) {
    if (!p || !p->page_table || !path || !entry_out) return -EINVAL;

    char dbg[24];
    int fd = vfs_open_compat(path, VFS_O_RDONLY, 0);
    if (fd < 0) { elf_log("open fail"); return -ENOENT; }

    Elf64_Ehdr ehdr;
    int64_t hr = read_exact(fd, &ehdr, sizeof(ehdr));
    if (hr < 0) {
        if (hr == -EIO) elf_log("hdr read err");
        else            elf_log("hdr short read");
        vfs_close(fd);
        return -EIO;
    }

    if (ehdr.e_ident_magic != ELF_MAGIC ||
        ehdr.e_ident_class != 2 ||
        ehdr.e_machine != EM_X86_64) {
        elf_log("bad magic/sane");
        vfs_close(fd);
        return -ENOEXEC;
    }

    int loaded_any = 0;

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr ph;
        if (vfs_seek(fd, (int64_t)(ehdr.e_phoff + (uint64_t)i * ehdr.e_phentsize), 0) < 0)
            continue;
        if (read_exact(fd, &ph, sizeof(ph)) != (int64_t)sizeof(ph))
            continue;
        if (ph.p_type != PT_LOAD) continue;
        if (ph.p_memsz == 0) continue;
        if (ph.p_filesz > ph.p_memsz) {
            elf_log("filesz>memsz");
            vfs_close(fd);
            return -ENOEXEC;
        }
        if ((ph.p_vaddr & 0xFFFULL) != (ph.p_offset & 0xFFFULL)) {
            elf_log("p_offset/vaddr misaligned");
            vfs_close(fd);
            return -ENOEXEC;
        }

        uint64_t map_start = ph.p_vaddr & ~0xFFFULL;
        uint64_t map_end   = (ph.p_vaddr + ph.p_memsz + 0xFFF) & ~0xFFFULL;
        if (map_end <= map_start) continue;
        uint64_t pages = (map_end - map_start) / 4096;

        uint64_t phys = pmm_alloc_pages(pages);
        if (phys == PMM_OOM) {
            elf_log("OOM pages");
            vfs_close(fd);
            return -ENOMEM;
        }
        p->mem_pages += pages;

        uint8_t *seg = (uint8_t *)(uintptr_t)phys;
        kmemset(seg, 0, pages * 4096);

        if (ph.p_filesz > 0) {
            uint8_t *dst = seg + (ph.p_vaddr - map_start);
            uint64_t left = ph.p_filesz;
            if (vfs_seek(fd, (int64_t)ph.p_offset, 0) < 0) {
                elf_log("seek seg fail");
                vfs_close(fd);
                return -EIO;
            }
            while (left > 0) {
                int64_t r = vfs_read(fd, dst, left);
                if (r <= 0) {
                    kser_puts("elf: read fail left=");
                    num_to_str(left, dbg, 10); kser_puts(dbg);
                    kser_puts("\n");
                    vfs_close(fd);
                    return -EIO;
                }
                dst  += r;
                left -= (uint64_t)r;
            }
        }

        if (vmm_map_range(p->page_table, map_start, phys, pages,
                          VMM_FLAG_RW | VMM_FLAG_USER) < 0) {
            elf_log("map fail");
            vfs_close(fd);
            return -ENOMEM;
        }
        loaded_any = 1;
    }

    vfs_close(fd);

    if (!loaded_any) {
        elf_log("no PT_LOAD");
        return -ENOEXEC;
    }
    *entry_out = ehdr.e_entry;
    return 0;
}