#pragma once

#include "input.h"

void meowim_open(void);
void meowim_open_file(const char *filename);
void meowim_close(void);
int meowim_is_active(void);
void editor_input(const input_event_t *event);
void editor_buffers_init(void);