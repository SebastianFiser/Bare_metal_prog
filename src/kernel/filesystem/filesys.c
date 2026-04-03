#include "filesys.h"
#include "common.h"
#include "console.h"

#define MAX_FILES 16

struct file {
    char name[32];
    char data[512];
    int size;
    int used;
};

static struct file files[MAX_FILES];

static int fs_strlen(const char* str) {
    int len = 0;
    if (!str)
        return 0;

    while (str[len] != '\0') {
        len++;
    }
    return len;
}

void fs_copy_string(char *dst, const char *src, int max_len) {
    if (!dst || !src || max_len <= 0) {
        return;
    }

    int i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int fs_find_index(const char* name) {
    if (!name || name[0] == '\0') {
        return -1;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int fs_find_free_slot(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            return i;
        }
    }
    return -1;
}

int fs_create(const char* name) {
    int slot;

    if (!name || name[0] == '\0') {
        return -1;
    }

    if (fs_strlen(name) >= (int)sizeof(files[0].name)) {
        return -1;
    }

    if (fs_find_index(name) >= 0) {
        return -2;
    }

    slot = fs_find_free_slot();
    if (slot < 0) {
        return -3;
    }

    files[slot].used = 1;
    files[slot].size = 0;
    memset(files[slot].data, 0, sizeof(files[slot].data));
    fs_copy_string(files[slot].name, name, sizeof(files[slot].name));
    return 0;
}

int fs_write(const char* name, const char* data) {
    int index;
    int data_len;

    if (!name || !data) {
        return -1;
    }

    index = fs_find_index(name);
    if (index < 0) {
        return -2;
    }

    data_len = fs_strlen(data);
    if (data_len >= (int)sizeof(files[index].data)) {
        data_len = sizeof(files[index].data) - 1;
    }

    memset(files[index].data, 0, sizeof(files[index].data));
    memcpy(files[index].data, data, (size_t)data_len);
    files[index].data[data_len] = '\0';
    files[index].size = data_len;

    return data_len;
}

int fs_read(const char* name, char* out, int out_size) {
    int index;

    if (!name || !out || out_size <= 0) {
        return -1;
    }

    index = fs_find_index(name);
    if (index < 0) {
        return -2;
    }

    if (files[index].size + 1 > out_size) {
        return -3;
    }

    memcpy(out, files[index].data, (size_t)files[index].size + 1);
    return files[index].size;
}

int fs_list(void) {
    int count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            continue;
        }

        console_write(files[i].name);
        console_write(" (");
        console_print_dec(files[i].size);
        console_write(" bytes)\n");
        count++;
    }
    return count;
}