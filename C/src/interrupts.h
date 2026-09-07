#pragma once

// full definition lives in interrupts.c — only isr.s needs to call these,
// and it doesn't care about the struct's contents
struct registers;

void isr_common_handler(struct registers *regs);
void irq0_handler(void);
