#include "common.h"

typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned char u8;

#define VIRTIO_MMIO_BASE 0x10008000u

#define REG_MAGIC_VALUE 0x000
#define REG_VERSION 0x004
#define REG_DEVICE_ID 0x008
#define REG_DEVICE_FEATURES 0x010
#define REG_DEVICE_FEATURES_SEL 0x014
#define REG_DRIVER_FEATURES 0x020
#define REG_DRIVER_FEATURES_SEL 0x024
#define REG_GUEST_PAGE_SIZE 0x028
#define REG_QUEUE_SEL 0x030
#define REG_QUEUE_NUM_MAX 0x034
#define REG_QUEUE_NUM 0x038
#define REG_QUEUE_ALIGN 0x03c
#define REG_QUEUE_PFN 0x040
#define REG_QUEUE_NOTIFY 0x050
#define REG_STATUS 0x070

#define STATUS_ACKNOWLEDGE 1u
#define STATUS_DRIVER 2u
#define STATUS_DRIVER_OK 4u
#define STATUS_FAILED 128u

#define VIRTQ_DESC_F_NEXT 1u
#define VIRTQ_DESC_F_WRITE 2u

#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO 0x0100u
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO 0x1101u

#define QUEUE_SIZE 8u
#define PAGE_SIZE 4096u

struct virtq_desc {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
};

struct virtq_avail {
    u16 flags;
    u16 idx;
    u16 ring[QUEUE_SIZE];
    u16 used_event;
};

struct virtq_used_elem {
    u32 id;
    u32 len;
};

struct virtq_used {
    u16 flags;
    u16 idx;
    struct virtq_used_elem ring[QUEUE_SIZE];
};

struct virtio_gpu_ctrl_hdr {
    u32 type;
    u32 flags;
    u64 fence_id;
    u32 ctx_id;
    u32 padding;
};

struct virtio_gpu_rect {
    u32 x;
    u32 y;
    u32 width;
    u32 height;
};

struct virtio_gpu_display_one {
    struct virtio_gpu_rect r;
    u32 enabled;
    u32 flags;
};

struct virtio_gpu_resp_display_info {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_display_one pmodes[16];
};

static u8 queue_mem[PAGE_SIZE * 2] __attribute__((aligned(PAGE_SIZE)));

static volatile struct virtq_desc *q_desc_ptr;
static volatile struct virtq_avail *q_avail_ptr;
static volatile struct virtq_used *q_used_ptr;

static volatile struct virtio_gpu_ctrl_hdr req_hdr;
static volatile struct virtio_gpu_resp_display_info resp_info;

static inline volatile u32 *mmio_reg(u32 off) {
    return (volatile u32 *)(VIRTIO_MMIO_BASE + off);
}

static inline void mmio_write(u32 off, u32 value) {
    *mmio_reg(off) = value;
}

static inline u32 mmio_read(u32 off) {
    return *mmio_reg(off);
}

static inline u32 align_up(u32 value, u32 alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static int setup_legacy_queue(void) {
    for (u32 i = 0; i < (u32)sizeof(queue_mem); i++) {
        queue_mem[i] = 0;
    }

    mmio_write(REG_QUEUE_SEL, 0);
    u32 qmax = mmio_read(REG_QUEUE_NUM_MAX);
    if (qmax == 0 || qmax < QUEUE_SIZE) {
        mmio_write(REG_STATUS, STATUS_FAILED);
        printf("virtio: queue too small (%d)\n", (int)qmax);
        return -1;
    }

    mmio_write(REG_QUEUE_NUM, QUEUE_SIZE);
    mmio_write(REG_QUEUE_ALIGN, PAGE_SIZE);

    u32 qbase = (u32)(unsigned long)queue_mem;
    mmio_write(REG_QUEUE_PFN, qbase / PAGE_SIZE);

    u32 desc_off = 0;
    u32 avail_off = desc_off + (u32)(sizeof(struct virtq_desc) * QUEUE_SIZE);
    u32 used_off = align_up(avail_off + (u32)sizeof(struct virtq_avail), PAGE_SIZE);

    q_desc_ptr = (volatile struct virtq_desc *)(queue_mem + desc_off);
    q_avail_ptr = (volatile struct virtq_avail *)(queue_mem + avail_off);
    q_used_ptr = (volatile struct virtq_used *)(queue_mem + used_off);
    return 0;
}

void video_init(void) {
    if (mmio_read(REG_MAGIC_VALUE) != 0x74726976u) {
        printf("virtio: bad magic\n");
        return;
    }
    if (mmio_read(REG_VERSION) != 1u) {
        printf("virtio: expected legacy version 1, got %d\n", (int)mmio_read(REG_VERSION));
        return;
    }
    if (mmio_read(REG_DEVICE_ID) != 16u) {
        printf("virtio: device id %d is not GPU\n", (int)mmio_read(REG_DEVICE_ID));
        return;
    }

    mmio_write(REG_STATUS, 0);
    mmio_write(REG_STATUS, STATUS_ACKNOWLEDGE);
    mmio_write(REG_STATUS, STATUS_ACKNOWLEDGE | STATUS_DRIVER);

    mmio_write(REG_DEVICE_FEATURES_SEL, 0);
    (void)mmio_read(REG_DEVICE_FEATURES);
    mmio_write(REG_DRIVER_FEATURES_SEL, 0);
    mmio_write(REG_DRIVER_FEATURES, 0);
    mmio_write(REG_GUEST_PAGE_SIZE, PAGE_SIZE);

    if (setup_legacy_queue() < 0) {
        return;
    }

    mmio_write(REG_STATUS, STATUS_ACKNOWLEDGE | STATUS_DRIVER | STATUS_DRIVER_OK);

    req_hdr.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    req_hdr.flags = 0;
    req_hdr.fence_id = 0;
    req_hdr.ctx_id = 0;
    req_hdr.padding = 0;

    q_desc_ptr[0].addr = (u64)(unsigned long)&req_hdr;
    q_desc_ptr[0].len = sizeof(req_hdr);
    q_desc_ptr[0].flags = VIRTQ_DESC_F_NEXT;
    q_desc_ptr[0].next = 1;

    q_desc_ptr[1].addr = (u64)(unsigned long)&resp_info;
    q_desc_ptr[1].len = sizeof(resp_info);
    q_desc_ptr[1].flags = VIRTQ_DESC_F_WRITE;
    q_desc_ptr[1].next = 0;

    q_avail_ptr->flags = 0;
    q_avail_ptr->ring[0] = 0;
    q_avail_ptr->idx = 1;

    __asm__ volatile ("fence w,w" ::: "memory");
    mmio_write(REG_QUEUE_NOTIFY, 0);

    while (q_used_ptr->idx == 0) {
    }
    __asm__ volatile ("fence r,r" ::: "memory");

    if (resp_info.hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
        printf("virtio-gpu: bad response type %x\n", resp_info.hdr.type);
        return;
    }

    if (resp_info.pmodes[0].enabled) {
        printf("virtio-gpu mode0: %dx%d\n",
               (int)resp_info.pmodes[0].r.width,
               (int)resp_info.pmodes[0].r.height);
    } else {
        printf("virtio-gpu: mode0 disabled\n");
    }
}