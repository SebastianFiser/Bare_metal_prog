#include "meowim.h"
#include "console.h"
#include "filesys.h"
#include "input.h"
#include "shell.h"

static console_state_t saved_state;
static int active = 0;

#define MEOWIM_BUF_MAX 2048
#define MEOWIM_NAME_MAX 32
#define BODY_TOP 1
#define BODY_ROWS (VGA_HEIGHT - 2)

static char text_buf[MEOWIM_BUF_MAX];
static unsigned int text_len = 0;
static unsigned int cursor = 0;
static unsigned int view_first_line = 0;
static int dirty = 0;
static char current_file[MEOWIM_NAME_MAX];
static unsigned int desired_col = 0;

static void put_cell(unsigned int x, unsigned int y, unsigned char color, char ch) {
	if (x >= VGA_WIDTH || y >= VGA_HEIGHT) {
		return;
	}
	VGA_MEMORY[y * VGA_WIDTH + x] = ((unsigned short)color << 8) | (unsigned char)ch;
}

static void draw_row_text(unsigned int y, unsigned char color, const char *s) {
	unsigned int x = 0;
	while (x < VGA_WIDTH) {
		char ch = ' ';
		if (*s) {
			ch = *s;
			s++;
		}
		put_cell(x, y, color, ch);
		x++;
	}
}

static void clear_body(void) {
	for (unsigned int y = BODY_TOP; y < (VGA_HEIGHT - 1); y++) {
		for (unsigned int x = 0; x < VGA_WIDTH; x++) {
			put_cell(x, y, 0x0F, ' ');
		}
	}
}

static void copy_file_name(const char *name) {
	unsigned int i = 0;
	if (!name || !name[0]) {
		name = "untitled.txt";
	}
	while (name[i] && i < (MEOWIM_NAME_MAX - 1)) {
		current_file[i] = name[i];
		i++;
	}
	current_file[i] = '\0';
}

static void index_to_line_col(unsigned int idx, unsigned int *line, unsigned int *col) {
	unsigned int l = 0;
	unsigned int c = 0;

	if (idx > text_len) {
		idx = text_len;
	}

	for (unsigned int i = 0; i < idx; i++) {
		char ch = text_buf[i];
		if (ch == '\n') {
			l++;
			c = 0;
		} else {
			c++;
			if (c >= VGA_WIDTH) {
				l++;
				c = 0;
			}
		}
	}

	*line = l;
	*col = c;
}

static unsigned int count_lines(void) {
	unsigned int line = 0;
	unsigned int col = 0;

	for (unsigned int i = 0; i < text_len; i++) {
		char ch = text_buf[i];
		if (ch == '\n') {
			line++;
			col = 0;
			continue;
		}

		col++;
		if (col >= VGA_WIDTH) {
			line++;
			col = 0;
		}
	}

	return line + 1;
}

static unsigned int find_cursor_for_line_col(unsigned int target_line, unsigned int target_col) {
	unsigned int line = 0;
	unsigned int col = 0;
	unsigned int best_index = 0;
	unsigned int best_col = 0;

	for (unsigned int i = 0; i < text_len; i++) {
		if (line == target_line) {
			best_index = i;
			best_col = col;
			if (col >= target_col) {
				return i;
			}
		}

		char ch = text_buf[i];
		if (ch == '\n') {
			line++;
			col = 0;
			continue;
		}

		col++;
		if (col >= VGA_WIDTH) {
			line++;
			col = 0;
		}
	}

	if (line == target_line) {
		best_index = text_len;
		best_col = col;
	}

	if (best_col < target_col) {
		return best_index;
	}

	return best_index;
}

static void ensure_cursor_visible(void) {
	unsigned int cursor_line = 0;
	unsigned int cursor_col = 0;
	index_to_line_col(cursor, &cursor_line, &cursor_col);

	if (cursor_line < view_first_line) {
		view_first_line = cursor_line;
	}

	if (cursor_line >= (view_first_line + BODY_ROWS)) {
		view_first_line = cursor_line - BODY_ROWS + 1;
	}

	desired_col = cursor_col;
	(void)cursor_col;
}

static int meowim_save_file(void) {
	int create_result = fs_create(current_file);
	if (create_result != 0 && create_result != -2) {
		return create_result;
	}

	text_buf[text_len] = '\0';
	if (fs_write(current_file, text_buf) < 0) {
		return -1;
	}

	dirty = 0;
	return 0;
}

