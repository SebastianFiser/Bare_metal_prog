#include "heap.h"
#include "console.h"

__attribute__((aligned(8)))
static unsigned char heap[HEAP_SIZE];
static block_t* free_list;

static int ptr_in_heap(const void *ptr) {
    const unsigned char *p = (const unsigned char*)ptr;
    const unsigned char *start = &heap[0];
    const unsigned char *end = &heap[HEAP_SIZE];
    return p >= start && p < end;
}

void heap_init() {
    free_list = (block_t*)heap;

    free_list->size = HEAP_SIZE - sizeof(block_t);
    free_list->free = 1;
    free_list->next = NULL;
}

//size = (size + 7) & ~7;
void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    //align size
    size = (size + 7) & ~7;

    block_t* curr = free_list;

    while(curr) {
        if(curr->free && curr->size >= size) {
            if (curr->size >= size + sizeof(block_t) + 8) {
                block_t* new_block = (block_t*)((char*)(curr + 1) + size);
                new_block->size = curr->size - size - sizeof(block_t);
                new_block->free = 1;
                new_block->next = curr->next;
                new_block->magic = HEAP_MAGIC;
                curr->size = size;
                curr->next = new_block;
            }
            curr->free = 0;
            curr->magic = HEAP_MAGIC;
            return (void*)(curr+1);
        }
        curr = curr->next; 
    }
    return NULL;
}

void kfree(void* ptr) {
    block_t *curr;

    if (!ptr) {
        return;
    }

    curr = ((block_t*)ptr) - 1;
    ASSERT(curr->free == 0);
    ASSERT(curr->magic == HEAP_MAGIC);
    if (!ptr_in_heap(curr)) {
        PANIC("kfree: pointer outside heap");
    }

    ASSERT(curr->free == 0);
    curr->free = 1;

    // Coalesce adjacent free blocks to reduce fragmentation.
    curr = free_list;
    while (curr && curr->next) {
        if (curr->free && curr->next->free && (char*)curr + sizeof(block_t) + curr->size == (char*)curr->next) {
            curr->size += sizeof(block_t) + curr->next->size;
            curr->next = curr->next->next;
            continue;
        }
        curr = curr->next;
    }
}

void heap_dump(void) {
    block_t *curr = free_list;
    unsigned int index = 0;

    console_write("heap_dump begin\n");
    while (curr) {
        unsigned int block_addr = (unsigned int)curr;
        unsigned int payload_addr = (unsigned int)(curr + 1);
        unsigned int next_addr = (unsigned int)curr->next;
        console_write("blk %d: hdr=0x%x data=0x%x size=%d free=%d next=0x%x\n",
                      index, block_addr, payload_addr, curr->size, curr->free, next_addr);
        curr = curr->next;
        index++;
        if (index > 1024) {
            PANIC("heap_dump: too many blocks");
        }
    }
    console_write("heap_dump end\n");
}

void heap_validate(void) {
    block_t *curr = free_list;
    unsigned int guard = 0;

    ASSERT(ptr_in_heap(free_list));

    while (curr) {
        unsigned char *block_start = (unsigned char*)curr;
        unsigned char *payload_start = (unsigned char*)(curr + 1);
        unsigned char *block_end = payload_start + curr->size;

        ASSERT(ptr_in_heap(curr));
        ASSERT(ptr_in_heap(payload_start));
        ASSERT(block_end <= &heap[HEAP_SIZE]);

        if (curr->next) {
            ASSERT((unsigned char*)curr->next >= block_end);
            ASSERT(ptr_in_heap(curr->next));
        }

        curr = curr->next;
        guard++;
        ASSERT(guard <= 1024);
        (void)block_start;
    }
}

void* kzalloc(size_t size) {
    void* p = kmalloc(size);
    if (!p) return NULL;
    memset(p, 0, size);
    return p;
}

void* kcalloc(size_t size, size_t count) {
    if(count == 0 || size == 0) return NULL;
    if (count > ((size_t)-1) / size) return NULL;
    return kzalloc(count * size);
}