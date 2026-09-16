#include "thread.h"

static thread_t thread_table[MAX_THREADS];
static uint32_t thread_count = 0;
static thread_t *current_thread = 0;

void thread_init(void)
{
    thread_count = 0;
    current_thread = 0;
}

thread_t *thread_create(pcb_t *process, void (*entry)(void))
{
    if (thread_count >= MAX_THREADS) {
        return 0;
    }

    thread_t *thread = &thread_table[thread_count];

    thread->tid = thread_count;
    thread->state = THREAD_READY;
    thread->process = process;
    thread->eip = (uint32_t)entry;
    thread->esp = (uint32_t)&thread->stack[THREAD_STACK_SIZE / 4 - 1];
    thread->next = 0;

    thread_count++;

    return thread;
}

void thread_yield(void)
{
    if (current_thread != 0) {
        current_thread->state = THREAD_READY;
    }
}

void thread_exit(void)
{
    if (current_thread != 0) {
        current_thread->state = THREAD_TERMINATED;
    }
}

thread_t *thread_get_table(void)
{
    return thread_table;
}

uint32_t thread_get_count(void)
{
    return thread_count;
}

thread_t *thread_get_current(void)
{
    return current_thread;
}

void thread_set_current(thread_t *thread)
{
    current_thread = thread;
}
