[bits 64]

global _start

section .text

_start:

    ; RDX = EFI_SYSTEM_TABLE

    mov rbx, rdx


    ; EFI_SYSTEM_TABLE->ConOut
    ; offset 0x40

    mov rcx, [rbx+0x40]


    ; ConOut->OutputString
    ; offset 0x08

    mov rax, [rcx+0x08]


    ; argument 2 = CHAR16*

    lea rdx, [message]


    call rax


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
    dw 'F'
    dw 'W'
    dw 'S'
    dw 'e'
    dw 't'
    dw 'u'
    dw 'p'
    dw 13
    dw 10
    dw 0