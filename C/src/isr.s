.section .text
.global isr0

# =====================================================================
# ISR/IRQ stubs
#
# The CPU can only jump straight to raw code on an interrupt, not call
# a C function directly — it doesn't know the C calling convention and
# won't save registers for you. Each stub here does the low-level part:
# push a fake error code if the CPU didn't push one, save every
# general-purpose register, call the matching C `*_handler()`, restore
# the registers, and `iretq` back to whatever was interrupted.
# =====================================================================

# divide by zero has no cpu error code so we fake our own
isr0:
    push $0
    push $0
    jmp isr_common_stub

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
    call isr_handler # c func
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
