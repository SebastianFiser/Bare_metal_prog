#include "process.h"

static process_t g_process_table[PROCESS_MAX];
static size_t g_process_count = 0;
static int g_current_index = -1;
static int g_next_pid = 0;

static void process_name_copy(char *dst, const char *src) {
	size_t i = 0;
	if (src == NULL) {
		dst[0] = '\0';
		return;
	}

	while (src[i] != '\0' && i < 31) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
}

static int process_alloc_slot(void) {
	size_t i;
	for (i = 0; i < PROCESS_MAX; i++) {
		if (g_process_table[i].pid == -1) {
			return (int)i;
		}
	}
	return -1;
}

static void process_fill(process_t *p, const char *name, process_state_t state, int parent_pid, void (*entry_point)(void)) {
	p->pid = g_next_pid++;
	process_name_copy(p->name, name);
	p->state = state;
	p->parent_pid = parent_pid;
	p->stack_top = (uint32_t)entry_point;
	p->page_dir_ptr = NULL;
}

void process_init(void) {
	size_t i;
	g_process_count = 0;
	g_current_index = -1;
	g_next_pid = 0;

	for (i = 0; i < PROCESS_MAX; i++) {
		g_process_table[i].pid = -1;
		g_process_table[i].name[0] = '\0';
		g_process_table[i].state = PROCESS_ZOMBIE;
		g_process_table[i].parent_pid = -1;
		g_process_table[i].stack_top = 0;
		g_process_table[i].page_dir_ptr = NULL;
	}

	process_fill(&g_process_table[0], "idle", PROCESS_READY, -1, NULL);
	process_fill(&g_process_table[1], "kernel", PROCESS_RUNNING, 0, NULL);
	g_process_count = 2;
	g_current_index = 1;
}

void process_create(const char *name, void (*entry_point)(void)) {
	int slot = process_alloc_slot();
	int parent_pid;

	if (slot < 0) {
		return;
	}

	if (g_current_index >= 0) {
		parent_pid = g_process_table[g_current_index].pid;
	} else {
		parent_pid = -1;
	}

	process_fill(&g_process_table[slot], name, PROCESS_READY, parent_pid, entry_point);
	g_process_count++;
}

void get_curr_process(process_t *out) {
	if (out == NULL) {
		return;
	}

	if (g_current_index < 0 || g_current_index >= (int)PROCESS_MAX) {
		out->pid = -1;
		out->name[0] = '\0';
		out->state = PROCESS_ZOMBIE;
		out->parent_pid = -1;
		out->stack_top = 0;
		out->page_dir_ptr = NULL;
		return;
	}

	*out = g_process_table[g_current_index];
}

process_t *process_get_current(void) {
	if (g_current_index < 0 || g_current_index >= (int)PROCESS_MAX) {
		return NULL;
	}
	return &g_process_table[g_current_index];
}

bool process_set_current(int pid) {
	size_t i;
	for (i = 0; i < PROCESS_MAX; i++) {
		if (g_process_table[i].pid == pid) {
			if (g_current_index >= 0 && g_current_index < (int)PROCESS_MAX && g_process_table[g_current_index].pid != -1 && g_process_table[g_current_index].state == PROCESS_RUNNING) {
				g_process_table[g_current_index].state = PROCESS_READY;
			}
			g_current_index = (int)i;
			g_process_table[i].state = PROCESS_RUNNING;
			return true;
		}
	}
	return false;
}

const process_t *process_table_get(size_t *count_out) {
	if (count_out != NULL) {
		*count_out = g_process_count;
	}
	return g_process_table;
}

static const char *process_state_name(process_state_t state) {
    switch (state) {
        case PROCESS_NEW: return "NEW";
        case PROCESS_READY: return "READY";
        case PROCESS_RUNNING: return "RUNNING";
        case PROCESS_BLOCKED: return "BLOCKED";
        case PROCESS_ZOMBIE: return "ZOMBIE";
        default: return "UNKNOWN";
    }
}