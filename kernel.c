static void clear_screen(unsigned char color) {
    volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
    const unsigned short blank = (unsigned short)(color << 8) | ' ';

    for (unsigned int i = 0; i < 80 * 25; i++) {
        vga[i] = blank;
    }
}

static void write_text(unsigned char text) {
    volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
    
}

void kernel_main(void) {
    clear_screen(0x4F);

}