#pragma once

typedef enum {
    INPUT_EVENT_CHAR,
    INPUT_EVENT_ENTER,
    INPUT_EVENT_BACKSPACE,
    INPUT_EVENT_PAGEUP,
    INPUT_EVENT_PAGEDOWN,
    INPUT_EVENT_UP,
    INPUT_EVENT_DOWN,
    INPUT_EVENT_LEFT,
    INPUT_EVENT_RIGHT
} input_event_type_t;

typedef struct {
    input_event_type_t type;
    char ch;
} input_event_t;

typedef enum {
    MODE_SHELL,
    MODE_EDITOR
} ui_mode_t;

void input_handle_event(const input_event_t *event);
void input_set_mode(ui_mode_t mode);
ui_mode_t input_get_mode(void);
void input_buffers_init(void);
