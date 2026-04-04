# Bare Metal Kernel

A hobby x86 bare-metal OS kernel written in C and assembly, bootable via GRUB2, featuring a VGA text-mode console, keyboard input, a basic shell, an in-memory filesystem, and the **meowim** text editor.

## 🎮 Demo
Try this OS directly in your browser — no install needed!

👉 **[Launch Live Demo](https://sebastianfiser.github.io/Bare_metal_prog/)**

The demo is automatically built and deployed via GitHub Actions whenever changes are pushed to `main`.

## HOW TO BUILD LOCALLY
1. Install dependencies: `bash download_dep.sh`
2. Build and run: `bash run.sh`

## REPOSITORY STRUCTURE
```
src/
  boot/
    boot.asm                  - boot entry + multiboot2 header
  kernel/
    core/
      kernel.c / kernel.h     - kernel main, GDT, IDT, interrupt handlers
    hardware/
      console.c / console.h   - VGA text-mode console, colors, scrolling
      keyboard.c / keymaps.h  - PS/2 keyboard driver + keymap
    lib/
      common.c / common.h     - shared helpers (string, I/O port ops)
      progs/
        meowim.c / meowim.h   - meowim fullscreen text editor
    shell/
      shell.c / shell.h       - command dispatcher and built-in commands
      input.c / input.h       - input event abstraction (shell / editor modes)
    filesystem/
      filesys.c / filesys.h   - in-memory filesystem (files + directories)
config/
  linker/linker.ld            - linker script
  grub/grub.cfg               - GRUB boot config
run.sh                        - local build + QEMU run script
download_dep.sh               - install build dependencies
```

## SHELL COMMANDS
| Command | Description |
|---------|-------------|
| `help` | list built-in commands |
| `whoami` | display current user |
| `whatisthis` | show info about this project |
| `echo <text>` | print text to the console |
| `clear` | clear the screen |
| `ls` | list files in the current directory |
| `cat <file>` | display the contents of a file |
| `makef <name>` | create a new empty file |
| `edit [file]` | open meowim text editor (optional filename) |
| `cd <dir>` | change current directory |
| `mkdir <dir>` | create a new directory |

### meowim editor shortcuts
| Key | Action |
|-----|--------|
| `Ctrl+S` | save file |
| `Ctrl+Q` | quit editor |
| `Arrow keys` | move cursor |
| `PgUp / PgDn` | scroll view |
| `Backspace` | delete character |
| `Enter` | new line |

## HOW TO RUN MANUALLY
1. Download `os.iso` from the [GitHub Pages site](https://sebastianfiser.github.io/Bare_metal_prog/os.iso)
2. Use an emulator like [copy.sh/v86](https://copy.sh/v86/), place the ISO in the CD image slot, and click start

## CREDITS

1. All live demo / GitHub Pages work is done by AI — I have no clue how to do that
2. Rest of the project is done by me, me and myself. My soul and flesh suffered

## WORK STATUS
- [x] Bootable kernel (GRUB2 / multiboot2)
- [x] VGA text-mode console with color support
- [x] GDT and IDT setup
- [x] PS/2 keyboard driver
- [x] Shell with built-in commands
- [x] In-memory filesystem (files + directories, cd/ls/cat/makef/mkdir)
- [x] meowim fullscreen text editor (open, edit, save, scroll)
- [ ] File deletion command
- [ ] Command history (arrow-up recall)
- [ ] More shell commands
