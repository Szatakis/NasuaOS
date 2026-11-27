OUTPUT_FORMAT(elf32-littleriscv)
ENTRY(_start)

PHDRS
{
    limine_requests PT_LOAD;
    text PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
}

SECTIONS
{
    . = 0x80200000;
    . = ALIGN(0x1000);
    .text : { *(.text.start) *(.text*) } :text
    . = ALIGN(0x1000);
    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests
    . = ALIGN(0x1000);
    .rodata : { *(.rodata*) } :rodata
    . = ALIGN(0x1000);
    . = ALIGN(16);
    __global_pointer$ = . + 0x800;
    .data : { *(.data*) *(.sdata*) } :data
    .bss : { *(.sbss*) *(COMMON) *(.bss*) } :data

    /DISCARD/ : { *(.eh_frame*) *(.note .note.*) }
}
