[bits 32]

section .multiboot
align 8

multiboot_header:
    dd 0xE85250D6
    dd 0
    dd multiboot_header_end - multiboot_header

    dd -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header))


    dw 5                  ; type
    dw 0                  ; flags
    dd 20                 ; size

    dd 1280               ; width
    dd 720                ; height
    dd 32                 ; depth

    align 8

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