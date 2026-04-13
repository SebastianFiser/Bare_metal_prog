#pragma once
#include "common.h"

#define PROCESS_MAX 32

typedef enum {
    PROCESS_NEW,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_ZOMBIE
} process_state_t;

typedef struct process {
    int pid;
    char name[32];
    process_state_t state;
    int parent_pid;
    uint32_t stack_top;
    uint32_t *page_dir_ptr;
} process_t;

void process_init(void);
void process_create(const char *name, void (*entry_point)(void));
void get_curr_process(process_t *out);
process_t *process_get_current(void);
bool process_set_current(int pid);
const process_t *process_table_get(size_t *count_out);