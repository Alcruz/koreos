ENTRY(_start)

MEMORY
{
    ram : ORIGIN = 0x40000000, LENGTH = 512M
}

SECTIONS
{
    . = 0x40000000;

    .boot : {
        KEEP(*(.text.boot))
    } > ram

    .text : {
        *(.text)
        *(.text*)
    } > ram

    .rodata : {
        *(.rodata)
        *(.rodata*)
    } > ram

    .data : {
        *(.data)
        *(.data*)
    } > ram

    _bss_start = .;
    .bss : {
        *(.bss)
        *(.bss*)
        *(COMMON)
    } > ram
    _bss_end = .;

    . = . + 0x8000;
    _stack_end = .;
}
