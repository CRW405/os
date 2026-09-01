# 64 bit multiboot header
.set MB2_MAGIC,  0xE85250D6                                 # GRUB looks for this magic number
.set MB2_ARCH,   0                                          # architecture field, 0 = i386/protected mode
.set MB2_HDR_LEN, (mb2_header_end - mb2_header_start)       # precompute distance so that things line up
.set MB2_CHECKSUM, -(MB2_MAGIC + MB2_ARCH + MB2_HDR_LEN)    # compute checksum

# AT&T s assembly is different than intel asm assembly
# % prefixes registers
# $ prefixed literals
# command source, destination # opposite of intel syntax
# for comments instead of ;
# %eax is a 32 bit register while %ax is the lower 16 bits of the register, %al - 8 bits
# .set - kinda like setting a const

# create the actual header
.section .multiboot, "a"
.align 8
mb2_header_start:
    .long MB2_MAGIC
    .long MB2_ARCH
    .long MB2_HDR_LEN
    .long MB2_CHECKSUM

    # required end tag
    .align 8
    .word 0
    .word 0
    .long 8
mb2_header_end:

# reserving memory for the stack and page tables
.section .bss
.align 4096
p4_table:
    .skip 4096
p3_table:
    .skip 4096
p2_table:
    .skip 4096
.align 16
stack_bottom:
    .skip 16384
stack_top:

# Global Descriptor Table
.section .rodata
gdt64:
    .quad 0             # emits 8 byte value. GDT must start with 8 null bytes
gdt64_code:
# execs | code/data type | present | long mode
    .quad (1<<43) | (1<<44) | (1<< 47) | (1<<53)
    # .quad (1<<43) | (1<<44) | (1<< 47) | (1<<55)
gdt64_pointer:
    .word . - gdt64 - 1
    .quad gdt64

.section .text
.code32
.global _start
_start:
    mov $stack_top, %esp   # set up the stack, grows down from stack_top

    call check_multiboot
    call check_cpuid
    call check_long_mode

    call setup_page_tables
    call enable_paging

    lgdt (gdt64_pointer)
    ljmp $(gdt64_code -gdt64), $long_mode_start   # far jump reloads %cs, enters 64 bit mode

# GRUB leaves this magic value in %eax on multiboot2 boot, bail if it's missing
check_multiboot:
    cmp $0x36D76289, %eax
    jne .halt
    ret

# try to flip EFLAGS bit 21 (ID); if it doesn't stick, CPUID isn't supported
check_cpuid:
    pushfl
    pop %eax
    mov %eax, %ecx
    xor $(1<<21), %eax
    push %eax
    popfl
    pushfl
    pop %eax
    push %ecx
    popfl
    xor %ecx, %eax
    jz .halt
    ret

# ask CPUID for extended features, check the long mode bit (edx bit 29)
check_long_mode:
    mov $0x80000000, %eax
    cpuid
    cmp $0x80000001, %eax   # extended functions must be available first
    jb .halt
    mov $0x80000001, %eax
    cpuid
    test $(1<<29), %edx
    jz .halt
    ret

# identity map the first 1GB with 2MB pages: p4 -> p3 -> p2, one p4/p3 entry, 512 p2 entries
setup_page_tables:
    mov $p3_table, %eax
    or $0b11, %eax
    mov %eax, (p4_table)

    mov $p2_table, %eax
    or $0b11, %eax
    mov %eax, (p3_table)

    xor %ecx, %ecx
.map_p2_table:
    mov $0x200000, %eax   # 2MB, size of one huge page
    mul %ecx               # eax = 2MB * ecx, the physical address of this page
    or $0b10000011, %eax   # present | writable | huge page
    mov %eax, p2_table(,%ecx,8)
    inc %ecx
    cmp $512, %ecx
    jne .map_p2_table
    ret

# point cr3 at the page tables, enable PAE, set the long mode bit, then enable paging
enable_paging:
    mov $p4_table, %eax
    mov %eax, %cr3

    mov %cr4, %eax
    or $(1<<5), %eax   # PAE
    mov %eax, %cr4

    mov $0xC0000080, %ecx   # EFER msr
    rdmsr
    or $(1<<8), %eax        # LME, long mode enable
    wrmsr

    mov %cr0, %eax
    or $(1<<31), %eax   # PG, enable paging
    mov %eax, %cr0
    ret

.halt:
    hlt
    jmp .halt

.code64
long_mode_start:
    xor %ax, %ax   # null out the segment registers, GDT entries aren't used in long mode
    mov %ax, %ss
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    call kernel_main
    jmp .halt
