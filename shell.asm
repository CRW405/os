[bits 16]
[org 0x7c00]

; set up registers and stack
start:
    cli
    xor ax, ax          ; zero ax, we use ax to zero the other registers
    mov ds, ax          ; data
    mov es, ax          ; extra
    mov ss, ax          ; stack
    mov sp, 0x7c00      ; stack pointer
    sti
    call main

; main
main:
    mov ax, hello        ; push the hello message onto the stack for print
    push ax
    call print
    add sp, 2            ; pop message(2 bytes) off the stack

    call shell

; halt loop to pause cpu when no work to be done
hang:
    cli
.loop:
    hlt
    jmp .loop

; print an arg
print:
    ; create local buffer
    push bp             ; save base pointer
    mov bp, sp          ; set new base pointer

    mov si, [bp+4]      ; get string addres from stack, bp is 2 bytes, so +4 brings us to our arg
.print_loop:
    lodsb               ; load string byte, autoincrements

    or al, al           ; check if null term
    jz .done            ; if so, we're done

    mov ah, 0x0E        ; BIOS teletype
    int 0x10            ; BIOS video interrupt

    jmp .print_loop     ; loop
.done:
    pop bp              ; destroy local buffer
    ret                 ; return

; shell
shell:
    push bp
    mov bp, sp

    sub sp, 32          ; 32 byte input buffer
    mov di, bp          ; di points to local buffer
    sub di, 32          ; move to start of buffer

    xor al, al
    xor ax, ax
    mov cx, 16          ; 16 words in 32 bytes
    rep stosw           ; fill with zero, "repeat" store ax from es to di then increment di by 2
.read_keypress:
    mov ah, 0x00        ; BIOS get keypress
    int 0x16            ; returns ASCII in al

    cmp al, 0x0D
    je .handle_enter

    cmp al, 0x08
    je .handle_backspace

    cmp cx, 31          ; ignore possible overflow
    jge .read_keypress

    ; store char in buffer
    mov bx, bp
    sub bx, 32          ; start of buffer
    add bx, cx          ; add current length
    mov [bx], al        ; store char

    inc cx              ; increment length

    ; echo to screen
    mov ah, 0x0E
    int 0x10
    jmp .read_keypress
.handle_backspace:
    ; dont backspace before buffer
    cmp cx, 0
    jbe .read_keypress

    ; "backspace" "space" "backspace". eg move cursor back and "clear" char
    dec cx
    mov ah, 0x0E
    mov al, 0x08
    int 0x10
    mov al, ' '
    int 0x10
    mov al, 0x08
    int 0x10

    jmp .read_keypress
.handle_enter:
    mov bx, bp
    sub bx, 32
    add bx, cx
    mov byte [bx], 0    ; null terminate

    ; newline
    mov ah, 0x0E
    mov al, 0x0D
    int 0x10
    mov al, 0x0A
    int 0x10

    ; process command here, pass (bp-32)
    mov bx, command_table
.dispatch_loop:
    mov ax, [bx]         ; command name pointer (0 marks end of table)
    or ax, ax
    jz .command_done     ; no match - unknown command, do nothing

    mov si, bp
    sub si, 32           ; si -> start of input buffer (the command line)
    mov di, ax           ; di -> candidate command name
    call match_command
    jnc .dispatch_call   ; matched - si now points at the argument

    add bx, 4            ; next {name, handler} table entry
    jmp .dispatch_loop
.dispatch_call:
    mov ax, [bx+2]       ; handler address
    push si              ; arg for the handler (points into the input buffer)
    call ax
    add sp, 2
.command_done:
    mov sp, bp
    pop bp
    jmp shell

; compare the input buffer (si) against a null-terminated command name (di)
; on match:    si -> start of the argument (or its trailing null), CF clear
; on mismatch: CF set
match_command:
.compare_loop:
    mov al, [di]
    or al, al
    jz .name_matched     ; reached the end of the command name
    cmp al, [si]
    jne .mismatch
    inc si
    inc di
    jmp .compare_loop
.name_matched:
    ; si is right after the matched name in the input - must be a space or end of line
    cmp byte [si], ' '
    je .has_arg
    cmp byte [si], 0
    jne .mismatch        ; e.g. "echoes" typed against "echo"
    clc
    ret
.has_arg:
    inc si               ; skip the space, si -> argument
    clc
    ret
.mismatch:
    stc
    ret

; echo <arg> - print the argument back out, followed by a newline
; expects: [bp+4] = pointer to the (possibly empty) argument string
cmd_echo:
    push bp
    mov bp, sp

    mov ax, [bp+4]
    push ax
    call print
    add sp, 2

    mov ah, 0x0E
    mov al, 0x0D
    int 0x10
    mov al, 0x0A
    int 0x10

    pop bp
    ret

; {name pointer, handler pointer} pairs, terminated by a null name pointer
command_table:
    dw echo_cmd, cmd_echo
    dw 0

echo_cmd:
    db "echo", 0

; define string data
hello:
    db "Hello, World!", 0x0D, 0x0A, 0

times 510-($-$$) db 0
dw 0xAA55
