#pragma once
#include <stdint.h>
void gdt_init(void);
void gdt_set_kernel_stack(uint64_t rsp0);
void gdt_setup_stacks(void);
uint64_t gdt_tss_debug_rsp0(void);