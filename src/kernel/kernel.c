#include "common.h"
#include "kernel.h"
#include "console.h"

void kernel_main(void) {
    screen_init();
    console_write("Hello, World! This is a simple kernel.\nTesting overflow function\n");
    PANIC("booted");
    for (;;)
        __asm__ volatile ("cli; hlt");
}