ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 1M;

    .text :
    {
        . = ALIGN(8);

        KEEP(*(.multiboot))

        *(.text*)
    } :text

    .rodata :
    {
        *(.rodata*)
    } :text

    .data :
    {
        *(.data*)
    } :data

    .bss :
    {
        . = ALIGN(16);

        *(COMMON)
        *(.bss*)
    } :data

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.comment)
        *(.note*)
    }
}
