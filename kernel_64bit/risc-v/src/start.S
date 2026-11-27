.section .text.start
.global _start
_start:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    la sp, stack_top
    li a0, 0
    call kmain
1:  wfi
    j 1b

.section .bss
.align 4
.skip 65536
stack_top:

.section .note.GNU-stack,"",@progbits
