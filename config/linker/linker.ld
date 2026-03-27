ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5); /* R+X */
    data PT_LOAD FLAGS(6); /* R+W */
}

SECTIONS
{
    . = 1M;

    /* MUST be first so GRUB finds it */
    .multiboot : {
        *(.multiboot)
    } :text

    .text : {
        *(.text.boot)
        *(.text*)
    } :text

    .rodata : {
        *(.rodata*)
    } :text

    .data : {
        *(.data*)
    } :data

    .bss : {
        stack_bottom = .;
        . = . + 16384;
        stack_top = .;

        *(.bss*)
        *(COMMON)
    } :data
}