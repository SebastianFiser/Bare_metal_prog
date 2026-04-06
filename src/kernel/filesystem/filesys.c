#include "filesys.h"
#include "common.h"
#include "console.h"
#include "heap.h"

static fs_node_t *root = 0;

static int fs_strlen_local(const char* str) {
    int len = 0;
    if (!str) {
        return 0;
    }
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

static int fs_has_slash(const char *name) {
    if (!name) {
        return 0;
    }
    while (*name) {
        if (*name == '/') {
            return 1;
        }
        name++;
    }
    return 0;
}

static fs_node_t *fs_alloc_node(void) {
    fs_node_t *node = (fs_node_t *)kmalloc(sizeof(fs_node_t));
    if (!node) return 0;
    memset(node, 0, sizeof(fs_node_t));
    return node;
}

fs_node_t *fs_root(void) {
    return root;
}

fs_node_t *fs_find_child(fs_node_t *parent, const char *name) {
    if (!parent || parent->type != FS_NODE_DIR || !name || !name[0]) {
        return 0;
    }

    for (unsigned int i = 0; i < parent->child_count; i++) {
        if (strcmp(parent->children[i]->name, name) == 0) {
            return parent->children[i];
        }
    }
    return 0;
}

fs_node_t *fs_resolve_path(fs_node_t *start, const char *path) {
    fs_node_t *node = start ? start : root;

    if (!node) {
        return 0;
    }

    if (!path || !path[0]) {
        return node;
    }

    if (path[0] == '/') {
        node = root;
        path++;
    }

    while (*path) {
        char part[32];
        unsigned int len = 0;

        while (*path == '/') {
            path++;
        }
        if (!*path) {
            break;
        }

        while (*path && *path != '/' && len < (sizeof(part) - 1)) {
            part[len++] = *path++;
        }
        part[len] = '\0';

        if (strcmp(part, ".") == 0) {
            continue;
        }

        if (strcmp(part, "..") == 0) {
            if (node->parent) {
                node = node->parent;
            }
            continue;
        }

        node = fs_find_child(node, part);
        if (!node) {
            return 0;
        }
    }

    return node;
}

int fs_create(fs_node_t *parent, const char* name, fs_node_type_t type) {
    fs_node_t *node;

    if (!parent || parent->type != FS_NODE_DIR || !name || !name[0]) {
        return -1;
    }

    if (fs_has_slash(name) || fs_strlen_local(name) >= (int)sizeof(parent->name)) {
        return -1;
    }

    if (parent->child_count >= (sizeof(parent->children) / sizeof(parent->children[0]))) {
        return -3;
    }

    if (fs_find_child(parent, name)) {
        return -2;
    }

    node = fs_alloc_node();
    if (!node) {
        return -3;
    }

    strcpy(node->name, name);
    node->type = type;
    node->parent = parent;
    node->size = 0;
    node->data = 0;
    node->capacity = 0;
    parent->children[parent->child_count++] = node;

    return 0;
}

int fs_write(fs_node_t *cwd, const char* name, const char* data) {
    fs_node_t *node;
    int data_len;
    unsigned int needed;
    char *new_buf;

    if (!cwd || !name || !data) {
        return -1;
    }

    node = fs_resolve_path(cwd, name);
    if (!node || node->type != FS_NODE_FILE) {
        return -2;
    }

    data_len = fs_strlen_local(data);
    if (data_len < 0) {
        return -1;
    }

    needed = (unsigned int)data_len + 1; // include '\0'

    if (node->capacity < needed) {
        new_buf = (char*)kmalloc(needed);
        if (!new_buf) {
            return -3; // no memory
        }

        if (node->data && node->size > 0) {
            memcpy(new_buf, node->data, node->size);
        }

        if (node->data) {
            kfree(node->data);
        }

        node->data = new_buf;
        node->capacity = needed;
    }

    memcpy(node->data, data, (size_t)data_len);
    node->data[data_len] = '\0';
    node->size = (unsigned int)data_len;

    return data_len;
}

int fs_read(fs_node_t *cwd, const char* name, char* out, int out_size) {
    fs_node_t *node;

    if (!cwd || !name || !out || out_size <= 0) {
        return -1;
    }

    node = fs_resolve_path(cwd, name);
    if (!node || node->type != FS_NODE_FILE) {
        return -2;
    }

    if(!node->data) {
        if (out_size < 1) return -3;
        out[0] = '\0';
        return 0;

    }

    if ((int)node->size + 1 > out_size) {
        return -3;
    }

    memcpy(out, node->data, (size_t)node->size + 1);
    return (int)node->size;
}

int fs_list_dir(fs_node_t *dir) {
    if (!dir || dir->type != FS_NODE_DIR) {
        return -1;
    }

    for (unsigned int i = 0; i < dir->child_count; i++) {
        if (dir->children[i]->type == FS_NODE_DIR) {
            console_write_colored(CONSOLE_COLOR_DIR, "[DIR] %s\n", dir->children[i]->name);
        } else {
            console_write("%s\n", dir->children[i]->name);
        }
    }

    return (int)dir->child_count;
}

void fs_get_path(fs_node_t *dir, char *out, int out_size) {
    fs_node_t *stack[16];
    int depth = 0;

    if (!out || out_size <= 0) {
        return;
    }

    if (!dir) {
        dir = root;
    }

    if (!dir || dir == root) {
        out[0] = '/';
        out[1] = '\0';
        return;
    }

    while (dir && dir != root && depth < (int)(sizeof(stack) / sizeof(stack[0]))) {
        stack[depth++] = dir;
        dir = dir->parent;
    }

    int pos = 0;
    out[pos++] = '/';

    for (int i = depth - 1; i >= 0; i--) {
        const char *name = stack[i]->name;
        int j = 0;
        while (name[j] && pos < out_size - 1) {
            out[pos++] = name[j++];
        }
        if (i > 0 && pos < out_size - 1) {
            out[pos++] = '/';
        }
    }

    out[pos] = '\0';
}

void fs_init(void) {
    root = fs_alloc_node();
    if (!root) {
        return;
    }

    strcpy(root->name, "/");
    root->type = FS_NODE_DIR;
    root->parent = 0;

    fs_create(root, "home", FS_NODE_DIR);
    fs_create(root, "tmp", FS_NODE_DIR);
    fs_create(root, "readme.txt", FS_NODE_FILE);
}

static int fs_find_child_index(fs_node_t *parent, fs_node_t *child) {
    unsigned int i;
    if (!parent || !child) return -1;
    for (i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) return (int)i;
    }
    return -1;
}

static void fs_free_subtree(fs_node_t *node) {
    while (node->child_count > 0) {
        fs_node_t *last = node->children[node->child_count - 1];
        fs_free_subtree(last);
        node->child_count--;
    }

    if (node->data) {
        kfree(node->data);
        node->data = 0;
        node->capacity = 0;
        node->size = 0;
    }

    kfree(node);
}

int fs_delete(fs_node_t *cwd, const char *path) {
    fs_node_t *node;
    fs_node_t *parent;
    int idx;
    unsigned int i;

    if (!cwd || !path || !path[0]) return -1;

    node = fs_resolve_path(cwd, path);
    if (!node || node == root) return -2;

    parent = node->parent;
    if (!parent) return -2;

    idx = fs_find_child_index(parent, node);
    if (idx < 0) return -2;

    for (i = (unsigned int)idx; i + 1 < parent->child_count; i++) {
        parent->children[i] = parent->children[i + 1];
    }
    parent->child_count--;

    fs_free_subtree(node);
    return 0;
}
