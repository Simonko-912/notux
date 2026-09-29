BITS 64

global _start
extern kmain

section .text.entry

_start:
    cli

    ; SysV ABI:
    ; before CALL, RSP must be 8 mod 16
    sub rsp, 8

    ; RDI already contains BootInfo*
    call kmain

.hang:
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
