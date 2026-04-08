#pragma once

#include "common.h"

void fb_init(uint32_t multiboot_info_ptr);

typedef struct {
    uint32_t* address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bpp;
} framebuffer_t;