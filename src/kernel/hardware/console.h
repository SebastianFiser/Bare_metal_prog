#pragma once

#include "common.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile unsigned short*)0xB8000)

#define CONSOLE_MAX_COLS 160
#define CONSOLE_MAX_ROWS 64

#define HISTORY_LINES 512
#define HISTORY_COLS CONSOLE_MAX_COLS

#define CONSOLE_COLOR_DEFAULT 0x0F
#define CONSOLE_COLOR_ERROR   0x0C
#define CONSOLE_COLOR_DIR     0x0B
#define CONSOLE_COLOR_INFO    0x0A

void clear_screen(unsigned char color);
void screen_init(void);
void console_putchar(char c);
void console_write(const char* fmt, ...);
void console_redraw_view(void);
void console_draw_cursor(void);
void console_cursor_left(void);
void console_cursor_right(void);
void console_print_dec(int num);
void console_scroll_up(void);
void console_scroll_down(void);
void console_set_color(unsigned char color);
unsigned char console_get_color(void);
void console_write_colored(unsigned char color, const char* fmt, ...);
void console_write_ascii(const char* name);
void write_text(unsigned int x, unsigned int y, unsigned char color, const char* fmt, ...);

typedef struct {
    unsigned int cursor_x;
    unsigned int cursor_y;
    unsigned char current_color;

    unsigned int hist_write_line;
    unsigned int hist_write_col;
    unsigned int hist_line_count;
    unsigned int scroll_lines_from_bottom;
    unsigned int view_top_line;
    bool follow_bottom;

    char history[HISTORY_LINES][HISTORY_COLS];
    unsigned char history_color[HISTORY_LINES][HISTORY_COLS];
} console_state_t;

void console_save_state(console_state_t* state);
void console_restore_state(const console_state_t* state);