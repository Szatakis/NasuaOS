    .section .text.start
    .global _start
_start:
    ldr x0, =stack_top
    mov sp, x0
    mov x30, xzr
    bl kmain
1:  wfe
    b 1b

    .section .note.GNU-stack,"",@progbits
