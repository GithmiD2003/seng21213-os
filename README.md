# SENG21213-OS

Minimal 32-bit x86 teaching operating system for **SENG 21213 – Computer
Architecture & Operating Systems**, University of Kelaniya. It is written in
freestanding C and NASM assembly and runs in QEMU.

## Milestones

| Release | Stage | Main features | Status |
|---|---|---|---|
| `v0.1-stage0` | Boot and shell | MBR bootloader, protected mode, VGA, keyboard, shell | Complete |
| `v0.2-stage1` | Process scheduling | PCBs, PIT/IRQ0, context switching, round-robin scheduler | Complete |
| `v0.3-stage2` | Threads and synchronization | Kernel threads, mutexes, semaphores, producer-consumer | Complete |
| `v0.4-stage3` | Physical memory | BIOS E820 map, bitmap allocator, `meminfo` | Complete |
| `v0.5-stage4` | File system | RAM disk, superblock, inodes, file and shell APIs | Current |

## Stage 4: RAM Disk File System

Stage 4 adds a 1 MB RAM disk backed by 256 frames from the Stage 3 physical
memory manager. The disk uses 512-byte blocks and is formatted during boot.

The file system contains:

- A superblock describing the disk layout
- Separate inode and data-block allocation bitmaps
- 64 inodes with eight direct block pointers each
- A flat directory supporting file names up to 27 characters
- `fs_open()`, `fs_read()`, `fs_write()`, `fs_close()`, and `fs_unlink()`
- `ls`, `touch`, `cat`, `write`, and `rm` shell commands

## Project Structure

```text
boot/
├── boot.asm           MBR loader, E820 detection, protected-mode transition
└── switch.asm         IRQ0 context-switch entry
kernel/
├── kernel_entry.asm   Protected-mode kernel entry
├── kernel.c           Initialization, demonstrations, and shell
├── process.c          Process table and process creation
├── scheduler.c        PIT, PIC, IDT, and round-robin scheduling
├── thread.c/.h        Kernel threads
├── mutex.c/.h         Blocking mutex
├── semaphore.c/.h     Counting semaphore
├── pmm.c/.h           Stage 3 physical frame allocator
├── ramdisk.c/.h       1 MB frame-backed RAM disk
├── fs.c/.h            Inodes, directory, file API, and allocation bitmaps
├── keyboard.c/.h      PS/2 keyboard driver
└── vga.c/.h           VGA text-mode driver
include/types.h        Freestanding integer and utility types
linker.ld               Kernel layout at physical address 0x10000
Makefile                Build, run, debug, and clean targets
```

## Build and Run

Required Ubuntu/WSL packages: NASM, GCC with 32-bit support, binutils, Make,
and QEMU.

```bash
make clean
make
make run
```

Example shell session:

```text
touch hello.txt
write hello.txt Hello from SENG21213-OS
ls
cat hello.txt
rm hello.txt
ls
meminfo
```

Close QEMU or press `Ctrl+A`, then `X`, to stop it.

## Git Submission Rules

- The repository is private and `tiroshanm` is a collaborator.
- Generated files such as `build/`, `boot/boot.bin`, and `*.img` are ignored.
- Only source files, headers, `Makefile`, `linker.ld`, `.gitignore`, and this
  README are committed.
- Each completed stage has a GitHub release with clear release notes.
