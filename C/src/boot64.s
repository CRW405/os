# 64 bit multiboot header
.set MB2_MAGIC,  0xE85250D6                                 # GRUB looks for this magic number
.set MB2_ARCH,   0                                          # architecture field, 0 = i386/protected mode
.set MB2_HDR_LEN, (mb2_header_end - mb2_header_start)       # precompute distance so that things line up
.set MB2_CHECKSUM, -(MB2_MAGIC + MB2_ARCH + MB2_HDR_LEN)    # compute checksum
# checksum is chosen so magic + arch + len + checksum wraps around to 0 - GRUB uses that to check the header

# AT&T s assembly is different than intel asm assembly
# % prefixes registers
# $ prefixed literals
# command source, destination # opposite of intel syntax
# for comments instead of ;
# %eax is a 32 bit register while %ax is the lower 16 bits of the register, %al - 8 bits
# .set - kinda like setting a const
# instruction suffixes (b/w/l/q) pick the operand size when it's not obvious from the registers used

# create the actual header
.section .multiboot, "a"    # "a" flag = allocatable, this section takes up space in the final binary
.align 8                    # multiboot2 requires the header to start on an 8 byte boundary
mb2_header_start:
    .long MB2_MAGIC         # .long emits 4 bytes
    .long MB2_ARCH
    .long MB2_HDR_LEN
    .long MB2_CHECKSUM

    # required end tag, every multiboot2 header must be terminated with a tag of type 0, size 8
    .align 8
    .word 0     # tag type
    .word 0     # tag flags
    .long 8     # tag size (bytes), including these 8 bytes themselves
mb2_header_end:

# reserving memory for the stack and page tables
.section .bss       # .bss holds uninitialized data, none of this takes up space in the binary on disk
.align 4096         # page tables must be page (4096 byte) aligned
p4_table:
    .skip 4096      # .skip reserves n zeroed bytes without writing anything now, one page each
p3_table:
    .skip 4096      # each table is 512 entries * 8 bytes = 4096 bytes
p2_table:
    .skip 4096      # no p1_table - we're mapping with 2MB huge pages, so p2 entries point straight to memory
.align 16           # stack just needs to be aligned, 16 is conventional
stack_bottom:
    .skip 16384     # 16KB of stack space
stack_top:          # the stack grows downward, so esp starts here and moves toward stack_bottom

# Global Descriptor Table
.section .rodata
gdt64:
    .quad 0                 # emits 8 byte value. GDT must start with 8 null bytes
gdt64_code:
# execs | code/data type | present | granularity
# bit 43 (executable), 44 (S: code/data, not a system segment), 47 (present), 55 (granularity)
# NOTE: the long mode bit (L) is actually bit 53, not 55 - worth double checking this entry
    .quad (1<<43) | (1<<44) | (1<< 47) | (1<<55)
gdt64_pointer:
    .word . - gdt64 - 1     # size of the GDT minus 1 (lgdt wants the limit, not the size)
    .quad gdt64             # base address of the GDT

.section .text
.code32                     # assemble as 32 bit instructions, we're still in protected mode at this point
.global _start              # exposes this label to the linker as the entry point
_start:
    mov $stack_top, %esp    # set up the stack, grows down from stack_top

    call check_multiboot
    call check_cpuid
    call check_long_mode

    call setup_page_tables
    call enable_paging

    lgdt (gdt64_pointer)                            # load our GDT into the GDTR
    ljmp $(gdt64_code -gdt64), $long_mode_start     # far jump: selector, offset - reloads %cs, enters 64 bit mode

# GRUB leaves this magic value in %eax on multiboot2 boot, bail if it's missing
check_multiboot:
    cmp $0x36D76289, %eax   # cmp just subtracts and sets flags, doesn't store the result
    jne .halt               # jump if not equal (ZF not set)
    ret                     # pops the return address pushed by call and jumps back to it

# try to flip EFLAGS bit 21 (ID); if it doesn't stick, CPUID isn't supported
check_cpuid:
    pushfl                  # push EFLAGS onto the stack
    pop %eax                # ...and pop it into %eax so we can inspect/modify it
    mov %eax, %ecx          # keep a copy of the original flags in %ecx
    xor $(1<<21), %eax      # flip the ID bit in our copy
    push %eax
    popfl                   # write the modified value back into EFLAGS
    pushfl
    pop %eax                # read EFLAGS back - if the CPU allows toggling ID, our flipped bit survives
    push %ecx
    popfl                   # restore the original EFLAGS from %ecx
    xor %ecx, %eax          # compare original vs modified - non-zero means the bit actually flipped
    jz .halt                # if they're equal, the bit didn't move, so no CPUID support
    ret

# ask CPUID for extended features, check the long mode bit (edx bit 29)
check_long_mode:
    mov $0x80000000, %eax   # cpuid reads %eax as the "leaf" number, selects what info comes back
    cpuid                   # results land in %eax/%ebx/%ecx/%edx depending on the leaf
    cmp $0x80000001, %eax   # extended functions must be available first
    jb .halt                # jump if below - highest supported leaf is less than what we need
    mov $0x80000001, %eax   # leaf 0x80000001 - extended processor info
    cpuid
    test $(1<<29), %edx     # test ands the operands together and sets flags, without storing the result
    jz .halt                # bit 29 of %edx is the long mode flag
    ret

# identity map the first 1GB with 2MB pages: p4 -> p3 -> p2, one p4/p3 entry, 512 p2 entries
setup_page_tables:
    mov $p3_table, %eax
    or $0b11, %eax          # present + writable flags
    mov %eax, (p4_table)    # write the p3 table's address into p4's first (and only) entry

    mov $p2_table, %eax
    or $0b11, %eax
    mov %eax, (p3_table)    # same idea, p2 table's address into p3's first entry

    xor %ecx, %ecx                  # %ecx is our loop counter / p2 entry index, start at 0
.map_p2_table:
    mov $0x200000, %eax             # 2MB, size of one huge page
    mul %ecx                        # unsigned multiply: %eax = %eax * %ecx (edx:eax holds the full result, we only need eax)
    or $0b10000011, %eax            # present | writable | huge page
    mov %eax, p2_table(,%ecx,8)     # symbol + index*scale addressing: p2_table[ecx], each entry is 8 bytes
    inc %ecx
    cmp $512, %ecx                  # 512 entries * 2MB = 1GB mapped
    jne .map_p2_table
    ret

# point cr3 at the page tables, enable PAE, set the long mode bit, then enable paging
enable_paging:
    mov $p4_table, %eax
    mov %eax, %cr3      # cr3 holds the physical address of the top level page table

    mov %cr4, %eax
    or $(1<<5), %eax    # PAE
    mov %eax, %cr4

    mov $0xC0000080, %ecx   # EFER msr, rdmsr/wrmsr use %ecx to pick which msr, edx:eax as the value
    rdmsr                   # read EFER into edx:eax
    or $(1<<8), %eax        # LME, long mode enable
    wrmsr                   # write edx:eax back into EFER

    mov %cr0, %eax
    or $(1<<31), %eax   # PG, enable paging
    mov %eax, %cr0
    ret

.halt:
    hlt         # halts the cpu until the next interrupt
    jmp .halt   # in case an interrupt wakes it up, go straight back to sleep

.code64             # from here on, assemble as 64 bit instructions
long_mode_start:
    xor %ax, %ax    # null out the segment registers, GDT entries aren't used in long mode
    mov %ax, %ss    # (%cs was already reloaded by the far jump that got us here)
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    call kernel_main
    jmp .halt   # kernel_main should never return, but halt if it does
