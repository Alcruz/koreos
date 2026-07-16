ENTRY(_start)

SECTIONS
{
    /* QEMU 6.2's arm_load_kernel() does not honor this image's text_offset=0
     * Linux/arm64 boot header field: it always loads a raw "-kernel" blob at
     * ram_base + 0x200000 and synthesizes its own tiny entry stub at ram_base
     * (set x0..x3, branch to the real load address) rather than jumping to
     * ram_base+text_offset directly. Link at the address QEMU actually uses,
     * or every absolute address baked in at link time (VBAR_EL1, _kernel_start
     * et al) is off by 0x200000 while PC-relative code keeps working — which
     * is exactly what made this look fine until the first exception vectored
     * into empty RAM. Confirmed via `-d in_asm`: real _start executes at
     * 0x40200000, not the 0x40000000 the header/CLAUDE.md assume. */
    . = 0x40200000;
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
