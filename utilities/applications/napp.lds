ENTRY(_start)

SECTIONS
{
    . = 0;

    .napp_header : {
        KEEP(*(.napp_header))
        . = ALIGN(64);
    }

    .text : {
        KEEP(*(.napp_entry))
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
        *(.bss .bss.*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame.*)
    }
}
