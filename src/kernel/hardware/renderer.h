#pragma once

#include "common.h"

typedef enum {
    RENDER_VGA = 0,
    RENDER_FB = 1
} render_mode_t;

extern render_mode_t g_render_mode;

render_mode_t renderer_get_mode(void);
void renderer_set_mode(render_mode_t mode);
