bits 64

global gdt_load

section .text

gdt_load:
    lgdt [rdi]

    lea rax, [rel .reload_segments]
    push qword 0x08
    push rax
    retfq

.reload_segments:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    mov ax, 0x28
    ltr ax
    ret
