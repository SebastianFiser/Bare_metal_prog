#pragma once

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile unsigned short*)0xB8000)

void clear_screen(unsigned char color);
void screen_init(void);
void console_putchar(char c);
static void scroll(void);
void write_text(unsigned int x, unsigned int y, unsigned char color, const char* fmt, ...);