/*
 * Notux OS — Kernel Linker Script
 * Single PT_LOAD covers everything — simpler and more portable.
 * The RWX warning is acceptable for now (kernel owns all memory anyway).
 */
ENTRY(_start)

SECTIONS {
    . = 0x0000000000100000;

    .text.entry : { KEEP(*(.text.entry)) }
    .text       : { *(.text .text.*) }
    .rodata     : { *(.rodata .rodata.*) }
    .data       : { *(.data .data.*) }

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    __kernel_end = .;

    /DISCARD/ : {
        *(.note.GNU-stack)
        *(.eh_frame)
        *(.comment)
    }
}
