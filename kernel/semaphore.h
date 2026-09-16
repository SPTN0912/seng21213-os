#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "../include/types.h"
#include "thread.h"

typedef struct {
    int32_t value;
} semaphore_t;

void semaphore_init(semaphore_t *sem, int32_t value);
void semaphore_wait(semaphore_t *sem);
void semaphore_signal(semaphore_t *sem);

#endif /* SEMAPHORE_H */
