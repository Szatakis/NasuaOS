[bits 32]

section .multiboot
align 8

header_start:

dd 0xE85250D6        ; magic multiboot2
dd 0                ; architecture i386
dd header_end - header_start
dd -(0xE85250D6 + 0 + (header_end - header_start))


; framebuffer request
align 8
dw 5                ; framebuffer tag
dw 0
dd 20
dd 1280             ; width
dd 720              ; height
dd 32               ; depth


; end tag
align 8
dw 0
dw 0
dd 8


header_end:


section .text

global _start
extern kmain


_start:

cli


mov esp, stack_top


push ebx        ; multiboot info


call kmain



.hang:

hlt
jmp .hang



section .bss

align 16

stack:

resb 16384


stack_top: