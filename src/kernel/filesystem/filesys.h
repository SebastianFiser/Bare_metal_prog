#pragma once

#include "common.h"

typedef enum {
    FS_NODE_FILE,
    FS_NODE_DIR
} fs_node_type_t;

typedef struct fs_node {
    char *name;
    fs_node_type_t type;
    struct fs_node *parent;
    struct fs_node **children;
    unsigned int child_count;
    unsigned int child_capacity;

    char *data;
    unsigned int size;
    unsigned int capacity;
} fs_node_t;

void fs_init(void);
fs_node_t *fs_root(void);
fs_node_t *fs_find_child(fs_node_t *parent, const char *name);
fs_node_t *fs_resolve_path(fs_node_t *start, const char *path);
void fs_get_path(fs_node_t *dir, char *out, int out_size);

int fs_create(fs_node_t *parent, const char* name, fs_node_type_t type);
int fs_write(fs_node_t *cwd, const char* name, const char* data);
int fs_read(fs_node_t *cwd, const char* name, char* out, int out_size);
int fs_list_dir(fs_node_t *dir);

int fs_delete(fs_node_t *cwd, const char *path);
void fs_shutdown(void);