#pragma once
#define va_list  __builtin_va_list
#define va_start __builtin_va_start
#define va_end   __builtin_va_end
#define va_arg   __builtin_va_arg

void clear_screen(unsigned char color);
void write_text(unsigned int x, unsigned int y, unsigned char color, const char* fmt, ...);