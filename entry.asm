[BITS 32]
section .text
global _start
extern _kernel_main

_start:
    mov edi, 0x14000
    mov ecx, 0x2000
    xor eax, eax
    rep stosb

    call _kernel_main
.hang:
    cli
    hlt
    jmp .hang
