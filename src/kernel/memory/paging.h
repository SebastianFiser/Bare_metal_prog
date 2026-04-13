#pragma once
#include "common.h"

#define PAGE_PRESENT (1<<0)
#define PAGE_RW (1<<1)
#define PAGE_USER (1<<2)
#define PAGE_WRITE_TROUGH (1<<3)
#define PAGE_CACHE_DISABLE (1<<4)
#define PAGE_ACCESED (1<<5)
#define PAGE_DIRTY (1<<6)
#define PAGE_GLOBAL (1<<8)
#define PAGE_FRAME_MASK (0xFFFFF000)

typedef struct {
    uint32_t entries[1024];
} page_table_t;

typedef struct {
    uint32_t entries[1024];
} page_directory_t;

void paging_init(uint32_t fb_base, uint32_t fb_size);
void paging_enable(void);
void paging_debug_lookup(uint32_t vaddr, uint32_t *out_pde, uint32_t *out_pte);
void map_page(uint32_t virt, uint32_t phys, uint32_t flags);
void unmap_page(uint32_t virt);
bool is_mapped(uint32_t virt);
uint32_t translate(uint32_t virt);