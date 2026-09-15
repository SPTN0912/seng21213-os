# SENG21213-OS — Stage 1: Process Management & Scheduler

> **Course**: SENG 21213 – Computer Architecture & Operating Systems  
> **Year**: 2nd Year, Software Engineering  
> **Assignment**: Build your own x86 Operating System

---

## What Is This?

This is **Stage 1** of your semester-long OS assignment. Stage 0 established the
boot process, VGA display, keyboard input, and shell. Stage 1 adds process
management, timer interrupts, context switching, and a Round-Robin scheduler.

```
seng21213-os/
├── boot/
│   ├── boot.asm              ← MBR bootloader (16-bit → 32-bit protected mode)
│   └── switch.asm            ← Context switching and IRQ0 timer handler
│
├── kernel/
│   ├── kernel_entry.asm      ← Protected-mode entry point, calls kernel_main()
│   ├── kernel.c              ← Main kernel, shell, and Stage 1 processes
│   ├── process.c             ← Process creation and PCB management
│   ├── process.h             ← PCB structure, states, and process interface
│   ├── scheduler.c           ← PIT, IDT, IRQ0, and Round-Robin scheduler
│   ├── vga.c                 ← VGA 80×25 text-mode driver implementation
│   ├── vga.h                 ← VGA driver declarations and colors
│   ├── keyboard.c            ← PS/2 keyboard polling driver implementation
│   └── keyboard.h            ← Keyboard driver declarations
│
├── include/
│   └── types.h               ← Primitive integer types and bool
│
├── linker.ld                 ← Linker script (kernel loaded at 0x10000)
├── Makefile                  ← Build system
├── Dockerfile                ← Reproducible build environment
├── .gitignore                ← Ignored build artifacts
└── README.md                 ← Project documentation
```

---

## Milestone Schedule

| Lecture | Milestone | Files to Add |
|---------|-----------|-------------|
| L08 | ✅ Stage 0 – Boot + VGA + Shell | *Given to you* |
| L09 | ✅ Stage 1 – Process Management & Scheduler | `kernel/process.c`, `kernel/process.h`, `kernel/scheduler.c`, `boot/switch.asm` |
| L10 | Threads & Synchronisation | `kernel/thread.c`, `kernel/mutex.c` |
| L11 | Memory Management | `kernel/pmm.c`, `kernel/vmm.c` |
| L12 | File System | `kernel/fs.c`, `kernel/ramdisk.c` |

---

## Quick Start

### Option A: Docker (Recommended for all platforms)

```bash
# 1. Install Docker Desktop (Windows/Mac) or Docker Engine (Linux)
# 2. Build the image once:
docker build -t seng21213-os-builder .

# 3. Build the OS:
docker run --rm -v "$(pwd)":/os seng21213-os-builder

# 4. Run in QEMU (install QEMU locally):
qemu-system-i386 -drive format=raw,file=seng21213-os.img -m 32M
```

### Option B: Native Linux/WSL2

```bash
# Ubuntu/Debian
sudo apt install nasm gcc gcc-multilib binutils qemu-system-x86 make

# Build
make all

# Run
make run
```

### Option C: macOS (Homebrew)

```bash
brew install nasm x86_64-elf-binutils qemu

# You also need an i686-elf-gcc cross-compiler:
# See: https://wiki.osdev.org/GCC_Cross-Compiler
make all
make run
```

---

## Understanding the Boot Process

```
Power On
  │
  ▼
BIOS (firmware in ROM)
  │  Loads 512-byte MBR from disk sector 1 into RAM at 0x7C00
  ▼
boot/boot.asm  (Real Mode, 16-bit)
  │  Prints "Loading SENG21213-OS..."
  │  Reads 64 sectors (kernel) from disk into RAM at 0x10000
  │  Sets up GDT (Global Descriptor Table)
  │  Switches CPU to 32-bit Protected Mode
  │  Far-jumps to 0x10000
  ▼
kernel/kernel_entry.asm  (Protected Mode, 32-bit)
  │  Calls kernel_main()
  ▼
  ▼
kernel/kernel.c  →  kernel_main()
  │  vga_init()       – set up text display
  │  kb_init()        – set up keyboard
  │  print_splash()   – welcome screen
  │  process_init()   – initialize the process table
  │  process_create() – create the shell and demo processes
  │  scheduler_init() – configure IDT, PIC, and PIT
  ▼
PIT → IRQ0 → Round-Robin scheduler → Context switch
```

---

## Building Lecture 9: Process Management

## Stage 1: Process Management & Scheduler

Stage 1 implements basic process management and preemptive Round-Robin
scheduling using the x86 PIT timer and IRQ0.

### Process Management

Each process is represented by a Process Control Block (PCB) containing:

- Process ID (PID)
- Process state
- Saved stack pointer (ESP)
- Entry point (EIP)
- 4 KB process stack

The kernel creates three processes:

1. Shell process
2. Demo process 1
3. Demo process 2

### Timer and Scheduling

The PIT is configured to generate approximately 100 timer interrupts per
second (one interrupt every 10 ms).

IRQ0 is handled by `boot/switch.asm`. The handler saves the current CPU
register state and calls `scheduler_tick_context()` to select the next process.

The scheduler uses Round-Robin scheduling to give each READY/RUNNING process
a time slice.

### Shell Command

The `ps` command displays the PID and current state of every process:

```text
PID    STATE
----------------
1      RUNNING
2      READY
3      READY

##Stage 1 Files

kernel/process.h
kernel/process.c
kernel/scheduler.c
boot/switch.asm

## Debugging Tips

```bash
# Debug with GDB
make run-debug
# In another terminal:
gdb
(gdb) target remote :1234
(gdb) set architecture i386
(gdb) symbol-file build/kernel.elf
(gdb) break kernel_main
(gdb) continue

# Inspect the disk image
xxd seng21213-os.img | head -32    # View MBR
xxd seng21213-os.img | grep -c aa55  # Verify boot signature
```

---

## Key Learning Resources

| Topic | Reference |
|-------|-----------|
| x86 Protected Mode | Intel IA-32 Manual, Vol 3, Chapter 3 |
| VGA Text Mode | OSDev Wiki: Text UI |
| Interrupts / IDT | Stallings Ch.1; OSDev: IDT |
| Process Management | Stallings Ch.3–4 (your lecture notes) |
| Memory Management | Stallings Ch.7–8 (your lecture notes) |
| OSDev community | https://wiki.osdev.org |

---

## Assessment Rubric (per milestone)

| Criterion | Weight |
|-----------|--------|
| Code compiles and kernel boots in QEMU | 30% |
| Feature implementation (correct behaviour) | 40% |
| Code quality and comments | 20% |
| Lab demo and viva questions | 10% |

---

*Happy hacking! Remember: every commercial OS started exactly like this.*
