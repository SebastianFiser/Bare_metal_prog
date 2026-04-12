#pragma once
#include "common.h"

typedef struct {
    uint32_t entries[1024];
} page_table_t;

typedef struct {
    uint32_t entries[1024];
} page_directory_t;

void paging_init(uint32_t fb_base, uint32_t fb_size);
void paging_enable(void);
void paging_debug_lookup(uint32_t vaddr, uint32_t *out_pde, uint32_t *out_pte);