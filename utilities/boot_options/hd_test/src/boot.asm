[bits 32]

extern kernel_end
extern kmain

; Multiboot2 header

section .multiboot
align 8

multiboot_header:
    dd 0xE85250D6
    dd 0

    dd multiboot_header_end - multiboot_header

    dd -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header))

    dw 5
    dw 0
    dd 20

    dd 1280
    dd 720
    dd 32


    dd 0

    dw 0
    dw 0
    dd 8

multiboot_header_end:


section .text

global _start

_start:
    cli

    mov [multiboot_magic], eax
    mov [multiboot_info], ebx


    mov esp, stack_top

    and esp, 0xFFFFFFF0

    push dword [multiboot_info]
    push dword [multiboot_magic]

    call kmain

    add esp, 8


.hang:
    cli
    hlt

    jmp .hang


; Data

section .data

align 4

multiboot_magic:
    dd 0

multiboot_info:
    dd 0


section .bss

align 16

stack:
    resb 16384

stack_top: