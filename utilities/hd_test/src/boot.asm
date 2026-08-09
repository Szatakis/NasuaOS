[bits 32]

extern kernel_end
extern kmain

; Multiboot2 header

section .multiboot
align 8

multiboot_header:
    ; Multiboot2 header

    dd 0xE85250D6                    ; magic
    dd 0                              ; architecture = i386

    ; header length
    dd multiboot_header_end - multiboot_header

    ; checksum
    dd -(0xE85250D6 + 0 + \
        (multiboot_header_end - multiboot_header))


    ; Framebuffer request
    ;
    ; type   = 5
    ; flags  = 0
    ; size   = 20
    ;
    ; width  = 1280
    ; height = 720
    ; depth  = 32

    dw 5
    dw 0
    dd 20

    dd 1280
    dd 720
    dd 32


    dd 0


    ; End tag

    dw 0
    dw 0
    dd 8


multiboot_header_end:


; Code

section .text

global _start

_start:
    cli

    ; Multiboot2
    ;
    ; EAX = 0x36D76289
    ; EBX = adres Multiboot2 information structure

    mov [multiboot_magic], eax
    mov [multiboot_info], ebx


    ; Stack

    mov esp, stack_top

    and esp, 0xFFFFFFF0


    ; kmain(magic, mbi_addr)
    ; cdecl:

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


; Stack

section .bss

align 16

stack:
    resb 16384

stack_top: