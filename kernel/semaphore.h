#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "../include/types.h"

#define SEM_MAX_WAITERS 8

typedef struct {
    volatile int count;

    uint32_t wait_queue[SEM_MAX_WAITERS];
    uint32_t wait_head;
    uint32_t wait_tail;
    uint32_t wait_count;
} semaphore_t;

void sem_init(semaphore_t *sem, int value);
void sem_wait(semaphore_t *sem);
void sem_signal(semaphore_t *sem);

#endif