#include "process.h"

static pcb_t process_table[MAX_PROCESSES];
static uint32_t process_count = 0;
static uint32_t next_pid = 1;

void process_init(void) {
    process_count = 0;
    next_pid = 1;

    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].pid = 0;
        process_table[i].state = TERMINATED;
        process_table[i].esp = 0;
        process_table[i].eip = 0;
        process_table[i].next = 0;
    }
}

pcb_t *process_create(void (*entry)(void)) {
    if (process_count >= MAX_PROCESSES || entry == 0) {
        return 0;
    }

    pcb_t *pcb = &process_table[process_count];

    pcb->pid = next_pid++;
    pcb->state = READY;
    pcb->eip = (uint32_t)entry;
    pcb->next = 0;

        /*
     * Build an initial interrupt-return stack frame.
     *
     * The IRQ0 handler will restore the registers with POPAD
     * and return to the process using IRET.
     */
    uint32_t *stack = &pcb->stack[STACK_SIZE / 4];

    /*
     * IRET frame.
     */
    *(--stack) = 0x202;             /* EFLAGS: IF enabled */
    *(--stack) = 0x08;              /* CS: kernel code segment */
    *(--stack) = (uint32_t)entry;   /* EIP: process entry */

    /*
     * Values restored by POPAD.
     */
    *(--stack) = 0; /* EAX */
    *(--stack) = 0; /* ECX */
    *(--stack) = 0; /* EDX */
    *(--stack) = 0; /* EBX */
    *(--stack) = 0; /* Original ESP - ignored by POPAD */
    *(--stack) = 0; /* EBP */
    *(--stack) = 0; /* ESI */
    *(--stack) = 0; /* EDI */

    pcb->esp = (uint32_t)stack;

    process_count++;

    return pcb;
}

void process_yield(void) {
    /* Scheduler will be connected here. */
}

void process_exit(void) {
    /* Process termination will be connected here. */
}

void scheduler_tick(void) {
    /* Timer-driven scheduling will be connected here. */
}
pcb_t *process_get_table(void) {
    return process_table;
}

uint32_t process_get_count(void) {
    return process_count;
}

static pcb_t *current_process = 0;

pcb_t *process_get_current(void) {
    return current_process;
}

void process_set_current(pcb_t *pcb) {
    current_process = pcb;
}
