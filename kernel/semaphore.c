#include "semaphore.h"

void semaphore_init(semaphore_t *sem, int32_t value)
{
    sem->value = value;
}

void semaphore_wait(semaphore_t *sem)
{
    if (sem->value > 0) {
        sem->value--;
    }
}

void semaphore_signal(semaphore_t *sem)
{
    sem->value++;
}
