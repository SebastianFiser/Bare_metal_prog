#include "common.h"
#include "console.h"

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