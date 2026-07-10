ENTRY(_start)

SECTIONS
{
    . = 0x40000000;
    _kernel_start = .;

    .boot : {
        KEEP(*(.text.boot))
    }

    .text : {
        *(.text)
        *(.text*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata*)
    }

    .data : {
        *(.data)
        *(.data*)
    }

    . = ALIGN(8);
    _bss_start = .;
    .bss : {
        *(.bss)
        *(.bss*)
        *(COMMON)
    }
    . = ALIGN(8);
    _bss_end = .;

    . = . + 0x8000;
    _stack_end = .;

    . = ALIGN(0x1000);
    _kernel_end = .;
}

_kernel_size = _kernel_end - _kernel_start;
