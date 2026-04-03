#include "console.h"
#include "shell.h"
#include "input.h"
#include "meowim.h"

#define LINE_MAX 128

static char line_buffer[LINE_MAX];
static unsigned int line_len = 0;
static unsigned int line_cursor = 0;
static ui_mode_t current_mode = MODE_SHELL;

void input_set_mode(ui_mode_t mode) {
    current_mode = mode;
}

ui_mode_t input_get_mode(void) {
    return current_mode;
}

void input_handle_event(const input_event_t *event) {
    if (current_mode == MODE_EDITOR) {
        editor_input(event);
        return;
    }

    if (event->type == INPUT_EVENT_CHAR) {
        if (line_len >= LINE_MAX - 1) return;

        if (line_cursor == line_len) {
            line_buffer[line_len++] = event->ch;
            line_cursor = line_len;
            console_putchar(event->ch);
            return;
        }

        for (unsigned int i = line_len; i > line_cursor; i--) {
            line_buffer[i] = line_buffer[i - 1];
        }
        line_buffer[line_cursor] = event->ch;
        line_len++;
        line_cursor++;

        for (unsigned int i = line_cursor - 1; i < line_len; i++) {
            console_putchar(line_buffer[i]);
        }

        unsigned int back = line_len - line_cursor;
        for (unsigned int i = 0; i < back; i++) {
            console_cursor_left();
        }
        return;
    }

    if (event->type == INPUT_EVENT_BACKSPACE) {
        if (line_cursor == 0) return;

        line_cursor--;

        for (unsigned int i = line_cursor; i + 1 < line_len; i++) {
            line_buffer[i] = line_buffer[i + 1];
        }
        line_len--;
        line_buffer[line_len] = '\0';

        console_cursor_left();
        for (unsigned int i = line_cursor; i < line_len; i++) {
            console_putchar(line_buffer[i]);
        }
        console_putchar(' '); // Clear last char

        unsigned int back = (line_len - line_cursor) + 1;
        for (unsigned int i = 0; i < back; i++) {
            console_cursor_left();
        }
        return;
    }
    

    if (event->type == INPUT_EVENT_ENTER) {
        console_write("\n");
        line_buffer[line_len] = '\0';
        shell_execute_command(line_buffer);
        line_len = 0;
        line_cursor = 0;
        return;
    }

    if (event->type == INPUT_EVENT_PAGEUP) {
        console_scroll_up();
        return;
    }

    if (event->type == INPUT_EVENT_PAGEDOWN) {
        console_scroll_down();
        return;
    }

    if (event->type == INPUT_EVENT_LEFT) {
        if (line_cursor > 0) {
            console_cursor_left();
            line_cursor--;
        }
        return;
    }

     if (event->type == INPUT_EVENT_RIGHT) {
        if (line_cursor < line_len) {
            console_cursor_right();
            line_cursor++;
        }
     }
}
