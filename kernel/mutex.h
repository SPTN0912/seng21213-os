#ifndef MUTEX_H
#define MUTEX_H

#include "../include/types.h"
#include "thread.h"

typedef struct {
    uint32_t locked;
    thread_t *owner;
} mutex_t;

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

#endif /* MUTEX_H */
