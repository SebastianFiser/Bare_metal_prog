#include "common.h"
#include "console.h"

static unsigned int cursor_x = 0;
static unsigned int cursor_y = 0;
static unsigned char current_color = 0x0F; 

#define HISTORY_LINES 512
#define HISTORY_COLS VGA_WIDTH

static char history[HISTORY_LINES][HISTORY_COLS];
static unsigned int hist_write_line = 0;
static unsigned int hist_write_col = 0;
static unsigned int hist_line_count = 1;
static unsigned int scroll_lines_from_bottom = 0;
static unsigned int view_top_line = 0;
static bool follow_bottom = true;

static unsigned int get_bottom_top_line(void) {
    if (hist_line_count <= VGA_HEIGHT) {
        return 0;
    } else if (hist_line_count < HISTORY_LINES) {
        return hist_line_count - VGA_HEIGHT;
    } else {
        return (hist_write_line + HISTORY_LINES - VGA_HEIGHT) % HISTORY_LINES;
    }
}

static void apply_scroll_position(void) {
    unsigned int bottom_top = get_bottom_top_line();

    if (hist_line_count < HISTORY_LINES) {
        unsigned int max_scroll = (hist_line_count > VGA_HEIGHT) ? (hist_line_count - VGA_HEIGHT) : 0;
        if (scroll_lines_from_bottom > max_scroll) {
            scroll_lines_from_bottom = max_scroll;
        }
        view_top_line = bottom_top - scroll_lines_from_bottom;
        return;
    }
    view_top_line = (bottom_top + HISTORY_LINES - (scroll_lines_from_bottom % HISTORY_LINES)) % HISTORY_LINES;
}

static void clear_history_line(unsigned int line) {
    for (unsigned int x = 0; x < HISTORY_COLS; x++) {
        history[line][x] = ' ';
    }
}

static void history_new_line(void) {
    hist_write_col = 0;
    hist_write_line = (hist_write_line + 1) % HISTORY_LINES;
    clear_history_line(hist_write_line);

    if (hist_line_count < HISTORY_LINES) {
        hist_line_count++;
    }
}

static void update_view_to_bottom(void) {
    if (!follow_bottom) {
        return;
    }

    if (hist_line_count <= VGA_HEIGHT) {
        view_top_line = 0;
    } else if (hist_line_count < HISTORY_LINES) {
        view_top_line = hist_line_count - VGA_HEIGHT;
    } else {
        view_top_line = (hist_write_line + HISTORY_LINES - (VGA_HEIGHT - 1)) % HISTORY_LINES;
    }
}

static void vga_putc_at(unsigned int *cx, unsigned int *cy, unsigned char color, char c) {
    if (*cy >= VGA_HEIGHT) return;

    if (c == '\n') {
        *cx = 0;
        (*cy)++;
        return;
    }

    if (*cx >= VGA_WIDTH) {
        *cx = 0;
        (*cy)++;
        if (*cy >= VGA_HEIGHT) return;
    }

    unsigned int index = (*cy) * VGA_WIDTH + (*cx);
    VGA_MEMORY[index] = ((unsigned short)color << 8) | (unsigned char)c;
    (*cx)++;
}

static void vga_print_dec(unsigned int *cx, unsigned int *cy, unsigned char color, int value) {
    unsigned int magnitude;
    if (value < 0) {
        vga_putc_at(cx, cy, color, '-');
        magnitude = (unsigned int)(-value);
    } else {
        magnitude = (unsigned int)value;
    }

    unsigned int divisor = 1;
    while (magnitude / divisor > 9) divisor *= 10;

    do {
        vga_putc_at(cx, cy, color, (char)('0' + (magnitude / divisor)));
        magnitude %= divisor;
        divisor /= 10;
    } while (divisor > 0);
}

static void vga_print_hex(unsigned int *cx, unsigned int *cy, unsigned char color, unsigned int value) {
    for (int i = 7; i >= 0; i--) {
        unsigned int nibble = (value >> (i * 4)) & 0xF;
        vga_putc_at(cx, cy, color, "0123456789abcdef"[nibble]);
    }
}

void clear_screen(unsigned char color) {
    const unsigned short blank = ((unsigned short)color << 8) | ' ';
    for (unsigned int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = blank;
    }
}

void write_text(unsigned int x, unsigned int y, unsigned char color, const char* fmt, ...) {
    unsigned int cx = x;
    unsigned int cy = y;

    va_list vargs;
    va_start(vargs, fmt);

    while (*fmt && cy < VGA_HEIGHT) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case '\0':
                    vga_putc_at(&cx, &cy, color, '%');
                    goto end;

                case '%':
                    vga_putc_at(&cx, &cy, color, '%');
                    break;

                case 's': {
                    const char *s = va_arg(vargs, const char *);
                    if (!s) s = "(null)";
                    while (*s && cy < VGA_HEIGHT) {
                        vga_putc_at(&cx, &cy, color, *s++);
                    }
                    break;
                }

                case 'd': {
                    int value = va_arg(vargs, int);
                    vga_print_dec(&cx, &cy, color, value);
                    break;
                }

                case 'x': {
                    unsigned int value = va_arg(vargs, unsigned int);
                    vga_print_hex(&cx, &cy, color, value);
                    break;
                }

                default:
                    vga_putc_at(&cx, &cy, color, '%');
                    vga_putc_at(&cx, &cy, color, *fmt);
                    break;
            }
        } else {
            vga_putc_at(&cx, &cy, color, *fmt);
        }
        fmt++;
    }
    end:
        va_end(vargs);
}

