[bits 32]

extern kernel_end
extern kmain


; Multiboot2 Header
section .multiboot
align 8

multiboot_header:
    ; Multiboot2 header
    dd 0xE85250D6 ; Magic
    dd 0 ; Architecture: i386
    dd multiboot_header_end - multiboot_header ; Header size

    ; Checksum
    dd -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header))


    ; Framebuffer request
    dw 5 ; Type
    dw 0 ; Flags
    dd 20 ; Size

    dd 1280 ; Width
    dd 720 ; Height
    dd 32 ; Depth


    ; End tag
    dd 0 ; Padding

    dw 0 ; Type
    dw 0 ; Flags
    dd 8 ; Size


multiboot_header_end:


; Kernel Entry Point
section .text

global _start


_start:
    ; Disable interrupts during early kernel initialization
    cli


    ; EAX = Multiboot2 magic
    ; EBX = Multiboot2 information structure
    mov [multiboot_magic], eax
    mov [multiboot_info], ebx


    ; Initialize kernel stack
    mov esp, stack_top

    ; Align stack to 16 bytes for ABI compatibility
    and esp, 0xFFFFFFF0


    ; Call C++ kernel
    ; kmain(multiboot_magic, multiboot_info)
    push dword [multiboot_info]
    push dword [multiboot_magic]

    call kmain

    ; Clean up arguments
    add esp, 8
    ; Kernel should never return

.hang:
    cli
    hlt
    jmp .hang


; Multiboot2 Data
section .data
align 4

multiboot_magic:
    dd 0

multiboot_info:
    dd 0


; Kernel Stack
section .bss
align 16

stack:
    resb 16384 ; 16 KiB kernel stack

stack_top: