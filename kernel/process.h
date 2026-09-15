#ifndef PROCESS_H
#define PROCESS_H

#include "../include/types.h"

#define MAX_PROCESSES 16
#define STACK_SIZE 4096

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
} proc_state_t;

typedef struct pcb {
    uint32_t pid;
    proc_state_t state;
    uint32_t esp;
    uint32_t eip;
    uint32_t stack[STACK_SIZE / 4];
    struct pcb *next;
} pcb_t;

void process_init(void);
pcb_t *process_create(void (*entry)(void));
void process_yield(void);
void process_exit(void);
void scheduler_tick(void);

pcb_t *process_get_table(void);
uint32_t process_get_count(void);
pcb_t *process_get_current(void);
void process_set_current(pcb_t *pcb);

void scheduler_init(void);
uint32_t scheduler_tick_context(uint32_t current_esp);

#endif
