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
extern g_user_rflags
extern sched_enter_from_exit

syscall_entry_asm:
    swapgs
    mov  qword [gs:8],  rsp   ; save user rsp directly in gs slot
    mov  qword [gs:16], rcx  ; save user RIP for the return path
    mov  qword [gs:32], rdi  ; a1
    mov  qword [gs:40], rsi  ; a2
    mov  qword [gs:48], rdx  ; a3
    mov  qword [gs:56], r10  ; a4
    mov  qword [gs:64], r8   ; a5
    mov  qword [gs:72], r9   ; a6
    mov  [rel g_user_rflags], r11  ; R11 = user RFLAGS, clobbered by the C call
    mov  rsp, qword [gs:0]   ; switch to kernel stack

    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rax                 ; save syscall number for diag in handler

    ; syscall_handler(nr, a1, a2, a3, a4, a5) is SysV, so the call reads
    ; rdi rsi rdx rcx r8 r9.  The user's registers hold nr in rax and the
    ; args in rdi rsi rdx r10 r8 r9, so they must be shuffled — rax is the
    ; syscall number and has to end up in rdi, not rdi's own value.
    mov  rdi, rax            ; nr
    mov  rsi, qword [gs:32]  ; a1
    mov  rdx, qword [gs:40]  ; a2
    mov  rcx, qword [gs:48]  ; a3
    mov  r8,  qword [gs:56]  ; a4
    mov  r9,  qword [gs:64]  ; a5

    call syscall_handler     ; result in rax

    cmp  byte [rel g_user_exit], 0
    jne  .exit_path

    add  rsp, 8              ; discard saved syscall nr
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
    mov  r11, [rel g_user_rflags]   ; R11 was clobbered by syscall_handler
    and  r11, 0xFFFFFFFFFFFFFEFF   ; clear TF (bit 8): user never single-steps
    mov  rsp, rdx
    push qword 0x23          ; SS
    push rdx                 ; RSP
    push qword r11           ; user RFLAGS preserved from SYSCALL time
    push qword 0x1B          ; CS
    push rcx                 ; RIP
    swapgs
    iretq

.exit_path:
    ; The process requested exit — never return to user space.  Unwind the
    ; saved registers exactly as the normal path does, then let the
    ; scheduler enter another process.  Do NOT sti/hlt here: that would
    ; leave the CPU at CPL0, where the next interrupt pushes a 3-word frame
    ; that the ring-3-only stub unwinds as if it were 5 words.
    ;
    ; We still hold the kernel GS base (the SYSCALL entry did swapgs).  The
    ; process enter_process is about to iretq into must resume with the USER
    ; base (0) so that its own first SYSCALL swaps GS_BASE to the kernel
    ; area; leaving the kernel base loaded here would make that first
    ; SYSCALL swapgs to 0 and read the kernel stack slot [gs:0] from
    ; address 0 -- a garbage RSP that turns the very first syscall into a
    ; #PF and every subsequent context into the torn-frame crashes seen in
    ; the shell respawn loop.  Put the bases back before starting the next
    ; process.
    mov  byte [rel g_user_exit], 0
    add  rsp, 8              ; discard saved syscall nr
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    swapgs                   ; GS_BASE=user(0), KERNEL_GS_BASE=&g_sys_area
    call sched_enter_from_exit
.hang:
    hlt
    jmp  .hang

section .note.GNU-stack noalloc noexec nowrite progbits
