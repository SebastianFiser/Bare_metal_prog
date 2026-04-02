#include "kernel.h"
#include "console.h"
#include "keymaps.h"
#include "shell.h"

#define LINE_MAX 128
static char line_buffer[LINE_MAX];
static unsigned int line_len = 0;

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

    if (scancode & 0x80) {
        goto send_eoi;
    }

    if (scancode == 0x0E) {
        if (line_len > 0) {
            line_len--;
            line_buffer[line_len] = '\0';
            console_write("\b \b"); // Move back, print space, move back again
        }
        goto send_eoi;
    }

    if (scancode == 0x1C) { // Enter key
        console_write("\n");
        line_buffer[line_len] = '\0'; // Null-terminate the string
        shell_execute_command(line_buffer);
        line_len = 0; // Reset buffer for next input
        goto send_eoi;
    }

    if (scancode == 0x49) { // PageUp
        console_scroll_up();
        goto send_eoi;
    }

    if (scancode == 0x51) { // PageDown
        console_scroll_down();
        goto send_eoi;
    }

    char ch = scancode_map[scancode];
    if (ch != 0) {
        if (line_len < LINE_MAX - 1) {
            line_buffer[line_len++] = ch;
            console_putchar(ch);   // vypisuje znaky vedle sebe
            }
    }

    send_eoi:
        if (regs->int_no >= 40) outb(0xA0, 0x20); // Send reset signal to slave PIC
        outb(0x20, 0x20); // Send reset signal to master PIC
}