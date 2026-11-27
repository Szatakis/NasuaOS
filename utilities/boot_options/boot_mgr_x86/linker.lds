ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    /* Load high enough that the 32-bit kernel image (linked at 1 MiB,
       spanning ~0x100000-0x49a000) cannot overwrite us while it loads. */
    . = 0x8000000;

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
