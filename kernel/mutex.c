#include "mutex.h"

void mutex_init(mutex_t *mutex)
{
    mutex->locked = 0;
    mutex->owner = 0;
}

void mutex_lock(mutex_t *mutex)
{
    if (mutex->locked == 0) {
        mutex->locked = 1;
        mutex->owner = thread_get_current();
    }
}

void mutex_unlock(mutex_t *mutex)
{
    if (mutex->locked == 1 &&
        mutex->owner == thread_get_current()) {
        mutex->locked = 0;
        mutex->owner = 0;
    }
}
