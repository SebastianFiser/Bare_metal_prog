#include "common.h"
#include "kernel.h"
#include "console.h"
#include "shell.h"
#include "filesys.h"
#include "heap.h"
#include "input.h"
#include "meowim.h"
#include "screen.h"
#include "paging.h"

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

// isr14 - Page Fault (CPU already pushes error code)
__attribute__((naked)) void isr14() {
    __asm__ __volatile__ (
        "pushl $14 \n\t"
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
    idt_set_gate(14, (unsigned int)isr14, 0x08, 0x8E);
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
        return;
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
    else if (regs->int_no == 14) {
        uint32_t cr2;
        uint32_t pde;
        uint32_t pte;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

        /* Bring-up recovery: if we're in FB mode and a write fault lands on an unmapped page,
           map that page on demand so framebuffer rendering can continue. */
        if (fb_is_available() && renderer_get_mode() == RENDER_FB && (regs->err_code & 0x1U) == 0U) {
            uint32_t page = cr2 & PAGE_FRAME_MASK;
            map_page(page, page, PAGE_RW | PAGE_CACHE_DISABLE);
            return;
        }

        paging_debug_lookup(cr2, &pde, &pte);
        PANIC("Page Fault: cr2=0x%x err=0x%x eip=0x%x pde=0x%x pte=0x%x", cr2, regs->err_code, regs->eip, pde, pte);
    }
    else {
        console_write("Unhandled interrupt: %d\n", regs->int_no);
    }
}

void init_filesys(void)
{
    fs_init();
}

static void VGA_INIT(uint32_t multiboot_info_ptr) {
    uint32_t fb_base = fb_get_address();
    uint32_t fb_size = fb_get_pitch() * fb_get_height();

    console_write("fb debug: avail=%d base=0x%x size=0x%x pitch=0x%x w=0x%x h=0x%x\n",
        fb_is_available(), fb_base, fb_size, fb_get_pitch(), fb_get_width(), fb_get_height());

    heap_init();
    paging_init(fb_base, fb_size);
    paging_enable();
    input_buffers_init();
    editor_buffers_init();
    console_write("heap initialized\n");
    screen_init();
    console_write("VGA fallback initialized\n");
    gdt_install();
    console_write("gdt active\n");
    idt_init();
    console_write("idt active\n");
    pic_remap();
    console_write("PIC remapped\n");
    timer_init();
    console_write("timer initialized\n");
   
    init_filesys();
    console_write("file system initialized\n");
    console_set_color(0x0E);
    console_write_ascii("logo");
    console_set_color(0x0F);
    console_write("If you dont know where to start, try 'help' command.\n");
    shell_prompt();
    console_save_state(&vga_boot_state);

    for (;;) {
        keyboard_poll();
        __asm__ volatile ("sti; hlt");
    }
}

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info_ptr) {
    if (multiboot_magic != 0x36d76289U) {
        for ( ;;) {
            __asm__ volatile("cli; hlt");
        }
    }

    fb_init(multiboot_info_ptr);
    if (fb_is_available()) {
        renderer_set_mode(RENDER_FB);
    } else {
        renderer_set_mode(RENDER_VGA);
    }

    VGA_INIT(multiboot_info_ptr);
}