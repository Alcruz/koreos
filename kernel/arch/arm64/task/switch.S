// void switch_to(task_t *prev, task_t *next)
//   x0 = prev, x1 = next.
//
// Save prev's callee-saved integer state (AAPCS64 x19-x28, fp, lr) plus sp into
// prev->ctx, then restore next->ctx and return through the newly loaded lr.
// Caller-saved regs (x0-x18) are the compiler's responsibility across this
// call, so we only touch the callee-saved set here.
//
// Offsets below match cpu_context_t (kernel/include/task.h); the layout is
// guarded by _Static_asserts in kernel/core/task.c since the assembler here
// runs without cpp and can't share a header. cpu_context_t is the first field
// of task_t, so struct-offset == task-pointer-offset.
//
//   x19 x20 x21 x22 x23 x24 x25 x26 x27 x28  fp(x29)  lr(x30)  sp
//    0   8  16  24  32  40  48  56  64  72     80       88     96

    .section .text
    .global switch_to
    .type switch_to, %function
switch_to:
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]     // fp, lr
    mov x9, sp                  // str/ldr can't name sp directly
    str x9, [x0, #96]

    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldp x29, x30, [x1, #80]
    ldr x9, [x1, #96]
    mov sp, x9
    ret
    .size switch_to, . - switch_to
