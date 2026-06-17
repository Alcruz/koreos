.section .text.boot
.global _start

_start:
    // x0 = device tree blob address (passed by bootloader/QEMU)
    ldr x30, =_stack_end
    mov sp, x30

    // Zero the BSS (uninitialized globals).
    ldr x9, =_bss_start
    ldr x10, =_bss_end
1:  cmp x9, x10
    b.ge 2f
    str xzr, [x9], #8
    b 1b
2:

    // Jump to main kernel code (x0 still holds the DTB pointer)
    bl kernel_main

halt:
    wfe
    b halt
