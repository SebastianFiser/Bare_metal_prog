BITS 32

section .multiboot
align 8

header_start:
    dd 0xE85250D6
    dd 0
    dd header_end - header_start
    dd -(0xE85250D6 + 0 + (header_end - header_start))

    dd 0
    dd 0
header_end:


section .text
global _start
extern kernel_main

_start:
    cli

    mov esp, stack_top

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