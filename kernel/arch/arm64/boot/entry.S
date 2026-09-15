.section .text.boot
.global _start

// arm64 Linux Image header (64 bytes). Keep it even though this QEMU
// (6.2, arm_load_kernel) ignores text_offset for raw -kernel blobs — a
// different loader (u-boot, real firmware) may honor it. code0 branches
// past the header to _start_main.
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
    // x0 = device tree blob pointer, passed by QEMU. Preserve it all the
    // way to kernel_main — kernel_main panics if it doesn't get a valid
    // DTB rather than guess RAM size. Use x9+ for any scratch below.
    ldr x30, =_stack_end
    mov sp, x30

    // Zero BSS (_bss_start.._bss_end, 8-byte aligned) before any C code
    // runs — nothing else does it, and C globals rely on it.
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
