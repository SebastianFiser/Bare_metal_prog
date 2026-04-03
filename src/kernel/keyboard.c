#include "kernel.h"
#include "console.h"
#include "keymaps.h"
#include "shell.h"

typedef enum {
    INPUT_EVENT_CHAR,
    INPUT_EVENT_ENTER,
    INPUT_EVENT_BACKSPACE,
    INPUT_EVENT_PAGEUP,
    INPUT_EVENT_PAGEDOWN,
} input_event_type_t;

typedef struct {
    input_event_type_t type;
    char ch;
} input_event_t;

#define LINE_MAX 128
#define KBD_EVENT_QUEUE_SIZE 64

static input_event_t event_queue[KBD_EVENT_QUEUE_SIZE];
static unsigned int event_head = 0;
static unsigned int event_tail = 0;
static unsigned int event_count = 0;

static char line_buffer[LINE_MAX];
static unsigned int line_len = 0;

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

    if (scancode == 0x49) { // PageUp
        keyboard_push_event(INPUT_EVENT_PAGEUP, 0);
        goto send_eoi;
    }

    if (scancode == 0x51) { // PageDown
        keyboard_push_event(INPUT_EVENT_PAGEDOWN, 0);
        goto send_eoi;
    }

    char ch = scancode_map[scancode];
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
        if (event.type == INPUT_EVENT_CHAR) {
            if (line_len < LINE_MAX - 1) {
                line_buffer[line_len++] = event.ch;
                console_putchar(event.ch);
            }
            continue;
        }

        if (event.type == INPUT_EVENT_BACKSPACE) {
            if (line_len > 0) {
                line_len--;
                line_buffer[line_len] = '\0';
                console_write("\b \b");
            }
            continue;
        }

        if (event.type == INPUT_EVENT_ENTER) {
            console_write("\n");
            line_buffer[line_len] = '\0';
            shell_execute_command(line_buffer);
            line_len = 0;
            continue;
        }

        if (event.type == INPUT_EVENT_PAGEUP) {
            console_scroll_up();
            continue;
        }

        if (event.type == INPUT_EVENT_PAGEDOWN) {
            console_scroll_down();
            continue;
        }
    }
}