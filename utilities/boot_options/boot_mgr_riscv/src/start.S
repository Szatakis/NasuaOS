    .section .text.start
    .global _start
_start:
    la sp, stack_top
    li a0, 0
    call kmain
1:  wfi
    j 1b

    .section .note.GNU-stack,"",@progbits
