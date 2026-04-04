# Bare Metal Kernel

A hobby x86 bare-metal operating system kernel written in C and x86 assembly.
It boots via GRUB2 (multiboot2), runs entirely in protected mode, and provides a VGA
text-mode console, a PS/2 keyboard driver, an interactive shell, an in-memory
filesystem, and a built-in fullscreen text editor called **meowim**.

---

## Live Demo

Try the kernel directly in your browser — no installation required.

**[Launch Live Demo](https://sebastianfiser.github.io/Bare_metal_prog/)**

The demo ISO is automatically built and deployed to GitHub Pages via GitHub Actions
on every push to `main`.

---

## Getting Started

### Prerequisites

Install all required build tools with the provided script:

```bash
bash download_dep.sh
```

This installs: `gcc-multilib`, `nasm`, `qemu-system-x86`, `grub-pc-bin`, `xorriso`, and `binutils`.

### Build and Run

```bash
bash run.sh
```

This compiles the kernel, creates a bootable ISO, and launches it in QEMU.

### Run Without a GUI (headless)

```bash
NO_GUI=1 bash run.sh
```

---

## Running the ISO Manually

1. Download `os.iso` from the [GitHub Pages site](https://sebastianfiser.github.io/Bare_metal_prog/os.iso).
2. Open an x86 emulator such as [copy.sh/v86](https://copy.sh/v86/).
3. Load the ISO into the CD-ROM slot and click **Start**.

---

## Repository Structure

```
src/
├── boot/
│   └── boot.asm                  Boot entry point and multiboot2 header
└── kernel/
    ├── core/
    │   ├── kernel.c / kernel.h   Kernel entry, GDT, IDT, interrupt handlers
    ├── hardware/
    │   ├── console.c / console.h VGA text-mode console, colour attributes, scrolling
    │   ├── keyboard.c            PS/2 keyboard interrupt driver
    │   └── keymaps.h             Keyboard scancode-to-ASCII mapping
    ├── lib/
    │   ├── common.c / common.h   Shared utilities: string ops, I/O port helpers
    │   └── progs/
    │       ├── meowim.c          Fullscreen text editor implementation
    │       └── meowim.h
    ├── shell/
    │   ├── shell.c / shell.h     Command dispatcher and built-in command handlers
    │   └── input.c / input.h     Input event layer (shell mode / editor mode)
    └── filesystem/
        ├── filesys.c             In-memory filesystem (files and directories)
        └── filesys.h

config/
├── linker/linker.ld              Linker script for the kernel ELF binary
└── grub/grub.cfg                 GRUB2 boot configuration

run.sh                            Build script: compiles, links, creates ISO, runs QEMU
download_dep.sh                   Installs all required build dependencies
```

---

## Shell Reference

After boot the kernel drops into an interactive shell. The following commands are available:

| Command | Description |
|---------|-------------|
| `help` | Display a list of all available commands |
| `whoami` | Print information about the current user |
| `whatisthis` | Display a short description of this project |
| `echo <text …>` | Print the given text to the console |
| `clear` | Clear the screen |
| `ls` | List files and directories in the current directory |
| `cat <file>` | Print the contents of a file |
| `makef <name>` | Create a new empty file in the current directory |
| `mkdir <name>` | Create a new directory in the current directory |
| `cd <path>` | Change the current working directory |
| `edit [file]` | Open the meowim text editor, optionally with a file |

### meowim Text Editor

meowim is a simple fullscreen VGA text editor inspired by modal editors.

| Key | Action |
|-----|--------|
| `Ctrl+S` | Save the current file |
| `Ctrl+Q` | Quit the editor and return to the shell |
| `Arrow keys` | Move the cursor |
| `Page Up / Page Down` | Scroll the view |
| `Backspace` | Delete the character before the cursor |
| `Enter` | Insert a new line |

---

## Features

- [x] Bootable kernel image (GRUB2 / multiboot2, 32-bit protected mode)
- [x] Global Descriptor Table (GDT) and Interrupt Descriptor Table (IDT)
- [x] Programmable Interrupt Controller (PIC) remapping
- [x] PS/2 keyboard driver with interrupt handling
- [x] VGA text-mode console with colour support and hardware cursor
- [x] Interactive shell with tokenizer and command dispatch
- [x] In-memory filesystem supporting files and directories (`ls`, `cat`, `makef`, `mkdir`, `cd`)
- [x] meowim fullscreen text editor with scrolling, cursor navigation, and file save
- [ ] File and directory deletion
- [ ] Shell command history (arrow-up recall)
- [ ] Additional shell built-ins

---

## Credits

- Live demo and GitHub Pages deployment — handled by AI tooling.
- Everything else — written by hand, line by line.
