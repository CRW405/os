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
    ; jz hang       ; jump if zero - if zero flag set and end of string, jump to hang
    jz shell_loop   ; start echo shell

    mov ah, 0x0E    ; BIOS teletype command
    int 0x10        ; BIOS video interrupt
    jmp print_loop  ; repeat for next char

shell_loop:
    mov ah, 0x00    ; BIOS get keypress
    int 0x16        ; returns ASCII in al
    cmp al, 0x0D    ; detect enter press
    je newline
    mov ah, 0x0E    ; print character back out
    int 0x10
    jmp shell_loop

newline:
    mov ah, 0x0E
    mov al, 0x0D
    int 0x10
    mov al, 0x0A
    int 0x10
    jmp shell_loop

hang:
    cli             ; clear interrupt
.loop:
    hlt             ; halt until interrupt
    jmp .loop

msg:
    db "Hello, World!", 0x0D, 0x0A, 0
                    ; 0x0D - Carriage Return
                    ; 0x0A - Linefeed
                    ; 0    - null (terminator)

times 510-($-$$) db 0
                    ; pad sector to 510 bytes
dw 0xAA55           ; Boot Signature

