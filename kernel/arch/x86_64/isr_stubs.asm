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

; In 64-bit mode iretq always pops five words (RIP, CS, RFLAGS, RSP, SS),
; but the CPU only pushes RSP/SS when the interrupt changes privilege
; level.  Returning from a ring-0 entry with a plain iretq therefore reads
; two words that were never pushed and leaves rsp 16 bytes too high, so the
; kernel resumes on a corrupted stack.  Synthesize the missing pair into a
; full frame instead (the same trick Linux uses for its int 0x80 entry).
; RFLAGS comes from the saved frame, not pushfq, so IF is restored as it was.
; CpuState (see process.h) is laid out as:
;     r15..rax | vector | error_code | RIP | CS | RFLAGS | RSP | SS
; The CPU only pushes RSP/SS when the interrupt changes privilege level, so
; a ring-0 entry yields a 3-word frame and everything past RFLAGS is not
; there at all.  isr_dispatch and sched_tick must not read or write those
; two slots in that case: writing them lands on the interrupted function's
; return address and its caller's frame, which is what turns a plain
; sched_yield() into a jump through low memory.
;
; So make every entry the same shape: slide the saved block 16 bytes down
; into free stack and append the two missing words.  The frame then ends
; exactly at the entry rsp, so the iretq below leaves rsp where the CPU
; left it, and C always sees a full five-word frame.
;
; Offsets from rsp (after PUSH_REGS):
;     0..112 GPRs | 120 vector | 128 error_code | 136 RIP | 144 CS
;
; Leaving the stub.  How many words iretq pops depends on where it is
; going, not on which vector we are on:
;
;   * privilege change (ring 3 -> ring 0): the CPU pushed SS, RSP, RFLAGS,
;     CS, RIP, so iretq pops five.
;   * IST / TSS stack switch (vectors idt.c points at an IST): the CPU had
;     to record the stack it is leaving, so it also pushes five, even at
;     CPL 0.
;   * plain same-CPL entry: the CPU pushes only RIP, CS, RFLAGS, and iretq
;     pops only those three -- RSP and SS are NOT fetched.
;
; So the same-CPL case needs no fixup at all.  Synthesising an RSP/SS pair
; for it (which this macro used to do) is not merely redundant: iretq never
; pops them, so every interrupt silently ate 16 bytes of the interrupted
; stack, and after a few thousand IRQs the stack ran off the end of its
; region.  The only real work here is making sure TF is not handed back.
;
; sched_tick fills in RIP/CS/RFLAGS only.  It cannot fill RSP/SS for a
; same-CPL switch, and must not: iretq would ignore them anyway, and
; slot 20/21 of a ring-0 frame is live data belonging to the interrupted
; function.  A ring-0 -> ring-3 switch needs a genuine RSP/SS, so it never
; comes back through here at all -- sched_tick hands it to
; sched_enter_from_exit, which builds a complete frame of its own.
%macro LEAVE_FRAME 2
    mov  r11, 0FFFFFFFFFFFFFEFFh
    and  qword [rsp + 2*8], r11      ; never resume with TF still set
    iretq
%endmacro

; ── ISR without CPU error code (push dummy 0) ───────────────────
%macro ISR_NOERR 2
global isr%1
isr%1:
    push qword 0        ; dummy error code
    push qword %1       ; vector number
    PUSH_REGS
    mov  rdi, rsp       ; arg0 = CpuState*
    call isr_dispatch
    POP_REGS
    add  rsp, 16        ; pop vector + error_code
    LEAVE_FRAME %1, %2
%endmacro

; ── ISR with CPU error code (CPU already pushed it) ─────────────
%macro ISR_ERR 2
global isr%1
isr%1:
                        ; error code already on stack from CPU
    push qword %1       ; vector number
    PUSH_REGS
    mov  rdi, rsp
    call isr_dispatch
    POP_REGS
    add  rsp, 16
    LEAVE_FRAME %1, %2
%endmacro

; ── Exception stubs (0-31) ──────────────────────────────────────
; second arg marks the vectors idt.c gives an IST
ISR_NOERR 0, 0  ; #DE divide error
ISR_NOERR 1, 0  ; #DB debug
ISR_NOERR 2, 1  ; NMI           (IST 2)
ISR_NOERR 3, 0  ; #BP breakpoint
ISR_NOERR 4, 0  ; #OF overflow
ISR_NOERR 5, 0  ; #BR bound range
ISR_NOERR 6, 0  ; #UD invalid opcode
ISR_NOERR 7, 0  ; #NM device not available
ISR_ERR   8, 1  ; #DF double fault (IST 1)
ISR_NOERR 9, 0  ; coprocessor overrun
ISR_ERR   10, 0 ; #TS invalid TSS
ISR_ERR   11, 0 ; #NP segment not present
ISR_ERR   12, 0 ; #SS stack fault
ISR_ERR   13, 0 ; #GP general protection
ISR_ERR   14, 0 ; #PF page fault
ISR_NOERR 15, 0
ISR_NOERR 16, 0 ; #MF x87 FPU
ISR_ERR   17, 0 ; #AC alignment check
ISR_NOERR 18, 0 ; #MC machine check
ISR_NOERR 19, 0 ; #XM SIMD
ISR_NOERR 20, 0
ISR_NOERR 21, 0
ISR_NOERR 22, 0
ISR_NOERR 23, 0
ISR_NOERR 24, 0
ISR_NOERR 25, 0
ISR_NOERR 26, 0
ISR_NOERR 27, 0
ISR_NOERR 28, 0
ISR_NOERR 29, 0
ISR_ERR   30, 0
ISR_NOERR 31, 0

; ── IRQ stubs (32-255) ──────────────────────────────────────────
%assign VEC 32
%rep 224
ISR_NOERR VEC, 0
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
