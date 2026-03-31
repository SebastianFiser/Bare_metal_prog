#pragma once

#include "console.h"
struct sbiret {
    long error;
    long value;
};

struct gdt_entry {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char base_middle;
    unsigned char access;
    unsigned char granularity;
    unsigned char base_high;
} __attribute__((packed));

struct gdt_ptr {
    unsigned short limit;
    unsigned long base;
} __attribute__((packed));

struct gdt_entry gdt[3];
struct gdt_ptr gp;
struct idt_entry {
    unsigned short base_low;
    unsigned short sel;
    unsigned char always0;
    unsigned char flags;
    unsigned short base_high;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned long base;
} __attribute__((packed));

struct registers {
    unsigned long es, ds;                  // Data segment selector
    unsigned long edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha.
    unsigned long int_no, err_code;    // Interrupt number and error code (if applicable)
    unsigned long eip, cs, eflags, useresp, ss; // Pushed by the processor automatically.
};

void idt_init();
void idt_set_gate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags);
void trap_handler_logic(struct registers *regs);

#define PANIC(fmt, ...)  \
    do { \
        console_write("PANIC: %s:%d " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        for (;;) { \
            __asm__ volatile ("cli; hlt"); \
        } \
    } while (0)
