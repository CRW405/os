[bits 16]           ; 16 bit encoding
[org 0x7c00]        ; offset to where BIOS loads the boot sector

start:
    xor ax, ax      ; 0 the register
    mov ds, ax      ; zero the rest
    mov es, ax
    mov si, msg     ; load index with offset of string

print_loop:
    lodsb           ; load string byte - loads si into al and increments si
    or al, al       ; check if null (terminator)
    jz hang         ; jump if zero - if zero flag set and end of string, jump to hang

    mov ah, 0x0E    ; BIOS teletype command
    int 0x10        ; BIOS video interrupt
    jmp print_loop  ; repeat for next char

hang:
    cli             ; clear interrupt
.loop:
    hlt             ; halt until interrupt
    jmp .loop

msg:
    db "Goodbye. Space?", 0x0D, 0x0A, 0
                    ; 0x0D - Carriage Return
                    ; 0x0A - Linefeed
                    ; 0    - null (terminator)

times 510-($-$$) db 0
                    ; pad sector to 510 bytes
dw 0xAA55           ; Boot Signature

