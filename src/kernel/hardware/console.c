#include "common.h"
#include "console.h"
#include "heap.h"
#include "kernel.h"
#include "renderer.h"
#include "screen.h"

static unsigned int cursor_x = 0;
static unsigned int cursor_y = 0;
static unsigned char current_color = 0x0F; 

static char *history_chars = NULL;
static unsigned char *history_colors = NULL;
static unsigned int hist_write_line = 0;
static unsigned int hist_write_col = 0;
static unsigned int hist_line_count = 1;
static unsigned int scroll_lines_from_bottom = 0;
static unsigned int view_top_line = 0;
static bool follow_bottom = true;

static char *fb_prev_chars = NULL;
static unsigned char *fb_prev_colors = NULL;
static bool *fb_prev_valid = NULL;
static unsigned int fb_prev_cursor_x = CONSOLE_MAX_COLS;
static unsigned int fb_prev_cursor_y = CONSOLE_MAX_ROWS;
static render_mode_t last_render_mode = RENDER_VGA;
static unsigned int history_cols = VGA_WIDTH;
static unsigned int view_rows = VGA_HEIGHT;

static char *saved_history_chars = NULL;
static unsigned char *saved_history_colors = NULL;
static unsigned int saved_history_cols = 0;

static void apply_scroll_position(void);
static void console_free_saved_state(void);

static void console_sanitize_state(void) {
    unsigned int max_scroll;

    if (history_cols == 0 || view_rows == 0) {
        return;
    }

    if (hist_line_count == 0) {
        hist_line_count = 1;
    } else if (hist_line_count > HISTORY_LINES) {
        hist_line_count = HISTORY_LINES;
    }

    hist_write_line %= HISTORY_LINES;

    if (hist_write_col > history_cols) {
        hist_write_col = history_cols;
    }

    if (cursor_x >= history_cols) {
        cursor_x = history_cols - 1;
    }
    if (cursor_y >= view_rows) {
        cursor_y = view_rows - 1;
    }

    max_scroll = (hist_line_count > view_rows) ? (hist_line_count - view_rows) : 0;
    if (scroll_lines_from_bottom > max_scroll) {
        scroll_lines_from_bottom = max_scroll;
    }
}

#define HIST_IDX(line, col) ((line) * history_cols + (col))
#define FB_IDX(row, col) ((row) * history_cols + (col))

static unsigned int console_cols(void) {
    if (renderer_get_mode() == RENDER_FB && fb_is_available()) {
        unsigned int cols = fb_get_width() / FB_CELL_WIDTH;
        if (cols == 0) {
            cols = 1;
        }
        if (cols > CONSOLE_MAX_COLS) {
            cols = CONSOLE_MAX_COLS;
        }
        return cols;
    }

    return VGA_WIDTH;
}

static unsigned int console_rows(void) {
    if (renderer_get_mode() == RENDER_FB && fb_is_available()) {
        unsigned int rows = fb_get_height() / FB_CELL_HEIGHT;
        if (rows == 0) {
            rows = 1;
        }
        if (rows > CONSOLE_MAX_ROWS) {
            rows = CONSOLE_MAX_ROWS;
        }
        return rows;
    }

    return VGA_HEIGHT;
}

