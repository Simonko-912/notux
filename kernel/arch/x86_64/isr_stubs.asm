; Notux OS — ISR stubs (proper register save/restore)
; kernel/arch/x86_64/isr_stubs.asm
;
; Layout on stack when isr_dispatch(CpuState*) is called:
;   CPU pushed (bottom-to-top): ss, rsp, rflags, cs, rip
;   We push:  error_code (0 if none), vector, then all GPRs
;   That matches CpuState: r15..rax | vector | error_code | rip..ss
;
; The iretq restores rip,cs,rflags,rsp,ss automatically.

BITS 64
section .text

extern isr_dispatch

; ── save/restore macros ─────────────────────────────────────────
%macro PUSH_REGS 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro POP_REGS 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

; ── ISR without CPU error code (push dummy 0) ───────────────────
%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0        ; dummy error code
    push qword %1       ; vector number
    PUSH_REGS
    mov  rdi, rsp       ; arg0 = CpuState*
    call isr_dispatch
    POP_REGS
    add  rsp, 16        ; pop vector + error_code
    iretq
%endmacro

; ── ISR with CPU error code (CPU already pushed it) ─────────────
%macro ISR_ERR 1
global isr%1
isr%1:
                        ; error code already on stack from CPU
    push qword %1       ; vector number
    PUSH_REGS
    mov  rdi, rsp
    call isr_dispatch
    POP_REGS
    add  rsp, 16
    iretq
%endmacro

; ── Exception stubs (0-31) ──────────────────────────────────────
ISR_NOERR 0    ; #DE divide error
ISR_NOERR 1    ; #DB debug
ISR_NOERR 2    ; NMI
ISR_NOERR 3    ; #BP breakpoint
ISR_NOERR 4    ; #OF overflow
ISR_NOERR 5    ; #BR bound range
ISR_NOERR 6    ; #UD invalid opcode
ISR_NOERR 7    ; #NM device not available
ISR_ERR   8    ; #DF double fault
ISR_NOERR 9    ; coprocessor overrun
ISR_ERR   10   ; #TS invalid TSS
ISR_ERR   11   ; #NP segment not present
ISR_ERR   12   ; #SS stack fault
ISR_ERR   13   ; #GP general protection
ISR_ERR   14   ; #PF page fault
ISR_NOERR 15
ISR_NOERR 16   ; #MF x87 FPU
ISR_ERR   17   ; #AC alignment check
ISR_NOERR 18   ; #MC machine check
ISR_NOERR 19   ; #XM SIMD
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

; ── IRQ stubs (32-255) ──────────────────────────────────────────
%assign VEC 32
%rep 224
ISR_NOERR VEC
%assign VEC VEC+1
%endrep

; ── Stub pointer table ──────────────────────────────────────────
section .data
global isr_stubs
align 8
isr_stubs:
%assign VEC 0
%rep 256
    dq isr %+ VEC
%assign VEC VEC+1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits
