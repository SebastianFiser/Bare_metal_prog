#include "kernel.h"
#include "console.h"

static const char scancode_map[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm',
    [0x39] = ' ', [0x1C] = '\n'
};

void pic_remap(void) {
    unsigned char a1 = inb(0x21);
    unsigned char a2 = inb(0xA1);

    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait(); // master offset 32
    outb(0xA1, 0x28); io_wait(); // slave offset 40
    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();

    // dočasně: jen klávesnice
    outb(0x21, 0xFD); io_wait();
    outb(0xA1, 0xFF); io_wait();
}

void keyboard_handler(struct registers *regs) {
    unsigned char scancode = inb(0x60);

    // Ignore key release events (break codes).
    if (scancode < 0x80) {
        char ch = scancode_map[scancode];
        if (ch) {
            char out[2] = { ch, '\0' };
            console_write("%s\n", out);
        }
    }

    if (regs->int_no >= 40) outb(0xA0, 0x20); // Send reset signal to slave PIC
    outb(0x20, 0x20); // Send reset signal to master PIC
}