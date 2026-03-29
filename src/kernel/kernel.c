#include "common.h"
#include "kernel.h"
#include "console.h"

void kernel_main(void) {
    screen_init();
    console_putchar('A');
    console_putchar('B');
    console_putchar('C');

    for (;;)
        __asm__ volatile ("cli; hlt");
}