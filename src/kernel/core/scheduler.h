#pragma once
#include "kernel.h"

void scheduler_init(void);
void scheduler_tick(struct registers *regs);