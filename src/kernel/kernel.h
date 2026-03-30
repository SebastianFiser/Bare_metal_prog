#pragma once

#include "console.h"
struct sbiret {
    long error;
    long value;
};

#define PANIC(fmt, ...)  \
    do { \
        console_write("PANIC: %s:%d " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        for (;;) { \
            __asm__ volatile ("cli; hlt"); \
        } \
    } while (0)
