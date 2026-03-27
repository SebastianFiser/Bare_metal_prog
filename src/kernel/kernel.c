static void clear_screen(unsigned char color) {
    volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
    const unsigned short blank = (unsigned short)(color << 8) | ' ';

    for (unsigned int i = 0; i < 80 * 25; i++) {
        vga[i] = blank;
    }
}

void write_text(unsigned int x, unsigned int y, unsigned char color, const char* text) {
    volatile unsigned short* vga = (volatile unsigned short*)0xB8000;

    unsigned int cx = x;
    unsigned int cy = y;
    //unsigned int index = y * 80 + x;

    while (*text) {
        if (*text == '\n') {
            cx = 0;
            cy = (cy + 1) % 25; // Move to the next line, wrap around if necessary
            text++;
        } 
        else if (cx >= 80) {
            cx = 0;
            cy = (cy + 1) % 25; // Move to the next line, wrap around if necessary
        }
        else {
            unsigned int index = cy * 80 + cx;
            unsigned short cell = (unsigned short)(color << 8) | *text;
            vga[index] = cell;
            cx++;
            text++;
        }
    }



}

void kernel_main(void) {
    clear_screen(0x4F);
    write_text(0, 0, 0x4F, "Hello, World!, welcome to my kernel!");
    write_text(0, 1, 0x4F, "This is a simple kernel written in C.");
    write_text(0, 2, 0x4F, "This idea was supported by HackClub!");
    write_text(0, 3, 0x4F, "Right now i cant do anything, but my guy is working on it!");
}