static bool console_alloc_buffers(unsigned int new_cols, unsigned int new_rows, bool preserve) {
    char *new_history_chars;
    unsigned char *new_history_colors;
    char *new_fb_prev_chars;
    unsigned char *new_fb_prev_colors;
    bool *new_fb_prev_valid;
    unsigned int old_cols = history_cols;

    if (new_cols == 0 || new_rows == 0) {
        return false;
    }

    new_history_chars = (char*)kcalloc(sizeof(char), HISTORY_LINES * new_cols);
    new_history_colors = (unsigned char*)kcalloc(sizeof(unsigned char), HISTORY_LINES * new_cols);
    new_fb_prev_chars = (char*)kcalloc(sizeof(char), new_rows * new_cols);
    new_fb_prev_colors = (unsigned char*)kcalloc(sizeof(unsigned char), new_rows * new_cols);
    new_fb_prev_valid = (bool*)kcalloc(sizeof(bool), new_rows * new_cols);

    if (!new_history_chars || !new_history_colors || !new_fb_prev_chars || !new_fb_prev_colors || !new_fb_prev_valid) {
        if (new_history_chars) kfree(new_history_chars);
        if (new_history_colors) kfree(new_history_colors);
        if (new_fb_prev_chars) kfree(new_fb_prev_chars);
        if (new_fb_prev_colors) kfree(new_fb_prev_colors);
        if (new_fb_prev_valid) kfree(new_fb_prev_valid);
        return false;
    }

    for (unsigned int y = 0; y < HISTORY_LINES; y++) {
        for (unsigned int x = 0; x < new_cols; x++) {
            new_history_chars[y * new_cols + x] = ' ';
            new_history_colors[y * new_cols + x] = current_color;
        }
    }

    if (preserve && history_chars && history_colors) {
        unsigned int copy_cols = (old_cols < new_cols) ? old_cols : new_cols;
        for (unsigned int y = 0; y < HISTORY_LINES; y++) {
            for (unsigned int x = 0; x < copy_cols; x++) {
                new_history_chars[y * new_cols + x] = history_chars[y * old_cols + x];
                new_history_colors[y * new_cols + x] = history_colors[y * old_cols + x];
            }
        }
    }

    if (history_chars) kfree(history_chars);
    if (history_colors) kfree(history_colors);
    if (fb_prev_chars) kfree(fb_prev_chars);
    if (fb_prev_colors) kfree(fb_prev_colors);
    if (fb_prev_valid) kfree(fb_prev_valid);

    history_chars = new_history_chars;
    history_colors = new_history_colors;
    fb_prev_chars = new_fb_prev_chars;
    fb_prev_colors = new_fb_prev_colors;
    fb_prev_valid = new_fb_prev_valid;

    history_cols = new_cols;
    view_rows = new_rows;

    if (hist_write_col >= history_cols) {
        hist_write_col = history_cols ? (history_cols - 1) : 0;
    }
    if (cursor_x >= history_cols) {
        cursor_x = history_cols ? (history_cols - 1) : 0;
    }
    if (cursor_y >= view_rows) {
        cursor_y = view_rows ? (view_rows - 1) : 0;
    }

    fb_prev_cursor_x = CONSOLE_MAX_COLS;
    fb_prev_cursor_y = CONSOLE_MAX_ROWS;
    return true;
}

static bool console_ensure_layout(void) {
    unsigned int target_cols = console_cols();
    unsigned int target_rows = console_rows();

    if (!history_chars || !history_colors || !fb_prev_chars || !fb_prev_colors || !fb_prev_valid) {
        return console_alloc_buffers(target_cols, target_rows, false);
    }

    if (history_cols != target_cols || view_rows != target_rows) {
        if (!console_alloc_buffers(target_cols, target_rows, true)) {
            return false;
        }
        if (hist_write_col >= history_cols) {
            hist_write_col = history_cols ? (history_cols - 1) : 0;
        }
        apply_scroll_position();
    }

    return true;
}

static void resolve_row_source(unsigned int screen_y, bool *row_valid, unsigned int *hist_y) {
    if (hist_line_count < HISTORY_LINES) {
        unsigned int candidate = view_top_line + screen_y;
        if (candidate < hist_line_count) {
            *row_valid = true;
            *hist_y = candidate;
        } else {
            *row_valid = false;
            *hist_y = 0;
        }
    } else {
        *row_valid = true;
        *hist_y = (view_top_line + screen_y) % HISTORY_LINES;
    }
}

static void fb_invalidate_row(unsigned int row) {
    unsigned int cols = console_cols();
    unsigned int rows = view_rows;

    if (row >= rows) {
        return;
    }

    for (unsigned int x = 0; x < cols; x++) {
        fb_prev_valid[FB_IDX(row, x)] = false;
    }
}

static void fb_shift_cache_down_one_row(void);
static void fb_shift_cache_up_one_row(void);

static uint32_t vga_index_to_rgb(unsigned char index) {
    static const uint32_t palette[16] = {
        0x00000000, 0x000000AA, 0x0000AA00, 0x0000AAAA,
        0x00AA0000, 0x00AA00AA, 0x00AA5500, 0x00AAAAAA,
        0x00555555, 0x005555FF, 0x0055FF55, 0x0055FFFF,
        0x00FF5555, 0x00FF55FF, 0x00FFFF55, 0x00FFFFFF
    };

    return palette[index & 0x0F];
}

