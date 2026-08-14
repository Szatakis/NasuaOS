[bits 32]

section .multiboot

align 8

multiboot_header:
    dd 0xE85250D6
    dd 0
    dd multiboot_header_end - multiboot_header

    dd -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header))


    ; Framebuffer request

    dw 5
    dw 0
    dd 20

    dd 1280
    dd 720
    dd 32


    align 8


    ; End tag

    dw 0
    dw 0
    dd 8


multiboot_header_end:


section .text

global _start

extern kmain


_start:
    cli

    mov esp, stack_top

    ; EBX = Multiboot2 information
    ; EAX = Multiboot2 magic

    push ebx
    push eax

    call kmain


.hang:
    cli
    hlt
    jmp .hang


section .bss

align 16

stack:
    resb 16384

stack_top: