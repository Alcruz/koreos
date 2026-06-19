// AArch64 exception vector table for EL1.
//
// VBAR_EL1 points here. The table is 2KB-aligned and holds 16 entries of
// 128 bytes each, in four groups of four (Sync, IRQ, FIQ, SError):
//   - Current EL with SP_EL0   (indices 0-3)
//   - Current EL with SP_ELx   (indices 4-7)
//   - Lower EL using AArch64   (indices 8-11)
//   - Lower EL using AArch32   (indices 12-15)
//
// Each entry saves the integer register file and tail-calls a common stub
// that dispatches to the C handler with (type, frame).

.section .text

// Save x0-x30 onto the stack as a 256-byte, 16-byte-aligned trap frame.
.macro kernel_entry
    sub     sp, sp, #256
    stp     x0, x1,   [sp, #16 * 0]
    stp     x2, x3,   [sp, #16 * 1]
    stp     x4, x5,   [sp, #16 * 2]
    stp     x6, x7,   [sp, #16 * 3]
    stp     x8, x9,   [sp, #16 * 4]
    stp     x10, x11, [sp, #16 * 5]
    stp     x12, x13, [sp, #16 * 6]
    stp     x14, x15, [sp, #16 * 7]
    stp     x16, x17, [sp, #16 * 8]
    stp     x18, x19, [sp, #16 * 9]
    stp     x20, x21, [sp, #16 * 10]
    stp     x22, x23, [sp, #16 * 11]
    stp     x24, x25, [sp, #16 * 12]
    stp     x26, x27, [sp, #16 * 13]
    stp     x28, x29, [sp, #16 * 14]
    str     x30,      [sp, #16 * 15]
.endm

.macro kernel_exit
    ldp     x0, x1,   [sp, #16 * 0]
    ldp     x2, x3,   [sp, #16 * 1]
    ldp     x4, x5,   [sp, #16 * 2]
    ldp     x6, x7,   [sp, #16 * 3]
    ldp     x8, x9,   [sp, #16 * 4]
    ldp     x10, x11, [sp, #16 * 5]
    ldp     x12, x13, [sp, #16 * 6]
    ldp     x14, x15, [sp, #16 * 7]
    ldp     x16, x17, [sp, #16 * 8]
    ldp     x18, x19, [sp, #16 * 9]
    ldp     x20, x21, [sp, #16 * 10]
    ldp     x22, x23, [sp, #16 * 11]
    ldp     x24, x25, [sp, #16 * 12]
    ldp     x26, x27, [sp, #16 * 13]
    ldp     x28, x29, [sp, #16 * 14]
    ldr     x30,      [sp, #16 * 15]
    add     sp, sp, #256
    eret
.endm

// .align 7 places each entry on the required 128-byte boundary.
.macro ventry type
    .align 7
    kernel_entry
    mov     x0, #\type
    mov     x1, sp
    b       __exception_common
.endm

.align 11               // VBAR_EL1 requires 2KB alignment
.global vectors
vectors:
    // Current EL with SP_EL0
    ventry 0                // Synchronous
    ventry 1                // IRQ
    ventry 2                // FIQ
    ventry 3                // SError

    // Current EL with SP_ELx
    ventry 4                // Synchronous
    ventry 5                // IRQ
    ventry 6                // FIQ
    ventry 7                // SError

    // Lower EL using AArch64
    ventry 8                // Synchronous
    ventry 9                // IRQ
    ventry 10               // FIQ
    ventry 11               // SError

    // Lower EL using AArch32
    ventry 12               // Synchronous
    ventry 13               // IRQ
    ventry 14               // FIQ
    ventry 15               // SError

__exception_common:
    bl      handle_exception
    kernel_exit
