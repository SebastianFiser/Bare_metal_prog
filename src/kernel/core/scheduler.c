#include "scheduler.h"
#include "process.h"
#include "console.h"

void scheduler_init(void) {
    /* nothing yet */
}

void scheduler_tick(struct registers *regs) {
    if (regs == NULL) return;

    int curr = process_get_current_index();
    if (curr < 0) return;

    /* save current CPU frame into the current process */
    process_save_current_regs(regs);

    /* pick next ready process */
    int next = process_get_next_ready_index();
    if (next < 0) return; // žádný ready proces

    process_t *p = &g_process_table[next];
    console_write("Switching to PID %d: eip=0x%x esp=0x%x cs=0x%x ss=0x%x\n",
        p->pid, p->saved_regs.eip, p->saved_regs.useresp, p->saved_regs.cs, p->saved_regs.ss);

    process_set_current_index(next);
    process_restore_regs_for_index(next, regs);

    /* debug */
    console_write("[scheduler] switch to pid=%d name=%s\n", process_get_current()->pid, process_get_current()->name);
}
#include "scheduler.h"