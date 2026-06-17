.section .text.boot
.global _start

_start:
    // x0 = device tree blob address (passed by bootloader/QEMU)
    // Set up minimal stack from linker-provided boundary
    ldr x30, =_stack_end
    mov sp, x30

    // Jump to main kernel code
    bl kernel_main

halt:
    wfe
    b halt