static void decode_vga_cell_color(unsigned char cell_color, uint32_t *fg, uint32_t *bg) {
    unsigned char fg_idx = cell_color & 0x0F;
    unsigned char bg_idx = (cell_color >> 4) & 0x0F;

    *fg = vga_index_to_rgb(fg_idx);
    *bg = vga_index_to_rgb(bg_idx);
}

static void fb_invalidate_cache(void) {
    unsigned int cols = console_cols();
    unsigned int rows = view_rows;

    for (unsigned int y = 0; y < rows; y++) {
        for (unsigned int x = 0; x < cols; x++) {
            fb_prev_valid[FB_IDX(y, x)] = false;
        }
    }
    fb_prev_cursor_x = CONSOLE_MAX_COLS;
    fb_prev_cursor_y = CONSOLE_MAX_ROWS;
}

static unsigned int get_bottom_top_line(void) {
    unsigned int rows = view_rows;

    if (hist_line_count <= rows) {
        return 0;
    } else if (hist_line_count < HISTORY_LINES) {
        return hist_line_count - rows;
    } else {
        return (hist_write_line + HISTORY_LINES - rows) % HISTORY_LINES;
    }
}

static void apply_scroll_position(void) {
    unsigned int rows = view_rows;
    unsigned int bottom_top = get_bottom_top_line();

    if (hist_line_count < HISTORY_LINES) {
        unsigned int max_scroll = (hist_line_count > rows) ? (hist_line_count - rows) : 0;
        if (scroll_lines_from_bottom > max_scroll) {
            scroll_lines_from_bottom = max_scroll;
        }
        view_top_line = bottom_top - scroll_lines_from_bottom;
        return;
    }
    view_top_line = (bottom_top + HISTORY_LINES - (scroll_lines_from_bottom % HISTORY_LINES)) % HISTORY_LINES;
}

