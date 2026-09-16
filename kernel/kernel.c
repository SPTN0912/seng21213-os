/* =============================================================================
 * SENG21213-OS :: Main Kernel  (Stage 0 – Foundations)
 * File   : kernel/kernel.c
 *
 * PURPOSE
 *   This is the heart of your operating system. Right now it:
 *     1. Initialises VGA text-mode display
 *     2. Initialises the keyboard driver
 *     3. Prints a splash screen
 *     4. Runs a minimal interactive shell ("ksh")
 *
 * ASSIGNMENT MILESTONES  (what YOU will add in later lectures)
 *   Lecture  9  – Process Management  →  process.h / process.c / scheduler.c
 *   Lecture 10  – Threads             →  thread.h  / thread.c
 *   Lecture 11  – Memory Management   →  pmm.h     / pmm.c / vmm.c
 *   Lecture 12  – File System         →  fs.h      / fs.c
 *
 * CODING CONVENTION
 *   - Prefix kernel-internal functions with k_ (e.g. k_strcmp)
 *   - All driver APIs live in their own .h/.c pair
 *   - NEVER call malloc – use the PMM you build in Lecture 11
 * ============================================================================*/


#include "vga.h"
#include "keyboard.h"
#include "process.h"
#include "pmm.h"
#include "fs.h"
#include "../include/types.h"

/* ---------------------------------------------------------------------------
 * Forward declarations of shell commands
 * --------------------------------------------------------------------------*/
static void test_pmm(void);
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void cmd_meminfo(void);
static void cmd_ps(void);
static void cmd_ls(void);
static void cmd_touch(const char *name);
static void cmd_cat(const char *name);
static void cmd_write(const char *args);
static void cmd_rm(const char *name);

/* ---------------------------------------------------------------------------
 * Stage 1 demo processes
 * --------------------------------------------------------------------------*/
static void shell_process(void);
static void demo_process_1(void);
static void demo_process_2(void);

/* ---------------------------------------------------------------------------
 * Utility: minimal string helpers (no libc in a freestanding kernel!)
 * --------------------------------------------------------------------------*/
static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

/* Skip leading spaces */
static const char *k_ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}

/* ---------------------------------------------------------------------------
 * Splash Screen
 * --------------------------------------------------------------------------*/
