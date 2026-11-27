[bits 32]

; Multiboot2 Header
section .multiboot

align 8

multiboot_header:
    ; Multiboot2 header
    dd 0xE85250D6 ; Magic
    dd 0 ; Architecture: i386
    dd multiboot_header_end - multiboot_header ; Header size

    ; Checksum:
    ; magic + architecture + header size + checksum = 0
    dd -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header))


    ; Framebuffer request
    dw 5 ; Tag type
    dw 0 ; Flags
    dd 20 ; Tag size

    dd 1280 ; Width
    dd 720 ; Height
    dd 32 ; Bits per pixel


    ; Multiboot2 end tag
    align 8

    dw 0 ; Tag type
    dw 0 ; Flags
    dd 8 ; Tag size


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


    ; EAX = Multiboot2 boot magic
    ; EBX = Physical address of Multiboot2 information structure
    push ebx ; Multiboot2 information
    push eax ; Multiboot2 magic


    ; Enter the kernel
    call kmain
    ; kmain() should never return.
    ; If it does, halt the CPU permanently.

.hang:
    cli
    hlt
    jmp .hang


; Kernel Stack
section .bss

align 16

stack:
    resb 16384 ; 16 KiB kernel stack

stack_top:


section .text

; Returns 1 in EAX when the CPU supports 64-bit long mode, 0 otherwise.
global cpu_has_long_mode

cpu_has_long_mode:
    push ebx

    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29        ; EDX.LM - long mode capable
    jz .no

    mov eax, 1
    cpuid
    test edx, 1 << 6         ; EDX.PAE - physical address extension
    jz .no

    mov eax, 1
    pop ebx
    ret

.no:
    xor eax, eax
    pop ebx
    ret