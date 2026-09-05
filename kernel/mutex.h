#ifndef MUTEX_H
#define MUTEX_H

#include "../include/types.h"

#define MUTEX_MAX_WAITERS 8

typedef struct {
    volatile int locked;

    uint32_t wait_queue[MUTEX_MAX_WAITERS];
    uint32_t wait_head;
    uint32_t wait_tail;
    uint32_t wait_count;
} mutex_t;

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

#endif