#include "paging.h"

static page_directory_t kernel_pdir __attribute__((aligned(4096)));
static page_table_t kernel_ptabs_all[1024] __attribute__((aligned(4096)));

#define PAGE_SIZE_4K 0x1000U
#define PAGE_SIZE_4M 0x400000U

static void map_4m_region(uint32_t pde_index, uint32_t phys_base, page_table_t *table) {
    kernel_pdir.entries[pde_index] = ((uint32_t)table) | 0x3U;

    for (uint32_t j = 0; j < 1024U; j++) {
        table->entries[j] = (phys_base + (j * PAGE_SIZE_4K)) | 0x3U;
    }
}

void paging_init(uint32_t fb_base, uint32_t fb_size) {
    page_directory_t *pdir = &kernel_pdir;
    (void)fb_base;
    (void)fb_size;

    memset(pdir, 0, sizeof(page_directory_t));
    memset(kernel_ptabs_all, 0, sizeof(kernel_ptabs_all));

    for (uint32_t i = 0; i < 1024U; i++) {
        map_4m_region(i, i * PAGE_SIZE_4M, &kernel_ptabs_all[i]);
    }
}

void paging_enable(void) {
    uint32_t pdir_addr = (uint32_t)&kernel_pdir;
    __asm__ volatile (
        "mov %0, %%cr3 \n\t"
        "mov %%cr0, %%eax \n\t"
        "or $0x80010000, %%eax \n\t"
        "mov %%eax, %%cr0 \n\t"
        :
        : "r"(pdir_addr)
        : "eax", "memory"
    );
}

void paging_debug_lookup(uint32_t vaddr, uint32_t *out_pde, uint32_t *out_pte) {
    uint32_t pde_index = (vaddr >> 22) & 0x3FFU;
    uint32_t pte_index = (vaddr >> 12) & 0x3FFU;
    uint32_t pde = kernel_pdir.entries[pde_index];

    if (out_pde) {
        *out_pde = pde;
    }

    if (out_pte) {
        *out_pte = 0U;
    }

    if ((pde & 0x1U) == 0U) {
        return;
    }

    page_table_t *pt = (page_table_t*)(pde & 0xFFFFF000U);
    if (out_pte) {
        *out_pte = pt->entries[pte_index];
    }
}