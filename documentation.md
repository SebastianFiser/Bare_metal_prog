# DUMB KERNEL High-Level API Documentation

This document lists the main high-level APIs exposed by the kernel modules and how to use them.

## 1. Console API
Source: src/kernel/hardware/console.h

Purpose:
- Text output in VGA text mode.
- Colored output and cursor control.
- View redraw/scroll.
- Snapshot save/restore for full-screen tools (for example editor mode).

Main APIs:
- `void screen_init(void)`
: Initialize console history/view and draw first frame.
- `void clear_screen(unsigned char color)`
: Fill VGA screen with spaces in selected color.
- `void console_putchar(char c)`
: Print one character into console stream/history.
- `void console_write(const char* fmt, ...)`
: Formatted output (`%s`, `%d`, `%x`, `%%`).
- `void console_write_colored(unsigned char color, const char* fmt, ...)`
: Temporary colored formatted output.
- `void console_write_ascii(const char* name)`
: Print predefined ASCII art preset by name.
- `void console_set_color(unsigned char color)` / `unsigned char console_get_color(void)`
: Manage default text color.
- `void console_scroll_up(void)` / `void console_scroll_down(void)`
: Navigate history view.
- `void console_redraw_view(void)`
: Repaint the visible VGA frame from history.
- `void console_draw_cursor(void)`
: Draw software cursor.
- `void console_cursor_left(void)` / `void console_cursor_right(void)`
: Cursor movement in current line context.
- `void write_text(unsigned int x, unsigned int y, unsigned char color, const char* fmt, ...)`
: Absolute-position write helper for overlays/status rows.
- `void console_save_state(console_state_t* state)` / `void console_restore_state(const console_state_t* state)`
: Save and restore full console state (cursor, history, colors, scroll).

Typical usage:
- Boot banner and status logs.
- Shell prompt and command output.
- Colored diagnostics (`error`, `info`, `dir`).
- Fullscreen apps that need to return to previous terminal state.

---

## 2. Timer and Interrupt API
Source: src/kernel/core/kernel.h

Purpose:
- Configure PIT periodic timer.
- Handle timer ticks and expose uptime.
- Setup IDT and route interrupts.

Timer constants:
- `PIT_BASE_FREQUENCY`
: Base PIT clock (Hz).
- `TIMER_FREQUENCY`
: Target kernel tick rate.
- `DIVISOR`
: PIT divisor derived from base and target frequencies.

Main timer APIs:
- `void timer_init(void)`
: Program PIT channel 0 to periodic mode.
- `void tick_handler(void)`
: Handle one timer interrupt tick.
- `unsigned long timer_get_ticks(void)`
: Read monotonic tick counter.
- `void read_uptime(unsigned int *seconds, unsigned int *milliseconds)`
: Convert ticks to uptime values.

Main interrupt APIs:
- `void idt_init(void)`
: Build/load IDT.
- `void idt_set_gate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags)`
: Configure one IDT entry.
- `void trap_handler_logic(struct registers *regs)`
: Core dispatcher for IRQ/exceptions.
- `void pic_remap(void)`
: Remap PIC offsets and masks.
- `void keyboard_handler(struct registers *regs)`
: IRQ1 keyboard logic.
- `void keyboard_poll(void)`
: Drain queued keyboard events into input router.

Typical usage:
- Kernel boot initialization order (`idt_init` -> `pic_remap` -> `timer_init`).
- Periodic status/overlay updates from tick-based timing.
- Uptime command/reporting.

---

## 3. Input Routing API
Source: src/kernel/shell/input.h

Purpose:
- Abstract keyboard-originated actions as semantic input events.
- Route events to the active UI mode (shell/editor).

Data types:
- `input_event_type_t`
: Event kind (`CHAR`, `ENTER`, `BACKSPACE`, arrows, page up/down, etc.).
- `input_event_t`
: Event payload (`type` + optional `ch`).
- `ui_mode_t`
: Current front-end mode (`MODE_SHELL`, `MODE_EDITOR`).

