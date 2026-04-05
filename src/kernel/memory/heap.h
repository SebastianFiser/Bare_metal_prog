#pragma once
#include "kernel.h"

#define HEAP_SIZE (1024 * 1024 * 8) // 64 MiB
typedef unsigned int size_t;
static unsigned char heap[HEAP_SIZE];

typedef struct block {
    size_t size;
    int free;
    struct block* next;
} block_t;


#define ASSERT(x) \
    if (!(x)) { \
        PANIC("Assertion failed: %s\n" #x); \
    }

static block_t* free_list;

void heap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);

void heap_dump(void);
void heap_validate(void);