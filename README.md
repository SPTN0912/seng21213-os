# SENG21213-OS — Stage 4: RAM Disk File System

> **Course**: SENG 21213 – Computer Architecture & Operating Systems  
> **Year**: 2nd Year, Software Engineering  
> **Assignment**: Build your own x86 Operating System

---

## What Is This?

This is Stage 4 of your semester-long OS assignment. Stage 0 established the boot process, VGA display, keyboard input, and shell. Stage 1 added process management, timer interrupts, context switching, and a Round-Robin scheduler. Stage 2 added threads and basic synchronization primitives including mutexes and semaphores. Stage 3 added physical memory management using the BIOS E820 memory map and a physical frame bitmap. Stage 4 adds a simple RAM disk file system with a superblock, block and inode bitmaps, inodes, a flat directory, file operations, and shell commands for creating, reading, writing, listing, and deleting files.

```
seng21213-os/
│
├── boot/
│   ├── boot.asm              ← MBR bootloader; switches from Real Mode
│   │                            to 32-bit Protected Mode
│   │
│   └── switch.asm            ← Context switching and IRQ0 timer interrupt
│                                used by the scheduler
│
├── kernel/
│   ├── kernel_entry.asm      ← Protected-mode entry point; calls kernel_main()
│   │
│   ├── kernel.c              ← Main kernel code, shell, and commands
│   │                            including Stage 4 filesystem commands
│   │
│   ├── process.c             ← Process creation and PCB management
│   ├── process.h             ← Process structures and declarations
│   │
│   ├── scheduler.c           ← PIT, IDT, PIC and Round-Robin scheduler
│   │
│   ├── thread.c              ← Thread creation, management and scheduling
│   ├── thread.h              ← Thread Control Block and declarations
│   │
│   ├── mutex.c               ← Mutex locking and unlocking
│   ├── mutex.h               ← Mutex structure and declarations
│   │
│   ├── semaphore.c           ← Semaphore wait and signal operations
│   ├── semaphore.h           ← Semaphore structure and declarations
│   │
│   ├── pmm.c                 ← Physical memory manager
│   ├── pmm.h                 ← Physical memory manager declarations
│   │
│   ├── ramdisk.c             ← Stage 4: 1 MB RAM disk implementation
│   ├── ramdisk.h             ← Stage 4: RAM disk definitions
│   │
│   ├── fs.c                  ← Stage 4: File system implementation
│   │                            (open, read, write, close, unlink, list)
│   │
│   ├── fs.h                  ← Stage 4: File system structures,
│   │                            constants and API declarations
│   │
│   ├── vga.c                 ← VGA text-mode display driver
│   ├── vga.h                 ← VGA driver declarations and colors
│   │
│   ├── keyboard.c            ← PS/2 keyboard input driver
│   └── keyboard.h            ← Keyboard driver declarations
│
├── include/
│   └── types.h               ← Basic integer and boolean types
│
├── linker.ld                 ← Kernel memory layout and sections
│
├── Makefile                  ← Build system for the OS
│
├── Dockerfile                ← Reproducible Docker build environment
│
├── .gitignore                ← Ignores build artifacts and generated files
│
└── README.md                 ← Project documentation and milestone details

```

---

## Milestone Schedule

| Lecture | Milestone | Files to Add |
|---------|-----------|-------------|
| L08 | ✅ Stage 0 – Boot + VGA + Shell | *Given to you* |
| L09 | ✅ Stage 1 – Process Management & Scheduler | `kernel/process.c`, `kernel/process.h`, `kernel/scheduler.c`, `boot/switch.asm` |
| L10 | ✅ Stage 2 – Threads & Synchronisation | `kernel/thread.c`, `kernel/thread.h`, `kernel/mutex.c`, `kernel/mutex.h`, `kernel/semaphore.c`, `kernel/semaphore.h` |
| L11 | ✅ Stage 3 – Physical Memory Management | `kernel/pmm.c`, `kernel/pmm.h` |
| L12 | ✅ Stage 4 – RAM Disk File System | `kernel/ramdisk.c`, `kernel/ramdisk.h`, `kernel/fs.c`, `kernel/fs.h` |

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

---

## Stage 2: Threads & Synchronisation

Stage 2 introduces basic thread management and synchronization primitives.

### Thread Management

Threads are represented using a Thread Control Block (TCB) containing:

- Thread ID (TID)
- Thread state
- Saved stack pointer (ESP)
- Entry point (EIP)
- Associated process
- 4 KB thread stack

The thread interface provides functions for:

- Initializing the thread subsystem
- Creating threads
- Yielding the current thread
- Terminating the current thread
- Accessing the thread table and current thread

### Mutex

The mutex implementation provides mutual exclusion using:

- Lock state
- Owner thread

The following operations are provided:

```text
mutex_init()
mutex_lock()
mutex_unlock()

##Semaphore

The semaphore implementation maintains an integer counter and provides:

semaphore_init()
semaphore_wait()
semaphore_signal()

##Stage 2 Files
kernel/thread.h
kernel/thread.c
kernel/mutex.h
kernel/mutex.c
kernel/semaphore.h
kernel/semaphore.c

### Stage 3 – Physical Memory Management

Implemented:

- BIOS E820 memory map parsing
- Physical frame bitmap with 1 bit per 4 KB frame
- First-fit physical frame allocation
- Physical frame freeing
- Total, used, and free memory tracking
- Physical frame 0 reservation
- `meminfo` shell command
- PMM test allocating and freeing 100 frames
- Verified PMM operation in QEMU

---

## Stage 4 – RAM Disk File System

Stage 4 implements a simple flat file system stored entirely in a 1 MB
RAM disk.

### RAM Disk

The RAM disk is divided into 256 blocks of 4 KB each.

The file system layout is:

| Block | Purpose |
|-------|---------|
| 0 | Superblock |
| 1 | Directory |
| 2 | Block bitmap |
| 3 | Inode bitmap |
| 4 | Inode table |
| 5–255 | File data blocks |

### File System Features

Implemented:

- 1 MB RAM disk
- Superblock with file system metadata
- Block bitmap
- Inode bitmap
- Inode table
- Flat directory with 28-character file names
- Eight direct block pointers per inode
- Maximum file size of 32 KB
- File opening and closing
- File reading and writing
- File deletion
- Open file table
- Append mode

### Shell Commands

The following file system commands are available:

```text
ls
touch <name>
cat <name>
write <name> <text>
rm <name>

##Stage 4 Verification
The file system was tested in QEMU by:

Creating files
Writing data to files
Reading file contents
Listing directory contents
Deleting files
Testing multiple files in the same RAM disk session
Verifying deleted files no longer appear in ls

The RAM disk is volatile, so its contents are cleared when the OS is
restarted.

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