static void clear_history_line(unsigned int line) {
    for (unsigned int x = 0; x < history_cols; x++) {
        history_chars[HIST_IDX(line, x)] = ' ';
        history_colors[HIST_IDX(line, x)] = current_color;
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
    unsigned int rows = view_rows;

    if (!follow_bottom) {
        return;
    }

    if (hist_line_count <= rows) {
        view_top_line = 0;
    } else if (hist_line_count < HISTORY_LINES) {
        view_top_line = hist_line_count - rows;
    } else {
        view_top_line = (hist_write_line + HISTORY_LINES - (rows - 1)) % HISTORY_LINES;
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
    if (!console_ensure_layout()) {
        return;
    }
    
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
    fb_invalidate_cache();
    console_redraw_view();
}

void console_redraw_view(void) {
    bool use_fb = (renderer_get_mode() == RENDER_FB);
    unsigned int cols;
    unsigned int rows;
    unsigned char color = current_color;

    if (!console_ensure_layout()) {
        return;
    }

    console_sanitize_state();

    cols = console_cols();
    rows = view_rows;

    if (use_fb && last_render_mode != RENDER_FB) {
        fb_invalidate_cache();
    }

    if (use_fb && fb_prev_cursor_x < cols && fb_prev_cursor_y < rows) {
        fb_prev_valid[FB_IDX(fb_prev_cursor_y, fb_prev_cursor_x)] = false;
    }

    for (unsigned int y = 0; y < rows; y++) {
        bool row_valid = false;
        unsigned int hist_y = 0;
        resolve_row_source(y, &row_valid, &hist_y);

        for (unsigned int x = 0; x < cols; x++) {
            char ch = row_valid ? history_chars[HIST_IDX(hist_y, x)] : ' ';
            unsigned char cell_color = row_valid ? history_colors[HIST_IDX(hist_y, x)] : color;

            if (use_fb) {
                if (x == cursor_x && y == cursor_y) {
                    unsigned char fg = cell_color & 0x0F;
                    unsigned char bg = (cell_color >> 4) & 0x0F;
                    cell_color = (unsigned char)((fg << 4) | bg);
                }

                if (fb_prev_valid[FB_IDX(y, x)] && fb_prev_chars[FB_IDX(y, x)] == ch && fb_prev_colors[FB_IDX(y, x)] == cell_color) {
                    continue;
                }

                uint32_t fg_rgb;
                uint32_t bg_rgb;
                decode_vga_cell_color(cell_color, &fg_rgb, &bg_rgb);
                fb_draw_char(x * FB_CELL_WIDTH, y * FB_CELL_HEIGHT, ch, fg_rgb, bg_rgb);
                fb_prev_chars[FB_IDX(y, x)] = ch;
                fb_prev_colors[FB_IDX(y, x)] = cell_color;
                fb_prev_valid[FB_IDX(y, x)] = true;
                continue;
            } else {
                unsigned int vga_index = y * VGA_WIDTH + x;
                VGA_MEMORY[vga_index] = ((unsigned short)cell_color << 8) | (unsigned char)ch;
            }
        }
    }

    if (use_fb) {
        fb_prev_cursor_x = cursor_x;
        fb_prev_cursor_y = cursor_y;
    } else {
        console_draw_cursor();
    }

    last_render_mode = renderer_get_mode();
}

void console_putchar(char c) {
    unsigned int cols;
    unsigned int rows;

    if (!console_ensure_layout()) {
        return;
    }

    console_sanitize_state();
    cols = console_cols();
    rows = view_rows;

    if (c == '\b') {
        if (hist_write_col > 0) {
            hist_write_col--;
            history_chars[HIST_IDX(hist_write_line, hist_write_col)] = ' ';
            history_colors[HIST_IDX(hist_write_line, hist_write_col)] = current_color;
        } else if (hist_line_count > 1) {
            hist_write_line = (hist_write_line + HISTORY_LINES - 1) % HISTORY_LINES;
            hist_write_col = cols;
            while (hist_write_col > 0 && history_chars[HIST_IDX(hist_write_line, hist_write_col - 1)] == ' ') {
                hist_write_col--;
            }
            if (hist_write_col > 0) {
                hist_write_col--;
                history_chars[HIST_IDX(hist_write_line, hist_write_col)] = ' ';
                history_colors[HIST_IDX(hist_write_line, hist_write_col)] = current_color;
            }
        }
        goto redraw;
    }

    if (c == '\n') {
        history_new_line();
        goto redraw;
    }

    if (hist_write_col >= cols) {
        history_new_line();
    }

    history_chars[HIST_IDX(hist_write_line, hist_write_col)] = c;
    history_colors[HIST_IDX(hist_write_line, hist_write_col)] = current_color;
    hist_write_col++;

redraw:
    cursor_x = hist_write_col;
    cursor_y = (hist_line_count < rows) ? (hist_line_count - 1) : (rows - 1);
    scroll_lines_from_bottom = 0;
    follow_bottom = true;
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
    unsigned int cols;
    unsigned int rows;
    unsigned int max_scroll;
    bool use_fb;

    if (!console_ensure_layout()) {
        return;
    }

    console_sanitize_state();
    cols = console_cols();
    rows = view_rows;
    max_scroll = (hist_line_count > rows) ? (hist_line_count - rows) : 0;
    use_fb = (renderer_get_mode() == RENDER_FB) && fb_is_available();

    if (max_scroll == 0) {
        return;
    }

    if (scroll_lines_from_bottom < max_scroll) {
        scroll_lines_from_bottom++;
    }

    follow_bottom = (scroll_lines_from_bottom == 0);
    apply_scroll_position();

    if (use_fb && rows > 1) {
        fb_blit(0, 0, 0, FB_CELL_HEIGHT, cols * FB_CELL_WIDTH, (rows - 1U) * FB_CELL_HEIGHT);
        fb_shift_cache_up_one_row();
        if (cursor_y > 0) {
            fb_invalidate_row(cursor_y - 1U);
        }
    }

    console_redraw_view();
}

void console_scroll_down(void) {
    unsigned int cols;
    unsigned int rows;
    bool use_fb;

    if (!console_ensure_layout()) {
        return;
    }

    console_sanitize_state();
    cols = console_cols();
    rows = view_rows;
    use_fb = (renderer_get_mode() == RENDER_FB) && fb_is_available();

    if (scroll_lines_from_bottom > 0) {
        scroll_lines_from_bottom--;
    }

    follow_bottom = (scroll_lines_from_bottom == 0);
    apply_scroll_position();

    if (use_fb && rows > 1) {
        fb_blit(0, FB_CELL_HEIGHT, 0, 0, cols * FB_CELL_WIDTH, (rows - 1U) * FB_CELL_HEIGHT);
        fb_shift_cache_down_one_row();
        if (cursor_y + 1U < rows) {
            fb_invalidate_row(cursor_y + 1U);
        }
    }

    console_redraw_view();
}

void console_set_color(unsigned char color) {
    current_color = color;
}

unsigned char console_get_color(void) {
    return current_color;
}

void console_write_colored(unsigned char color, const char* fmt, ...) {
    unsigned char old_color = current_color;
    unsigned char effective_color = color ? color : old_color;
    current_color = effective_color;

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
    current_color = old_color;
}

void console_write_ascii(const char* name) {
    if (!name || strcmp(name, "help") == 0) {
        console_write("ASCII presets: cat, skull, logo\n");
        return;
    }

    if (strcmp(name, "cat") == 0) {
        console_write(" /\\_/\\\n");
        console_write("( o.o )\n");
        console_write(" > ^ <\n");
        return;
    }

    if (strcmp(name, "logo") == 0) {
        console_write("DDDD   U   U  M   M  BBBB     K   K  EEEEE  RRRR   N   N  EEEEE  L\n");
        console_write("D   D  U   U  MM MM  B   B    K  K   E      R   R  NN  N  E      L\n");
        console_write("D   D  U   U  M M M  BBBB     KKK    EEEE   RRRR   N N N  EEEE   L\n");
        console_write("D   D  U   U  M   M  B   B    K  K   E      R  R   N  NN  E      L\n");
        console_write("DDDD    UUU   M   M  BBBB     K   K  EEEEE  R   R  N   N  EEEEE  LLLLL\n");
        return;
    }

    console_write_colored(CONSOLE_COLOR_ERROR, "Unknown ASCII preset: %s\n", name);
    console_write("Use: help\n");
}

void console_draw_cursor(void) {
    unsigned int cols = console_cols();
    unsigned int rows = view_rows;

    if (cursor_x >= cols) return;
    if (cursor_y >= rows) return;

    if (renderer_get_mode() == RENDER_FB) {
        bool row_valid = false;
        unsigned int hist_y = 0;
        char ch = ' ';
        unsigned char cell_color = current_color;
        uint32_t fg_rgb;
        uint32_t bg_rgb;

        if (hist_line_count < HISTORY_LINES) {
            unsigned int candidate = view_top_line + cursor_y;
            if (candidate < hist_line_count) {
                row_valid = true;
                hist_y = candidate;
            }
        } else {
            row_valid = true;
            hist_y = (view_top_line + cursor_y) % HISTORY_LINES;
        }

        if (row_valid) {
            ch = history_chars[HIST_IDX(hist_y, cursor_x)];
            cell_color = history_colors[HIST_IDX(hist_y, cursor_x)];
        }

        decode_vga_cell_color(cell_color, &fg_rgb, &bg_rgb);
        fb_draw_char(cursor_x * FB_CELL_WIDTH, cursor_y * FB_CELL_HEIGHT, ch, bg_rgb, fg_rgb);
        return;
    }

    unsigned int idx = cursor_y * VGA_WIDTH + cursor_x;
    unsigned short cell = VGA_MEMORY[idx];
    unsigned char ch = (unsigned char)(cell & 0xFF);

    VGA_MEMORY[idx] = ((unsigned short)0x70 << 8) | ch;
}

void console_cursor_left(void) {
    if (hist_write_col == 0) {
        return;
    }

    unsigned int rows = view_rows;
    hist_write_col--;
    cursor_x = hist_write_col;
    cursor_y = (hist_line_count < rows) ? (hist_line_count - 1) : (rows - 1);
    update_view_to_bottom();
    console_redraw_view();
}

void console_cursor_right(void) {
    unsigned int cols = console_cols();
    unsigned int rows = view_rows;

    if (hist_write_col >= cols) {
        return;
    }

    hist_write_col++;
    cursor_x = hist_write_col;
    cursor_y = (hist_line_count < rows) ? (hist_line_count - 1) : (rows - 1);
    update_view_to_bottom();
    console_redraw_view();
}

bool console_save_state(console_state_t *state) {
    if (!state) {
        return false;
    }
    if (!history_chars || !history_colors) {
        return false;
    }

    console_free_saved_state();

    saved_history_chars = (char*)kcalloc(sizeof(char), HISTORY_LINES * history_cols);
    saved_history_colors = (unsigned char*)kcalloc(sizeof(unsigned char), HISTORY_LINES * history_cols);
    if (!saved_history_chars || !saved_history_colors) {
        console_free_saved_state();
        return false;
    }

    for (unsigned int y = 0; y < HISTORY_LINES; y++) {
        for (unsigned int x = 0; x < history_cols; x++) {
            saved_history_chars[y * history_cols + x] = history_chars[HIST_IDX(y, x)];
            saved_history_colors[y * history_cols + x] = history_colors[HIST_IDX(y, x)];
        }
    }

    saved_history_cols = history_cols;

    state->cursor_x = cursor_x;
    state->cursor_y = cursor_y;
    state->current_color = current_color;
    state->hist_write_line = hist_write_line;
    state->hist_write_col = hist_write_col;
    state->hist_line_count = hist_line_count;
    state->scroll_lines_from_bottom = scroll_lines_from_bottom;
    state->view_top_line = view_top_line;
    state->follow_bottom = follow_bottom;
    state->history_cols = history_cols;
    state->view_rows = view_rows;

    return true;
}

bool console_restore_state(const console_state_t *state) {
    if (!state) {
        return false;
    }
    if (!saved_history_chars || !saved_history_colors || saved_history_cols == 0) {
        return false;
    }
    if (!console_ensure_layout()) {
        return false;
    }

    cursor_x = state->cursor_x;
    cursor_y = state->cursor_y;
    current_color = state->current_color;
    hist_write_line = state->hist_write_line;
    hist_write_col = state->hist_write_col;
    hist_line_count = state->hist_line_count;
    scroll_lines_from_bottom = state->scroll_lines_from_bottom;
    view_top_line = state->view_top_line;
    follow_bottom = state->follow_bottom;

    unsigned int copy_cols = (saved_history_cols < history_cols) ? saved_history_cols : history_cols;

    for (unsigned int y = 0; y < HISTORY_LINES; y++) {
        for (unsigned int x = 0; x < history_cols; x++) {
            if (x < copy_cols) {
                history_chars[HIST_IDX(y, x)] = saved_history_chars[y * saved_history_cols + x];
                history_colors[HIST_IDX(y, x)] = saved_history_colors[y * saved_history_cols + x];
            } else {
                history_chars[HIST_IDX(y, x)] = ' ';
                history_colors[HIST_IDX(y, x)] = current_color;
            }
        }
    }

    if (hist_write_col >= history_cols) {
        hist_write_col = history_cols ? (history_cols - 1) : 0;
    }
    if (cursor_x >= history_cols) {
        cursor_x = history_cols ? (history_cols - 1) : 0;
    }
    if (cursor_y >= view_rows) {
        cursor_y = view_rows ? (view_rows - 1) : 0;
    }

    fb_invalidate_cache();
    console_redraw_view();
    return true;
}

static void fb_shift_cache_down_one_row(void) {
    unsigned int cols = console_cols();
    unsigned int rows = view_rows;

    for (int y = (int)rows - 1; y > 0; y--) {
        for (unsigned int x = 0; x < cols; x++) {
            fb_prev_chars[FB_IDX(y, x)] = fb_prev_chars[FB_IDX(y - 1, x)];
            fb_prev_colors[FB_IDX(y, x)] = fb_prev_colors[FB_IDX(y - 1, x)];
            fb_prev_valid[FB_IDX(y, x)] = fb_prev_valid[FB_IDX(y - 1, x)];
        }
    }

    for (unsigned int x = 0; x < cols; x++) {
        fb_prev_valid[FB_IDX(0, x)] = false;
    }
}

static void fb_shift_cache_up_one_row(void) {
    unsigned int cols = console_cols();
    unsigned int rows = view_rows;

    for (unsigned int y = 0; y < rows - 1; y++) {
        for (unsigned int x = 0; x < cols; x++) {
            fb_prev_chars[FB_IDX(y, x)] = fb_prev_chars[FB_IDX(y + 1, x)];
            fb_prev_colors[FB_IDX(y, x)] = fb_prev_colors[FB_IDX(y + 1, x)];
            fb_prev_valid[FB_IDX(y, x)] = fb_prev_valid[FB_IDX(y + 1, x)];
        }
    }

    for (unsigned int x = 0; x < cols; x++) {
        fb_prev_valid[FB_IDX(rows - 1, x)] = false;
    }
}

static void console_free_saved_state(void) {
    if (saved_history_chars) {
        kfree(saved_history_chars);
        saved_history_chars = NULL;
    }
    if (saved_history_colors) {
        kfree(saved_history_colors);
        saved_history_colors = NULL;
    }
    saved_history_cols = 0;
}