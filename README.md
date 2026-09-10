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
| `v0.4-stage3` | Physical memory | BIOS E820 map, bitmap allocator, `meminfo` | Current |
| `v0.5-stage4` | File system | RAM disk, superblock, inodes, file and shell APIs | Planned |

## Stage 3: Physical Memory Manager

The bootloader requests the BIOS E820 memory map before entering protected
mode. It stores the entry count at physical address `0x5000` and up to 32
24-byte E820 entries starting at `0x5004`.

`kernel/pmm.c` uses this map to manage the 32 MB QEMU memory range:

- Each bitmap bit represents one 4 KB physical frame.
- All frames begin reserved; only complete E820 type-1 ranges become free.
- The first 1 MB stays reserved for BIOS data, the bootloader, kernel, stacks,
  and memory-mapped hardware.
- `pmm_alloc_frame()` returns an identity-mapped frame address.
- `pmm_free_frame()` validates alignment and E820 usability before releasing a
  frame.
- The `meminfo` shell command shows E820 entry, total, used, and free counts.

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

At the shell prompt, run:

```text
meminfo
```

Close QEMU or press `Ctrl+A`, then `X`, to stop it.

## Git Submission Rules

- The repository is private and `tiroshanm` is a collaborator.
- Generated files such as `build/`, `boot/boot.bin`, and `*.img` are ignored.
- Only source files, headers, `Makefile`, `linker.ld`, `.gitignore`, and this
  README are committed.
- Each completed stage has a GitHub release with clear release notes.
