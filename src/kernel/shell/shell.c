#include "common.h"
#include "console.h"
#include "filesys.h"
#include "input.h"
#include "meowim.h"
#include "shell.h"
#include "heap.h"
#include "renderer.h"
#include "screen.h"

#define SHELL_MAX_ARGS 8
#define SHELL_MAX_TOKEN 32

#define SHELL_READ_BUF_SIZE 256
#define SHELL_PATH_BUF_SIZE 128

static char *shell_read_buf;
static char *shell_path_buf;

typedef void (*shell_cmd_handler_t)(int argc, char argv[][SHELL_MAX_TOKEN]);
static fs_node_t *current_dir = 0;

static int shell_ensure_buffers(void) {
    if (!shell_read_buf) {
        shell_read_buf = (char*)kcalloc_tag(sizeof(char), SHELL_READ_BUF_SIZE, "shell_read_buf");
        if (!shell_read_buf) {
            return 0;
        }
    }

    if (!shell_path_buf) {
        shell_path_buf = (char*)kcalloc_tag(sizeof(char), SHELL_PATH_BUF_SIZE, "shell_path_buf");
        if (!shell_path_buf) {
            return 0;
        }
    }

    return 1;
}

static int shell_ensure_current_dir(void) {
    if (!current_dir) {
        current_dir = fs_root();
        if (!current_dir) return 0;
    }
    return 1;
}

fs_node_t *shell_get_cwd(void) {
    shell_ensure_current_dir();
    return current_dir;
}

typedef struct {
    const char *name;
    const char *help;
    shell_cmd_handler_t handler;
} shell_command_t;

static int streq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

static int shell_tokenize(const char *in, char argv[][SHELL_MAX_TOKEN]) {
    int argc = 0;
    int i = 0;

    while (in[i] != '\0' && argc < SHELL_MAX_ARGS) {
        while (in[i] == ' ') {
            i++;
        }
        if (in[i] == '\0') {
            break;
        }

        int j = 0;
        while (in[i] != '\0' && in[i] != ' ' && j < (SHELL_MAX_TOKEN - 1)) {
            argv[argc][j++] = in[i++];
        }
        argv[argc][j] = '\0';
        argc++;

        while (in[i] != '\0' && in[i] != ' ') {
            i++;
        }
    }

    return argc;
}

static void cmd_help(int argc, char argv[][SHELL_MAX_TOKEN]);
static void whatisthis(int argc, char argv[][SHELL_MAX_TOKEN]);
static void whoami(int argc, char argv[][SHELL_MAX_TOKEN]);
static void cmd_echo(int argc, char argv[][SHELL_MAX_TOKEN]);
static void cmd_clear(int argc, char argv[][SHELL_MAX_TOKEN]);
static void print_files(int argc, char argv[][SHELL_MAX_TOKEN]);
static void read_file(int argc, char argv[][SHELL_MAX_TOKEN]);
static void make_file(int argc, char argv[][SHELL_MAX_TOKEN]);
static void edit_file(int argc, char argv[][SHELL_MAX_TOKEN]);
static void cmd_cd(int argc, char argv[][SHELL_MAX_TOKEN]);
static void cmd_mkdir(int argc, char argv[][SHELL_MAX_TOKEN]);
static void cmd_heap_dump(int argc, char argv[][SHELL_MAX_TOKEN]);
static void cmd_heap_validate(int argc, char argv[][SHELL_MAX_TOKEN]);
static void cmd_fs_stress(int argc, char argv[][SHELL_MAX_TOKEN]);
static int parse_uint_arg(const char *s, unsigned int *out);
static void build_test_name(const char *prefix, unsigned int idx, char *out, unsigned int out_size);

static const shell_command_t commands[] = {
    {"help", "list built-in commands", cmd_help},
    {"whoami", "display current user", whoami},
    {"whatisthis", "tell generic info about his project", whatisthis},
    {"echo", "display a line of text written in a second argument", cmd_echo},
    {"clear", "clear the screen", cmd_clear},
    {"ls", "list files in the file system", print_files},
    {"cat", "display the contents of a file", read_file},
    {"makef", "create an empty file, usage: makef <name>", make_file},
    {"edit", "open meowim editor, usage: edit <filename>", edit_file},
    {"cd", "change the current directory", cmd_cd},
    {"mkdir", "create a new directory", cmd_mkdir},
    {"fstress", "run RAM FS stress test, usage: fstress <iterations>", cmd_fs_stress},
    {"memdump", "dump heap metadata for debugging", cmd_heap_dump},
    {"memvalidate", "validate heap integrity for debugging", cmd_heap_validate},
};

#define SHELL_COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

static void cmd_heap_dump(int argc, char argv[][SHELL_MAX_TOKEN]) {
    heap_dump();
}

static void cmd_heap_validate(int argc, char argv[][SHELL_MAX_TOKEN]) {
    heap_validate();
}

