BITS 64

global _start
extern kmain

section .text.entry

_start:
    cli

    ; RDI already contains BootInfo*. Switch onto the stack the loader
    ; allocated for us before doing anything else: the firmware's own
    ; loader stack is still live underneath us, and the PMM is about to
    ; recycle that memory. A same-CPL exception or interrupt pushes onto
    ; the *current* stack, so leaving rsp on firmware memory is fatal.
    mov  rax, [rdi + 112]         ; bi->kstack_phys
    test rax, rax
    jz   .fw_stack
    mov  rcx, [rdi + 120]         ; bi->kstack_bytes
    add  rax, rcx                 ; stacks grow down: start at the top
    and  rax, -16                 ; RSP must be 16-aligned at the CALL
    mov  rsp, rax
    jmp  .call
.fw_stack:
    ; SysV ABI:
    ; before CALL, RSP must be 8 mod 16
    sub rsp, 8
.call:
    call kmain

.hang:
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
