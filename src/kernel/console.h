#pragma once

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile unsigned short*)0xB8000)

void clear_screen(unsigned char color);
void screen_init(void);
void console_putchar(char c);
void console_write(const char* fmt, ...);
void console_redraw_view(void);
void console_draw_cursor(void);
void console_scroll_up(void);
void console_scroll_down(void);
void write_text(unsigned int x, unsigned int y, unsigned char color, const char* fmt, ...);