static void print_splash(void) {
    vga_clear(VGA_BLACK);

    /* Top banner box */
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);

    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 0: Kernel Foundations", VGA_LIGHT_CYAN, VGA_BLACK);

    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Engineering – Department of Software Engineering",
                   VGA_LIGHT_GREY, VGA_BLACK);

    vga_set_cursor(4, 2);
    vga_puts_color("  Built by students, for students.  Type 'help' to begin.",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    vga_set_cursor(5, 2);
    vga_puts_color("  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
                   VGA_DARK_GREY, VGA_BLACK);

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  Welcome! This kernel was compiled from source and booted entirely\n");
    vga_puts("  from bare metal. There is no Linux or Windows underneath – only\n");
    vga_puts("  the code you and your team write.\n");
    vga_puts("\n");
    vga_puts("  Assignment milestones to implement:\n");
    vga_puts_color("    [L09] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Process Management  – PCB, ready queue, round-robin scheduler\n");
    vga_puts_color("    [L10] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Threads & Sync      – kernel threads, mutex, semaphore\n");
    vga_puts_color("    [L11] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Memory Management   – physical page allocator, virtual memory\n");
    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("File System         – RAM disk, FAT-like directory structure\n");
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Shell command implementations
 * --------------------------------------------------------------------------*/
static void cmd_help(void) {
    vga_puts_color("\n  SENG21213-OS Shell Commands\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  help    – Show this help message\n");
    vga_puts("  clear   – Clear the screen\n");
    vga_puts("  about   – About this OS and course\n");
    vga_puts("  echo    – Echo text to screen\n");
    vga_puts("  mem     – Memory map (stub)\n");
    vga_puts("  meminfo – Show physical memory information\n");
    vga_puts_color("\n  Milestones (to implement):\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ps      – [L09] List processes\n");
    vga_puts("  kill    – [L09] Terminate a process\n");
    vga_puts("  threads – [L10] List kernel threads\n");
    vga_puts("  free    – [L11] Show free memory\n");
vga_puts("  ls      – [L12] List files\n");
vga_puts("  touch   – [L12] Create an empty file\n");
vga_puts("  cat     – [L12] Print file contents\n");
vga_puts("  write   – [L12] Append text to a file\n");
vga_puts("  rm      – [L12] Remove a file\n\n");
}

static void cmd_clear(void) {
    vga_clear(VGA_BLACK);
}

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  VM Target    : QEMU (qemu-system-i386)\n");
    vga_puts("  Course       : SENG 21213 – Sem 2\n");
    vga_puts("  Reference    : Stallings, OS: Internals & Design Principles\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_mem(void) {
    /* Stage 0 stub – students implement the real PMM in Lecture 11 */
    vga_puts_color("\n  Memory Map (stub – implement PMM in Lecture 11)\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  0x00000000 – 0x000FFFFF  :  First 1 MB (reserved/BIOS)\n");
    vga_puts("  0x00100000 – 0x00EFFFFF  :  Extended memory (usable ~14 MB)\n");
    vga_puts("  0x00F00000 – 0x00FFFFFF  :  BIOS / ROM area\n");
    vga_puts("  0xB8000    – 0xBFFFF     :  VGA frame buffer\n");
    vga_puts_color("\n  TODO: Use BIOS int 0x15, EAX=0xE820 to get real memory map\n\n",
                   VGA_YELLOW, VGA_BLACK);
}
static void cmd_meminfo(void) {
    uint32_t total = pmm_get_total_memory();
    uint32_t used = pmm_get_used_memory();
    uint32_t free = pmm_get_free_memory();

    vga_puts_color("\n  Physical Memory Information\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  --------------------------------\n");

    vga_printf("  Total Memory : %u MB\n", total / (1024 * 1024));
    vga_printf("  Used Memory  : %u KB\n", used / 1024);
    vga_printf("  Free Memory  : %u MB\n", free / (1024 * 1024));

    vga_puts("\n");
}
static void cmd_ps(void) {
    pcb_t *table = process_get_table();
    uint32_t count = process_get_count();

    vga_puts_color("\n  PID    STATE\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ----------------\n");

    for (uint32_t i = 0; i < count; i++) {
        vga_printf("  %u      ", table[i].pid);

        switch (table[i].state) {
            case READY:
                vga_puts_color("READY\n", VGA_LIGHT_GREEN, VGA_BLACK);
                break;

            case RUNNING:
                vga_puts_color("RUNNING\n", VGA_YELLOW, VGA_BLACK);
                break;

            case BLOCKED:
                vga_puts_color("BLOCKED\n", VGA_LIGHT_RED, VGA_BLACK);
                break;

            case TERMINATED:
                vga_puts_color("TERMINATED\n", VGA_LIGHT_RED, VGA_BLACK);
                break;

            default:
                vga_puts("UNKNOWN\n");
                break;
        }
    }

    vga_puts("\n");
}
/* ---------------------------------------------------------------------------
 * Stage 4 - RAM disk filesystem commands
 * --------------------------------------------------------------------------*/

static void cmd_ls(void) {
    char buffer[4096];
    int count;

    count = fs_list(buffer, sizeof(buffer));

    vga_puts_color("\n  Files on RAM disk\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  -----------------\n");

    if (count < 0) {
        vga_puts_color("  Error: could not list files.\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    if (count == 0) {
        vga_puts("  (empty)\n");
        return;
    }

    vga_puts(buffer);
}
static void cmd_touch(const char *name) {
    int fd;

    if (name == NULL || k_strlen(name) == 0) {
        vga_puts_color("  Usage: touch <name>\n",
                       VGA_YELLOW, VGA_BLACK);
        return;
    }

    fd = fs_open(name, FS_O_CREATE | FS_O_WRONLY);

    if (fd < 0) {
        vga_puts_color("  Error: could not create file.\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    fs_close(fd);

    vga_puts("  File created: ");
    vga_puts(name);
    vga_puts("\n");
}
static void cmd_cat(const char *name) {
    int fd;
    int bytes_read;
    char buffer[256];

    if (name == NULL || k_strlen(name) == 0) {
        vga_puts_color("  Usage: cat <name>\n",
                       VGA_YELLOW, VGA_BLACK);
        return;
    }

    fd = fs_open(name, FS_O_RDONLY);

    if (fd < 0) {
        vga_puts_color("  Error: file not found.\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    while (true) {
        bytes_read = fs_read(fd, buffer, sizeof(buffer) - 1);

        if (bytes_read <= 0) {
            break;
        }

        buffer[bytes_read] = '\0';
        vga_puts(buffer);
    }

    fs_close(fd);
    vga_puts("\n");
}
static void cmd_write(const char *args) {
    char name[FS_NAME_MAX];
    const char *text;
    uint32_t name_length;
    uint32_t text_length;
    int fd;
    int bytes_written;

    if (args == NULL) {
        vga_puts_color("  Usage: write <name> <text>\n",
                       VGA_YELLOW, VGA_BLACK);
        return;
    }

    args = k_ltrim(args);

    if (k_strlen(args) == 0) {
        vga_puts_color("  Usage: write <name> <text>\n",
                       VGA_YELLOW, VGA_BLACK);
        return;
    }

    /* Find the space separating the filename and text. */
    text = args;

    while (*text != '\0' && *text != ' ') {
        text++;
    }

    name_length = (uint32_t)(text - args);

    if (name_length == 0 || name_length >= FS_NAME_MAX) {
        vga_puts_color("  Error: invalid filename.\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    /* Copy the filename into a separate buffer. */
    for (uint32_t i = 0; i < name_length; i++) {
        name[i] = args[i];
    }
    name[name_length] = '\0';

    /* Skip spaces before the text. */
    text = k_ltrim(text);

    text_length = k_strlen(text);

    if (text_length == 0) {
        vga_puts_color("  Usage: write <name> <text>\n",
                       VGA_YELLOW, VGA_BLACK);
        return;
    }

    /* Open an existing file in append mode. */
    fd = fs_open(name, FS_O_WRONLY | FS_O_APPEND);

    if (fd < 0) {
        vga_puts_color("  Error: file not found. Use touch first.\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    bytes_written = fs_write(fd, text, text_length);

    fs_close(fd);

    if (bytes_written < 0) {
        vga_puts_color("  Error: could not write to file.\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    vga_puts("  Written ");
    vga_printf("%u", (uint32_t)bytes_written);
    vga_puts(" bytes to ");
    vga_puts(name);
    vga_puts("\n");
}
static void cmd_rm(const char *name) {
    int result;

    if (name == NULL || k_strlen(name) == 0) {
        vga_puts_color("  Usage: rm <name>\n",
                       VGA_YELLOW, VGA_BLACK);
        return;
    }

    result = fs_unlink(name);

    if (result < 0) {
        vga_puts_color("  Error: could not remove file.\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    vga_puts("  File removed: ");
    vga_puts(name);
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static char  shell_buf[256];
static char  prompt[] = "\n  ksh> ";

static void shell_run(void) {
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true) {
        vga_puts_color(prompt, VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        /* Trim leading whitespace */
        const char *cmd = k_ltrim(shell_buf);
        if (k_strlen(cmd) == 0) continue;

        /* Dispatch */
        if (k_strcmp(cmd, "help")  == 0) { cmd_help();  continue; }
        if (k_strcmp(cmd, "clear") == 0) { cmd_clear(); continue; }
        if (k_strcmp(cmd, "about") == 0) { cmd_about(); continue; }
        if (k_strcmp(cmd, "mem")   == 0) { cmd_mem();   continue; }
if (k_strcmp(cmd, "meminfo") == 0) {
    cmd_meminfo();
    continue;
}

        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

        /* Milestone stubs */
        /* Stage 1: process listing */
if (k_strcmp(cmd, "ps") == 0) {
    cmd_ps();
    continue;
}

/* Stage 4: RAM disk filesystem commands */
if (k_strcmp(cmd, "ls") == 0) {
    cmd_ls();
    continue;
}

if (k_strncmp(cmd, "touch ", 6) == 0) {
    cmd_touch(k_ltrim(cmd + 6));
    continue;
}

if (k_strncmp(cmd, "cat ", 4) == 0) {
    cmd_cat(k_ltrim(cmd + 4));
    continue;
}

if (k_strncmp(cmd, "write ", 6) == 0) {
    cmd_write(k_ltrim(cmd + 6));
    continue;
}

if (k_strncmp(cmd, "rm ", 3) == 0) {
    cmd_rm(k_ltrim(cmd + 3));
    continue;
}

/* Milestone stubs */
if (k_strcmp(cmd, "kill")    == 0 ||
    k_strcmp(cmd, "threads") == 0 ||
    k_strcmp(cmd, "free")    == 0) {
    vga_puts_color("  [TODO] This command is not yet implemented.\n",
                   VGA_YELLOW, VGA_BLACK);
    vga_puts("  Implement it as part of your lecture assignment.\n");
    continue;
}

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(cmd);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    }
}
/* ---------------------------------------------------------------------------
 * Stage 1 process entry points
 * --------------------------------------------------------------------------*/

/*
 * Process 1: runs the existing interactive shell.
 */
static void shell_process(void) {
    shell_run();

    /*
     * shell_run() normally never returns.
     * Keep the process alive if it ever does.
     */
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
/*
 * Process 2: demonstrates that the scheduler is running.
 */
static void demo_process_1(void) {
    for (;;) {
        for (volatile uint32_t i = 0; i < 50000; i++) {
        }
    }
}
/*
 * Process 3: second scheduler demonstration process.
 */
static void demo_process_2(void) {
    for (;;) {
        for (volatile uint32_t i = 0; i < 100000; i++) {
        }
    }
}
/* ---------------------------------------------------------------------------
 * Kernel entry point – called from kernel_entry.asm
 * --------------------------------------------------------------------------*/
static void test_pmm(void)
{
    uint32_t frames[100];

    vga_puts_color("\n  PMM Test: Allocating 100 frames...\n",
                   VGA_YELLOW, VGA_BLACK);

    for (uint32_t i = 0; i < 100; i++) {
        frames[i] = pmm_alloc_frame();

        if (frames[i] == 0) {
            vga_puts_color("  ERROR: Frame allocation failed!\n",
                           VGA_LIGHT_RED, VGA_BLACK);
            return;
        }
    }

    vga_puts_color("  100 frames allocated successfully.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    for (uint32_t i = 0; i < 100; i++) {
        pmm_free_frame(frames[i]);
    }

    vga_puts_color("  100 frames freed successfully.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    vga_printf("  Used memory after test: %u KB\n",
               pmm_get_used_memory() / 1024);
}
void kernel_main(void) {
    vga_init();
    kb_init();
    print_splash();

    pmm_init();
test_pmm();

/* Initialize the Stage 4 RAM disk filesystem. */
fs_init();

process_init();

    /* Create the Stage 1 processes. */
    process_create(shell_process);
    process_create(demo_process_1);
    process_create(demo_process_2);

    /* Initialize the timer and scheduler. */
    scheduler_init();

    /*
     * The scheduler now controls execution.
     * The PIT will generate IRQ0 every 10 ms.
     */
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