void screen_init(void) {
    for (unsigned int y = 0; y < HISTORY_LINES; y++) {
        clear_history_line(y);
    }

    hist_write_line = 0;
    hist_write_col = 0;
    hist_line_count = 1;
    view_top_line = 0;
    follow_bottom = true;

    cursor_x = 0;
    cursor_y = 0;
    console_redraw_view();
}

static void scroll(void) {
    for (unsigned int y = 1; y < VGA_HEIGHT; y ++) {
        for (unsigned x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }
    unsigned short blank = ((unsigned short)current_color << 8) | ' ';
    for (unsigned int x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }
}

void console_redraw_view(void) {
    unsigned char color = current_color;

    for (unsigned int y = 0; y < VGA_HEIGHT; y++) {
        bool row_valid = false;
        unsigned int hist_y = 0;

        if (hist_line_count < HISTORY_LINES) {
            unsigned int candidate = view_top_line + y;
            if (candidate < hist_line_count) {
                row_valid = true;
                hist_y = candidate;
            }
        } else {
            row_valid = true;
            hist_y = (view_top_line + y) % HISTORY_LINES;
        }

        for (unsigned int x = 0; x < VGA_WIDTH; x++) {
            char ch = row_valid ? history[hist_y][x] : ' ';
            unsigned int vga_index = y * VGA_WIDTH + x;
            VGA_MEMORY[vga_index] = ((unsigned short)color << 8) | (unsigned char)ch; 
        }
    }
}

void console_putchar(char c) {
    if (c == '\b') {
        if (hist_write_col > 0) {
            hist_write_col--;
            history[hist_write_line][hist_write_col] = ' ';
        } else if (hist_line_count > 1) {
            hist_write_line = (hist_write_line + HISTORY_LINES - 1) % HISTORY_LINES;
            hist_write_col = VGA_WIDTH;
            while (hist_write_col > 0 && history[hist_write_line][hist_write_col - 1] == ' ') {
                hist_write_col--;
            }
            if (hist_write_col > 0) {
                hist_write_col--;
                history[hist_write_line][hist_write_col] = ' ';
            }
        }
        goto redraw;
    }

    if (c == '\n') {
        history_new_line();
        goto redraw;
    }

    if (hist_write_col >= VGA_WIDTH) {
        history_new_line();
    }

    history[hist_write_line][hist_write_col] = c;
    hist_write_col++;

redraw:
    cursor_x = hist_write_col;
    cursor_y = (hist_line_count < VGA_HEIGHT) ? (hist_line_count - 1) : (VGA_HEIGHT - 1);
    update_view_to_bottom();
    console_redraw_view();
}

void console_print_dec(int num) {
    unsigned int magnitude;

    if (num < 0) {
        console_putchar('-');
        magnitude = (unsigned int)(-num);
    } else {
        magnitude = (unsigned int)num;
    }

    unsigned int divisor = 1;
    while (magnitude / divisor > 9) divisor *= 10;

    do {
        console_putchar((char)('0' + (magnitude / divisor)));
        magnitude %= divisor;
        divisor /= 10;
    } while (divisor > 0);
}

void console_print_hex(unsigned int value) {
    for (int i = 7; i >= 0; i--) {
        unsigned int nibble = (value >> (i * 4)) & 0xF;
        console_putchar("0123456789abcdef"[nibble]);
    }
}

void console_write(const char* fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case '\0':
                    console_putchar('%');
                    goto end;

                case '%':
                    console_putchar('%');
                    break;

                case 's': {
                    const char *s = va_arg(vargs, const char *);
                    if (!s) s = "(null)";
                    while (*s) {
                        console_putchar(*s++);
                    }
                    break;
                }

                case 'd': {
                    int value = va_arg(vargs, int);
                    console_print_dec(value);
                    break;
                }

                case 'x': {
                    unsigned int value = va_arg(vargs, unsigned int);
                    console_print_hex(value);
                    break;
                }

                case 'b': {
                    if (cursor_x > 0) {
                        cursor_x--;
                        console_putchar(' '); // Overwrite with space
                        cursor_x--;
                    }
                    break;
                }

                default:
                    console_putchar('%');
                    console_putchar(*fmt);
                    break;
            }
        } else {
            console_putchar(*fmt);
        }
        fmt++;
    }
    end:
        va_end(vargs);
}

void console_scroll_up(void) {
    unsigned int max_scroll = (hist_line_count > VGA_HEIGHT) ? (hist_line_count - VGA_HEIGHT) : 0;
    if (max_scroll == 0) {
        return;
    }

    if (scroll_lines_from_bottom < max_scroll) {
        scroll_lines_from_bottom++;
    }

    follow_bottom = (scroll_lines_from_bottom == 0);
    apply_scroll_position();
    console_redraw_view();
}

void console_scroll_down(void) {
    if (scroll_lines_from_bottom > 0) {
        scroll_lines_from_bottom--;
    }

    follow_bottom = (scroll_lines_from_bottom == 0);
    apply_scroll_position();
    console_redraw_view();
}

