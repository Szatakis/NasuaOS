.section .text.start
.global _start
_start:
    mov r4, r2           /* r2 = DTB physical address */
    ldr sp, =stack_top
    mov r0, r4
    bl kmain
1:  wfi
    b 1b

.section .bss
.align 4
.skip 65536
stack_top:
