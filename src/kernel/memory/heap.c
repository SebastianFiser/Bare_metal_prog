#include "heap.h"
#include "console.h"

__attribute__((aligned(8)))
static unsigned char heap[HEAP_SIZE];
static block_t* free_list;
static const char *heap_default_tag = "generic";

static int ptr_in_heap(const void *ptr) {
    const unsigned char *p = (const unsigned char*)ptr;
    const unsigned char *start = &heap[0];
    const unsigned char *end = &heap[HEAP_SIZE];
    return p >= start && p < end;
}

static void write_hex_byte(unsigned char value) {
    static const char *hex = "0123456789abcdef";
    console_putchar(hex[(value >> 4) & 0xF]);
    console_putchar(hex[value & 0xF]);
}

static void write_escaped_byte(unsigned char value) {
    if (value >= 32 && value <= 126) {
        console_putchar((char)value);
        return;
    }

    if (value == '\n') {
        console_write("\\n");
        return;
    }
    if (value == '\r') {
        console_write("\\r");
        return;
    }
    if (value == '\t') {
        console_write("\\t");
        return;
    }
    if (value == '\0') {
        console_write("\\0");
        return;
    }

    console_write("\\x");
    write_hex_byte(value);
}

void heap_init() {
    free_list = (block_t*)heap;

    free_list->size = HEAP_SIZE - sizeof(block_t);
    free_list->free = 1;
    free_list->tag = 0;
    free_list->next = NULL;
}

//size = (size + 7) & ~7;
void* kmalloc_tag(size_t size, const char *tag) {
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
                new_block->tag = 0;
                new_block->next = curr->next;
                new_block->magic = HEAP_MAGIC;
                curr->size = size;
                curr->next = new_block;
            }
            curr->free = 0;
            curr->magic = HEAP_MAGIC;
            curr->tag = tag ? tag : heap_default_tag;
            return (void*)(curr+1);
        }
        curr = curr->next; 
    }
    return NULL;
}

void* kmalloc(size_t size) {
    return kmalloc_tag(size, heap_default_tag);
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
    curr->tag = 0;

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
        console_write("blk %d: hdr=0x%x data=0x%x size=%d free=%d tag=%s next=0x%x\n",
                      index, block_addr, payload_addr, curr->size, curr->free,
                      curr->tag ? curr->tag : "-", next_addr);
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
    void* p = kmalloc_tag(size, heap_default_tag);
    if (!p) return NULL;
    memset(p, 0, size);
    return p;
}

void* kcalloc_tag(size_t size, size_t count, const char *tag) {
    void* p;

    if(count == 0 || size == 0) return NULL;
    if (count > ((size_t)-1) / size) return NULL;

    p = kmalloc_tag(count * size, tag);
    if (!p) return NULL;
    memset(p, 0, count * size);
    return p;
}

void* kcalloc(size_t size, size_t count) {
    return kcalloc_tag(size, count, heap_default_tag);
}

int memblock_range(void) {
    block_t *curr = free_list;
    int count = 0;

    while (curr) {
        count++;
        curr = curr->next;

        if (count > 1024) {
            PANIC("memblock_range: too many blocks");
        }
    }

    return count;
}

void dump_memblock(int block_num, int size, const char format, int show_stats) {
    block_t *curr = free_list;
    int index = 0;
    int dump_size;
    unsigned char *data;
    unsigned int printable_count = 0;

    if (block_num < 0) {
        console_write_colored(CONSOLE_COLOR_ERROR, "dump_memblock: invalid block index\n");
        return;
    }

    while (curr && index < block_num) {
        curr = curr->next;
        index++;
    }

    if (!curr) {
        console_write_colored(CONSOLE_COLOR_ERROR, "dump_memblock: block %d not found\n", block_num);
        return;
    }

    if (format != 'h' && format != 't' && format != 'a') {
        console_write_colored(CONSOLE_COLOR_ERROR, "dump_memblock: unsupported format\n");
        return;
    }

    dump_size = size;
    if (dump_size <= 0) {
        dump_size = 64;
    }
    if ((unsigned int)dump_size > curr->size) {
        dump_size = (int)curr->size;
    }

    data = (unsigned char*)(curr + 1);

    if (show_stats) {
        for (int i = 0; i < dump_size; i++) {
            if (data[i] >= 32 && data[i] <= 126) {
                printable_count++;
            }
        }
    }

    console_write("memblock %d: hdr=0x%x data=0x%x size=%d free=%d tag=%s dump=%d format=",
                  block_num,
                  (unsigned int)curr,
                  (unsigned int)data,
                  curr->size,
                  curr->free,
                  curr->tag ? curr->tag : "-",
                  dump_size);
    console_putchar(format);
    console_write("\n");

    if (show_stats) {
        unsigned int ratio = (dump_size > 0) ? ((printable_count * 100U) / (unsigned int)dump_size) : 0;
        console_write("stats: printable=%d/%d (%d%%)\n", printable_count, dump_size, ratio);
    }

    if (dump_size == 0) {
        console_write("(empty block)\n");
        return;
    }

    if (format == 't') {
        for (int i = 0; i < dump_size; i++) {
            write_escaped_byte(data[i]);
            if (((i + 1) % 32) == 0) {
                console_write("\n");
            }
        }
        if ((dump_size % 32) != 0) {
            console_write("\n");
        }
        return;
    }

    if (format == 'a') {
        console_write("-- hex --\n");
    }

    for (int i = 0; i < dump_size; i += 16) {
        int line_count = ((dump_size - i) < 16) ? (dump_size - i) : 16;

        console_write("0x%x: ", (unsigned int)(data + i));
        for (int j = 0; j < line_count; j++) {
            write_hex_byte(data[i + j]);
            console_write(" ");
        }

        for (int j = line_count; j < 16; j++) {
            console_write("   ");
        }

        console_write("|");
        for (int j = 0; j < line_count; j++) {
            char ch = (data[i + j] >= 32 && data[i + j] <= 126) ? (char)data[i + j] : '.';
            console_putchar(ch);
        }
        console_write("|\n");
    }

    if (format == 'a') {
        console_write("-- text --\n");
        for (int i = 0; i < dump_size; i++) {
            write_escaped_byte(data[i]);
            if (((i + 1) % 32) == 0) {
                console_write("\n");
            }
        }
        if ((dump_size % 32) != 0) {
            console_write("\n");
        }
    }
}