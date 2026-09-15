#include "process.h"
#include "../include/types.h"

/*
 * x86 I/O ports
 */
#define PIT_COMMAND 0x43
#define PIT_CHANNEL0 0x40

#define PIC_MASTER_COMMAND 0x20
#define PIC_MASTER_DATA    0x21

/*
 * Interrupt vector used for IRQ0 after PIC remapping.
 */
#define IRQ0_VECTOR 0x20

/*
 * PIT input frequency is approximately 1.193182 MHz.
 * 1,193,182 / 100 = approximately 11,932.
 *
 * This gives a timer tick of approximately 10 ms.
 */
#define PIT_FREQUENCY 1193182
#define TIMER_FREQUENCY 100
#define PIT_DIVISOR (PIT_FREQUENCY / TIMER_FREQUENCY)

/*
 * Kernel code segment from boot.asm GDT.
 */
#define KERNEL_CODE_SEGMENT 0x08

/*
 * CPU I/O helpers.
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ __volatile__(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

/*
 * IDT entry.
 */
typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

/*
 * IDT pointer.
 */
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t idt_pointer;

/*
 * Assembly IRQ0 handler.
 */
extern void irq0_handler(void);

/*
 * Set one IDT entry.
 */
static void idt_set_gate(
    uint8_t vector,
    uint32_t handler,
    uint16_t selector,
    uint8_t flags
) {
    idt[vector].offset_low = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector = selector;
    idt[vector].zero = 0;
    idt[vector].type_attr = flags;
    idt[vector].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
}

/*
 * Load the IDT.
 */
static void idt_load(void) {
    __asm__ __volatile__(
        "lidtl (%0)"
        :
        : "r"(&idt_pointer)
    );
}

/*
 * Remap the PIC.
 *
 * Original IRQ0 is vector 8.
 * We move master PIC IRQs to vectors 32-39,
 * so IRQ0 becomes vector 32 (0x20).
 */
static void pic_remap(void) {
    uint8_t master_mask = 0xFF;

    /*
     * ICW1: start initialization sequence.
     */
    outb(PIC_MASTER_COMMAND, 0x11);

    /*
     * ICW2: master PIC vector offset.
     */
    outb(PIC_MASTER_DATA, 0x20);

    /*
     * ICW3: master has slave PIC on IRQ2.
     */
    outb(PIC_MASTER_DATA, 0x04);

    /*
     * ICW4: 8086/88 mode.
     */
    outb(PIC_MASTER_DATA, 0x01);

    /*
     * Temporarily mask all master IRQs.
     */
    outb(PIC_MASTER_DATA, master_mask);
}

/*
 * Program PIT channel 0 for approximately 100 Hz.
 */
static void pit_init(void) {
    uint16_t divisor = PIT_DIVISOR;

    /*
     * Channel 0
     * Access mode: low byte then high byte
     * Mode 3: square-wave generator
     * Binary mode
     */
    outb(PIT_COMMAND, 0x36);

    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

/*
 * Enable IRQ0 on the master PIC.
 */
static void pic_enable_irq0(void) {
    /*
     * Keep all IRQs masked except IRQ0.
     */
    outb(PIC_MASTER_DATA, 0xFE);
}

/*
 * Initialise the interrupt and timer infrastructure.
 */
void scheduler_init(void) {
    uint32_t i;

    /*
     * Clear the IDT.
     */
    for (i = 0; i < 256; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].type_attr = 0;
        idt[i].offset_high = 0;
    }

    idt_pointer.limit = sizeof(idt) - 1;
    idt_pointer.base = (uint32_t)&idt[0];

    /*
     * IRQ0 uses interrupt vector 0x20 after PIC remapping.
     *
     * 0x8E = present + ring 0 + 32-bit interrupt gate.
     */
    idt_set_gate(
        IRQ0_VECTOR,
        (uint32_t)irq0_handler,
        KERNEL_CODE_SEGMENT,
        0x8E
    );

    pic_remap();
    idt_load();
    pit_init();
    pic_enable_irq0();

 /*
     * All interrupt infrastructure is now ready.
     * Enable hardware interrupts so the PIT can generate IRQ0.
     */
    __asm__ __volatile__("sti");
}

/*
 * Return the next READY process in Round-Robin order.
 */
static pcb_t *scheduler_next_process(pcb_t *current) {
    pcb_t *table = process_get_table();
    uint32_t count = process_get_count();

    if (count == 0) {
        return 0;
    }

    /*
     * First scheduler tick:
     * start with the first READY process.
     */
    if (current == 0) {
        for (uint32_t i = 0; i < count; i++) {
            if (table[i].state == READY) {
                return &table[i];
            }
        }

        return 0;
    }

    /*
     * Find the current process index.
     */
    uint32_t current_index = 0;

    for (uint32_t i = 0; i < count; i++) {
        if (&table[i] == current) {
            current_index = i;
            break;
        }
    }

    /*
     * Round-Robin:
     * start immediately after the current process.
     */
    for (uint32_t offset = 1; offset <= count; offset++) {
        uint32_t index = (current_index + offset) % count;

        if (table[index].state == READY ||
            table[index].state == RUNNING) {
            return &table[index];
        }
    }

    return 0;
}

/*
 * Called by the IRQ0 assembly handler.
 *
 * The actual register-stack switching will be connected
 * to the assembly IRQ0 handler.
 */
uint32_t scheduler_tick_context(uint32_t current_esp) {
    pcb_t *current = process_get_current();
    pcb_t *next;

    /*
     * First timer tick: start the first process.
     */
    if (current == 0) {
        next = scheduler_next_process(0);

        if (next == 0) {
            return current_esp;
        }

        next->state = RUNNING;
        process_set_current(next);

        return next->esp;
    }

    /*
     * Save the current process's stack pointer.
     */
    current->esp = current_esp;

    /*
     * Current process goes back to READY state.
     */
    if (current->state == RUNNING) {
        current->state = READY;
    }

    /*
     * Select the next process.
     */
    next = scheduler_next_process(current);

    if (next == 0) {
        current->state = RUNNING;
        return current_esp;
    }

    next->state = RUNNING;
    process_set_current(next);

    return next->esp;
}
