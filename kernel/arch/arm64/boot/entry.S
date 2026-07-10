.section .text.boot
.global _start

_start:
    b       _start_main         // code0: branch past the 64-byte header
    .long   0                   // code1 (reserved)
    .quad   0                   // text_offset: load offset from RAM base
    .quad   _kernel_size        // image_size: memory footprint to reserve
    .quad   0                   // flags: LE, 2MB-aligned placement
    .quad   0                   // res2
    .quad   0                   // res3
    .quad   0                   // res4
    .long   0x644d5241          // magic: "ARM\x64"
    .long   0                   // res5 (PE/COFF header offset, unused)

_start_main:
    // x0 = device tree blob address (passed by QEMU)
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
