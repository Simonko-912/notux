; Notux OS — C Runtime Startup (crt0)
; libnotux/crt0.asm
;
; Entry point for every user-space Notux program.
;
;   _start:
;     1. argc/argv/envp from stack
;     2. zero BSS
;     3. run .init_array constructors
;     4. call main(argc, argv, envp)
;     5. run .fini_array destructors
;     6. exit(main's return value)

bits 64
section .text
global _start
global nx_syscall6

extern main
extern __bss_start
extern __bss_end
extern __init_array_start
extern __init_array_end
extern __fini_array_start
extern __fini_array_end

%define SYS_EXIT 60

; ── Generic 6-arg syscall wrapper ────────────────────────────────
; C:  long nx_syscall6(nr, a1, a2, a3, a4, a5)
; in: rdi=nr  rsi=a1  rdx=a2  rcx=a3  r8=a4  r9=a5
; out: rax = kernel return value
nx_syscall6:
    mov  rax, rdi
    mov  rdi, rsi
    mov  rsi, rdx
    mov  rdx, rcx
    mov  r10, r8
    mov  r8,  r9
    syscall
    ret

_start:
    xor  rbp, rbp            ; end of call chain

    ; ── 1. argc (rsp), argv[0..] next, then envp ──────────────
    pop  rdi                 ; argc
    mov  rsi, rsp            ; argv
    lea  rdx, [rsi + rdi*8 + 8]  ; envp = argv + argc + 1

    ; stash in callee-saved regs (survive BSS clear + ctors)
    mov  r12, rdi            ; argc
    mov  r13, rsi            ; argv
    mov  r14, rdx            ; envp

    ; ── 2. Zero BSS ────────────────────────────────────────────
    lea  rdi, [rel __bss_start]
    lea  rcx, [rel __bss_end]
    sub  rcx, rdi
    jle  .bss_done
    xor  eax, eax
    rep  stosb
.bss_done:

    ; ── 3. Global constructors ─────────────────────────────────
    lea  rbx, [rel __init_array_start]
    lea  r15, [rel __init_array_end]
.ctor_loop:
    cmp  rbx, r15
    jge  .ctor_done
    mov  rax, [rbx]
    call rax
    add  rbx, 8
    jmp  .ctor_loop
.ctor_done:

    ; ── 4. Call main ───────────────────────────────────────────
    mov  rdi, r12
    mov  rsi, r13
    mov  rdx, r14
    call main
    mov  r12, rax            ; save return value

    ; ── 5. Global destructors ──────────────────────────────────
    lea  rbx, [rel __fini_array_start]
    lea  r15, [rel __fini_array_end]
.dtor_loop:
    cmp  rbx, r15
    jge  .dtor_done
    mov  rax, [rbx]
    call rax
    add  rbx, 8
    jmp  .dtor_loop
.dtor_done:

    ; ── 6. exit(return_value) ──────────────────────────────────
    mov  rdi, r12
    mov  rax, SYS_EXIT
    syscall

.halt:
    hlt
    jmp  .halt

section .note.GNU-stack noalloc noexec nowrite progbits