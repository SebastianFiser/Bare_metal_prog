#pragma once

#include "console.h"
struct sbiret {
    long error;
    long value;
};

#define PANIC(fmt, ...)  \
    do { \
        write_text(0, 0, 0x4F, "PANIC: %s:%d " fmt "\n",__FILE__, __LINE__, ##__VA_ARGS__); \
        while (1) {} \
    } while (0)
