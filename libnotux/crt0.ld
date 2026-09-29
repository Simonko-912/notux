OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x0000008000000000;

    .text : {
        *(.text*)
    }

    . = ALIGN(0x1000);
    .rodata : {
        *(.rodata*)
    }

    . = ALIGN(0x1000);
    .data : {
        *(.data*)
    }

    . = ALIGN(0x1000);
    __bss_start = .;
    .bss : {
        *(.bss*)
        *(COMMON)
    }
    __bss_end = .;

    . = ALIGN(0x1000);
    __init_array_start = .;
    .init_array : {
        *(.init_array*)
    }
    __init_array_end = .;

    __fini_array_start = .;
    .fini_array : {
        *(.fini_array*)
    }
    __fini_array_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}