static void meowim_render(void) {
	draw_row_text(0, 0x1E, " MEOWIM FULLSCREEN ");
	write_text(1, 0, 0x1E, "file:%s", current_file);
	if (dirty) {
		write_text(VGA_WIDTH - 10, 0, 0x1E, "[modified]");
	}
	clear_body();

	unsigned int x = 0;
	unsigned int y = 0;
	unsigned int global_line = 0;
	unsigned int cx = 0;
	unsigned int cy = BODY_TOP;

	ensure_cursor_visible();

	for (unsigned int i = 0; i < text_len; i++) {
		if (i == cursor) {
			cx = x;
			cy = BODY_TOP + (global_line - view_first_line);
		}

		char ch = text_buf[i];
		if (ch == '\n') {
			x = 0;
			global_line++;
			y = global_line - view_first_line;
			continue;
		}

		if (global_line >= view_first_line && y < BODY_ROWS) {
			put_cell(x, BODY_TOP + y, 0x0F, ch);
		}

		x++;
		if (x >= VGA_WIDTH) {
			x = 0;
			global_line++;
			y = global_line - view_first_line;
		}
	}

	if (cursor >= text_len) {
		cx = x;
		cy = BODY_TOP + (global_line - view_first_line);
	}

	if (cy < BODY_TOP) {
		cy = BODY_TOP;
	}
	if (cy >= (VGA_HEIGHT - 1)) {
		cy = VGA_HEIGHT - 2;
		cx = VGA_WIDTH - 1;
	}

	unsigned int idx = cy * VGA_WIDTH + cx;
	unsigned short cell = VGA_MEMORY[idx];
	unsigned char ch = (unsigned char)(cell & 0xFF);
	VGA_MEMORY[idx] = ((unsigned short)0x70 << 8) | ch;

	draw_row_text(VGA_HEIGHT - 1, 0x70, " Ctrl+S Save  Ctrl+Q Quit  PgUp/PgDn Scroll  Arrows Move ");
}

static void insert_char(char ch) {
	if (text_len >= (MEOWIM_BUF_MAX - 1)) {
		return;
	}

	for (unsigned int i = text_len; i > cursor; i--) {
		text_buf[i] = text_buf[i - 1];
	}

	text_buf[cursor] = ch;
	text_len++;
	cursor++;
	dirty = 1;
}

static void backspace_char(void) {
	if (cursor == 0 || text_len == 0) {
		return;
	}

	cursor--;
	for (unsigned int i = cursor; i + 1 < text_len; i++) {
		text_buf[i] = text_buf[i + 1];
	}
	text_len--;
	dirty = 1;
}

static void move_left(void) {
	if (cursor == 0) {
		return;
	}

	cursor--;
	unsigned int line = 0;
	unsigned int col = 0;
	index_to_line_col(cursor, &line, &col);
	desired_col = col;
}

static void move_right(void) {
	if (cursor >= text_len) {
		return;
	}

	cursor++;
	unsigned int line = 0;
	unsigned int col = 0;
	index_to_line_col(cursor, &line, &col);
	desired_col = col;
}

static void move_vertical(int delta) {
	unsigned int cursor_line = 0;
	unsigned int cursor_col = 0;
	unsigned int total_lines = count_lines();

	index_to_line_col(cursor, &cursor_line, &cursor_col);

	if (delta < 0) {
		if (cursor_line == 0) {
			return;
		}
		cursor_line--;
	} else {
		if (cursor_line + 1 >= total_lines) {
			return;
		}
		cursor_line++;
	}

	cursor = find_cursor_for_line_col(cursor_line, desired_col);
	if (cursor > text_len) {
		cursor = text_len;
	}

	index_to_line_col(cursor, &cursor_line, &cursor_col);
	desired_col = cursor_col;
}

void meowim_open(void) {
	meowim_open_file("untitled.txt");
}

void meowim_open_file(const char *filename) {
	if (active) {
		return;
	}

	console_save_state(&saved_state);
	input_set_mode(MODE_EDITOR);
	active = 1;

	copy_file_name(filename);

	int read_result = fs_read(current_file, text_buf, MEOWIM_BUF_MAX);
	if (read_result >= 0) {
		text_len = (unsigned int)read_result;
	} else {
		text_len = 0;
	}
    cursor = 0;
	view_first_line = 0;
	dirty = 0;

    meowim_render();
}

void meowim_close(void) {
	if (!active) {
		return;
	}

	active = 0;
	input_set_mode(MODE_SHELL);
	console_restore_state(&saved_state);
	shell_prompt();
}

int meowim_is_active(void) {
	return active;
}

void editor_input(const input_event_t *event) {
	if (!active) {
		return;
	}

    if (event->type == INPUT_EVENT_PAGEDOWN) {
		view_first_line += BODY_ROWS / 2;
		meowim_render();
		return;
	}

	if (event->type == INPUT_EVENT_PAGEUP) {
		if (view_first_line > (BODY_ROWS / 2)) {
			view_first_line -= BODY_ROWS / 2;
		} else {
			view_first_line = 0;
		}
		meowim_render();
        return;
    }

	if (event->type == INPUT_EVENT_UP) {
		move_vertical(-1);
		meowim_render();
		return;
	}

	if (event->type == INPUT_EVENT_DOWN) {
		move_vertical(1);
		meowim_render();
		return;
	}

	if (event->type == INPUT_EVENT_CHAR) {
		if (event->ch == 19) { // Ctrl+S
			meowim_save_file();
			meowim_render();
			return;
		}

		if (event->ch == 17) { // Ctrl+Q
			meowim_close();
			return;
		}

        insert_char(event->ch);
		desired_col = cursor;
		meowim_render();
		return;
	}

	if (event->type == INPUT_EVENT_BACKSPACE) {
        backspace_char();
		meowim_render();
		return;
	}

	if (event->type == INPUT_EVENT_ENTER) {
		insert_char('\n');
		desired_col = 0;
		meowim_render();
		return;
	}

    if (event->type == INPUT_EVENT_LEFT) {
		move_left();
        meowim_render();
        return;
    }

    if (event->type == INPUT_EVENT_RIGHT) {
		move_right();
        meowim_render();
	}
}