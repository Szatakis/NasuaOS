[bits 32]

; Multiboot2 Header
section .multiboot
align 8

multiboot_header:
    ; Header
    dd 0xE85250D6
    dd 0
    dd multiboot_header_end - multiboot_header
    dd -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header))


    ; Framebuffer Request
    dw 5 ; Type
    dw 0 ; Flags
    dd 20 ; Size

    dd 1280 ; Width
    dd 720 ; Height
    dd 32 ; Color depth


    ; End Tag
    align 8

    dw 0 ; Type
    dw 0 ; Flags
    dd 8 ; Size


multiboot_header_end:


; Kernel Entry Point
section .text

global _start
extern kmain


_start:
    ; Disable interrupts
    cli


    ; Initialize kernel stack
    mov esp, stack_top


    ; EBX = Multiboot2 information structure
    ; EAX = Multiboot2 magic value
    push ebx ; Multiboot information
    push eax ; Multiboot magic


    ; Enter kernel
    call kmain
    ; kmain() should never return

.hang:
    cli
    hlt
    jmp .hang


; Kernel Stack
section .bss

align 16

stack:
    resb 16384 ; 16 KiB

stack_top: