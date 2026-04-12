#pragma once

#include "common.h"

void fb_init(uint32_t multiboot_info_ptr);
bool fb_is_available(void);
uint32_t fb_get_width(void);
uint32_t fb_get_height(void);
uint32_t fb_get_pitch(void);
uint32_t fb_get_address(void);

typedef struct {
    uint32_t* address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bpp;
} framebuffer_t;

void fb_clear(uint32_t color);
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void fb_blit(uint32_t dst_x, uint32_t dst_y, uint32_t src_x, uint32_t src_y, uint32_t width, uint32_t height);
void fb_present();

#define FB_FONT_WIDTH 8
#define FB_FONT_HEIGHT 8

/* Visual cell size used by the console grid when rendering 8x8 glyphs. */
#define FB_CELL_WIDTH 11
#define FB_CELL_HEIGHT 16

/* Letter bitmaps and text rendering for the linear framebuffer. */
const uint8_t* fb_get_glyph(char c);
void fb_draw_char_cell(uint32_t x, uint32_t y, uint32_t cell_width, uint32_t cell_height, char c, uint32_t fg_color, uint32_t bg_color);
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg_color, uint32_t bg_color);
void fb_draw_text(uint32_t x, uint32_t y, const char* text, uint32_t fg_color, uint32_t bg_color);
