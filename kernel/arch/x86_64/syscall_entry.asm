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
    mov  rsp,     [gs:0]     ; switch to kernel stack

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
    mov  r8,  [gs:8]         ; a5 becomes saved user rsp (diag only)
    mov  r9,  [gs:8]         ; placeholder 7th arg (unused for now)

    call syscall_handler     ; result in rax

    cmp  byte [rel g_user_exit], 0
    jne  .exit_path

    add  rsp, 8
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    pop  rcx
    pop  r11
    mov  rsp, [gs:8]         ; restore user rsp
    swapgs
    db 0x48, 0x0F, 0x07      ; sysretq

.exit_path:
    ; The process requested exit — never return to user space.
    mov  byte [rel g_user_exit], 0
    sti
.hang:
    hlt
    jmp  .hang

section .note.GNU-stack noalloc noexec nowrite progbits
