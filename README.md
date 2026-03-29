KERNEL-BOTABLE with grub
LOTS of upcoming changes 

## 🎮 Demo
Try this OS directly in your browser — no install needed!

👉 **[Launch Live Demo](https://sebastianfiser.github.io/Bare_metal_prog/)**

The demo is automatically built and deployed via GitHub Actions whenever changes are pushed to `main`.

## HOW TO BUILD LOCALLY
1. Install dependencies: `bash download_dep.sh`
2. Build and run: `bash run.sh`

## REPOSITORY STRUCTURE
- `src/boot/boot.asm` - boot entry + multiboot2 header
- `src/kernel/kernel.c` - kernel C code
- `src/kernel/common.c` and `src/kernel/common.h` - shared kernel helpers
- `config/linker/linker.ld` - linker script
- `config/grub/grub.cfg` - GRUB boot config
- `run.sh` - local build + run script

## HOW TO RUN MANUALLY
1. Download `os.iso` from the [GitHub Pages site](https://sebastianfiser.github.io/Bare_metal_prog/os.iso)
2. Use an emulator like [copy.sh/v86](https://copy.sh/v86/), place the ISO in the CD image slot, and click start


## CREDIS

1. All live demo work is done by AI, i have no clue how to do that
2. rest of the project is done by me, me and myself. My soul and flesh suffered
