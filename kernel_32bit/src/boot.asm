[bits 32]

section .multiboot
align 4

MBALIGN equ 1<<0
MEMINFO equ 1<<1
VIDEO   equ 1<<2

FLAGS equ MBALIGN | MEMINFO | VIDEO
MAGIC equ 0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

dd MAGIC
dd FLAGS
dd CHECKSUM

; graphics fields
dd 0
dd 0
dd 0
dd 0
dd 1280
dd 720
dd 32

section .text
global _kmain
extern kmain

_kmain:

    cli

    mov esp, stack_top

    push ebx

    call kmain

.hang:
    hlt
    jmp .hang

section .bss
align 16

stack:
    resb 4096

stack_top: