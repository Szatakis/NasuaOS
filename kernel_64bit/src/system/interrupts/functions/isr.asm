bits 64

global isr_default
global isr_divide_error
global isr_page_fault
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

; PAGE FAULT (#PF)

isr_page_fault:
    cli
    push 14

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