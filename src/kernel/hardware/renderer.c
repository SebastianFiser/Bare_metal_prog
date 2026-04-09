#include "renderer.h"

render_mode_t g_render_mode = RENDER_VGA;

render_mode_t renderer_get_mode(void) {
    return g_render_mode;
}

void renderer_set_mode(render_mode_t mode) {
    g_render_mode = mode;
}
