#include "screen.h"
#include "common.h"
#include "console.h"

static framebuffer_t fb;

typedef struct multiboot2_info {
    uint32_t total_size;
    uint32_t reserved;
} multiboot2_info_t;

typedef struct multiboot2_tag {
    uint32_t type;
    uint32_t size;
} multiboot2_tag_t;

typedef struct multiboot2_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
} __attribute__((packed)) multiboot2_tag_framebuffer_t;

enum {
    MULTIBOOT2_TAG_END = 0,
    MULTIBOOT2_TAG_FRAMEBUFFER = 8
};

void fb_init(uint32_t multiboot_info_ptr) {
    console_write("fb_init called\n");
    multiboot2_info_t* mb = (multiboot2_info_t*)multiboot_info_ptr;
    uint32_t offset = 8;

    while (offset < mb->total_size) {
        multiboot2_tag_t* tag = (multiboot2_tag_t*)((uint8_t*)mb + offset);

        if (tag->type == MULTIBOOT2_TAG_END) {
            break;
        }

        if (tag->type == MULTIBOOT2_TAG_FRAMEBUFFER) {
            multiboot2_tag_framebuffer_t* fb_tag = (multiboot2_tag_framebuffer_t*)tag;

            if (fb_tag->framebuffer_type != 1 || fb_tag->framebuffer_bpp != 32) {
                console_write("fb_init: unsupported framebuffer mode type=%d bpp=%d\n", fb_tag->framebuffer_type, fb_tag->framebuffer_bpp);
                return;
            }

            fb.address = (uint32_t*)(uint32_t)fb_tag->framebuffer_addr;
            fb.width = fb_tag->framebuffer_width;
            fb.height = fb_tag->framebuffer_height;
            fb.pitch = fb_tag->framebuffer_pitch;
            fb.bpp = fb_tag->framebuffer_bpp;

            console_write("FB: %x %dx%d pitch=%D bpp=%d\n", fb.address, fb.width, fb.height, fb.pitch, fb.bpp);
            return;
        }

        offset += (tag->size + 7) & ~7U;
    }

    console_write("fb_init: framebuffer tag not found\n");
}