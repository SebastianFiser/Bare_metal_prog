BITS 32

section .multiboot
align 8

header_start:
    dd 0xE85250D6
    dd 0
    dd header_end - header_start
    dd -(0xE85250D6 + 0 + (header_end - header_start))

    dw 5            ; framebuffer request tag
    dw 0
    dd 20
    dd 0            ; width = any
    dd 0            ; height = any
    dd 32           ; depth = 32 bpp

    align 8

    dw 0            ; end tag type
    dw 0            ; end tag flags
    dd 8            ; end tag size
header_end:


section .text
global _start
extern kernel_main

_start:
    cli

    mov esp, stack_top
    push ebx
    push eax
    call kernel_main

.hang:
    hlt
    jmp .hang


section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits