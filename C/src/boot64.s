# =====================================================================
# Multiboot2 header
#
# Same idea as the multiboot1 header in boot32.s — GRUB scans for this
# magic number near the start of the image to recognize a bootable
# kernel — but multiboot2's layout is tag-based instead of fixed-field,
# so it needs an explicit end tag and an 8-byte alignment.
# =====================================================================

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

# =====================================================================
# BSS reservations: page tables and stack
#
# Long mode requires paging to be enabled, so before we can even get
# there we need memory for a page table hierarchy (p4 -> p3 -> p2) and
# a stack. All zero-initialized, so it lives in .bss rather than
# taking up space in the binary on disk.
# =====================================================================

.section .bss
.align 4
.global multiboot_info    # exported so kernel.c can read the saved pointer later,
                           # %ebx itself gets clobbered long before the shell exists
multiboot_magic:
    .skip 4
multiboot_info:
    .skip 4

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

# =====================================================================
# Global Descriptor Table
#
# Long mode's paging does the real memory protection, so segmentation
# is basically turned off — but the CPU still requires a GDT with at
# least a valid code segment to far-jump into 64-bit mode and set %cs.
# =====================================================================

.section .rodata
gdt64:
    .quad 0             # emits 8 byte value. GDT must start with 8 null bytes
gdt64_code:
# execs | code/data type | present | long mode
    .quad (1<<43) | (1<<44) | (1<< 47) | (1<<53)
gdt64_pointer:
    .word . - gdt64 - 1
    .quad gdt64

# =====================================================================
# Entry point: 32-bit setup before the jump to long mode
#
# GRUB drops us here in 32-bit protected mode. Before we can run 64-bit
# code we have to: confirm the CPU actually supports what we need
# (multiboot handoff, CPUID, long mode), build identity-mapped page
# tables, flip on paging, then load the GDT and far-jump into 64-bit
# mode.
# =====================================================================

.section .text
.code32
.global _start
_start:
    mov $stack_top, %esp   # set up the stack, grows down from stack_top

    mov %eax, multiboot_magic
    mov %ebx, multiboot_info

    call check_multiboot
    call check_cpuid
    call check_long_mode

    call setup_page_tables
    call enable_paging

    lgdt (gdt64_pointer)
    ljmp $(gdt64_code -gdt64), $long_mode_start   # far jump reloads %cs, enters 64 bit mode

# GRUB leaves this magic value in %eax on multiboot2 boot, bail if it's missing
check_multiboot:
    cmp $0x36D76289, multiboot_magic
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

# =====================================================================
# 64-bit entry point
#
# We land here after the far jump reloads %cs with the 64-bit code
# segment. The other segment registers still hold stale 32-bit
# selectors; since segmentation isn't really used in long mode, they're
# just zeroed out rather than reloaded from the GDT.
# =====================================================================

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
