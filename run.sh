#!/bin/bash
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT_DIR/build"
ISO="$ROOT_DIR/iso"
KERNEL=kernel.elf
BOOT_SRC="$ROOT_DIR/src/boot/boot.asm"
KERNEL_SRC="$ROOT_DIR/src/kernel/kernel.c"
COMMON_SRC="$ROOT_DIR/src/kernel/common.c"
LINKER_SCRIPT="$ROOT_DIR/config/linker/linker.ld"
GRUB_CFG="$ROOT_DIR/config/grub/grub.cfg"
OUTPUT_ISO="$ROOT_DIR/os.iso"

echo "[CLEAN]"
rm -rf "$BUILD" "$ISO" "$OUTPUT_ISO"

echo "[SETUP]"
mkdir -p "$ISO/boot/grub"
mkdir -p "$BUILD"

echo "[1/5] ASM compile"
nasm -f elf32 "$BOOT_SRC" -o "$BUILD/boot.o"

echo "[2/5] C compile"
gcc -ffreestanding -m32 -c "$KERNEL_SRC" -o "$BUILD/kernel.o"
gcc -ffreestanding -m32 -c "$COMMON_SRC" -o "$BUILD/common.o"
gcc -ffreestanding -m32 -c "$CONSOLE_SRC" -o "$BUILD/console.o"

echo "[3/5] LINK"
ld -m elf_i386 -T "$LINKER_SCRIPT" -o "$BUILD/$KERNEL" \
    "$BUILD/boot.o" "$BUILD/kernel.o" "$BUILD/common.o" "$BUILD/console.o"

echo "[4/5] COPY"
cp "$BUILD/$KERNEL" "$ISO/boot/"
cp "$GRUB_CFG" "$ISO/boot/grub/grub.cfg"

echo "[5/5] ISO BUILD"
grub-mkrescue -o "$OUTPUT_ISO" "$ISO/"

echo "[RUN]"
QEMU_FLAGS="-m 512 -boot order=d -no-reboot -no-shutdown -serial stdio -monitor none"

if [ "${NO_GUI:-0}" = "1" ]; then
    QEMU_FLAGS="$QEMU_FLAGS -nographic"
fi

qemu-system-i386 -cdrom "$OUTPUT_ISO" $QEMU_FLAGS