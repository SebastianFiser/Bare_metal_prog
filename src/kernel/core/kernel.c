#include "common.h"
#include "kernel.h"
#include "console.h"
#include "shell.h"
#include "filesys.h"
#include "heap.h"
#include "input.h"
#include "meowim.h"
#include "screen.h"

static console_state_t vga_boot_state;

__attribute__((naked)) void gdt_flush(unsigned int gdt_ptr_addr) {
    __asm__ __volatile__ (
        "mov 4(%%esp), %%eax \n\t"
        "lgdt (%%eax) \n\t"
        "mov $0x10, %%ax \n\t"
        "mov %%ax, %%ds \n\t"
        "mov %%ax, %%es \n\t"
        "mov %%ax, %%fs \n\t"
        "mov %%ax, %%gs \n\t"
        "mov %%ax, %%ss \n\t"
        "ljmp $0x08, $.1 \n\t"
        ".1: \n\t"
        "ret"
        : : : "eax" 
    );
}

void gdt_set_gate(int num, unsigned long base, unsigned long limit, unsigned char access, unsigned char gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F);

    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access = access;
}

void gdt_install() {
    gp.limit = (sizeof(struct gdt_entry) * 3) - 1;
    gp.base = (unsigned long)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Code segment
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Data segment

    gdt_flush((unsigned int)&gp);
}

struct gdt_entry gdt[3];
struct gdt_ptr gp;

struct idt_entry idt[256];
struct idt_ptr idtp;
static volatile unsigned long timer_ticks = 0;

__attribute__((naked)) void idt_load(unsigned int idt_ptr_addr) {
    __asm__ __volatile__ (
        "mov 4(%%esp), %%eax \n\t"
        "lidt (%%eax) \n\t"
        "ret"
        : : : "eax"
    );
}

__attribute__((naked)) void common_stub() {
    __asm__ __volatile__ (
        "pusha \n\t"
        "push %ds \n\t"
        "push %es \n\t"
        "mov $0x10, %ax \n\t"
        "mov %ax, %ds \n\t"
        "mov %ax, %es \n\t"
        "push %esp \n\t"
        "call trap_handler_logic \n\t"
        "add $4, %esp \n\t"
        "pop %es \n\t"
        "pop %ds \n\t"
        "popa \n\t"
        "add $8, %esp \n\t"
        "iret \n\t"
    );
}

//isr0 Devivsion by zero
__attribute__((naked)) void isr0() {
    __asm__ __volatile__ (
        "pushl $0 \n\t" // Push error code (0 for exceptions without an error code)
        "pushl $0 \n\t" // Push interrupt number (0 for isr0)
        "jmp common_stub \n\t"
    );
}

// iSR 13 GPF
__attribute__((naked)) void isr13() {
    __asm__ __volatile__ (
        "pushl $13 \n\t"
        "jmp common_stub \n\t"
    );
}

__attribute__((naked)) void isr32() {
    __asm__ __volatile__ (
        "pushl $0 \n\t"   // Push error code (0 for IRQ)
        "pushl $32 \n\t"  // Push interrupt number (32 = 0 after PIC remap)
        "jmp common_stub \n\t"
    );
}

// isr33 - IRQ1 Keyboard
__attribute__((naked)) void isr33() {
    __asm__ __volatile__ (
        "pushl $0 \n\t"   // Push error code (0 for IRQ)
        "pushl $33 \n\t"  // Push interrupt number (33 = 32 + 1 after PIC remap)
        "jmp common_stub \n\t"
    );
}


//C logic for handler
void idt_set_gate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
    idt[num].base_high = (base >> 16) & 0xFFFF;
}

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    idt_set_gate(0, (unsigned int)isr0, 0x08, 0x8E);
    idt_set_gate(13, (unsigned int)isr13, 0x08, 0x8E);
    idt_set_gate(32, (unsigned int)isr32 , 0x08, 0x8E);
    idt_set_gate(33, (unsigned int)isr33, 0x08, 0x8E);

    idt_load((unsigned int)&idtp);
}

void timer_init(void) {
    unsigned short divisor = (unsigned short)DIVISOR;

    // PIT: channel 0, low/high byte, mode 3, binary counter.
    outb(0x43, 0x36);
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));
}

void tick_handler(void) {
    timer_ticks++;
}

unsigned long timer_get_ticks(void) {
    return timer_ticks;
}

void read_uptime(unsigned int *seconds, unsigned int *milliseconds) {
    unsigned long ticks;
    unsigned long total_ms;

    if (!seconds || !milliseconds) {
        return;
    }

    ticks = timer_ticks;
    total_ms = (ticks * 1000U) / TIMER_FREQUENCY;
    *seconds = total_ms / 1000U;
    *milliseconds = total_ms % 1000U;
}

void trap_handler_logic(struct registers *regs) {
    if (regs->int_no == 33) {
        keyboard_handler(regs);
    }
    else if (regs->int_no == 32) {
        tick_handler();
    outb(0x20, 0x20); // EOI master
    return;
    }
    else if (regs->int_no == 0) {
        PANIC("Division by zero");
    } 
    else if (regs->int_no == 13) {
        PANIC("General Protection Fault");
    }
    else {
        console_write("Unhandled interrupt: %d\n", regs->int_no);
    }
}

void init_filesys(void)
{
    fs_node_t *root_dir;

    fs_init();
    root_dir = fs_root();

    fs_create(root_dir, "readme.txt", FS_NODE_FILE);
    fs_write(root_dir, "readme.txt", "This is a simple file system. You can create files with 'makef <filename>', and read them with 'cat <filename>'. Edit function is still work in progress");
}

static void VGA_INIT(uint32_t multiboot_info_ptr) {
    unsigned long last_overlay_redraw_tick = 0;

    screen_init();
    gdt_install();
    idt_init();
    pic_remap();
    timer_init();
    heap_init();
    init_filesys();
    shell_prompt();
    console_save_state(&vga_boot_state);

    for (;;) {
        unsigned long now = timer_get_ticks();

        // Keep status overlays live, but do not overwrite fullscreen editor.
        if (input_get_mode() == MODE_SHELL && now != last_overlay_redraw_tick) {
            console_redraw_view();
            last_overlay_redraw_tick = now;
        }

        keyboard_poll();
        __asm__ volatile ("sti; hlt");
    }
}

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info_ptr) {
    if (multiboot_magic != 0x36d76289U) {
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    fb_init(multiboot_info_ptr);
    fb_clear(0x00FF0000);
    VGA_INIT(multiboot_info_ptr);
}