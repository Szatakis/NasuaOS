bits 64

global isr_default
global isr_divide_error
global isr_page_fault
global isr_double_fault
global isr_gpf
global isr_stack_fault
global isr_invalid_opcode
global isr_overflow
global isr_bound_range
global isr_invalid_tss
global isr_segment_not_present
global isr_alignment_check
global isr_spurious
global irq0
extern isr_handler

section .text

%macro PUSH_REGS 0
    push rax
    push rbx
    push rcx
    push rdx

    push rbp
    push rdi
    push rsi

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

    pop rsi
    pop rdi
    pop rbp

    pop rdx
    pop rcx
    pop rbx
    pop rax

%endmacro


; DEFAULT INTERRUPT

isr_default:
    cli
    push 0
    push 0xDE

    jmp isr_common

; DIVIDE ERROR (#DE)

isr_divide_error:
    cli
    push 0
    push 0

    jmp isr_common

; PAGE FAULT (#PF) — CPU pushes error code

isr_page_fault:
    cli
    push 14

    jmp isr_common

; DOUBLE FAULT (#DF, vector 8) — CPU pushes error code (always 0)

isr_double_fault:
    cli
    push 8

    jmp isr_common

; GENERAL PROTECTION FAULT (#GP, vector 13) — CPU pushes error code

isr_gpf:
    cli
    push 13

    jmp isr_common

; STACK-SEGMENT FAULT (#SS, vector 12) — CPU pushes error code

isr_stack_fault:
    cli
    push 12

    jmp isr_common

; INVALID OPCODE (#UD, vector 6) — no error code

isr_invalid_opcode:
    cli
    push 0
    push 6

    jmp isr_common

; OVERFLOW (#OF, vector 4) — no error code

isr_overflow:
    cli
    push 0
    push 4

    jmp isr_common

; BOUND RANGE EXCEEDED (#BR, vector 5) — no error code

isr_bound_range:
    cli
    push 0
    push 5

    jmp isr_common

; INVALID TSS (#TS, vector 10) — CPU pushes error code

isr_invalid_tss:
    cli
    push 10

    jmp isr_common

; SEGMENT NOT PRESENT (#NP, vector 11) — CPU pushes error code

isr_segment_not_present:
    cli
    push 11

    jmp isr_common

; ALIGNMENT CHECK (#AC, vector 17) — CPU pushes error code

isr_alignment_check:
    cli
    push 17

    jmp isr_common

; IRQ0 PIT

irq0:
    push 0
    push 32

    jmp isr_common

; LAPIC SPURIOUS INTERRUPT (vector 0xFF)

isr_spurious:
    push 0
    push 0xFF

    jmp isr_common

; COMMON HANDLER

isr_common:
    cld

    PUSH_REGS

    mov rdi,rsp

    sub rsp,8
    call isr_handler
    add rsp,8

    POP_REGS

    add rsp,16

    iretq