Main APIs:
- `void input_handle_event(const input_event_t *event)`
: Dispatch one event to active subsystem.
- `void input_set_mode(ui_mode_t mode)`
: Switch routing mode.
- `ui_mode_t input_get_mode(void)`
: Read current mode.

Typical usage:
- Shell line editing in shell mode.
- Arrow/page navigation and text editing in editor mode.

---

## 4. Shell API
Source: src/kernel/shell/shell.h

Purpose:
- Present command-line interface.
- Execute built-in commands.
- Track working directory for fs-aware commands.

Main APIs:
- `void shell_prompt(void)`
: Print prompt (includes current path).
- `void shell_execute_command(const char *cmd)`
: Parse and run one command line.
- `struct fs_node *shell_get_cwd(void)`
: Current working directory node.

Typical usage:
- Called by input layer when user presses Enter.
- Used by editor/file commands to resolve relative paths.

---

## 5. Filesystem API (In-Memory Tree)
Source: src/kernel/filesystem/filesys.h

Purpose:
- Manage hierarchical in-memory files and directories.
- Resolve relative/absolute paths from current directory context.

Data types:
- `fs_node_type_t`
: `FS_NODE_FILE`, `FS_NODE_DIR`.
- `fs_node_t`
: Tree node (name, parent/children, file data).

Main APIs:
- `void fs_init(void)`
: Initialize filesystem and root structure.
- `fs_node_t *fs_root(void)`
: Get root node.
- `fs_node_t *fs_find_child(fs_node_t *parent, const char *name)`
: Find direct child by name.
- `fs_node_t *fs_resolve_path(fs_node_t *start, const char *path)`
: Resolve `.` `..` relative/absolute path.
- `void fs_get_path(fs_node_t *dir, char *out, int out_size)`
: Build printable path string from node.
- `int fs_create(fs_node_t *parent, const char* name, fs_node_type_t type)`
: Create file/dir in a directory.
- `int fs_write(fs_node_t *cwd, const char* name, const char* data)`
: Write text to file (path resolved from cwd).
- `int fs_read(fs_node_t *cwd, const char* name, char* out, int out_size)`
: Read file content to caller buffer.
- `int fs_list_dir(fs_node_t *dir)`
: Print/list entries in directory.

Typical usage:
- Shell commands: `ls`, `cat`, `makef`, `mkdir`, `cd`.
- Editor open/save through cwd-aware paths.

---

## 6. Meowim Editor API
Source: src/kernel/lib/progs/meowim.h

Purpose:
- Fullscreen text editing mode integrated with shell and console snapshots.

Main APIs:
- `void meowim_open(void)`
: Open editor with default file.
- `void meowim_open_file(const char *filename)`
: Open editor for concrete file.
- `void meowim_close(void)`
: Exit editor and restore shell console state.
- `int meowim_is_active(void)`
: Query editor active flag.
- `void editor_input(const input_event_t *event)`
: Handle events while editor mode is active.

Typical usage:
- Shell `edit` command enters editor mode.
- Input router sends keys to editor while `MODE_EDITOR` is active.

---

## 7. Keyboard Mapping Layer
Source: src/kernel/hardware/keymaps.h

Purpose:
- Translate keyboard scancodes to characters for normal/shift states.

Exposed data:
- `scancode_map[128]`
- `scancode_map_shift[128]`
- Shift scancode constants (`SCANCODE_LSHIFT`, etc.)

Typical usage:
- Used internally by keyboard IRQ handler to produce `input_event_t` values.

---

## Integration Pattern (High-Level Flow)
1. IRQ1 keyboard interrupt captures scancode.
2. Keyboard layer maps scancode and queues semantic input events.
3. `keyboard_poll()` drains queue and calls `input_handle_event(...)`.
4. Input router dispatches to shell or editor based on UI mode.
5. Subsystem writes output through Console API.
6. Timer IRQ updates ticks used by uptime and periodic overlays.

---

## Notes
- These are high-level APIs intended for module integration, not all low-level internals.
- For exact behavior, always verify corresponding `.c` implementations.
