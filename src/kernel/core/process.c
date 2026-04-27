#include "process.h"
#include "../../memory/heap.h"

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
	const size_t default_stack = 0x4000; // 16 KiB
	p->pid = g_next_pid++;
	process_name_copy(p->name, name);
	p->state = state;
	p->parent_pid = parent_pid;
	p->page_dir_ptr = NULL;

	p->stack = NULL;
	p->stack_size = 0;

		if (entry_point != NULL) {
		if (p->stack != NULL) {
			p->stack_size = default_stack;
			/* prepare initial saved register frame so a context-restore will start at entry_point */
			/* zero registers */
			for (size_t i = 0; i < sizeof(struct registers); i++) {
				((uint8_t *)&p->saved_regs)[i] = 0;
			}
			p->saved_regs.eip = (uint32_t)entry_point;
			p->saved_regs.cs = 0x08; /* kernel code segment */
			p->saved_regs.eflags = 0x202;
			/* set useresp to top of allocated stack (grow-down) */
			p->saved_regs.useresp = (uint32_t)p->stack + (uint32_t)p->stack_size - 4;
			p->saved_regs.ss = 0x10; /* kernel data segment */
		}
	}
}

static void idle_proc(void) {
	while(1) {
		__asm__ volatile ("hlt");
	}
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
		g_process_table[i].stack = NULL;
		g_process_table[i].stack_size = 0;
		g_process_table[i].page_dir_ptr = NULL;
		/* zero saved regs */
		for (size_t j = 0; j < sizeof(struct registers); j++) {
			((uint8_t *)&g_process_table[i].saved_regs)[j] = 0;
		}
	}

	process_fill(&g_process_table[0], "idle", PROCESS_READY, -1, idle_proc);
	process_fill(&g_process_table[1], "kernel", PROCESS_RUNNING, 0, NULL);
	g_process_count = 2;
	g_current_index = 1;
}

int process_create(const char *name, void (*entry_point)(void)) {
	int slot = process_alloc_slot();
	int parent_pid;
    int pid;

	if (slot < 0) {
		return -1;
	}

	if (g_current_index >= 0) {
		parent_pid = g_process_table[g_current_index].pid;
	} else {
		parent_pid = -1;
	}

	process_fill(&g_process_table[slot], name, PROCESS_READY, parent_pid, entry_point);
	g_process_count++;

    pid = g_process_table[slot].pid;
    return pid;
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
		out->stack = NULL;
		out->stack_size = 0;
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

process_t *process_find(int pid) {
    size_t i;
    for (i = 0; i < g_process_count; i++) {
        if (g_process_table[i].pid == pid) {
            return &g_process_table[i];
        }
    }
    return NULL;
}

void process_save_current_regs(struct registers *regs) {
	if (g_current_index < 0 || g_current_index >= (int)PROCESS_MAX || regs == NULL) return;
	g_process_table[g_current_index].saved_regs = *regs;
}

void process_restore_regs_for_index(int index, struct registers *out_regs) {
	if (index < 0 || index >= (int)PROCESS_MAX || out_regs == NULL) return;
	*out_regs = g_process_table[index].saved_regs;
}

int process_get_next_ready_index(void) {
	if (g_process_count == 0) return -1;
	int start = g_current_index;
	int i = start;
	for (size_t cnt = 0; cnt < g_process_count; cnt++) {
		i = (i + 1) % (int)g_process_count;
		if (g_process_table[i].pid >= 0 && g_process_table[i].state == PROCESS_READY) {
			return i;
		}
	}
	return -1;
}

int process_get_current_index(void) {
	return g_current_index;
}

bool process_set_current_index(int index) {
	if (index < 0 || index >= (int)PROCESS_MAX) return false;
	if (g_current_index >= 0 && g_current_index < (int)PROCESS_MAX && g_process_table[g_current_index].pid != -1 && g_process_table[g_current_index].state == PROCESS_RUNNING) {
		g_process_table[g_current_index].state = PROCESS_READY;
	}
	g_current_index = index;
	g_process_table[index].state = PROCESS_RUNNING;
	return true;
}

const process_t *process_find_const(int pid) {
    return process_find(pid);
}

int process_kill(int pid) {
	size_t i;
	process_t *victim;

	if (pid == 0 || pid == 1) {
		return -2;
	}

	victim = process_find(pid);
	if (victim == NULL) {
		return -1;
	}

	if (victim->state == PROCESS_ZOMBIE) {
		return -3;
	}

	victim->state = PROCESS_ZOMBIE;
	victim->esp = 0;
	victim->ebp = 0;
	victim->stack = NULL;
	victim->stack_size = 0;
	victim->page_dir_ptr = NULL;

	if (g_current_index >= 0 && g_current_index < (int)PROCESS_MAX && g_process_table[g_current_index].pid == pid) {
		for (i = 0; i < g_process_count; i++) {
			if (g_process_table[i].pid >= 0 && g_process_table[i].state == PROCESS_READY) {
				process_set_current(g_process_table[i].pid);
				return 0;
			}
		}

		g_current_index = -1;
	}

	return 0;
}