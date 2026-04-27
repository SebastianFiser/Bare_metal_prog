#!/bin/bash
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT_DIR/build"
ISO="$ROOT_DIR/iso"
KERNEL=kernel.elf
BOOT_SRC="$ROOT_DIR/src/boot/boot.asm"
KERNEL_SRC="$ROOT_DIR/src/kernel/core/kernel.c"
COMMON_SRC="$ROOT_DIR/src/kernel/lib/common.c"
LINKER_SCRIPT="$ROOT_DIR/config/linker/linker.ld"
GRUB_CFG="$ROOT_DIR/config/grub/grub.cfg"
OUTPUT_ISO="$ROOT_DIR/os.iso"
CONSOLE_SRC="$ROOT_DIR/src/kernel/hardware/console.c"
KEYBOARD_SRC="$ROOT_DIR/src/kernel/hardware/keyboard.c"
SHELL_SRC="$ROOT_DIR/src/kernel/shell/shell.c"
INPUT_SRC="$ROOT_DIR/src/kernel/shell/input.c"
FILESYS_SRC="$ROOT_DIR/src/kernel/filesystem/filesys.c"
MEOWIM_SRC="$ROOT_DIR/src/kernel/lib/progs/meowim.c"
HEAP_SRC="$ROOT_DIR/src/kernel/memory/heap.c"
SCREEN_SRC="$ROOT_DIR/src/kernel/hardware/screen.c"
RENDERER_SRC="$ROOT_DIR/src/kernel/hardware/renderer.c"

echo "[CLEAN]"
rm -rf "$BUILD" "$ISO" "$OUTPUT_ISO"

echo "[SETUP]"
mkdir -p "$ISO/boot/grub"
mkdir -p "$BUILD"

echo "[1/5] ASM compile"
nasm -f elf32 "$BOOT_SRC" -o "$BUILD/boot.o"

echo "[2/5] C compile"
INCLUDE_FLAGS="-I$ROOT_DIR/src/kernel/core -I$ROOT_DIR/src/kernel/lib -I$ROOT_DIR/src/kernel/lib/progs -I$ROOT_DIR/src/kernel/hardware -I$ROOT_DIR/src/kernel/filesystem -I$ROOT_DIR/src/kernel/shell -I$ROOT_DIR/src/kernel/memory"
gcc -ffreestanding -m32 $INCLUDE_FLAGS -c "$KERNEL_SRC" -o "$BUILD/kernel.o"
gcc -ffreestanding -m32 $INCLUDE_FLAGS -c "$COMMON_SRC" -o "$BUILD/common.o"
gcc -ffreestanding -m32 $INCLUDE_FLAGS -c "$CONSOLE_SRC" -o "$BUILD/console.o"
gcc -ffreestanding -m32 $INCLUDE_FLAGS -c "$KEYBOARD_SRC" -o "$BUILD/keyboard.o"
gcc -ffreestanding -m32 $INCLUDE_FLAGS -c "$SHELL_SRC" -o "$BUILD/shell.o"
gcc -ffreestanding -m32 $INCLUDE_FLAGS -c "$INPUT_SRC" -o "$BUILD/input.o"
gcc -ffreestanding -m32 $INCLUDE_FLAGS -c "$FILESYS_SRC" -o "$BUILD/filesys.o"
gcc -ffreestanding -m32 $INCLUDE_FLAGS -c "$MEOWIM_SRC" -o "$BUILD/meowim.o"
gcc -ffreestanding -m32 $INCLUDE_FLAGS -c "$HEAP_SRC" -o "$BUILD/heap.o"
gcc -ffreestanding -m32 $INCLUDE_FLAGS -c "$SCREEN_SRC" -o "$BUILD/screen.o"
gcc -ffreestanding -m32 $INCLUDE_FLAGS -c "$RENDERER_SRC" -o "$BUILD/renderer.o"
gcc -ffreestanding -m32 $INCLUDE_FLAGS -c "$ROOT_DIR/src/kernel/core/process.c" -o "$BUILD/process.o"
gcc -ffreestanding -m32 $INCLUDE_FLAGS -c "$ROOT_DIR/src/kernel/core/scheduler.c" -o "$BUILD/scheduler.o"
gcc -ffreestanding -m32 $INCLUDE_FLAGS -c "$ROOT_DIR/src/kernel/memory/paging.c" -o "$BUILD/paging.o"


echo "[3/5] LINK"
ld -m elf_i386 -T "$LINKER_SCRIPT" -o "$BUILD/$KERNEL" \
    "$BUILD/boot.o" "$BUILD/kernel.o" "$BUILD/common.o" "$BUILD/console.o" "$BUILD/keyboard.o" "$BUILD/shell.o" "$BUILD/input.o" \
    "$BUILD/filesys.o" "$BUILD/meowim.o" "$BUILD/heap.o" "$BUILD/screen.o" "$BUILD/renderer.o" "$BUILD/paging.o" "$BUILD/process.o" "$BUILD/scheduler.o"

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