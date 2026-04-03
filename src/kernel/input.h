#pragma once

typedef enum {
    INPUT_EVENT_CHAR,
    INPUT_EVENT_ENTER,
    INPUT_EVENT_BACKSPACE,
    INPUT_EVENT_PAGEUP,
    INPUT_EVENT_PAGEDOWN,
    INPUT_EVENT_LEFT,
    INPUT_EVENT_RIGHT
} input_event_type_t;

typedef struct {
    input_event_type_t type;
    char ch;
} input_event_t;

void input_handle_event(const input_event_t *event);
