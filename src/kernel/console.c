#include "common.h"
#include "console.h"

static unsigned int cursor_x = 0;
static unsigned int cursor_y = 0;
static unsigned char current_color = 0x0F; 

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
    clear_screen(current_color);
    cursor_x = 0;
    cursor_y = 0;
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

void console_putchar(char c) {
    vga_putc_at(&cursor_x, &cursor_y, current_color, c);
    if (cursor_y >= VGA_HEIGHT) {
        scroll();
        cursor_y = VGA_HEIGHT - 1;
        cursor_x = 0;
        }
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

