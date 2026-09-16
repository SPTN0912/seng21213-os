#ifndef THREAD_H
#define THREAD_H

#include "../include/types.h"
#include "process.h"

#define MAX_THREADS 32
#define THREAD_STACK_SIZE 4096

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

typedef struct thread {
    uint32_t tid;
    thread_state_t state;

    uint32_t esp;
    uint32_t eip;

    pcb_t *process;

    uint32_t stack[THREAD_STACK_SIZE / 4];

    struct thread *next;
} thread_t;

void thread_init(void);

thread_t *thread_create(
    pcb_t *process,
    void (*entry)(void)
);

void thread_yield(void);
void thread_exit(void);

thread_t *thread_get_table(void);
uint32_t thread_get_count(void);
thread_t *thread_get_current(void);
void thread_set_current(thread_t *thread);

#endif /* THREAD_H */