static int parse_uint_arg(const char *s, unsigned int *out) {
    unsigned int value = 0;

    if (!s || !s[0] || !out) {
        return 0;
    }

    for (unsigned int i = 0; s[i] != '\0'; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return 0;
        }
        value = value * 10 + (unsigned int)(s[i] - '0');
    }

    *out = value;
    return 1;
}

static void build_test_name(const char *prefix, unsigned int idx, char *out, unsigned int out_size) {
    char digits[10];
    unsigned int dcount = 0;
    unsigned int pos = 0;

    if (!out || out_size == 0) {
        return;
    }

    while (*prefix && pos < (out_size - 1)) {
        out[pos++] = *prefix++;
    }

    if (idx == 0) {
        if (pos < (out_size - 1)) {
            out[pos++] = '0';
        }
    } else {
        while (idx > 0 && dcount < sizeof(digits)) {
            digits[dcount++] = (char)('0' + (idx % 10));
            idx /= 10;
        }
        while (dcount > 0 && pos < (out_size - 1)) {
            out[pos++] = digits[--dcount];
        }
    }

    out[pos] = '\0';
}

static void cmd_fs_stress(int argc, char argv[][SHELL_MAX_TOKEN]) {
    unsigned int iterations = 100;
    unsigned int ok = 0;
    unsigned int fail = 0;
    char *dname = 0;
    char *fname = 0;
    char *read_back = 0;
    const char *payload = "ramfs_stress_payload";

    shell_ensure_current_dir();

    if (argc >= 2) {
        if (!parse_uint_arg(argv[1], &iterations) || iterations == 0) {
            console_write_colored(CONSOLE_COLOR_ERROR, "Usage: fstress <positive_number>\n");
            return;
        }
    }

    if (iterations > 1000) {
        iterations = 1000;
    }

    /* Allocate temp buffers on heap */
    dname = (char*)kmalloc_tag(24, "fstress_dname");
    fname = (char*)kmalloc_tag(24, "fstress_fname");
    read_back = (char*)kmalloc_tag(32, "fstress_read_back");
    
    if (!dname || !fname || !read_back) {
        console_write_colored(CONSOLE_COLOR_ERROR, "fstress: out of memory\n");
        if (dname) kfree(dname);
        if (fname) kfree(fname);
        if (read_back) kfree(read_back);
        return;
    }

    console_write("fstress: starting %d iterations\n", iterations);

    for (unsigned int i = 0; i < iterations; i++) {
        build_test_name("d", i, dname, 24);
        build_test_name("f", i, fname, 24);

        if (fs_create(current_dir, dname, FS_NODE_DIR) < 0) {
            fail++;
            continue;
        }

        if (fs_create(current_dir, fname, FS_NODE_FILE) < 0) {
            fs_delete(current_dir, dname);
            fail++;
            continue;
        }

        if (fs_write(current_dir, fname, payload) < 0) {
            fs_delete(current_dir, fname);
            fs_delete(current_dir, dname);
            fail++;
            continue;
        }

        if (fs_read(current_dir, fname, read_back, 32) < 0) {
            fs_delete(current_dir, fname);
            fs_delete(current_dir, dname);
            fail++;
            continue;
        }

        if (fs_delete(current_dir, fname) < 0) {
            fs_delete(current_dir, dname);
            fail++;
            continue;
        }

        if (fs_delete(current_dir, dname) < 0) {
            fail++;
            continue;
        }

        ok++;
    }

    console_write("fstress: ok=%d fail=%d\n", ok, fail);
    
    /* Free temp buffers */
    kfree(dname);
    kfree(fname);
    kfree(read_back);
    
    heap_validate();
}

static void cmd_mkdir(int argc, char argv[][SHELL_MAX_TOKEN]) {
    fs_node_t *existing;

    shell_ensure_current_dir();

    if (argc < 2) {
        console_write_colored(CONSOLE_COLOR_ERROR, "Usage: mkdir <directory>\n");
        return;
    }

    if (fs_create(current_dir, argv[1], FS_NODE_DIR) < 0) {
        existing = fs_find_child(current_dir, argv[1]);
        if (existing && existing->type == FS_NODE_DIR) {
            console_write_colored(CONSOLE_COLOR_ERROR, "mkdir: '%s' already exists as directory\n", argv[1]);
        } else if (existing && existing->type == FS_NODE_FILE) {
            console_write_colored(CONSOLE_COLOR_ERROR, "mkdir: '%s' already exists as file\n", argv[1]);
        } else {
            console_write_colored(CONSOLE_COLOR_ERROR, "mkdir: cannot create '%s'\n", argv[1]);
        }
        return;
    }

    console_write("Directory '%s' created\n", argv[1]);
}

static void cmd_cd(int argc, char argv[][SHELL_MAX_TOKEN]) {
    shell_ensure_current_dir();

    if (argc < 2) {
        console_write_colored(CONSOLE_COLOR_ERROR, "Usage: cd <directory>\n");
        return;
    }

    fs_node_t *target = fs_resolve_path(current_dir, argv[1]);
    if (!target || target->type != FS_NODE_DIR) {
        console_write_colored(CONSOLE_COLOR_ERROR, "Directory not found: %s\n", argv[1]);
        return;
    }

    current_dir = target;
}

static void edit_file(int argc, char argv[][SHELL_MAX_TOKEN]) {
    if (argc >= 2) {
        meowim_open_file(argv[1]);
    } else {
        meowim_open();
    }
}

static void print_files(int argc, char argv[][SHELL_MAX_TOKEN]) {
    (void)argc;
    (void)argv;

    shell_ensure_current_dir();
    console_write_colored(CONSOLE_COLOR_INFO, "listing current directory only:\n");
    fs_list_dir(current_dir);
}

static void read_file(int argc, char argv[][SHELL_MAX_TOKEN]) {
    shell_ensure_current_dir();

    if (argc < 2) {
        console_write_colored(CONSOLE_COLOR_ERROR, "Usage: cat <filename>\n");
        return;
    }

    if (!shell_ensure_buffers()) {
        console_write_colored(CONSOLE_COLOR_ERROR, "cat: out of memmory\n");
        return;
    }

    int result = fs_read(current_dir, argv[1], shell_read_buf, SHELL_READ_BUF_SIZE);
    if (result < 0) {
        console_write_colored(CONSOLE_COLOR_ERROR, "cat: cannot read file: %d\n", result);
        return;
    }

    console_write("%s\n", shell_read_buf);
}

static void make_file(int argc, char argv[][SHELL_MAX_TOKEN]) {
    fs_node_t *existing;

    shell_ensure_current_dir();

    if (argc < 2) {
        console_write_colored(CONSOLE_COLOR_ERROR, "Usage: makef <filename>\n");
        return;
    }

    if (fs_create(current_dir, argv[1], FS_NODE_FILE) < 0) {
        existing = fs_find_child(current_dir, argv[1]);
        if (existing && existing->type == FS_NODE_DIR) {
            console_write_colored(CONSOLE_COLOR_ERROR, "makef: '%s' already exists as directory\n", argv[1]);
            console_write_colored(CONSOLE_COLOR_ERROR, "hint: cd %s && makef <new_name>\n", argv[1]);
        } else if (existing && existing->type == FS_NODE_FILE) {
            console_write_colored(CONSOLE_COLOR_ERROR, "makef: '%s' already exists as file\n", argv[1]);
        } else {
            console_write_colored(CONSOLE_COLOR_ERROR, "makef: cannot create '%s'\n", argv[1]);
        }
        return;
    }

    console_write("File '%s' created\n", argv[1]);
}

static void cmd_clear(int argc, char argv[][SHELL_MAX_TOKEN]) {
    (void)argc;
    (void)argv;

    clear_screen(0x0F);
}

static void cmd_echo(int argc, char argv[][SHELL_MAX_TOKEN]) {
    if (argc < 2) {
        console_write("\n");
        return;
    }

    for (int i = 1; i < argc; i++) {
        console_write(argv[i]);
        if (i < argc - 1) {
            console_write(" ");
        }
    }
    console_write("\n");
}
static void whatisthis(int argc, char argv[][SHELL_MAX_TOKEN]) {
    (void)argc;
    (void)argv;

    console_write("This is a simple hobby OS kernel written in C, with a custom VGA text mode \nconsole and basic shell functionality.\nIt demonstrates low-level programming concepts such as memory management,\ninterrupt handling, and hardware interaction.\nMade by Sebastian.F. This is my first larger C project.\nPlease help me save up some cookies for new headphones,\nby rating this project well, thanks!\n");
}

static void whoami(int argc, char argv[][SHELL_MAX_TOKEN]) {
    (void)argc;
    (void)argv;

    console_write("You are a user of this kernel!\n");
}

static void cmd_help(int argc, char argv[][SHELL_MAX_TOKEN]) {
    (void)argc;
    (void)argv;

    for (unsigned int i = 0; i < SHELL_COMMAND_COUNT; i++) {
        console_write(commands[i].name);
        console_write(" - ");
        console_write(commands[i].help);
        console_write("\n");
    }
}

void shell_prompt(void) {
    shell_ensure_current_dir();

    if (!shell_ensure_buffers()) {
        console_write_colored(CONSOLE_COLOR_ERROR, "shell: out of memmory\n");
        return;
    }

    fs_get_path(current_dir, shell_path_buf, SHELL_PATH_BUF_SIZE);
    console_write_colored(CONSOLE_COLOR_INFO, "%s> ", shell_path_buf);
}

void shell_execute_command(const char *cmdline) {
    char argv[SHELL_MAX_ARGS][SHELL_MAX_TOKEN];
    int argc = shell_tokenize(cmdline, argv);

    if (argc == 0) {
        shell_prompt();
        return;
    }

    for (unsigned int i = 0; i < SHELL_COMMAND_COUNT; i++) {
        if (streq(argv[0], commands[i].name)) {
            commands[i].handler(argc, argv);
            if (input_get_mode() == MODE_SHELL) {
                shell_prompt();
            }
            return;
        }
    }

    console_write(argv[0]);
    console_write_colored(CONSOLE_COLOR_ERROR, ": command not found\n");
    shell_prompt();
}


