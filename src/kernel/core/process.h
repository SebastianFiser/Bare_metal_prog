#pragma once
#include "common.h"
#include "kernel.h" // for struct registers

#define PROCESS_MAX 32
extern process_t g_process_table[PROCESS_MAX];
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
    uint32_t *page_dir_ptr;
        uint32_t esp;
        uint32_t ebp;
        /* saved execution context for this process (used for context switch) */
        struct registers saved_regs;
        /* kernel-allocated stack for the process */
        void *stack;
        size_t stack_size;
} process_t;

void process_init(void);
int process_create(const char *name, void (*entry_point)(void));
void get_curr_process(process_t *out);
process_t *process_get_current(void);
bool process_set_current(int pid);
const process_t *process_table_get(size_t *count_out);
process_t *process_find(int pid);
const process_t *process_find_const(int pid);
int process_kill(int pid);

/* Additional helpers for scheduler integration */
void process_save_current_regs(struct registers *regs);
void process_restore_regs_for_index(int index, struct registers *out_regs);
int process_get_next_ready_index(void);
int process_get_current_index(void);
bool process_set_current_index(int index);