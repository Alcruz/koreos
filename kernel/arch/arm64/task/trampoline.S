// void task_trampoline(void)
//
// First code a freshly primed task runs. `task_start` puts the entry fn in x19,
// its argument in x20, and this address in ctx.lr; `switch_to` restores those
// callee-saved slots on the switch, then `ret`s here. We move arg -> x0 and
// call the entry.
//
// The entry has no exit path yet — no scheduler yield, no task_exit — so if it
// ever returns, we panic. Loud failure beats a silent spin.

    .section .text
    .global task_trampoline
    .type task_trampoline, %function
task_trampoline:
    mov x0, x20                     // arg
    blr x19                         // fn(arg)
    adrp x0, .Lreturned
    add  x0, x0, :lo12:.Lreturned
    bl   panic                      // noreturn
1:  wfe                             // unreachable belt-and-braces
    b    1b
    .size task_trampoline, . - task_trampoline

    .section .rodata
.Lreturned:
    .asciz "task: entry function returned"
