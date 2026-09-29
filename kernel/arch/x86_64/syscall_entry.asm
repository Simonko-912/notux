; Notux OS — SYSCALL entry trampoline
; kernel/arch/x86_64/syscall_entry.asm
;
; Ring-3 → ring-0 via the SYSCALL instruction.  On entry (hardware):
;   rcx = user return RIP, r11 = user RFLAGS, stack = USER stack.
; swapgs exchanges GS_BASE with KERNEL_GS_BASE so that:
;   [gs:0] = current kernel stack top
;   [gs:8] = slot for the saved user RSP
;
; The handler runs with interrupts masked (SFMASK clears IF at SYSCALL).

bits 64
section .text
global syscall_entry_asm
extern syscall_handler
extern g_user_exit

syscall_entry_asm:
    swapgs
    mov  [gs:8],  rsp        ; save user rsp
    mov  [gs:16], rcx        ; save user RIP for the return path
    mov  [gs:24], r11        ; save user flags for the return path
    mov  rsp,     [gs:0]     ; switch to kernel stack

    ; On some QEMU/OVMF configurations SYSRET can return with unexpected
    ; selector state. Use an explicit IRET frame for the ring-3 return path.
    push r11                 ; saved rflags
    push rcx                 ; saved rip
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rax                 ; save syscall number for diag in handler

    ; Args for syscall_handler(nr, a1, a2, a3, a4, a5):
    ;   user regs: rax=nr rdi=a1 rsi=a2 rdx=a3 r10=a4 r8=a5
    ;   SysV:      rdi     rsi     rdx     rcx  r8   r9
    mov  rcx, r10
    mov  r8,  r8             ; keep syscall a5 in r8
    xor  r9d, r9d            ; a6 is unused by libnotux nx_syscall wrapper
    push qword [gs:8]        ; stack arg 8: saved user rsp
    mov  [gs:32], rcx        ; keep RIP in scratch area
    push qword [gs:32]       ; stack arg 7: saved RIP after SYSCALL

    call syscall_handler     ; result in rax

    cmp  byte [rel g_user_exit], 0
    jne  .exit_path

    add  rsp, 24
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    pop  rcx
    pop  r11
    ; Build a clean ring-3 IRET frame directly below the saved user RSP.
    mov  rdx, [gs:8]         ; user RSP
    mov  rcx, [gs:16]        ; user RIP
    mov  rsp, rdx
    mov  rbp, rsp            ; mark return frame top before pushing IRET frame
    push qword 0x23          ; SS
    push rdx                 ; RSP
    push qword [gs:24]       ; RFLAGS
    push qword 0x1B          ; CS
    push qword [gs:16]       ; RIP
    swapgs
    iretq

.exit_path:
    ; The process requested exit — never return to user space.
    mov  byte [rel g_user_exit], 0
    sti
.hang:
    hlt
    jmp  .hang

section .note.GNU-stack noalloc noexec nowrite progbits
