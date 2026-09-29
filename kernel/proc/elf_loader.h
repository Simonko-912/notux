#pragma once
#include <stdint.h>

/* Forward declaration of Process struct */
struct Process;
typedef struct Process Process;

/*
 * Load an ELF binary into a process's address space
 *
 * @param p: Process to load the ELF into
 * @param path: Path to the ELF file
 * @param entry_out: Pointer to store the entry point address
 * @return: 0 on success, -1 on failure
 */
int proc_load_elf(Process *p, const char *path, uint64_t *entry_out);