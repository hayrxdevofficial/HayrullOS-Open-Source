[BITS 16]
[ORG 0x7C00]

SECTORS_PER_TRACK equ 18
HEADS             equ 2

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl
    sti

    mov si, msg_loading
    call print_string

    mov word [current_lba], 1
    mov word [remaining], 128
    mov ax, 0x1000
    mov es, ax
    xor bx, bx

read_loop:
    cmp word [remaining], 0
    je read_done

    mov ax, [remaining]
    cmp ax, 55
    jbe .chunk_ok
    mov ax, 55
.chunk_ok:
    mov [chunk_size], ax

    mov ax, [current_lba]
    call lba_to_chs

    mov al, [chunk_size]
    mov ah, 0x02
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov ax, [chunk_size]
    mov cl, 9
    shl ax, cl
    add bx, ax
    jnc .no_carry
    mov ax, es
    add ax, 0x1000
    mov es, ax
.no_carry:

    mov ax, [chunk_size]
    add [current_lba], ax
    sub [remaining], ax
    jmp read_loop

read_done:
    xor ax, ax
    mov es, ax

    mov si, msg_read_ok
    call print_string

    mov ax, 0x0013
    int 0x10

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode

lba_to_chs:
    xor dx, dx
    mov cx, SECTORS_PER_TRACK
    div cx
    inc dx
    mov bl, dl
    xor dx, dx
    mov cx, HEADS
    div cx
    mov ch, al
    mov dh, dl
    mov cl, bl
    ret

disk_error:
    mov si, msg_error
    call print_string
    hlt

print_string:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print_string
.done:
    ret

[BITS 32]
protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    jmp 0x10000

[BITS 16]
boot_drive:   db 0
current_lba:  dw 0
remaining:    dw 0
chunk_size:   dw 0

msg_loading: db "HayrullOS loading...", 13, 10, 0
msg_read_ok: db "Kernel read OK.", 13, 10, 0
msg_error:   db "Disk error!", 13, 10, 0

gdt_start:
    dq 0x0
gdt_code:
    dw 0xFFFF, 0x0
    db 0x0, 10011010b, 11001111b, 0x0
gdt_data:
    dw 0xFFFF, 0x0
    db 0x0, 10010010b, 11001111b, 0x0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

times 510 - ($ - $$) db 0
dw 0xAA55
