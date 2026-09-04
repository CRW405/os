.section .text

# =====================================================================
# ISR/IRQ stubs
#
# The CPU can only jump straight to raw code on an interrupt, not call
# a C function directly — it doesn't know the C calling convention and
# won't save registers for you. Each stub here does the low-level part:
# push a fake error code if the CPU didn't push one, save every
# general-purpose register, call the matching C handler, restore the
# registers, and `iretq` back to whatever was interrupted.
#
# Vectors 0-31 are CPU exceptions (divide-by-zero, page fault, general
# protection fault, ...). A handful of them (8, 10-14, 17, 21, 29, 30)
# have the CPU push a real error code; the rest don't, so we push a
# dummy 0 in its place to keep every frame the same shape. All 32 fall
# through to the same isr_common_stub, which hands a pointer to the
# saved frame to isr_common_handler() in kernel.c.
# =====================================================================

.macro ISR_NOERRCODE num
.global isr\num
isr\num:
    push $0        # dummy error code, this vector's CPU doesn't push one
    push $\num     # vector number, so the C handler knows what happened
    jmp isr_common_stub
.endm

.macro ISR_ERRCODE num
.global isr\num
isr\num:
    push $\num     # vector number; the CPU already pushed a real error code
    jmp isr_common_stub
.endm

ISR_NOERRCODE 0    # divide-by-zero
ISR_NOERRCODE 1    # debug
ISR_NOERRCODE 2    # non-maskable interrupt
ISR_NOERRCODE 3    # breakpoint
ISR_NOERRCODE 4    # overflow
ISR_NOERRCODE 5    # bound range exceeded
ISR_NOERRCODE 6    # invalid opcode
ISR_NOERRCODE 7    # device not available
ISR_ERRCODE   8    # double fault
ISR_NOERRCODE 9    # coprocessor segment overrun (legacy, unused on real hw)
ISR_ERRCODE   10   # invalid TSS
ISR_ERRCODE   11   # segment not present
ISR_ERRCODE   12   # stack-segment fault
ISR_ERRCODE   13   # general protection fault
ISR_ERRCODE   14   # page fault
ISR_NOERRCODE 15   # reserved
ISR_NOERRCODE 16   # x87 floating-point exception
ISR_ERRCODE   17   # alignment check
ISR_NOERRCODE 18   # machine check
ISR_NOERRCODE 19   # SIMD floating-point exception
ISR_NOERRCODE 20   # virtualization exception
ISR_ERRCODE   21   # control protection exception
ISR_NOERRCODE 22   # reserved
ISR_NOERRCODE 23   # reserved
ISR_NOERRCODE 24   # reserved
ISR_NOERRCODE 25   # reserved
ISR_NOERRCODE 26   # reserved
ISR_NOERRCODE 27   # reserved
ISR_NOERRCODE 28   # hypervisor injection exception
ISR_ERRCODE   29   # VMM communication exception
ISR_ERRCODE   30   # security exception
ISR_NOERRCODE 31   # reserved

# shared by every isrN stub above: save all GPRs, call the C handler with
# a pointer to the saved frame, restore, then drop the vector+error code
# pair before returning.
isr_common_stub:
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    mov %rsp, %rdi         # arg0 (System V ABI) = pointer to the saved frame
    call isr_common_handler

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax
    add $16, %rsp   # drop our vector + error code pair
    iretq

# IRQ1 is the keyboard interrupt handler. It is called when the keyboard generates an interrupt.
.global irq1

irq1:
    push $0         # dummy error code — hardware IRQs never push one
    push $33        # vector number, purely for bookkeeping
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    call irq1_handler

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    add $16, %rsp
    iretq

# IRQ0 is the timer interrupt handler. The PIT fires this on a fixed
# interval; the handler just acknowledges it (see irq0_handler in kernel.c).
.global irq0

irq0:
    push $0
    push $32
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    call irq0_handler

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    add $16, %rsp
    iretq
