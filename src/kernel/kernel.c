#include "common.h"
#include "kernel.h"

void kernel_main(void) {
    clear_screen(0x0F);
    write_text(0, 0, 0x0F, "10 / 5 = %d", 10 / 5);
    write_text(0, 1, 0x0F, "1 + 1 = %d", 1 + 1);
    write_text(0, 2, 0x0F, "This idea was supported by HackClub! \n");
    write_text(0, 3, 0x0F, "Right now i cant do anything, but my guy is working on it!");
    PANIC("Booted");

    for (;;)
        __asm__ volatile ("cli; hlt");
}