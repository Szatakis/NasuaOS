[bits 32]

section .multiboot
align 4

header:
    dd 0x1BADB002
    dd 0
    dd -(0x1BADB002)

section .text
global _kstart
extern kmain

_kstart:
    cli

    mov esp, stack_top

    call kmain

.hang:
    hlt
    jmp .hang

section .bss
align 16

stack:
    resb 4096

stack_top: