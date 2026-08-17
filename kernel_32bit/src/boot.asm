[bits 32]

; Multiboot2 Header
section .multiboot
align 8

multiboot_header_start:
    ; Multiboot2 header
    dd 0xE85250D6 ; Magic
    dd 0 ; Architecture: i386
    dd multiboot_header_end - multiboot_header_start

    ; Checksum:
    ; magic + architecture + header_length + checksum = 0
    dd -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header_start))


    ; Framebuffer request
    align 8

    dw 5 ; Framebuffer tag
    dw 0 ; Flags
    dd 20 ; Tag size

    dd 1280 ; Width
    dd 720 ; Height
    dd 32 ; Bits per pixel


    ; End tag
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
    ; Disable interrupts during early kernel initialization
    cli


    ; Initialize kernel stack
    mov esp, stack_top


    ; Pass Multiboot2 information structure to the kernel
    ;
    ; EBX contains the physical address of the Multiboot2 information
    ; structure provided by the bootloader.
    push ebx


    ; Enter the C++ kernel
    call kmain
    ; kmain() should never return.
    ; If it does, halt the CPU permanently.

.hang:
    hlt
    jmp .hang


; Kernel Stack
section .bss

align 16

stack:
    resb 16384 ; 16 KiB kernel stack

stack_top: