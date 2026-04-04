#include "common.h"
#include "console.h"
#include "filesys.h"
#include "input.h"
#include "meowim.h"
#include "shell.h"

#define SHELL_MAX_ARGS 8
#define SHELL_MAX_TOKEN 32

typedef void (*shell_cmd_handler_t)(int argc, char argv[][SHELL_MAX_TOKEN]);
static fs_node_t *current_dir = 0;

static void shell_ensure_current_dir(void) {
    if (!current_dir) {
        current_dir = fs_root();
    }
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
};

#define SHELL_COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

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

    char buffer[256];
    int result = fs_read(current_dir, argv[1], buffer, sizeof(buffer));
    if (result < 0) {
        console_write_colored(CONSOLE_COLOR_ERROR, "Error reading file: %d\n", result);
        return;
    }

    console_write("%s\n", buffer);
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
    char path[64];
    shell_ensure_current_dir();
    fs_get_path(current_dir, path, sizeof(path));
    console_write("%s>> ", path);
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


