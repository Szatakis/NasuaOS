[bits 32]

section .multiboot
align 4

    dd 0x1BADB002
    dd 0
    dd -(0x1BADB002)

section .text
global _start

_start:
    cli

    mov esp, stack_top

    mov edi, 0xB8000
    mov esi, message

.print:
    lodsb
    test al, al
    jz .hang

    mov ah, 0x0F
    stosw
    jmp .print

.hang:
    hlt
    jmp .hang

section .rodata

message db "Hello World!", 0

section .bss
align 16

stack:
    resb 4096

stack_top: