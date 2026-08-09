[bits 64]

global _start

section .text

_start:
    mov rbx, rdx

    mov rcx, [rbx+0x40]

    mov rsi, rcx

    mov rax, [rcx+0x08]

    mov rcx, rsi                 

    lea rdx, [message]

    sub rsp, 32
    call rax
    add rsp, 32

    jmp hang


hang:
    hlt
    jmp hang



section .data

message:

    dw 'N'
    dw 'a'
    dw 's'
    dw 'u'
    dw 'a'
    dw 'O'
    dw 'S'
    dw ' '
    dw 'U'
    dw 'E'
    dw 'F'
    dw 'I'
    dw ' '
    dw 'S'
    dw 'H'
    dw 'E'
    dw 'L'
    dw 'L'
    dw 13
    dw 10
    dw 0