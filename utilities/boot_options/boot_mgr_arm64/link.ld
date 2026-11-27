OUTPUT_FORMAT(elf64-littleaarch64)
ENTRY(_start)

SECTIONS
{
    . = 0xffffffff80400000;

    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    }

    . = ALIGN(0x1000);
    .text : { *(.text.start) *(.text*) }
    . = ALIGN(0x1000);
    .rodata : { *(.rodata*) }
    . = ALIGN(0x1000);
    .data : { *(.data*) }
    .bss : { *(COMMON) *(.bss*) }

    . = ALIGN(16);
    .stack (NOLOAD) : {
        . = . + 0x10000;
        stack_top = .;
    }

    /DISCARD/ : { *(.eh_frame*) *(.note .note.*) }
}
