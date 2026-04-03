#pragma once

int fs_create(const char* name);
int fs_write(const char* name, const char* data);
int fs_read(const char* name, char* out, int out_size);
int fs_list(void);