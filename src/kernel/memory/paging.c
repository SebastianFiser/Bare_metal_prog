extern unsigned char __kernel_start[];
extern unsigned char __kernel_end[];
#include "paging.h"

static page_directory_t kernel_pdir __attribute__((aligned(4096)));
static page_table_t kernel_ptabs_all[1024] __attribute__((aligned(4096)));

#define PAGE_SIZE_4K 0x1000U
#define PAGE_SIZE_4M 0x400000U

static inline uint32_t page_align_down(uint32_t addr) {
    return addr & PAGE_FRAME_MASK;
}

static inline uint32_t page_align_up(uint32_t addr) {
    return (addr + (PAGE_SIZE_4K - 1U)) & PAGE_FRAME_MASK;
}

static void map_identity_range(uint32_t start, uint32_t end, uint32_t flags) {
    uint32_t curr = page_align_down(start);
    uint32_t limit = page_align_up(end);

    while (curr < limit) {
        map_page(curr, curr, flags);
        curr += PAGE_SIZE_4K;
    }
}



static inline void invlpg(uint32_t vaddr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(vaddr) : "memory");
}

static page_table_t *get_table(uint32_t pde_index, int create, uint32_t flags) {
    uint32_t pde = kernel_pdir.entries[pde_index];

    if ((pde & PAGE_PRESENT) != 0U) {
        return (page_table_t*)(pde & PAGE_FRAME_MASK);
    }

    if (!create) {
        return NULL;
    }

    memset(&kernel_ptabs_all[pde_index], 0, sizeof(page_table_t));
    kernel_pdir.entries[pde_index] = ((uint32_t)&kernel_ptabs_all[pde_index])
        | PAGE_PRESENT
        | PAGE_RW
        | (flags & PAGE_USER);

    return &kernel_ptabs_all[pde_index];
}

void paging_init(uint32_t fb_base, uint32_t fb_size) {
    page_directory_t *pdir = &kernel_pdir;
    uint32_t kernel_start = (uint32_t)__kernel_start;
    uint32_t kernel_end = (uint32_t)__kernel_end;

    memset(pdir, 0, sizeof(page_directory_t));
    memset(kernel_ptabs_all, 0, sizeof(kernel_ptabs_all));

    map_identity_range(kernel_start, kernel_end, PAGE_RW);

    if (fb_base != 0U) {
        uint32_t map_size = (fb_size != 0U) ? fb_size : PAGE_SIZE_4K;
        uint32_t fb_end = fb_base + map_size;

        if (map_size < (16U * 1024U * 1024U)) {
            map_size = 16U * 1024U * 1024U;
            fb_end = fb_base + map_size;
        }

        if (map_size > (64U * 1024U * 1024U)) {
            map_size = 64U * 1024U * 1024U;
            fb_end = fb_base + map_size;
        }

        if (fb_end < fb_base) {
            fb_end = 0xFFFFFFFFU;
        }

        map_identity_range(fb_base, fb_end, PAGE_RW | PAGE_CACHE_DISABLE);
    }

    unmap_page(0x00000000U);
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

void map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pde_index = (virt >> 22) & 0x3FFU;
    uint32_t pte_index = (virt >> 12) & 0x3FFU;
    page_table_t *pt = get_table(pde_index, 1, flags);
    uint32_t entry_flags = (flags & ~PAGE_FRAME_MASK) | PAGE_PRESENT;

    pt->entries[pte_index] = (phys & PAGE_FRAME_MASK) | entry_flags;
    invlpg(virt);
}

void unmap_page(uint32_t virt) {
    uint32_t pde_index = (virt >> 22) & 0x3FFU;
    uint32_t pte_index = (virt >> 12) & 0x3FFU;
    page_table_t *pt = get_table(pde_index, 0, 0U);

    if (!pt) {
        return;
    }

    pt->entries[pte_index] = 0U;
    invlpg(virt);
}

bool is_mapped(uint32_t virt) {
    uint32_t pde_index = (virt >> 22) & 0x3FFU;
    uint32_t pte_index = (virt >> 12) & 0x3FFU;
    uint32_t pde = kernel_pdir.entries[pde_index];

    if ((pde & PAGE_PRESENT) == 0U) {
        return false;
    }

    page_table_t *pt = (page_table_t*)(pde & PAGE_FRAME_MASK);
    return (pt->entries[pte_index] & PAGE_PRESENT) != 0U;
}

uint32_t translate(uint32_t virt) {
    uint32_t pde_index = (virt >> 22) & 0x3FFU;
    uint32_t pte_index = (virt >> 12) & 0x3FFU;
    uint32_t page_offset = virt & 0xFFFU;
    uint32_t pde = kernel_pdir.entries[pde_index];

    if ((pde & PAGE_PRESENT) == 0U) {
        return 0U;
    }

    page_table_t *pt = (page_table_t*)(pde & 0xFFFFF000U);
    uint32_t pte = pt->entries[pte_index];

    if ((pte & PAGE_PRESENT) == 0U) {
        return 0U;
    }

    return (pte & PAGE_FRAME_MASK) | page_offset;
}