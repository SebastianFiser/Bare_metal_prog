#!/bin/bash
set -xue

QEMU=qemu-system-riscv32

# Path to clang and compiler flags
CC=clang  # Ubuntu users: use CC=clang
CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32-unknown-elf -fuse-ld=lld -fno-stack-protector -ffreestanding -nostdlib"

# Build the kernel
$CC $CFLAGS -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel.elf \
    kernel.c common.c #video.c

# Start QEMU
$QEMU -machine virt -device virtio-gpu-device -display gtk -bios default -serial mon:stdio --no-reboot \
    -kernel kernel.elf