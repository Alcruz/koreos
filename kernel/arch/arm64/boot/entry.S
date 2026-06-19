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

    // Install the EL1 exception vector table (x0/DTB preserved; x9 is scratch)
    ldr x9, =vectors
    msr vbar_el1, x9
    isb

    // Jump to main kernel code (x0 still holds the DTB pointer)
    bl kernel_main

halt:
    wfe
    b halt
