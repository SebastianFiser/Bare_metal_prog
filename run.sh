#!/bin/bash
set -e

BUILD=build
ISO=iso
KERNEL=kernel.elf

echo "[CLEAN]"
rm -rf $BUILD $ISO os.iso

echo "[SETUP]"
mkdir -p $ISO/boot/grub
mkdir -p $BUILD

echo "[1/5] ASM compile"
nasm -f elf32 boot.asm -o $BUILD/boot.o

echo "[2/5] C compile"
gcc -ffreestanding -m32 -c kernel.c -o $BUILD/kernel.o

echo "[3/5] LINK"
ld -m elf_i386 -T linker.ld -o $BUILD/$KERNEL \
    $BUILD/boot.o $BUILD/kernel.o

echo "[4/5] COPY"
cp $BUILD/$KERNEL $ISO/boot/
cp grub.cfg $ISO/boot/grub/grub.cfg

echo "[5/5] ISO BUILD"
grub-mkrescue -o os.iso iso/

echo "[RUN]"
QEMU_FLAGS="-m 512 -boot order=d -no-reboot -no-shutdown -serial stdio -monitor none"

if [ "${NO_GUI:-0}" = "1" ]; then
    QEMU_FLAGS="$QEMU_FLAGS -nographic"
fi

qemu-system-i386 -cdrom os.iso $QEMU_FLAGS