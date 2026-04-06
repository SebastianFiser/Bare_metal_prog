#pragma once
#include "kernel.h"

#define HEAP_MAGIC 0xDEADBEEF
#define HEAP_SIZE (1024 * 1024 * 8) // 8 MiB
typedef unsigned int size_t;

typedef struct block {
    unsigned int magic;
    size_t size;
    int free;
    struct block* next;
} block_t;


#define ASSERT(x) \
    if (!(x)) { \
        PANIC("Assertion failed: %s\n" #x); \
    }

void heap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);

void heap_dump(void);
void heap_validate(void);