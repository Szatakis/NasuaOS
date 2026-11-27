.section .text.start
.global _start
_start:
    ldr x0, =stack_top
    mov sp, x0
    mov x0, xzr
    bl kmain
1:  wfe
    b 1b

.section .bss
.align 4
.skip 65536
stack_top:

.section .note.GNU-stack,"",@progbits
