# Koreos Roadmap

Living roadmap for the AArch64 kernel. Phases are ordered by dependency, not
calendar. Each phase has a **goal** (what "done" looks like), **deliverables**
(concrete artifacts / tests), and **why now** (why the phase sits where it
does in the order).

Legend: `[x]` shipped · `[~]` in progress · `[ ]` not started

---

## Phase 0 — Boot & serial `[x]`

- [x] `entry.S` with arm64 Linux Image header, BSS zeroing, stack, EL1 vectors
- [x] Raw `kernel.bin` boots on QEMU `virt` at `0x40200000` with DTB in `x0`
- [x] PL011 UART driver + `kprint` (strings / hex / decimal)
- [x] EL1 exception vectors + `panic`
- [x] Host-side unit test harness (Unity + ASan/UBSan)

## Phase 1 — Physical memory `[x]`

- [x] FDT parser, `/memory` node discovery (`fdt_get_memory`)
- [x] `memmap` with reservations (kernel image, DTB)
- [x] Physical frame allocator (`pmm`) — single-page + contiguous
- [x] Kernel heap (`kmalloc` / `kfree` / `kzalloc`)

## Phase 2 — MMU `[x]`

- [x] `MAIR_EL1` setup, identity page tables, translation enabled
- [x] Post-`mmu_enable` serial output proves fetch/stack/globals/MMIO map

## Phase 3 — Interrupts & timing `[x]`

- [x] GICv2 driver (distributor + CPU interface, per-IRQ enable/priority)
- [x] Generic IRQ dispatch layer (`kernel/core/irq.c`)
- [x] Periodic ARM generic timer driver, tick counter on serial

## Phase 4 — Tasks & scheduling `[~]`

Goal: two kernel tasks time-slicing on a single CPU.

- [x] `task_t` + `cpu_context_t` (callee-saved x19-x28, fp, lr, sp)
- [x] `task_create` / `task_destroy` allocating stack from `pmm` and struct
      from `heap`
- [x] `switch_to(prev, next)` in asm — save/restore callee-saved + sp, return
      via `lr`
- [x] Task start trampoline that primes `ctx.lr` so a fresh task enters a C
      entry point cleanly
- [x] Run queue (FIFO round-robin using the intrusive `next` link)
- [ ] Timer IRQ preempts the current task and calls `schedule()`
- [ ] Idle task (single `wfe` loop) so the scheduler always has something to
      run
- [ ] Host test for run-queue ops; QEMU smoke test where two tasks each print
      their id N times and interleave

**Why now:** the timer IRQ already fires (Phase 3), so preemption is unblocked.
Everything above scheduling (syscalls, userspace) needs a way to save/restore
CPU state — that's `switch_to`.

## Phase 5 — Synchronization primitives `[ ]`

Goal: safe concurrent access to kernel data structures once preemption is on.

- [ ] IRQ save/restore (`local_irq_save` / `local_irq_restore`) using `DAIF`
- [ ] Spinlock (single-CPU: reduces to IRQ disable; forward-compatible with
      real ticket lock later)
- [ ] Sleep/wake primitive built on wait queues + scheduler
- [ ] Convert the existing PMM / heap free-lists to take the appropriate lock
- [ ] Host tests exercising contended paths via cooperative interleaving

**Why now:** before adding blocking I/O or syscalls, the shared kernel data
structures (heap, PMM, run queue) need real mutual exclusion. Doing it after
one more subsystem is added is strictly more churn.

## Phase 6 — Higher-half kernel & virtual memory `[ ]`

Goal: kernel executes from a fixed high virtual address; user address space
is a distinct low range.

- [ ] Link kernel at a higher-half VA (e.g. `0xFFFF_0000_4020_0000`) with a
      trampoline that switches `TTBR1_EL1` before jumping to the high VA
- [ ] Separate `TTBR0_EL1` (user) and `TTBR1_EL1` (kernel) tables
- [ ] Kernel-linear map for the direct-mapped physical window
- [ ] `vmalloc`-style non-contiguous kernel VA allocator
- [ ] Per-task page-table root; `switch_to` swaps `TTBR0_EL1` and does the
      required TLB invalidation

**Why now:** without a higher-half, giving each task its own low-VA
address space (Phase 7) collides with kernel code. Do the layout change
before userspace exists so we only migrate kernel-only pointers.

## Phase 7 — Userspace & syscalls `[ ]`

Goal: run an EL0 program that traps into the kernel via `svc`.

- [ ] EL1 → EL0 `eret` path with a clean initial user context
- [ ] Syscall entry via `svc #0` through the synchronous EL0 vector
- [ ] Minimal syscall set: `write`, `exit`, `getpid`, `yield`
- [ ] User stack allocation + user page table population
- [ ] `execve`-style loader for a flat ELF (or raw blob) baked into the
      kernel image
- [ ] First userspace program: `hello, world` via `write(1, ...)`

## Phase 8 — Storage & filesystem `[ ]`

- [ ] `virtio-blk` driver (MMIO transport on QEMU virt)
- [ ] Block cache
- [ ] Read-only FAT or a simple custom FS to load a userspace init binary from
      disk instead of embedding it

## Phase 9 — Beyond `[ ]`

Not planned in detail yet — revisit after Phase 8:

- [ ] SMP (secondary CPU bring-up, per-CPU data, real spinlocks)
- [ ] `virtio-net` + a tiny network stack
- [ ] Shell / init program in userspace
- [ ] Signals, fork/exec, richer syscall surface

---

## Working principles

- **DTB is the source of truth.** Panic on missing/invalid DTB info rather
  than guessing (already true for RAM sizing; extend to IRQ controllers,
  timer freq, etc. as they land).
- **Test on the host when possible.** Address-agnostic C modules
  (mm, lib, data structures, scheduler policy) get Unity tests under
  ASan/UBSan. Reserve QEMU runs for anything that touches real MMIO or CPU
  state.
- **Small, reviewable phases.** Each phase should end at a demonstrable state
  (something new prints, a new test passes, a new user-visible behavior
  works) so bisecting a regression stays cheap.
