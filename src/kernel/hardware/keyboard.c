#include "kernel.h"
#include "keymaps.h"
#include "input.h"

#define KBD_EVENT_QUEUE_SIZE 64

static input_event_t event_queue[KBD_EVENT_QUEUE_SIZE];
static unsigned int event_head = 0;
static unsigned int event_tail = 0;
static unsigned int event_count = 0;
static int extended = 0;
static int shift_pressed = 0;
static int ctrl_pressed = 0;

#define SCANCODE_CTRL_PRESS 0x1D
#define SCANCODE_CTRL_RELEASE 0x9D

static void keyboard_push_event(input_event_type_t type, char ch) {
    if (event_count >= KBD_EVENT_QUEUE_SIZE) {
        return;
    }

    event_queue[event_tail].type = type;
    event_queue[event_tail].ch = ch;
    event_tail = (event_tail + 1) % KBD_EVENT_QUEUE_SIZE;
    event_count++;
}

static int keyboard_pop_event(input_event_t *event) {
    if (event_count == 0) {
        return 0;
    }

    *event = event_queue[event_head];
    event_head = (event_head + 1) % KBD_EVENT_QUEUE_SIZE;
    event_count--;
    return 1;
}

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

    if (scancode == 0xE0) {
        extended = 1;
        goto send_eoi;
    }

    if (extended) {
        extended = 0;

        if (scancode == SCANCODE_CTRL_PRESS) {
            ctrl_pressed = 1;
            goto send_eoi;
        }

        if (scancode == SCANCODE_CTRL_RELEASE) {
            ctrl_pressed = 0;
            goto send_eoi;
        }

        if (scancode & 0x80) {
            goto send_eoi;
        }

        if (scancode == 0x48) { // Up arrow
            keyboard_push_event(INPUT_EVENT_UP, 0);
            goto send_eoi;
        }

        if (scancode == 0x50) { // Down arrow
            keyboard_push_event(INPUT_EVENT_DOWN, 0);
            goto send_eoi;
        }

        if (scancode == 0x4B) { // Left arrow
            keyboard_push_event(INPUT_EVENT_LEFT, 0);
            goto send_eoi;
        }

        if (scancode == 0x4D) { // Right arrow
            keyboard_push_event(INPUT_EVENT_RIGHT, 0);
            goto send_eoi;
        }

        if (scancode == 0x49) { // PageUp
            keyboard_push_event(INPUT_EVENT_PAGEUP, 0);
            goto send_eoi;
        }

        if (scancode == 0x51) { // PageDown
            keyboard_push_event(INPUT_EVENT_PAGEDOWN, 0);
            goto send_eoi;
        }
        goto send_eoi;
    }

    /* Handle shift key press/release */
    if (scancode == SCANCODE_LSHIFT || scancode == SCANCODE_RSHIFT) {
        shift_pressed = 1;
        goto send_eoi;
    }

    if (scancode == SCANCODE_LSHIFT_RELEASE || scancode == SCANCODE_RSHIFT_RELEASE) {
        shift_pressed = 0;
        goto send_eoi;
    }

    if (scancode == SCANCODE_CTRL_PRESS) {
        ctrl_pressed = 1;
        goto send_eoi;
    }

    if (scancode == SCANCODE_CTRL_RELEASE) {
        ctrl_pressed = 0;
        goto send_eoi;
    }

    if (scancode & 0x80) {
        goto send_eoi;
    }

    if (scancode == 0x0E) {
        keyboard_push_event(INPUT_EVENT_BACKSPACE, 0);
        goto send_eoi;
    }

    if (scancode == 0x1C) { // Enter key
        keyboard_push_event(INPUT_EVENT_ENTER, 0);
        goto send_eoi;
    }

    if (scancode == 0x48) { // Up arrow
        keyboard_push_event(INPUT_EVENT_UP, 0);
        goto send_eoi;
    }

    if (scancode == 0x50) { // Down arrow
        keyboard_push_event(INPUT_EVENT_DOWN, 0);
        goto send_eoi;
    }

    if (scancode == 0x49) { // PageUp
        keyboard_push_event(INPUT_EVENT_PAGEUP, 0);
        goto send_eoi;
    }

    if (scancode == 0x51) { // PageDown
        keyboard_push_event(INPUT_EVENT_PAGEDOWN, 0);
        goto send_eoi;
    }

    /* Use appropriate map based on shift state */
    char ch = 0;
    if (shift_pressed) {
        ch = scancode_map_shift[scancode];
    } else {
        ch = scancode_map[scancode];
    }

    if (ctrl_pressed && ch >= 'A' && ch <= 'Z') {
        ch = (char)(ch - 'A' + 1);
    } else if (ctrl_pressed && ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - 'a' + 1);
    }
    
    if (ch != 0) {
        keyboard_push_event(INPUT_EVENT_CHAR, ch);
    }

    send_eoi:
        if (regs->int_no >= 40) outb(0xA0, 0x20); // Send reset signal to slave PIC
        outb(0x20, 0x20); // Send reset signal to master PIC
}

void keyboard_poll(void) {
    input_event_t event;

    while (keyboard_pop_event(&event)) {
        input_handle_event(&event);
    }
}