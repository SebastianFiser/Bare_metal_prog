#include "common.h"
#include "kernel.h"
#include "console.h"

void kernel_main(void) {
    screen_init();
    console_write("Hello, World! This is a simple kernel.\nTesing owerflow function");
    for (int i = 0; i < 100; i++) {
        console_write("Line %d: The quick brown fox jumps over the lazy dog.\n", i);
    }

    for (;;)
        __asm__ volatile ("cli; hlt");
}