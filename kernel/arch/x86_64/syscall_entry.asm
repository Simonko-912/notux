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
    mov  qword [gs:8], rsp   ; save user rsp directly in gs slot
    mov  qword [gs:16], rcx  ; save user RIP for the return path
    mov  qword [gs:24], r10  ; save a4
    mov  qword [gs:32], r8   ; save a5
    mov  qword [gs:40], r9   ; save a6
    mov  rsp, qword [gs:0]   ; switch to kernel stack

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
    mov  rcx, [gs:24]        ; a4
    mov  r8,  [gs:32]        ; a5
    mov  r9,  [gs:40]        ; a6
    push qword [gs:8]        ; stack arg 8: saved user rsp
    push qword [gs:16]       ; stack arg 7: saved RIP after SYSCALL

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
    ; Return via IRET with explicit ring-3 selectors because this environment
    ; does not preserve the expected SYSRET selector state reliably.
    mov  rdx, [gs:8]         ; user RSP
    mov  rcx, [gs:16]        ; user RIP
    mov  rsp, rdx
    push qword 0x23          ; SS
    push rdx                 ; RSP
    push qword r11           ; RFLAGS from SYSCALL
    push qword 0x1B          ; CS
    push rcx                 ; RIP
    swapgs
    iretq

.old_return_path:
    add  rsp, 8              ; discard saved syscall nr
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
